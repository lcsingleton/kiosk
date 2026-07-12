#include "CommandServer.h"
#include "CalendarClient.h"
#include "DelegatedAuth.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QPointer>
#include <QSet>
#include <QTimer>

namespace
{
bool personHasEmail( const PersonConfig &p, const QString &email )
{
	for ( const QString &candidate : p.emails )
	{
		if ( candidate.compare( email, Qt::CaseInsensitive ) == 0 )
			return true;
	}
	return false;
}

// A few taps landing within this long of each other collapse into one
// GET+PATCH — deliberately generous so it comfortably absorbs "tap Mum, tap
// Dad, tap the kids" as one round trip. The UI doesn't wait on this: it
// shows each tap's new state optimistically (see AttendeeBadges.qml), so
// this only trades off wire efficiency against how long a *wrong* optimistic
// guess could stick around, not perceived responsiveness.
constexpr int kAttendeeDebounceMs = 2000;

QString attendeeBatchKey( const QString &calendarId, const QString &eventId )
{
	return calendarId + QLatin1Char( '\x1f' ) + eventId;
}
} // namespace

CommandServer::CommandServer( CalendarClient *client, const QVector<PersonConfig> &people,
							  DelegatedAuth *delegatedAuth, QObject *parent )
	: QObject( parent ), m_client( client )
{
	for ( const PersonConfig &p : people )
		m_peopleByName.insert( p.person, p );

	connect( &m_server, &QLocalServer::newConnection, this, &CommandServer::onNewConnection );

	if ( delegatedAuth )
	{
		connect( delegatedAuth, &DelegatedAuth::authorizationPending, this,
				&CommandServer::broadcastAuthorizationPending );
	}

	m_handlers[CommandAction::ScheduleEvent] =
		[this]( const Command &c, std::function<void( Result )> reply ) { handleSchedule( c, reply ); };
	m_handlers[CommandAction::RescheduleEvent] =
		[this]( const Command &c, std::function<void( Result )> reply ) { handleReschedule( c, reply ); };
	m_handlers[CommandAction::CancelEvent] = [this]( const Command &c, std::function<void( Result )> reply ) {
		handleCancel( c, reply );
	};
	m_handlers[CommandAction::RenameEvent] =
		[this]( const Command &c, std::function<void( Result )> reply ) { handleRename( c, reply ); };
	m_handlers[CommandAction::ChangeEventLocation] = [this]( const Command &c,
															 std::function<void( Result )> reply ) {
		handleChangeLocation( c, reply );
	};
	m_handlers[CommandAction::InviteParticipant] = [this]( const Command &c,
														   std::function<void( Result )> reply ) {
		handleInviteParticipant( c, reply );
	};
	m_handlers[CommandAction::UninviteParticipant] = [this]( const Command &c,
															 std::function<void( Result )> reply ) {
		handleUninviteParticipant( c, reply );
	};
}

bool CommandServer::listen( const QString &socketPath, QString &error )
{
	// Cleans up a stale socket file left behind by a previous crashed run —
	// QLocalServer::listen() fails if the path already exists.
	QLocalServer::removeServer( socketPath );
	if ( !m_server.listen( socketPath ) )
	{
		error = m_server.errorString();
		return false;
	}
	return true;
}

void CommandServer::onNewConnection()
{
	while ( QLocalSocket *socket = m_server.nextPendingConnection() )
	{
		m_recvBuffers.insert( socket, QByteArray() );

		connect( socket, &QLocalSocket::readyRead, this, [this, socket]() {
			QByteArray &buf = m_recvBuffers[socket];
			buf += socket->readAll();
			int idx;
			while ( ( idx = buf.indexOf( '\n' ) ) >= 0 )
			{
				const QByteArray line = buf.left( idx );
				buf.remove( 0, idx + 1 );
				if ( !line.trimmed().isEmpty() )
					handleLine( socket, line );
			}
		} );

		connect( socket, &QLocalSocket::disconnected, this, [this, socket]() {
			m_recvBuffers.remove( socket );
			socket->deleteLater();
		} );
	}
}

