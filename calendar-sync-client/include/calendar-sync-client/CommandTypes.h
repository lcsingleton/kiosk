#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

// Intent-named commands the kiosk app sends over the command socket — never
// CRUD verbs. New actions are added by extending this list plus one new
// handler in CommandServer, not by changing the envelope shape.
namespace CommandAction
{
constexpr auto ScheduleEvent = "ScheduleEvent";
constexpr auto RescheduleEvent = "RescheduleEvent";
constexpr auto CancelEvent = "CancelEvent";
constexpr auto RenameEvent = "RenameEvent";
constexpr auto ChangeEventLocation = "ChangeEventLocation";
constexpr auto InviteParticipant = "InviteParticipant";
constexpr auto UninviteParticipant = "UninviteParticipant";
} // namespace CommandAction

// The "event" value on an unsolicited notification line — see
// AuthorizationPendingEvent below.
namespace CommandEvent
{
constexpr auto AuthorizationPending = "AuthorizationPending";
} // namespace CommandEvent

// One line of NDJSON from the kiosk app:
// {"commandId":"...","action":"RescheduleEvent","calendarId":"...","eventId":"...","etag":"...","payload":{...}}
// calendarId/eventId/etag are empty for ScheduleEvent (there's no existing
// event yet); everything else populates them. payload's shape depends on
// action — decode it with the matching *Payload struct below rather than
// pulling fields out by key.
struct Command
{
	QString commandId;
	QString action;
	QString calendarId;
	QString eventId;
	QString etag;
	QJsonObject payload;

	static Command fromJson( const QJsonObject &obj );
};

// CancelEvent carries no payload: calendarId/eventId/etag on Command fully
// identify it.
struct ScheduleEventPayload
{
	QString summary;
	QString start;
	QString end;
	QString description; // optional; empty means absent
	// Configured person names (e.g. "Mum"), same boundary as
	// ParticipantPayload::person below — resolved to email(s) daemon-side.
	QStringList attendees;

	static ScheduleEventPayload fromJson( const QJsonObject &obj );
	QJsonObject toJson() const;
};

struct RescheduleEventPayload
{
	QString newStart;
	QString newEnd;

	static RescheduleEventPayload fromJson( const QJsonObject &obj );
	QJsonObject toJson() const;
};

struct RenameEventPayload
{
	QString newSummary;

	static RenameEventPayload fromJson( const QJsonObject &obj );
	QJsonObject toJson() const;
};

struct ChangeEventLocationPayload
{
	QString newLocation;

	static ChangeEventLocationPayload fromJson( const QJsonObject &obj );
	QJsonObject toJson() const;
};

// Shared by InviteParticipant/UninviteParticipant: `person` is a configured
// person's name (e.g. "Mum"), not a raw email address — the daemon resolves
// which address(es) that refers to itself, same boundary as
// CalendarBridge's `people` (name/color only, no emails reach the UI).
struct ParticipantPayload
{
	QString person;

	static ParticipantPayload fromJson( const QJsonObject &obj );
	QJsonObject toJson() const;
};

// One line of NDJSON back to the kiosk app, matched to its Command by
// commandId. errorCode is one of a closed set the UI can key off:
// "conflict", "not_found", "auth_failure", "upstream_unavailable",
// "invalid_request". Empty errorCode means success.
struct Result
{
	QString commandId;
	QString errorCode;
	QString errorMessage;

	bool ok() const
	{
		return errorCode.isEmpty();
	}
	static Result success( const QString &commandId );
	static Result failure( const QString &commandId, const QString &code, const QString &message );
	QJsonObject toJson() const;
};

// An unsolicited NDJSON line the daemon can push to every connected kiosk
// app, not tied to any commandId — distinguished on the wire by having
// "event" instead of "commandId". Only one kind exists today: the daemon
// fell back to the delegated-user OAuth device flow (see DelegatedAuth) to
// satisfy an invite/uninvite the service account alone can't do, and a
// human needs to complete that grant out of band before the pending
// command can finish. The kiosk app has no keyboard-friendly way to
// complete a Google sign-in itself, so it just has to display
// verificationUrl + userCode long enough for someone to do it on a phone.
struct AuthorizationPendingEvent
{
	QString verificationUrl;
	QString userCode;
	int expiresInSecs = 0;

	QJsonObject toJson() const;
};
