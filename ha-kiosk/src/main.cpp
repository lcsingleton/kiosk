#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "CalendarBridge.h"

int main( int argc, char *argv[] )
{
	QGuiApplication app( argc, argv );

	QCommandLineParser parser;
	parser.addOption( { "calendar-snapshot", "Path to the calendar-sync daemon's snapshot JSON.", "path",
						"/run/kiosk/calendar-snapshot.json" } );
	parser.addOption( { "calendar-socket", "Path to the calendar-sync daemon's command socket.", "path",
						"/run/kiosk/calendar-sync.sock" } );
	parser.process( app );

	CalendarBridge calendarBridge( parser.value( "calendar-snapshot" ), parser.value( "calendar-socket" ) );

	QQmlApplicationEngine engine;
	engine.rootContext()->setContextProperty( "calendarBridge", &calendarBridge );

	// qml/ and assets/ both install under share/ha-kiosk/ (FHS:
	// architecture-independent data doesn't belong next to the binary),
	// always one fixed hop from wherever the binary itself landed — see
	// ha-kiosk/CMakeLists.txt.
	const QString shareDir =
		QDir::cleanPath( QCoreApplication::applicationDirPath() + "/../share/ha-kiosk" );
	const QString assetsPath = shareDir + "/assets";
	engine.rootContext()->setContextProperty( "assetsUrl", QUrl::fromLocalFile( assetsPath + "/" ) );

	const QString qmlPath = QDir( shareDir + "/qml" ).filePath( "main.qml" );
	engine.load( QUrl::fromLocalFile( qmlPath ) );

	if ( engine.rootObjects().isEmpty() )
		return -1;

	return app.exec();
}
