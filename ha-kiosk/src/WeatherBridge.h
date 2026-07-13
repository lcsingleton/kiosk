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
	/// @return Today's remaining hours, one object per hour:
	/// @code{.json}
	/// { "hour": "3pm", "icon": "⛅", "temp": 22 }
	/// @endcode
	/// `hour` is a label ("3pm") on every 3rd on-the-hour point (see
	/// LineChart.qml) and an empty string on the rest; `icon` is an emoji
	/// from IconMap; `temp` is a rounded whole Celsius degree.
	Q_PROPERTY( QVariantList hourlyForecast READ hourlyForecast NOTIFY snapshotChanged )
	/// Day-by-day forecast from the snapshot's "forecast" field.
	/// @return Up to 7 days, one object per day:
	/// @code{.json}
	/// { "day": "Today", "icon": "☀️", "hi": "24°", "lo": "12°" }
	/// @endcode
	/// `day` is "Today" for the first entry, otherwise a 3-letter weekday
	/// ("Mon"); `hi`/`lo` are pre-formatted degree strings, not bare numbers.
	Q_PROPERTY( QVariantList forecast READ forecast NOTIFY snapshotChanged )
	/// Latest observed conditions from the snapshot's "observations" field.
	/// @return A single object of pre-formatted, already-unit-converted strings:
	/// @code{.json}
	/// { "humidity": "62%", "windSpeed": "12 km/h", "rainToday": "0.0 mm" }
	/// @endcode
	Q_PROPERTY( QVariantMap observations READ observations NOTIFY snapshotChanged )
	/// Sunrise/sunset info from the snapshot's "sun" field.
	/// @return Today's sunrise/sunset, each an ISO-8601 UTC instant straight
	/// from BOM, not localized, since QML compares these directly against "now":
	/// @code{.json}
	/// { "sunrise": "2024-06-14T20:31:00Z", "sunset": "2024-06-15T06:58:00Z" }
	/// @endcode
	Q_PROPERTY( QVariantMap sun READ sun NOTIFY snapshotChanged )
	/// Recent observed history from the snapshot's "weatherHistory" field.
	/// @return One point per sample:
	/// @code{.json}
	/// { "hour": "3pm", "temp": 21.4 }
	/// @endcode
	/// Same `hour`-labeling rule as hourlyForecast, but `temp` is a Celsius
	/// value with one decimal place (converted from the station's Fahrenheit
	/// reading) since this feeds a chart rather than a rounded display label.
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
