#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <functional>

/// Authenticates as a real Google user via the OAuth2 device authorization
/// grant (RFC 8628) instead of a service account — no browser redirect or
/// embedded webview needed, which matters since the daemon is headless. This
/// exists only as CalendarClient's fallback for the one thing a bare service
/// account can never do: invite attendees without domain-wide delegation
/// (see CalendarClient's "delegation_required" error code).
///
/// First call ever (no refresh token on disk yet): emits authorizationPending
/// with a URL/code a human completes out of band (phone, laptop) — the
/// accessToken() callback isn't invoked until that finishes or the grant
/// expires. Every later call is fast: a cached token, or a silent
/// refresh_token exchange, with no human involved.
class DelegatedAuth : public QObject
{
	Q_OBJECT
  public:
	/// `tokenStorePath` holds just the refresh token (mode 0600), separate
	/// from the service account key — losing it just means repeating the
	/// one-time device-code grant, not a security incident on its own.
	/// @param clientId OAuth client ID for the device-code grant.
	/// @param clientSecret OAuth client secret for the device-code grant.
	/// @param tokenStorePath See above.
	/// @param parent Standard QObject ownership parent.
	DelegatedAuth( const QString &clientId,
				   const QString &clientSecret,
				   const QString &tokenStorePath,
				   QObject *parent = nullptr );

	/// Invokes `callback` with a valid bearer token for a real Google
	/// account. On failure (device code expired, grant revoked and no
	/// network to re-request one, etc.), token is empty and error is set.
	void accessToken( std::function<void( QString token, QString error )> callback );

  signals:
	/// Fired once per fresh device-code grant (i.e. only when no usable
	/// refresh token is on disk yet). Callers should log/surface
	/// verificationUrl + userCode somewhere a human will see it before
	/// expiresInSecs elapses.
	void authorizationPending( QString verificationUrl, QString userCode, int expiresInSecs );

  private:
	void requestDeviceCode( std::function<void( QString, QString )> callback );
	void pollForToken( const QString &deviceCode,
					   int intervalSecs,
					   qint64 deadlineEpoch,
					   std::function<void( QString, QString )> callback );
	void refreshAccessToken( std::function<void( QString, QString )> callback );
	void handleTokenResponse( const QJsonObject &obj, std::function<void( QString, QString )> callback );

	QString loadRefreshToken() const;
	void persistRefreshToken( const QString &refreshToken ) const;

	QString m_clientId;
	QString m_clientSecret;
	QString m_tokenStorePath;

	QString m_cachedToken;
	qint64 m_cachedTokenExpiry = 0; // epoch seconds

	QNetworkAccessManager m_nam;
};
