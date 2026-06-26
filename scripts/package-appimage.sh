#!/bin/bash
set -e

BUILD_DIR="build-appimage"
APPDIR="NexKey.AppDir"
APPIMAGE_DESKTOP="$BUILD_DIR/nexkey-appimage.desktop"
LINUXDEPLOY="./linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT_PLUGIN="./linuxdeploy-plugin-qt-x86_64.AppImage"

cmake -S . -B "$BUILD_DIR" -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel "$(nproc)"
DESTDIR="$(pwd)/$APPDIR" cmake --install "$BUILD_DIR"

QT_PLUGINS_DIR="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || qmake -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
if [ -n "$QT_PLUGINS_DIR" ] && [ -f "$QT_PLUGINS_DIR/platforms/libqwayland.so" ]; then
    mkdir -p "$APPDIR/usr/plugins/platforms"
    cp "$QT_PLUGINS_DIR/platforms/libqwayland.so" "$APPDIR/usr/plugins/platforms/"
fi

cp packaging/nexkey.desktop "$APPIMAGE_DESKTOP"
sed -i 's/^Icon=.*/Icon=nexkey-vi/' "$APPIMAGE_DESKTOP"

if [ ! -f "$LINUXDEPLOY" ]; then
    wget -q -O "$LINUXDEPLOY" https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fi

if [ ! -f "$LINUXDEPLOY_QT_PLUGIN" ]; then
    wget -q -O "$LINUXDEPLOY_QT_PLUGIN" https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
fi

chmod +x "$LINUXDEPLOY" "$LINUXDEPLOY_QT_PLUGIN"

APPIMAGE_EXTRACT_AND_RUN=1 \
QMAKE="$(command -v qmake6 || command -v qmake)" \
"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --desktop-file "$APPIMAGE_DESKTOP" \
    --icon-file nexkey-gui/resources/icons/nexkey-vi.svg \
    --plugin qt \
    --output appimage

echo "AppImage built."
