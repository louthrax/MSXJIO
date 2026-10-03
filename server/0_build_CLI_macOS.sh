#!/usr/bin/env bash

# Command line server (JIOServerCLI.pro), zip archive for macOS

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

RunOnMacVM.sh "$SCRIPT_DIR" ./tools/build_CLI_macOS.zsh
