#pragma once

#include <QByteArray>
#include <QString>

typedef struct _GstElement GstElement;

/// Owns a GStreamer pipeline that samples greyscale frames off the camera
/// for MotionDetector to compare. Built from a source pipeline description
/// (Config::cameraPipeline) with an appsink stage appended internally, so
/// callers never touch GStreamer elements directly.
class CameraCapture
{
  public:
	/// @param sourcePipeline GStreamer pipeline description up to (not
	/// including) the appsink — see Config::cameraPipeline.
	explicit CameraCapture( const QString &sourcePipeline );
	~CameraCapture();

	CameraCapture( const CameraCapture & ) = delete;
	CameraCapture &operator=( const CameraCapture & ) = delete;

	/// Pulls the most recently captured frame without blocking. Returns
	/// false if no new frame has arrived since the last call — callers
	/// should simply skip that poll cycle rather than treating it as an
	/// error (the camera may not have produced a frame yet, e.g. right
	/// after startup, or the pipeline failed to build at all).
	/// @param bytesPerPixel Derived from the negotiated caps' actual buffer
	/// size (not parsed from the format string) — 1 for an 8-bit-per-pixel
	/// format like GRAY8, 2 for a 16-bit one like GRAY16_LE. Pass through to
	/// MotionDetector::update() unchanged.
	bool latestFrame( QByteArray &data, int &width, int &height, int &bytesPerPixel );

  private:
	GstElement *m_pipeline = nullptr;
	GstElement *m_appsink = nullptr;
};
