#!/usr/bin/env bash
set -euo pipefail

# Command line server (JIOServerCLI.pro), zip archive for Windows (on the Windows VM)

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

RunOnWindowsVM.sh "$SCRIPT_DIR" ./tools/Build_CLI_Windows.bat
