# Shared Qt message handler (stderr + best-effort /var/log/<module>/ file)
# used by all three executables — see FileLogger.h. Structured the same way
# as calendar-sync-client (the existing precedent for code shared across
# ha-kiosk and the daemons).
set(CMAKE_AUTOMOC ON)

add_library(kiosk-log STATIC
    include/kiosk-log/FileLogger.h
    src/FileLogger.cpp
)

target_include_directories(kiosk-log PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Qt6 REQUIRED COMPONENTS Core)
target_link_libraries(kiosk-log PUBLIC Qt6::Core)
