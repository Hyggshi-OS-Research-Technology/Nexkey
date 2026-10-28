#!/usr/bin/env bash
# NexKey - build from source and install into a prefix.
#
# This is the reliable way to get a working installation on a distribution whose
# packages do not match the prebuilt .deb: everything is linked against the
# libraries that are actually installed on the machine.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
prefix="/usr/local"
build_dir="${root}/build-local"
use_sudo=1
run_check=1

while [ $# -gt 0 ]; do
    case "$1" in
        --prefix)
            prefix="${2:-}"
            shift 2
            ;;
        --build-dir)
            build_dir="${2:-}"
            shift 2
            ;;
        --no-sudo)
            use_sudo=0
            shift
            ;;
        --no-check)
            run_check=0
            shift
            ;;
        -h|--help)
            cat <<'USAGE'
Usage: scripts/install-local.sh [--prefix DIR] [--build-dir DIR] [--no-sudo]

Builds NexKey in Release mode and installs it. The default prefix is
/usr/local, which Fcitx5 already searches and which needs no package
metadata, so no dependency is resolved by the package manager.

Run it once after editing the sources to replace an earlier installation.
USAGE
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n' "$1" >&2
            exit 2
            ;;
    esac
done

if [ "${use_sudo}" -eq 1 ] && [ "$(id -u)" -ne 0 ] && [ ! -w "${prefix}" ]; then
    install_prefix=(sudo)
else
    install_prefix=()
fi

# The helper may be running from an earlier installation. The kernel refuses to
# overwrite a binary that is being executed, so it is stopped first and started
# again afterwards when it was running.
helper_was_running=0
if command -v systemctl >/dev/null 2>&1 &&
    systemctl --user is-active --quiet nexkey-uinput.service; then
    helper_was_running=1
    printf '== stopping nexkey-uinput for the install\n'
    systemctl --user stop nexkey-uinput.service >/dev/null 2>&1 || true
fi

printf '== configuring %s\n' "${build_dir}"
cmake -S "${root}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${prefix}" \
    -DNEXKEY_BUILD_TESTS=OFF

printf '\n== building\n'
cmake --build "${build_dir}" --parallel "$(nproc 2>/dev/null || echo 2)"

printf '\n== installing into %s\n' "${prefix}"
"${install_prefix[@]}" cmake --install "${build_dir}"

if [ "${helper_was_running}" -eq 1 ]; then
    # The unit file itself may have changed, so the user manager is told before
    # the service is started again.
    systemctl --user daemon-reload >/dev/null 2>&1 || true
    systemctl --user start nexkey-uinput.service >/dev/null 2>&1 ||
        printf 'warning: nexkey-uinput did not start again\n' >&2
fi

if [ "${run_check}" -eq 1 ] && [ -x "${root}/scripts/check-install.sh" ]; then
    printf '\n== checking the installation\n'
    "${root}/scripts/check-install.sh" --prefix "${prefix}" || true
fi

cat <<MESSAGE

Next steps:
  1. Restart Fcitx5 so it rescans the addon and input method directories:
       fcitx5 -d --replace
  2. Add NexKey in the input method list:
       fcitx5-configtool
  3. Switch to NexKey with the Fcitx5 trigger key (usually Control+space).

The configuration lives in ~/.config/nexkey/nexkey.conf.

The optional virtual keyboard helper is installed but not enabled:
  sudo install -m 0644 ${root}/packaging/udev/60-nexkey-uinput.rules /etc/udev/rules.d/
  systemctl --user enable --now nexkey-uinput
docs/UINPUT.md explains what it is for and what it can do.

To remove this installation again:
  ./scripts/uninstall-local.sh --prefix ${prefix}
Add --purge to delete ~/.config/nexkey and ~/.cache/nexkey as well.
MESSAGE
