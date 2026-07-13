#pragma once

#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <functional>

/// Thin wrapper over a self-hosted InfluxDB 2.x instance holding Telegraf-
/// collected Ecowitt WH65LP readings (bridged through ecowitt2mqtt) — the
/// local counterpart to BomClient's forecast, since BOM has no historic-
/// observation endpoint of its own. Read-only, same reasoning as BomClient:
/// this daemon never writes to InfluxDB, only queries it.
class InfluxClient : public QObject
{
	Q_OBJECT
  public:
	/// Stores the InfluxDB 2.x connection details used by every query below;
	/// no connection is made until the first fetch call.
	/// @param url InfluxDB server base URL.
	/// @param org InfluxDB organization name.
	/// @param bucket InfluxDB bucket to query.
	/// @param token InfluxDB API token.
	/// @param parent Standard QObject ownership parent.
	InfluxClient(
		const QString &url, const QString &org, const QString &bucket, const QString &token, QObject *parent = nullptr );

	/// Queries the last `hours` of the "weather" measurement's "temp" field
	/// (imperial/Fahrenheit, per the ecowitt2mqtt bridge's configured unit
	/// system), averaged into one bucket per `windowMinutes` via Flux's
	/// aggregateWindow — the raw feed is roughly one sample every 15s, far
	/// finer than even this bucketing keeps.
	/// @return Oldest-first array of:
	/// @code{.json}
	/// { "time": "2024-06-14T20:31:00Z", "fahrenheit": 68.4 }
	/// @endcode
	/// On failure, the array is empty and error is set.
	void fetchTemperatureHistory( int hours, int windowMinutes, std::function<void( QJsonValue rows, QString error )> callback );

	/// Queries the most recent humidity/windspeed/dailyrain readings (each
	/// still in the ecowitt2mqtt bridge's imperial units — %, mph, inches).
	/// This is the local station's own current conditions, replacing BOM's
	/// nearest-station observations for the humidity/wind/rain stats shown
	/// on WeatherCard (whose title already names this station).
	/// @return A flat object of field name -> latest value; any field with
	/// no recent reading is simply absent:
	/// @code{.json}
	/// { "humidity": 90, "windspeed": 8.5, "dailyrain": 0.02 }
	/// @endcode
	/// On failure, the object is empty and error is set.
	void fetchCurrentConditions( std::function<void( QJsonValue conditions, QString error )> callback );

  private:
	QString m_url;
	QString m_org;
	QString m_bucket;
	QString m_token;
	QNetworkAccessManager m_nam;
};
