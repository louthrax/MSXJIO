#!/usr/bin/env bash

set -euo pipefail

cd "$(dirname "$0")/Tmp"

rm -f *

zcc --allseg CODE --no-crt -nostdlib +z80 --sdcccall1 -mz80 -Cl-reloc-info -odriver ../driver.c 2>&1 | grep -v ": warning 283:" | \
awk '
/: error /  {print "\033[1;31m" $0 "\033[0m"; next}
/: warning / {print "\033[1;33m" $0 "\033[0m"; next}
{print}'

z88dk-z80asm -b -mz80 -reloc-info -o./jumper ../jumper.asm
rm ../jumper.o

zcc --allseg CODE --no-crt -nostdlib +z80 --sdcccall1 -mz80 -Cl-r0x100     -omain   ../main.c   2>&1 | grep -v ": warning 283:" | \
awk '
/: error /  {print "\033[1;31m" $0 "\033[0m"; next}
/: warning / {print "\033[1;33m" $0 "\033[0m"; next}
{print}'
