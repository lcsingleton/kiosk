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
    src/SnapshotWriter.cpp
    src/CommandServer.cpp
)

target_link_libraries(ha-kiosk-google-calendar-sync PRIVATE
    calendar-sync-client
    Qt6::Core
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
)

install(TARGETS ha-kiosk-google-calendar-sync RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
