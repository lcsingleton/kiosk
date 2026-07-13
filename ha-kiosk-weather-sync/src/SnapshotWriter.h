#pragma once

#include <QJsonObject>
#include <QString>

/// Persists a built snapshot to disk for WeatherBridge to read.
namespace SnapshotWriter
{

/// Atomically replaces the file at `path` with `snapshot`'s JSON (write to a
/// temp file in the same directory, then rename) so WeatherBridge on the app
/// side never observes a half-written file. Returns false and fills `error`
/// on failure.
bool write( const QString &path, const QJsonObject &snapshot, QString &error );

} // namespace SnapshotWriter
