#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_NAME="$(basename "$(ls "$SCRIPT_DIR"/*.pro)" .pro)"
PROJECT_TO_BUILD="$(realpath "$SCRIPT_DIR/..")"

BUILD_HASH="$(git -C "$PROJECT_TO_BUILD" rev-parse HEAD)"
BUILD_VERSION="$(<"$PROJECT_TO_BUILD/server/Version.txt")"

PROJECT_PACKAGE_NAME="${PROJECT_NAME}_LinuxAppImage_${BUILD_VERSION}"
BUILD_DIR="/mnt/DataLinux/Tmp/$PROJECT_PACKAGE_NAME"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
mkdir -p "$SCRIPT_DIR/0_Builds"

docker run --rm -it \
  -u "$(id -u)":"$(id -g)" \
  -e PROJECT_NAME="$PROJECT_NAME" \
  --mount type=bind,src="$PROJECT_TO_BUILD",dst=/src \
  --mount type=bind,src="$BUILD_DIR",dst=/build \
  -w /build \
  qt_dynamic_builder \
  bash -lc '
    (
      /qt/install/bin/qmake "/src/server/${PROJECT_NAME}.pro" &&
      make -j"$(nproc)" &&
      QMAKE=/qt/install/bin/qmake linuxdeploy \
        --appdir=/build/AppDir \
        --executable="/build/${PROJECT_NAME}" \
        --plugin=qt \
        --output=appimage \
        --desktop-file="/src/server/${PROJECT_NAME}.desktop" \
        -i "/src/server/${PROJECT_NAME}.svg"
    ) || bash
  '

cp -f \
  "$BUILD_DIR/${PROJECT_NAME}.desktop-x86_64.AppImage" \
  "$SCRIPT_DIR/0_Builds/$PROJECT_PACKAGE_NAME.AppImage"