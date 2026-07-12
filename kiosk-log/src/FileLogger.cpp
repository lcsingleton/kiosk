#include "kiosk-log/FileLogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>

#include <cstdio>
#include <cstdlib>

namespace
{

QFile *g_logFile = nullptr;
QMutex g_mutex;

const char *levelName( QtMsgType type )
{
	switch ( type )
	{
		case QtDebugMsg:
			return "DEBUG";
		case QtInfoMsg:
			return "INFO";
		case QtWarningMsg:
			return "WARNING";
		case QtCriticalMsg:
			return "CRITICAL";
		case QtFatalMsg:
			return "FATAL";
	}
	return "UNKNOWN";
}

// Installing a custom handler fully replaces Qt's own default one (there's
// no chaining), so this has to reproduce the stderr write itself, not just
// add a file on top of whatever the default handler would have done.
void messageHandler( QtMsgType type, const QMessageLogContext &context, const QString &message )
{
	Q_UNUSED( context )

	const QString line = QStringLiteral( "%1 [%2] %3" )
							  .arg( QDateTime::currentDateTime().toString( "yyyy-MM-dd HH:mm:ss.zzz" ),
									QString::fromLatin1( levelName( type ) ), message );

	fprintf( stderr, "%s\n", qPrintable( line ) );

	QMutexLocker locker( &g_mutex );
	if ( g_logFile )
	{
		QTextStream stream( g_logFile );
		stream << line << '\n';
		stream.flush();
	}
	locker.unlock();

	if ( type == QtFatalMsg )
		abort();
}

} // namespace

void FileLogger::install( const QString &moduleName )
{
	const QString dirPath = QStringLiteral( "/var/log/%1" ).arg( moduleName );
	QDir().mkpath( dirPath );

	auto *file = new QFile( QDir( dirPath ).filePath( moduleName + ".log" ) );
	if ( file->open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) )
	{
		g_logFile = file;
	}
	else
	{
		fprintf( stderr,
				 "FileLogger: cannot open %s for writing (%s) — logging to stderr only\n",
				 qPrintable( file->fileName() ), qPrintable( file->errorString() ) );
		delete file;
	}

	qInstallMessageHandler( messageHandler );
}
