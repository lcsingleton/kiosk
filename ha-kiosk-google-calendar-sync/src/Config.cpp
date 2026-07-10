#include "Config.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool Config::load( const QString &path, Config &out, QString &error )
{
	QFile file( path );
	if ( !file.open( QIODevice::ReadOnly ) )
	{
		error = QStringLiteral( "cannot open config file %1: %2" ).arg( path, file.errorString() );
		return false;
	}

	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson( file.readAll(), &parseError );
	if ( parseError.error != QJsonParseError::NoError )
	{
		error = QStringLiteral( "invalid JSON in %1: %2" ).arg( path, parseError.errorString() );
		return false;
	}
	const QJsonObject root = doc.object();

	out.serviceAccountKeyPath = root.value( "serviceAccountKeyPath" ).toString();
	out.pollIntervalSeconds = root.value( "pollIntervalSeconds" ).toInt( 120 );
	out.socketPath = root.value( "socketPath" ).toString();
	out.snapshotPath = root.value( "snapshotPath" ).toString();

	out.calendars.clear();
	for ( const QJsonValue &v : root.value( "calendars" ).toArray() )
	{
		const QJsonObject c = v.toObject();
		CalendarConfig cal;
		cal.calendarId = c.value( "calendarId" ).toString();
		cal.color = c.value( "color" ).toString();
		if ( cal.calendarId.isEmpty() )
		{
			error =
				QStringLiteral( "config %1: every entry in \"calendars\" needs \"calendarId\"" ).arg( path );
			return false;
		}
		out.calendars.append( cal );
	}

	out.people.clear();
	for ( const QJsonValue &v : root.value( "people" ).toArray() )
	{
		const QJsonObject p = v.toObject();
		PersonConfig person;
		person.person = p.value( "person" ).toString();
		person.color = p.value( "color" ).toString();
		for ( const QJsonValue &e : p.value( "emails" ).toArray() )
		{
			const QString email = e.toString();
			if ( !email.isEmpty() )
				person.emails.append( email );
		}
		if ( person.person.isEmpty() || person.color.isEmpty() || person.emails.isEmpty() )
		{
			error = QStringLiteral(
						"config %1: every entry in \"people\" needs \"person\", \"color\", and a non-empty \"emails\"" )
						.arg( path );
			return false;
		}
		out.people.append( person );
	}

	if ( out.serviceAccountKeyPath.isEmpty() )
	{
		error = QStringLiteral( "config %1: missing \"serviceAccountKeyPath\"" ).arg( path );
		return false;
	}
	if ( !QFileInfo::exists( out.serviceAccountKeyPath ) )
	{
		error = QStringLiteral( "service account key not found at %1" ).arg( out.serviceAccountKeyPath );
		return false;
	}
	if ( out.snapshotPath.isEmpty() )
	{
		error = QStringLiteral( "config %1: missing \"snapshotPath\"" ).arg( path );
		return false;
	}
	if ( out.calendars.isEmpty() )
	{
		error = QStringLiteral( "config %1: \"calendars\" is empty — nothing to sync" ).arg( path );
		return false;
	}

	return true;
}
