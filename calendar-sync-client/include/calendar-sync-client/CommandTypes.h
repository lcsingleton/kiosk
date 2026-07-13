#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

/// Intent-named commands the kiosk app sends over the command socket — never
/// CRUD verbs. New actions are added by extending this list plus one new
/// handler in CommandServer, not by changing the envelope shape.
namespace CommandAction
{
/// Creates a new event — see CalendarSyncClient::scheduleEvent and ScheduleEventPayload.
constexpr auto ScheduleEvent = "ScheduleEvent";
/// Changes an existing event's start/end — see CalendarSyncClient::rescheduleEvent and
/// RescheduleEventPayload.
constexpr auto RescheduleEvent = "RescheduleEvent";
/// Deletes an existing event — see CalendarSyncClient::cancelEvent. No payload.
constexpr auto CancelEvent = "CancelEvent";
/// Changes an existing event's summary/title — see CalendarSyncClient::renameEvent and RenameEventPayload.
constexpr auto RenameEvent = "RenameEvent";
/// Changes an existing event's location — see CalendarSyncClient::changeEventLocation and
/// ChangeEventLocationPayload.
constexpr auto ChangeEventLocation = "ChangeEventLocation";
/// Adds an attendee to an existing event — see CalendarSyncClient::inviteParticipant and
/// ParticipantPayload.
constexpr auto InviteParticipant = "InviteParticipant";
/// Removes an attendee from an existing event — see CalendarSyncClient::uninviteParticipant and
/// ParticipantPayload.
constexpr auto UninviteParticipant = "UninviteParticipant";
} // namespace CommandAction

/// The "event" value on an unsolicited notification line — see
/// AuthorizationPendingEvent below.
namespace CommandEvent
{
/// See AuthorizationPendingEvent.
constexpr auto AuthorizationPending = "AuthorizationPending";
} // namespace CommandEvent

/// One line of NDJSON from the kiosk app:
/// @code{.json}
/// {
///   "commandId": "c1",
///   "action": "RescheduleEvent",
///   "calendarId": "primary",
///   "eventId": "abc123",
///   "etag": "\"33t9\"",
///   "payload": { "newStart": "2024-06-14T14:00:00+10:00", "newEnd": "2024-06-14T15:00:00+10:00" }
/// }
/// @endcode
/// calendarId/eventId/etag are empty for ScheduleEvent (there's no existing
/// event yet); everything else populates them. payload's shape depends on
/// action — decode it with the matching *Payload struct below rather than
/// pulling fields out by key.
struct Command
{
	/// Caller-generated identifier correlating this command with its eventual Result.
	QString commandId;
	/// One of the CommandAction constants.
	QString action;
	/// Target calendar's id; empty for ScheduleEvent.
	QString calendarId;
	/// Target event's id; empty for ScheduleEvent.
	QString eventId;
	/// Concurrency guard for the target event; empty for ScheduleEvent.
	QString etag;
	/// Action-specific payload — decode with the matching *Payload struct's
	/// fromJson(), not by pulling fields out by key.
	QJsonObject payload;

	/// Parses one NDJSON command line's JSON object into a Command.
	static Command fromJson( const QJsonObject &obj );
};

// CancelEvent carries no payload: calendarId/eventId/etag on Command fully
// identify it.
/// Payload for the ScheduleEvent command — see CalendarSyncClient::scheduleEvent.
struct ScheduleEventPayload
{
	/// Event title.
	QString summary;
	/// ISO-8601 start date-time.
	QString start;
	/// ISO-8601 end date-time.
	QString end;
	QString description; ///< Optional; empty means absent.
	/// Configured person names (e.g. "Mum"), same boundary as
	/// ParticipantPayload::person below — resolved to email(s) daemon-side.
	QStringList attendees;

	/// Decodes a ScheduleEvent command's payload object.
	static ScheduleEventPayload fromJson( const QJsonObject &obj );
	/// Encodes this payload for a ScheduleEvent command.
	QJsonObject toJson() const;
};

