#include "SnapshotBuilder.h"

#include <QDateTime>
#include <QJsonValue>

namespace
{
// Used when an event's resolved effective color is empty (no colorId, and
// its calendar has no configured fallback either) — matches the original
// mock data's "colorOther" for family-wide/unattributed items.
constexpr auto kNeutralAccent = "#9085e9";
} // namespace

SnapshotBuilder::SnapshotBuilder( const QVector<PersonConfig> &people,
								  const QHash<QString, QString> &eventColorIdToHex,
								  const QHash<QString, QString> &calendarFallbackHex )
	: m_people( people ), m_eventColorIdToHex( eventColorIdToHex ),
	  m_calendarFallbackHex( calendarFallbackHex )
{
	m_today = QDate::currentDate();
	// ISO day-of-week: Monday=1 .. Sunday=7. Saturday=6. If today IS the
	// weekend, "this weekend" means today's remaining half; otherwise it's
	// the upcoming Saturday/Sunday.
	const int dow = m_today.dayOfWeek();
	const int daysUntilSaturday = ( 6 - dow + 7 ) % 7;
	m_weekendSaturday = m_today.addDays( daysUntilSaturday );
	m_weekendSunday = m_weekendSaturday.addDays( 1 );
}

void SnapshotBuilder::addEvents( const QString &calendarId, const QJsonArray &events )
{
	for ( const QJsonValue &v : events )
	{
		const QJsonObject event = v.toObject();
		if ( event.value( "status" ).toString() == "cancelled" )
			continue;
		classify( calendarId, event );
	}
}

namespace
{
bool personHasEmail( const PersonConfig &p, const QString &email )
{
	for ( const QString &candidate : p.emails )
	{
		if ( candidate.compare( email, Qt::CaseInsensitive ) == 0 )
			return true;
	}
	return false;
}
} // namespace

QVector<SnapshotBuilder::PersonMatch> SnapshotBuilder::resolveAttendedPeople( const QJsonObject &event ) const
{
	QVector<PersonMatch> matches;

	for ( const QJsonValue &v : event.value( "attendees" ).toArray() )
	{
		const QString attendeeEmail = v.toObject().value( "email" ).toString();
		if ( attendeeEmail.isEmpty() )
			continue;
		for ( const PersonConfig &p : m_people )
		{
			if ( personHasEmail( p, attendeeEmail ) )
			{
				matches.append( { p.person, p.color } );
				break;
			}
		}
	}
	if ( !matches.isEmpty() )
		return matches;

	// Nobody was explicitly tagged as a guest — fall back to whoever
	// created the event (e.g. a parent's own Google account), so their
	// events still land in their own column by default.
	const QString creatorEmail = event.value( "creator" ).toObject().value( "email" ).toString();
	if ( creatorEmail.isEmpty() )
		return matches;
	for ( const PersonConfig &p : m_people )
	{
		if ( personHasEmail( p, creatorEmail ) )
		{
			matches.append( { p.person, p.color } );
			break;
		}
	}
	return matches;
}

QString SnapshotBuilder::resolveFallbackAccent( const QString &calendarId, const QJsonObject &event ) const
{
	const QString colorId = event.value( "colorId" ).toString();
	QString effectiveHex;
	if ( !colorId.isEmpty() && m_eventColorIdToHex.contains( colorId ) )
		effectiveHex = m_eventColorIdToHex.value( colorId );
	else
		effectiveHex = m_calendarFallbackHex.value( calendarId );
	return effectiveHex.isEmpty() ? QLatin1String( kNeutralAccent ) : effectiveHex;
}

