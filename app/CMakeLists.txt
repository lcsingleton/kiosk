cmake_minimum_required(VERSION 3.16)
project(hello-kiosk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

add_executable(hello-kiosk main.cpp)
target_link_libraries(hello-kiosk PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)

# Keep the .qml files next to the binary so they load via plain
# QUrl::fromLocalFile / same-directory implicit import, no Qt resource
# system or qml module URI needed for a throwaway prototype.
file(GLOB QML_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.qml")

add_custom_command(TARGET hello-kiosk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${QML_SOURCES}
        $<TARGET_FILE_DIR:hello-kiosk>
    COMMAND_EXPAND_LISTS)
