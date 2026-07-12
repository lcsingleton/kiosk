pragma Singleton
import QtQuick

// Shared color roles for every card/banner/popup — swap literals for
// Theme.<role> rather than hardcoding hex. `isDay` is driven off the
// weather-sync daemon's sunrise/sunset (see WeatherBridge.sun); every
// other file just reads Theme's colors and never touches isDay directly.
QtObject {
    id: theme

    // Dark until a real snapshot says otherwise — matches this app's
    // original always-dark look, and avoids flashing light on a cold boot
    // before weather-sync's first poll has landed (/run is tmpfs, so the
    // snapshot file doesn't exist yet at that point).
    property bool isDay: false

    function _recompute() {
        const sun = weatherBridge.sun
        const sunrise = sun && sun.sunrise ? new Date(sun.sunrise) : null
        const sunset = sun && sun.sunset ? new Date(sun.sunset) : null
        if (!sunrise || !sunset || isNaN(sunrise) || isNaN(sunset))
            return   // no snapshot yet — keep whatever isDay currently is
        const now = new Date()
        isDay = now >= sunrise && now < sunset
    }

    // A timer alone is enough: sun times only change once a day, and the
    // daemon's own poll interval is coarser than this anyway.
    // QtObject has no default property, so this must be an explicit
    // property assignment rather than an implicit child.
    property Timer _recomputeTimer: Timer {
        interval: 60000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: theme._recompute()
    }

    // ---- surfaces ----
    readonly property color background: isDay ? "#eef2f9" : "#05070c"
    readonly property color canvasGradientTop: isDay ? "#dbe6f5" : "#070b14"
    readonly property color canvasGradientBottom: isDay ? "#f4f7fc" : "#0e1729"
    readonly property color surface: isDay ? "#ffffff" : "#101a2e"
    readonly property color surfaceAlt: isDay ? "#e3eaf5" : "#16294a"
    readonly property color surfaceAlt2: isDay ? "#d3deee" : "#1c3355"
    readonly property color border: isDay ? "#c7d2e3" : "#2a3c5c"
    readonly property color divider: isDay ? "#c7d2e3" : "#1c2c48"

    // ---- text (darkest/most-prominent first) ----
    readonly property color textPrimary: isDay ? "#101a2e" : "#eef2f9"
    readonly property color textTertiary: isDay ? "#3d4d68" : "#c7d2e3"
    readonly property color textSecondary: isDay ? "#5c6f8f" : "#8296b8"
    readonly property color textMuted: isDay ? "#8296b8" : "#5c6f8f"

    // ---- accents ----
    readonly property color accentBlue: isDay ? "#1f66c9" : "#3987e5"
    readonly property color accentGreen: isDay ? "#0b8a0b" : "#0ca30c"
    readonly property color accentTeal: isDay ? "#0d7a56" : "#199e70"
    readonly property color accentOrange: isDay ? "#b5490f" : "#d95926"
    readonly property color accentAmber: isDay ? "#a06800" : "#c98500"
    readonly property color accentPurple: isDay ? "#6f61c9" : "#9085e9"
    readonly property color forecastMuted: isDay ? "#39516f" : "#4d6f96"

    // ---- error banner ----
    readonly property color errorBackground: isDay ? "#fbdede" : "#3d1414"
    readonly property color errorBorder: "#e5484d"
    readonly property color errorText: isDay ? "#8c1c1f" : "#f5d0d1"
    readonly property color errorIcon: isDay ? "#c22e33" : "#ff6b6f"

    // ---- auth-prompt (info) banner ----
    readonly property color infoBackground: isDay ? "#dcecfb" : "#122b3d"
    readonly property color infoBorder: "#4dabf7"
    readonly property color infoText: isDay ? "#123a57" : "#d6ecfc"
    readonly property color infoTextMuted: isDay ? "#3d6488" : "#a9d3f0"
    readonly property color infoTextFaint: isDay ? "#5c7fa3" : "#7fa8c9"
}
