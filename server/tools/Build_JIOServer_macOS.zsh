#!/usr/bin/env zsh
set -euo pipefail

# JIOServer (graphical server): macOS disk image, with the command line server.
# Built on the macOS VM (0_Build_JIOServer_macOS.sh), in a folder of the VM, from the project folder shared with
# the host.

NAME=JIOServer
TARGET=JIOServer

# ---- Tools of the VM (edit once) ----
QT_DIR="$HOME/Qt/6.8.3/macos"
# CMake and Ninja of the Qt Tools (not on PATH)
export PATH="$HOME/Qt/Tools/Ninja:$HOME/Qt/Tools/CMake/CMake.app/Contents/bin:$HOME/Qt/Tools/CMake/bin:$PATH"
# -------------------------------------

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(<"$SCRIPT_DIR/Version.txt")"
PACKAGE="${NAME}_macOS_${VERSION}"
BUILD_DIR="$HOME/Build"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR" "$SCRIPT_DIR/0_Builds"
# universal: Apple Silicon (arm64) and Intel (x86_64); the Qt of the VM is universal
cmake -G Ninja -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_DIR" \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build "$BUILD_DIR" --target "$TARGET"

# the command line server rides inside the bundle (shares its Qt frameworks), as the .pro build did
cmake --build "$BUILD_DIR" --target JIOServerCLI
cp -f "$BUILD_DIR/JIOServerCLI.app/Contents/MacOS/JIOServerCLI" "$BUILD_DIR/$TARGET.app/Contents/MacOS/JIOServerCLI"
"$QT_DIR/bin/macdeployqt" "$BUILD_DIR/$TARGET.app" -executable="$BUILD_DIR/$TARGET.app/Contents/MacOS/JIOServerCLI" -dmg
cp -f "$BUILD_DIR/$TARGET.dmg" "$SCRIPT_DIR/0_Builds/$PACKAGE.dmg"
echo "Package: $SCRIPT_DIR/0_Builds/$PACKAGE.dmg"
