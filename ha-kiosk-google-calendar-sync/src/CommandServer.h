#pragma once

#include <QHash>
#include <QLocalServer>
#include <QObject>
#include <QVector>
#include <functional>

#include "Config.h"
#include "calendar-sync-client/CommandTypes.h"

class CalendarClient;
class QLocalSocket;
class QTimer;

// One in-flight commandId waiting on a debounced attendee batch, paired
// with the reply callback CommandServer::handleLine built for it — each
// batched command still gets its own Result when the batch finally flushes.
struct PendingAttendeeReply
{
	QString commandId;
	std::function<void( Result )> reply;
};

// Accumulates invite/uninvite taps for one event (keyed by calendarId +
// eventId) across a short debounce window, so tapping several people's
// badges in quick succession produces one GET+PATCH round trip instead of
// one per tap — see CommandServer::queueAttendeeChange.
struct PendingAttendeeBatch
{
	QString calendarId;
	QString eventId;
	QString etag;
	QHash<QString, bool> deltas; // person name -> desired invited state, last tap wins
	QVector<PendingAttendeeReply> pending;
};

// Listens on a Unix domain socket for NDJSON command lines from the kiosk
// app, dispatches each by its `action` string through a lookup table (so
// adding a new command later is one new handler, not a protocol change),
// and writes an NDJSON Result line back on the same connection.
class CommandServer : public QObject
{
	Q_OBJECT
  public:
	// `people`: the configured household, used to resolve InviteParticipant/
	// UninviteParticipant's payload.person (a name) to the email address(es)
	// that identifies them as a Calendar API attendee — only this class and
	// SnapshotBuilder ever see those emails; they never reach the UI.
	explicit CommandServer( CalendarClient *client, const QVector<PersonConfig> &people,
							QObject *parent = nullptr );

	// Removes any stale socket file from a previous run, then listens.
	bool listen( const QString &socketPath, QString &error );

  signals:
	// Emitted after any command completes successfully, so main.cpp can
	// refresh the snapshot immediately instead of waiting for the next
	// poll tick.
	void writeSucceeded();

  private:
	using Handler = std::function<void( const Command &, std::function<void( Result )> )>;

	void onNewConnection();
	void handleLine( QLocalSocket *socket, const QByteArray &line );
	void dispatch( const Command &cmd, std::function<void( Result )> reply );
	void sendResult( QLocalSocket *socket, const Result &result );

	void handleSchedule( const Command &cmd, std::function<void( Result )> reply );
	void handleReschedule( const Command &cmd, std::function<void( Result )> reply );
	void handleCancel( const Command &cmd, std::function<void( Result )> reply );
	void handleRename( const Command &cmd, std::function<void( Result )> reply );
	void handleChangeLocation( const Command &cmd, std::function<void( Result )> reply );
	void handleInviteParticipant( const Command &cmd, std::function<void( Result )> reply );
	void handleUninviteParticipant( const Command &cmd, std::function<void( Result )> reply );

	// Shared by handleRename/handleChangeLocation: both patch a single
	// Google Calendar event field to a caller-supplied value.
	void patchField( const Command &cmd, const QString &jsonKey, const QString &value,
					 std::function<void( Result )> reply );

	// Shared by handleInviteParticipant/handleUninviteParticipant: records
	// this tap's desired state for `person` on the (calendarId, eventId)
	// batch, (re)starting that batch's debounce timer, rather than hitting
	// the network immediately.
	void queueAttendeeChange( const Command &cmd, const QString &person, bool invited,
							  std::function<void( Result )> reply );
	// Fires once a batch's debounce window elapses with no further taps:
	// one GET, one PATCH applying every accumulated delta, then one Result
	// per originally-queued commandId.
	void flushAttendeeBatch( const QString &key );

	CalendarClient *m_client;
	QLocalServer m_server;
	QHash<QLocalSocket *, QByteArray> m_recvBuffers;
	QHash<QString, Handler> m_handlers;
	QHash<QString, PersonConfig> m_peopleByName;
	QHash<QString, PendingAttendeeBatch> m_pendingAttendeeBatches;
	QHash<QString, QTimer *> m_attendeeDebounceTimers;
};
