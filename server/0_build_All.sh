#!/bin/bash

set -ex

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

cd "$SCRIPT_DIR"
./0_build_Linux_Static.sh

cd "$SCRIPT_DIR"
./0_build_Linux_AppImage.sh

cd "$SCRIPT_DIR"
./0_build_Android.sh

cd "$SCRIPT_DIR"
./0_build_macOS.sh

cd "$SCRIPT_DIR"
./0_build_Windows.sh

# command line server
cd "$SCRIPT_DIR"
./0_build_CLI_Linux_Static.sh

cd "$SCRIPT_DIR"
./0_build_CLI_macOS.sh

cd "$SCRIPT_DIR"
./0_build_CLI_Windows.sh