void CommandServer::handleLine( QLocalSocket *socket, const QByteArray &line )
{
	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson( line, &parseError );
	if ( parseError.error != QJsonParseError::NoError || !doc.isObject() )
	{
		qWarning().noquote() << "malformed command line, ignoring:" << parseError.errorString();
		return;
	}

	const Command cmd = Command::fromJson( doc.object() );
	if ( cmd.commandId.isEmpty() )
	{
		qWarning() << "command missing commandId, ignoring";
		return;
	}

	// A QPointer, not the raw socket*: the actual Calendar API call this
	// dispatches to is a real network round trip, during which the kiosk
	// app's connection may have already dropped and been deleted — writing
	// to a dangling QLocalSocket* at that point would be a use-after-free.
	QPointer<QLocalSocket> guardedSocket( socket );
	dispatch( cmd, [this, guardedSocket, commandId = cmd.commandId]( Result result ) {
		if ( guardedSocket )
		{
			sendResult( guardedSocket, result );
		}
		else
		{
			qWarning().noquote() << "client disconnected before result for command" << commandId
								 << "was ready";
		}
	} );
}

void CommandServer::dispatch( const Command &cmd, std::function<void( Result )> reply )
{
	const auto it = m_handlers.constFind( cmd.action );
	if ( it == m_handlers.constEnd() )
	{
		reply( Result::failure( cmd.commandId, QStringLiteral( "invalid_request" ),
								QStringLiteral( "Unknown action: %1" ).arg( cmd.action ) ) );
		return;
	}
	( *it )( cmd, reply );
}

void CommandServer::sendResult( QLocalSocket *socket, const Result &result )
{
	socket->write( QJsonDocument( result.toJson() ).toJson( QJsonDocument::Compact ) + "\n" );
	socket->flush();
	if ( result.ok() )
		emit writeSucceeded();
}

void CommandServer::broadcastAuthorizationPending( const QString &verificationUrl, const QString &userCode,
													int expiresInSecs )
{
	const AuthorizationPendingEvent event{ verificationUrl, userCode, expiresInSecs };
	const QByteArray line = QJsonDocument( event.toJson() ).toJson( QJsonDocument::Compact ) + "\n";
	for ( QLocalSocket *socket : m_recvBuffers.keys() )
	{
		socket->write( line );
		socket->flush();
	}
}

void CommandServer::handleSchedule( const Command &cmd, std::function<void( Result )> reply )
{
	const auto payload = ScheduleEventPayload::fromJson( cmd.payload );
	if ( payload.summary.isEmpty() || payload.start.isEmpty() || payload.end.isEmpty() )
	{
		reply( Result::failure( cmd.commandId, QStringLiteral( "invalid_request" ),
								QStringLiteral( "ScheduleEvent requires payload.summary, .start, .end" ) ) );
		return;
	}

	QJsonObject eventBody;
	eventBody["summary"] = payload.summary;
	eventBody["start"] = QJsonObject{ { "dateTime", payload.start } };
	eventBody["end"] = QJsonObject{ { "dateTime", payload.end } };
	if ( !payload.description.isEmpty() )
		eventBody["description"] = payload.description;
	// Deliberately no "attendees" here — see the insertEvent callback below.

	const QStringList attendeeNames = payload.attendees;
	m_client->insertEvent(
		cmd.calendarId, eventBody,
		[this, cmd, reply, attendeeNames]( QJsonObject event, QString code, QString message ) {
			if ( !code.isEmpty() )
			{
				reply( Result::failure( cmd.commandId, code, message ) );
				return;
			}
			reply( Result::success( cmd.commandId ) );

			// Attendees go on as a follow-up PATCH, never the insertEvent
			// body itself: unlike patchEvent, insertEvent has no
			// delegation_required -> DelegatedAuth fallback (see
			// CalendarClient.h), so a service account without domain-wide
			// delegation would fail the *entire create* the moment
			// attendees[] was in the initial body. Routing it through
			// patchEvent instead reuses that fallback — same as
			// InviteParticipant — and, deliberately, doesn't gate this
			// ScheduleEvent's own success reply on it: the delegated-auth
			// fallback can mean a human needs to complete an out-of-band
			// device-code grant, which can take minutes.
			if ( attendeeNames.isEmpty() )
				return;
			QJsonArray attendees;
			for ( const QString &person : attendeeNames )
			{
				const auto personIt = m_peopleByName.constFind( person );
				if ( personIt != m_peopleByName.constEnd() && !personIt->emails.isEmpty() )
					attendees.append( QJsonObject{ { "email", personIt->emails.first() } } );
			}
			if ( attendees.isEmpty() )
				return;

			QJsonObject patchBody;
			patchBody["attendees"] = attendees;
			m_client->patchEvent( cmd.calendarId, event.value( "id" ).toString(),
								  event.value( "etag" ).toString(), patchBody,
								  []( QJsonObject, QString patchCode, QString patchMessage ) {
									  if ( !patchCode.isEmpty() )
										  qWarning().noquote() << "failed to add attendees to new event:"
															   << patchMessage;
								  } );
		} );
}

