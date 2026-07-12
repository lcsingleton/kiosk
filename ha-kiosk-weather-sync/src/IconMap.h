#pragma once

#include <QString>

namespace IconMap
{

// Maps a BOM `icon_descriptor` (e.g. "sunny", "night_clear", "storm") to the
// emoji already used by WeatherCard.qml's mock data. Falls back to a plain
// sun/cloud glyph for any descriptor BOM adds that isn't in the table yet,
// rather than showing nothing.
QString emoji( const QString &iconDescriptor );

} // namespace IconMap
