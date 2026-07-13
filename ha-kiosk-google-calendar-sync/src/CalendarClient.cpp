#include "CalendarClient.h"
#include "DelegatedAuth.h"
#include "GoogleAuth.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace
{

constexpr auto kApiBase = "https://www.googleapis.com/calendar/v3/calendars/";

QUrl eventUrl( const QString &calendarId, const QString &eventId = QString() )
{
	QString path = QString::fromLatin1( kApiBase ) + QUrl::toPercentEncoding( calendarId ) + "/events";
	if ( !eventId.isEmpty() )
		path += "/" + QUrl::toPercentEncoding( eventId );
	return QUrl( path );
}

// Maps an HTTP failure onto the daemon's closed set of error codes
// (conflict/not_found/auth_failure/upstream_unavailable/invalid_request)
// consumed by CommandServer when it packages a command result.
QPair<QString, QString> classifyFailure( QNetworkReply *reply, const QByteArray &body )
{
	const int status = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
	QString code;
	switch ( status )
	{
		case 400:
			code = QStringLiteral( "invalid_request" );
			break;
		case 401:
			code = QStringLiteral( "auth_failure" );
			break;
		case 403:
			// Distinguished from a generic auth_failure because it's
			// recoverable: CalendarClient::patchEvent retries it with
			// DelegatedAuth instead of just reporting failure. Matched on
			// Google's stable machine-readable reason code, not the
			// human-readable message (whose exact capitalization/wording —
			// "Domain-Wide Delegation of Authority" — isn't a contract the
			// way the reason code is).
			code = body.contains( "forbiddenForServiceAccounts" ) ? QStringLiteral( "delegation_required" )
																  : QStringLiteral( "auth_failure" );
			break;
		case 404:
		case 410:
			code = QStringLiteral( "not_found" );
			break;
		case 412:
			code = QStringLiteral( "conflict" );
			break;
		default:
			code = QStringLiteral( "upstream_unavailable" );
			break;
	}
	QString message = reply->errorString();
	if ( !body.isEmpty() )
		message += QStringLiteral( " — %1" ).arg( QString::fromUtf8( body ) );
	return { code, message };
}

} // namespace

CalendarClient::CalendarClient( GoogleAuth *auth, DelegatedAuth *delegatedAuth, QObject *parent )
	: QObject( parent ), m_auth( auth ), m_delegatedAuth( delegatedAuth )
{
}

void CalendarClient::listEvents( const QString &calendarId,
								 const QDateTime &timeMin,
								 const QDateTime &timeMax,
								 std::function<void( QJsonArray, QString )> callback )
{
	m_auth->accessToken(
		[this, calendarId, timeMin, timeMax, callback]( QString token, QString error )
		{
			if ( token.isEmpty() )
			{
				callback( {}, error );
				return;
			}

			// Recursively follows nextPageToken so a busy calendar's events
			// within the window are never silently truncated to one page.
			auto fetchPage = std::make_shared<std::function<void( QString, QJsonArray )>>();
			*fetchPage =
				[this, calendarId, timeMin, timeMax, token, callback, fetchPage]( QString pageToken, QJsonArray accumulated )
			{
				QUrl url = eventUrl( calendarId );
				QUrlQuery query;
				query.addQueryItem( "timeMin", timeMin.toUTC().toString( Qt::ISODate ) );
				query.addQueryItem( "timeMax", timeMax.toUTC().toString( Qt::ISODate ) );
				query.addQueryItem( "singleEvents", "true" );
				query.addQueryItem( "orderBy", "startTime" );
				// Calendar API's max page size; larger windows page via nextPageToken above.
				query.addQueryItem( "maxResults", "250" );
				if ( !pageToken.isEmpty() )
					query.addQueryItem( "pageToken", pageToken );
				url.setQuery( query );

				QNetworkRequest request( url );
				request.setRawHeader( "Authorization", "Bearer " + token.toUtf8() );

				QNetworkReply *reply = m_nam.get( request );
				connect( reply,
						 &QNetworkReply::finished,
						 this,
						 [reply, accumulated, callback, fetchPage]() mutable
						 {
							 reply->deleteLater();
							 if ( reply->error() != QNetworkReply::NoError )
							 {
								 const QByteArray body = reply->readAll();
								 callback( {},
										   QStringLiteral( "Calendar API request failed: %1%2" )
											   .arg( reply->errorString(),
													 body.isEmpty()
														 ? QString()
														 : QStringLiteral( " — %1" ).arg( QString::fromUtf8( body ) ) ) );
								 return;
							 }

							 const QJsonObject obj = QJsonDocument::fromJson( reply->readAll() ).object();
							 for ( const QJsonValue &v : obj.value( "items" ).toArray() )
								 accumulated.append( v );

							 const QString nextPageToken = obj.value( "nextPageToken" ).toString();
							 if ( nextPageToken.isEmpty() )
								 callback( accumulated, QString() );
							 else
								 ( *fetchPage )( nextPageToken, accumulated );
						 } );
			};
			( *fetchPage )( QString(), QJsonArray() );
		} );
}

