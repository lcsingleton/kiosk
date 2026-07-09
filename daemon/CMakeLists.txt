cmake_minimum_required(VERSION 3.16)
project(kiosk-calendar-sync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(OpenSSL REQUIRED)

add_executable(kiosk-calendar-sync
    main.cpp
    Config.cpp
    GoogleAuth.cpp
    CalendarClient.cpp
    SnapshotBuilder.cpp
    SnapshotWriter.cpp
    CommandTypes.cpp
    CommandServer.cpp
)

target_link_libraries(kiosk-calendar-sync PRIVATE
    Qt6::Core
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
)
