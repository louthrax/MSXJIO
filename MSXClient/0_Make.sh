#!/usr/bin/env bash

cd "$(dirname "$0")"

wine iccZ80.exe drv_jio.c -z9 -uu -a drv_jio_c.as
./clean_iar_asm.py drv_jio_c.as drv_jio_c.asm

rm -r -f ./obj rdate.inc
date +"db \"%Y-%m-%d\"" > rdate.inc
z88dk.z88dk-z80asm -b -d -l -m -DJIO -Oobj -o=rs_dos2.bin p1_main.asm p3_paging.asm driver.asm drv_jio.asm p0_kernel.asm
z88dk.z88dk-appmake +glue -b obj/rs_dos2 --filler 0xFF --clean

z88dk.z88dk-appmake +rom  -b obj/rs_dos2__.bin -o ./rs_dos2.rom -s 32768 --org 0

z88dk.z88dk-appmake +rom  -b obj/rs_dos2__.bin -o ./rs_dos2_64k.rom -s 65536 --org 16384 --fill 0xFF

dd if=/dev/zero bs=1 count=65536 | tr '\0' '\377' > rs_dos2_64k_NMS_8220.rom
dd if=rs_dos2.rom of=rs_dos2_64k_NMS_8220.rom bs=1 count=16384 seek=0 conv=notrunc
dd if=rs_dos2.rom of=rs_dos2_64k_NMS_8220.rom bs=1 skip=16384 count=16384 seek=32768 conv=notrunc

cp ./rs_dos2.rom /media/laurent/DataBackupNAS/msxftp/RSDISK
rm -r -f ./obj rdate.inc
