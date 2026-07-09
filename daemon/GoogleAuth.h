#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <functional>

#include <QNetworkAccessManager>

// Authenticates as a Google Cloud service account: signs a short-lived JWT
// with the service account's private key (RS256/OpenSSL) and exchanges it
// for a bearer access token, caching the token until shortly before it
// expires. This is the only place in the daemon that touches the service
// account's private key.
class GoogleAuth : public QObject {
    Q_OBJECT
public:
    explicit GoogleAuth(const QString &serviceAccountKeyPath, QObject *parent = nullptr);

    // Loads and parses the service account key file. Must succeed before
    // accessToken() is called. Returns false and fills `error` on failure
    // (missing/unreadable file, malformed JSON, unparsable private key).
    bool init(QString &error);

    // Invokes `callback` with a valid bearer token, refreshing/re-signing
    // as needed. On failure, token is empty and error is set.
    void accessToken(std::function<void(QString token, QString error)> callback);

private:
    QString buildSignedJwt(QString &error) const;
    void requestToken(const QString &jwt, std::function<void(QString, QString)> callback);

    QString m_keyPath;
    QString m_clientEmail;
    QByteArray m_privateKeyPem;

    QString m_cachedToken;
    qint64 m_cachedTokenExpiry = 0; // epoch seconds

    QNetworkAccessManager m_nam;
};
