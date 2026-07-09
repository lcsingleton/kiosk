#pragma once

#include <QHash>
#include <QLocalServer>
#include <QObject>
#include <functional>

#include "CommandTypes.h"

class CalendarClient;
class QLocalSocket;

// Listens on a Unix domain socket for NDJSON command lines from the kiosk
// app, dispatches each by its `action` string through a lookup table (so
// adding a new command later is one new handler, not a protocol change),
// and writes an NDJSON Result line back on the same connection.
class CommandServer : public QObject {
    Q_OBJECT
public:
    explicit CommandServer(CalendarClient *client, QObject *parent = nullptr);

    // Removes any stale socket file from a previous run, then listens.
    bool listen(const QString &socketPath, QString &error);

signals:
    // Emitted after any command completes successfully, so main.cpp can
    // refresh the snapshot immediately instead of waiting for the next
    // poll tick.
    void writeSucceeded();

private:
    using Handler = std::function<void(const Command &, std::function<void(Result)>)>;

    void onNewConnection();
    void handleLine(QLocalSocket *socket, const QByteArray &line);
    void dispatch(const Command &cmd, std::function<void(Result)> reply);
    void sendResult(QLocalSocket *socket, const Result &result);

    void handleSchedule(const Command &cmd, std::function<void(Result)> reply);
    void handleReschedule(const Command &cmd, std::function<void(Result)> reply);
    void handleCancel(const Command &cmd, std::function<void(Result)> reply);
    void handlePatchField(const Command &cmd, const QString &jsonKey, const QString &payloadKey,
                           std::function<void(Result)> reply);
    void handleInvite(const Command &cmd, std::function<void(Result)> reply);

    CalendarClient *m_client;
    QLocalServer m_server;
    QHash<QLocalSocket *, QByteArray> m_recvBuffers;
    QHash<QString, Handler> m_handlers;
};
