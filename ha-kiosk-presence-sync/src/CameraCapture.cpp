#include "CameraCapture.h"

#include <QDebug>

#include <gst/app/gstappsink.h>
#include <gst/gst.h>

namespace
{
// MotionDetector only ever wants the *latest* frame, never a backlog, so the
// sink is told to drop anything older rather than queue it.
constexpr int kAppsinkMaxBuffers = 1;
constexpr const char *kAppsinkName = "presence-sink";
} // namespace

CameraCapture::CameraCapture( const QString &sourcePipeline )
{
	const QString fullDescription = sourcePipeline + QStringLiteral( " ! appsink name=%1 max-buffers=%2 drop=true sync=false" )
														  .arg( kAppsinkName )
														  .arg( kAppsinkMaxBuffers );

	GError *gerror = nullptr;
	m_pipeline = gst_parse_launch( fullDescription.toUtf8().constData(), &gerror );
	if ( !m_pipeline || gerror )
	{
		qCritical().noquote() << "failed to build camera pipeline:" << ( gerror ? gerror->message : "unknown error" );
		if ( gerror )
			g_error_free( gerror );
		if ( m_pipeline )
		{
			gst_object_unref( m_pipeline );
			m_pipeline = nullptr;
		}
		return;
	}

	m_appsink = gst_bin_get_by_name( GST_BIN( m_pipeline ), kAppsinkName );
	gst_element_set_state( m_pipeline, GST_STATE_PLAYING );
}

CameraCapture::~CameraCapture()
{
	if ( m_appsink )
		gst_object_unref( m_appsink );
	if ( m_pipeline )
	{
		gst_element_set_state( m_pipeline, GST_STATE_NULL );
		gst_object_unref( m_pipeline );
	}
}

bool CameraCapture::latestFrame( QByteArray &data, int &width, int &height, int &bytesPerPixel )
{
	if ( !m_appsink )
		return false;

	// Zero timeout: non-blocking, matching the header's "skip this cycle"
	// contract rather than stalling the daemon's poll loop on a slow camera.
	GstSample *sample = gst_app_sink_try_pull_sample( GST_APP_SINK( m_appsink ), 0 );
	if ( !sample )
		return false;

	GstCaps *caps = gst_sample_get_caps( sample );
	GstStructure *structure = caps ? gst_caps_get_structure( caps, 0 ) : nullptr;
	if ( !structure || !gst_structure_get_int( structure, "width", &width ) || !gst_structure_get_int( structure, "height", &height ) )
	{
		gst_sample_unref( sample );
		return false;
	}

	GstBuffer *buffer = gst_sample_get_buffer( sample );
	GstMapInfo map;
	if ( !buffer || !gst_buffer_map( buffer, &map, GST_MAP_READ ) )
	{
		gst_sample_unref( sample );
		return false;
	}

	data = QByteArray( reinterpret_cast<const char *>( map.data ), static_cast<int>( map.size ) );
	// Derived from the actual mapped buffer size rather than parsed out of
	// the caps' "format" string, so this works for whichever of GRAY8/
	// GRAY16_LE/etc. the pipeline negotiated without needing to enumerate
	// every format name GStreamer might report. Anything that doesn't
	// divide evenly (an unexpected/padded layout) falls back to 1 rather
	// than risk MotionDetector misreading the buffer as 16-bit garbage.
	const qint64 pixelCount = qint64( width ) * qint64( height );
	bytesPerPixel =
		( pixelCount > 0 && map.size % pixelCount == 0 ) ? static_cast<int>( map.size / pixelCount ) : 1;

	gst_buffer_unmap( buffer, &map );
	gst_sample_unref( sample );
	return true;
}
