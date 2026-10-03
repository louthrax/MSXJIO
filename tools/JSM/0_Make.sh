#!/usr/bin/env bash

set -ex

cd "$(dirname "$0")"

rm -rf ./0_Builds
z88dk-z80asm -b -d -l -m -O0_Builds/obj -o=JSM.BIN jsm.as
cp ./0_Builds/obj/JSM.BIN ./0_Builds/
cp ./0_Builds/JSM.BIN ./JSM.BAS /mnt/DataBackupNAS/msxftp/RSDISK
