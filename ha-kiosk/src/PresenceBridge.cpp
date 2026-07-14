#include "PresenceBridge.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

PresenceBridge::PresenceBridge( const QString &snapshotPath, QObject *parent ) : QObject( parent ), m_snapshotPath( snapshotPath )
{
	connect( &m_watcher, &QFileSystemWatcher::fileChanged, this, &PresenceBridge::reloadSnapshot );
	connect( &m_watcher, &QFileSystemWatcher::directoryChanged, this, &PresenceBridge::reloadSnapshot );

	// Watch the containing directory too: on a cold boot the snapshot file
	// may not exist yet (the daemon hasn't written its first cycle), and
	// QFileSystemWatcher can only watch paths that already exist.
	const QString dir = QFileInfo( snapshotPath ).absolutePath();
	if ( QDir( dir ).exists() )
		m_watcher.addPath( dir );
	reloadSnapshot();
}

void PresenceBridge::reloadSnapshot()
{
	QFile file( m_snapshotPath );
	if ( file.open( QIODevice::ReadOnly ) )
	{
		m_snapshot = QJsonDocument::fromJson( file.readAll() ).object();
		emit presenceChanged();
	}
	// else: m_snapshot and the Q_PROPERTYs backed by it are left at their
	// last-good value and presenceChanged is not emitted, so a transient
	// read failure (e.g. the daemon's rename window) never flashes
	// motionDetected to false.

	// Re-arm the file watch every time: inotify drops a watch across the
	// atomic rename the daemon uses to replace this file, so it must be
	// re-added after every observed change, not just once at startup.
	if ( QFileInfo::exists( m_snapshotPath ) && !m_watcher.files().contains( m_snapshotPath ) )
		m_watcher.addPath( m_snapshotPath );
}

bool PresenceBridge::motionDetected() const
{
	return m_snapshot.value( QLatin1String( "motionDetected" ) ).toBool();
}

QString PresenceBridge::lastMotionAt() const
{
	return m_snapshot.value( QLatin1String( "lastMotionAt" ) ).toString();
}
