cmake_minimum_required(VERSION 3.16)
project(kiosk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The tablet is a single-purpose appliance, not a general Debian install, so
# default `cmake --install` to /opt/kiosk (see deploy/README.md) instead of
# CMake's usual /usr/local. Still overridable with -DCMAKE_INSTALL_PREFIX=.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/kiosk" CACHE PATH "Install prefix" FORCE)
endif()
include(GNUInstallDirs)

# calendar-sync-client is a shared library consumed by both ha-kiosk (the
# command-socket client) and ha-kiosk-google-calendar-sync (the daemon that
# serves the other end of that socket), so it lives at the root, alongside
# its two consumers, rather than nested under either one. Must be added
# before both.
add_subdirectory(calendar-sync-client)
add_subdirectory(ha-kiosk-google-calendar-sync)
add_subdirectory(ha-kiosk)
