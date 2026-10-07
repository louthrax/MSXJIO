#!/usr/bin/env bash
set -euo pipefail

# Graphical server, Windows installer (on the Windows VM)

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

RunOnWindowsVM.sh "$SCRIPT_DIR" ./tools/Build_Windows.bat
