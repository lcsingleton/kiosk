cmake_minimum_required(VERSION 3.16)
project(ha-kiosk-google-calendar-sync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")

find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(OpenSSL REQUIRED)

# calendar-sync-client is added by the root CMakeLists.txt (it's a shared
# sibling, not private to this target).
add_executable(ha-kiosk-google-calendar-sync
    src/main.cpp
    src/Config.cpp
    src/GoogleAuth.cpp
    src/DelegatedAuth.cpp
    src/CalendarClient.cpp
    src/SnapshotBuilder.cpp
    src/CommandServer.cpp
)

target_link_libraries(ha-kiosk-google-calendar-sync PRIVATE
    calendar-sync-client
    kiosk-log
    snapshot-writer
    Qt6::Core
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
)

install(TARGETS ha-kiosk-google-calendar-sync RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# etc/ mirrors its destination path on the tablet (see deploy/README.md), so
# these install absolute to /etc rather than under CMAKE_INSTALL_PREFIX.
# `DESTDIR=<stage> cmake --install build` (see docker/build.sh) prepends
# <stage>, turning this into a full FHS staging tree usable by the emulator
# container or, later, a `dpkg-buildpackage`-style .deb.
install(FILES etc/systemd/system/ha-kiosk-google-calendar-sync.service
    DESTINATION /etc/systemd/system)
install(FILES etc/tmpfiles.d/kiosk.conf DESTINATION /etc/tmpfiles.d)
install(FILES etc/logrotate.d/ha-kiosk-google-calendar-sync DESTINATION /etc/logrotate.d)
# Kept as .example.json (never installed as the live daemon-config.json) —
# same reasoning as deploy/README.md step 6: real deployments hand-edit a
# copy, so the packaged/staged file must never clobber it on upgrade.
install(FILES etc/daemon-config.example.json DESTINATION /etc/kiosk)
