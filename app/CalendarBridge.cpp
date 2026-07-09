#include "CalendarBridge.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QUuid>

CalendarBridge::CalendarBridge(const QString &snapshotPath, const QString &socketPath, QObject *parent)
    : QObject(parent), m_snapshotPath(snapshotPath), m_socketPath(socketPath) {
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &CalendarBridge::reloadSnapshot);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &CalendarBridge::reloadSnapshot);

    // Watch the containing directory too: on a cold boot the snapshot file
    // may not exist yet (the daemon hasn't written its first cycle), and
    // QFileSystemWatcher can only watch paths that already exist.
    const QString dir = QFileInfo(snapshotPath).absolutePath();
    if (QDir(dir).exists())
        m_watcher.addPath(dir);
    reloadSnapshot();

    connect(&m_socket, &QLocalSocket::readyRead, this, &CalendarBridge::onSocketReadyRead);
    connect(&m_socket, &QLocalSocket::connected, this, [this]() { m_reconnectTimer.stop(); });
    connect(&m_socket, &QLocalSocket::disconnected, this, [this]() { m_reconnectTimer.start(); });
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        m_reconnectTimer.start();
    });

    // The daemon may not be up yet (or may restart later) — keep retrying
    // on a fixed interval rather than treating "not connected right now" as
    // a terminal state.
    m_reconnectTimer.setInterval(3000);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &CalendarBridge::connectToDaemon);
    connectToDaemon();
}

void CalendarBridge::connectToDaemon() {
    if (m_socket.state() == QLocalSocket::ConnectedState || m_socket.state() == QLocalSocket::ConnectingState)
        return;
    m_socket.connectToServer(m_socketPath);
}

void CalendarBridge::reloadSnapshot() {
    QFile file(m_snapshotPath);
    if (file.open(QIODevice::ReadOnly)) {
        m_snapshot = QJsonDocument::fromJson(file.readAll()).object();
        emit snapshotChanged();
    }

    // Re-arm the file watch every time: inotify drops a watch across the
    // atomic rename the daemon uses to replace this file, so it must be
    // re-added after every observed change, not just once at startup.
    if (QFileInfo::exists(m_snapshotPath) && !m_watcher.files().contains(m_snapshotPath))
        m_watcher.addPath(m_snapshotPath);
}

QVariantList CalendarBridge::arrayProperty(const char *key) const {
    return m_snapshot.value(QLatin1String(key)).toArray().toVariantList();
}

QVariantList CalendarBridge::people() const { return arrayProperty("people"); }
QVariantList CalendarBridge::todayHighlights() const { return arrayProperty("todayHighlights"); }
QVariantList CalendarBridge::todaySchedule() const { return arrayProperty("todaySchedule"); }
QVariantList CalendarBridge::weekend() const { return arrayProperty("weekend"); }
QVariantList CalendarBridge::upcoming() const { return arrayProperty("upcoming"); }

QString CalendarBridge::sendCommand(const QString &action, const QString &calendarId, const QString &eventId,
                                     const QString &etag, const QJsonObject &payload, const QString &what) {
    const QString commandId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    if (m_socket.state() != QLocalSocket::ConnectedState) {
        // Deferred so a caller that connects to commandFailed immediately
        // after calling this (rather than before) still receives it.
        QTimer::singleShot(0, this, [this, commandId, what]() {
            emit commandFailed(commandId, what, QStringLiteral("upstream_unavailable"),
                                QStringLiteral("not connected to the calendar sync daemon"));
        });
        return commandId;
    }

    QJsonObject obj;
    obj["commandId"] = commandId;
    obj["action"] = action;
    obj["calendarId"] = calendarId;
    obj["eventId"] = eventId;
    obj["etag"] = etag;
    obj["payload"] = payload;

    m_pending.insert(commandId, PendingCommand{what});
    m_socket.write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
    m_socket.flush();
    return commandId;
}

QString CalendarBridge::scheduleEvent(const QString &calendarId, const QString &summary,
                                       const QString &startIso, const QString &endIso,
                                       const QString &description) {
    QJsonObject payload{{"summary", summary}, {"start", startIso}, {"end", endIso}};
    if (!description.isEmpty())
        payload["description"] = description;
    return sendCommand(QStringLiteral("ScheduleEvent"), calendarId, QString(), QString(), payload,
                        QStringLiteral("Adding \"%1\"").arg(summary));
}

QString CalendarBridge::rescheduleEvent(const QString &calendarId, const QString &eventId, const QString &etag,
                                         const QString &newStartIso, const QString &newEndIso) {
    QJsonObject payload{{"newStart", newStartIso}, {"newEnd", newEndIso}};
    return sendCommand(QStringLiteral("RescheduleEvent"), calendarId, eventId, etag, payload,
                        QStringLiteral("Rescheduling event"));
}

QString CalendarBridge::cancelEvent(const QString &calendarId, const QString &eventId, const QString &etag) {
    return sendCommand(QStringLiteral("CancelEvent"), calendarId, eventId, etag, QJsonObject(),
                        QStringLiteral("Cancelling event"));
}

QString CalendarBridge::renameEvent(const QString &calendarId, const QString &eventId, const QString &etag,
                                     const QString &newSummary) {
    QJsonObject payload{{"newSummary", newSummary}};
    return sendCommand(QStringLiteral("RenameEvent"), calendarId, eventId, etag, payload,
                        QStringLiteral("Renaming to \"%1\"").arg(newSummary));
}

QString CalendarBridge::changeEventLocation(const QString &calendarId, const QString &eventId, const QString &etag,
                                             const QString &newLocation) {
    QJsonObject payload{{"newLocation", newLocation}};
    return sendCommand(QStringLiteral("ChangeEventLocation"), calendarId, eventId, etag, payload,
                        QStringLiteral("Changing location"));
}

void CalendarBridge::onSocketReadyRead() {
    m_recvBuffer += m_socket.readAll();
    int idx;
    while ((idx = m_recvBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_recvBuffer.left(idx);
        m_recvBuffer.remove(0, idx + 1);
        if (!line.trimmed().isEmpty())
            handleResultLine(line);
    }
}

void CalendarBridge::handleResultLine(const QByteArray &line) {
    const QJsonObject obj = QJsonDocument::fromJson(line).object();
    const QString commandId = obj.value("commandId").toString();

    const auto it = m_pending.find(commandId);
    if (it == m_pending.end())
        return; // unknown or already-handled commandId
    const QString what = it->what;
    m_pending.erase(it);

    if (obj.value("status").toString() == QLatin1String("ok")) {
        emit commandSucceeded(commandId, what);
    } else {
        const QJsonObject err = obj.value("error").toObject();
        emit commandFailed(commandId, what, err.value("code").toString(), err.value("message").toString());
    }
}
