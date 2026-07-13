#include "calendar-sync-client/CalendarSyncClient.h"
#include "calendar-sync-client/CommandTypes.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

CalendarSyncClient::CalendarSyncClient( const QString &socketPath, QObject *parent )
	: QObject( parent ), m_socketPath( socketPath )
{
	connect( &m_socket, &QLocalSocket::readyRead, this, &CalendarSyncClient::onReadyRead );
	connect( &m_socket, &QLocalSocket::connected, this, [this]() { m_reconnectTimer.stop(); } );
	connect( &m_socket, &QLocalSocket::disconnected, this, [this]() { m_reconnectTimer.start(); } );
	connect(
		&m_socket, &QLocalSocket::errorOccurred, this, [this]( QLocalSocket::LocalSocketError ) { m_reconnectTimer.start(); } );

	// The daemon may not be up yet (or may restart later) — keep retrying
	// on a fixed interval rather than treating "not connected right now" as
	// a terminal state.
	m_reconnectTimer.setInterval( 3000 );
	connect( &m_reconnectTimer, &QTimer::timeout, this, &CalendarSyncClient::connectToServer );
	connectToServer();
}

void CalendarSyncClient::connectToServer()
{
	if ( m_socket.state() == QLocalSocket::ConnectedState || m_socket.state() == QLocalSocket::ConnectingState )
		return;
	m_socket.connectToServer( m_socketPath );
}

QString CalendarSyncClient::sendCommand(
	const QString &action, const QString &calendarId, const QString &eventId, const QString &etag, const QJsonObject &payload )
{
	const QString commandId = QUuid::createUuid().toString( QUuid::WithoutBraces );

	if ( m_socket.state() != QLocalSocket::ConnectedState )
	{
		// Deferred so a caller that connects to commandFailed immediately
		// after calling this (rather than before) still receives it.
		QTimer::singleShot( 0,
							this,
							[this, commandId]()
							{
								emit commandFailed( commandId,
													QStringLiteral( "upstream_unavailable" ),
													QStringLiteral( "not connected to the calendar sync daemon" ) );
							} );
		return commandId;
	}

	QJsonObject obj;
	obj["commandId"] = commandId;
	obj["action"] = action;
	obj["calendarId"] = calendarId;
	obj["eventId"] = eventId;
	obj["etag"] = etag;
	obj["payload"] = payload;

	m_pending.insert( commandId );
	m_socket.write( QJsonDocument( obj ).toJson( QJsonDocument::Compact ) + "\n" );
	m_socket.flush();
	return commandId;
}

QString CalendarSyncClient::scheduleEvent( const QString &calendarId,
										   const QString &summary,
										   const QString &startIso,
										   const QString &endIso,
										   const QString &description,
										   const QStringList &attendees )
{
	const ScheduleEventPayload payload{ summary, startIso, endIso, description, attendees };
	return sendCommand( QStringLiteral( "ScheduleEvent" ), calendarId, QString(), QString(), payload.toJson() );
}

QString CalendarSyncClient::rescheduleEvent(
	const QString &calendarId, const QString &eventId, const QString &etag, const QString &newStartIso, const QString &newEndIso )
{
	const RescheduleEventPayload payload{ newStartIso, newEndIso };
	return sendCommand( QStringLiteral( "RescheduleEvent" ), calendarId, eventId, etag, payload.toJson() );
}

QString CalendarSyncClient::cancelEvent( const QString &calendarId, const QString &eventId, const QString &etag )
{
	return sendCommand( QStringLiteral( "CancelEvent" ), calendarId, eventId, etag, QJsonObject() );
}

QString CalendarSyncClient::renameEvent( const QString &calendarId,
										 const QString &eventId,
										 const QString &etag,
										 const QString &newSummary )
{
	const RenameEventPayload payload{ newSummary };
	return sendCommand( QStringLiteral( "RenameEvent" ), calendarId, eventId, etag, payload.toJson() );
}

QString CalendarSyncClient::changeEventLocation( const QString &calendarId,
												 const QString &eventId,
												 const QString &etag,
												 const QString &newLocation )
{
	const ChangeEventLocationPayload payload{ newLocation };
	return sendCommand( QStringLiteral( "ChangeEventLocation" ), calendarId, eventId, etag, payload.toJson() );
}

QString CalendarSyncClient::inviteParticipant( const QString &calendarId,
											   const QString &eventId,
											   const QString &etag,
											   const QString &person )
{
	const ParticipantPayload payload{ person };
	return sendCommand( QStringLiteral( "InviteParticipant" ), calendarId, eventId, etag, payload.toJson() );
}

QString CalendarSyncClient::uninviteParticipant( const QString &calendarId,
												 const QString &eventId,
												 const QString &etag,
												 const QString &person )
{
	const ParticipantPayload payload{ person };
	return sendCommand( QStringLiteral( "UninviteParticipant" ), calendarId, eventId, etag, payload.toJson() );
}

void CalendarSyncClient::onReadyRead()
{
	// A single readyRead can deliver a partial line, several whole lines, or
	// both; buffer across calls and only hand complete lines to
	// handleResultLine, leaving any trailing partial line for next time.
	m_recvBuffer += m_socket.readAll();
	int idx;
	while ( ( idx = m_recvBuffer.indexOf( '\n' ) ) >= 0 )
	{
		const QByteArray line = m_recvBuffer.left( idx );
		m_recvBuffer.remove( 0, idx + 1 );
		if ( !line.trimmed().isEmpty() )
			handleResultLine( line );
	}
}

void CalendarSyncClient::handleResultLine( const QByteArray &line )
{
	const QJsonObject obj = QJsonDocument::fromJson( line ).object();

	// Unsolicited pushes (only AuthorizationPending today) share this same
	// line stream with command results; an "event" key rather than
	// "commandId" is what tells the two apart.
	if ( obj.value( "event" ).toString() == QLatin1String( CommandEvent::AuthorizationPending ) )
	{
		emit authorizationRequired(
			obj.value( "verificationUrl" ).toString(), obj.value( "userCode" ).toString(), obj.value( "expiresInSecs" ).toInt() );
		return;
	}

	const QString commandId = obj.value( "commandId" ).toString();

	const auto it = m_pending.find( commandId );
	if ( it == m_pending.end() )
		return; // unknown or already-handled commandId
	m_pending.erase( it );

	if ( obj.value( "status" ).toString() == QLatin1String( "ok" ) )
	{
		emit commandSucceeded( commandId );
	}
	else
	{
		const QJsonObject err = obj.value( "error" ).toObject();
		emit commandFailed( commandId, err.value( "code" ).toString(), err.value( "message" ).toString() );
	}
}
