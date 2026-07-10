#include "CalendarBridge.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

CalendarBridge::CalendarBridge( const QString &snapshotPath, const QString &socketPath, QObject *parent )
	: QObject( parent ), m_snapshotPath( snapshotPath ), m_syncClient( socketPath )
{
	connect( &m_watcher, &QFileSystemWatcher::fileChanged, this, &CalendarBridge::reloadSnapshot );
	connect( &m_watcher, &QFileSystemWatcher::directoryChanged, this, &CalendarBridge::reloadSnapshot );

	// Watch the containing directory too: on a cold boot the snapshot file
	// may not exist yet (the daemon hasn't written its first cycle), and
	// QFileSystemWatcher can only watch paths that already exist.
	const QString dir = QFileInfo( snapshotPath ).absolutePath();
	if ( QDir( dir ).exists() )
		m_watcher.addPath( dir );
	reloadSnapshot();

	connect( &m_syncClient, &CalendarSyncClient::commandSucceeded, this,
			 [this]( const QString &commandId ) {
				 const QString what = m_pendingWhat.take( commandId );
				 emit commandSucceeded( commandId, what );
			 } );
	connect( &m_syncClient, &CalendarSyncClient::commandFailed, this,
			 [this]( const QString &commandId, const QString &errorCode, const QString &errorMessage ) {
				 const QString what = m_pendingWhat.take( commandId );
				 emit commandFailed( commandId, what, errorCode, errorMessage );
			 } );
}

void CalendarBridge::reloadSnapshot()
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

QVariantList CalendarBridge::arrayProperty( const char *key ) const
{
	return m_snapshot.value( QLatin1String( key ) ).toArray().toVariantList();
}

QVariantList CalendarBridge::people() const
{
	return arrayProperty( "people" );
}
QVariantList CalendarBridge::todayHighlights() const
{
	return arrayProperty( "todayHighlights" );
}
QVariantList CalendarBridge::todaySchedule() const
{
	return arrayProperty( "todaySchedule" );
}
QVariantList CalendarBridge::weekend() const
{
	return arrayProperty( "weekend" );
}
QVariantList CalendarBridge::upcoming() const
{
	return arrayProperty( "upcoming" );
}

QString CalendarBridge::scheduleEvent( const QString &calendarId, const QString &summary,
									   const QString &startIso, const QString &endIso,
									   const QString &description )
{
	const QString commandId = m_syncClient.scheduleEvent( calendarId, summary, startIso, endIso, description );
	m_pendingWhat.insert( commandId, QStringLiteral( "Adding \"%1\"" ).arg( summary ) );
	return commandId;
}

QString CalendarBridge::rescheduleEvent( const QString &calendarId, const QString &eventId,
										 const QString &etag, const QString &newStartIso,
										 const QString &newEndIso )
{
	const QString commandId =
		m_syncClient.rescheduleEvent( calendarId, eventId, etag, newStartIso, newEndIso );
	m_pendingWhat.insert( commandId, QStringLiteral( "Rescheduling event" ) );
	return commandId;
}

QString CalendarBridge::cancelEvent( const QString &calendarId, const QString &eventId, const QString &etag )
{
	const QString commandId = m_syncClient.cancelEvent( calendarId, eventId, etag );
	m_pendingWhat.insert( commandId, QStringLiteral( "Cancelling event" ) );
	return commandId;
}

QString CalendarBridge::renameEvent( const QString &calendarId, const QString &eventId, const QString &etag,
									 const QString &newSummary )
{
	const QString commandId = m_syncClient.renameEvent( calendarId, eventId, etag, newSummary );
	m_pendingWhat.insert( commandId, QStringLiteral( "Renaming to \"%1\"" ).arg( newSummary ) );
	return commandId;
}

QString CalendarBridge::changeEventLocation( const QString &calendarId, const QString &eventId,
											 const QString &etag, const QString &newLocation )
{
	const QString commandId = m_syncClient.changeEventLocation( calendarId, eventId, etag, newLocation );
	m_pendingWhat.insert( commandId, QStringLiteral( "Changing location" ) );
	return commandId;
}
