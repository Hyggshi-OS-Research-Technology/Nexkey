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

# System wide autostart entry: the tray icon is what most users expect after an
# install. The file ships below the data directory and the package maintainer
# script links it into /etc/xdg/autostart, which keeps "cmake --install
# --prefix" self contained and lets dpkg handle the removal.
option(NEXKEY_INSTALL_AUTOSTART "Ship a system wide XDG autostart entry" ON)
if(NEXKEY_INSTALL_AUTOSTART)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/packaging/nexkey-autostart.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/nexkey
        RENAME nexkey-autostart.desktop
    )
endif()

if(NEXKEY_INSTALL_DOCS)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/CONFIGURATION.md
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/ARCHITECTURE.md
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/PACKAGING.md
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/FCITX5.md
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/IBUS.md
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
# Runtime dependencies.
#
# Two library families use different package names depending on how new the
# distribution is: the 64 bit time_t transition renamed libqt6core6 to
# libqt6core6t64 (Ubuntu 24.04 and later, Debian 13 and later) and dropped
# libfcitx5core0 for libfcitx5core7 earlier. A package that names only the newer
# spelling cannot be installed on the older half of the distributions and the
# other way round, so the default list offers both as alternatives.
#
# dpkg-shlibdeps calculates the exact names of the host distribution, which is
# what a maintainer uploading to one release wants, but the resulting package
# then only installs there. NEXKEY_PACKAGING_SHLIBDEPS chooses between the two.
option(NEXKEY_PACKAGING_SHLIBDEPS
    "Calculate the Debian runtime dependencies with dpkg-shlibdeps for the host distribution only"
    OFF
)

set(_nexkey_depends "libc6, libstdc++6")
if(Qt6_FOUND)
    string(APPEND _nexkey_depends
        ", libqt6core6t64 | libqt6core6"
        ", libqt6gui6t64 | libqt6gui6"
        ", libqt6widgets6t64 | libqt6widgets6"
        ", libqt6dbus6t64 | libqt6dbus6"
    )
endif()
if(Fcitx5Core_FOUND)
    string(APPEND _nexkey_depends ", libfcitx5core7 | libfcitx5core0")
endif()
if(IBus_FOUND)
    string(APPEND _nexkey_depends ", libibus-1.0-5")
endif()
# ICU keeps its soname per major version, so a package that links ICU needs the
# package of the ICU it was built against; there is no portable alternative.
if(NEXKEY_UNICODE_BACKEND MATCHES "^icu ([0-9]+)")
    string(APPEND _nexkey_depends ", libicu${CMAKE_MATCH_1}")
endif()

find_program(NEXKEY_FILE_TOOL NAMES file)
find_program(NEXKEY_DPKG_SHLIBDEPS NAMES dpkg-shlibdeps)

if(NEXKEY_PACKAGING_SHLIBDEPS AND NEXKEY_FILE_TOOL AND NEXKEY_DPKG_SHLIBDEPS)
    message(STATUS
        "NexKey packaging: dependency names are calculated by dpkg-shlibdeps, "
        "the package is meant for this distribution release only")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6")
else()
    if(NEXKEY_PACKAGING_SHLIBDEPS)
        message(WARNING
            "NexKey packaging: dpkg-shlibdeps or file is missing, "
            "falling back to the portable dependency list")
    endif()
    message(STATUS "NexKey packaging: portable dependency list")
    message(STATUS "NexKey packaging: Depends=${_nexkey_depends}")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "${_nexkey_depends}")
endif()

set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "fcitx5, fcitx5-frontend-gtk3 | fcitx5-frontend-gtk4 | fcitx5-frontend-qt5 | fcitx5-frontend-qt6")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "fcitx5-config-qt")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/postrm"
)

include(CPack)
