#include "IconMap.h"

#include <QHash>

QString IconMap::emoji( const QString &iconDescriptor )
{
	static const QHash<QString, QString> table = {
		{ "sunny", "☀️" },
		{ "clear", "🌙" },
		{ "night_clear", "🌙" },
		{ "partly_cloudy", "⛅" },
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
		{ "heavy_shower", "🌧️" },
		{ "night_showers", "🌧️" },
		{ "storm", "⛈️" },
		{ "night_storm", "⛈️" },
		{ "cyclone", "🌀" },
		{ "tropical_cyclone", "🌀" },
		{ "frost", "🥶" },
		{ "snow", "❄️" },
	};
	return table.value( iconDescriptor, "🌤️" );
}
