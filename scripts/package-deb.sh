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

printf '\nPackage written to %s\n' "${build_dir}"
ls -1 "${build_dir}"/*.deb 2>/dev/null || true
