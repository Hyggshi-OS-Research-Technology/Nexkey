add_executable(nexkey main.cpp)
target_link_libraries(nexkey PRIVATE NexKey::vkcore)
install(TARGETS nexkey RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
