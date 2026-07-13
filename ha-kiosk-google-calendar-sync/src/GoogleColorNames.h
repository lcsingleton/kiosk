#pragma once

#include <QHash>
#include <QString>

/// Google Calendar's color *names* (Banana, Peacock, Beetroot, ...) are a UI
/// labeling convention only — the Calendar API's colors endpoint
/// (GET /calendar/v3/colors) returns the authoritative numeric-id -> hex
/// mapping but never a name. These tables are this daemon's best-effort
/// record of Google's own id -> name convention, used only to let daemon
/// config files say "Banana" instead of a numeric id or a memorized hex.
namespace GoogleColorNames
{

/// The EVENT table (11 colors) is the one actually assignable per-event via
/// Google Calendar's "change event color" picker, and is used with
/// confidence.
inline const QHash<QString, QString> &eventColorIds()
{
	static const QHash<QString, QString> table{
		{ "lavender", "1" },
		{ "sage", "2" },
		{ "grape", "3" },
		{ "flamingo", "4" },
		{ "banana", "5" },
		{ "tangerine", "6" },
		{ "peacock", "7" },
		{ "graphite", "8" },
		{ "blueberry", "9" },
		{ "basil", "10" },
		{ "tomato", "11" },
	};
	return table;
}

/// The CALENDAR table (24 colors, used for a whole calendar's own
/// default/list color) is reproduced from public documentation of Google's
/// convention rather than verified against a live account — if a resolved
/// hex doesn't match what you see in Google Calendar's UI for a given name,
/// override it in daemon-config.json with a literal hex instead (any config
/// color already accepts a plain "#rrggbb" value).
inline const QHash<QString, QString> &calendarColorIds()
{
	static const QHash<QString, QString> table{
		{ "cocoa", "1" },	  { "flamingo", "2" },	 { "tomato", "3" },		{ "tangerine", "4" },	   { "pumpkin", "5" },
		{ "mustard", "6" },	  { "eucalyptus", "7" }, { "pistachio", "8" },	{ "avocado", "9" },		   { "cypress", "10" },
		{ "peacock", "11" },  { "cobalt", "12" },	 { "blueberry", "13" }, { "lavender", "14" },	   { "wisteria", "15" },
		{ "graphite", "16" }, { "birch", "17" },	 { "radicchio", "18" }, { "cherryblossom", "19" }, { "grape", "20" },
		{ "amethyst", "21" }, { "beetroot", "22" },	 { "rosewood", "23" },
	};
	return table;
}

} // namespace GoogleColorNames
