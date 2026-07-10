#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>

#include <calendar-sync-client/CalendarSyncClient.h>

// Reads the calendar-sync daemon's JSON snapshot file (read side) and sends
// intent-named commands over its command socket (write side, via
// CalendarSyncClient — see that class for the wire protocol itself).
//
// Read side: DashboardData.qml binds its calendar-related properties
// directly onto this — see the "swap the source, keep the fields" comment
// there — and they update whenever the snapshot file changes.
//
// Write side: touch gestures call the Q_INVOKABLE methods below, each of
// which sends one command and returns a commandId immediately; the eventual
// result arrives asynchronously as commandSucceeded/commandFailed, keyed by
// that same commandId, so a caller can show specific feedback ("Couldn't
// reschedule 'Dentist checkup': ...") rather than a generic error. This
// class's own job on the write side is just remembering, per commandId,
// the human-facing description of what was asked for — the protocol and
// socket itself belong to CalendarSyncClient.
class CalendarBridge : public QObject
{
	Q_OBJECT
	Q_PROPERTY( QVariantList people READ people NOTIFY snapshotChanged )
	Q_PROPERTY( QVariantList todayHighlights READ todayHighlights NOTIFY snapshotChanged )
	Q_PROPERTY( QVariantList todaySchedule READ todaySchedule NOTIFY snapshotChanged )
	Q_PROPERTY( QVariantList weekend READ weekend NOTIFY snapshotChanged )
	Q_PROPERTY( QVariantList upcoming READ upcoming NOTIFY snapshotChanged )

  public:
	explicit CalendarBridge( const QString &snapshotPath, const QString &socketPath,
							 QObject *parent = nullptr );

	QVariantList people() const;
	QVariantList todayHighlights() const;
	QVariantList todaySchedule() const;
	QVariantList weekend() const;
	QVariantList upcoming() const;

	// Each returns the generated commandId immediately (before the result
	// is known) so QML can correlate it with commandSucceeded/commandFailed
	// if it wants to, though most callers will just react to the signals.
	Q_INVOKABLE QString scheduleEvent( const QString &calendarId, const QString &summary,
									   const QString &startIso, const QString &endIso,
									   const QString &description = QString() );
	Q_INVOKABLE QString rescheduleEvent( const QString &calendarId, const QString &eventId,
										 const QString &etag, const QString &newStartIso,
										 const QString &newEndIso );
	Q_INVOKABLE QString cancelEvent( const QString &calendarId, const QString &eventId, const QString &etag );
	Q_INVOKABLE QString renameEvent( const QString &calendarId, const QString &eventId, const QString &etag,
									 const QString &newSummary );
	Q_INVOKABLE QString changeEventLocation( const QString &calendarId, const QString &eventId,
											 const QString &etag, const QString &newLocation );

  signals:
	void snapshotChanged();
	void commandSucceeded( const QString &commandId, const QString &what );
	void commandFailed( const QString &commandId, const QString &what, const QString &errorCode,
						const QString &errorMessage );

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
