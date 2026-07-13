#include "calendar-sync-client/CommandTypes.h"

#include <QJsonArray>

Command Command::fromJson( const QJsonObject &obj )
{
	Command c;
	c.commandId = obj.value( "commandId" ).toString();
	c.action = obj.value( "action" ).toString();
	c.calendarId = obj.value( "calendarId" ).toString();
	c.eventId = obj.value( "eventId" ).toString();
	c.etag = obj.value( "etag" ).toString();
	c.payload = obj.value( "payload" ).toObject();
	return c;
}

Result Result::success( const QString &commandId )
{
	Result r;
	r.commandId = commandId;
	return r;
}

Result Result::failure( const QString &commandId, const QString &code, const QString &message )
{
	Result r;
	r.commandId = commandId;
	r.errorCode = code;
	r.errorMessage = message;
	return r;
}

QJsonObject Result::toJson() const
{
	QJsonObject obj;
	obj["commandId"] = commandId;
	if ( ok() )
	{
		obj["status"] = "ok";
	}
	else
	{
		obj["status"] = "error";
		QJsonObject err;
		err["code"] = errorCode;
		err["message"] = errorMessage;
		obj["error"] = err;
	}
	return obj;
}

ScheduleEventPayload ScheduleEventPayload::fromJson( const QJsonObject &obj )
{
	ScheduleEventPayload p;
	p.summary = obj.value( "summary" ).toString();
	p.start = obj.value( "start" ).toString();
	p.end = obj.value( "end" ).toString();
	p.description = obj.value( "description" ).toString();
	for ( const QJsonValue &v : obj.value( "attendees" ).toArray() )
		p.attendees.append( v.toString() );
	return p;
}

QJsonObject ScheduleEventPayload::toJson() const
{
	QJsonObject obj{ { "summary", summary }, { "start", start }, { "end", end } };
	// Omitted rather than sent as an empty string/array — fromJson() above
	// treats a missing key the same as an empty one, so this only trims the
	// wire payload and doesn't create a distinct "absent" state.
	if ( !description.isEmpty() )
		obj["description"] = description;
	if ( !attendees.isEmpty() )
		obj["attendees"] = QJsonArray::fromStringList( attendees );
	return obj;
}

RescheduleEventPayload RescheduleEventPayload::fromJson( const QJsonObject &obj )
{
	RescheduleEventPayload p;
	p.newStart = obj.value( "newStart" ).toString();
	p.newEnd = obj.value( "newEnd" ).toString();
	return p;
}

QJsonObject RescheduleEventPayload::toJson() const
{
	return QJsonObject{ { "newStart", newStart }, { "newEnd", newEnd } };
}

RenameEventPayload RenameEventPayload::fromJson( const QJsonObject &obj )
{
	RenameEventPayload p;
	p.newSummary = obj.value( "newSummary" ).toString();
	return p;
}

QJsonObject RenameEventPayload::toJson() const
{
	return QJsonObject{ { "newSummary", newSummary } };
}

ChangeEventLocationPayload ChangeEventLocationPayload::fromJson( const QJsonObject &obj )
{
	ChangeEventLocationPayload p;
	p.newLocation = obj.value( "newLocation" ).toString();
	return p;
}

QJsonObject ChangeEventLocationPayload::toJson() const
{
	return QJsonObject{ { "newLocation", newLocation } };
}

ParticipantPayload ParticipantPayload::fromJson( const QJsonObject &obj )
{
	ParticipantPayload p;
	p.person = obj.value( "person" ).toString();
	return p;
}

QJsonObject ParticipantPayload::toJson() const
{
	return QJsonObject{ { "person", person } };
}

QJsonObject AuthorizationPendingEvent::toJson() const
{
	// The "event" key (rather than "commandId") is what lets
	// CalendarSyncClient::handleResultLine tell this apart from a Result
	// reply on the shared line stream.
	return QJsonObject{ { "event", CommandEvent::AuthorizationPending },
						{ "verificationUrl", verificationUrl },
						{ "userCode", userCode },
						{ "expiresInSecs", expiresInSecs } };
}
