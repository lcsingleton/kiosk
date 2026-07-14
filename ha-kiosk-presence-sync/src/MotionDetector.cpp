#include "MotionDetector.h"

#include <QtGlobal>

namespace
{
// Per-pixel delta, as a fraction of pixelMaxValue, that counts as "changed"
// — chosen to ignore ordinary sensor/compression noise rather than genuine
// presence; not exposed via Config since it's a property of the algorithm,
// not a deployment concern. ~25/255 at 8-bit depth.
constexpr double kPixelDiffThresholdFraction = 25.0 / 255.0;

// Mean-brightness shift, as a fraction of pixelMaxValue, required together
// with kGlobalLightingChangeFraction below to treat a sample as a room-wide
// lighting change (a light switch, the sun coming out) rather than
// presence. Deliberately well above ordinary auto-exposure/sensor-noise
// drift between samples. ~30/255 at 8-bit depth.
constexpr double kGlobalBrightnessDeltaThresholdFraction = 30.0 / 255.0;

// Fraction of pixels that must individually register as "changed" (per
// kPixelDiffThresholdFraction) for a sample to count as a lighting change
// rather than presence. This — not the mean shift alone — is what actually
// distinguishes the two: a light flip shifts nearly every pixel in the same
// direction, while presence only ever changes some local region of the
// frame. Without this, a person occupying a smaller fraction of the frame
// at high contrast can shift the *mean* by just as much as a lighting
// change while only touching a fraction of the pixels — this fraction check
// is what tells them apart. Already depth-independent (a fraction of pixel
// count, not of value range).
constexpr double kGlobalLightingChangeFraction = 0.8;

// Reads one pixel's raw value regardless of depth — `base` must point into
// a buffer produced with the same bytesPerPixel this was computed for.
int pixelAt( const char *base, qsizetype index, int bytesPerPixel )
{
	if ( bytesPerPixel == 2 )
		return reinterpret_cast<const quint16 *>( base )[index];
	return static_cast<uchar>( base[index] );
}

void setPixelAt( char *base, qsizetype index, int bytesPerPixel, int value )
{
	if ( bytesPerPixel == 2 )
		reinterpret_cast<quint16 *>( base )[index] = static_cast<quint16>( value );
	else
		base[index] = static_cast<char>( value );
}
} // namespace

MotionDetector::MotionDetector( double changedFraction, double backgroundAdaptRate, int holdMs, int pixelMaxValue )
	: m_changedFraction( changedFraction )
	, m_backgroundAdaptRate( backgroundAdaptRate )
	, m_holdMs( holdMs )
	, m_pixelDiffThreshold( kPixelDiffThresholdFraction * pixelMaxValue )
	, m_globalBrightnessDeltaThreshold( kGlobalBrightnessDeltaThresholdFraction * pixelMaxValue )
{
}

void MotionDetector::update( const QByteArray &frame, int width, int height, int bytesPerPixel, qint64 nowMs )
{
	m_lastEvaluatedMs = nowMs;

	if ( frame.isEmpty() || bytesPerPixel <= 0 )
		return;

	const qsizetype pixelCount = frame.size() / bytesPerPixel;

	const bool sameSize = ( width == m_backgroundWidth && height == m_backgroundHeight &&
							 bytesPerPixel == m_backgroundBytesPerPixel && frame.size() == m_background.size() );
	if ( !sameSize || m_background.isEmpty() )
	{
		// No comparable reference yet (first frame, or the camera/pipeline
		// changed resolution or pixel format) — seed the background
		// outright rather than reporting presence against a reference that
		// isn't real yet.
		m_background = frame;
		m_backgroundWidth = width;
		m_backgroundHeight = height;
		m_backgroundBytesPerPixel = bytesPerPixel;
		return;
	}

	const char *bgData = m_background.constData();
	const char *curData = frame.constData();

	qint64 changed = 0;
	qint64 curSum = 0;
	qint64 bgSum = 0;
	for ( qsizetype i = 0; i < pixelCount; ++i )
	{
		const int curVal = pixelAt( curData, i, bytesPerPixel );
		const int bgVal = pixelAt( bgData, i, bytesPerPixel );
		if ( qAbs( curVal - bgVal ) > m_pixelDiffThreshold )
			++changed;
		curSum += curVal;
		bgSum += bgVal;
	}

	const double fraction = double( changed ) / double( pixelCount );
	const double meanDelta = double( curSum - bgSum ) / double( pixelCount );

	if ( fraction >= kGlobalLightingChangeFraction && qAbs( meanDelta ) >= m_globalBrightnessDeltaThreshold )
	{
		// Room-wide lighting shift, not presence (see the constants' docs
		// above) — the old background is simply stale under the new
		// lighting, not "wrong until it slowly adapts", so re-seed it
		// outright instead of running it through the normal slow blend
		// below or reporting this sample as presence.
		m_background = frame;
		return;
	}

	if ( fraction >= m_changedFraction )
	{
		m_lastMotionMs = nowMs;
		return;
	}

	// Only adapt the background while nothing's currently in frame —
	// otherwise someone standing there a while would slowly blend into
	// "empty" and presence would silently drop out from under them.
	// bgData/curData above are no longer read after this point, so
	// detaching/mutating m_background in place here is safe.
	char *bgMutData = m_background.data();
	for ( qsizetype i = 0; i < pixelCount; ++i )
	{
		const int curVal = pixelAt( curData, i, bytesPerPixel );
		const int bgVal = pixelAt( bgMutData, i, bytesPerPixel );
		setPixelAt( bgMutData, i, bytesPerPixel, bgVal + int( m_backgroundAdaptRate * ( curVal - bgVal ) ) );
	}
}

bool MotionDetector::presence() const
{
	if ( m_lastMotionMs < 0 || m_lastEvaluatedMs < 0 )
		return false;
	return ( m_lastEvaluatedMs - m_lastMotionMs ) <= m_holdMs;
}

qint64 MotionDetector::lastMotionMs() const
{
	return m_lastMotionMs;
}
