cmake_minimum_required(VERSION 3.16)
project(ha-kiosk-weather-sync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")

# No OpenSSL dependency here (unlike ha-kiosk-google-calendar-sync): BOM's
# API needs no auth, so this daemon never signs or exchanges a token.
find_package(Qt6 REQUIRED COMPONENTS Core Network)

add_executable(ha-kiosk-weather-sync
    src/main.cpp
    src/Config.cpp
    src/BomClient.cpp
    src/InfluxClient.cpp
    src/IconMap.cpp
    src/SnapshotBuilder.cpp
)

target_link_libraries(ha-kiosk-weather-sync PRIVATE
    kiosk-log
    snapshot-writer
    Qt6::Core
    Qt6::Network
)

install(TARGETS ha-kiosk-weather-sync RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# etc/ mirrors its destination path on the tablet (see deploy/README.md), so
# these install absolute to /etc rather than under CMAKE_INSTALL_PREFIX.
# `DESTDIR=<stage> cmake --install build` (see docker/build.sh) prepends
# <stage>, turning this into a full FHS staging tree usable by the emulator
# container or, later, a `dpkg-buildpackage`-style .deb.
install(FILES etc/systemd/system/ha-kiosk-weather-sync.service
    DESTINATION /etc/systemd/system)
install(FILES etc/logrotate.d/ha-kiosk-weather-sync DESTINATION /etc/logrotate.d)
# Source file shares its basename (daemon-config.example.json) with the
# calendar-sync daemon's own example — renamed on install, same as
# deploy/README.md step 7's `cp ... weather-config.json`, so both can live
# side by side under /etc/kiosk without collision. Never installed as the
# live weather-config.json, same reasoning as the calendar daemon's example.
install(FILES etc/daemon-config.example.json DESTINATION /etc/kiosk
    RENAME weather-config.example.json)