/// Payload for the RescheduleEvent command — see CalendarSyncClient::rescheduleEvent.
struct RescheduleEventPayload
{
	/// New ISO-8601 start date-time.
	QString newStart;
	/// New ISO-8601 end date-time.
	QString newEnd;

	/// Decodes a RescheduleEvent command's payload object.
	static RescheduleEventPayload fromJson( const QJsonObject &obj );
	/// Encodes this payload for a RescheduleEvent command.
	QJsonObject toJson() const;
};

/// Payload for the RenameEvent command — see CalendarSyncClient::renameEvent.
struct RenameEventPayload
{
	/// New event title.
	QString newSummary;

	/// Decodes a RenameEvent command's payload object.
	static RenameEventPayload fromJson( const QJsonObject &obj );
	/// Encodes this payload for a RenameEvent command.
	QJsonObject toJson() const;
};

/// Payload for the ChangeEventLocation command — see CalendarSyncClient::changeEventLocation.
struct ChangeEventLocationPayload
{
	/// New event location string.
	QString newLocation;

	/// Decodes a ChangeEventLocation command's payload object.
	static ChangeEventLocationPayload fromJson( const QJsonObject &obj );
	/// Encodes this payload for a ChangeEventLocation command.
	QJsonObject toJson() const;
};

/// Shared by InviteParticipant/UninviteParticipant: `person` is a configured
/// person's name (e.g. "Mum"), not a raw email address — the daemon resolves
/// which address(es) that refers to itself, same boundary as
/// CalendarBridge's `people` (name/color only, no emails reach the UI).
struct ParticipantPayload
{
	/// Configured person's name (e.g. "Mum"); not a raw email address.
	QString person;

	/// Decodes an InviteParticipant/UninviteParticipant command's payload object.
	static ParticipantPayload fromJson( const QJsonObject &obj );
	/// Encodes this payload for an InviteParticipant/UninviteParticipant command.
	QJsonObject toJson() const;
};

/// One line of NDJSON back to the kiosk app, matched to its Command by
/// commandId. errorCode is one of a closed set the UI can key off:
/// "conflict", "not_found", "auth_failure", "upstream_unavailable",
/// "invalid_request". Empty errorCode means success.
struct Result
{
	/// Matches the originating Command::commandId.
	QString commandId;
	/// One of the closed set named in the struct doc above; empty means success.
	QString errorCode;
	/// Human-readable detail for errorCode; empty on success.
	QString errorMessage;

	/// True when errorCode is empty, i.e. the command succeeded.
	bool ok() const
	{
		return errorCode.isEmpty();
	}
	/// Builds a success Result for @p commandId.
	static Result success( const QString &commandId );
	/// Builds a failure Result for @p commandId with the given error @p code and @p message.
	static Result failure( const QString &commandId, const QString &code, const QString &message );
	/// Encodes this Result as the NDJSON reply object.
	QJsonObject toJson() const;
};

/// An unsolicited NDJSON line the daemon can push to every connected kiosk
/// app, not tied to any commandId — distinguished on the wire by having
/// "event" instead of "commandId". Only one kind exists today: the daemon
/// fell back to the delegated-user OAuth device flow (see DelegatedAuth) to
/// satisfy an invite/uninvite the service account alone can't do, and a
/// human needs to complete that grant out of band before the pending
/// command can finish. The kiosk app has no keyboard-friendly way to
/// complete a Google sign-in itself, so it just has to display
/// verificationUrl + userCode long enough for someone to do it on a phone.
struct AuthorizationPendingEvent
{
	/// URL the human needs to visit to complete the device-flow grant.
	QString verificationUrl;
	/// Code the human enters at verificationUrl.
	QString userCode;
	/// Seconds until this grant offer expires.
	int expiresInSecs = 0;

	/// Encodes this event as the unsolicited NDJSON push line.
	QJsonObject toJson() const;
};
