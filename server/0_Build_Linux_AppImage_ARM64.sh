#!/bin/bash
set -euo pipefail

# JIOServer: Linux AppImage for ARM64 (aarch64), built in the qt_arm64_builder image of the Dockers project
# (Ubuntu 24.04 arm64, emulated by qemu-user-static: slower than the x86_64 build). Qt 6.9.0 for arm64 in
# /qt/install (as the x86_64 builds), with the Wayland platform plugins. Built on Ubuntu 24.04, the
# AppImage runs on the ARM64 distributions with glibc 2.39 or later (Ubuntu 24.04, Debian 13, Raspberry
# Pi OS 13, Fedora 40...).
# Needs the qt_arm64_builder image (Dockers/0_Build_qt_arm64_builder.sh) and the ARM emulation.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_NAME="JIOServer"
PROJECT_TO_BUILD="$(realpath "$SCRIPT_DIR/..")"     # project root (server/ is below it), mounted at /src

BUILD_VERSION="$(<"$SCRIPT_DIR/Version.txt")"

PROJECT_PACKAGE_NAME="${PROJECT_NAME}_LinuxAppImageARM64_${BUILD_VERSION}"
BUILD_DIR="/mnt/DataLinux/Tmp/$PROJECT_PACKAGE_NAME"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
mkdir -p "$SCRIPT_DIR/0_Builds"

# Dynamic Qt build in the container (CMake + Ninja), then packaged as an AppImage with linuxdeploy, with
# a PNG icon rendered from the SVG by Inkscape (appimagetool needs a PNG for .DirIcon). Wayland: its platform plugins, and the wayland-egl client buffer
# integration (OpenGL contexts on Wayland), which linuxdeploy-plugin-qt only deploys for Wayland
# compositors: copied, its dependencies deployed by linuxdeploy (--deploy-deps-only).
docker run --rm \
  --platform linux/arm64 \
  -u "$(id -u)":"$(id -g)" \
  -e PROJECT_NAME="$PROJECT_NAME" \
  -e HOME=/tmp \
  --mount type=bind,src="$PROJECT_TO_BUILD",dst=/src \
  --mount type=bind,src="$BUILD_DIR",dst=/build \
  -w /build \
  qt_arm64_builder \
  bash -lc '
    set -e
    QMAKE=/qt/install/bin/qmake
    QT_PLUGINS=$("$QMAKE" -query QT_INSTALL_PLUGINS)
    cmake -G Ninja -DCMAKE_PREFIX_PATH=/qt/install -S /src/server -B /build -DCMAKE_BUILD_TYPE=Release
    cmake --build /build --target JIOServer
    SELF_CALL=1 inkscape "/src/server/JIOServer.svg" --export-type=png -w 256 -h 256 --export-filename=/build/JIOServer.png
    EGL=/build/AppDir/usr/plugins/wayland-graphics-integration-client
    mkdir -p "$EGL"
    cp "$QT_PLUGINS"/wayland-graphics-integration-client/libqt-plugin-wayland-egl.so "$EGL/"
    LD_LIBRARY_PATH=/qt/install/lib QMAKE="$QMAKE" EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so" linuxdeploy \
      --appdir=/build/AppDir \
      --deploy-deps-only="$EGL" \
      --executable="/build/JIOServer" \
      --plugin=qt \
      --output=appimage \
      --desktop-file="/src/server/JIOServer.desktop" \
      -i "/build/JIOServer.png"
  '

cp -f "$(ls -t "$BUILD_DIR"/*-aarch64.AppImage | head -1)" "$SCRIPT_DIR/0_Builds/$PROJECT_PACKAGE_NAME.AppImage"
echo "Package: $SCRIPT_DIR/0_Builds/$PROJECT_PACKAGE_NAME.AppImage"
