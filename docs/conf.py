project = "ha-tab kiosk"

extensions = ["breathe"]

breathe_projects = {
    "kiosk-log": "_doxygen/kiosk-log/xml",
    "calendar-sync-client": "_doxygen/calendar-sync-client/xml",
    "ha-kiosk-weather-sync": "_doxygen/ha-kiosk-weather-sync/xml",
    "ha-kiosk-google-calendar-sync": "_doxygen/ha-kiosk-google-calendar-sync/xml",
    "ha-kiosk": "_doxygen/ha-kiosk/xml",
}
breathe_default_project = "ha-kiosk"

html_theme = "breeze"
html_theme_options = {"header_tabs": False}
