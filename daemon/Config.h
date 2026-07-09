#pragma once

#include <QString>
#include <QVector>

// One calendar to poll. `color` (hex or a Google color name, resolved at
// startup — see GoogleColorNames.h) is NOT a person's color: it's the
// calendar's own fallback color, used for an event that has no per-event
// color override, so it still visually symbolizes which calendar it came
// from even when no configured person matches it.
struct CalendarConfig
{
	QString calendarId;
	QString color;
};

// One column/person in the day-grid. `color` (hex or a Google color name)
// is matched against each event's *resolved* effective color (its own
// per-event color if set, else its calendar's fallback color above) to
// decide which person's column an event belongs to — not by which calendar
// it was fetched from.
struct PersonConfig
{
	QString person;
	QString color;
};

struct Config
{
	QString serviceAccountKeyPath;
	int pollIntervalSeconds = 120;
	QString socketPath;
	QString snapshotPath;
	QVector<CalendarConfig> calendars;
	QVector<PersonConfig> people;

	// Reads and validates a config JSON file at `path`. On failure, returns
	// false and fills `error` with a message suitable for logging as-is.
	static bool load( const QString &path, Config &out, QString &error );
};
