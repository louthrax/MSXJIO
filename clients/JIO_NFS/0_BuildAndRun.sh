#!/usr/bin/env bash
set -euo pipefail

# 0_Build.sh then 0_Run.sh. Usage: 0_BuildAndRun.sh [machine]   (Panasonic_FS-A1ST by default)

cd "$(dirname "$0")"

./0_Build.sh
./0_Run.sh "${1:-Panasonic_FS-A1ST}"
