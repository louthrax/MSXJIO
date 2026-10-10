#!/usr/bin/env bash
set -euo pipefail

# JIO-ROM.COM (JIO MSX-DOS 2 ROM started on a MSX-DOS 1 computer, ROM in a segment of the memory mapper)
# The ROM run in RAM is built by clients/JIO_MSX-DOS (target dos2ram, drv_jio_c.asm used as is: see its 0_Build.sh for
# the IAR compiler step), then included in JIO-ROM.COM.
# JIO-ROM.COM in 0_Builds, intermediate files in 0_Temp

cd "$(dirname "$0")"

OBJ=0_Temp
rm -rf "$OBJ" ./0_Builds
mkdir -p "$OBJ" ./0_Builds

../JIO_MSX-DOS/0_Build.sh dos2ram --no-iar > /dev/null

z88dk-z80asm -b -d -l -m -O"$OBJ" -o=JIO-ROM.COM jiorom.asm
cp "$OBJ/JIO-ROM.COM" ./0_Builds/
echo "0_Builds/JIO-ROM.COM"
