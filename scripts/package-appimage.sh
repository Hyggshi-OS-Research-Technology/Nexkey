#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# NexKey - build a self contained AppImage with linuxdeploy.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${root}/build-appimage"
output_dir="${root}/dist"
appdir="${root}/NexKey.AppDir"
tools="${root}/.tools"
skip_build=0
keep_appdir=0
use_qt_plugin=1
require_gui=1

log() { printf '\n==> %s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Build a self contained NexKey AppImage.

The script installs a Release build into an AppDir, removes the development
files, downloads linuxdeploy into .tools/ once and writes
dist/NexKey-<version>-x86_64.AppImage.

Usage: ./scripts/package-appimage.sh [options]

Options:
  --build-dir <dir>     build tree to use        (default: build-appimage)
  --output-dir <dir>    where the image is saved (default: dist)
  --skip-build          reuse an existing build tree
  --keep-appdir         do not delete the AppDir afterwards
  --no-qt-plugin        skip the linuxdeploy Qt plugin
  --allow-missing-gui   package without the Qt tray application
  --help                print this help

Environment:
  QMAKE                 qmake to use for the Qt plugin (default: qmake6)
  NO_STRIP              set to 1 to keep the debug symbols
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) build_dir="${2:?missing value}"; shift 2 ;;
        --output-dir) output_dir="${2:?missing value}"; shift 2 ;;
        --skip-build) skip_build=1; shift ;;
        --keep-appdir) keep_appdir=1; shift ;;
        --no-qt-plugin) use_qt_plugin=0; shift ;;
        --allow-missing-gui) require_gui=0; shift ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option $1 (try --help)" ;;
    esac
done

for tool in cmake curl; do
    command -v "${tool}" >/dev/null 2>&1 || die "${tool} is required"
done

jobs="$(nproc 2>/dev/null || echo 2)"

if [ "${skip_build}" -eq 0 ]; then
    log "Configuring a Release build in ${build_dir}"
    # The IBus component file points at an absolute engine path, which cannot
    # work inside an image, so IBus is left to the distribution package.
    cmake -S "${root}" -B "${build_dir}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DNEXKEY_BUILD_TESTS=OFF \
        -DNEXKEY_BUILD_IBUS=OFF

    log "Building"
    cmake --build "${build_dir}" --parallel "${jobs}"
fi

[ -f "${build_dir}/CMakeCache.txt" ] || die "${build_dir} is not a configured build tree"
version="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "${build_dir}/CMakeCache.txt" | head -1)"
[ -n "${version}" ] || die "cannot determine the project version"

log "Installing into ${appdir}"
rm -rf "${appdir}"
DESTDIR="${appdir}" cmake --install "${build_dir}"

# An image without the tray application is useless for most people, so fail
# loudly instead of shipping a package that only contains the CLI.
if [ ! -x "${appdir}/usr/bin/nexkey-gui" ]; then
    if [ "${require_gui}" -eq 1 ]; then
        die "nexkey-gui was not built: install the Qt6 development files (qt6-base-dev) or pass --allow-missing-gui"
    fi
    printf 'warning: nexkey-gui is missing, packaging the command line tool only\n'
fi

log "Removing development files from the AppDir"
rm -rf "${appdir}/usr/include" \
       "${appdir}/usr/lib/cmake" \
       "${appdir}/usr/share/doc"
find "${appdir}/usr/lib" \( -name '*.a' -o -name 'cmake' -type d \) -exec rm -rf {} + 2>/dev/null || true

desktop_file="${appdir}/usr/share/applications/nexkey.desktop"
icon_file="${appdir}/usr/share/icons/hicolor/scalable/apps/nexkey.svg"
[ -f "${desktop_file}" ] || die "the desktop file is missing from the AppDir"
[ -f "${icon_file}" ] || die "the application icon is missing from the AppDir"

mkdir -p "${tools}" "${output_dir}"
linuxdeploy="${tools}/linuxdeploy-x86_64.AppImage"
qt_plugin="${tools}/linuxdeploy-plugin-qt-x86_64.AppImage"

download() {
    local url="$1" target="$2"
    if [ -s "${target}" ]; then
        return
    fi
    log "Downloading $(basename "${target}")"
    curl -fL --retry 3 --retry-delay 2 -o "${target}.part" "${url}"
    mv "${target}.part" "${target}"
    chmod +x "${target}"
}

download https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
    "${linuxdeploy}"

linuxdeploy_args=(
    --appdir "${appdir}"
    --desktop-file "${desktop_file}"
    --icon-file "${icon_file}"
    --custom-apprun "${root}/packaging/appimage/AppRun"
    --output appimage
)

if [ "${use_qt_plugin}" -eq 1 ]; then
    download https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage \
        "${qt_plugin}"

    qmake="$(command -v qmake6 || command -v qmake || true)"
    [ -n "${qmake}" ] || die "qmake6 was not found: install qt6-base-dev-tools or pass --no-qt-plugin"
    export QMAKE="${QMAKE:-${qmake}}"
    linuxdeploy_args+=(--plugin qt)
fi

log "Running linuxdeploy"
export APPIMAGE_EXTRACT_AND_RUN=1
export OUTPUT_DIR="${output_dir}"
(
    cd "${output_dir}"
    "${linuxdeploy}" "${linuxdeploy_args[@]}"
)

built="$(find "${output_dir}" -maxdepth 1 -name '*.AppImage' | head -1)"
[ -n "${built}" ] || die "linuxdeploy did not produce an AppImage"

target="${output_dir}/NexKey-${version}-x86_64.AppImage"
if [ "${built}" != "${target}" ]; then
    mv -f "${built}" "${target}"
fi
chmod +x "${target}"

if [ "${keep_appdir}" -eq 0 ]; then
    rm -rf "${appdir}"
fi

log "AppImage ready"
printf '  file:   %s\n' "${target}"
printf '  size:   %s\n' "$(du -h "${target}" | cut -f1)"
printf '  sha256: %s\n' "$(sha256sum "${target}" | cut -d' ' -f1)"
