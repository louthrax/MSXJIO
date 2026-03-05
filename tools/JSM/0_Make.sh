#!/usr/bin/env bash

set -ex

cd "$(dirname "$0")"

rm -rf ./obj
z88dk-z80asm -b -d -l -m -Oobj -o=JSM.BIN jsm.as
cp ./obj/JSM.BIN .
cp ./obj/JSM.BIN ./JSM.BAS /mnt/DataBackupNAS/msxftp/RSDISK
