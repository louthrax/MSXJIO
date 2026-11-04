#!/usr/bin/env zsh
set -euo pipefail

# JIOServerCLI (command line server): zip archive.
# Built on the macOS VM (0_Build_JIOServerCLI_macOS.sh), in a folder of the VM, from the project folder shared with
# the host.

NAME=JIOServerCLI
TARGET=JIOServerCLI

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

# an application bundle (macdeployqt only deploys bundles), without the GUI plugins, and a JIOServerCLI
# symlink to its executable, to run from a terminal
"$QT_DIR/bin/macdeployqt" "$BUILD_DIR/$TARGET.app" -no-plugins
mkdir "$BUILD_DIR/$PACKAGE"
mv "$BUILD_DIR/$TARGET.app" "$BUILD_DIR/$PACKAGE/"
ln -s "$TARGET.app/Contents/MacOS/$TARGET" "$BUILD_DIR/$PACKAGE/$TARGET"
"$BUILD_DIR/$PACKAGE/$TARGET" --version
rm -f "$SCRIPT_DIR/0_Builds/$PACKAGE.zip"
ditto -c -k --keepParent "$BUILD_DIR/$PACKAGE" "$SCRIPT_DIR/0_Builds/$PACKAGE.zip"
echo "Package: $SCRIPT_DIR/0_Builds/$PACKAGE.zip"
