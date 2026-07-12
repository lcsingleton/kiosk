#include "SnapshotBuilder.h"

#include "IconMap.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

namespace
{

QDateTime parseIso( const QString &s )
{
	QDateTime dt = QDateTime::fromString( s, Qt::ISODateWithMs );
	if ( !dt.isValid() )
		dt = QDateTime::fromString( s, Qt::ISODate );
	return dt.toLocalTime();
}

// "3pm", "4pm", ... — no leading zero, lowercase am/pm, matching the mock
// hour labels in DashboardData.qml.
QString hourLabel( const QDateTime &dt )
{
	const int hour24 = dt.time().hour();
	int hour12 = hour24 % 12;
	if ( hour12 == 0 )
		hour12 = 12;
	return QString::number( hour12 ) + ( hour24 < 12 ? "am" : "pm" );
}

QString tempString( double celsius )
{
	return QString::number( qRound( celsius ) ) + QStringLiteral( "°" );
}

// Whether a point at `dt` should carry a chart label — every 3rd hour, on
// the hour. Used for both hourlyForecast (always hourly) and weatherHistory
// (now sub-hourly buckets, see InfluxClient), so the chart's x-axis stays
// legible regardless of how many underlying points exist between labels —
// see LineChart.qml, which places a gridline/label at any point whose
// `label` is non-empty rather than at a fixed point-index stride.
bool isLabelHour( const QDateTime &dt )
{
	return dt.time().minute() == 0 && dt.time().hour() % 3 == 0;
}

QJsonArray buildHourlyForecast( const QJsonValue &hourly )
{
	// "Today"/hour-of-day are evaluated against the host's configured
	// system timezone, not the geohash's — the two only agree if this
	// daemon runs on a machine whose system tz is actually set to the
	// geohash's location (true for the tablet in practice; worth knowing
	// if testing this daemon somewhere else, e.g. a UTC dev container).
	const QDateTime now = QDateTime::currentDateTime();
	QJsonArray result;
	for ( const QJsonValue &v : hourly.toArray() )
	{
		const QJsonObject h = v.toObject();
		const QDateTime dt = parseIso( h.value( "time" ).toString() );
		if ( !dt.isValid() || dt < now || dt.date() != now.date() )
			continue;

		QJsonObject entry;
		entry["hour"] = isLabelHour( dt ) ? hourLabel( dt ) : QString();
		entry["icon"] = IconMap::emoji( h.value( "icon_descriptor" ).toString() );
		entry["temp"] = qRound( h.value( "temp" ).toDouble() );
		result.append( entry );
	}
	return result;
}

// BOM's response runs one day longer than its own forecast horizon: the
// trailing day is a stub with temp_max/icon_descriptor still null (only
// temp_min is known that far out). Skip anything incomplete rather than
// rendering a bogus "0°" high, and cap at 7 regardless (the 7-day strip
// this feeds).
QJsonArray buildDailyForecast( const QJsonValue &daily )
{
	QJsonArray result;
	for ( const QJsonValue &v : daily.toArray() )
	{
		if ( result.size() >= 7 )
			break;

		const QJsonObject d = v.toObject();
		if ( d.value( "temp_max" ).isNull() || d.value( "icon_descriptor" ).isNull() )
			continue;

		const QDateTime dt = parseIso( d.value( "date" ).toString() );

		QJsonObject entry;
		entry["day"] = result.isEmpty() ? QStringLiteral( "Today" ) : dt.date().toString( "ddd" );
		entry["icon"] = IconMap::emoji( d.value( "icon_descriptor" ).toString() );
		entry["hi"] = tempString( d.value( "temp_max" ).toDouble() );
		entry["lo"] = tempString( d.value( "temp_min" ).toDouble() );
		result.append( entry );
	}
	return result;
}

// Sunrise/sunset for "today" (BOM's day-0 entry), passed through verbatim
// as BOM's own ISO8601 UTC strings — unlike the hourly/daily helpers above,
// these are absolute instants compared directly against "now" on the QML
// side, so there's no system-tz-vs-geohash-tz assumption to worry about.
QJsonObject buildSun( const QJsonValue &daily )
{
	const QJsonArray days = daily.toArray();
	if ( days.isEmpty() )
		return {};

	const QJsonObject astronomical = days.first().toObject().value( "astronomical" ).toObject();

	QJsonObject result;
	result["sunrise"] = astronomical.value( "sunrise_time" ).toString();
	result["sunset"] = astronomical.value( "sunset_time" ).toString();
	return result;
}

double fahrenheitToCelsius( double fahrenheit )
{
	return ( fahrenheit - 32.0 ) * 5.0 / 9.0;
}

// Telegraf/InfluxDB's finely-bucketed Ecowitt readings (see InfluxClient),
// reshaped to the same { hour, temp } points as the mock weatherHistory
// literal it replaces in DashboardData.qml — one decimal place, same as
// that mock, since this feeds a chart rather than a rounded display label.
// Only on-the-hour, every-3rd-hour points get a non-empty "hour" label
// (see isLabelHour) — the rest are blank, since labeling every few-minute
// bucket would flood the chart's x-axis.
QJsonArray buildWeatherHistory( const QJsonValue &influxRows )
{
	QJsonArray result;
	for ( const QJsonValue &v : influxRows.toArray() )
	{
		const QJsonObject row = v.toObject();
		const QDateTime dt = parseIso( row.value( "time" ).toString() );
		if ( !dt.isValid() )
			continue;

		QJsonObject entry;
		entry["hour"] = isLabelHour( dt ) ? hourLabel( dt ) : QString();
		entry["temp"] = qRound( fahrenheitToCelsius( row.value( "fahrenheit" ).toDouble() ) * 10.0 ) / 10.0;
		result.append( entry );
	}
	return result;
}

// Sourced from InfluxClient::fetchCurrentConditions — the Ecowitt station's
// own latest reading, not BOM's nearest-station observations (WeatherCard's
// title already names this station, so its own sensor is the more accurate
// source for the conditions shown right next to it). Imperial units in,
// same pre-formatted-string shape as the BOM-sourced version it replaces.
QJsonObject buildObservations( const QJsonValue &currentConditions )
{
	const QJsonObject c = currentConditions.toObject();

	QJsonObject result;
	result["humidity"] = QString::number( qRound( c.value( "humidity" ).toDouble() ) ) + "%";
	result["windSpeed"] =
		QString::number( qRound( c.value( "windspeed" ).toDouble() * 1.60934 ) ) + QStringLiteral( " km/h" );
	result["rainToday"] =
		QString::number( c.value( "dailyrain" ).toDouble() * 25.4, 'f', 1 ) + QStringLiteral( " mm" );
	return result;
}

} // namespace

QJsonObject SnapshotBuilder::build( const QJsonValue &daily, const QJsonValue &hourly,
									 const QJsonValue &currentConditions, const QJsonValue &weatherHistory )
{
	QJsonObject snapshot;
	snapshot["hourlyForecast"] = buildHourlyForecast( hourly );
	snapshot["forecast"] = buildDailyForecast( daily );
	snapshot["observations"] = buildObservations( currentConditions );
	snapshot["sun"] = buildSun( daily );
	snapshot["weatherHistory"] = buildWeatherHistory( weatherHistory );
	return snapshot;
}
