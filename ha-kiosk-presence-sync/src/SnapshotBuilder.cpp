#include "SnapshotBuilder.h"

#include <QDateTime>
#include <QJsonValue>

QJsonObject SnapshotBuilder::build( bool motionDetected, qint64 lastMotionMs )
{
	QJsonObject snapshot;
	snapshot["motionDetected"] = motionDetected;
	snapshot["lastMotionAt"] = lastMotionMs < 0
									? QJsonValue( QJsonValue::Null )
									: QJsonValue( QDateTime::fromMSecsSinceEpoch( lastMotionMs, Qt::UTC ).toString( Qt::ISODate ) );
	snapshot["updatedAt"] = QDateTime::currentDateTimeUtc().toString( Qt::ISODate );
	return snapshot;
}
