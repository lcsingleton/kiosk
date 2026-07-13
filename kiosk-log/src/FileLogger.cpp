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

// Qt may invoke the message handler from any thread that logs, so the file
// handle it writes through is shared state and needs locking around it.
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
								   QString::fromLatin1( levelName( type ) ),
								   message );

	// Raw stdio, not qDebug()/QTextStream(stdout): we're inside the message
	// handler itself, so routing back through Qt's logging would recurse.
	fprintf( stderr, "%s\n", qPrintable( line ) );

	// Only the file write needs the lock — stderr is left to libc, and
	// g_logFile is the only state shared across concurrent logging threads.
	QMutexLocker locker( &g_mutex );
	if ( g_logFile )
	{
		// Re-wrapping the QFile each call is cheap and avoids keeping a
		// QTextStream (with its own buffering) alive across calls; the file
		// itself was opened in Append mode so writes always land at EOF.
		QTextStream stream( g_logFile );
		stream << line << '\n';
		stream.flush();
	}
	locker.unlock();

	// Installing a custom handler suppresses Qt's default fatal-message
	// behavior too, so this has to reproduce the abort() a qFatal() would
	// otherwise trigger.
	if ( type == QtFatalMsg )
		abort();
}

} // namespace

void FileLogger::install( const QString &moduleName )
{
	// Best-effort: the return value is ignored deliberately. If the directory
	// can't be created (missing provisioning, no permission, etc.) the
	// open() below fails instead and that failure is what's actually handled.
	const QString dirPath = QStringLiteral( "/var/log/%1" ).arg( moduleName );
	QDir().mkpath( dirPath );

	auto *file = new QFile( QDir( dirPath ).filePath( moduleName + ".log" ) );
	if ( file->open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) )
	{
		g_logFile = file;
	}
	else
	{
		// File logging is a nice-to-have, not a requirement: warn once via
		// stderr and fall through so g_logFile stays null and every
		// subsequent message just logs to stderr instead of failing outright.
		fprintf( stderr,
				 "FileLogger: cannot open %s for writing (%s) — logging to stderr only\n",
				 qPrintable( file->fileName() ),
				 qPrintable( file->errorString() ) );
		delete file;
	}

	// Installed last, once g_logFile is in its final state, so no message
	// can arrive through the handler while that state is still being set up.
	qInstallMessageHandler( messageHandler );
}
