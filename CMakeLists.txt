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

# ha-kiosk links against the calendar-sync-client library target defined
# under ha-kiosk-google-calendar-sync/lib, so that subdirectory must be
# added first.
add_subdirectory(ha-kiosk-google-calendar-sync)
add_subdirectory(ha-kiosk)
