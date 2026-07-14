#include "IdleController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QTimer>

IdleController::IdleController( qint64 idleTimeoutMs, QObject *parent )
	: QObject( parent ), m_idleTimeoutMs( idleTimeoutMs ), m_lastActivityMs( QDateTime::currentMSecsSinceEpoch() )
{
	qApp->installEventFilter( this );

	auto *checkTimer = new QTimer( this );
	checkTimer->setInterval( 1000 );
	connect( checkTimer, &QTimer::timeout, this, &IdleController::checkIdle );
	checkTimer->start();
}

bool IdleController::screenAsleep() const
{
	return m_screenAsleep;
}

bool IdleController::eventFilter( QObject *watched, QEvent *event )
{
	switch ( event->type() )
	{
		case QEvent::MouseButtonPress:
		case QEvent::MouseMove:
		case QEvent::TouchBegin:
		case QEvent::TouchUpdate:
			reportActivity();
			break;
		default:
			break;
	}
	// Never consumed — this is a pure observer so existing per-card tap
	// handling in QML keeps working exactly as before.
	return QObject::eventFilter( watched, event );
}

void IdleController::reportActivity()
{
	m_lastActivityMs = QDateTime::currentMSecsSinceEpoch();
	if ( m_screenAsleep )
	{
		m_screenAsleep = false;
		emit sleepStateChanged();
	}
}

void IdleController::checkIdle()
{
	if ( m_screenAsleep )
		return;

	const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_lastActivityMs;
	if ( elapsed >= m_idleTimeoutMs )
	{
		m_screenAsleep = true;
		emit sleepStateChanged();
	}
}
