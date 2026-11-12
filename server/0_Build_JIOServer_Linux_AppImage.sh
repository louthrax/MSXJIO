#!/usr/bin/env bash
set -euo pipefail

# JIOServer (graphical server): Linux AppImage.
# Built in the qt_dynamic_builder image of the Dockers project (Debian 11, Qt 6.9.0): runs on the x86_64
# distributions with glibc 2.31 or later. --arm64: for ARM64, in the qt_dynamic_builder_arm64 image
# (Ubuntu 24.04, emulated by qemu-user-static: much slower), with the Wayland plugins: runs on the ARM64
# distributions with glibc 2.39 or later.
# Usage: 0_Build_JIOServer_Linux_AppImage.sh [--arm64]

# the package name, the CMake target (and its executable), the desktop file and the icon of the
# project (SVG, rendered as a PNG: appimagetool needs a PNG for .DirIcon)
NAME=JIOServer
TARGET=JIOServer
DESKTOP_FILE=JIOServer.desktop
ICON=JIOServer.svg

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"   # mounted at /src (CMake runs git there)
SRC_DIR="/src${SCRIPT_DIR#"$REPO_DIR"}"                        # the project, in the container
VERSION="$(<"$SCRIPT_DIR/Version.txt")"

IMAGE=qt_dynamic_builder PLATFORM=linux/amd64 KIND=LinuxAppImage
if [[ ${1:-} == --arm64 ]]; then
  IMAGE=qt_dynamic_builder_arm64 PLATFORM=linux/arm64 KIND=LinuxAppImageARM64
fi

PACKAGE="${NAME}_${VERSION}_${KIND}"
BUILD_DIR="/mnt/DataLinux/Tmp/$PACKAGE"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR" "$SCRIPT_DIR/0_Builds"

# Wayland, when the Qt of the image has it: its platform plugins, and the wayland-egl client buffer
# integration (OpenGL contexts on Wayland), which linuxdeploy-plugin-qt only deploys for Wayland
# compositors: copied, its dependencies deployed by linuxdeploy (--deploy-deps-only).
docker run --rm --platform "$PLATFORM" -u "$(id -u):$(id -g)" \
  -e HOME=/tmp -e SRC_DIR="$SRC_DIR" -e TARGET="$TARGET" -e DESKTOP_FILE="$DESKTOP_FILE" -e ICON="$ICON" \
  --mount type=bind,src="$REPO_DIR",dst=/src \
  --mount type=bind,src="$BUILD_DIR",dst=/build \
  -w /build "$IMAGE" bash -lc '
    set -e
    cmake -G Ninja -S "$SRC_DIR" -B /build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/qt/install
    cmake --build /build --target "$TARGET"
    SELF_CALL=1 inkscape "$SRC_DIR/$ICON" --export-type=png -w 256 -h 256 --export-filename="/build/${ICON%.svg}.png"
    EGL_PLUGIN="$(/qt/install/bin/qmake -query QT_INSTALL_PLUGINS)/wayland-graphics-integration-client/libqt-plugin-wayland-egl.so"
    EGL=/build/AppDir/usr/plugins/wayland-graphics-integration-client
    WAYLAND=()
    if [ -f "$EGL_PLUGIN" ]; then
      mkdir -p "$EGL"
      cp "$EGL_PLUGIN" "$EGL/"
      export EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so"
      WAYLAND=(--deploy-deps-only="$EGL")
    fi
    LD_LIBRARY_PATH=/qt/install/lib QMAKE=/qt/install/bin/qmake linuxdeploy --appdir=/build/AppDir "${WAYLAND[@]}" \
      --executable="/build/$TARGET" --plugin=qt --output=appimage \
      --desktop-file="$SRC_DIR/$DESKTOP_FILE" -i "/build/${ICON%.svg}.png"
  '

cp -f "$BUILD_DIR"/*.AppImage "$SCRIPT_DIR/0_Builds/$PACKAGE.AppImage"
echo "Package: $SCRIPT_DIR/0_Builds/$PACKAGE.AppImage"
