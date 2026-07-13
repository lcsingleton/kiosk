#pragma once

#include <QLocalSocket>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

class QJsonObject;

/// Client side of the calendar-sync daemon's command socket protocol (see
/// CommandTypes.h for the wire format). Owns the connection: dials the
/// daemon's Unix domain socket, reconnects on drop, and turns each intent
/// call below into one NDJSON command line, returning the generated
/// commandId immediately. The eventual result arrives asynchronously as
/// commandSucceeded/commandFailed, keyed by that same commandId.
///
/// This class only knows about the protocol and the socket, not what a
/// caller wants to tell a human about a given command — callers that want
/// user-facing text (e.g. "Adding \"Dentist checkup\"") should track their
/// own commandId -> description mapping alongside these calls.
class CalendarSyncClient : public QObject
{
	Q_OBJECT

  public:
	/// Starts connecting to @p socketPath immediately and keeps retrying on
	/// drop or failure — see the class docs.
	/// @param socketPath Path to the calendar-sync daemon's command socket.
	/// @param parent Standard QObject ownership parent.
	explicit CalendarSyncClient( const QString &socketPath, QObject *parent = nullptr );

	/// Sends a ScheduleEvent command to create a new event; there's no
	/// existing eventId/etag yet so none is needed. @p attendees is
	/// configured person names (e.g. "Mum"), not raw email addresses —
	/// resolved to address(es) daemon-side.
	Q_INVOKABLE QString scheduleEvent( const QString &calendarId,
									   const QString &summary,
									   const QString &startIso,
									   const QString &endIso,
									   const QString &description = QString(),
									   const QStringList &attendees = QStringList() );
	/// Sends a RescheduleEvent command to change an existing event's
	/// start/end. @p etag is a concurrency guard: the daemon reports
	/// errorCode "conflict" if the event has changed upstream since this
	/// etag was read.
	Q_INVOKABLE QString rescheduleEvent( const QString &calendarId,
										 const QString &eventId,
										 const QString &etag,
										 const QString &newStartIso,
										 const QString &newEndIso );
	/// Sends a CancelEvent command to delete an existing event. No payload
	/// beyond calendarId/eventId/etag.
	Q_INVOKABLE QString cancelEvent( const QString &calendarId, const QString &eventId, const QString &etag );
	/// Sends a RenameEvent command to change an existing event's summary/title.
	Q_INVOKABLE QString renameEvent( const QString &calendarId,
									 const QString &eventId,
									 const QString &etag,
									 const QString &newSummary );
	/// Sends a ChangeEventLocation command to change an existing event's location.
	Q_INVOKABLE QString changeEventLocation( const QString &calendarId,
											 const QString &eventId,
											 const QString &etag,
											 const QString &newLocation );
	/// Sends an InviteParticipant command to add an attendee to an existing
	/// event. @p person is a configured person's name (e.g. "Mum"), not a
	/// raw email address — resolved to address(es) daemon-side.
	Q_INVOKABLE QString inviteParticipant( const QString &calendarId,
										   const QString &eventId,
										   const QString &etag,
										   const QString &person );
	/// Sends an UninviteParticipant command to remove an attendee from an
	/// existing event. @p person is a configured person's name (e.g. "Mum"),
	/// not a raw email address — resolved to address(es) daemon-side.
	Q_INVOKABLE QString uninviteParticipant( const QString &calendarId,
											 const QString &eventId,
											 const QString &etag,
											 const QString &person );

  signals:
	/// Emitted when the command identified by @p commandId completed successfully.
	void commandSucceeded( const QString &commandId );
	/// Emitted when the command identified by @p commandId failed. @p
	/// errorCode is one of the closed set documented on Result in
	/// CommandTypes.h.
	void commandFailed( const QString &commandId, const QString &errorCode, const QString &errorMessage );

	/// The daemon fell back to the delegated-user OAuth device flow (see
	/// DelegatedAuth) and needs a human to visit verificationUrl and enter
	/// userCode within expiresInSecs before the invite/uninvite that
	/// triggered it can complete. Not tied to a specific commandId — the
	/// pending command that triggered this still resolves via
	/// commandSucceeded/commandFailed on its own, whenever that grant
	/// finishes or expires.
	void authorizationRequired( const QString &verificationUrl, const QString &userCode, int expiresInSecs );

  private slots:
	void connectToServer();
	void onReadyRead();

  private:
	QString sendCommand( const QString &action,
						 const QString &calendarId,
						 const QString &eventId,
						 const QString &etag,
						 const QJsonObject &payload );
	void handleResultLine( const QByteArray &line );

	QString m_socketPath;
	QLocalSocket m_socket;
	QByteArray m_recvBuffer;
	QTimer m_reconnectTimer;
	QSet<QString> m_pending;
};
