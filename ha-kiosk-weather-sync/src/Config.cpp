#include "Config.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

bool Config::load( const QString &path, Config &out, QString &error )
{
	QFile file( path );
	if ( !file.open( QIODevice::ReadOnly ) )
	{
		error = QStringLiteral( "cannot open config file %1: %2" ).arg( path, file.errorString() );
		return false;
	}

	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson( file.readAll(), &parseError );
	if ( parseError.error != QJsonParseError::NoError )
	{
		error = QStringLiteral( "invalid JSON in %1: %2" ).arg( path, parseError.errorString() );
		return false;
	}
	const QJsonObject root = doc.object();

	out.geohash = root.value( "geohash" ).toString();
	out.pollIntervalSeconds = root.value( "pollIntervalSeconds" ).toInt( 600 );
	out.snapshotPath = root.value( "snapshotPath" ).toString();

	out.influxUrl = root.value( "influxUrl" ).toString();
	out.influxOrg = root.value( "influxOrg" ).toString();
	out.influxBucket = root.value( "influxBucket" ).toString();
	out.influxToken = root.value( "influxToken" ).toString();

	// BOM's hourly-forecast and observations endpoints reject anything but a
	// lowercase-alphanumeric 6-character geohash (only the /locations search
	// endpoint returns a longer, 7-character one — see Config.h), so this
	// shape is enforced here rather than letting a malformed geohash surface
	// later as a cryptic BOM API error.
	static const QRegularExpression geohashPattern( QStringLiteral( "^[0-9a-z]{6}$" ) );
	if ( out.geohash.isEmpty() || !geohashPattern.match( out.geohash ).hasMatch() )
	{
		error = QStringLiteral( "config %1: \"geohash\" must be a 6-character BOM location geohash "
								"(resolve one via https://api.weather.bom.gov.au/v1/locations?search=<lat>,<lon> "
								"and drop the last character — that endpoint returns 7)" )
					.arg( path );
		return false;
	}
	if ( out.snapshotPath.isEmpty() )
	{
		error = QStringLiteral( "config %1: missing \"snapshotPath\"" ).arg( path );
		return false;
	}

	// The four InfluxDB settings only make sense as a set (see Config.h), so
	// they're validated as one optional group rather than individually: a
	// config is either fully wired for InfluxDB or plainly not using it,
	// never silently half-configured.
	const bool anyInflux =
		!out.influxUrl.isEmpty() || !out.influxOrg.isEmpty() || !out.influxBucket.isEmpty() || !out.influxToken.isEmpty();
	const bool allInflux =
		!out.influxUrl.isEmpty() && !out.influxOrg.isEmpty() && !out.influxBucket.isEmpty() && !out.influxToken.isEmpty();
	if ( anyInflux && !allInflux )
	{
		error = QStringLiteral( "config %1: \"influxUrl\"/\"influxOrg\"/\"influxBucket\"/\"influxToken\" "
								"must be set together or all left empty" )
					.arg( path );
		return false;
	}

	return true;
}
