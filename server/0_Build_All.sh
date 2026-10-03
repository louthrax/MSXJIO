#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

cd "$SCRIPT_DIR"
./0_Build_Linux_Static.sh

cd "$SCRIPT_DIR"
./0_Build_Linux_AppImage.sh

cd "$SCRIPT_DIR"
./0_Build_Android.sh

cd "$SCRIPT_DIR"
./0_Build_macOS.sh

cd "$SCRIPT_DIR"
./0_Build_Windows.sh

# command line server
cd "$SCRIPT_DIR"
./0_Build_CLI_Linux_Static.sh

cd "$SCRIPT_DIR"
./0_Build_CLI_macOS.sh

cd "$SCRIPT_DIR"
./0_Build_CLI_Windows.sh
