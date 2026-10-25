# SPDX-License-Identifier: MIT
# NexKey - Fcitx5 addon build.

add_library(nexkey-fcitx5 MODULE nexkey.cpp)

target_link_libraries(nexkey-fcitx5 PRIVATE NexKey::Core Fcitx5::Core)
nexkey_configure_target(nexkey-fcitx5)

set_target_properties(nexkey-fcitx5 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "nexkey"
)

install(TARGETS nexkey-fcitx5 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/fcitx5)
install(FILES data/nexkey-addon.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/addon)
install(FILES data/nexkey.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod)