void CalendarClient::getEvent( const QString &calendarId,
							   const QString &eventId,
							   std::function<void( QJsonObject, QString, QString )> callback )
{
	m_auth->accessToken(
		[this, calendarId, eventId, callback]( QString token, QString authError )
		{
			if ( token.isEmpty() )
			{
				callback( {}, QStringLiteral( "auth_failure" ), authError );
				return;
			}

			QNetworkRequest request( eventUrl( calendarId, eventId ) );
			request.setRawHeader( "Authorization", "Bearer " + token.toUtf8() );

			QNetworkReply *reply = m_nam.get( request );
			connect( reply,
					 &QNetworkReply::finished,
					 this,
					 [reply, callback]()
					 {
						 reply->deleteLater();
						 const QByteArray body = reply->readAll();
						 if ( reply->error() != QNetworkReply::NoError )
						 {
							 const auto [code, message] = classifyFailure( reply, body );
							 callback( {}, code, message );
							 return;
						 }
						 callback( QJsonDocument::fromJson( body ).object(), QString(), QString() );
					 } );
		} );
}

void CalendarClient::fetchColorDefinitions( std::function<void( QJsonObject, QJsonObject, QString )> callback )
{
	m_auth->accessToken(
		[this, callback]( QString token, QString authError )
		{
			if ( token.isEmpty() )
			{
				callback( {}, {}, authError );
				return;
			}

			QNetworkRequest request( QUrl( QStringLiteral( "https://www.googleapis.com/calendar/v3/colors" ) ) );
			request.setRawHeader( "Authorization", "Bearer " + token.toUtf8() );

			QNetworkReply *reply = m_nam.get( request );
			connect( reply,
					 &QNetworkReply::finished,
					 this,
					 [reply, callback]()
					 {
						 reply->deleteLater();
						 const QByteArray body = reply->readAll();
						 if ( reply->error() != QNetworkReply::NoError )
						 {
							 const auto [code, message] = classifyFailure( reply, body );
							 callback( {}, {}, QStringLiteral( "%1: %2" ).arg( code, message ) );
							 return;
						 }
						 const QJsonObject obj = QJsonDocument::fromJson( body ).object();
						 callback( obj.value( "calendar" ).toObject(), obj.value( "event" ).toObject(), QString() );
					 } );
		} );
}

void CalendarClient::patchEvent( const QString &calendarId,
								 const QString &eventId,
								 const QString &etag,
								 const QJsonObject &patchBody,
								 std::function<void( QJsonObject, QString, QString )> callback )
{
	m_auth->accessToken(
		[this, calendarId, eventId, etag, patchBody, callback]( QString token, QString authError )
		{
			if ( token.isEmpty() )
			{
				callback( {}, QStringLiteral( "auth_failure" ), authError );
				return;
			}

			sendPatch(
				calendarId,
				eventId,
				etag,
				patchBody,
				token,
				[this, calendarId, eventId, etag, patchBody, callback]( QJsonObject event, QString code, QString message )
				{
					if ( code != QLatin1String( "delegation_required" ) || !m_delegatedAuth )
					{
						callback( event, code, message );
						return;
					}

					// Resume the same patch, as a delegated real user
					// instead of the service account, once (and
					// whenever, however long that takes — see
					// DelegatedAuth) a token becomes available.
					m_delegatedAuth->accessToken(
						[this, calendarId, eventId, etag, patchBody, callback]( QString delegatedToken, QString delegatedError )
						{
							if ( delegatedToken.isEmpty() )
							{
								callback( {}, QStringLiteral( "delegation_required" ), delegatedError );
								return;
							}
							sendPatch( calendarId, eventId, etag, patchBody, delegatedToken, callback );
						} );
				} );
		} );
}

