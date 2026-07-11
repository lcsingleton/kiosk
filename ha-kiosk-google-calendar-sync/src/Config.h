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

// One column/person in the day-grid. `emails` is matched against an
// event's attendees[].email (falling back to its creator.email when no
// attendee matches any configured person) to decide whose column an event
// belongs to — see SnapshotBuilder::resolveAttendedPeople. A person can
// list more than one address here: e.g. an alias tag added as a guest to
// mark them on a multi-person event, alongside the real Google account
// they actually create events from (matched via creator.email when they
// made the event themselves and nobody tagged anyone specific). `color`
// (hex or a Google color name) is purely that person's display accent; it
// plays no part in identifying whose event this is.
struct PersonConfig
{
	QString person;
	QString color;
	QVector<QString> emails;
};

struct Config
{
	QString serviceAccountKeyPath;
	int pollIntervalSeconds = 120;
	QString socketPath;
	QString snapshotPath;
	QVector<CalendarConfig> calendars;
	QVector<PersonConfig> people;

	// Optional OAuth "TV and limited input device" client, used only as
	// CalendarClient's fallback for inviting attendees when the service
	// account hits Google's domain-wide-delegation wall (see
	// DelegatedAuth). Leave all three empty to disable that fallback
	// entirely — invite/uninvite will then just fail with
	// "delegation_required" on a bare service account, same as before this
	// existed.
	QString oauthClientId;
	QString oauthClientSecret;
	QString userTokenPath;

	// Reads and validates a config JSON file at `path`. On failure, returns
	// false and fills `error` with a message suitable for logging as-is.
	static bool load( const QString &path, Config &out, QString &error );
};
