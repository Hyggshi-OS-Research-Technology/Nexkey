# Shared application data, documentation and CPack configuration.

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/nexkey.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/icons/nexkey.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/icons/nexkey-vn.svg
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/icons/nexkey-en.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/status
)

if(NEXKEY_INSTALL_DOCS)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/CONFIGURATION.md
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/ARCHITECTURE.md
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/PACKAGING.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
    )
endif()

set(CPACK_PACKAGE_NAME "nexkey")
set(CPACK_PACKAGE_VENDOR "Hyggshi OS Research Technology")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vietnamese input method for Linux")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "hyggshidev@gmail.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_NAME "nexkey")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Hyggshi OS Research Technology <hyggshidev@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/postrm"
)

include(CPack)