void CalendarClient::sendPatch( const QString &calendarId,
								const QString &eventId,
								const QString &etag,
								const QJsonObject &patchBody,
								const QString &token,
								std::function<void( QJsonObject, QString, QString )> callback )
{
	// Explicit rather than relying on the API's own default: a PATCH that
	// touches attendees[] (invite/uninvite) must never trigger Google's
	// guest-notification emails.
	QUrl url = eventUrl( calendarId, eventId );
	QUrlQuery query;
	query.addQueryItem( "sendUpdates", "none" );
	url.setQuery( query );

	QNetworkRequest request( url );
	request.setRawHeader( "Authorization", "Bearer " + token.toUtf8() );
	request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );
	if ( !etag.isEmpty() )
		request.setRawHeader( "If-Match", etag.toUtf8() );

	QNetworkReply *reply =
		m_nam.sendCustomRequest( request, "PATCH", QJsonDocument( patchBody ).toJson( QJsonDocument::Compact ) );
	connect( reply,
			 &QNetworkReply::finished,
			 this,
			 [reply, callback]()
			 {
				 reply->deleteLater();
				 const QByteArray body = reply->readAll();
				 if ( reply->error() != QNetworkReply::NoError )
				 {
					 const auto [code, message] = classifyFailure( reply, body );
					 callback( {}, code, message );
					 return;
				 }
				 callback( QJsonDocument::fromJson( body ).object(), QString(), QString() );
			 } );
}

void CalendarClient::insertEvent( const QString &calendarId,
								  const QJsonObject &eventBody,
								  std::function<void( QJsonObject, QString, QString )> callback )
{
	m_auth->accessToken(
		[this, calendarId, eventBody, callback]( QString token, QString authError )
		{
			if ( token.isEmpty() )
			{
				callback( {}, QStringLiteral( "auth_failure" ), authError );
				return;
			}

			QNetworkRequest request( eventUrl( calendarId ) );
			request.setRawHeader( "Authorization", "Bearer " + token.toUtf8() );
			request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );

			QNetworkReply *reply = m_nam.post( request, QJsonDocument( eventBody ).toJson( QJsonDocument::Compact ) );
			connect( reply,
					 &QNetworkReply::finished,
					 this,
					 [reply, callback]()
					 {
						 reply->deleteLater();
						 const QByteArray body = reply->readAll();
						 if ( reply->error() != QNetworkReply::NoError )
						 {
							 const auto [code, message] = classifyFailure( reply, body );
							 callback( {}, code, message );
							 return;
						 }
						 callback( QJsonDocument::fromJson( body ).object(), QString(), QString() );
					 } );
		} );
}

void CalendarClient::deleteEvent( const QString &calendarId,
								  const QString &eventId,
								  const QString &etag,
								  std::function<void( QString, QString )> callback )
{
	m_auth->accessToken(
		[this, calendarId, eventId, etag, callback]( QString token, QString authError )
		{
			if ( token.isEmpty() )
			{
				callback( QStringLiteral( "auth_failure" ), authError );
				return;
			}

			QNetworkRequest request( eventUrl( calendarId, eventId ) );
			request.setRawHeader( "Authorization", "Bearer " + token.toUtf8() );
			if ( !etag.isEmpty() )
				request.setRawHeader( "If-Match", etag.toUtf8() );

			QNetworkReply *reply = m_nam.deleteResource( request );
			connect( reply,
					 &QNetworkReply::finished,
					 this,
					 [reply, callback]()
					 {
						 reply->deleteLater();
						 const QByteArray body = reply->readAll();
						 if ( reply->error() != QNetworkReply::NoError )
						 {
							 const auto [code, message] = classifyFailure( reply, body );
							 callback( code, message );
							 return;
						 }
						 callback( QString(), QString() );
					 } );
		} );
}
