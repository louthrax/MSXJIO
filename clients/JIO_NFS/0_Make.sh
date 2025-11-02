#!/usr/bin/env bash

set -euo pipefail

cd "$(dirname "$0")/Tmp"

rm -f *

zcc -s --allseg CODE --no-crt -nostdlib +z80 --sdcccall1 -mz80 -Cl-reloc-info -odriver ../driver.c 2>&1 | grep -v ": warning 283:" | \
awk '
/: error /  {print "\033[1;31m" $0 "\033[0m"; next}
/: warning / {print "\033[1;33m" $0 "\033[0m"; next}
{print}'

grep '=' driver.sym | sed -E 's/^([A-Za-z0-9_]+)[[:space:]]*=[[:space:]]*\$([0-9A-Fa-f]+).*/#define driver_\1 0x\2/' > driver.h


z88dk-z80asm -b -mz80 -reloc-info -o./jumper ../jumper.asm
rm ../jumper.o

zcc --allseg CODE --no-crt -nostdlib +z80 --sdcccall1 -mz80 -Cl-r0x100     -omain   ../main.c   2>&1 | grep -v ": warning 283:" | \
awk '
/: error /  {print "\033[1;31m" $0 "\033[0m"; next}
/: warning / {print "\033[1;33m" $0 "\033[0m"; next}
{print}'

zcc -a --allseg CODE --no-crt -nostdlib +z80 --sdcccall1 -mz80 -Cl-reloc-info ../driver.c > /dev/null 2>&1
zcc -a --allseg CODE --no-crt -nostdlib +z80 --sdcccall1 -mz80 -Cl-r0x100     ../main.c  > /dev/null 2>&1

mv ../driver.c.asm .
mv ../main.c.asm .

cd ..

openmsx -machine Philips_NMS_8255 openMSX_CopyFiles.tcl