#include "InfluxClient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace
{

// Minimal parser for the flat (non-annotated) CSV InfluxDB's v2 query API
// returns for Accept: application/csv — one header row naming the columns,
// then one data row per sample. Good enough for this query's shape (no
// column value here ever contains a comma); not a general CSV parser.
QJsonArray parseCsv( const QByteArray &body, const QString &timeColumn, const QString &valueColumn )
{
	QJsonArray rows;
	const QStringList lines = QString::fromUtf8( body ).split( QLatin1Char( '\n' ), Qt::SkipEmptyParts );
	if ( lines.isEmpty() )
		return rows;

	const QStringList header = lines.first().split( QLatin1Char( ',' ) );
	const int timeIdx = header.indexOf( timeColumn );
	const int valueIdx = header.indexOf( valueColumn );
	if ( timeIdx < 0 || valueIdx < 0 )
		return rows;

	for ( int i = 1; i < lines.size(); ++i )
	{
		const QStringList cols = lines.at( i ).split( QLatin1Char( ',' ) );
		if ( cols.size() <= qMax( timeIdx, valueIdx ) )
			continue;

		bool ok = false;
		const double value = cols.at( valueIdx ).toDouble( &ok );
		if ( !ok )
			continue;

		QJsonObject row;
		row["time"] = cols.at( timeIdx ).trimmed();
		row["fahrenheit"] = value;
		rows.append( row );
	}
	return rows;
}

// Same flat-CSV shape as parseCsv above, but for a query that returns one
// row per *field* (last() across several fields at once) rather than one
// row per timestamp of a single field — so this keys off _field/_value
// instead of _time/_value.
QJsonObject parseLatestValues( const QByteArray &body )
{
	QJsonObject result;
	const QStringList lines = QString::fromUtf8( body ).split( QLatin1Char( '\n' ), Qt::SkipEmptyParts );
	if ( lines.isEmpty() )
		return result;

	const QStringList header = lines.first().split( QLatin1Char( ',' ) );
	const int fieldIdx = header.indexOf( QStringLiteral( "_field" ) );
	const int valueIdx = header.indexOf( QStringLiteral( "_value" ) );
	if ( fieldIdx < 0 || valueIdx < 0 )
		return result;

	for ( int i = 1; i < lines.size(); ++i )
	{
		const QStringList cols = lines.at( i ).split( QLatin1Char( ',' ) );
		if ( cols.size() <= qMax( fieldIdx, valueIdx ) )
			continue;

		bool ok = false;
		const double value = cols.at( valueIdx ).toDouble( &ok );
		if ( !ok )
			continue;

		result[cols.at( fieldIdx ).trimmed()] = value;
	}
	return result;
}

} // namespace

InfluxClient::InfluxClient( const QString &url, const QString &org, const QString &bucket, const QString &token, QObject *parent )
	: QObject( parent ), m_url( url ), m_org( org ), m_bucket( bucket ), m_token( token )
{
}

void InfluxClient::fetchTemperatureHistory( int hours, int windowMinutes, std::function<void( QJsonValue, QString )> callback )
{
	QUrl url( m_url + "/api/v2/query" );
	QUrlQuery query;
	query.addQueryItem( "org", m_org );
	url.setQuery( query );

	QNetworkRequest request( url );
	request.setRawHeader( "Authorization", "Token " + m_token.toUtf8() );
	request.setRawHeader( "Accept", "application/csv" );
	request.setRawHeader( "Content-Type", "application/vnd.flux" );

	// bucket comes from local operator-controlled config, not untrusted
	// input, so string interpolation into the Flux query body is fine here.
	const QString flux = QStringLiteral( "from(bucket: \"%1\")\n"
										 "  |> range(start: -%2h)\n"
										 "  |> filter(fn: (r) => r._measurement == \"weather\" and r._field == \"temp\")\n"
										 "  |> aggregateWindow(every: %3m, fn: mean, createEmpty: false)\n" )
							 .arg( m_bucket )
							 .arg( hours )
							 .arg( windowMinutes );

	QNetworkReply *reply = m_nam.post( request, flux.toUtf8() );
	connect( reply,
			 &QNetworkReply::finished,
			 this,
			 [reply, callback]()
			 {
				 reply->deleteLater();
				 const QByteArray body = reply->readAll();
				 if ( reply->error() != QNetworkReply::NoError )
				 {
					 callback(
						 {},
						 QStringLiteral( "InfluxDB query failed: %1%2" )
							 .arg( reply->errorString(),
								   body.isEmpty() ? QString() : QStringLiteral( " — %1" ).arg( QString::fromUtf8( body ) ) ) );
					 return;
				 }
				 callback( parseCsv( body, QStringLiteral( "_time" ), QStringLiteral( "_value" ) ), QString() );
			 } );
}

void InfluxClient::fetchCurrentConditions( std::function<void( QJsonValue, QString )> callback )
{
	QUrl url( m_url + "/api/v2/query" );
	QUrlQuery query;
	query.addQueryItem( "org", m_org );
	url.setQuery( query );

	QNetworkRequest request( url );
	request.setRawHeader( "Authorization", "Token " + m_token.toUtf8() );
	request.setRawHeader( "Accept", "application/csv" );
	request.setRawHeader( "Content-Type", "application/vnd.flux" );

	// 6h lookback rather than 1h: same reasoning as the geohash/bucket trust
	// note above doesn't apply here, this is just slack for the station
	// being briefly offline — last() still finds the latest reading in that
	// window rather than the query coming back empty.
	const QString flux = QStringLiteral( "from(bucket: \"%1\")\n"
										 "  |> range(start: -6h)\n"
										 "  |> filter(fn: (r) => r._measurement == \"weather\" and (r._field == \"humidity\" "
										 "or r._field == \"windspeed\" or r._field == \"dailyrain\"))\n"
										 "  |> last()\n" )
							 .arg( m_bucket );

	QNetworkReply *reply = m_nam.post( request, flux.toUtf8() );
	connect( reply,
			 &QNetworkReply::finished,
			 this,
			 [reply, callback]()
			 {
				 reply->deleteLater();
				 const QByteArray body = reply->readAll();
				 if ( reply->error() != QNetworkReply::NoError )
				 {
					 callback(
						 {},
						 QStringLiteral( "InfluxDB query failed: %1%2" )
							 .arg( reply->errorString(),
								   body.isEmpty() ? QString() : QStringLiteral( " — %1" ).arg( QString::fromUtf8( body ) ) ) );
					 return;
				 }
				 callback( parseLatestValues( body ), QString() );
			 } );
}
