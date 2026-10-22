#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_NAME="JIOServer"
PROJECT_TO_BUILD="$(realpath "$SCRIPT_DIR/..")"

BUILD_VERSION="$(<"$SCRIPT_DIR/Version.txt")"

PROJECT_PACKAGE_NAME="${PROJECT_NAME}_LinuxStatic_${BUILD_VERSION}"
BUILD_DIR="/mnt/DataLinux/Tmp/$PROJECT_PACKAGE_NAME"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
mkdir -p "$SCRIPT_DIR/0_Builds"

docker run --rm \
  -u "$(id -u)":"$(id -g)" \
  --mount type=bind,src="$PROJECT_TO_BUILD",dst=/src \
  --mount type=bind,src="$BUILD_DIR",dst=/build \
  -w /build \
  qt_static_builder \
  bash -lc '
    cmake -G Ninja -S /src/server -B /build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/qt/install &&
    cmake --build /build --target JIOServer &&
    strip "/build/JIOServer" &&
    upx "/build/JIOServer"
  '

rm -f "$SCRIPT_DIR/0_Builds/$PROJECT_PACKAGE_NAME.zip"
7z a -tzip \
  "$SCRIPT_DIR/0_Builds/$PROJECT_PACKAGE_NAME.zip" \
  "$BUILD_DIR/JIOServer" \
  "$SCRIPT_DIR/JIOServer.svg"
echo "Package: $SCRIPT_DIR/0_Builds/$PROJECT_PACKAGE_NAME.zip"
