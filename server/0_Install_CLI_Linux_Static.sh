#!/bin/bash
set -euo pipefail

# Builds (0_Build_CLI_Linux_Static.sh) and installs the static Linux command line server (JIOServerCLI) in ~/bin

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_NAME="JIOServerCLI"
BUILD_VERSION="$(<"$SCRIPT_DIR/Version.txt")"
PACKAGE="$SCRIPT_DIR/0_Builds/${PROJECT_NAME}_LinuxStatic_${BUILD_VERSION}.zip"
INSTALL_DIR="$HOME/bin"

"$SCRIPT_DIR/0_Build_CLI_Linux_Static.sh"

mkdir -p "$INSTALL_DIR"
7z e -so "$PACKAGE" "$PROJECT_NAME" > "$INSTALL_DIR/$PROJECT_NAME.new"
chmod +x "$INSTALL_DIR/$PROJECT_NAME.new"
mv -f "$INSTALL_DIR/$PROJECT_NAME.new" "$INSTALL_DIR/$PROJECT_NAME"

echo "$PROJECT_NAME $BUILD_VERSION installed in $INSTALL_DIR"
