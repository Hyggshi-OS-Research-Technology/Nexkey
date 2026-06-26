add_executable(nexkey-ibus main.cpp NexKeyIBusEngine.cpp)
target_include_directories(nexkey-ibus PRIVATE ${IBUS_INCLUDE_DIRS})
target_link_libraries(nexkey-ibus PRIVATE NexKey::vkcore ${IBUS_LIBRARIES})
target_compile_options(nexkey-ibus PRIVATE ${IBUS_CFLAGS_OTHER})

install(TARGETS nexkey-ibus RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/nexkey)
install(FILES data/nexkey.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/ibus/component)
