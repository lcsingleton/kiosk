#include "CommandServer.h"
#include "CalendarClient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QPointer>

CommandServer::CommandServer(CalendarClient *client, QObject *parent)
    : QObject(parent), m_client(client) {
    connect(&m_server, &QLocalServer::newConnection, this, &CommandServer::onNewConnection);

    m_handlers[CommandAction::ScheduleEvent] =
        [this](const Command &c, std::function<void(Result)> reply) { handleSchedule(c, reply); };
    m_handlers[CommandAction::RescheduleEvent] =
        [this](const Command &c, std::function<void(Result)> reply) { handleReschedule(c, reply); };
    m_handlers[CommandAction::CancelEvent] =
        [this](const Command &c, std::function<void(Result)> reply) { handleCancel(c, reply); };
    m_handlers[CommandAction::RenameEvent] =
        [this](const Command &c, std::function<void(Result)> reply) { handlePatchField(c, "summary", "newSummary", reply); };
    m_handlers[CommandAction::ChangeEventLocation] =
        [this](const Command &c, std::function<void(Result)> reply) { handlePatchField(c, "location", "newLocation", reply); };
}

bool CommandServer::listen(const QString &socketPath, QString &error) {
    // Cleans up a stale socket file left behind by a previous crashed run —
    // QLocalServer::listen() fails if the path already exists.
    QLocalServer::removeServer(socketPath);
    if (!m_server.listen(socketPath)) {
        error = m_server.errorString();
        return false;
    }
    return true;
}

void CommandServer::onNewConnection() {
    while (QLocalSocket *socket = m_server.nextPendingConnection()) {
        m_recvBuffers.insert(socket, QByteArray());

        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            QByteArray &buf = m_recvBuffers[socket];
            buf += socket->readAll();
            int idx;
            while ((idx = buf.indexOf('\n')) >= 0) {
                const QByteArray line = buf.left(idx);
                buf.remove(0, idx + 1);
                if (!line.trimmed().isEmpty())
                    handleLine(socket, line);
            }
        });

        connect(socket, &QLocalSocket::disconnected, this, [this, socket]() {
            m_recvBuffers.remove(socket);
            socket->deleteLater();
        });
    }
}

void CommandServer::handleLine(QLocalSocket *socket, const QByteArray &line) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning().noquote() << "malformed command line, ignoring:" << parseError.errorString();
        return;
    }

    const Command cmd = Command::fromJson(doc.object());
    if (cmd.commandId.isEmpty()) {
        qWarning() << "command missing commandId, ignoring";
        return;
    }

    // A QPointer, not the raw socket*: the actual Calendar API call this
    // dispatches to is a real network round trip, during which the kiosk
    // app's connection may have already dropped and been deleted — writing
    // to a dangling QLocalSocket* at that point would be a use-after-free.
    QPointer<QLocalSocket> guardedSocket(socket);
    dispatch(cmd, [this, guardedSocket, commandId = cmd.commandId](Result result) {
        if (guardedSocket) {
            sendResult(guardedSocket, result);
        } else {
            qWarning().noquote() << "client disconnected before result for command" << commandId << "was ready";
        }
    });
}

void CommandServer::dispatch(const Command &cmd, std::function<void(Result)> reply) {
    const auto it = m_handlers.constFind(cmd.action);
    if (it == m_handlers.constEnd()) {
        reply(Result::failure(cmd.commandId, QStringLiteral("invalid_request"),
                               QStringLiteral("Unknown action: %1").arg(cmd.action)));
        return;
    }
    (*it)(cmd, reply);
}

void CommandServer::sendResult(QLocalSocket *socket, const Result &result) {
    socket->write(QJsonDocument(result.toJson()).toJson(QJsonDocument::Compact) + "\n");
    socket->flush();
    if (result.ok())
        emit writeSucceeded();
}

void CommandServer::handleSchedule(const Command &cmd, std::function<void(Result)> reply) {
    const QString summary = cmd.payload.value("summary").toString();
    const QString start = cmd.payload.value("start").toString();
    const QString end = cmd.payload.value("end").toString();
    if (summary.isEmpty() || start.isEmpty() || end.isEmpty()) {
        reply(Result::failure(cmd.commandId, QStringLiteral("invalid_request"),
                               QStringLiteral("ScheduleEvent requires payload.summary, .start, .end")));
        return;
    }

    QJsonObject eventBody;
    eventBody["summary"] = summary;
    eventBody["start"] = QJsonObject{{"dateTime", start}};
    eventBody["end"] = QJsonObject{{"dateTime", end}};
    const QString description = cmd.payload.value("description").toString();
    if (!description.isEmpty())
        eventBody["description"] = description;

    m_client->insertEvent(cmd.calendarId, eventBody, [cmd, reply](QJsonObject, QString code, QString message) {
        reply(code.isEmpty() ? Result::success(cmd.commandId) : Result::failure(cmd.commandId, code, message));
    });
}

void CommandServer::handleReschedule(const Command &cmd, std::function<void(Result)> reply) {
    const QString newStart = cmd.payload.value("newStart").toString();
    const QString newEnd = cmd.payload.value("newEnd").toString();
    if (newStart.isEmpty() || newEnd.isEmpty()) {
        reply(Result::failure(cmd.commandId, QStringLiteral("invalid_request"),
                               QStringLiteral("RescheduleEvent requires payload.newStart, .newEnd")));
        return;
    }

    QJsonObject patchBody;
    patchBody["start"] = QJsonObject{{"dateTime", newStart}};
    patchBody["end"] = QJsonObject{{"dateTime", newEnd}};

    m_client->patchEvent(cmd.calendarId, cmd.eventId, cmd.etag, patchBody,
        [cmd, reply](QJsonObject, QString code, QString message) {
            reply(code.isEmpty() ? Result::success(cmd.commandId) : Result::failure(cmd.commandId, code, message));
        });
}

void CommandServer::handleCancel(const Command &cmd, std::function<void(Result)> reply) {
    m_client->deleteEvent(cmd.calendarId, cmd.eventId, cmd.etag, [cmd, reply](QString code, QString message) {
        reply(code.isEmpty() ? Result::success(cmd.commandId) : Result::failure(cmd.commandId, code, message));
    });
}

void CommandServer::handlePatchField(const Command &cmd, const QString &jsonKey, const QString &payloadKey,
                                      std::function<void(Result)> reply) {
    const QString value = cmd.payload.value(payloadKey).toString();
    if (value.isEmpty()) {
        reply(Result::failure(cmd.commandId, QStringLiteral("invalid_request"),
                               QStringLiteral("%1 requires payload.%2").arg(cmd.action, payloadKey)));
        return;
    }

    QJsonObject patchBody;
    patchBody[jsonKey] = value;

    m_client->patchEvent(cmd.calendarId, cmd.eventId, cmd.etag, patchBody,
        [cmd, reply](QJsonObject, QString code, QString message) {
            reply(code.isEmpty() ? Result::success(cmd.commandId) : Result::failure(cmd.commandId, code, message));
        });
}
