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
#include "InfluxClient.h"
#include "SnapshotBuilder.h"
#include "SnapshotWriter.h"

#include <kiosk-log/FileLogger.h>

namespace
{

// How far back the "weatherHistory" chart segment looks, and how finely
// it's bucketed — fixed rather than configurable, since both are properties
// of the chart they feed (WeatherCard's 24h-history + rest-of-today-
// forecast layout), not a deployment concern.
constexpr int kWeatherHistoryHours = 24;
constexpr int kWeatherHistoryWindowMinutes = 5;

// Fetches BOM's daily/hourly forecast for `geohash`, plus (if `influxClient`
// is non-null) the local Ecowitt station's current conditions and hourly
// temperature history from InfluxDB, builds one snapshot, writes it, then
// invokes `onDone(hadError)`. All calls run concurrently; the snapshot is
// only built once all of them have landed. Without an influxClient,
// currentConditions/weatherHistory are simply left empty in the snapshot —
// there's no BOM fallback for either (see BomClient/SnapshotBuilder).
void runSyncCycle( BomClient *bomClient, InfluxClient *influxClient, const QString &geohash,
					const QString &snapshotPath, std::function<void( bool hadError )> onDone )
{
	auto daily = std::make_shared<QJsonValue>();
	auto hourly = std::make_shared<QJsonValue>();
	auto currentConditions = std::make_shared<QJsonValue>();
	auto weatherHistory = std::make_shared<QJsonValue>();
	auto pending = std::make_shared<int>( influxClient ? 4 : 2 );
	auto hadError = std::make_shared<bool>( false );

	auto maybeFinish = [daily, hourly, currentConditions, weatherHistory, pending, hadError, snapshotPath,
						 onDone]() {
		if ( --( *pending ) > 0 )
			return;

		const QJsonObject snapshot =
			SnapshotBuilder::build( *daily, *hourly, *currentConditions, *weatherHistory );
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

	bomClient->fetchDaily( geohash, [daily, hadError, maybeFinish]( QJsonValue data, QString error ) {
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
	bomClient->fetchHourly( geohash, [hourly, hadError, maybeFinish]( QJsonValue data, QString error ) {
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
	if ( influxClient )
	{
		influxClient->fetchTemperatureHistory(
			kWeatherHistoryHours, kWeatherHistoryWindowMinutes,
			[weatherHistory, hadError, maybeFinish]( QJsonValue data, QString error ) {
				if ( !error.isEmpty() )
				{
					qWarning().noquote() << "failed to fetch weather history:" << error;
					*hadError = true;
				}
				else
				{
					*weatherHistory = data;
				}
				maybeFinish();
			} );
		influxClient->fetchCurrentConditions(
			[currentConditions, hadError, maybeFinish]( QJsonValue data, QString error ) {
				if ( !error.isEmpty() )
				{
					qWarning().noquote() << "failed to fetch current conditions:" << error;
					*hadError = true;
				}
				else
				{
					*currentConditions = data;
				}
				maybeFinish();
			} );
	}
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

	auto bomClient = new BomClient( &app );
	InfluxClient *influxClient = config.influxUrl.isEmpty()
									 ? nullptr
									 : new InfluxClient( config.influxUrl, config.influxOrg, config.influxBucket,
														  config.influxToken, &app );
	if ( !influxClient )
		qInfo().noquote() << "no InfluxDB config — weatherHistory and observations will be empty";

	const bool once = parser.isSet( "once" );
	const QString geohash = config.geohash;
	const QString snapshotPath = config.snapshotPath;
	const int pollIntervalMs = qMax( 1, config.pollIntervalSeconds ) * 1000;

	auto pollTimer = std::make_shared<QTimer>();
	auto cycle = [bomClient, influxClient, geohash, snapshotPath, once]() {
		runSyncCycle( bomClient, influxClient, geohash, snapshotPath, [once]( bool hadError ) {
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