void CommandServer::handleReschedule( const Command &cmd, std::function<void( Result )> reply )
{
	const auto payload = RescheduleEventPayload::fromJson( cmd.payload );
	if ( payload.newStart.isEmpty() || payload.newEnd.isEmpty() )
	{
		reply( Result::failure( cmd.commandId, QStringLiteral( "invalid_request" ),
								QStringLiteral( "RescheduleEvent requires payload.newStart, .newEnd" ) ) );
		return;
	}

	QJsonObject patchBody;
	patchBody["start"] = QJsonObject{ { "dateTime", payload.newStart } };
	patchBody["end"] = QJsonObject{ { "dateTime", payload.newEnd } };

	m_client->patchEvent( cmd.calendarId, cmd.eventId, cmd.etag, patchBody,
						  [cmd, reply]( QJsonObject, QString code, QString message ) {
							  reply( code.isEmpty() ? Result::success( cmd.commandId )
													: Result::failure( cmd.commandId, code, message ) );
						  } );
}

void CommandServer::handleCancel( const Command &cmd, std::function<void( Result )> reply )
{
	m_client->deleteEvent( cmd.calendarId, cmd.eventId, cmd.etag,
						   [cmd, reply]( QString code, QString message ) {
							   reply( code.isEmpty() ? Result::success( cmd.commandId )
													 : Result::failure( cmd.commandId, code, message ) );
						   } );
}

void CommandServer::handleRename( const Command &cmd, std::function<void( Result )> reply )
{
	const auto payload = RenameEventPayload::fromJson( cmd.payload );
	if ( payload.newSummary.isEmpty() )
	{
		reply( Result::failure( cmd.commandId, QStringLiteral( "invalid_request" ),
								QStringLiteral( "RenameEvent requires payload.newSummary" ) ) );
		return;
	}
	patchField( cmd, "summary", payload.newSummary, reply );
}

void CommandServer::handleChangeLocation( const Command &cmd, std::function<void( Result )> reply )
{
	const auto payload = ChangeEventLocationPayload::fromJson( cmd.payload );
	if ( payload.newLocation.isEmpty() )
	{
		reply( Result::failure( cmd.commandId, QStringLiteral( "invalid_request" ),
								QStringLiteral( "ChangeEventLocation requires payload.newLocation" ) ) );
		return;
	}
	patchField( cmd, "location", payload.newLocation, reply );
}

void CommandServer::patchField( const Command &cmd, const QString &jsonKey, const QString &value,
								std::function<void( Result )> reply )
{
	QJsonObject patchBody;
	patchBody[jsonKey] = value;

	m_client->patchEvent( cmd.calendarId, cmd.eventId, cmd.etag, patchBody,
						  [cmd, reply]( QJsonObject, QString code, QString message ) {
							  reply( code.isEmpty() ? Result::success( cmd.commandId )
													: Result::failure( cmd.commandId, code, message ) );
						  } );
}

void CommandServer::handleInviteParticipant( const Command &cmd, std::function<void( Result )> reply )
{
	const auto payload = ParticipantPayload::fromJson( cmd.payload );
	const auto personIt = m_peopleByName.constFind( payload.person );
	if ( payload.person.isEmpty() || personIt == m_peopleByName.constEnd() || personIt->emails.isEmpty() )
	{
		reply( Result::failure( cmd.commandId, 
								QStringLiteral( "invalid_request" ),
								QStringLiteral( "InviteParticipant requires a known payload.person" ) ) );
		return;
	}
	queueAttendeeChange( cmd, payload.person, true, reply );
}

