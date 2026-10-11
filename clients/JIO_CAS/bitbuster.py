# BitBuster compression (Arjan Bakker), format of SofaCas (tools/sofacas: encode.cpp, depacker of g_acLoadROM.as)
#
# Bit stream: a byte of 8 flag bits, then the bytes of these 8 flags. Literal: bit 0, byte. Match (2 bytes at least,
# offset 1..2048): bit 1, byte (offset-1 if < 128, else bits 6-0 of offset-1 with bit 7 set, then bits 10-7 of
# offset-1 in 4 bits), length-2 in Elias gamma code. End: match with a length overflowing 16 bits.
# depack() is a model of the Z80 depacker of jiocas.asm (used by 0_Build.sh to check the in-place decompression).

MAX_OFFSET = 2048


class BitWriter:
    def __init__(self):
        self.out = bytearray()
        self.bits = 0
        self.count = 0
        self.pending = bytearray()

    def bit(self, b):
        if self.count == 8:
            self.out.append(self.bits)
            self.out += self.pending
            self.pending = bytearray()
            self.count = 0
        self.bits = ((self.bits << 1) | (1 if b else 0)) & 0xFF
        self.count += 1

    def byte(self, v):
        self.pending.append(v & 0xFF)

    def gamma(self, v):
        mask = 1
        v2 = v
        while v2:
            self.bit(1)
            mask <<= 1
            v2 = (v2 - 1) >> 1
        self.bit(0)
        v += 1
        mask >>= 1
        while mask:
            self.bit(v & mask)
            mask >>= 1


def encode(data):
    """Greedy longest match in the last MAX_OFFSET bytes"""
    w = BitWriter()
    n = len(data)
    pos = 0
    heads = {}

    def add(p):
        if p + 1 < n:
            heads.setdefault(data[p:p + 2], []).append(p)

    while pos < n:
        best_len, best_off = 1, 0
        for c in reversed(heads.get(data[pos:pos + 2], [])):
            if pos - c > MAX_OFFSET:
                break
            length = 0
            while pos + length < n and data[c + length] == data[pos + length]:
                length += 1
            if length > best_len:
                best_len, best_off = length, pos - c
        if best_len > 1:
            off = best_off - 1
            w.bit(1)
            if off > 127:
                w.byte(off | 128)
                for m in (8, 4, 2, 1):
                    w.bit((off >> 7) & m)
            else:
                w.byte(off)
            w.gamma(best_len - 2)
            for p in range(pos, pos + best_len):
                add(p)
            pos += best_len
        else:
            w.bit(0)
            w.byte(data[pos])
            add(pos)
            pos += 1
    # end: match with a length of 16 bits + 1
    w.bit(1)
    w.byte(0)
    for _ in range(16):
        w.bit(1)
    for _ in range(17):
        w.bit(0)
    while True:
        w.bit(0)
        if w.count == 1:
            break
    return bytes(w.out)


def depack(mem, src, dst):
    """Model of the Z80 depacker: data at mem[src], output at mem[dst]. Returns the end of the output."""
    a = 128

    def getbit():
        nonlocal a, src
        a <<= 1
        c = a >> 8
        a &= 0xFF
        if a:
            return c
        a = mem[src]
        src += 1
        c = a >> 7
        a = ((a << 1) | 1) & 0xFF
        return c

    while True:
        if not getbit():
            mem[dst] = mem[src]
            dst += 1
            src += 1
            continue
        c = mem[src]
        src += 1
        b = 0
        if c & 128:
            for _ in range(3):
                b = (b << 1) | getbit()
            if not getbit():
                c &= 127
        off = ((b << 8) | c) + 1
        hl = 1
        count = 1
        while getbit():
            count += 1
        for _ in range(count - 1):
            hl = (hl << 1) | getbit()
            if hl > 0xFFFF:
                return dst
        for _ in range(hl + 1):
            mem[dst] = mem[dst - off]
            dst += 1
