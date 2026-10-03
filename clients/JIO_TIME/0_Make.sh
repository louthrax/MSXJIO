#!/usr/bin/env bash

set -ex

cd "$(dirname "$0")"

rm -rf ./0_Builds
z88dk-z80asm -b -d -l -m -O0_Builds/obj -o=JIOTIME.COM jiotime.asm
cp ./0_Builds/obj/JIOTIME.COM ./0_Builds/
cp ./0_Builds/JIOTIME.COM /mnt/DataBackupNAS/msxftp/RSDISK
