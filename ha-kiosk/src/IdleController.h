#pragma once

#include <QObject>

/// Tracks whether the kiosk should be considered "asleep" (dimmed) due to a
/// lack of activity — either real touch input or camera-detected presence
/// (see PresenceBridge). Installs itself as a QGuiApplication-wide event
/// filter rather than requiring a QML MouseArea over every card, so it never
/// interferes with existing per-card tap handling in main.qml.
class IdleController : public QObject
{
	Q_OBJECT
	/// Whether the screen is currently considered idle/dimmed. main.qml
	/// binds a full-screen dim overlay's opacity to this.
	Q_PROPERTY( bool screenAsleep READ screenAsleep NOTIFY sleepStateChanged )

  public:
	/// @param idleTimeoutMs How long, in milliseconds, with no activity
	/// before screenAsleep becomes true.
	/// @param parent Standard QObject ownership parent.
	explicit IdleController( qint64 idleTimeoutMs, QObject *parent = nullptr );

	/// @see screenAsleep
	bool screenAsleep() const;

	bool eventFilter( QObject *watched, QEvent *event ) override;

  public slots:
	/// Resets the idle clock and wakes the screen if it was asleep. Called
	/// both from real input (via eventFilter) and from PresenceBridge's
	/// presenceChanged signal when motion is currently detected.
	void reportActivity();

  signals:
	/// @see screenAsleep
	void sleepStateChanged();

  private slots:
	void checkIdle();

  private:
	qint64 m_idleTimeoutMs;
	qint64 m_lastActivityMs;
	bool m_screenAsleep = false;
};
