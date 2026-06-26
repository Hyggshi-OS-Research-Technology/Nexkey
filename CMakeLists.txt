cmake_minimum_required(VERSION 3.26)
project(NexKey VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_IBUS "Build IBus engine backend" ON)
option(BUILD_FCITX5 "Build Fcitx5 engine backend" ON)
option(BUILD_GUI "Build Qt6 tray/settings GUI" ON)
option(BUILD_CLI "Build nexkey-cli tool" ON)
option(BUILD_TESTS "Build unit tests" ON)

include(GNUInstallDirs)

add_subdirectory(libvkcore)

find_package(PkgConfig QUIET)

if(BUILD_IBUS)
    if(PkgConfig_FOUND)
        pkg_check_modules(IBUS QUIET ibus-1.0)
    endif()
    if(IBUS_FOUND)
        add_subdirectory(nexkey-ibus)
    else()
        message(WARNING "IBus development files not found; nexkey-ibus will be skipped")
    endif()
endif()

if(BUILD_FCITX5)
    find_package(Fcitx5Core QUIET)
    if(Fcitx5Core_FOUND)
        add_subdirectory(nexkey-fcitx5)
    else()
        message(WARNING "Fcitx5Core not found; nexkey-fcitx5 will be skipped")
    endif()
endif()

if(BUILD_GUI)
    find_package(Qt6 6.5 QUIET COMPONENTS Core Widgets DBus)
    if(Qt6_FOUND)
        add_subdirectory(nexkey-gui)
    else()
        message(WARNING "Qt6 Core/Widgets/DBus not found; nexkey-gui will be skipped")
    endif()
endif()

if(BUILD_CLI)
    add_subdirectory(nexkey-cli)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(cmake/Packaging.cmake)
