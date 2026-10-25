#!/usr/bin/env bash
# NexKey - configure and build a release tree.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${root}/build}"
build_type="${2:-Debug}"

cmake -S "${root}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE="${build_type}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DNEXKEY_BUILD_TESTS=ON

cmake --build "${build_dir}" --parallel "$(nproc 2>/dev/null || echo 2)"

printf '\nBuilt:\n'
printf '  command line tool: %s/cli/nexkey\n' "${build_dir}"
if [ -d "${build_dir}/gui" ]; then
    printf '  tray application:  %s/gui/nexkey-gui\n' "${build_dir}"
fi
