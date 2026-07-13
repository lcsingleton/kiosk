#pragma once

#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

/// Reads the weather-sync daemon's JSON snapshot file. This is the query
/// side only — unlike CalendarBridge, there is no write side to mirror:
/// BOM is a read-only upstream, so this class has no command-socket client,
/// no Q_INVOKABLE mutators, and no commandSucceeded/commandFailed signals.
/// DashboardData.qml binds its weather-related properties directly onto
/// this — see the "swap the source, keep the fields" comment there — and
/// they update whenever the snapshot file changes.
class WeatherBridge : public QObject
{
	Q_OBJECT
	/// Hour-by-hour forecast from the snapshot's "hourlyForecast" field.
	Q_PROPERTY( QVariantList hourlyForecast READ hourlyForecast NOTIFY snapshotChanged )
	/// Day-by-day forecast from the snapshot's "forecast" field.
	Q_PROPERTY( QVariantList forecast READ forecast NOTIFY snapshotChanged )
	/// Latest observed conditions from the snapshot's "observations" field.
	Q_PROPERTY( QVariantMap observations READ observations NOTIFY snapshotChanged )
	/// Sunrise/sunset info from the snapshot's "sun" field.
	Q_PROPERTY( QVariantMap sun READ sun NOTIFY snapshotChanged )
	/// Recent observed history from the snapshot's "weatherHistory" field.
	Q_PROPERTY( QVariantList weatherHistory READ weatherHistory NOTIFY snapshotChanged )

  public:
	/// @param snapshotPath Path to the weather-sync daemon's JSON snapshot file, watched and reloaded on
	/// change.
	/// @param parent Standard QObject ownership parent.
	explicit WeatherBridge( const QString &snapshotPath, QObject *parent = nullptr );

	/// @see hourlyForecast
	QVariantList hourlyForecast() const;
	/// @see forecast
	QVariantList forecast() const;
	/// @see observations
	QVariantMap observations() const;
	/// @see sun
	QVariantMap sun() const;
	/// @see weatherHistory
	QVariantList weatherHistory() const;

  signals:
	/// Fires whenever the snapshot file is reloaded (initial load or a change on disk); backs every
	/// Q_PROPERTY above.
	void snapshotChanged();

  private slots:
	void reloadSnapshot();

  private:
	QString m_snapshotPath;
	QFileSystemWatcher m_watcher;
	QJsonObject m_snapshot;
};
