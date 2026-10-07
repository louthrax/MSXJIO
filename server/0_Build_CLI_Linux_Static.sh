#!/bin/bash
set -euo pipefail

# Command line server (JIOServerCLI.pro), static Linux executable

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_NAME="JIOServerCLI"
PROJECT_TO_BUILD="$(realpath "$SCRIPT_DIR/..")"

BUILD_HASH="$(git -C "$PROJECT_TO_BUILD" rev-parse HEAD)"
BUILD_VERSION="$(<"$PROJECT_TO_BUILD/server/Version.txt")"

PROJECT_PACKAGE_NAME="${PROJECT_NAME}_LinuxStatic_${BUILD_VERSION}"
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
  qt_static_builder \
  bash -lc '
    /qt/install/bin/qmake "/src/server/${PROJECT_NAME}.pro" CONFIG+=static DEFINES+=QT_STATIC_BUILD &&
    make -j"$(nproc)" || bash
  '

rm -f "$SCRIPT_DIR/0_Builds/$PROJECT_PACKAGE_NAME.zip"
7z a -tzip \
  "$SCRIPT_DIR/0_Builds/$PROJECT_PACKAGE_NAME.zip" \
  "$BUILD_DIR/$PROJECT_NAME"
