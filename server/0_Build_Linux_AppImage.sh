#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_NAME="JIOServer"
PROJECT_TO_BUILD="$(realpath "$SCRIPT_DIR/..")"     # project root (server/ is below it), mounted at /src

BUILD_VERSION="$(<"$SCRIPT_DIR/Version.txt")"

PROJECT_PACKAGE_NAME="${PROJECT_NAME}_LinuxAppImage_${BUILD_VERSION}"
BUILD_DIR="/mnt/DataLinux/Tmp/$PROJECT_PACKAGE_NAME"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
mkdir -p "$SCRIPT_DIR/0_Builds"

docker run --rm \
  -u "$(id -u)":"$(id -g)" \
  --mount type=bind,src="$PROJECT_TO_BUILD",dst=/src \
  --mount type=bind,src="$BUILD_DIR",dst=/build \
  -w /build \
  qt_dynamic_builder \
  bash -lc '
    cmake -G Ninja -S /src/server -B /build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/qt/install &&
    cmake --build /build --target JIOServer &&
    QMAKE=/qt/install/bin/qmake linuxdeploy \
      --appdir=/build/AppDir \
      --executable="/build/JIOServer" \
      --plugin=qt \
      --output=appimage \
      --desktop-file="/src/server/JIOServer.desktop" \
      -i "/src/server/JIOServer.svg"
  '

cp -f \
  "$BUILD_DIR/JIOServer.desktop-x86_64.AppImage" \
  "$SCRIPT_DIR/0_Builds/$PROJECT_PACKAGE_NAME.AppImage"
echo "Package: $SCRIPT_DIR/0_Builds/$PROJECT_PACKAGE_NAME.AppImage"
