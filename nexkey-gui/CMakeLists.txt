qt_add_executable(nexkey-gui
    main.cpp
    TrayManager.hpp
    TrayManager.cpp
    SettingsWindow.hpp
    SettingsWindow.cpp
    DBusService.hpp
    DBusService.cpp
    resources/nexkey.qrc
)

set_target_properties(nexkey-gui PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_link_libraries(nexkey-gui PRIVATE Qt6::Core Qt6::Widgets Qt6::DBus NexKey::vkcore)
install(TARGETS nexkey-gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/icons/nexkey-vi.svg resources/icons/nexkey-en.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/nexkey/icons)
install(FILES dbus/org.nexkey.Settings.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces)
