#!/usr/bin/env bash
# NexKey - install the packages a full build needs.
#
# The engine, the command line tool and the tests need only a C++23 compiler and
# CMake; the tray application, the Fcitx5 addon and the IBus engine need the
# development files listed below. Run with --dry-run to see the command.
set -euo pipefail

dry_run=0
for argument in "$@"; do
    case "${argument}" in
        --dry-run)
            dry_run=1
            ;;
        -h|--help)
            cat <<'USAGE'
Usage: scripts/install-build-deps.sh [--dry-run]

Installs the build dependencies of NexKey with the package manager of the host:
apt (Debian, Ubuntu), dnf (Fedora), pacman (Arch) or zypper (openSUSE).
USAGE
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n' "${argument}" >&2
            exit 2
            ;;
    esac
done

run() {
    if [ "${dry_run}" -eq 1 ]; then
        printf '%s\n' "$*"
    else
        "$@"
    fi
}

if command -v apt-get >/dev/null 2>&1; then
    packages=(
        build-essential cmake ninja-build pkg-config
        qt6-base-dev qt6-base-dev-tools
        libfcitx5core-dev libibus-1.0-dev
    )
    run sudo apt-get update
    run sudo apt-get install -y "${packages[@]}"
elif command -v dnf >/dev/null 2>&1; then
    packages=(
        gcc-c++ cmake ninja-build pkgconf-pkg-config
        qt6-qtbase-devel fcitx5-devel ibus-devel
    )
    run sudo dnf install -y "${packages[@]}"
elif command -v pacman >/dev/null 2>&1; then
    packages=(base-devel cmake ninja pkgconf qt6-base fcitx5 ibus)
    run sudo pacman -S --needed "${packages[@]}"
elif command -v zypper >/dev/null 2>&1; then
    packages=(
        gcc-c++ cmake ninja pkg-config
        qt6-base-devel fcitx5-devel ibus-devel
    )
    run sudo zypper install -y "${packages[@]}"
else
    printf 'No supported package manager found.\n' >&2
    printf 'Install a C++23 compiler, CMake 3.21+, Qt 6, Fcitx5 and IBus by hand.\n' >&2
    exit 1
fi

printf '\nNow build and package:\n'
printf '  ./scripts/package-deb.sh     # .deb for this distribution release\n'
printf '  ./scripts/test.sh            # build and run the test suite\n'
