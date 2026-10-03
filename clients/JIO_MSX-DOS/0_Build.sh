#!/usr/bin/env bash
set -euo pipefail

# JIO ROMs: MSX-DOS 1 (dos1) and MSX-DOS 2 (dos2, JIO drives and local drives, e.g. the internal floppy drive).
# Usage: 0_Build.sh [dos1] [dos2] [--no-iar]       (both ROMs by default)
#   --no-iar   drv_jio_c.asm used as is (no IAR compiler step, which needs wine and iccZ80.exe)
# ROMs in 0_Builds, intermediate files in 0_Builds/obj/JIO_MSX-DOS (repository root).

cd "$(dirname "$0")"

ROMS=()
IAR=1
for ARG in "$@"; do
    case "$ARG" in
        dos1|dos2) ROMS+=("$ARG") ;;
        --no-iar)  IAR=0 ;;
        *)         echo "Usage: $0 [dos1] [dos2] [--no-iar]" >&2; exit 2 ;;
    esac
done
[ ${#ROMS[@]} -gt 0 ] || ROMS=(dos1 dos2)

if [ "$IAR" = 1 ]; then
    export WINEDEBUG=-all
    wine iccZ80.exe drv_jio.c -z9 -uu -a drv_jio_c.as
    ./clean_iar_asm.py drv_jio_c.as drv_jio_c.asm
fi

mkdir -p 0_Builds
date +"db \"%Y-%m-%d\"" > rdate.inc
trap 'rm -f rdate.inc' EXIT

# ROM (assembled, 16 or 32 KB) and its 64 KB versions: page 1 (4000H) of a 64 KB ROM, and for the NMS 8220
# (16 KB ROM: at 0; 32 KB ROM: its first half at 4000H and C000H, its second half at 0 and 8000H)
build_rom() { # name size sources...
    local NAME=$1 SIZE=$2
    shift 2
    local OBJ=../../0_Builds/obj/JIO_MSX-DOS/$NAME
    local ROM=0_Builds/jio_$NAME

    rm -rf "$OBJ"
    mkdir -p "$OBJ"
    z88dk-z80asm -b -d -l -m -O"$OBJ" -o=jio_$NAME.bin "$@"
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
        dos1) build_rom dos1 16384 -DJIO -DIDEDOS1 dos1x.asm drv_jio.asm ;;
        dos2) build_rom dos2 32768 -DJIO -DHYBRID p1_main.asm p3_paging.asm drv_jio.asm p0_kernel.asm ;;
    esac
done
