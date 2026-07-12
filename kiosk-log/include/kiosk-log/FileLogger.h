#pragma once

#include <QString>

// Installs a process-wide Qt message handler that formats every
// qDebug/qInfo/qWarning/qCritical/qFatal message with a timestamp and level,
// writes it to stderr (still captured by systemd's journal, same as before
// this existed), and — best-effort — also appends it to
// /var/log/<moduleName>/<moduleName>.log.
//
// The file side is optional, not required: if that directory can't be
// created or opened (no LogsDirectory= provisioning, no permission, running
// as a non-root user for manual `--once` testing, etc.), this logs one
// warning and carries on stderr-only. Call once, as early as possible in
// main(), before anything else might log.
namespace FileLogger
{

void install( const QString &moduleName );

} // namespace FileLogger
