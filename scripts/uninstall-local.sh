#!/usr/bin/env bash
# NexKey - remove an installation created by install-local.sh.
#
# A source install puts files in a prefix that no package manager knows about,
# so `apt remove` cannot clean it up and a half removed tree is what makes the
# next build look broken: an old addon library that Fcitx5 keeps loading, an old
# nexkey-uinput that is still running, a unit file that starts both.
#
# This script removes exactly what an install of this tree wrote. It reads
# install_manifest.txt from the build directory when there is one, which is the
# complete list, and falls back to the paths a NexKey install always uses, so it
# also works after the build tree is gone.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
prefix="/usr/local"
build_dir="${root}/build-local"
use_sudo=1
purge=0
dry_run=0
stop_apps=0

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
        --purge)
            purge=1
            shift
            ;;
        --stop-apps)
            stop_apps=1
            shift
            ;;
        --dry-run)
            dry_run=1
            shift
            ;;
        -h|--help)
            cat <<'USAGE'
Usage: scripts/uninstall-local.sh [--prefix DIR] [--build-dir DIR]
                                  [--purge] [--stop-apps] [--dry-run] [--no-sudo]

Removes a NexKey installation from the prefix install-local.sh installed it
into (default /usr/local). The helper service is stopped and disabled first, so
no process keeps a deleted binary open.

  --prefix DIR    prefix that was installed into (default /usr/local)
  --build-dir DIR build tree to read install_manifest.txt from (default
                  ./build-local); the known paths are used when it is missing
  --purge         also delete ~/.config/nexkey and ~/.cache/nexkey
  --stop-apps     also stop the tray application (nexkey-gui) if it is running
  --dry-run       print what would be removed and change nothing
  --no-sudo       never escalate; fails on the files that need root

Run it as yourself, not with sudo: the systemd user unit belongs to your user
manager, and `sudo ./scripts/uninstall-local.sh` would disable it in root's.
USAGE
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n' "$1" >&2
            exit 2
            ;;
    esac
done

if [ -z "${prefix}" ]; then
    printf 'A prefix is required\n' >&2
    exit 2
fi
# Trailing slashes would turn "${prefix}/bin" into "//bin" in the comparisons.
prefix="${prefix%/}"

if [ "${use_sudo}" -eq 1 ] && [ "$(id -u)" -ne 0 ] && [ ! -w "${prefix}" ]; then
    elevated=(sudo)
else
    elevated=()
fi

if [ "$(id -u)" -eq 0 ] && [ -n "${SUDO_USER:-}" ]; then
    printf 'warning: running as root; the systemd user unit belongs to %s\n' "${SUDO_USER}" >&2
    printf '         run this script as %s without sudo instead\n' "${SUDO_USER}" >&2
fi

say() { printf '%s\n' "$*"; }

# --- the service first ------------------------------------------------------
# Both because a running helper would keep working against a socket that is
# about to disappear, and because the kernel refuses to overwrite a binary that
# is being executed: this is what makes the next install fail with "Text file
# busy".
stop_helper() {
    if ! command -v systemctl >/dev/null 2>&1; then
        return 0
    fi
    if ! systemctl --user cat nexkey-uinput.service >/dev/null 2>&1 &&
        [ ! -f "${prefix}/lib/systemd/user/nexkey-uinput.service" ]; then
        return 0
    fi
    say "  systemctl --user disable --now nexkey-uinput"
    if [ "${dry_run}" -eq 1 ]; then
        return 0
    fi
    systemctl --user disable --now nexkey-uinput >/dev/null 2>&1 || true
    systemctl --user daemon-reload >/dev/null 2>&1 || true
}

stop_tray() {
    [ "${stop_apps}" -eq 1 ] || return 0
    command -v pkill >/dev/null 2>&1 || return 0
    pkill -x nexkey-gui >/dev/null 2>&1 || return 0
    say "  stopped nexkey-gui"
}

say "== stopping NexKey services"
stop_helper
stop_tray

# --- what to remove ---------------------------------------------------------
paths=()

add_path() {
    local candidate="$1"
    local existing
    for existing in "${paths[@]+"${paths[@]}"}"; do
        [ "${existing}" = "${candidate}" ] && return 0
    done
    paths+=("${candidate}")
}

# The manifest is the exact list an install wrote. Only paths inside the prefix
# are taken from it: a manifest that was produced with another prefix must never
# make this script delete something the user did not ask for.
manifest_count=0
if [ -f "${build_dir}/install_manifest.txt" ]; then
    # The last line has no newline, so the loop needs the extra test or the last
    # file of the manifest would be the one that is never removed.
    while IFS= read -r line || [ -n "${line}" ]; do
        case "${line}" in
            "${prefix}/"*) add_path "${line}"; manifest_count=$((manifest_count + 1)) ;;
            "") ;;
            *) say "  skipped (outside ${prefix}): ${line}" ;;
        esac
    done <"${build_dir}/install_manifest.txt"
else
    say "  no ${build_dir}/install_manifest.txt, using the known paths"
fi

