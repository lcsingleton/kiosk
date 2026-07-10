#include "calendar-sync-client/CommandTypes.h"

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
	return p;
}

QJsonObject ScheduleEventPayload::toJson() const
{
	QJsonObject obj{ { "summary", summary }, { "start", start }, { "end", end } };
	if ( !description.isEmpty() )
		obj["description"] = description;
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
