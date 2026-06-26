add_library(nexkey-fcitx5 MODULE NexKeyEngine.cpp)
target_link_libraries(nexkey-fcitx5 PRIVATE NexKey::vkcore Fcitx5::Core)
set_target_properties(nexkey-fcitx5 PROPERTIES PREFIX "")

install(TARGETS nexkey-fcitx5 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/fcitx5)
install(FILES data/nexkey.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod)
install(FILES data/nexkey-addon.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/addon)
