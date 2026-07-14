# Atomic JSON-snapshot writer shared by all three sync daemons — see
# SnapshotWriter.h. Structured the same way as kiosk-log (the existing
# precedent for code shared across ha-kiosk and the daemons).

add_library(snapshot-writer STATIC
    include/snapshot-writer/SnapshotWriter.h
    src/SnapshotWriter.cpp
)

target_include_directories(snapshot-writer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Qt6 REQUIRED COMPONENTS Core)
target_link_libraries(snapshot-writer PUBLIC Qt6::Core)
