#include "Config.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
// MIPI-CSI via libcamera's GStreamer element — see Config.h's cameraPipeline
// doc for the USB/V4L2 swap. 1fps source: the app only ever pulls one frame
// per pollIntervalMs (2s by default) anyway, so capturing faster than that
// just burns ISP/CPU on frames that get dropped (see CameraCapture's
// max-buffers=1 drop=true appsink).
const QString kDefaultCameraPipeline =
	QStringLiteral( "libcamerasrc ! videoconvert ! video/x-raw,format=GRAY8,width=320,height=240,framerate=1/1" );
} // namespace

bool Config::load( const QString &path, Config &out, QString &error )
{
	QFile file( path );
	if ( !file.open( QIODevice::ReadOnly ) )
	{
		error = QStringLiteral( "cannot open config file %1: %2" ).arg( path, file.errorString() );
		return false;
	}

	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson( file.readAll(), &parseError );
	if ( parseError.error != QJsonParseError::NoError )
	{
		error = QStringLiteral( "invalid JSON in %1: %2" ).arg( path, parseError.errorString() );
		return false;
	}
	const QJsonObject root = doc.object();

	out.cameraPipeline = root.value( "cameraPipeline" ).toString( kDefaultCameraPipeline );
	out.pixelMaxValue = root.value( "pixelMaxValue" ).toInt( 255 );
	out.pollIntervalMs = root.value( "pollIntervalMs" ).toInt( 2000 );
	out.motionThreshold = root.value( "motionThreshold" ).toDouble( 0.02 );
	out.backgroundAdaptRate = root.value( "backgroundAdaptRate" ).toDouble( 0.05 );
	out.presenceHoldSeconds = root.value( "presenceHoldSeconds" ).toInt( 10 );
	out.snapshotPath = root.value( "snapshotPath" ).toString();

	if ( out.snapshotPath.isEmpty() )
	{
		error = QStringLiteral( "config %1: missing \"snapshotPath\"" ).arg( path );
		return false;
	}

	return true;
}
