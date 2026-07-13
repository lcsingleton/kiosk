#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <calendar-sync-client/CalendarSyncClient.h>

/// Reads the calendar-sync daemon's JSON snapshot file (read side) and sends
/// intent-named commands over its command socket (write side, via
/// CalendarSyncClient — see that class for the wire protocol itself).
///
/// Read side: DashboardData.qml binds its calendar-related properties
/// directly onto this — see the "swap the source, keep the fields" comment
/// there — and they update whenever the snapshot file changes.
///
/// Write side: touch gestures call the Q_INVOKABLE methods below, each of
/// which sends one command and returns a commandId immediately; the eventual
/// result arrives asynchronously as commandSucceeded/commandFailed, keyed by
/// that same commandId, so a caller can show specific feedback ("Couldn't
/// reschedule 'Dentist checkup': ...") rather than a generic error. This
/// class's own job on the write side is just remembering, per commandId,
/// the human-facing description of what was asked for — the protocol and
/// socket itself belong to CalendarSyncClient.
class CalendarBridge : public QObject
{
	Q_OBJECT
	/// Household members from the snapshot's "people" field, for QML to render.
	Q_PROPERTY( QVariantList people READ people NOTIFY snapshotChanged )
	/// Today's highlighted/important events from the snapshot's "todayHighlights" field.
	Q_PROPERTY( QVariantList todayHighlights READ todayHighlights NOTIFY snapshotChanged )
	/// Today's full event list from the snapshot's "todaySchedule" field.
	Q_PROPERTY( QVariantList todaySchedule READ todaySchedule NOTIFY snapshotChanged )
	/// Weekend's events from the snapshot's "weekend" field.
	Q_PROPERTY( QVariantList weekend READ weekend NOTIFY snapshotChanged )
	/// Later upcoming events from the snapshot's "upcoming" field.
	Q_PROPERTY( QVariantList upcoming READ upcoming NOTIFY snapshotChanged )
	/// Which calendar a newly-created event lands on — see main.cpp's
	/// "defaultCalendarId" comment for why this isn't per-person.
	Q_PROPERTY( QString defaultCalendarId READ defaultCalendarId NOTIFY snapshotChanged )

  public:
	/// @param snapshotPath Path to the calendar-sync daemon's JSON snapshot file, watched and reloaded on
	/// change.
	/// @param socketPath Path to the calendar-sync daemon's command socket, passed through to
	/// CalendarSyncClient.
	/// @param parent Standard QObject ownership parent.
	explicit CalendarBridge( const QString &snapshotPath, const QString &socketPath, QObject *parent = nullptr );

	/// @see people
	QVariantList people() const;
	/// @see todayHighlights
	QVariantList todayHighlights() const;
	/// @see todaySchedule
	QVariantList todaySchedule() const;
	/// @see weekend
	QVariantList weekend() const;
	/// @see upcoming
	QVariantList upcoming() const;
	/// @see defaultCalendarId
	QString defaultCalendarId() const;

	// Each returns the generated commandId immediately (before the result
	// is known) so QML can correlate it with commandSucceeded/commandFailed
	// if it wants to, though most callers will just react to the signals.
	/// Sends a create-event command for a new event with the given fields; result arrives via
	/// commandSucceeded/commandFailed.
	Q_INVOKABLE QString scheduleEvent( const QString &calendarId,
									   const QString &summary,
									   const QString &startIso,
									   const QString &endIso,
									   const QString &description = QString(),
									   const QStringList &attendees = QStringList() );
	/// Sends a command to move an existing event to a new start/end time; result arrives via
	/// commandSucceeded/commandFailed.
	Q_INVOKABLE QString rescheduleEvent( const QString &calendarId,
										 const QString &eventId,
										 const QString &etag,
										 const QString &newStartIso,
										 const QString &newEndIso );
	/// Sends a command to delete an existing event; result arrives via commandSucceeded/commandFailed.
	Q_INVOKABLE QString cancelEvent( const QString &calendarId, const QString &eventId, const QString &etag );
	/// Sends a command to change an existing event's summary/title; result arrives via
	/// commandSucceeded/commandFailed.
	Q_INVOKABLE QString renameEvent( const QString &calendarId,
									 const QString &eventId,
									 const QString &etag,
									 const QString &newSummary );
	/// Sends a command to change an existing event's location; result arrives via
	/// commandSucceeded/commandFailed.
	Q_INVOKABLE QString changeEventLocation( const QString &calendarId,
											 const QString &eventId,
											 const QString &etag,
											 const QString &newLocation );
	/// Sends a command to add a participant to an existing event; result arrives via
	/// commandSucceeded/commandFailed.
	Q_INVOKABLE QString inviteParticipant( const QString &calendarId,
										   const QString &eventId,
										   const QString &etag,
										   const QString &person );
	/// Sends a command to remove a participant from an existing event; result arrives via
	/// commandSucceeded/commandFailed.
	Q_INVOKABLE QString uninviteParticipant( const QString &calendarId,
											 const QString &eventId,
											 const QString &etag,
											 const QString &person );

  signals:
	/// Fires whenever the snapshot file is reloaded (initial load or a change on disk); backs every
	/// Q_PROPERTY above.
	void snapshotChanged();
	/// Fires when the daemon reports a command (identified by commandId) completed successfully.
	void commandSucceeded( const QString &commandId, const QString &what );
	/// Fires when the daemon reports a command (identified by commandId) failed.
	void commandFailed( const QString &commandId, const QString &what, const QString &errorCode, const QString &errorMessage );
	/// Fires when CalendarSyncClient reports the daemon needs the user to complete an OAuth device-code flow.
	void authorizationRequired( const QString &verificationUrl, const QString &userCode, int expiresInSecs );

  private slots:
	void reloadSnapshot();

  private:
	QVariantList arrayProperty( const char *key ) const;

	QString m_snapshotPath;
	QFileSystemWatcher m_watcher;
	QJsonObject m_snapshot;

	CalendarSyncClient m_syncClient;
	QHash<QString, QString> m_pendingWhat;
};
