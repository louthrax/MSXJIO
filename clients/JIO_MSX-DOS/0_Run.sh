#!/usr/bin/env bash
set -euo pipefail

# MSX-DOS 2 JIO ROM (built by 0_Build.sh) in openMSX. Usage: 0_Run.sh [machine]   (Panasonic_FS-A1ST by default)

cd "$(dirname "$0")"

openmsx -machine "${1:-Panasonic_FS-A1ST}" -carta ./0_Builds/jio_dos2.rom -command "debug set_watchpoint write_io 0x2D"
