#!/usr/bin/env bash
set -euo pipefail

# Graphical server, macOS disk image (on the macOS VM)

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

RunOnMacVM.sh "$SCRIPT_DIR" ./tools/Build_macOS.zsh
