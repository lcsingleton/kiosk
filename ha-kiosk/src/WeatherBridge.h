#pragma once

#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

// Reads the weather-sync daemon's JSON snapshot file. This is the query
// side only — unlike CalendarBridge, there is no write side to mirror:
// BOM is a read-only upstream, so this class has no command-socket client,
// no Q_INVOKABLE mutators, and no commandSucceeded/commandFailed signals.
// DashboardData.qml binds its weather-related properties directly onto
// this — see the "swap the source, keep the fields" comment there — and
// they update whenever the snapshot file changes.
class WeatherBridge : public QObject
{
	Q_OBJECT
	Q_PROPERTY( QVariantList hourlyForecast READ hourlyForecast NOTIFY snapshotChanged )
	Q_PROPERTY( QVariantList forecast READ forecast NOTIFY snapshotChanged )
	Q_PROPERTY( QVariantMap observations READ observations NOTIFY snapshotChanged )

  public:
	explicit WeatherBridge( const QString &snapshotPath, QObject *parent = nullptr );

	QVariantList hourlyForecast() const;
	QVariantList forecast() const;
	QVariantMap observations() const;

  signals:
	void snapshotChanged();

  private slots:
	void reloadSnapshot();

  private:
	QString m_snapshotPath;
	QFileSystemWatcher m_watcher;
	QJsonObject m_snapshot;
};
