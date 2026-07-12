#pragma once

#include <QString>

struct Config
{
	// A 6-character BOM location geohash, e.g. "r3gx2f" — resolve one for
	// your location with:
	//   curl "https://api.weather.bom.gov.au/v1/locations?search=<lat>,<lon>"
	// That endpoint returns a 7-character geohash; BOM's daily-forecast
	// endpoint accepts that as-is, but hourly-forecast and observations
	// both reject it ("Invalid Geohash ... not 6 character") — drop the
	// last character before putting it here.
	QString geohash;
	int pollIntervalSeconds = 600;
	QString snapshotPath;

	// InfluxDB 2.x connection for a local Telegraf-collected temperature
	// history (see WeatherBridge::weatherHistory). Optional as a group:
	// leave all four empty/omitted to run without a "weatherHistory"
	// snapshot key — e.g. a dev setup with no InfluxDB reachable.
	QString influxUrl; // e.g. "http://192.168.1.150:8086"
	QString influxOrg;
	QString influxBucket;
	QString influxToken;

	// Reads and validates a config JSON file at `path`. On failure, returns
	// false and fills `error` with a message suitable for logging as-is.
	static bool load( const QString &path, Config &out, QString &error );
};
