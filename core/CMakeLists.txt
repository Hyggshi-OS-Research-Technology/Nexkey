add_library(nexkey-core STATIC
    src/Converter.cpp
    src/Delivery.cpp
    src/Engine.cpp
    src/Grapheme.cpp
    src/Settings.cpp
    src/Replacement.cpp
    src/Status.cpp
    src/Syllable.cpp
    src/Unicode.cpp
    src/Uinput.cpp
    src/UinputClient.cpp
    src/Word.cpp
)

add_library(NexKey::Core ALIAS nexkey-core)

nexkey_configure_target(nexkey-core)
set_target_properties(nexkey-core PROPERTIES
    EXPORT_NAME Core
    POSITION_INDEPENDENT_CODE ON
)

# Unicode normalization, grapheme segmentation and case mapping need the Unicode
# character database, which is far too large to maintain by hand. ICU provides
# it; without ICU a compact implementation that knows the Vietnamese alphabet is
# compiled in, so a plain build still works and only loses coverage outside that
# alphabet.
if(NEXKEY_USE_ICU)
    find_package(ICU 67 QUIET COMPONENTS uc i18n)
    if(ICU_FOUND)
        target_link_libraries(nexkey-core PRIVATE ICU::uc ICU::i18n)
        target_compile_definitions(nexkey-core PRIVATE NEXKEY_HAVE_ICU=1)
        message(STATUS
            "NexKey: ICU ${ICU_VERSION} provides normalization, grapheme segmentation "
            "and Unicode case mapping")
        set(NEXKEY_UNICODE_BACKEND "icu ${ICU_VERSION}" PARENT_SCOPE)
    else()
        message(STATUS
            "NexKey: ICU not found, Unicode support covers the Vietnamese alphabet only "
            "(install libicu-dev for full coverage)")
        set(NEXKEY_UNICODE_BACKEND "builtin" PARENT_SCOPE)
    endif()
else()
    message(STATUS "NexKey: NEXKEY_USE_ICU=OFF, Unicode support covers the Vietnamese alphabet only")
    set(NEXKEY_UNICODE_BACKEND "builtin" PARENT_SCOPE)
endif()

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
