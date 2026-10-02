#!/usr/bin/env bash

set -ex

cd "$(dirname "$0")"

rm -rf ./obj
z88dk-z80asm -b -d -l -m -Oobj -o=JIOTIME.COM jiotime.asm
cp ./obj/JIOTIME.COM .
cp ./JIOTIME.COM /mnt/DataBackupNAS/msxftp/RSDISK
