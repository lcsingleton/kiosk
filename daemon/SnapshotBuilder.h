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
// exactly; eventId/calendarId/etag/startIso/endIso are additive (existing
// QML bindings ignore fields they don't reference).
//
// Classification is by each event's *resolved effective color* (its own
// per-event colorId if set, else its owning calendar's configured fallback
// color) matched against each configured person's color — not by which
// calendar the event came from. An event whose resolved color matches no
// configured person still appears in weekend/upcoming/todayHighlights
// (tagged with its own resolved color as accent, still visually symbolizing
// its origin) but is left out of todaySchedule's per-person day-grid,
// which has no column for an unmatched person.
class SnapshotBuilder
{
  public:
	// eventColorIdToHex: the live Calendar API "event" palette (numeric id
	// -> hex), used to resolve an event's own colorId when it has one.
	// calendarFallbackHex: calendarId -> that calendar's configured
	// fallback color (already resolved to hex), used when an event has no
	// colorId of its own.
	SnapshotBuilder( const QVector<PersonConfig> &people, const QHash<QString, QString> &eventColorIdToHex,
					 const QHash<QString, QString> &calendarFallbackHex );

	// Classifies and appends every (non-cancelled) event in `events` —
	// already filtered by CalendarClient to the [today, +N days) window —
	// into today/weekend/upcoming buckets based on its own date and
	// resolved effective color.
	void addEvents( const QString &calendarId, const QJsonArray &events );

	QJsonObject build() const;

  private:
	void classify( const QString &calendarId, const QJsonObject &event );
	QString resolvePersonAndColor( const QString &calendarId, const QJsonObject &event,
								   QString &outPerson ) const;

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
