# SPDX-License-Identifier: MIT
# NexKey - Qt6 tray application build.

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

qt_add_executable(nexkey-gui
    main.cpp
    Autostart.cpp
    Autostart.hpp
    QtHelpers.hpp
    Daemon.cpp
    Daemon.hpp
    SettingsDialog.cpp
    SettingsDialog.hpp
    TrayApplication.cpp
    TrayApplication.hpp
    resources.qrc
)

target_link_libraries(nexkey-gui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    NexKey::Core
)

nexkey_configure_target(nexkey-gui)

install(TARGETS nexkey-gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
