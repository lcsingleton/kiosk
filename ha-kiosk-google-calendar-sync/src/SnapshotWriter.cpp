#include "SnapshotWriter.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

bool SnapshotWriter::write( const QString &path, const QJsonObject &snapshot, QString &error )
{
	const QDir dir = QFileInfo( path ).dir();
	if ( !dir.exists() && !dir.mkpath( "." ) )
	{
		error = QStringLiteral( "cannot create directory %1" ).arg( dir.path() );
		return false;
	}

	// QSaveFile writes to a temp file alongside `path` and only replaces the
	// real file — via rename(), atomic on POSIX — once commit() succeeds, so
	// a reader on the app side (CalendarBridge polling this same path) never
	// observes a partially written file, and a crash/power loss mid-write
	// leaves the previous snapshot intact instead of a truncated one.
	QSaveFile file( path );
	if ( !file.open( QIODevice::WriteOnly ) )
	{
		error = QStringLiteral( "cannot open %1 for writing: %2" ).arg( path, file.errorString() );
		return false;
	}
	file.write( QJsonDocument( snapshot ).toJson( QJsonDocument::Compact ) );
	if ( !file.commit() )
	{
		error = QStringLiteral( "failed to commit %1: %2" ).arg( path, file.errorString() );
		return false;
	}
	return true;
}
