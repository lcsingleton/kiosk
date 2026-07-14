#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QTimer>

#include <memory>

#include <gst/gst.h>

#include "CameraCapture.h"
#include "Config.h"
#include "MotionDetector.h"
#include "SnapshotBuilder.h"

#include <kiosk-log/FileLogger.h>
#include <snapshot-writer/SnapshotWriter.h>

namespace
{
// Presence is sampled several times a second, so — unlike weather-sync,
// which polls every 10 minutes and can just write every cycle — this only
// writes the snapshot when motionDetected actually flips, plus a periodic
// heartbeat so a downstream watcher (PresenceBridge) can tell the daemon is
// still alive during a long, motionless stretch.
constexpr qint64 kHeartbeatMs = 30000;
} // namespace

int main( int argc, char *argv[] )
{
	FileLogger::install( "ha-kiosk-presence-sync" );

	QCoreApplication app( argc, argv );
	QCoreApplication::setApplicationName( "ha-kiosk-presence-sync" );

	// No GStreamer-specific CLI parsing needed, so gst_init consumes nothing
	// from argv — kept separate from QCommandLineParser below.
	gst_init( nullptr, nullptr );

	QCommandLineParser parser;
	parser.addOption( { "config", "Path to daemon config JSON.", "path" } );
	parser.addOption( { "once", "Run a single sample cycle and exit, instead of polling forever (for testing)." } );
	parser.process( app );

	const QString configPath = parser.value( "config" );
	if ( configPath.isEmpty() )
	{
		qCritical() << "usage: ha-kiosk-presence-sync --config <path> [--once]";
		return 1;
	}

	Config config;
	QString error;
	if ( !Config::load( configPath, config, error ) )
	{
		qCritical() << "config error:" << error;
		return 1;
	}

	// Guards against two instances racing over the same snapshot — lives
	// next to snapshotPath (normally /run/kiosk, an FHS runtime directory)
	// rather than a hardcoded path, so a config pointed elsewhere (e.g. for
	// testing) still gets its own lock.
	const QString runtimeDir = QFileInfo( config.snapshotPath ).absolutePath();
	QDir().mkpath( runtimeDir );
	QLockFile lockFile( runtimeDir + "/ha-kiosk-presence-sync.lock" );
	if ( !lockFile.tryLock() )
	{
		qCritical().noquote() << "another instance is already running (lock held at" << lockFile.fileName() << ")";
		return 1;
	}

	CameraCapture camera( config.cameraPipeline );
	MotionDetector detector( config.motionThreshold, config.backgroundAdaptRate, config.presenceHoldSeconds * 1000, config.pixelMaxValue );

	const bool once = parser.isSet( "once" );
	const QString snapshotPath = config.snapshotPath;

	auto lastWritten = std::make_shared<bool>( false );
	auto everWritten = std::make_shared<bool>( false );
	auto lastWriteMs = std::make_shared<qint64>( 0 );
	auto hadError = std::make_shared<bool>( false );

	auto cycle = [&camera, &detector, snapshotPath, lastWritten, everWritten, lastWriteMs, hadError, once]()
	{
		QByteArray frame;
		int width = 0;
		int height = 0;
		int bytesPerPixel = 0;
		const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
		if ( camera.latestFrame( frame, width, height, bytesPerPixel ) )
			detector.update( frame, width, height, bytesPerPixel, nowMs );
		// else: no new frame this cycle — still worth re-checking presence()
		// below, since the hold window may have just expired on its own.

		const bool motionDetected = detector.presence();
		const bool dueForHeartbeat = !*everWritten || ( nowMs - *lastWriteMs ) >= kHeartbeatMs;
		if ( motionDetected != *lastWritten || dueForHeartbeat )
		{
			const QJsonObject snapshot = SnapshotBuilder::build( motionDetected, detector.lastMotionMs() );
			QString writeError;
			if ( SnapshotWriter::write( snapshotPath, snapshot, writeError ) )
			{
				*lastWritten = motionDetected;
				*everWritten = true;
				*lastWriteMs = nowMs;
			}
			else
			{
				qCritical().noquote() << "failed to write snapshot:" << writeError;
				*hadError = true;
			}
		}

		if ( once )
			QTimer::singleShot( 0, qApp, [hadError]() { QCoreApplication::exit( *hadError ? 1 : 0 ); } );
	};

	QTimer pollTimer;
	if ( !once )
	{
		pollTimer.setInterval( qMax( 50, config.pollIntervalMs ) );
		QObject::connect( &pollTimer, &QTimer::timeout, cycle );
		pollTimer.start();
		qInfo().noquote() << "sampling every" << config.pollIntervalMs << "ms";
	}

	cycle();

	return app.exec();
}
