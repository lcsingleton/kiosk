#pragma once

#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QString>

/// Reads the presence-sync daemon's JSON snapshot file. Read-only, same
/// shape as WeatherBridge: no command-socket client, no Q_INVOKABLE
/// mutators — camera-derived presence is state the kiosk only ever observes.
class PresenceBridge : public QObject
{
	Q_OBJECT
	/// Whether the camera currently considers someone/something present —
	/// true from the moment motion is detected until the daemon's
	/// presenceHoldSeconds after the last detected change.
	Q_PROPERTY( bool motionDetected READ motionDetected NOTIFY presenceChanged )
	/// ISO-8601 UTC instant of the last detected change, or an empty string
	/// if no motion has been detected yet this run.
	Q_PROPERTY( QString lastMotionAt READ lastMotionAt NOTIFY presenceChanged )

  public:
	/// @param snapshotPath Path to the presence-sync daemon's JSON snapshot
	/// file, watched and reloaded on change.
	/// @param parent Standard QObject ownership parent.
	explicit PresenceBridge( const QString &snapshotPath, QObject *parent = nullptr );

	/// @see motionDetected
	bool motionDetected() const;
	/// @see lastMotionAt
	QString lastMotionAt() const;

  signals:
	/// Fires whenever the snapshot file is reloaded (initial load or a
	/// change on disk); backs both Q_PROPERTYs above.
	void presenceChanged();

  private slots:
	void reloadSnapshot();

  private:
	QString m_snapshotPath;
	QFileSystemWatcher m_watcher;
	QJsonObject m_snapshot;
};
