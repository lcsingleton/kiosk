#pragma once

#include <QLocalSocket>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

class QJsonObject;

// Client side of the calendar-sync daemon's command socket protocol (see
// CommandTypes.h for the wire format). Owns the connection: dials the
// daemon's Unix domain socket, reconnects on drop, and turns each intent
// call below into one NDJSON command line, returning the generated
// commandId immediately. The eventual result arrives asynchronously as
// commandSucceeded/commandFailed, keyed by that same commandId.
//
// This class only knows about the protocol and the socket, not what a
// caller wants to tell a human about a given command — callers that want
// user-facing text (e.g. "Adding \"Dentist checkup\"") should track their
// own commandId -> description mapping alongside these calls.
class CalendarSyncClient : public QObject
{
	Q_OBJECT

  public:
	explicit CalendarSyncClient( const QString &socketPath, QObject *parent = nullptr );

	Q_INVOKABLE QString scheduleEvent( const QString &calendarId, const QString &summary,
									   const QString &startIso, const QString &endIso,
									   const QString &description = QString(),
									   const QStringList &attendees = QStringList() );
	Q_INVOKABLE QString rescheduleEvent( const QString &calendarId, const QString &eventId,
										 const QString &etag, const QString &newStartIso,
										 const QString &newEndIso );
	Q_INVOKABLE QString cancelEvent( const QString &calendarId, const QString &eventId, const QString &etag );
	Q_INVOKABLE QString renameEvent( const QString &calendarId, const QString &eventId, const QString &etag,
									 const QString &newSummary );
	Q_INVOKABLE QString changeEventLocation( const QString &calendarId, const QString &eventId,
											 const QString &etag, const QString &newLocation );
	Q_INVOKABLE QString inviteParticipant( const QString &calendarId, const QString &eventId,
										   const QString &etag, const QString &person );
	Q_INVOKABLE QString uninviteParticipant( const QString &calendarId, const QString &eventId,
											 const QString &etag, const QString &person );

  signals:
	void commandSucceeded( const QString &commandId );
	void commandFailed( const QString &commandId, const QString &errorCode, const QString &errorMessage );

	// The daemon fell back to the delegated-user OAuth device flow (see
	// DelegatedAuth) and needs a human to visit verificationUrl and enter
	// userCode within expiresInSecs before the invite/uninvite that
	// triggered it can complete. Not tied to a specific commandId — the
	// pending command that triggered this still resolves via
	// commandSucceeded/commandFailed on its own, whenever that grant
	// finishes or expires.
	void authorizationRequired( const QString &verificationUrl, const QString &userCode, int expiresInSecs );

  private slots:
	void connectToServer();
	void onReadyRead();

  private:
	QString sendCommand( const QString &action, const QString &calendarId, const QString &eventId,
						 const QString &etag, const QJsonObject &payload );
	void handleResultLine( const QByteArray &line );

	QString m_socketPath;
	QLocalSocket m_socket;
	QByteArray m_recvBuffer;
	QTimer m_reconnectTimer;
	QSet<QString> m_pending;
};
