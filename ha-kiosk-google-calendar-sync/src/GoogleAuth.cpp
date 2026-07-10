#include "GoogleAuth.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

namespace
{

constexpr auto kTokenEndpoint = "https://oauth2.googleapis.com/token";
constexpr auto kScope = "https://www.googleapis.com/auth/calendar";
constexpr qint64 kJwtLifetimeSecs = 3600;
constexpr qint64 kRefreshMarginSecs = 60;

QByteArray base64UrlEncode( const QByteArray &raw )
{
	return raw.toBase64( QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals );
}

QString opensslErrors()
{
	QString out;
	unsigned long code;
	while ( ( code = ERR_get_error() ) != 0 )
	{
		char buf[256];
		ERR_error_string_n( code, buf, sizeof( buf ) );
		if ( !out.isEmpty() )
			out += "; ";
		out += QString::fromLatin1( buf );
	}
	return out.isEmpty() ? QStringLiteral( "unknown OpenSSL error" ) : out;
}

} // namespace

GoogleAuth::GoogleAuth( const QString &serviceAccountKeyPath, QObject *parent )
	: QObject( parent ), m_keyPath( serviceAccountKeyPath )
{
}

bool GoogleAuth::init( QString &error )
{
	QFile file( m_keyPath );
	if ( !file.open( QIODevice::ReadOnly ) )
	{
		error =
			QStringLiteral( "cannot open service account key %1: %2" ).arg( m_keyPath, file.errorString() );
		return false;
	}

	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson( file.readAll(), &parseError );
	if ( parseError.error != QJsonParseError::NoError )
	{
		error = QStringLiteral( "service account key %1 is not valid JSON: %2" )
					.arg( m_keyPath, parseError.errorString() );
		return false;
	}
	const QJsonObject root = doc.object();

	m_clientEmail = root.value( "client_email" ).toString();
	// Qt's JSON parser already unescapes the "\n" sequences inside the JSON
	// string into real newlines, so this is a directly usable PEM blob.
	m_privateKeyPem = root.value( "private_key" ).toString().toUtf8();

	if ( m_clientEmail.isEmpty() || m_privateKeyPem.isEmpty() )
	{
		error = QStringLiteral( "service account key %1 is missing \"client_email\" or \"private_key\"" )
					.arg( m_keyPath );
		return false;
	}
	return true;
}

QString GoogleAuth::buildSignedJwt( QString &error ) const
{
	const qint64 now = QDateTime::currentSecsSinceEpoch();

	const QJsonObject header{ { "alg", "RS256" }, 
							  { "typ", "JWT" } };
	const QJsonObject claims{ { "iss", m_clientEmail },
							  { "scope", kScope },
							  { "aud", kTokenEndpoint },
							  { "iat", now },
							  { "exp", now + kJwtLifetimeSecs } };

	const QByteArray signingInput =
		base64UrlEncode( QJsonDocument( header ).toJson( QJsonDocument::Compact ) ) + "." +
		base64UrlEncode( QJsonDocument( claims ).toJson( QJsonDocument::Compact ) );

	BIO *bio = BIO_new_mem_buf( m_privateKeyPem.constData(), m_privateKeyPem.size() );
	EVP_PKEY *pkey = PEM_read_bio_PrivateKey( bio, nullptr, nullptr, nullptr );
	BIO_free( bio );
	if ( !pkey )
	{
		error = QStringLiteral( "failed to parse service account private key: %1" ).arg( opensslErrors() );
		return {};
	}

	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	QByteArray signature;
	bool ok = ctx && EVP_DigestSignInit( ctx, nullptr, EVP_sha256(), nullptr, pkey ) == 1;
	if ( ok )
	{
		size_t sigLen = 0;
		ok = EVP_DigestSign( ctx, nullptr, &sigLen,
							 reinterpret_cast<const unsigned char *>( signingInput.constData() ),
							 signingInput.size() ) == 1;
		if ( ok )
		{
			signature.resize( static_cast<int>( sigLen ) );
			ok = EVP_DigestSign( ctx, reinterpret_cast<unsigned char *>( signature.data() ), &sigLen,
								 reinterpret_cast<const unsigned char *>( signingInput.constData() ),
								 signingInput.size() ) == 1;
			signature.resize( static_cast<int>( sigLen ) );
		}
	}
	if ( ctx )
		EVP_MD_CTX_free( ctx );
	EVP_PKEY_free( pkey );

	if ( !ok )
	{
		error = QStringLiteral( "failed to sign JWT: %1" ).arg( opensslErrors() );
		return {};
	}

	return QString::fromLatin1( signingInput ) + "." + QString::fromLatin1( base64UrlEncode( signature ) );
}

void GoogleAuth::accessToken( std::function<void( QString, QString )> callback )
{
	const qint64 now = QDateTime::currentSecsSinceEpoch();
	if ( !m_cachedToken.isEmpty() && now < m_cachedTokenExpiry )
	{
		callback( m_cachedToken, QString() );
		return;
	}

	QString error;
	const QString jwt = buildSignedJwt( error );
	if ( jwt.isEmpty() )
	{
		callback( QString(), error );
		return;
	}
	requestToken( jwt, callback );
}

void GoogleAuth::requestToken( const QString &jwt, std::function<void( QString, QString )> callback )
{
	QUrlQuery body;
	body.addQueryItem( "grant_type", "urn:ietf:params:oauth:grant-type:jwt-bearer" );
	body.addQueryItem( "assertion", jwt );

	QNetworkRequest request( QUrl( QString::fromLatin1( kTokenEndpoint ) ) );
	request.setHeader( QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded" );

	QNetworkReply *reply = m_nam.post( request, body.query( QUrl::FullyEncoded ).toUtf8() );
	connect( reply, &QNetworkReply::finished, this, [this, reply, callback]() {
		reply->deleteLater();
		if ( reply->error() != QNetworkReply::NoError )
		{
			const QByteArray body = reply->readAll();
			callback( QString(),
					  QStringLiteral( "token request failed: %1%2" )
						  .arg( reply->errorString(), body.isEmpty() ? QString()
																	 : QStringLiteral( " — %1" ).arg(
																		   QString::fromUtf8( body ) ) ) );
			return;
		}

		const QJsonObject obj = QJsonDocument::fromJson( reply->readAll() ).object();
		const QString token = obj.value( "access_token" ).toString();
		const int expiresIn = obj.value( "expires_in" ).toInt();
		if ( token.isEmpty() )
		{
			callback( QString(), QStringLiteral( "token endpoint returned no access_token" ) );
			return;
		}

		m_cachedToken = token;
		m_cachedTokenExpiry = QDateTime::currentSecsSinceEpoch() + expiresIn - kRefreshMarginSecs;
		callback( token, QString() );
	} );
}
