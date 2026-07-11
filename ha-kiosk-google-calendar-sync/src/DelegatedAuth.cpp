#include "DelegatedAuth.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace
{

constexpr auto kDeviceCodeEndpoint = "https://oauth2.googleapis.com/device/code";
constexpr auto kTokenEndpoint = "https://oauth2.googleapis.com/token";
constexpr auto kScope = "https://www.googleapis.com/auth/calendar";
constexpr auto kDeviceGrantType = "urn:ietf:params:oauth:grant-type:device_code";
constexpr qint64 kRefreshMarginSecs = 60;

} // namespace

DelegatedAuth::DelegatedAuth( const QString &clientId, const QString &clientSecret,
							  const QString &tokenStorePath, QObject *parent )
	: QObject( parent ), m_clientId( clientId ), m_clientSecret( clientSecret ),
	  m_tokenStorePath( tokenStorePath )
{
}

QString DelegatedAuth::loadRefreshToken() const
{
	QFile file( m_tokenStorePath );
	if ( !file.open( QIODevice::ReadOnly ) )
		return {};
	return QJsonDocument::fromJson( file.readAll() ).object().value( "refresh_token" ).toString();
}

void DelegatedAuth::persistRefreshToken( const QString &refreshToken ) const
{
	QDir().mkpath( QFileInfo( m_tokenStorePath ).absolutePath() );
	QFile file( m_tokenStorePath );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return;
	file.write(
		QJsonDocument( QJsonObject{ { "refresh_token", refreshToken } } ).toJson( QJsonDocument::Compact ) );
	file.close();
	file.setPermissions( QFile::ReadOwner | QFile::WriteOwner );
}

void DelegatedAuth::accessToken( std::function<void( QString, QString )> callback )
{
	const qint64 now = QDateTime::currentSecsSinceEpoch();
	if ( !m_cachedToken.isEmpty() && now < m_cachedTokenExpiry )
	{
		callback( m_cachedToken, QString() );
		return;
	}

	if ( !loadRefreshToken().isEmpty() )
	{
		refreshAccessToken( callback );
		return;
	}

	requestDeviceCode( callback );
}

