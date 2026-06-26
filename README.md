# NexKey

NexKey is a Vietnamese input method for Linux inspired by Unikey. It is written
in C++20 with a dependency-free core engine, a CLI test surface, and install
assets for IBus, Fcitx5, Qt6 tray settings, Debian packages, and AppImage.

## Features

- Telex, VNI, and VIQR input methods
- Unicode UTF-8 output plus starter TCVN3 and VNI legacy conversion
- Stateful composition buffer with smart backspace
- CLI commands for status, toggling, conversion, and input-pipeline testing
- IBus and Fcitx5 descriptors
- Qt6 settings and tray application when Qt6 development files are installed

## Build

```bash
./scripts/build.sh
```

The root CMake file enables all targets by default, but skips optional IBus,
Fcitx5, and Qt6 targets when their development packages are unavailable.

## CLI Examples

```bash
build/nexkey-cli/nexkey test "tieengs Vieetj"
build/nexkey-cli/nexkey test --method vni "tie6ng1 Vie6t5"
build/nexkey-cli/nexkey convert --from vni-encoding --to utf8 < input.txt
```

## Packaging

```bash
./scripts/package-deb.sh
./scripts/package-appimage.sh
```

AppImage packaging downloads `linuxdeploy` when missing.
