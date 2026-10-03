#!/usr/bin/env bash

# Command line server (JIOServerCLI.pro), zip archive for Windows

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

RunOnWindowsVM.sh "$SCRIPT_DIR" ./tools/build_CLI_Windows.bat
