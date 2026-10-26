add_library(nexkey-core STATIC
    src/Converter.cpp
    src/Engine.cpp
    src/Grapheme.cpp
    src/Settings.cpp
    src/Status.cpp
    src/Syllable.cpp
    src/Unicode.cpp
    src/Word.cpp
)

add_library(NexKey::Core ALIAS nexkey-core)

nexkey_configure_target(nexkey-core)
set_target_properties(nexkey-core PROPERTIES
    EXPORT_NAME Core
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(nexkey-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(TARGETS nexkey-core EXPORT NexKeyTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT NexKeyTargets
    FILE NexKeyTargets.cmake
    NAMESPACE NexKey::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NexKey
)
