add_executable(nexkey-tests
    test_buffer.cpp
    test_telex.cpp
    test_vni.cpp
    test_encoding.cpp
)
target_link_libraries(nexkey-tests PRIVATE NexKey::vkcore)
add_test(NAME nexkey-tests COMMAND nexkey-tests)
