#include "CommandTypes.h"

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
