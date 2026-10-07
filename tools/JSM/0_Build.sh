#!/usr/bin/env bash
set -euo pipefail

# JSM.BIN (serial monitor of the Bluetooth module, used by JSM.BAS)
# JSM.BIN in 0_Builds, intermediate files in 0_Temp

cd "$(dirname "$0")"

OBJ=0_Temp
rm -rf "$OBJ" ./0_Builds
mkdir -p "$OBJ" ./0_Builds
z88dk-z80asm -b -d -l -m -O"$OBJ" -o=JSM.BIN jsm.as
cp "$OBJ/JSM.BIN" ./0_Builds/
echo "0_Builds/JSM.BIN"
