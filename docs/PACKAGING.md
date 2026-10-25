# Packaging

NexKey ships two binary formats, both produced from the same build tree:

| Format | Script | Output |
| --- | --- | --- |
| Debian package | `scripts/package-deb.sh` | `build-deb/nexkey_<version>_<arch>.deb` |
| AppImage | `scripts/package-appimage.sh` | `dist/NexKey-<version>-x86_64.AppImage` |

Both are attached to the CI run of every push and pull request as the
`debian-package` and `appimage` artifacts.

## Debian package

```bash
./scripts/package-deb.sh
```

The script configures a Release build with `/usr` as the install prefix and
calls `cpack`. The package is built with CPack, configured in
`cmake/NexKeyPackaging.cmake`.

Contents (`<libdir>` is `lib` or the multiarch directory `lib/<triplet>`,
depending on the prefix CMake was configured with):

```
/usr/bin/nexkey-gui                            tray application
/usr/bin/nexkey                                command line tool
/usr/lib/<libdir>/fcitx5/nexkey.so             Fcitx5 input method addon
/usr/lib/<libdir>/nexkey/nexkey-ibus           IBus input method engine
/usr/lib/<libdir>/libnexkey-core.a             static core library
/usr/include/nexkey/*.hpp                      public headers
/usr/lib/<libdir>/cmake/NexKey/                CMake package files
/usr/share/fcitx5/addon/nexkey-addon.conf
/usr/share/fcitx5/inputmethod/nexkey.conf
/usr/share/ibus/component/nexkey.xml           IBus component description
/usr/share/applications/nexkey.desktop
/usr/share/nexkey/nexkey-autostart.desktop     template for the autostart entry
/usr/share/icons/hicolor/scalable/...
/usr/share/doc/NexKey/                         README, license, docs
/etc/xdg/autostart/nexkey.desktop              created by postinst, removed by postrm
```

IBus reads the engine location from `nexkey.xml`, so the installed copy is
rewritten at install time to the prefix the package really uses (`/usr` for the
`.deb`); a plain `cmake --install --prefix` therefore stays consistent as well.

The maintainer scripts do the following:

| Script | Work |
| --- | --- |
| `postinst` | installs `/etc/xdg/autostart/nexkey.desktop` from the shipped template, refreshes the desktop, icon, Fcitx5 and IBus caches, prints the next steps |
| `postrm` | removes the autostart entry, refreshes the caches |

### Dependencies

Runtime dependencies are computed from the built binaries with
`dpkg-shlibdeps` (`CPACK_DEBIAN_PACKAGE_SHLIBDEPS`), so the exact Qt6 and
Fcitx5 library packages end up in `Depends`. When `dpkg-shlibdeps` or `file` is
missing on the build host, CPack falls back to a fixed list derived from the
components that were enabled:

```
Depends: libc6, libstdc++6, libqt6core6t64 | libqt6core6, ...,
         libfcitx5core7 | libfcitx5core0, libibus-1.0-5
Recommends: fcitx5, fcitx5-frontend-gtk3 | fcitx5-frontend-gtk4 |
            fcitx5-frontend-qt5 | fcitx5-frontend-qt6
Suggests: fcitx5-config-qt
```

`Recommends` is where the actual input method daemons and the toolkit frontends
live: the backends are useless without Fcitx5 or IBus, but a user who only wants
the CLI can still install the package.

### Which backends go where

| Format | Fcitx5 addon | IBus engine |
| --- | --- | --- |
| `.deb` | yes | yes |
| AppImage | yes | no |

An IBus component file has to name the engine binary by absolute path
(`/usr/lib/<libdir>/nexkey/nexkey-ibus`), which does not exist inside an
AppImage, so the image is built with `-DNEXKEY_BUILD_IBUS=OFF` and users who
want IBus install the package.

### Autostart

The package shows the tray icon after login. The entry is a normal XDG autostart
file, so the settings dialog can hide it per user
(`~/.config/autostart/nexkey.desktop` with `Hidden=true`) and any desktop
environment can switch it off with its own startup tool.

## AppImage

```bash
./scripts/package-appimage.sh
```

Steps performed by the script:

1. Configure and build a Release tree in `build-appimage/` with `/usr` as the
   install prefix.
