#include "BomClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace
{
constexpr auto kApiBase = "https://api.weather.bom.gov.au/v1/locations/";
}

BomClient::BomClient( QObject *parent ) : QObject( parent )
{
}

void BomClient::getData( const QString &path, std::function<void( QJsonValue, QString )> callback )
{
	QNetworkRequest request( QUrl( QString::fromLatin1( kApiBase ) + path ) );
	QNetworkReply *reply = m_nam.get( request );
	connect( reply, &QNetworkReply::finished, this, [reply, callback]() {
		reply->deleteLater();
		const QByteArray body = reply->readAll();
		if ( reply->error() != QNetworkReply::NoError )
		{
			callback( {}, QStringLiteral( "BOM API request failed: %1%2" )
							  .arg( reply->errorString(),
									body.isEmpty() ? QString()
												   : QStringLiteral( " — %1" ).arg( QString::fromUtf8( body ) ) ) );
			return;
		}
		callback( QJsonDocument::fromJson( body ).object().value( "data" ), QString() );
	} );
}

// geohash is already validated by Config::load as ^[0-9a-z]{6}$, so it's
// URL-safe as-is — no percent-encoding needed.
void BomClient::fetchDaily( const QString &geohash, std::function<void( QJsonValue, QString )> callback )
{
	getData( geohash + "/forecasts/daily", callback );
}

void BomClient::fetchHourly( const QString &geohash, std::function<void( QJsonValue, QString )> callback )
{
	getData( geohash + "/forecasts/hourly", callback );
}
