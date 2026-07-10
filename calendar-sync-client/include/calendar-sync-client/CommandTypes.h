#pragma once

#include <QJsonObject>
#include <QString>

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
} // namespace CommandAction

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
