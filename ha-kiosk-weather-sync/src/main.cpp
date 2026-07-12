#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonValue>
#include <QLockFile>
#include <QTimer>

#include <memory>

#include "BomClient.h"
#include "Config.h"
#include "SnapshotBuilder.h"
#include "SnapshotWriter.h"

#include <kiosk-log/FileLogger.h>

namespace
{

// Fetches BOM's daily/hourly forecast and current observations for
// `geohash`, builds one snapshot, writes it, then invokes
// `onDone(hadError)`. All three calls run concurrently; the snapshot is
// only built once all three have landed.
void runSyncCycle( BomClient *client, const QString &geohash, const QString &snapshotPath,
					std::function<void( bool hadError )> onDone )
{
	auto daily = std::make_shared<QJsonValue>();
	auto hourly = std::make_shared<QJsonValue>();
	auto observations = std::make_shared<QJsonValue>();
	auto pending = std::make_shared<int>( 3 );
	auto hadError = std::make_shared<bool>( false );

	auto maybeFinish = [daily, hourly, observations, pending, hadError, snapshotPath, onDone]() {
		if ( --( *pending ) > 0 )
			return;

		const QJsonObject snapshot = SnapshotBuilder::build( *daily, *hourly, *observations );
		QString writeError;
		if ( SnapshotWriter::write( snapshotPath, snapshot, writeError ) )
		{
			qInfo().noquote() << "snapshot written to" << snapshotPath;
		}
		else
		{
			qCritical().noquote() << "failed to write snapshot:" << writeError;
			*hadError = true;
		}
		onDone( *hadError );
	};

	client->fetchDaily( geohash, [daily, hadError, maybeFinish]( QJsonValue data, QString error ) {
		if ( !error.isEmpty() )
		{
			qWarning().noquote() << "failed to fetch daily forecast:" << error;
			*hadError = true;
		}
		else
		{
			*daily = data;
		}
		maybeFinish();
	} );
	client->fetchHourly( geohash, [hourly, hadError, maybeFinish]( QJsonValue data, QString error ) {
		if ( !error.isEmpty() )
		{
			qWarning().noquote() << "failed to fetch hourly forecast:" << error;
			*hadError = true;
		}
		else
		{
			*hourly = data;
		}
		maybeFinish();
	} );
	client->fetchObservations(
		geohash, [observations, hadError, maybeFinish]( QJsonValue data, QString error ) {
			if ( !error.isEmpty() )
			{
				qWarning().noquote() << "failed to fetch observations:" << error;
				*hadError = true;
			}
			else
			{
				*observations = data;
			}
			maybeFinish();
		} );
}

} // namespace

int main( int argc, char *argv[] )
{
	FileLogger::install( "ha-kiosk-weather-sync" );

	QCoreApplication app( argc, argv );
	QCoreApplication::setApplicationName( "ha-kiosk-weather-sync" );

	QCommandLineParser parser;
	parser.addOption( { "config", "Path to daemon config JSON.", "path" } );
	parser.addOption(
		{ "once", "Run a single sync cycle and exit, instead of polling forever (for testing)." } );
	parser.process( app );

	const QString configPath = parser.value( "config" );
	if ( configPath.isEmpty() )
	{
		qCritical() << "usage: ha-kiosk-weather-sync --config <path> [--once]";
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
	QLockFile lockFile( runtimeDir + "/ha-kiosk-weather-sync.lock" );
	if ( !lockFile.tryLock() )
	{
		qCritical().noquote() << "another instance is already running (lock held at" << lockFile.fileName()
							  << ")";
		return 1;
	}

	auto client = new BomClient( &app );
	const bool once = parser.isSet( "once" );
	const QString geohash = config.geohash;
	const QString snapshotPath = config.snapshotPath;
	const int pollIntervalMs = qMax( 1, config.pollIntervalSeconds ) * 1000;

	auto pollTimer = std::make_shared<QTimer>();
	auto cycle = [client, geohash, snapshotPath, once]() {
		runSyncCycle( client, geohash, snapshotPath, [once]( bool hadError ) {
			if ( once )
				QTimer::singleShot( 0, qApp,
									[hadError]() { QCoreApplication::exit( hadError ? 1 : 0 ); } );
		} );
	};

	if ( !once )
	{
		pollTimer->setInterval( pollIntervalMs );
		QObject::connect( pollTimer.get(), &QTimer::timeout, cycle );
		pollTimer->start();
		qInfo().noquote() << "polling every" << config.pollIntervalSeconds << "seconds";
	}

	cycle();

	return app.exec();
}
