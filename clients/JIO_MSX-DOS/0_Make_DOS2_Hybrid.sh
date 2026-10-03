#!/usr/bin/env bash

# Hybrid ROM: local drives (FAT12, e.g. the internal floppy drive) and JIO drives together
set -ex

export WINEDEBUG=-all

cd "$(dirname "$0")"

wine iccZ80.exe drv_jio.c -z9 -uu -a drv_jio_c.as
./clean_iar_asm.py drv_jio_c.as drv_jio_c.asm

rm -r -f ./0_Builds/obj rdate.inc
mkdir -p 0_Builds
date +"db \"%Y-%m-%d\"" > rdate.inc
z88dk-z80asm -b -d -l -m -DJIO -DHYBRID -O0_Builds/obj -o=jio_dos2h.bin p1_main.asm p3_paging.asm drv_jio.asm p0_hybrid.asm
z88dk-appmake +glue -b 0_Builds/obj/jio_dos2h --filler 0xFF --clean

z88dk-appmake +rom  -b 0_Builds/obj/jio_dos2h__.bin -o ./0_Builds/jio_dos2_hybrid.rom -s 32768 --org 0

z88dk-appmake +rom  -b 0_Builds/obj/jio_dos2h__.bin -o ./0_Builds/jio_dos2_hybrid_64k.rom -s 65536 --org 16384 --fill 0xFF

dd if=/dev/zero bs=1 count=65536 | tr '\0' '\377' > 0_Builds/jio_dos2_hybrid_64k_NMS_8220.rom

dd if=0_Builds/jio_dos2_hybrid.rom of=0_Builds/jio_dos2_hybrid_64k_NMS_8220.rom bs=1 count=16384 skip=0     seek=16384 conv=notrunc
dd if=0_Builds/jio_dos2_hybrid.rom of=0_Builds/jio_dos2_hybrid_64k_NMS_8220.rom bs=1 count=16384 skip=0     seek=49152 conv=notrunc
dd if=0_Builds/jio_dos2_hybrid.rom of=0_Builds/jio_dos2_hybrid_64k_NMS_8220.rom bs=1 count=16384 skip=16384 seek=0     conv=notrunc
dd if=0_Builds/jio_dos2_hybrid.rom of=0_Builds/jio_dos2_hybrid_64k_NMS_8220.rom bs=1 count=16384 skip=16384 seek=32768 conv=notrunc

cp ./0_Builds/jio_dos2_hybrid.rom /mnt/DataBackupNAS/msxftp/RSDISK
rm -r -f ./0_Builds/obj rdate.inc
