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
		entry["hour"] = hourLabel( dt );
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

QJsonObject buildObservations( const QJsonValue &observations )
{
	const QJsonObject obs = observations.toObject();
	const QJsonObject wind = obs.value( "wind" ).toObject();

	QJsonObject result;
	result["humidity"] = QString::number( qRound( obs.value( "humidity" ).toDouble() ) ) + "%";
	result["windSpeed"] =
		QString::number( qRound( wind.value( "speed_kilometre" ).toDouble() ) ) + QStringLiteral( " km/h" );
	result["rainToday"] =
		QString::number( obs.value( "rain_since_9am" ).toDouble(), 'f', 1 ) + QStringLiteral( " mm" );
	return result;
}

} // namespace

QJsonObject SnapshotBuilder::build( const QJsonValue &daily, const QJsonValue &hourly,
									 const QJsonValue &observations )
{
	QJsonObject snapshot;
	snapshot["hourlyForecast"] = buildHourlyForecast( hourly );
	snapshot["forecast"] = buildDailyForecast( daily );
	snapshot["observations"] = buildObservations( observations );
	return snapshot;
}
