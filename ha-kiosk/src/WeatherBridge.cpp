#include "WeatherBridge.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

WeatherBridge::WeatherBridge( const QString &snapshotPath, QObject *parent )
	: QObject( parent ), m_snapshotPath( snapshotPath )
{
	connect( &m_watcher, &QFileSystemWatcher::fileChanged, this, &WeatherBridge::reloadSnapshot );
	connect( &m_watcher, &QFileSystemWatcher::directoryChanged, this, &WeatherBridge::reloadSnapshot );

	// Watch the containing directory too: on a cold boot the snapshot file
	// may not exist yet (the daemon hasn't written its first cycle), and
	// QFileSystemWatcher can only watch paths that already exist.
	const QString dir = QFileInfo( snapshotPath ).absolutePath();
	if ( QDir( dir ).exists() )
		m_watcher.addPath( dir );
	reloadSnapshot();
}

void WeatherBridge::reloadSnapshot()
{
	QFile file( m_snapshotPath );
	if ( file.open( QIODevice::ReadOnly ) )
	{
		m_snapshot = QJsonDocument::fromJson( file.readAll() ).object();
		emit snapshotChanged();
	}

	// Re-arm the file watch every time: inotify drops a watch across the
	// atomic rename the daemon uses to replace this file, so it must be
	// re-added after every observed change, not just once at startup.
	if ( QFileInfo::exists( m_snapshotPath ) && !m_watcher.files().contains( m_snapshotPath ) )
		m_watcher.addPath( m_snapshotPath );
}

QVariantList WeatherBridge::hourlyForecast() const
{
	return m_snapshot.value( QLatin1String( "hourlyForecast" ) ).toArray().toVariantList();
}

QVariantList WeatherBridge::forecast() const
{
	return m_snapshot.value( QLatin1String( "forecast" ) ).toArray().toVariantList();
}

QVariantMap WeatherBridge::observations() const
{
	return m_snapshot.value( QLatin1String( "observations" ) ).toObject().toVariantMap();
}
