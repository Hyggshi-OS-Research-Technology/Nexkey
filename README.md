# NexKey

NexKey is a Vietnamese input method for Linux in the spirit of UniKey. The
keystroke engine is a dependency free C++23 library, the desktop integration is
a Fcitx5 addon, and a small Qt6 application provides the tray icon and the
settings window. Everything is built, tested and packaged with CMake.

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
                   +------------+------------+
                   |   nexkey-fcitx5 addon   |
                   |   Fcitx5 InputMethod    |
                   +------------+------------+
                                |
                   +------------+------------+
                   |   nexkey-core (C++23)   |
                   |   engine + charsets     |
                   +-------------------------+
```

## Features

- Telex (with or without the `w` and bracket shortcuts), VNI and VIQR.
- Tone placement for open, closed and triphthong nuclei, in both the modern and
  the traditional style.
- Output as precomposed Unicode, Unicode NFD, TCVN3 or VIQR.
- State based composition with smart backspace and double key escape: pressing
  the same tone or vowel modifier twice types the key literally.
- Words that are not plausible Vietnamese, such as `windows` or `jazz`, are
  shown exactly as typed instead of being mangled.
- Fcitx5 input method with a live VI/EN submode, a VN/EN panel icon and a
  configurable toggle key.
- Qt6 tray application with a settings dialog and a D-Bus service
  (`org.nexkey.Daemon`) for scripts and desktop shortcuts.
- Command line tool to test the engine, convert files between charsets and
  script the configuration.
- Debian package and a self contained AppImage, both attached to every CI run.

## Requirements

| Component | Requirement |
| --- | --- |
| Compiler | GCC 12+, Clang 15+ (C++23) |
| Build system | CMake 3.21+ |
| Core and CLI | none |
| Tray application | Qt 6.4+ (Core, Gui, Widgets, DBus) |
| Fcitx5 addon | Fcitx5 5.0+ development files |
| Tests | none |

Every optional component is detected at configure time. A plain build with no
Qt and no Fcitx5 still produces the library, the CLI and the test suite.

## Build

```bash
./scripts/build.sh            # Debug build in ./build
./scripts/test.sh             # build and run the test suite
./scripts/package-deb.sh      # CPack Debian package  -> build-deb/*.deb
./scripts/package-appimage.sh # AppImage with linuxdeploy -> dist/*.AppImage
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
| `NEXKEY_WARNINGS_AS_ERRORS` | `OFF` | Turn compiler warnings into errors |
| `NEXKEY_INSTALL_DOCS` | `ON` | Install the documentation |

## Install and use with Fcitx5

```bash
sudo cmake --install build --prefix /usr
```

Alternatively use the AppImage, which carries its own copy of the addon and the
tray application (see below).

Then open the Fcitx5 configuration, add **NexKey** to the input method list and
switch to it. The addon reads `~/.config/nexkey/nexkey.conf` and reloads it as
soon as the tray application or `nexkey config set` rewrites the file.

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
tests/                unit tests with a tiny built-in harness
packaging/            desktop entry, Debian maintainer scripts
scripts/              build, test and packaging helpers
docs/                 configuration and architecture notes
```

More detail is in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and
[docs/PACKAGING.md](docs/PACKAGING.md).

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
- IBus support was dropped in favour of the Fcitx5 addon, which is a real
  input method engine and not a placeholder binary;
- the configuration file is now `~/.config/nexkey/nexkey.conf` with documented
  keys (see [docs/CONFIGURATION.md](docs/CONFIGURATION.md));
- the CLI gained `convert`, `config`, `status` and `toggle` commands and prints
  English messages only;
- TCVN3, VIQR and Unicode NFD are supported as output charsets, the incomplete
  VNI Windows table was removed rather than kept half working.

## License

MIT. See [LICENSE](LICENSE).

NexKey is not affiliated with UniKey or with any of the input methods that
inspired it.
