#pragma once

#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <functional>

/// Thin wrapper over BOM's public JSON weather API
/// (https://api.weather.bom.gov.au/v1) — the same API BOM's own website and
/// app use. Unlike CalendarClient, there is no auth of any kind: every call
/// here is a bare, unauthenticated GET, and this class only ever reads BOM's
/// data — it has no counterpart to CalendarClient's patch/insert/delete
/// methods. That's not an oversight: BOM is a read-only upstream, so this
/// client is the query side's *only* side, end to end.
class BomClient : public QObject
{
	Q_OBJECT
  public:
	/// @param parent Standard QObject ownership parent.
	explicit BomClient( QObject *parent = nullptr );

	/// GET /locations/{geohash}/forecasts/daily — one entry per day, each
	/// with temp_max/temp_min/rain/icon_descriptor.
	/// @param geohash BOM geohash identifying the forecast location.
	/// @param callback Invoked once with the response; on failure @p days is
	/// empty and @p error is set.
	void fetchDaily( const QString &geohash, std::function<void( QJsonValue days, QString error )> callback );

	/// GET /locations/{geohash}/forecasts/hourly — one entry per hour, each
	/// with time/temp/icon_descriptor.
	/// @param geohash BOM geohash identifying the forecast location.
	/// @param callback Invoked once with the response; on failure @p hours
	/// is empty and @p error is set.
	void fetchHourly( const QString &geohash, std::function<void( QJsonValue hours, QString error )> callback );

  private:
	// Shared GET + "data" field unwrap for both endpoints above — BOM
	// wraps every response as {"metadata": {...}, "data": ...}.
	void getData( const QString &path, std::function<void( QJsonValue data, QString error )> callback );

	QNetworkAccessManager m_nam;
};
