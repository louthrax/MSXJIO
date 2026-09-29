#!/usr/bin/env bash

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

RunOnMacVM.sh "$SCRIPT_DIR" ./tools/build_macOS.zsh
