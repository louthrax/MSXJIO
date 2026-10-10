#!/usr/bin/env bash
set -euo pipefail

# JIO-ROM.COM (JIO MSX-DOS 2 ROM started on a MSX-DOS 1 computer, ROM in a segment of the memory mapper)
# The ROM is assembled from the sources of clients/JIO_MSX-DOS with RAMROM (drv_jio_c.asm used as is, see
# 0_Build.sh of JIO_MSX-DOS for the IAR compiler step), then included in JIO-ROM.COM.
# JIO-ROM.COM in 0_Builds, intermediate files (ROM, map files) in 0_Temp

cd "$(dirname "$0")"

OBJ=0_Temp
rm -rf "$OBJ" ./0_Builds
mkdir -p "$OBJ/rom" ./0_Builds

# ROM (32 KB: page 1, kernel)
date +"db \"%Y-%m-%d\"" > "$OBJ/rom/rdate.inc"
( cd ../JIO_MSX-DOS &&
  z88dk-z80asm -b -d -l -m -I"../JIO_ROM/$OBJ/rom" -O"../JIO_ROM/$OBJ/rom" -o=jio_dos2_ram.bin -DJIO -DHYBRID -DRAMROM \
      p1_main.asm p3_paging.asm drv_jio.asm p0_kernel.asm )
# the driver must end before its CRC table (ORG 7E00H): the linker does not check it
TAIL=$(awk '/^__DRV_JIO_tail / { print strtonum("0x" substr($3,2)) }' "$OBJ/rom/jio_dos2_ram.map")
HEAD=$(awk '/^__DRV_CRCTAB_head / { print strtonum("0x" substr($3,2)) }' "$OBJ/rom/jio_dos2_ram.map")
if [ "$TAIL" -gt "$HEAD" ]; then
    echo "jio_dos2_ram: driver too big ($((TAIL - HEAD)) bytes over its CRC table)" >&2
    exit 1
fi
z88dk-appmake +glue -b "$OBJ/rom/jio_dos2_ram" --filler 0xFF --clean > /dev/null
z88dk-appmake +rom -b "$OBJ/rom/jio_dos2_ram__.bin" -o "$OBJ/jio_dos2_ram.rom" -s 32768 --org 0 > /dev/null

z88dk-z80asm -b -d -l -m -O"$OBJ" -o=JIO-ROM.COM jiorom.asm
cp "$OBJ/JIO-ROM.COM" ./0_Builds/
echo "0_Builds/JIO-ROM.COM"
