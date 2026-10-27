add_executable(nexkey-tests
    main.cpp
    test_encoding.cpp
    test_candidates.cpp
    test_engine.cpp
    test_settings.cpp
    test_status.cpp
    test_syllable.cpp
    test_telex.cpp
    test_unicode.cpp
    test_vietnamese_words.cpp
    test_vni_viqr.cpp
)

target_link_libraries(nexkey-tests PRIVATE NexKey::Core)
target_include_directories(nexkey-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core/src
)

nexkey_configure_target(nexkey-tests)

add_test(NAME nexkey-tests COMMAND nexkey-tests)
