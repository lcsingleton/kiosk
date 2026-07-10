# The client half of the calendar-sync daemon's command socket protocol
# (wire types plus the QLocalSocket wrapper), exported so ha-kiosk can talk
# to the daemon without duplicating knowledge of its wire format — that
# knowledge lives here, alongside the daemon's own CommandServer that speaks
# the other end of it.
#
# Set explicitly (rather than relying on the parent scope) since this
# library is also consumed from ha-kiosk's own CMakeLists.txt.
set(CMAKE_AUTOMOC ON)

# Headers are listed explicitly (not just the .cpp files) because AUTOMOC's
# implicit header discovery only looks next to each .cpp; ours live under
# include/, a different directory, so it would otherwise silently skip
# generating moc code for CalendarSyncClient.
add_library(calendar-sync-client STATIC
    include/calendar-sync-client/CommandTypes.h
    include/calendar-sync-client/CalendarSyncClient.h
    src/CommandTypes.cpp
    src/CalendarSyncClient.cpp
)

target_include_directories(calendar-sync-client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
target_link_libraries(calendar-sync-client PUBLIC Qt6::Core Qt6::Network)
