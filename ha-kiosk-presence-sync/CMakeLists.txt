cmake_minimum_required(VERSION 3.16)
project(ha-kiosk-presence-sync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")

find_package(Qt6 REQUIRED COMPONENTS Core)

find_package(PkgConfig REQUIRED)
# gstreamer-app pulls in core gstreamer-1.0 as a dependency; this daemon only
# ever touches the appsink API directly (see CameraCapture), never the wider
# GStreamer surface, so no other GStreamer package is linked.
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-app-1.0)

add_executable(ha-kiosk-presence-sync
    src/main.cpp
    src/Config.cpp
    src/CameraCapture.cpp
    src/MotionDetector.cpp
    src/SnapshotBuilder.cpp
)

target_link_libraries(ha-kiosk-presence-sync PRIVATE
    kiosk-log
    snapshot-writer
    Qt6::Core
    PkgConfig::GST
)

install(TARGETS ha-kiosk-presence-sync RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# etc/ mirrors its destination path on the tablet (see deploy/README.md), so
# these install absolute to /etc rather than under CMAKE_INSTALL_PREFIX.
install(FILES etc/systemd/system/ha-kiosk-presence-sync.service
    DESTINATION /etc/systemd/system)
install(FILES etc/logrotate.d/ha-kiosk-presence-sync DESTINATION /etc/logrotate.d)
install(FILES etc/daemon-config.example.json DESTINATION /etc/kiosk
    RENAME presence-config.example.json)
