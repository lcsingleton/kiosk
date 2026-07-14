#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "CalendarBridge.h"
#include "IdleController.h"
#include "PresenceBridge.h"
#include "WeatherBridge.h"

#include <kiosk-log/FileLogger.h>

int main( int argc, char *argv[] )
{
	FileLogger::install( "ha-kiosk" );

	QGuiApplication app( argc, argv );

	QCommandLineParser parser;
	parser.addOption( { "calendar-snapshot",
						"Path to the calendar-sync daemon's snapshot JSON.",
						"path",
						"/run/kiosk/calendar-snapshot.json" } );
	parser.addOption(
		{ "calendar-socket", "Path to the calendar-sync daemon's command socket.", "path", "/run/kiosk/calendar-sync.sock" } );
	parser.addOption(
		{ "weather-snapshot", "Path to the weather-sync daemon's snapshot JSON.", "path", "/run/kiosk/weather-snapshot.json" } );
	parser.addOption(
		{ "presence-snapshot", "Path to the presence-sync daemon's snapshot JSON.", "path", "/run/kiosk/presence-snapshot.json" } );
	parser.addOption( { "idle-timeout-ms", "Milliseconds of no activity before the screen dims.", "ms", "300000" } );
	parser.process( app );

	CalendarBridge calendarBridge( parser.value( "calendar-snapshot" ), parser.value( "calendar-socket" ) );
	WeatherBridge weatherBridge( parser.value( "weather-snapshot" ) );
	PresenceBridge presenceBridge( parser.value( "presence-snapshot" ) );
	IdleController idleController( parser.value( "idle-timeout-ms" ).toLongLong() );

	// Approaching the tablet counts as activity even without a touch, so it
	// wakes/stays awake the same way a tap would — but only while the
	// camera currently sees motion, never on every snapshot reload (e.g. a
	// heartbeat write with motionDetected still false shouldn't reset the
	// idle clock).
	QObject::connect( &presenceBridge,
					   &PresenceBridge::presenceChanged,
					   &idleController,
					   [&presenceBridge, &idleController]()
					   {
						   if ( presenceBridge.motionDetected() )
							   idleController.reportActivity();
					   } );

	QQmlApplicationEngine engine;
	engine.rootContext()->setContextProperty( "calendarBridge", &calendarBridge );
	engine.rootContext()->setContextProperty( "weatherBridge", &weatherBridge );
	engine.rootContext()->setContextProperty( "presenceBridge", &presenceBridge );
	engine.rootContext()->setContextProperty( "idleController", &idleController );

	// qml/ and assets/ both install under share/ha-kiosk/ (FHS:
	// architecture-independent data doesn't belong next to the binary),
	// always one fixed hop from wherever the binary itself landed — see
	// ha-kiosk/CMakeLists.txt.
	const QString shareDir = QDir::cleanPath( QCoreApplication::applicationDirPath() + "/../share/ha-kiosk" );
	const QString assetsPath = shareDir + "/assets";
	// Trailing slash so QML can build a full asset URL by simply appending a filename
	// (e.g. assetsUrl + "sunny.svg") without needing its own path-joining logic.
	engine.rootContext()->setContextProperty( "assetsUrl", QUrl::fromLocalFile( assetsPath + "/" ) );

	const QString qmlPath = QDir( shareDir + "/qml" ).filePath( "main.qml" );
	engine.load( QUrl::fromLocalFile( qmlPath ) );

	if ( engine.rootObjects().isEmpty() )
		return -1;

	return app.exec();
}
