#pragma once

#include <QJsonObject>
#include <QtGlobal>

/// Turns MotionDetector's current presence state into the read-model
/// snapshot PresenceBridge serves to QML.
namespace SnapshotBuilder
{

/// @param motionDetected Current presence state (see MotionDetector::presence()).
/// @param lastMotionMs Epoch-ms of the last detected change, or -1 if none
/// yet this run — encoded as a null "lastMotionAt" in that case, not a bogus
/// epoch-zero string.
QJsonObject build( bool motionDetected, qint64 lastMotionMs );

} // namespace SnapshotBuilder
