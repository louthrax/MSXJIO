#!/usr/bin/env bash

set -euo pipefail

# JIO.COM in 0_Builds, intermediate files in 0_Temp
cd "$(dirname "$0")"
SRC="$(pwd)"
OBJ="$SRC/0_Temp"
mkdir -p "$OBJ" "$SRC/0_Builds"
cd "$OBJ"

rm -f *

zcc -s --allseg CODE --no-crt -nostdlib +z80 --sdcccall1 -mz80 -Cl-reloc-info -odriver "$SRC/driver.c" 2>&1 | grep -v ": warning 283:" | \
awk '
/: error /  {print "\033[1;31m" $0 "\033[0m"; next}
/: warning / {print "\033[1;33m" $0 "\033[0m"; next}
{print}'

grep '=' driver.sym | sed -E 's/^([A-Za-z0-9_]+)[[:space:]]*=[[:space:]]*\$([0-9A-Fa-f]+).*/#define driver_\1 0x\2/' > driver.h


z88dk-z80asm -b -mz80 -reloc-info -o./jumper "$SRC/jumper.asm"
rm "$SRC/jumper.o"

z88dk-z80asm -b -mz80 -m -reloc-info -o./stub "$SRC/stub.asm"
rm "$SRC/stub.o"

zcc --allseg CODE --no-crt -nostdlib +z80 --sdcccall1 -mz80 -Cl-r0x100 -m  -omain   "$SRC/main.c"   2>&1 | grep -v ": warning 283:" | \
awk '
/: error /  {print "\033[1;31m" $0 "\033[0m"; next}
/: warning / {print "\033[1;33m" $0 "\033[0m"; next}
{print}'

zcc -a --allseg CODE --no-crt -nostdlib +z80 --sdcccall1 -mz80 -Cl-reloc-info "$SRC/driver.c" > /dev/null 2>&1
zcc -a --allseg CODE --no-crt -nostdlib +z80 --sdcccall1 -mz80 -Cl-r0x100     "$SRC/main.c"  > /dev/null 2>&1

mv "$SRC/driver.c.asm" .
mv "$SRC/main.c.asm" .

# The driver is relocated at install (16-bit addresses of driver.reloc): an address split in bytes or an
# uninitialized variable (placed at address 0, over the code) would not work
if grep -nE '& 0xFF\)|/ 256\)' driver.c.asm; then
    echo -e "\033[1;31mdriver.c: address split in bytes, not relocatable (use a pointer parameter)\033[0m"
    exit 1
fi
if grep -nE '^_g_[A-Za-z0-9_]+ += \$0000 ' driver.sym; then
    echo -e "\033[1;31mdriver.c: uninitialized variable at address 0 (initialize it)\033[0m"
    exit 1
fi
# Functions of stub.asm (BDOS functions 00H to 7FH mapped to the driver) must be the functions of g_aDosHandlers
# (driver.c)
if ! python3 - "$SRC/driver.c" "$SRC/stub.asm" <<'PYEOF'
import re, sys
table = open(sys.argv[1]).read()
table = table[table.index('g_aDosHandlers[] ='):]
table = table[:table.index('};')]
bitmap = [0] * 16
for function in re.findall(r'\{ 0x([0-9A-Fa-f]{2}),', table):
    if int(function, 16) >= 0x80:
        sys.exit(1)
    bitmap[int(function, 16) >> 3] |= 1 << (int(function, 16) & 7)
stub = open(sys.argv[2]).read()
stub = stub[stub.index('Functions:'):stub.index('SaveSP')]
sys.exit(0 if [int(byte, 16) for byte in re.findall(r'([0-9A-Fa-f]{2})h', stub)] == bitmap else 1)
PYEOF
then
    echo -e "\033[1;31mstub.asm: Functions does not match g_aDosHandlers of driver.c\033[0m"
    exit 1
fi
mv main JIO.COM
cp JIO.COM "$SRC/0_Builds/"
echo "$SRC/0_Builds/JIO.COM"