void CommandServer::handleUninviteParticipant( const Command &cmd, std::function<void( Result )> reply )
{
	const auto payload = ParticipantPayload::fromJson( cmd.payload );
	const auto personIt = m_peopleByName.constFind( payload.person );
	if ( payload.person.isEmpty() || personIt == m_peopleByName.constEnd() )
	{
		reply( Result::failure( cmd.commandId,
								QStringLiteral( "invalid_request" ),
								QStringLiteral( "UninviteParticipant requires a known payload.person" ) ) );
		return;
	}
	queueAttendeeChange( cmd, payload.person, false, reply );
}

void CommandServer::queueAttendeeChange( const Command &cmd,
										 const QString &person,
										 bool invited,
										 std::function<void( Result )> reply )
{
	const QString key = attendeeBatchKey( cmd.calendarId, cmd.eventId );

	PendingAttendeeBatch &batch = m_pendingAttendeeBatches[key];
	batch.calendarId = cmd.calendarId;
	batch.eventId = cmd.eventId;
	batch.deltas.insert( person, invited ); // a later tap for the same person within the window wins
	batch.pending.append( { cmd.commandId, reply } );

	QTimer *&timer = m_attendeeDebounceTimers[key];
	if ( !timer )
	{
		timer = new QTimer( this );
		timer->setSingleShot( true );
		connect( timer, &QTimer::timeout, this, [this, key]() { flushAttendeeBatch( key ); } );
	}
	timer->start( kAttendeeDebounceMs ); // restarts the wait if one's already running
}

void CommandServer::flushAttendeeBatch( const QString &key )
{
	const auto it = m_pendingAttendeeBatches.constFind( key );
	if ( it == m_pendingAttendeeBatches.constEnd() )
		return;
	const PendingAttendeeBatch batch = it.value();
	m_pendingAttendeeBatches.remove( key );
	if ( QTimer *timer = m_attendeeDebounceTimers.take( key ) )
		timer->deleteLater();

	// attendees[] is replaced wholesale by PATCH, so the current list has
	// to be read first, then every accumulated delta applied to it at
	// once, rather than one read-modify-write cycle per delta.
	m_client->getEvent(
		batch.calendarId, batch.eventId,
		[this, batch]( QJsonObject event, QString code, QString message ) {
			if ( !code.isEmpty() )
			{
				for ( const PendingAttendeeReply &p : batch.pending )
					p.reply( Result::failure( p.commandId, code, message ) );
				return;
			}

			QJsonArray next;
			QSet<QString> stillPresent; // person names already kept in `next`
			for ( const QJsonValue &v : event.value( "attendees" ).toArray() )
			{
				const QString email = v.toObject().value( "email" ).toString();
				QString matchedPerson;
				for ( auto deltaIt = batch.deltas.constBegin(); deltaIt != batch.deltas.constEnd(); ++deltaIt )
				{
					if ( personHasEmail( m_peopleByName.value( deltaIt.key() ), email ) )
					{
						matchedPerson = deltaIt.key();
						break;
					}
				}
				if ( matchedPerson.isEmpty() )
				{
					next.append( v ); // an attendee no tap in this batch touched
					continue;
				}
				stillPresent.insert( matchedPerson );
				if ( batch.deltas.value( matchedPerson ) )
					next.append( v ); // staying invited
				// else: uninvited — dropped from `next`
			}
			for ( auto deltaIt = batch.deltas.constBegin(); deltaIt != batch.deltas.constEnd(); ++deltaIt )
			{
				if ( deltaIt.value() && !stillPresent.contains( deltaIt.key() ) )
					next.append( QJsonObject{
						{ "email", m_peopleByName.value( deltaIt.key() ).emails.first() } } );
			}

			QJsonObject patchBody;
			patchBody["attendees"] = next;
			// The freshly-fetched event's etag, not batch.etag (the one the
			// kiosk's original tap was sent with) — that one can be
			// significantly stale by now: this PATCH may only be happening
			// after the delegated-auth device-code flow finished, which can
			// take minutes, so matching against a snapshot from before that
			// wait started would spuriously 412 even when nothing else
			// touched the event.
			m_client->patchEvent(
				batch.calendarId, batch.eventId, event.value( "etag" ).toString(), patchBody,
				[batch]( QJsonObject, QString patchCode, QString patchMessage ) {
					for ( const PendingAttendeeReply &p : batch.pending )
					{
						p.reply( patchCode.isEmpty()
									 ? Result::success( p.commandId )
									 : Result::failure( p.commandId, patchCode, patchMessage ) );
					}
				} );
		} );
}
