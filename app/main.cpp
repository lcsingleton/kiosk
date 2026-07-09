#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "CalendarBridge.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addOption({"calendar-snapshot", "Path to the calendar-sync daemon's snapshot JSON.", "path",
                       "/run/kiosk/calendar-snapshot.json"});
    parser.addOption({"calendar-socket", "Path to the calendar-sync daemon's command socket.", "path",
                       "/run/kiosk/calendar-sync.sock"});
    parser.process(app);

    CalendarBridge calendarBridge(parser.value("calendar-snapshot"), parser.value("calendar-socket"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("calendarBridge", &calendarBridge);

    const QString qmlPath = QDir(QCoreApplication::applicationDirPath()).filePath("main.qml");
    engine.load(QUrl::fromLocalFile(qmlPath));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
