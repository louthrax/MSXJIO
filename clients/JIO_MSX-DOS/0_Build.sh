#!/usr/bin/env bash
set -euo pipefail

# JIO ROMs: MSX-DOS 1 (dos1) and MSX-DOS 2 (dos2, JIO drives and local drives, e.g. the internal floppy drive).
# The serial lines are tried in turn at boot until the server answers (jio_ports.sh: hex = I/O port of a JIO cartridge
# (herraa1/msx-jio-cart-v1), used if the cartridge is found there; J1, J2 = joystick port 1 or 2):
#   dos1, dos2           JIO cartridge (I/O ports 00H, 20H, 30H probed), joystick port 2, joystick port 1
#   dos1safe, dos2safe   joystick ports 2 and 1 only: no I/O port written to probe the cartridge (other devices
#                        could be at these ports)
# The only difference is the list of the serial lines, environment:
#   JIO_PORTS       dos1, dos2, default "00 20 30 J2 J1", e.g. JIO_PORTS="00 J2" ./0_Build.sh dos2
#   JIOSAFE_PORTS   dos1safe, dos2safe, default "J2 J1", e.g. JIOSAFE_PORTS="J1" ./0_Build.sh dos2safe
# Usage: 0_Build.sh [dos1] [dos2] [dos1safe] [dos2safe] [--no-iar]       (all the ROMs by default)
#   --no-iar   drv_jio_c.asm used as is (no IAR compiler step, which needs wine and iccZ80.exe)
# ROMs in 0_Builds, intermediate and generated files (IAR compiler) in 0_Temp.

cd "$(dirname "$0")"

ROMS=()
IAR=1
for ARG in "$@"; do
    case "$ARG" in
        dos1|dos2|dos1safe|dos2safe) ROMS+=("$ARG") ;;
        --no-iar)  IAR=0 ;;
        *)         echo "Usage: $0 [dos1] [dos2] [dos1safe] [dos2safe] [--no-iar]" >&2; exit 2 ;;
    esac
done
[ ${#ROMS[@]} -gt 0 ] || ROMS=(dos1 dos2 dos1safe dos2safe)

mkdir -p 0_Builds 0_Temp

if [ "$IAR" = 1 ]; then
    export WINEDEBUG=-all
    wine iccZ80.exe drv_jio.c -z9 -uu -a 0_Temp/drv_jio_c.as
    mv -f drv_jio.r01 0_Temp/ 2> /dev/null || true
    ./clean_iar_asm.py 0_Temp/drv_jio_c.as drv_jio_c.asm
fi

# build date of the ROMs (INCLUDE "rdate.inc" in drv_jio.asm, found with -I0_Temp)
date +"db \"%Y-%m-%d\"" > 0_Temp/rdate.inc

# serial lines tried at boot (INCLUDE "jio_ports.inc" in drv_jio.asm, in the folder of each ROM)
./jio_ports.sh ${JIO_PORTS:-00 20 30 J2 J1} > /dev/null
./jio_ports.sh ${JIOSAFE_PORTS:-J2 J1} > /dev/null

# ROM (assembled, 16 or 32 KB) and its 64 KB versions: page 1 (4000H) of a 64 KB ROM, and for the NMS 8220
# (16 KB ROM: at 0; 32 KB ROM: its first half at 4000H and C000H, its second half at 0 and 8000H)
build_rom() { # name size "serial lines" sources...
    local NAME=$1 SIZE=$2 LINES=$3
    shift 3
    local OBJ=0_Temp/$NAME
    local ROM=0_Builds/jio_$NAME

    rm -rf "$OBJ"
    mkdir -p "$OBJ"
    ./jio_ports.sh $LINES > "$OBJ/jio_ports.inc"
    z88dk-z80asm -b -d -l -m -I"$OBJ" -I0_Temp -O"$OBJ" -o=jio_$NAME.bin "$@"
    # the driver must end before its CRC table (ORG 7E00H): the linker does not check it
    local TAIL HEAD
    TAIL=$(awk '/^__DRV_JIO_tail / { print strtonum("0x" substr($3,2)) }' "$OBJ/jio_$NAME.map")
    HEAD=$(awk '/^__DRV_CRCTAB_head / { print strtonum("0x" substr($3,2)) }' "$OBJ/jio_$NAME.map")
    if [ "$TAIL" -gt "$HEAD" ]; then
        echo "jio_$NAME: driver too big ($((TAIL - HEAD)) bytes over its CRC table)" >&2
        exit 1
    fi
    z88dk-appmake +glue -b "$OBJ/jio_$NAME" --filler 0xFF --clean
    z88dk-appmake +rom -b "$OBJ/jio_${NAME}__.bin" -o "$ROM.rom" -s "$SIZE" --org 0
    z88dk-appmake +rom -b "$OBJ/jio_${NAME}__.bin" -o "${ROM}_64k.rom" -s 65536 --org 16384 --fill 0xFF

    dd if=/dev/zero bs=1 count=65536 status=none | tr '\0' '\377' > "${ROM}_64k_NMS_8220.rom"
    if [ "$SIZE" = 16384 ]; then
        dd if="$ROM.rom" of="${ROM}_64k_NMS_8220.rom" bs=1 count=16384 skip=0     seek=0     conv=notrunc status=none
    else
        dd if="$ROM.rom" of="${ROM}_64k_NMS_8220.rom" bs=1 count=16384 skip=0     seek=16384 conv=notrunc status=none
        dd if="$ROM.rom" of="${ROM}_64k_NMS_8220.rom" bs=1 count=16384 skip=0     seek=49152 conv=notrunc status=none
        dd if="$ROM.rom" of="${ROM}_64k_NMS_8220.rom" bs=1 count=16384 skip=16384 seek=0     conv=notrunc status=none
        dd if="$ROM.rom" of="${ROM}_64k_NMS_8220.rom" bs=1 count=16384 skip=16384 seek=32768 conv=notrunc status=none
    fi
    echo "$ROM.rom, ${ROM}_64k.rom, ${ROM}_64k_NMS_8220.rom"
}

for ROM in "${ROMS[@]}"; do
    case "$ROM" in
        dos1)     build_rom dos1      16384 "${JIO_PORTS:-00 20 30 J2 J1}" -DJIO -DIDEDOS1 dos1x.asm drv_jio.asm ;;
        dos2)     build_rom dos2      32768 "${JIO_PORTS:-00 20 30 J2 J1}" -DJIO -DHYBRID p1_main.asm p3_paging.asm drv_jio.asm p0_kernel.asm ;;
        dos1safe) build_rom dos1_safe 16384 "${JIOSAFE_PORTS:-J2 J1}" -DJIO -DIDEDOS1 dos1x.asm drv_jio.asm ;;
        dos2safe) build_rom dos2_safe 32768 "${JIOSAFE_PORTS:-J2 J1}" -DJIO -DHYBRID p1_main.asm p3_paging.asm drv_jio.asm p0_kernel.asm ;;
    esac
done