# Always checked, because a few things are not in the manifest: the IBus
# component file is written by an install script, and a build configured without
# a component does not list the others either.
add_path "${prefix}/bin/nexkey"
add_path "${prefix}/bin/nexkey-gui"
add_path "${prefix}/bin/nexkey-uinput"
add_path "${prefix}/share/fcitx5/addon/nexkey.conf"
add_path "${prefix}/share/fcitx5/inputmethod/nexkey.conf"
add_path "${prefix}/share/ibus/component/nexkey.xml"
add_path "${prefix}/share/applications/nexkey.desktop"
add_path "${prefix}/share/nexkey/nexkey-autostart.desktop"
add_path "${prefix}/share/icons/hicolor/scalable/apps/nexkey.svg"
add_path "${prefix}/share/icons/hicolor/scalable/status/nexkey-vn.svg"
add_path "${prefix}/share/icons/hicolor/scalable/status/nexkey-en.svg"
add_path "${prefix}/lib/systemd/user/nexkey-uinput.service"
add_path "${prefix}/include/nexkey"
add_path "${prefix}/share/doc/NexKey"

# Library directories are the one place where the layout depends on the system
# (a multiarch Debian keeps the addon in lib/x86_64-linux-gnu), so they are
# globbed. An unmatched glob stays literal and is skipped by remove_path.
for libdir in "${prefix}/lib" "${prefix}/lib"/*/ "${prefix}/lib64"; do
    libdir="${libdir%/}"
    [ -d "${libdir}" ] || continue
    add_path "${libdir}/fcitx5/nexkey.so"
    add_path "${libdir}/nexkey/nexkey-ibus"
    add_path "${libdir}/libnexkey-core.a"
    add_path "${libdir}/cmake/NexKey"
done

removed=0
remove_path() {
    local target="$1"
    if [ ! -e "${target}" ] && [ ! -L "${target}" ]; then
        return 0
    fi
    if [ -d "${target}" ] && [ ! -L "${target}" ]; then
        say "  rm -r ${target}"
        [ "${dry_run}" -eq 1 ] && { removed=$((removed + 1)); return 0; }
        "${elevated[@]}" rm -rf -- "${target}"
    else
        say "  rm ${target}"
        [ "${dry_run}" -eq 1 ] && { removed=$((removed + 1)); return 0; }
        "${elevated[@]}" rm -f -- "${target}"
    fi
    removed=$((removed + 1))
}

say "== removing NexKey from ${prefix}"
for path in "${paths[@]}"; do
    remove_path "${path}"
done

# Directories that only existed for NexKey. rmdir refuses to touch one that
# still holds something else, which is what keeps this from eating a
# /usr/share/fcitx5 that other input methods use.
cleanup_dirs=(
    "${prefix}/bin"
    "${prefix}/share/fcitx5/addon"
    "${prefix}/share/fcitx5/inputmethod"
    "${prefix}/share/fcitx5"
    "${prefix}/share/ibus/component"
    "${prefix}/share/ibus"
    "${prefix}/share/nexkey"
    "${prefix}/share/applications"
    "${prefix}/lib/systemd/user"
    "${prefix}/lib/systemd"
    "${prefix}/lib/nexkey"
    "${prefix}/lib/fcitx5"
    "${prefix}/lib64/nexkey"
    "${prefix}/lib64/fcitx5"
    "${prefix}/include"
    "${prefix}/share/doc/NexKey"
    "${prefix}/share/icons/hicolor/scalable/apps"
    "${prefix}/share/icons/hicolor/scalable/status"
    "${prefix}/share/icons/hicolor/scalable"
    "${prefix}/share/icons/hicolor"
    "${prefix}/share/icons"
    "${prefix}/share/doc"
    "${prefix}/lib/cmake"
    "${prefix}/lib64/cmake"
    "${prefix}/lib"
    "${prefix}/lib64"
    "${prefix}/share"
)
for dir in "${prefix}/lib"/*/; do
    cleanup_dirs+=("${dir%/}/fcitx5" "${dir%/}/nexkey" "${dir%/}/cmake/NexKey")
done
for dir in "${cleanup_dirs[@]}"; do
    [ -d "${dir}" ] || continue
    [ "${dry_run}" -eq 1 ] && continue
    "${elevated[@]}" rmdir --ignore-fail-on-non-empty -- "${dir}" 2>/dev/null || true
done

# --- optional: the user's own data ------------------------------------------
if [ "${purge}" -eq 1 ]; then
    say "== removing configuration and caches"
    for path in \
        "${XDG_CONFIG_HOME:-${HOME}/.config}/nexkey" \
        "${XDG_CACHE_HOME:-${HOME}/.cache}/nexkey" \
        "${XDG_RUNTIME_DIR:-/nonexistent}/nexkey"; do
        if [ -e "${path}" ]; then
            say "  rm -r ${path}"
            [ "${dry_run}" -eq 1 ] || rm -rf -- "${path}"
            removed=$((removed + 1))
        fi
    done
fi

if [ "${dry_run}" -eq 1 ]; then
    say ""
    say "Dry run: nothing was changed. ${removed} path(s) would be removed."
    exit 0
fi

say ""
if [ "${removed}" -eq 0 ]; then
    say "Nothing to remove: ${prefix} does not contain a NexKey installation."
else
    say "Removed ${removed} path(s) from ${prefix} (${manifest_count} from the manifest)."
fi

say ""
cat <<MESSAGE
Next steps:
  1. Restart Fcitx5 so it forgets the addon:
       fcitx5 -d --replace
  2. If IBus was set up, refresh its cache and restart it:
       ibus write-cache --system
       ibus restart
  3. Open fcitx5-configtool and remove "NexKey" from the input method list if it
     is still there; Fcitx5 keeps the entry in its own profile and logs a
     warning about a missing addon until it is removed.
MESSAGE

if [ "${purge}" -eq 0 ]; then
    say "The configuration in ~/.config/nexkey/nexkey.conf was kept; pass --purge to delete it."
fi
