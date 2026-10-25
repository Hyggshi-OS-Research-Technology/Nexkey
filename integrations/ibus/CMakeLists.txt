# SPDX-License-Identifier: MIT
# NexKey - IBus input method engine build.
#
# An IBus engine is a separate process that registers itself on the session bus
# and is discovered through a component XML file.

add_executable(nexkey-ibus nexkey-ibus.cpp)

target_include_directories(nexkey-ibus PRIVATE ${IBus_INCLUDE_DIRS})
target_link_libraries(nexkey-ibus PRIVATE NexKey::Core ${IBus_LIBRARIES})
target_compile_options(nexkey-ibus PRIVATE ${IBus_CFLAGS_OTHER})

# GObject style code: the strict cast and conversion warnings used for the core
# library do not suit the GLib C API, so a narrower set is used here.
nexkey_configure_gobject_target(nexkey-ibus)

install(TARGETS nexkey-ibus RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/nexkey)

set(NEXKEY_IBUS_ENGINE_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/nexkey/nexkey-ibus")
configure_file(data/nexkey.xml "${CMAKE_CURRENT_BINARY_DIR}/nexkey.xml" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/nexkey.xml"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ibus/component
)
