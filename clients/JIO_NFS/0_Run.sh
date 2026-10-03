#!/usr/bin/env bash
set -euo pipefail

# JIO.COM (built by 0_Build.sh) on MSX-DOS 2 in openMSX: floppy image 0_Builds/disk.dsk with MSX-DOS 2 and JIO.COM.
# Usage: 0_Run.sh [machine]   (Philips_NMS_8255 by default)

cd "$(dirname "$0")"

openmsx -machine Philips_NMS_8255 openMSX_CopyFiles.tcl
killall openmsx 2> /dev/null || true
openmsx -machine "${1:-Philips_NMS_8255}" -script openMSX_Run.tcl
