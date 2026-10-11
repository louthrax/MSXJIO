#!/usr/bin/env bash
set -euo pipefail

# JIO-ROM.CAS (tape version of JIO-ROM.COM, MSX without MSX-DOS: BLOAD"CAS:",R)
# The ROM run in RAM is built by clients/JIO_MSX-DOS (target dos2ram, drv_jio_c.asm used as is: see its 0_Build.sh for
# the IAR compiler step).
# Tape image: binary file JIOROM (loader, jiocas.asm, at TAPE_ADR), then two data blocks read by the loader: ROM and
# kernel, 16 KB each, compressed (BitBuster, bitbuster.py): length, compressed data, 16-bit sum of the compressed
# bytes. The decompression in place in the segment is checked with the model of the depacker. Each block starts with
# the CAS header, at a multiple of 8.
# JIO-CAS.CAS (same loader assembled with SERVER): binary file JIOCAS only, the loader reads the ROM and its kernel
# from the JIO server: JIO-CAS.BIN (ROM run in RAM, 32 KB, then the 16-bit sums of the ROM and of the kernel), to put
# in the folder served as A:.
# JIO-ROM.CAS, JIO-CAS.CAS, JIO-CAS.BIN in 0_Builds, intermediate files in 0_Temp

cd "$(dirname "$0")"

OBJ=0_Temp
rm -rf "$OBJ" ./0_Builds
mkdir -p "$OBJ" ./0_Builds

../JIO_MSX-DOS/0_Build.sh dos2ram --no-iar > /dev/null

z88dk-z80asm -b -d -l -m -O"$OBJ" -o=JIOCAS.BIN jiocas.asm
python3 - "$OBJ/JIOCAS.BIN" ../JIO_MSX-DOS/0_Temp/jio_dos2_ram.rom ./0_Builds/JIO-ROM.CAS \
    "$(awk '/^TAPE_ADR / { print strtonum("0x" substr($3,2)) }' "$OBJ/JIOCAS.map")" \
    "$(awk '/^PACK_END / { print strtonum("0x" substr($3,2)) }' "$OBJ/JIOCAS.map")" "$(pwd)" <<'PYEOF'
import struct, sys
sys.dont_write_bytecode = True
sys.path.insert(0, sys.argv[6])
import bitbuster
loader = open(sys.argv[1], 'rb').read()
rom = open(sys.argv[2], 'rb').read()
start = int(sys.argv[4])
pack_end = int(sys.argv[5])
header = bytes([0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74])
cas = bytearray()
def block(data):
    cas.extend(header)
    cas.extend(data)
    cas.extend(bytes(-len(cas) % 8))
block(bytes([0xD0] * 10) + b'JIOROM')
block(struct.pack('<HHH', start, start + len(loader) - 1, start) + loader)
for name, data in (('ROM', rom[:16384]), ('kernel', rom[16384:32768])):
    packed = bitbuster.encode(data)
    # decompression in place: compressed data up to pack_end, output at 8000H
    mem = bytearray(0x10000)
    mem[pack_end - len(packed):pack_end] = packed
    end = bitbuster.depack(mem, pack_end - len(packed), 0x8000)
    if end != 0xC000 or mem[0x8000:0xC000] != data:
        sys.exit(name + ': decompression in place failed (increase PACK_END of jiocas.asm)')
    print('%s: %d -> %d bytes' % (name, len(data), len(packed)))
    block(struct.pack('<H', len(packed)) + packed + struct.pack('<H', sum(packed) & 0xFFFF))
open(sys.argv[3], 'wb').write(cas)
PYEOF
echo "0_Builds/JIO-ROM.CAS"

mkdir -p "$OBJ/server"
z88dk-z80asm -b -d -l -m -DSERVER -O"$OBJ/server" -o=JIOCAS.BIN jiocas.asm
python3 - "$OBJ/server/JIOCAS.BIN" ../JIO_MSX-DOS/0_Temp/jio_dos2_ram.rom ./0_Builds/JIO-CAS.CAS ./0_Builds/JIO-CAS.BIN \
    "$(awk '/^TAPE_ADR / { print strtonum("0x" substr($3,2)) }' "$OBJ/server/JIOCAS.map")" <<'PYEOF'
import struct, sys
loader = open(sys.argv[1], 'rb').read()
rom = open(sys.argv[2], 'rb').read()
start = int(sys.argv[5])
header = bytes([0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74])
cas = bytearray()
def block(data):
    cas.extend(header)
    cas.extend(data)
    cas.extend(bytes(-len(cas) % 8))
block(bytes([0xD0] * 10) + b'JIOCAS')
block(struct.pack('<HHH', start, start + len(loader) - 1, start) + loader)
open(sys.argv[3], 'wb').write(cas)
open(sys.argv[4], 'wb').write(rom[:32768] + struct.pack('<HH', sum(rom[:16384]) & 0xFFFF, sum(rom[16384:32768]) & 0xFFFF))
print('JIO-CAS: loader %d bytes' % len(loader))
PYEOF
echo "0_Builds/JIO-CAS.CAS, 0_Builds/JIO-CAS.BIN"
