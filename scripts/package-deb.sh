#!/usr/bin/env bash
# NexKey - build a Debian package with CPack.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${root}/build-deb"

cmake -S "${root}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DNEXKEY_BUILD_TESTS=OFF

cmake --build "${build_dir}" --parallel "$(nproc 2>/dev/null || echo 2)"
cpack --config "${build_dir}/CPackConfig.cmake" -B "${build_dir}"

package="$(ls -1 "${build_dir}"/*.deb 2>/dev/null | head -1 || true)"
if [ -z "${package}" ]; then
    printf 'The package was not created.\n' >&2
    exit 1
fi

printf '\nPackage written to %s\n' "${package}"
if command -v lsb_release >/dev/null 2>&1; then
    printf 'Built for the host release: %s\n' "$(lsb_release -ds 2>/dev/null || echo unknown)"
fi
printf '\nDepends: %s\n' "$(dpkg-deb --field "${package}" Depends 2>/dev/null || echo unknown)"
printf '\nInstall it with:\n  sudo apt install %s\n' "${package}"
