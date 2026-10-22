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
# In both tape files, the loader is compressed (BitBuster): the BLOAD file is the compressed loader then unpack.asm,
# which unpacks it to TAPE_ADR and starts it (pack_loader).
# JIO-ROM.CAS, JIO-CAS.CAS, JIO-CAS.BIN in 0_Builds, intermediate files in 0_Temp

cd "$(dirname "$0")"

OBJ=0_Temp
rm -rf "$OBJ" ./0_Builds
mkdir -p "$OBJ" ./0_Builds

../JIO_MSX-DOS/0_Build.sh dos2ram --no-iar > /dev/null

# BLOAD file of a loader (directory with JIOCAS.BIN and JIOCAS.map): JIOCAS.BLD (start, end and execution addresses,
# compressed loader, unpack.asm ending just below TAPE_ADR), decompression checked with the model of the depacker
pack_loader() { # directory
    python3 - "$1" "$(awk '/^TAPE_ADR / { print strtonum("0x" substr($3,2)) }' "$1/JIOCAS.map")" "$(pwd)" <<'PYEOF'
import struct, subprocess, sys
sys.dont_write_bytecode = True
sys.path.insert(0, sys.argv[3])
import bitbuster
d, tape = sys.argv[1], int(sys.argv[2])
loader = open(d + '/JIOCAS.BIN', 'rb').read()
packed = bitbuster.encode(loader)
def stub(pack, start):
    subprocess.run(['z88dk-z80asm', '-b', '-O' + d, '-o=UNPACK.BIN', '-DTAPE_ADR=%d' % tape, '-DPACK_ADR=%d' % pack,
                    '-DSTUB_ADR=%d' % start, 'unpack.asm'], check=True)
    return open(d + '/UNPACK.BIN', 'rb').read()
size = len(stub(0, 0))
pack = tape - size - len(packed)
code = stub(pack, pack + len(packed))
mem = bytearray(0x10000)
mem[pack:pack + len(packed)] = packed
end = bitbuster.depack(mem, pack, tape)
if end != tape + len(loader) or mem[tape:end] != loader:
    sys.exit('loader: decompression failed')
body = packed + code
open(d + '/JIOCAS.BLD', 'wb').write(struct.pack('<HHH', pack, pack + len(body) - 1, pack + len(packed)) + body)
print('loader: %d -> %d bytes (+ %d bytes to unpack it)' % (len(loader), len(packed), len(code)))
PYEOF
}

z88dk-z80asm -b -d -l -m -O"$OBJ" -o=JIOCAS.BIN jiocas.asm
pack_loader "$OBJ"
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
block(open(sys.argv[1][:-4] + '.BLD', 'rb').read())
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
pack_loader "$OBJ/server"
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
block(open(sys.argv[1][:-4] + '.BLD', 'rb').read())
open(sys.argv[3], 'wb').write(cas)
open(sys.argv[4], 'wb').write(rom[:32768] + struct.pack('<HH', sum(rom[:16384]) & 0xFFFF, sum(rom[16384:32768]) & 0xFFFF))
print('JIO-CAS: loader %d bytes' % len(loader))
PYEOF
echo "0_Builds/JIO-CAS.CAS, 0_Builds/JIO-CAS.BIN"

# BIN built in the server (COMMAND_BIN, served to JIO-CAS.CAS when no other BIN is chosen) and JIO-CAS.CAS (played
# on the audio output by the graphical server), if the server is there (not in the copy of the sources built by the
# tests)
if [ -d ../../server/bin ]; then
    cp ./0_Builds/JIO-CAS.BIN ./0_Builds/JIO-CAS.CAS ../../server/bin/
    echo "../../server/bin/JIO-CAS.BIN, ../../server/bin/JIO-CAS.CAS"
fi
