#!/usr/bin/env bash
# NexKey - format every C++ source with clang-format.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
clang_format="${CLANG_FORMAT:-clang-format}"

if ! command -v "${clang_format}" >/dev/null 2>&1; then
    echo "clang-format not found; install it or set CLANG_FORMAT" >&2
    exit 1
fi

mapfile -t files < <(find "${root}/core" "${root}/cli" "${root}/gui" \
    "${root}/integrations" "${root}/tests" \
    \( -name '*.cpp' -o -name '*.hpp' \) -print)

"${clang_format}" -i --style="file:${root}/.clang-format" "${files[@]}"
echo "Formatted ${#files[@]} files."
