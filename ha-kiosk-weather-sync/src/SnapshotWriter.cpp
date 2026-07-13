#include "SnapshotWriter.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

bool SnapshotWriter::write( const QString &path, const QJsonObject &snapshot, QString &error )
{
	// snapshotPath's directory may not exist yet (e.g. a fresh runtime
	// directory right after boot), so create it up front instead of failing
	// the write.
	const QDir dir = QFileInfo( path ).dir();
	if ( !dir.exists() && !dir.mkpath( "." ) )
	{
		error = QStringLiteral( "cannot create directory %1" ).arg( dir.path() );
		return false;
	}

	// QSaveFile writes to a temp file in the same directory and only renames
	// it over `path` on commit(), so a reader (WeatherBridge) never observes
	// a partially-written file, and a crash or power loss mid-write leaves
	// the previous snapshot in place rather than a truncated one.
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
