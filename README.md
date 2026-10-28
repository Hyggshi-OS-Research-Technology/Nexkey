# NexKey

NexKey is a Vietnamese input method for Linux in the spirit of UniKey. The
keystroke engine is a dependency free C++23 library, desktop integration is
provided for both **Fcitx5** and **IBus**, and a small Qt6 application provides
the tray icon and the settings window. Everything is built, tested and packaged
with CMake.

```
        +----------------------+      +---------------------+
        |  nexkey-gui (Qt6)    |      |  nexkey-cli         |
        |  tray + settings     |      |  test + convert     |
        +----------+-----------+      +----------+----------+
                   |                             |
                   +------------+----------------+
                                |
                      nexkey.conf (XDG config)
                                |
              +-----------------+------------------+
              |                                    |
   +----------+-----------+            +-----------+----------+
   |  nexkey (fcitx5)     |            |  nexkey-ibus         |
   |  Fcitx5 InputMethod  |            |  IBus engine         |
   +----------+-----------+            +-----------+----------+
              |                                    |
              +-----------------+------------------+
                                |
                   +------------+------------+
                   |   nexkey-core (C++23)   |
                   |   engine + charsets     |
                   +-------------------------+

  nexkey-uinput (optional, separate process) presses BACKSPACE through
  /dev/uinput for the applications that ignore everything else.
```

## Features

- **100% Native Direct Typing (Non-preedit)**: Eliminates annoying underlines while typing. Characters appear directly and immediately in the document (VS Code, Chrome, terminals, LibreOffice), avoiding input latency and visual clutter.
- **Works in every application, without a list of names**: when an application drops a replacement, NexKey repairs the word on screen, then moves that program one step down a ladder — deletions through the virtual keyboard `nexkey-uinput`, then `deleteSurroundingText`, then forwarded `BackSpace` keys, then the underlined preedit — and remembers the choice in `[app_rules]`. Nothing is hard coded to a browser or an editor.
- **Deleting through a virtual keyboard (optional)**: `nexkey-uinput` creates a keyboard device through `/dev/uinput` and presses `BACKSPACE`, which even Chromium and Electron applications honour, because as far as they can tell a real key was pressed. It is a separate process that can press `BACKSPACE` and nothing else, only for the user who started it. See [docs/UINPUT.md](docs/UINPUT.md).
- **Ultra High Performance**: Leverages instant SurroundingText & key forwarding optimized for high-speed Linux applications across X11 & Wayland.
- **System Tray Quick Settings**: Right-click the NexKey tray icon for instant toggles: output charset, spell check, macros, capitalized macro, non-Vietnamese auto-restore, custom dictionary, and the smart emoji picker.
- **Per-Application Rules (App Rules)**: Assign dedicated input modes per application or window (e.g. `code` → `telex`, `gnome-terminal` → `off`, `firefox` → `vni`, `chrome` → `preedit`), from the tray window or with `nexkey config set app_rules.chrome preedit`.
- **Backup & Restore**: Export and import complete configurations via JSON files with selective component filtering: Main Settings, Macros, Custom Keymaps, App Rules, and Custom Dictionary.
- **Dynamic `$TIME` / `$DATE` Macros**: Macro expansions support dynamic placeholders `$TIME` and `$DATE` (e.g. `kg` → `khô gà $TIME`), customizable via `Time Format` and `Date Format`.
- **Keyboard Presets**: Built-in support for popular layouts: Telex, Simple Telex, VNI, VIQR, and Microsoft layout with one-click preset switching.
- **Smart Emoji Picker**: Fast emoji search with fuzzy matching and a 9-recent-emoji history row with instant clipboard copying.
- **Non-Vietnamese Fallback**: Restores the raw keystrokes when a word cannot be Vietnamese, so `windows`, `jazz`, `https` and `git` come out as typed. Words that do spell a valid syllable (`test`, `last`) are kept as typed when they are listed in the personal dictionary.
- **Linux Native & Stable**: Seamlessly compatible with Wayland, X11, Chromium, Electron, and Wine. Zero character loss on space or window focus changes.

## Quick Settings (System Tray)

Right-click the NexKey system tray icon to access quick settings:

| Option | Function | Default |
| --- | --- | --- |
| **Output Charset** | Choose output encoding (Unicode, Unicode NFD, TCVN3, VIQR) | `Unicode` |
| **Spell Check** | Reserved for the dictionary-based check (stored in the configuration, not used by the engine yet) | On |
| **Macro** | Enable/disable macro shorthand expansion | On |
| **Capitalize Macro** | Auto-capitalize macro expansions when shorthand is capitalized | On |
| **Auto Non-VN Restore** | Automatically restore non-Vietnamese words | On |
| **Custom Dictionary** | Commit the words of `[custom_dictionary]` exactly as typed | Off |
| **Direct Typing** | Direct typing without preedit underline (Non-preedit) | On |
| **Emoji Picker...** | Open the smart emoji picker dialog | — |