void SnapshotBuilder::classify( const QString &calendarId, const QJsonObject &event )
{
	const QJsonObject start = event.value( "start" ).toObject();
	const bool allDay = !start.contains( "dateTime" );

	QDate date;
	QDateTime startDt, endDt;
	if ( allDay )
	{
		date = QDate::fromString( start.value( "date" ).toString(), Qt::ISODate );
	}
	else
	{
		startDt = QDateTime::fromString( start.value( "dateTime" ).toString(), Qt::ISODate );
		endDt = QDateTime::fromString( event.value( "end" ).toObject().value( "dateTime" ).toString(),
									   Qt::ISODate );
		date = startDt.date();
	}
	if ( !date.isValid() )
		return;

	const QString summary = event.value( "summary" ).toString( QStringLiteral( "(untitled)" ) );
	const QString eventId = event.value( "id" ).toString();
	const QString etag = event.value( "etag" ).toString();
	// Wall-clock time as Google reported it for this event (its dateTime
	// carries the event's own UTC offset baked in) — not converted to any
	// other zone, so it matches what the event's own calendar shows.
	const QString timeLabel = allDay ? QString() : startDt.toString( "h:mm AP" );
	// ISO8601 round-trip of the same dateTime (same offset preserved) — the
	// edit UI sends these straight back as RescheduleEvent's newStart/newEnd
	// without ever reconstructing a date from the decimal hour + day label.
	const QString startIso = allDay ? QString() : startDt.toString( Qt::ISODate );
	const QString endIso = allDay ? QString() : endDt.toString( Qt::ISODate );

	const QVector<PersonMatch> matches = resolveAttendedPeople( event );
	// Same accent a matched event's own person(s) would get is never used
	// here — this is only reached for weekend/upcoming (a fallback, so any
	// match already short-circuits it below) or an unmatched event.
	const QString fallbackAccent = resolveFallbackAccent( calendarId, event );

	if ( date == m_today )
	{
		if ( allDay )
		{
			// All-day/family-wide items aren't attributed to one person's
			// column regardless of match — same as the original mock.
			QJsonObject o;
			o["icon"] = QStringLiteral( "📌" );
			o["label"] = summary;
			o["time"] = QString();
			m_todayHighlights.append( o );
		}
		else
		{
			// No column exists for an unmatched person — a timed event
			// today matching nobody is left out of the day-grid entirely
			// rather than guessing which column to put it in. An event
			// matching several people gets one row per match, so it shows
			// in every one of their columns.
			const double startHour = startDt.time().hour() + startDt.time().minute() / 60.0;
			const double endHour = endDt.time().hour() + endDt.time().minute() / 60.0;
			for ( const PersonMatch &m : matches )
			{
				QJsonObject o;
				o["person"] = m.person;
				o["color"] = m.color;
				o["event"] = summary;
				o["start"] = startHour;
				o["duration"] = qMax( 0.0, endHour - startHour );
				o["eventId"] = eventId;
				o["calendarId"] = calendarId;
				o["etag"] = etag;
				o["startIso"] = startIso;
				o["endIso"] = endIso;
				m_todaySchedule.append( o );
			}
		}
		return;
	}

	// weekend/upcoming only carry one accent field — an event matching
	// several people shows the first match's color rather than picking a
	// winner some other way.
	const QString accent = matches.isEmpty() ? fallbackAccent : matches.first().color;

	if ( date == m_weekendSaturday || date == m_weekendSunday )
	{
		QJsonObject o;
		o["day"] = date == m_weekendSaturday ? QStringLiteral( "Saturday" ) : QStringLiteral( "Sunday" );
		o["date"] = date.toString( "MMM d" );
		o["title"] = summary;
		o["time"] = timeLabel;
		o["accent"] = accent;
		o["eventId"] = eventId;
		o["calendarId"] = calendarId;
		o["etag"] = etag;
		o["startIso"] = startIso;
		o["endIso"] = endIso;
		m_weekend.append( o );
		return;
	}

	if ( date > m_today )
	{
		QJsonObject o;
		o["month"] = date.toString( "MMM" ).toUpper();
		o["day"] = date.toString( "dd" );
		o["title"] = summary;
		o["time"] = timeLabel;
		o["accent"] = accent;
		o["eventId"] = eventId;
		o["calendarId"] = calendarId;
		o["etag"] = etag;
		o["startIso"] = startIso;
		o["endIso"] = endIso;
		m_upcoming.append( o );
	}
	// date < today: outside the requested fetch window in practice, ignore.
}

QJsonObject SnapshotBuilder::build() const
{
	QJsonArray people;
	for ( const PersonConfig &p : m_people )
	{
		QJsonObject o;
		o["name"] = p.person;
		o["color"] = p.color;
		people.append( o );
	}

	QJsonObject root;
	root["generatedAt"] = QDateTime::currentDateTimeUtc().toString( Qt::ISODate );
	root["people"] = people;
	root["todayHighlights"] = m_todayHighlights;
	root["todaySchedule"] = m_todaySchedule;
	root["weekend"] = m_weekend;
	root["upcoming"] = m_upcoming;
	return root;
}
