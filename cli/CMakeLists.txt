add_executable(nexkey
    main.cpp
    Cli.cpp
    Commands.cpp
)

target_link_libraries(nexkey PRIVATE NexKey::Core)
nexkey_configure_target(nexkey)

install(TARGETS nexkey RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