void DelegatedAuth::refreshAccessToken( std::function<void( QString, QString )> callback )
{
	QUrlQuery form;
	form.addQueryItem( "client_id", m_clientId );
	form.addQueryItem( "client_secret", m_clientSecret );
	form.addQueryItem( "refresh_token", loadRefreshToken() );
	form.addQueryItem( "grant_type", "refresh_token" );

	QNetworkRequest request( QUrl( QString::fromLatin1( kTokenEndpoint ) ) );
	request.setHeader( QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded" );

	QNetworkReply *reply = m_nam.post( request, form.query( QUrl::FullyEncoded ).toUtf8() );
	connect( reply, &QNetworkReply::finished, this, [this, reply, callback]() {
		reply->deleteLater();
		if ( reply->error() != QNetworkReply::NoError )
		{
			// The stored refresh token can be revoked out from under us
			// (e.g. a human removed the app's access at
			// myaccount.google.com/permissions) — fall back to a fresh
			// device-code grant rather than failing forever.
			requestDeviceCode( callback );
			return;
		}
		handleTokenResponse( QJsonDocument::fromJson( reply->readAll() ).object(), callback );
	} );
}

void DelegatedAuth::requestDeviceCode( std::function<void( QString, QString )> callback )
{
	QUrlQuery form;
	form.addQueryItem( "client_id", m_clientId );
	form.addQueryItem( "scope", kScope );

	QNetworkRequest request( QUrl( QString::fromLatin1( kDeviceCodeEndpoint ) ) );
	request.setHeader( QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded" );

	QNetworkReply *reply = m_nam.post( request, form.query( QUrl::FullyEncoded ).toUtf8() );
	connect( reply, &QNetworkReply::finished, this, [this, reply, callback]() {
		reply->deleteLater();
		const QByteArray body = reply->readAll();
		if ( reply->error() != QNetworkReply::NoError )
		{
			callback( QString(), QStringLiteral( "device code request failed: %1 — %2" )
										.arg( reply->errorString(), QString::fromUtf8( body ) ) );
			return;
		}

		const QJsonObject obj = QJsonDocument::fromJson( body ).object();
		const QString deviceCode = obj.value( "device_code" ).toString();
		const QString userCode = obj.value( "user_code" ).toString();
		const QString verificationUrl = obj.value( "verification_url" ).toString();
		const int expiresIn = obj.value( "expires_in" ).toInt( 1800 );
		const int interval = qMax( 1, obj.value( "interval" ).toInt( 5 ) );

		if ( deviceCode.isEmpty() || userCode.isEmpty() )
		{
			callback( QString(), QStringLiteral( "device code endpoint returned no device_code/user_code" ) );
			return;
		}

		emit authorizationPending( verificationUrl, userCode, expiresIn );
		pollForToken( deviceCode, interval, QDateTime::currentSecsSinceEpoch() + expiresIn, callback );
	} );
}

void DelegatedAuth::pollForToken( const QString &deviceCode, int intervalSecs, qint64 deadlineEpoch,
								  std::function<void( QString, QString )> callback )
{
	if ( QDateTime::currentSecsSinceEpoch() >= deadlineEpoch )
	{
		callback( QString(), QStringLiteral( "device code expired before authorization was completed" ) );
		return;
	}

	QTimer::singleShot( intervalSecs * 1000, this, [this, deviceCode, intervalSecs, deadlineEpoch, callback]() {
		QUrlQuery form;
		form.addQueryItem( "client_id", m_clientId );
		form.addQueryItem( "client_secret", m_clientSecret );
		form.addQueryItem( "device_code", deviceCode );
		form.addQueryItem( "grant_type", QString::fromLatin1( kDeviceGrantType ) );

		QNetworkRequest request( QUrl( QString::fromLatin1( kTokenEndpoint ) ) );
		request.setHeader( QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded" );

		QNetworkReply *reply = m_nam.post( request, form.query( QUrl::FullyEncoded ).toUtf8() );
		connect( reply, &QNetworkReply::finished, this,
				 [this, reply, deviceCode, intervalSecs, deadlineEpoch, callback]() {
					 reply->deleteLater();
					 const QJsonObject obj = QJsonDocument::fromJson( reply->readAll() ).object();
					 const QString error = obj.value( "error" ).toString();

					 if ( error == QLatin1String( "authorization_pending" ) )
					 {
						 pollForToken( deviceCode, intervalSecs, deadlineEpoch, callback );
						 return;
					 }
					 if ( error == QLatin1String( "slow_down" ) )
					 {
						 pollForToken( deviceCode, intervalSecs + 5, deadlineEpoch, callback );
						 return;
					 }
					 if ( !error.isEmpty() )
					 {
						 callback( QString(), QStringLiteral( "device code grant failed: %1" ).arg( error ) );
						 return;
					 }

					 handleTokenResponse( obj, callback );
				 } );
	} );
}

void DelegatedAuth::handleTokenResponse( const QJsonObject &obj,
										 std::function<void( QString, QString )> callback )
{
	const QString accessToken = obj.value( "access_token" ).toString();
	const int expiresIn = obj.value( "expires_in" ).toInt();
	const QString refreshToken = obj.value( "refresh_token" ).toString();

	if ( accessToken.isEmpty() )
	{
		callback( QString(), QStringLiteral( "token endpoint returned no access_token" ) );
		return;
	}

	// Google only returns refresh_token on the very first grant for a given
	// client/scope pair — a refresh-token-exchange response omits it, so
	// only overwrite the stored one when we actually got a new one.
	if ( !refreshToken.isEmpty() )
		persistRefreshToken( refreshToken );

	m_cachedToken = accessToken;
	m_cachedTokenExpiry = QDateTime::currentSecsSinceEpoch() + expiresIn - kRefreshMarginSecs;
	callback( accessToken, QString() );
}
