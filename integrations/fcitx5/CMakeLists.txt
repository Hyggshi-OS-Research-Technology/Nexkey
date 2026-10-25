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

# Fcitx5 takes the addon name from the file name in share/fcitx5/addon, not
# from the [Addon] Name key, and an input method entry is only accepted when
# its Addon key names an addon that exists. The addon therefore has to be
# installed as nexkey.conf, because data/nexkey.conf declares Addon=nexkey.
install(FILES data/nexkey-addon.conf
    DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/addon
    RENAME nexkey.conf
)
install(FILES data/nexkey.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod)