## Requirements

| Component | Requirement |
| --- | --- |
| Compiler | GCC 12+, Clang 15+ (C++23) |
| Build system | CMake 3.21+ |
| Core and CLI | optional: ICU 67+ (`libicu-dev`) |
| Tray application | Qt 6.2+ (Core, Gui, Widgets, DBus) |
| Fcitx5 addon | Fcitx5 5.0+ development files |
| IBus engine | IBus 1.5+ development files |
| Virtual keyboard deletions (optional) | `/dev/uinput`, plus a udev rule or the `uinput` group |
| Tests | none |

Every optional component is detected at configure time. A plain build with no
Qt and no Fcitx5 still produces the library, the CLI and the test suite.

ICU is the recommended dependency of the core library: it supplies Unicode
normalization (NFC/NFD), grapheme cluster segmentation and case mapping. Without
it NexKey still builds, but Unicode support is limited to the Vietnamese
alphabet plus combining marks, which is enough for Vietnamese text and not much
else. `nexkey version` prints the backend that was compiled in.

## Build

```bash
./scripts/install-build-deps.sh  # ICU, Qt6, Fcitx5 and IBus development files
./scripts/build.sh               # Debug build in ./build
./scripts/test.sh                # build and run the test suite
./scripts/package-deb.sh         # CPack Debian package  -> build-deb/*.deb
./scripts/package-appimage.sh    # AppImage with linuxdeploy -> dist/*.AppImage
```

Manual build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Useful CMake options:

| Option | Default | Meaning |
| --- | --- | --- |
| `NEXKEY_BUILD_CLI` | `ON` | Build the `nexkey` command line tool |
| `NEXKEY_BUILD_TESTS` | `ON` | Build and register the unit tests |
| `NEXKEY_BUILD_GUI` | `ON` | Build the Qt6 tray application when Qt6 is found |
| `NEXKEY_BUILD_FCITX5` | `ON` | Build the Fcitx5 addon when Fcitx5 is found |
| `NEXKEY_BUILD_IBUS` | `ON` | Build the IBus engine when IBus is found |
| `NEXKEY_WARNINGS_AS_ERRORS` | `OFF` | Turn compiler warnings into errors |
| `NEXKEY_USE_ICU` | `ON` | Link ICU for Unicode normalization, grapheme segmentation and case mapping when it is found |
| `NEXKEY_INSTALL_DOCS` | `ON` | Install the documentation |
| `NEXKEY_INSTALL_AUTOSTART` | `ON` | Ship the system wide autostart template |
| `NEXKEY_PACKAGING_SHLIBDEPS` | `OFF` | Let `dpkg-shlibdeps` name the dependencies for this release only, instead of the portable list |

## Install and use

```bash
sudo apt install build-essential cmake ninja-build pkg-config libicu-dev \
     qt6-base-dev libfcitx5core-dev libibus-1.0-dev   # build dependencies
./scripts/package-deb.sh                              # -> build-deb/*.deb
sudo apt install ./build-deb/nexkey_*.deb
```

`./scripts/install-build-deps.sh` runs the dependency step for apt, dnf, pacman
and zypper. A source install works as well and skips the package metadata
entirely, which is the reliable path on a distribution the prebuilt package was
not built for:

```bash
./scripts/install-local.sh            # build and install into /usr/local
./scripts/check-install.sh            # verify what Fcitx5 needs
```

A `.deb` from the CI artifacts is built on Ubuntu 24.04 and targets that
release; on an older release build the package locally with
`./scripts/package-deb.sh`, which links against the libraries of the host. The
AppImage carries the Fcitx5 addon and the tray application (see below) and has
the same limitation.

**Fcitx5**: restart Fcitx5, open `fcitx5-configtool`, add **NexKey** to the
input method list and switch to it. See [docs/FCITX5.md](docs/FCITX5.md) for the
step by step instructions and troubleshooting.

**IBus**: run `ibus write-cache --system`, restart IBus, then add **NexKey** in
`ibus-setup`. See [docs/IBUS.md](docs/IBUS.md) for details and troubleshooting.

Both backends read `~/.config/nexkey/nexkey.conf` and reload it as soon as the
tray application or `nexkey config set` rewrites the file.

The Fcitx5 addon can delete through a virtual keyboard where everything else
fails. That needs a udev rule for `/dev/uinput` and the helper running:

```bash
sudo install -m 0644 packaging/udev/60-nexkey-uinput.rules /etc/udev/rules.d/
sudo udevadm control --reload && sudo udevadm trigger --name-match=uinput
systemctl --user enable --now nexkey-uinput
```

NexKey works without it; this only adds the rung that applications ignoring
synthetic deletions still honour. [docs/UINPUT.md](docs/UINPUT.md) explains it.

The default toggle key is `Control+Shift`. It can be changed in the settings
window or from the command line:

