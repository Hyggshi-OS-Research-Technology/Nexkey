# The helper is the only part of NexKey that touches /dev/uinput, and it runs as
# its own process so that the input method itself never needs that permission.
add_executable(nexkey-uinput nexkey-uinput.cpp)

target_link_libraries(nexkey-uinput PRIVATE NexKey::Core)
nexkey_configure_target(nexkey-uinput)

install(TARGETS nexkey-uinput RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# A user unit keeps the device alive across restarts of Fcitx5. Nothing depends
# on the helper: when it is not running the input method deletes with the next
# mechanism in its list, so the unit can never block a login.
option(NEXKEY_INSTALL_SYSTEMD_UNIT "Install the systemd user unit for nexkey-uinput" ON)

if(NEXKEY_INSTALL_SYSTEMD_UNIT)
    # Deliberately not ${CMAKE_INSTALL_LIBDIR}: on a multiarch system that
    # expands to lib/x86_64-linux-gnu, which is not where systemd looks for
    # units.
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/nexkey-uinput.service
        DESTINATION lib/systemd/user
    )
endif()
