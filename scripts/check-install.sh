#!/usr/bin/env bash
# NexKey - check an installed tree the way Fcitx5 loads it.
#
# Fcitx5 discovers an input method through two files in its data directory and
# silently drops the entry when they do not agree. This script walks the same
# steps and names the file that is wrong, which is faster than guessing why
# NexKey is missing from the input method list.
set -uo pipefail

prefix=""
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix)
            prefix="${2:-}"
            shift 2
            ;;
        -h|--help)
            cat <<'USAGE'
Usage: scripts/check-install.sh [--prefix DIR]

Checks that the Fcitx5 addon files of an installed NexKey tree are consistent:
addon registration, shared library, input method entry, icons and the tray
application. Exits with 1 when something Fcitx5 needs is missing or mismatched.
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
    for candidate in /usr /usr/local; do
        if [ -d "${candidate}/share/fcitx5" ]; then
            prefix="${candidate}"
            break
        fi
    done
    prefix="${prefix:-/usr}"
fi

failures=0
warnings=0

ok() { printf '  ok    %s\n' "$1"; }
fail() {
    printf '  FAIL  %s\n' "$1"
    if [ $# -gt 1 ]; then
        printf '        %s\n' "$2"
    fi
    failures=$((failures + 1))
}
warn() {
    printf '  warn  %s\n' "$1"
    if [ $# -gt 1 ]; then
        printf '        %s\n' "$2"
    fi
    warnings=$((warnings + 1))
}
section() { printf '\n%s\n' "$1"; }

# Value of a key inside a small ini file, without the surrounding whitespace.
ini_value() {
    sed -n "s/^[[:space:]]*$2[[:space:]]*=[[:space:]]*//p" "$1" 2>/dev/null | head -1 | sed 's/[[:space:]]*$//'
}

printf 'NexKey installation check for prefix %s\n' "${prefix}"

section 'Addon registration'

addon_conf="${prefix}/share/fcitx5/addon/nexkey.conf"
inputmethod_conf="${prefix}/share/fcitx5/inputmethod/nexkey.conf"

if [ -f "${addon_conf}" ]; then
    ok "${addon_conf}"
else
    stale="$(ls "${prefix}"/share/fcitx5/addon/nexkey*.conf 2>/dev/null | head -1)"
    if [ -n "${stale}" ]; then
        fail "${addon_conf} is missing" \
             "found ${stale} instead: Fcitx5 takes the addon name from the file name, so a file called ${stale##*/} registers an addon that the input method entry does not point at. Reinstall NexKey: the addon must be installed as nexkey.conf"
    else
        fail "${addon_conf} is missing" "the Fcitx5 addon is not installed; build with -DNEXKEY_BUILD_FCITX5=ON and install it"
    fi
fi

for stale in "${prefix}"/share/fcitx5/addon/nexkey*.conf; do
    [ -e "${stale}" ] || continue
    [ "${stale}" = "${addon_conf}" ] && continue
    warn "leftover ${stale}" \
         "an earlier installation used another file name; remove it with: sudo rm ${stale}"
done

if [ -f "${inputmethod_conf}" ]; then
    ok "${inputmethod_conf}"
else
    fail "${inputmethod_conf} is missing" "the input method entry is not installed"
fi

for stale in "${prefix}"/share/fcitx5/inputmethod/nexkey*.conf; do
    [ -e "${stale}" ] || continue
    [ "${stale}" = "${inputmethod_conf}" ] && continue
    warn "leftover ${stale}" \
         "an earlier installation used another file name; remove it with: sudo rm ${stale}"
done

# An input method entry is only kept when its Addon key names an addon file
# that exists, so this is the check that would have caught a wrong file name.
if [ -f "${addon_conf}" ] && [ -f "${inputmethod_conf}" ]; then
    addon="$(ini_value "${inputmethod_conf}" Addon)"
    if [ -z "${addon}" ]; then
        fail 'the input method entry has no Addon key'
    elif [ -f "${prefix}/share/fcitx5/addon/${addon}.conf" ]; then
        ok "input method entry points at the addon '${addon}'"
    else
        fail "the input method entry points at '${addon}', which has no addon file" \
             "Fcitx5 drops such an entry without a message; the addon has to be installed as ${prefix}/share/fcitx5/addon/${addon}.conf"
    fi

    for key in Name LangCode; do
        value="$(ini_value "${inputmethod_conf}" "${key}")"
        if [ -z "${value}" ]; then
            fail "the input method entry has no ${key} key"
        else
            ok "${key}=${value}"
        fi
    done
fi

section 'Shared library'

library=""
if [ -f "${addon_conf}" ]; then
    library="$(ini_value "${addon_conf}" Library)"
    type="$(ini_value "${addon_conf}" Type)"
    if [ -n "${type}" ] && [ "${type}" != "SharedLibrary" ]; then
        fail "unexpected addon Type=${type}"
    fi
fi
library="${library:-nexkey}"

shared_object=""
for directory in "${prefix}"/lib/*/fcitx5 "${prefix}"/lib/fcitx5 "${prefix}"/lib64/fcitx5; do
    if [ -f "${directory}/${library}.so" ]; then
        shared_object="${directory}/${library}.so"
        break
    fi
done

if [ -n "${shared_object}" ]; then
    ok "${shared_object}"
    if command -v file >/dev/null 2>&1; then
        if file -b "${shared_object}" | grep -q '^ELF'; then
            ok 'the shared object is a valid ELF file'
        else
            fail "${shared_object} is not an ELF file"
        fi
    fi
    if command -v nm >/dev/null 2>&1; then
        if nm -D --defined-only "${shared_object}" 2>/dev/null | grep -q 'fcitx_addon_factory_instance'; then
            ok 'the addon factory symbol is exported'
        else
            fail "${shared_object} does not export fcitx_addon_factory_instance" \
                 "the file is not a Fcitx5 addon or was built against another framework"
        fi
    fi
    if command -v ldd >/dev/null 2>&1; then
        if ldd "${shared_object}" 2>/dev/null | grep -q 'not found'; then
            fail 'the shared object has unresolved libraries' \
                 "$(ldd "${shared_object}" | grep 'not found' | head -3 | tr '\n' ' ')"
        else
            ok 'all libraries of the shared object are present'
        fi
    fi
else
    fail "no ${library}.so below ${prefix}/lib" \
         "the addon library is missing; looked in lib/fcitx5 and lib/<triplet>/fcitx5"
fi

section 'Icons and desktop entry'

for icon in \
    "${prefix}/share/icons/hicolor/scalable/apps/nexkey.svg" \
    "${prefix}/share/icons/hicolor/scalable/status/nexkey-vn.svg" \
    "${prefix}/share/icons/hicolor/scalable/status/nexkey-en.svg"; do
    if [ -f "${icon}" ]; then
        ok "${icon}"
    else
        warn "${icon} is missing" 'the input method still works, but the panel icon falls back to a generic one'
    fi
done

if [ -x "${prefix}/bin/nexkey-gui" ]; then
    ok "${prefix}/bin/nexkey-gui"
else
    warn "${prefix}/bin/nexkey-gui is missing" 'the settings window and the tray icon are not installed'
fi

if [ -x "${prefix}/bin/nexkey" ]; then
    ok "${prefix}/bin/nexkey"
else
    warn "${prefix}/bin/nexkey is missing" 'the command line tool is not installed'
fi

section 'Runtime state'

if command -v fcitx5-remote >/dev/null 2>&1; then
    if fcitx5-remote --check >/dev/null 2>&1; then
        ok 'Fcitx5 is running'
        warn 'Fcitx5 was started before the files were installed' \
             'run "fcitx5 -d --replace" once, then add NexKey in fcitx5-configtool'
        profile="${XDG_CONFIG_HOME:-${HOME}/.config}/fcitx5/profile"
        if [ -f "${profile}" ]; then
            if grep -qx 'nexkey' "${profile}"; then
                ok 'nexkey is part of an input method group'
            else
                warn 'nexkey is not in an input method group yet' \
                     'open fcitx5-configtool, Input Method tab, add NexKey'
            fi
        fi
    else
        ok 'Fcitx5 is not running (nothing to restart)'
    fi
else
    warn 'fcitx5-remote is not installed' 'install the fcitx5 package to use this backend'
fi

config="${XDG_CONFIG_HOME:-${HOME}/.config}/nexkey/nexkey.conf"
if [ -f "${config}" ]; then
    ok "${config}"
else
    warn "${config} does not exist yet" 'it is created on the first change, the built in defaults are used until then'
fi

printf '\n'
if [ "${failures}" -gt 0 ]; then
    printf '%d problem(s) found, %d warning(s).\n' "${failures}" "${warnings}"
    exit 1
fi
if [ "${warnings}" -gt 0 ]; then
    printf 'No blocking problem, %d warning(s).\n' "${warnings}"
    exit 0
fi
printf 'Everything Fcitx5 needs is in place.\n'
exit 0