```bash
nexkey config set ui.toggle_key Ctrl+Shift+space
```

The Fcitx5 panel icon shows `VN` in Vietnamese mode and `EN` in English mode.

## AppImage

```bash
./scripts/package-appimage.sh
./dist/NexKey-2.0.0-x86_64.AppImage            # tray icon and settings window
./dist/NexKey-2.0.0-x86_64.AppImage --settings # open the settings window
```

The script builds a Release tree, installs it into an AppDir without the
development files, bundles Qt and the Fcitx5 addon with linuxdeploy, and prints
the size and the SHA-256 of the image. `packaging/appimage/AppRun` sets
`PATH`, `XDG_DATA_DIRS`, the Qt plugin paths and `FCITX_ADDON_DIRS` at runtime,
so both the tray application and the bundled addon work straight from the image.

The image is self contained apart from the FUSE runtime: on Ubuntu 24.04
install `libfuse2t64`, or extract it once with
`./NexKey-2.0.0-x86_64.AppImage --appimage-extract`.

Both the `.deb` and the `.AppImage` are uploaded as artifacts by CI. See
[docs/PACKAGING.md](docs/PACKAGING.md) for every option, the file layout and the
limitations.

## Command line

```bash
nexkey test "tieengs"                        # one word, composed
nexkey test --method vni "tie6ng1"           # the same word typed in VNI
nexkey test --tone-style traditional "hoaf"  # compare the two tone styles
nexkey test --trace "vieetj"                 # show the composition per keystroke
nexkey test                                  # read lines from standard input
nexkey convert --from unicode --to tcvn3 < in.txt > out.txt
nexkey status
nexkey toggle
nexkey config set output.encoding unicode-decomposed
```

`nexkey test` prints the composed text for every input line. `nexkey help`
lists every option, `nexkey methods` and `nexkey encodings` list the accepted
names.

## Configuration

The configuration lives in `$XDG_CONFIG_HOME/nexkey/nexkey.conf`, by default
`~/.config/nexkey/nexkey.conf`. See
[docs/CONFIGURATION.md](docs/CONFIGURATION.md) for every key, the file format
and the D-Bus interface.

## Character sets

| Name | Description |
| --- | --- |
| `unicode` | Precomposed UTF-8, the default |
| `unicode-decomposed` | UTF-8 in Unicode NFD |
| `tcvn3` | Legacy TCVN3 (ABC) single byte charset |
| `viqr` | Pure ASCII VIQR notation |

Conversion works in both directions, so `nexkey convert` can migrate old
documents from TCVN3 to Unicode and back.

## Project layout

```
core/                 C++23 engine, charsets, settings (no dependencies)
cli/                  nexkey command line tool
gui/                  Qt6 tray application and D-Bus service
integrations/fcitx5/  Fcitx5 input method addon
integrations/ibus/    IBus input method engine and component file
helpers/uinput/       nexkey-uinput, the optional virtual keyboard helper
tests/                unit tests with a tiny built-in harness
packaging/            desktop entry, Debian maintainer scripts
scripts/              build, test and packaging helpers
docs/                 usage, configuration and architecture notes
```

More detail is in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md),
[docs/FCITX5.md](docs/FCITX5.md), [docs/IBUS.md](docs/IBUS.md),
[docs/UINPUT.md](docs/UINPUT.md) and [docs/PACKAGING.md](docs/PACKAGING.md).

## Testing

The test suite covers the tone placement rules, Telex, VNI and VIQR parsing,
escape sequences, the fallback for non-Vietnamese words, the engine state
machine, charset conversion, configuration handling and the UTF-8 helpers:

```bash
ctest --test-dir build --output-on-failure
```

## Migrating from NexKey 1.x

Version 2.0 is a full rewrite:

- the engine moved to `core/`, is written in modern C++ and has no dependencies
  outside the standard library;
- the placeholder IBus binary was replaced by a real IBus engine that runs next
  to the Fcitx5 addon;
- the configuration file is now `~/.config/nexkey/nexkey.conf` with documented
  keys (see [docs/CONFIGURATION.md](docs/CONFIGURATION.md));
- the CLI gained `convert`, `config`, `status` and `toggle` commands and prints
  English messages only;
- TCVN3, VIQR and Unicode NFD are supported as output charsets, the incomplete
  VNI Windows table was removed rather than kept half working.

## Contributing

Contributions are warmly welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) (Bản tiếng Việt: [CONTRIBUTING-vi.md](CONTRIBUTING-vi.md)) and our [Code of Conduct](CODE_OF_CONDUCT.md) ([CODE_OF_CONDUCT.vi.md](CODE_OF_CONDUCT.vi.md)) for guidelines on reporting bugs, proposing features, and submitting pull requests.

## License

MIT. See [LICENSE](LICENSE).

NexKey is not affiliated with UniKey or with any of the input methods that
inspired it.
