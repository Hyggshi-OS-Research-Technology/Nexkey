#!/usr/bin/env bash
# NexKey - run the test suite.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${root}/build}"

if [ ! -f "${build_dir}/CMakeCache.txt" ]; then
    "${root}/scripts/build.sh" "${build_dir}"
fi

cmake --build "${build_dir}" --parallel "$(nproc 2>/dev/null || echo 2)"
ctest --test-dir "${build_dir}" --output-on-failure
