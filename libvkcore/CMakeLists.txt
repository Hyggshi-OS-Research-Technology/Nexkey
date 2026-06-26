add_library(vkcore
    src/VKBuffer.cpp
    src/VKEncoding.cpp
    src/VKTelex.cpp
    src/VKVni.cpp
    src/VKViqr.cpp
)

add_library(NexKey::vkcore ALIAS vkcore)

target_include_directories(vkcore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(vkcore PUBLIC cxx_std_20)

install(TARGETS vkcore EXPORT NexKeyTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
