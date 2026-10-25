cmake_minimum_required(VERSION 3.21)

project(NexKey
    VERSION 2.0.0
    DESCRIPTION "Vietnamese input method for Linux"
    HOMEPAGE_URL "https://github.com/Hyggshi-OS-Research-Technology/Nexkey"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(NexKeyHelpers)
include(GNUInstallDirs)

set(NEXKEY_CXX_STANDARD 23 CACHE STRING "C++ standard used to build NexKey (23 or 26)")
set_property(CACHE NEXKEY_CXX_STANDARD PROPERTY STRINGS 23 26)

if(NOT "cxx_std_${NEXKEY_CXX_STANDARD}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    message(FATAL_ERROR
        "The compiler does not support C++${NEXKEY_CXX_STANDARD}. "
        "Use a newer compiler or pass -DNEXKEY_CXX_STANDARD=23.")
endif()

option(NEXKEY_BUILD_CLI "Build the nexkey command line tool" ON)
option(NEXKEY_BUILD_TESTS "Build and register the unit tests" ON)
option(NEXKEY_BUILD_GUI "Build the Qt6 tray application" ON)
option(NEXKEY_BUILD_FCITX5 "Build the Fcitx5 input method addon" ON)
option(NEXKEY_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(NEXKEY_INSTALL_DOCS "Install documentation and license files" ON)

add_subdirectory(core)

if(NEXKEY_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(NEXKEY_BUILD_GUI)
    find_package(Qt6 6.4 QUIET COMPONENTS Core Gui Widgets DBus)
    if(Qt6_FOUND)
        add_subdirectory(gui)
    else()
        message(WARNING "Qt6 (Core, Gui, Widgets, DBus) not found: the tray application is skipped")
    endif()
endif()

if(NEXKEY_BUILD_FCITX5)
    find_package(Fcitx5Core 5.0 QUIET)
    if(Fcitx5Core_FOUND)
        add_subdirectory(integrations/fcitx5)
    else()
        message(WARNING "Fcitx5Core not found: the Fcitx5 addon is skipped")
    endif()
endif()

if(NEXKEY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(NexKeyPackaging)