2. Install into `NexKey.AppDir/` with `DESTDIR`, then delete the development
   files (`usr/include`, `usr/lib/**/cmake`, `usr/share/doc`, `*.a`).
3. Download `linuxdeploy` and `linuxdeploy-plugin-qt` into `.tools/` (once).
4. Run linuxdeploy with the desktop file, the application icon, the custom
   `packaging/appimage/AppRun` and the Qt plugin, so Qt itself and the platform
   plugins are bundled.
5. Write `dist/NexKey-<version>-x86_64.AppImage` and print its size and SHA-256.

Options:

```
--build-dir <dir>     build tree to use        (default: build-appimage)
--output-dir <dir>    where the image is saved (default: dist)
--skip-build          reuse an existing build tree
--keep-appdir         do not delete the AppDir afterwards
--no-qt-plugin        skip the linuxdeploy Qt plugin
--allow-missing-gui   package without the Qt tray application
--help                print the help
```

The script fails early when `nexkey-gui` was not built, because an image with
only the CLI is rarely what people expect; pass `--allow-missing-gui` to get one
on purpose. It also fails when `qmake6` is missing while the Qt plugin is
enabled, which keeps the image from silently shipping without Qt libraries.

### Running the image

```bash
chmod +x dist/NexKey-2.0.0-x86_64.AppImage
./dist/NexKey-2.0.0-x86_64.AppImage             # tray icon + settings window
./dist/NexKey-2.0.0-x86_64.AppImage --settings  # open the settings window
```

`AppRun` prepares the environment before starting the application:

| Variable | Value |
| --- | --- |
| `PATH` | prepends `usr/bin` of the image |
| `XDG_DATA_DIRS` | prepends `usr/share` of the image |
| `QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH` | bundled Qt plugins, when present |
| `FCITX_ADDON_DIRS` | prepends the bundled addon directory, when present |

The addon lands in `usr/lib/fcitx5` on distributions that use a plain library
directory and in `usr/lib/<triplet>/fcitx5` on Debian based ones, so `AppRun`
probes both layouts.

`FCITX_ADDON_DIRS` is what makes the bundled Fcitx5 addon usable from the
image: start Fcitx5 from a session where that variable is exported (for example
from the same shell, or by wrapping your Fcitx5 autostart entry) and the addon
shows up as an available input method.

The configuration is still read from `$XDG_CONFIG_HOME/nexkey/nexkey.conf`, so
the AppImage and a distribution package share the same settings.

### Limitations

- AppImages are not signed. Verify the SHA-256 printed by the script against
  the artifact of the CI run you downloaded.
- The image targets x86_64 and was tested on Ubuntu 24.04 and Debian 12.
- Bundling works around the limits of the FUSE runtime with
  `APPIMAGE_EXTRACT_AND_RUN=1`, which the script sets while linuxdeploy runs.
  Running the produced image itself needs FUSE (`libfuse2t64` on Ubuntu 24.04)
  unless you extract it with `--appimage-extract`.

## CI

`.github/workflows/ci.yml` has three jobs:

| Job | What it does |
| --- | --- |
| `core` | Builds the library, the CLI and the tests without Qt or Fcitx5, warnings as errors |
| `full` | Builds everything with Qt6 and Fcitx5, runs the tests, builds and inspects the Debian package |
| `appimage` | Builds the AppImage, extracts it and checks the layout, the bundled libraries and the CLI |

The `full` job also checks the produced package: the tray binary, the Fcitx5
addon, the IBus engine, the component file, the desktop file and the autostart
template must be inside, `Depends` must mention the Qt6, Fcitx5 and IBus
libraries, the component XML must parse and name the right engine path, the
extracted CLI must run, and the IBus engine must exit cleanly with an error when
no daemon is running.

The AppImage job verifies the image contents before uploading, so a broken
bundle fails the build instead of reaching users:

```bash
./NexKey-*.AppImage --appimage-extract
test -x squashfs-root/AppRun
test -x squashfs-root/usr/bin/nexkey-gui
find squashfs-root/usr/lib -name nexkey.so -path '*fcitx5*'
./squashfs-root/usr/bin/nexkey methods
```

Failures are reported as `::error::` annotations, so a broken bundle shows the
missing path (and the contents of its directory) directly on the pull request.
