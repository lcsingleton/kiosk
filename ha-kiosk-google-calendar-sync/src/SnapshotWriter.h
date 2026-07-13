#pragma once

#include <QJsonObject>
#include <QString>

namespace SnapshotWriter
{

/// Atomically replaces the file at `path` with `snapshot`'s JSON (write to a
/// temp file in the same directory, then rename) so CalendarBridge on the
/// app side never observes a half-written file. Returns false and fills
/// `error` on failure.
bool write( const QString &path, const QJsonObject &snapshot, QString &error );

} // namespace SnapshotWriter
