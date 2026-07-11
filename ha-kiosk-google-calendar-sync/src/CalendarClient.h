#pragma once

#include <QObject>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <functional>

class GoogleAuth;
class DelegatedAuth;

// Thin wrapper over the Calendar API v3 "events" resource. Knows nothing
// about JWTs/OAuth — it just asks GoogleAuth for a bearer token per call.
//
// Write methods (patchEvent/insertEvent/deleteEvent) report a closed set of
// error codes on failure: "conflict" (etag mismatch via If-Match — a 412
// from Google), "not_found", "auth_failure", "delegation_required" (see
// patchEvent), "upstream_unavailable", "invalid_request". errorCode is empty
// on success.
class CalendarClient : public QObject
{
	Q_OBJECT
  public:
	// `delegatedAuth` is optional (nullptr disables the fallback below) —
	// pass it when patchEvent should recover from a bare service account's
	// inability to invite attendees by retrying as a real delegated user.
	explicit CalendarClient( GoogleAuth *auth, DelegatedAuth *delegatedAuth = nullptr,
							 QObject *parent = nullptr );

	// Lists single (non-recurring-series) events in [timeMin, timeMax),
	// ordered by start time. On failure, events is empty and error is set.
	void listEvents( const QString &calendarId, const QDateTime &timeMin, const QDateTime &timeMax,
					 std::function<void( QJsonArray events, QString error )> callback );

	// Fetches one event by id — used by handlers (e.g. inviting a
	// participant) that need to read-modify-write a field Calendar API's
	// PATCH would otherwise replace wholesale, like attendees[].
	void
	getEvent( const QString &calendarId, const QString &eventId,
			  std::function<void( QJsonObject event, QString errorCode, QString errorMessage )> callback );

	// GET /calendar/v3/colors — the authoritative numeric-id -> hex mapping
	// for both the "event" (per-event override) and "calendar" (whole
	// calendar default) palettes. No auth-token-free caching here; callers
	// that need this once at startup should cache the result themselves.
	void fetchColorDefinitions(
		std::function<void( QJsonObject calendarColors, QJsonObject eventColors, QString error )> callback );

	// Partial update of an existing event, guarded by If-Match on `etag` so
	// a concurrent edit elsewhere is reported as a "conflict" rather than
	// silently overwritten.
	//
	// A patch touching attendees[] can come back "delegation_required" —
	// Google's hard rule that a bare service account can't invite attendees
	// without domain-wide delegation. When m_delegatedAuth is set, that
	// specific failure is retried once with a delegated-user token instead
	// of being reported to the caller; only a failure in *that* retry (or
	// having no delegatedAuth configured at all) reaches `callback`.
	void
	patchEvent( const QString &calendarId, const QString &eventId, const QString &etag,
				const QJsonObject &patchBody,
				std::function<void( QJsonObject event, QString errorCode, QString errorMessage )> callback );

	// Creates a new event. No etag involved — there's nothing to conflict with yet.
	void
	insertEvent( const QString &calendarId, const QJsonObject &eventBody,
				 std::function<void( QJsonObject event, QString errorCode, QString errorMessage )> callback );

	// Deletes an event, guarded by If-Match on `etag` the same way patchEvent is.
	void deleteEvent( const QString &calendarId, const QString &eventId, const QString &etag,
					  std::function<void( QString errorCode, QString errorMessage )> callback );

  private:
	// The actual PATCH send, factored out of patchEvent() so it can be
	// reissued with a different bearer token on the delegation_required
	// retry without repeating the request-building code.
	void sendPatch( const QString &calendarId, const QString &eventId, const QString &etag,
					const QJsonObject &patchBody, const QString &token,
					std::function<void( QJsonObject event, QString errorCode, QString errorMessage )> callback );

	GoogleAuth *m_auth;
	DelegatedAuth *m_delegatedAuth;
	QNetworkAccessManager m_nam;
};
