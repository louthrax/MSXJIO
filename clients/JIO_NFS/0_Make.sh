#!/usr/bin/env bash

set -ex

cd "$(dirname "$0")"

mkdir -p tmp
mkdir -p result
rm -f tmp/*

wine iccZ80.exe jio_nfs.c -z9 -uu -K -o tmp/jio_nfs.r01 -a tmp/jio_nfs.as
./clean_iar_asm.py tmp/jio_nfs.as tmp/jio_nfs.asm

z88dk-z80asm installer.asm -b -d -l -m -Otmp -o=../result/jio_nfs.com

mtools -c mcopy -i DOS2_tester/disk.dsk -D o -s result/jio_nfs.com ::
cp -f result/jio_nfs.com DOS2_tester/disk