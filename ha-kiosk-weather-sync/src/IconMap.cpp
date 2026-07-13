#include "IconMap.h"

#include <QHash>

QString IconMap::emoji( const QString &iconDescriptor )
{
	// Table keys mix two BOM naming conventions: the daily-forecast endpoint
	// names its night icons bare (e.g. "clear" for a clear night), while the
	// hourly endpoint prefixes them explicitly (e.g. "night_clear") — both
	// spellings are listed so a descriptor from either endpoint resolves.
	static const QHash<QString, QString> table = {
		{ "sunny", "☀️" },
		{ "clear", "🌙" },
		{ "night_clear", "🌙" },
		{ "partly_cloudy", "⛅" },
		// No combined moon+cloud emoji is used for the night variant; it
		// collapses onto the same plain moon as the other night entries.
		{ "night_partly_cloudy", "🌙" },
		{ "cloudy", "☁️" },
		{ "mostly_cloudy", "☁️" },
		{ "hazy", "🌤️" },
		{ "fog", "🌫️" },
		{ "dusty", "🌫️" },
		{ "wind", "🌬️" },
		{ "windy", "🌬️" },
		{ "light_rain", "🌦️" },
		{ "rain", "🌧️" },
		{ "shower", "🌦️" },
		{ "light_shower", "🌦️" },
		// Heavier and night shower variants use the plain rain cloud rather
		// than the sun+rain glyph above — there's no moon+rain emoji, so the
		// night case falls back to the same glyph as a heavy day shower.
		{ "heavy_shower", "🌧️" },
		{ "night_showers", "🌧️" },
		// Storm's cloud glyph reads the same day or night, so no separate
		// night entry is needed beyond BOM's own "night_storm" descriptor.
		{ "storm", "⛈️" },
		{ "night_storm", "⛈️" },
		{ "cyclone", "🌀" },
		{ "tropical_cyclone", "🌀" },
		{ "frost", "🥶" },
		{ "snow", "❄️" },
	};
	return table.value( iconDescriptor, "🌤️" );
}
