#pragma once

#include <QString>

/// On-disk JSON configuration for the presence-sync daemon; see load().
struct Config
{
	/// GStreamer source pipeline description up to (not including) the
	/// appsink stage — CameraCapture appends its own `appsink name=...
	/// max-buffers=1 drop=true sync=false` stage internally. Defaults to a
	/// MIPI-CSI/libcamera pipeline; for a USB webcam, override this to
	/// something like "v4l2src device=/dev/video0 ! videoconvert !
	/// video/x-raw,format=GRAY8,width=320,height=240,framerate=1/1" —
	/// nothing else in this daemon changes either way.
	///
	/// A third option, if the board's rkisp1 pipeline handler exposes
	/// libcamera's Raw stream role for this sensor (unconfirmed — see
	/// deploy/README.md's camera bring-up section): request the sensor's
	/// raw Bayer mosaic directly, with no `videoconvert` stage at all, e.g.
	/// "libcamerasrc ! video/x-bayer,format=rggb,width=320,height=240,framerate=1/1".
	/// MotionDetector never needs the data demosaiced into real RGB/luma —
	/// it only ever compares a frame against its own background reference
	/// of the same raw layout, position for position, so the untouched
	/// mosaic works as-is (and this SnapshotWriter/MotionDetector path is
	/// already format-agnostic — see CameraCapture::latestFrame). Only an
	/// *unpacked* raw format works this way (each pixel in its own whole
	/// byte or 16-bit slot); a *packed* one (e.g. several 10-bit samples
	/// crammed across byte boundaries) would need real bit-unpacking code
	/// this daemon doesn't have.
	QString cameraPipeline;
	/// Maximum raw value a single pixel can hold in whatever format
	/// cameraPipeline delivers. Defaults to 255 (8-bit GRAY8, the default
	/// pipeline above). Only meaningful to change alongside cameraPipeline:
	/// switching to a 16-bit format (e.g. "...,format=GRAY16_LE") only
	/// improves low-light precision if the sensor/ISP actually preserves
	/// more than 8 bits of real precision end-to-end and libcamera exposes
	/// it as such on its normal processed video path — many embedded ISPs
	/// only ever output 8-bit YUV/greyscale there, with true higher-depth
	/// data only available pre-ISP as raw Bayer (a much bigger lift to
	/// consume: demosaicing it yourself). Confirm on hardware — e.g.
	/// `gst-launch-1.0 libcamerasrc ! video/x-raw,format=GRAY16_LE ! fakesink -v`
	/// to see whether that negotiates at all, and whether a captured frame's
	/// actual value range genuinely spans more than 256 levels — before
	/// setting this above 255. If the sensor/ISP only preserves native
	/// 10-/12-bit precision without rescaling to fill 16 bits, set this to
	/// that native max (1023/4095) instead of 65535.
	int pixelMaxValue = 255;
	/// Milliseconds between frame samples; defaults to 2000 (1 sample every
	/// 2 seconds) if omitted — presence doesn't need sub-second sampling to
	/// wake a screen, and a slower cadence keeps CPU/ISP load down.
	int pollIntervalMs = 2000;
	/// Fraction (0-1) of pixels that must differ from the background
	/// reference (see MotionDetector) to count as presence; defaults to
	/// 0.02 (2%) if omitted.
	double motionThreshold = 0.02;
	/// Per-sample blend weight (0-1) toward each new frame when adapting
	/// the background reference on samples with no presence detected — see
	/// MotionDetector's constructor doc. Defaults to 0.05 if omitted.
	double backgroundAdaptRate = 0.05;
	/// How long, in seconds, presence stays true after the last detected
	/// change — a grace period so the screen doesn't immediately re-sleep
	/// the instant someone steps out of frame; defaults to 10.
	int presenceHoldSeconds = 10;
	/// Filesystem path SnapshotWriter atomically writes the rendered
	/// snapshot JSON to.
	QString snapshotPath;

	/// Reads and validates a config JSON file at `path`. On failure, returns
	/// false and fills `error` with a message suitable for logging as-is.
	static bool load( const QString &path, Config &out, QString &error );
};
