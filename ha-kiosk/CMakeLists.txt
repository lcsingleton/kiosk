cmake_minimum_required(VERSION 3.16)
project(ha-kiosk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)
# Land the binary under a "bin" dir even in an uninstalled build tree, so its
# relative walk up to a sibling "share" (below) matches the installed layout
# in both places.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network)

add_executable(ha-kiosk src/main.cpp src/CalendarBridge.cpp src/WeatherBridge.cpp)
target_link_libraries(ha-kiosk PRIVATE calendar-sync-client kiosk-log Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network)

# qml/ and assets/ are both architecture-independent data (interpreted QML
# text, images/fonts), so both belong under share/ per the FHS, not next to
# the binary. main.cpp resolves both at runtime via a fixed bin/../share
# offset, so mirror that layout in the build tree too, as siblings of bin/.
file(GLOB QML_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml")

add_custom_command(TARGET ha-kiosk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/ha-kiosk/qml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${QML_SOURCES}
        "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/ha-kiosk/qml"
    COMMAND_EXPAND_LISTS)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET ha-kiosk POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/ha-kiosk/assets")
endif()

install(TARGETS ha-kiosk RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${QML_SOURCES} DESTINATION ${CMAKE_INSTALL_DATADIR}/ha-kiosk/qml)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    install(DIRECTORY assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ha-kiosk/assets
        PATTERN ".gitkeep" EXCLUDE)
endif()

# etc/ mirrors its destination path on the tablet (see deploy/README.md), so
# these install absolute to /etc rather than under CMAKE_INSTALL_PREFIX.
# `DESTDIR=<stage> cmake --install build` (see docker/build.sh) prepends
# <stage>, turning this into a full FHS staging tree usable by the emulator
# container or, later, a `dpkg-buildpackage`-style .deb.
install(FILES etc/systemd/system/kiosk.service DESTINATION /etc/systemd/system)
install(FILES etc/modules-load.d/i2c-dev.conf DESTINATION /etc/modules-load.d)
install(FILES etc/logrotate.d/ha-kiosk DESTINATION /etc/logrotate.d)
install(FILES
    etc/udev/rules.d/99-mcp2221.rules
    etc/udev/rules.d/99-native-gpio.rules
    DESTINATION /etc/udev/rules.d)
