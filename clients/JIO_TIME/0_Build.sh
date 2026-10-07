#!/usr/bin/env bash
set -euo pipefail

# JIOTIME.COM (date and time of the MSX from the JIO server)
# JIOTIME.COM in 0_Builds, intermediate files in 0_Temp

cd "$(dirname "$0")"

OBJ=0_Temp
rm -rf "$OBJ" ./0_Builds
mkdir -p "$OBJ" ./0_Builds
z88dk-z80asm -b -d -l -m -O"$OBJ" -o=JIOTIME.COM jiotime.asm
cp "$OBJ/JIOTIME.COM" ./0_Builds/
echo "0_Builds/JIOTIME.COM"
