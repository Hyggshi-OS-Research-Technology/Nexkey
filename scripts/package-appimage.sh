#!/usr/bin/env bash
# NexKey - build a self contained AppImage with linuxdeploy.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${root}/build-appimage"
appdir="${root}/NexKey.AppDir"
tools="${root}/.tools"

cmake -S "${root}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DNEXKEY_BUILD_TESTS=OFF
cmake --build "${build_dir}" --parallel "$(nproc 2>/dev/null || echo 2)"
DESTDIR="${appdir}" cmake --install "${build_dir}"

mkdir -p "${tools}"
linuxdeploy="${tools}/linuxdeploy-x86_64.AppImage"
plugin="${tools}/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "${linuxdeploy}" ]; then
    curl -fL -o "${linuxdeploy}" \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fi
if [ ! -f "${plugin}" ]; then
    curl -fL -o "${plugin}" \
        https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
fi

chmod +x "${linuxdeploy}" "${plugin}"

export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}"

"${linuxdeploy}" \
    --appdir "${appdir}" \
    --desktop-file "${appdir}/usr/share/applications/nexkey.desktop" \
    --icon-file "${root}/gui/icons/nexkey.svg" \
    --plugin qt \
    --output appimage

printf '\nAppImage written to %s\n' "${root}"
ls -1 "${root}"/*.AppImage 2>/dev/null || true
