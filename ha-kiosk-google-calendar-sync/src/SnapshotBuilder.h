#pragma once

#include <QDate>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

#include "Config.h"

// Normalizes raw Calendar API v3 event JSON into the JSON shape
// app/DashboardData.qml expects: todayHighlights, todaySchedule, weekend,
// upcoming, people. Field names match DashboardData.qml's mock data
// exactly; eventId/calendarId/etag/startIso/endIso/attendees are additive
// (existing QML bindings ignore fields they don't reference).
//
// Classification is by *who the event is tagged to*, not which calendar it
// came from or what color it's been given: each of an event's
// attendees[].email is matched against each configured person's email; if
// none match (most events have no attendees at all), the event's own
// creator.email is tried instead. An event matching zero configured people
// still appears in weekend/upcoming/todayHighlights (tagged with a fallback
// accent — its own colorId if set, else its calendar's configured fallback
// color — still visually symbolizing its origin) but is left out of
// todaySchedule's per-person day-grid, which has no column for an
// unmatched person. An event matching more than one configured person gets
// one todaySchedule row per matched person (so it shows in every matched
// column), but weekend/upcoming only carry one accent field, so those use
// the first match.
class SnapshotBuilder
{
  public:
	// eventColorIdToHex: the live Calendar API "event" palette (numeric id
	// -> hex), used to resolve an event's own colorId when it has one, as
	// the fallback accent for an event matching no configured person.
	// calendarFallbackHex: calendarId -> that calendar's configured
	// fallback color (already resolved to hex), used the same way when an
	// event has no colorId of its own.
	SnapshotBuilder( const QVector<PersonConfig> &people, const QHash<QString, QString> &eventColorIdToHex,
					 const QHash<QString, QString> &calendarFallbackHex );

	// Classifies and appends every (non-cancelled) event in `events` —
	// already filtered by CalendarClient to the [today, +N days) window —
	// into today/weekend/upcoming buckets based on its own date and
	// matched people.
	void addEvents( const QString &calendarId, const QJsonArray &events );

	QJsonObject build() const;

  private:
	struct PersonMatch
	{
		QString person;
		QString color;
	};

	void classify( const QString &calendarId, const QJsonObject &event );
	// Attendees first, then (only if none of them matched) the event's
	// creator — never both, so a real invited guest on someone's personal
	// event doesn't also pull in the creator as a second match.
	QVector<PersonMatch> resolveAttendedPeople( const QJsonObject &event ) const;
	QString resolveFallbackAccent( const QString &calendarId, const QJsonObject &event ) const;
	// One entry per configured person — { name, color, invited } — driving
	// the invite/uninvite toggle badges in the UI. Unlike
	// resolveAttendedPeople, this never falls back to the event's creator:
	// it's a literal read of event.attendees[].email, since toggling a
	// badge "off" is meant to actually remove that person from the guest
	// list, not just unmatch a fallback.
	QJsonArray attendeeStatus( const QJsonObject &event ) const;

	QDate m_today;
	QDate m_weekendSaturday;
	QDate m_weekendSunday;
	QVector<PersonConfig> m_people;
	QHash<QString, QString> m_eventColorIdToHex;
	QHash<QString, QString> m_calendarFallbackHex;

	QJsonArray m_todayHighlights;
	QJsonArray m_todaySchedule;
	QJsonArray m_weekend;
	QJsonArray m_upcoming;
};
