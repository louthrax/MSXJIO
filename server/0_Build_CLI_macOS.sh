#!/usr/bin/env bash
set -euo pipefail

# Command line server (JIOServerCLI.pro), zip archive for macOS (on the macOS VM)

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

RunOnMacVM.sh "$SCRIPT_DIR" ./tools/Build_CLI_macOS.zsh
