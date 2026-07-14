#pragma once

#include <QByteArray>
#include <QtGlobal>

/// Presence detector using a slowly-adapting background reference rather
/// than plain frame-to-frame differencing — comparing each new frame
/// against "what the empty scene recently looked like" means presence stays
/// true continuously while someone remains in frame, not just during the
/// moments they're actually moving (a naive previous-frame diff would miss
/// someone standing still, since that looks identical to the frame right
/// before them). Also distinguishes a room-wide lighting change (a light
/// switch, the sun coming out) from presence via how much of the *frame*
/// changed, not just by how much on average, since a light flip otherwise
/// looks just like "most of the frame changed" to the per-pixel diff. No
/// OpenCV/ML, matching the rest of the project's lightweight dependency
/// footprint.
///
/// Pixel-depth agnostic: works against 8-bit-per-pixel (GRAY8) or
/// 16-bit-per-pixel (e.g. GRAY16_LE) frames, since some camera pipelines may
/// preserve more than 8 bits of real sensor precision — see
/// Config::pixelMaxValue's doc for the caveat on whether that actually
/// carries more information in practice.
class MotionDetector
{
  public:
	/// @param changedFraction Fraction (0-1) of pixels that must differ
	/// enough from the background reference to count as presence.
	/// @param backgroundAdaptRate Per-sample blend weight (0-1) toward the
	/// new frame when adapting the background reference — only applied on
	/// samples where no presence was detected, so a person standing there a
	/// while doesn't slowly get absorbed into "empty". Small values (e.g.
	/// 0.05) adapt over many samples, tracking slow lighting drift without
	/// reacting to any one frame.
	/// @param holdMs Grace period, in milliseconds, presence stays true
	/// after the last detected change — avoids immediately clearing
	/// presence (and re-sleeping the screen) the instant someone steps out
	/// of frame, or on a single noisy sample.
	/// @param pixelMaxValue The maximum raw value a single pixel can hold in
	/// whatever format the camera pipeline delivers — 255 for 8-bit GRAY8,
	/// or whatever the negotiated bit depth actually spans for a
	/// higher-depth format (see Config::pixelMaxValue). All of this class's
	/// internal thresholds are fractions of this, so they scale correctly
	/// regardless of pixel depth.
	MotionDetector( double changedFraction, double backgroundAdaptRate, int holdMs, int pixelMaxValue = 255 );

	/// Feeds one new greyscale frame captured at `nowMs` (any monotonic
	/// millisecond clock, e.g. QDateTime::currentMSecsSinceEpoch()).
	/// @param bytesPerPixel 1 for 8-bit-per-pixel frames, 2 for 16-bit
	/// little-endian frames (see CameraCapture::latestFrame). The first
	/// call (or the first call after a frame-size/depth change) only seeds
	/// the background reference and never reports presence, since there's
	/// nothing yet to compare against.
	void update( const QByteArray &frame, int width, int height, int bytesPerPixel, qint64 nowMs );

	/// Whether presence is currently considered active — true from the
	/// moment a frame first differs enough from the background reference
	/// until holdMs after the last such change, false if update() has never
	/// been called with a comparable frame.
	bool presence() const;

	/// Epoch-ms timestamp of the last detected change, or -1 if none yet.
	qint64 lastMotionMs() const;

  private:
	QByteArray m_background;
	int m_backgroundWidth = 0;
	int m_backgroundHeight = 0;
	int m_backgroundBytesPerPixel = 0;
	double m_changedFraction;
	double m_backgroundAdaptRate;
	int m_holdMs;
	double m_pixelDiffThreshold;
	double m_globalBrightnessDeltaThreshold;
	qint64 m_lastMotionMs = -1;
	qint64 m_lastEvaluatedMs = -1;
};
