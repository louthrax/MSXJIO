#!/usr/bin/env bash
set -euo pipefail

# JIO-ROM.CAS (tape version of JIO-ROM.COM, MSX without MSX-DOS: BLOAD"CAS:",R)
# The ROM run in RAM is built by clients/JIO_MSX-DOS (target dos2ram, drv_jio_c.asm used as is: see its 0_Build.sh for
# the IAR compiler step).
# Tape image: binary file JIOROM (loader, jiocas.asm, at TAPE_ADR), then two data blocks of 16 KB read by the loader
# (ROM, kernel). Each block starts with the CAS header, at a multiple of 8.
# JIO-ROM.CAS in 0_Builds, intermediate files in 0_Temp

cd "$(dirname "$0")"

OBJ=0_Temp
rm -rf "$OBJ" ./0_Builds
mkdir -p "$OBJ" ./0_Builds

../JIO_MSX-DOS/0_Build.sh dos2ram --no-iar > /dev/null

z88dk-z80asm -b -d -l -m -O"$OBJ" -o=JIOCAS.BIN jiocas.asm
python3 - "$OBJ/JIOCAS.BIN" ../JIO_MSX-DOS/0_Temp/jio_dos2_ram.rom ./0_Builds/JIO-ROM.CAS \
    "$(awk '/^TAPE_ADR / { print strtonum("0x" substr($3,2)) }' "$OBJ/JIOCAS.map")" <<'PYEOF'
import struct, sys
loader = open(sys.argv[1], 'rb').read()
rom = open(sys.argv[2], 'rb').read()
start = int(sys.argv[4])
header = bytes([0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74])
cas = bytearray()
def block(data):
    cas.extend(header)
    cas.extend(data)
    cas.extend(bytes(-len(cas) % 8))
block(bytes([0xD0] * 10) + b'JIOROM')
block(struct.pack('<HHH', start, start + len(loader) - 1, start) + loader)
block(rom[:16384])
block(rom[16384:32768])
open(sys.argv[3], 'wb').write(cas)
PYEOF
echo "0_Builds/JIO-ROM.CAS"
