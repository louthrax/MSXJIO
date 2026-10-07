#!/usr/bin/env python3
"""Mock JIO server (COMMAND_BDOS file functions) for testing the RFS kernel in openMSX.

The openMSX Tcl bridge sends:
  'T' len16 data     bytes transmitted by the MSX
  'R' len16          MSX waits for an answer packet of len bytes
and receives for 'R': status byte (1 = data follows, 0 = time-out) + data.

_RAMD creates the RAM disk H: in a temporary directory (destroyed at RESET, removed at exit).

MOCK_IMAGE (environment): disk image mode, the disk image (no partitions) is served with the
COMMAND_DRIVE_* commands (sectors, CRC checked both ways) and no drive is served by COMMAND_BDOS.

MOCK_DRIVE (environment): drive letter of the served directory (default A).

MOCK_DATE (environment): answer of COMMAND_DATE_TIME ("YYYY-MM-DD HH:MM:SS", default: now), "none" = no answer.

MOCK_READONLY (environment): "Read only" button of the server, the served directories cannot be modified
(error .WPROT), the RAM disk stays writable.

MOCK_DROP (environment): the answer of the first BDOS request containing this text is not sent (lost answer, the
client sends the request again).

MOCK_TX_BLOCKS (environment): if not empty, "TX blocks" flag in the answer of COMMAND_DRIVE_INFO (Bluetooth link of the
C++ server: JIO.COM sends its large writes in blocks).
"""
import os, re, socket, struct, sys, shutil, datetime, tempfile, atexit, signal

# served drive (MOCK_DRIVE environment: drive letter, default A:)
ROOTS = {ord(os.environ.get('MOCK_DRIVE', 'A').upper()) - 65: sys.argv[1]}
IMAGE = os.environ.get('MOCK_IMAGE')  # disk image mode
if IMAGE:
    ROOTS.clear()
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 9876
LOG = open(sys.argv[3] if len(sys.argv) > 3 else '/dev/stdout', 'w', buffering=1)
RAM = 7                           # RAM disk drive (H:)
DROP = os.environ.get('MOCK_DROP', '').encode()  # answer not sent once (request containing this text)
signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))

FIBFMT = '<B13sBHHHIB6sI13sB'
assert struct.calcsize(FIBFMT) == 50

E_OK, E_IDRV, E_NOFIL, E_NODIR, E_DUPF, E_DIRNE = 0, 0xDB, 0xD7, 0xD6, 0xD3, 0xD0
E_DIRX, E_FILEX, E_IHAND = 0xCC, 0xCB, 0xC3
E_FILRO = 0xD1
E_DKFUL, E_RAMDX, E_NORAM = 0xD4, 0xBC, 0xDE
E_WPROT = 0xF8
READONLY = bool(os.environ.get('MOCK_READONLY'))


class Incomplete(Exception):
    pass


FLAG_RX_CRC, FLAG_TX_CRC = 1, 2
CMD_READ, CMD_WRITE, CMD_INFO, CMD_CHANGED = 16, 17, 18, 19
CMD_DATE_TIME = 23
CMD_LOG = 24
REPORTS = {1: 'write protected', 3: 'drive not ready', 5: 'CRC error', 11: 'write fault'}


def crc16(data, crc=0):
    """CRC-16-CCITT XModem"""
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


def drive_command(r, flags, cmd):
    """COMMAND_DRIVE_* on the disk image. The CRC covers the packet from the signature."""
    def check_crc():
        if flags & FLAG_TX_CRC:
            computed = crc16(r.buf[:r.pos])
            received = r.word()
            if computed != received:
                LOG.write('*** command CRC %04X, computed %04X\n' % (received, computed))

    def answer(data):
        return [data] + ([struct.pack('<H', crc16(data))] if flags & FLAG_RX_CRC else [])

    if cmd in (CMD_READ, CMD_WRITE):
        sector, count, address = struct.unpack('<IBH', r.take(7))
        part, sector = sector >> 24, sector & 0xFFFFFF
        if sector & 0x800000:
            sector &= 0xFFFF
        data = r.take(count * 512) if cmd == CMD_WRITE else b''
        check_crc()
        LOG.write('%s P%d %d x %d\n' % ('READ' if cmd == CMD_READ else 'WRITE', part, sector, count))
        with open(IMAGE, 'r+b') as f:
            f.seek(sector * 512)
            if cmd == CMD_READ:
                return answer((f.read(count * 512) + b'\0' * count * 512)[:count * 512])
            f.write(data)
        return [struct.pack('<H', 0x1111)]      # DRIVE_ANSWER_WRITE_OK
    check_crc()
    if cmd == CMD_INFO:
        LOG.write('INFO\n')
        info = b'\r\nMock disk image\r\n'
        return answer((bytes([FLAG_RX_CRC | FLAG_TX_CRC, 1, 0]) + info + b'\0' * 512)[:512])
    LOG.write('DISK CHANGED\n')
    return [struct.pack('<H', 0x5555)]          # DRIVE_ANSWER_DISK_UNCHANGED


class Reader:
    def __init__(self, buf):
        self.buf, self.pos = buf, 0

    def take(self, n):
        if self.pos + n > len(self.buf):
            raise Incomplete()
        d = self.buf[self.pos:self.pos + n]
        self.pos += n
        return d

    def byte(self):
        return self.take(1)[0]

    def word(self):
        return struct.unpack('<H', self.take(2))[0]

    def string(self):
        i = self.buf.find(b'\0', self.pos)
        if i < 0:
            raise Incomplete()
        s = self.buf[self.pos:i]
        self.pos = i + 1
        return s.decode('latin1')

    def path_or_fib(self):
        if self.pos >= len(self.buf):
            raise Incomplete()
        if self.buf[self.pos] == 0xFF:
            return None, list(struct.unpack(FIBFMT, self.take(50)))
        return self.string(), None


def dos_datetime(t):
    d = datetime.datetime.fromtimestamp(t)
    return ((d.hour << 11) | (d.minute << 5) | (d.second // 2),
            ((max(d.year, 1980) - 1980) << 9) | (d.month << 5) | d.day)


def mask_regex(mask):
    mask = mask.strip()
    if mask in ('', '*', '*.*'):
        return re.compile('.*')

    def part(p):
        rx = ''
        for i, c in enumerate(p):
            if c == '*':
                rx += '[^.]*'
            elif c == '?':
                rx += '[^.]?' if all(x in '?*' for x in p[i + 1:]) else '[^.]'
            else:
                rx += re.escape(c)
        return rx
    if '.' not in mask:
        return re.compile('^' + part(mask) + '$', re.I)
    n, e = mask.rsplit('.', 1)
    rx = '^' + part(n) + (('(\\.' + part(e) + ')?') if all(x in '?*' for x in e) else ('\\.' + part(e))) + '$'
    return re.compile(rx, re.I)


DOS_CHARS = set("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!#$%&'()-@^_`{}~")


def valid_dos_name(n):
    base, dot, ext = n.partition('.')
    if not base or len(base) > 8 or len(ext) > 3 or (dot and not ext) or '.' in ext:
        return False
    return all(c in DOS_CHARS for c in base + ext)


def dos_names(directory):
    """host name -> MSX-DOS 8.3 name (valid 8.3 names kept, others get an alias XXXXXX~N.EXT)"""
    try:
        names = sorted(os.listdir(directory), key=str.lower)
    except OSError:
        return {}
    out, used = {}, set()
    for n in names:
        if valid_dos_name(n) and n.upper() not in used:
            out[n] = n.upper()
            used.add(n.upper())
    part = lambda t, m: ''.join(c.upper() for c in t if c in DOS_CHARS and c != '~')[:m]
    for n in names:
        if n in out:
            continue
        i = n.rfind('.')
        base = part(n[:i] if i > 0 else n, 8) or '_'
        ext = part(n[i + 1:], 3) if i > 0 else ''
        k = 1
        while True:
            tail = '~%d' % k
            alias = base[:8 - len(tail)] + tail + ('.' + ext if ext else '')
            if alias not in used:
                break
            k += 1
        out[n] = alias
        used.add(alias)
    return out


MSX_ACCENTS = 'ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒáíóúñÑªº¿'     # MSX international character set 80H..A8H


def msx_chars(name):
    """host name -> MSX character set (as the C++ server): ASCII kept, accented letters, '?' for the others"""
    out = bytearray()
    for c in name:
        if 0x20 <= ord(c) < 0x7F:
            out.append(ord(c))
        elif c in MSX_ACCENTS:
            out.append(0x80 + MSX_ACCENTS.index(c))
        else:
            out.append({'¡': 0xAD, '«': 0xAE, '»': 0xAF}.get(c, ord('?')))
    return bytes(out)


def dos_name(path):
    n = os.path.basename(path)
    if n in ('.', '..'):
        return n
    return dos_names(os.path.dirname(path)).get(n, n.upper())


class Server:
    def __init__(self):
        self.arch_reset = set()     # files with the archive attribute reset by the MSX (kept at RESET, as the server)
        self.reset()

    def reset(self):
        self.requests = {}          # numbered BDOS requests: number -> (bytes, answers), the last 16
        self.files = {}
        self.cwd = {d: '' for d in range(8)}
        self.finds = {}
        self.next_find = 0
        self.cur = 0
        self.wpath = ''
        self.ram_destroy()

    def ram_destroy(self):
        if getattr(self, 'ram_segs', 0):
            root = ROOTS.pop(RAM) + '/'
            for h, f in list(self.files.items()):
                if os.path.abspath(f.name).startswith(root):
                    f.close()
                    del self.files[h]
            shutil.rmtree(root, True)
        self.ram_segs = 0
        self.cwd[RAM] = ''

    def ram_free(self):
        used = 0
        for dp, dn, fn in os.walk(ROOTS[RAM]):
            used += sum((os.path.getsize(os.path.join(dp, x)) + 511) & ~511 for x in fn)
        return max(self.ram_segs * 16384 - used, 0)

    def on_ram(self, f):
        return self.ram_segs and os.path.abspath(f.name).startswith(ROOTS[RAM] + '/')

    def wprot(self, p):
        """write protected host path ("Read only", the RAM disk stays writable)"""
        return READONLY and not (self.ram_segs and os.path.abspath(p).startswith(ROOTS[RAM] + '/'))

    def served(self, d):
        return d in ROOTS and os.path.isdir(ROOTS[d])

    @staticmethod
    def find_entry(directory, name):
        dnames = dos_names(directory)
        for e in dnames:
            if e.lower() == name.lower():
                return e
        for e, d in dnames.items():
            if d == name.upper():
                return e
        return name.upper()

    def resolve(self, p, default=None):
        d = self.cur if default is None else default
        p = p.replace('\\', '/')
        if len(p) >= 2 and p[1] == ':':
            d = ord(p[0].upper()) - 65
            p = p[2:]
        if not self.served(d):
            return None, None
        items = [] if p.startswith('/') else [x for x in self.cwd[d].split('/') if x]
        for it in [x for x in p.split('/') if x]:
            if it == '.':
                continue
            if it == '..':
                if items:
                    items.pop()
                continue
            items.append(it)
        path = ROOTS[d]
        for it in items:
            path = os.path.join(path, self.find_entry(path, it))
        return d, path

    def rel(self, d, path, long_names=False):
        r = os.path.relpath(path, ROOTS[d])
        if r == '.':
            return ''
        host, out = ROOTS[d], []
        for item in r.split('/'):
            host = os.path.join(host, item)
            out.append(item if long_names else dos_name(host))
        return '\\'.join(out)

    def target(self, path, fib):
        if fib is not None:
            p = self.finds.get(fib[9])
            if p is None:
                return E_NOFIL, None, None
            return E_OK, (fib[7] - 1 if fib[7] else self.cur), p
        d, p = self.resolve(path)
        if d is None:
            return E_IDRV, None, None
        return E_OK, d, p

    def attributes(self, p):
        # archive attribute: none on the host, the files reset by the MSX are kept in self.arch_reset (as the server)
        a = 0x10 if os.path.isdir(p) else (0 if os.path.abspath(p) in self.arch_reset else 0x20)
        if not os.access(p, os.W_OK):
            a |= 1
        return a

    def fib(self, p, d, fid=0, mask=b'', attrs=0, result=0):
        st = os.stat(p)
        t, dt = dos_datetime(st.st_mtime)
        name = dos_name(p).encode('latin1')[:12]
        size = 0 if os.path.isdir(p) else min(st.st_size, 0xFFFFFFFF)
        return [0xFF, name, self.attributes(p), t, dt, 0, size, d + 1, bytes([attrs, 0, 0, 0, 0, 0]), fid, mask, result]

    @staticmethod
    def empty_fib(result):
        return [0, b'', 0, 0, 0, 0, 0, 0, b'\0' * 6, 0, b'', result]

    @staticmethod
    def entries(directory, root):
        try:
            names = os.listdir(directory)
        except OSError:
            return []
        dirs = sorted([n for n in names if os.path.isdir(os.path.join(directory, n))], key=str.lower)
        files = sorted([n for n in names if not os.path.isdir(os.path.join(directory, n))], key=str.lower)
        if os.path.abspath(directory) != os.path.abspath(root):
            dirs = ['.', '..'] + dirs
        return dirs + files

    def search(self, directory, root, mask, attrs, after=None):
        rx = mask_regex(mask)
        ents = self.entries(directory, root)
        dnames = dos_names(directory)
        start = 0
        if after is not None:
            names = [e.lower() for e in ents]
            start = names.index(after.lower()) + 1 if after.lower() in names else len(ents)
        for n in ents[start:]:
            p = os.path.join(directory, n)
            if not rx.match(dnames.get(n, n)):
                continue
            if os.path.isdir(p) and not (attrs & 0x10):
                continue
            return p
        return None

    def set_find(self, p, d, fid, mask, attrs):
        if not fid or fid not in self.finds:
            self.next_find += 1
            fid = self.next_find
        self.finds[fid] = p
        dr = self.rel(d, os.path.dirname(p))
        self.wpath = (dr + '\\' if dr else '') + dos_name(p)
        dr = self.rel(d, os.path.dirname(p), True)
        self.lwpath = (dr + '\\' if dr else '') + os.path.basename(p)
        return self.fib(p, d, fid, mask, attrs)

    @staticmethod
    def split(path):
        p = path.replace('\\', '/')
        i = max(p.rfind('/'), p.rfind(':'))
        return p[:i + 1], p[i + 1:]

    def find_target(self, path, fib, name):
        if fib is not None:
            e, d, p = self.target(None, fib)
            return e, d, p, name
        dpart, item = self.split(path)
        d, p = self.resolve(dpart)
        if d is None:
            return E_IDRV, None, None, item
        return E_OK, d, p, item

    def rename_or_move(self, d, p, new, move):
        if self.wprot(p):
            return E_WPROT
        if move:
            dd, ndir = self.resolve(new, d)
            np = os.path.join(ndir, os.path.basename(p)) if ndir and os.path.isdir(ndir) else None
            if not np:
                return E_NODIR
        else:
            np = os.path.join(os.path.dirname(p), self.find_entry(os.path.dirname(p), new))
        if os.path.exists(np):
            return E_DUPF
        os.rename(p, np)
        old, newp = os.path.abspath(p), os.path.abspath(np)
        for a in list(self.arch_reset):
            if a == old or a.startswith(old + os.sep):
                self.arch_reset.discard(a)
                self.arch_reset.add(newp + a[len(old):])
        return E_OK

    def handle_drive(self, p):
        for d, root in ROOTS.items():
            if os.path.abspath(p).startswith(os.path.abspath(root) + os.sep):
                return d
        return self.cur

    def add_file(self, f):
        for h in range(128, 256):
            if h not in self.files:
                self.files[h] = f
                return h
        return 0

    def command(self, r):
        func = r.byte()
        if func == 0x1D:
            LOG.write('RESET\n')
            self.reset()
            return []
        if func == 0x18:
            m = sum(1 << d for d in range(8) if self.served(d))
            LOG.write('LOGIN %02X\n' % m)
            return [bytes([m])]
        if func == 0x0E:
            d = r.byte()
            LOG.write('SELDSK %d\n' % d)
            if d < 8:
                self.cur = d
            return [bytes([8])]
        if func == 0x1B:
            d = r.byte()
            d = d - 1 if d else self.cur
            if not self.served(d):
                return [b'\0' * 5]
            total, _, free = shutil.disk_usage(ROOTS[d])
            if d == RAM and self.ram_segs:
                total, free = self.ram_segs * 16384, self.ram_free()
            spc = 1
            while total // 512 // spc > 0x7FFF and spc < 2:      # as the C++ server: COMMAND2 shows up to 32767K
                spc *= 2
            return [struct.pack('<BHH', spc, min(total // 512 // spc, 0x7FFF), min(free // 512 // spc, 0x7FFF))]
        if func in (0x40, 0x42):
            path, fib = r.path_or_fib()
            name = r.string() if fib is not None else ''
            attrs = r.byte()
            tmpl = r.take(13).split(b'\0')[0].decode('latin1') if func == 0x42 else ''
            e, d, directory, item = self.find_target(path, fib, name)
            LOG.write('%s %r %s %r attr=%02X\n' % ('FFIRST' if func == 0x40 else 'FNEW', path, fib and self.finds.get(fib[9]), name, attrs))
            if func == 0x40:
                if not item:
                    item = '*'
                if e == E_OK and attrs & 8:
                    out = [0xFF, (b'RAM DISK' if d == RAM and self.ram_segs else ('JIO DRIVE %c' % (65 + d)).encode()), 8, 0, 0, 0, 0, d + 1, b'\0' * 6, 0, b'', 0]
                elif e == E_OK:
                    p = self.search(directory, ROOTS[d], item, attrs)
                    out = self.set_find(p, d, 0, item.upper().encode()[:12], attrs) if p else self.empty_fib(E_NOFIL)
                else:
                    out = self.empty_fib(e)
            else:
                if '?' in item or '*' in item or not item:
                    item = tmpl
                if e == E_OK and not os.path.isdir(directory):
                    e = E_NODIR
                out = self.empty_fib(e)
                if e == E_OK:
                    p = os.path.join(directory, self.find_entry(directory, item))
                    if os.path.exists(p):
                        if attrs & 0x80:
                            e = E_FILEX
                        elif os.path.isdir(p):
                            e = E_OK if attrs & 0x10 else E_DIRX
                        elif attrs & 0x10:
                            e = E_FILEX
                    if e == E_OK and not (attrs & 0x10 and os.path.isdir(p)) and self.wprot(p):
                        e = E_WPROT
                        out = self.empty_fib(e)
                    elif e == E_OK:
                        if attrs & 0x10:
                            os.makedirs(p, exist_ok=True)
                        else:
                            open(p, 'wb').close()
                        out = self.set_find(p, d, 0, b'', 0)
                    else:
                        # existing entry (.FILEX, .DIRX): returned in the FIB with the error
                        out = self.set_find(p, d, 0, b'', 0)
                        out[11] = e
            LOG.write('  -> %r %02X\n' % (out[1], out[11]))
            return [struct.pack(FIBFMT, *out)]
        if func == 0x41:
            fib = list(struct.unpack(FIBFMT, r.take(50)))
            d = fib[7] - 1 if fib[7] else self.cur
            cur = self.finds.get(fib[9])
            p = None
            if cur is not None:
                p = self.search(os.path.dirname(cur), ROOTS[d], fib[10].split(b'\0')[0].decode('latin1'), fib[8][0], os.path.basename(cur))
            if p:
                out = self.set_find(p, d, fib[9], fib[10], fib[8][0])
            else:
                # the FIB still refers to the last entry found (can still be opened)
                out = fib
                out[11] = E_NOFIL
            LOG.write('FNEXT -> %r %02X\n' % (out[1], out[11]))
            return [struct.pack(FIBFMT, *out)]
        if func in (0x43, 0x44):
            path, fib = r.path_or_fib()
            mode = r.byte()
            attrs = r.byte() if func == 0x44 else 0
            e, d, p = self.target(path, fib)
            LOG.write('%s %r mode=%02X attr=%02X -> %s\n' % ('OPEN' if func == 0x43 else 'CREATE', path, mode, attrs, p))
            h = 0xFF
            if e == E_OK:
                if func == 0x43:
                    if not os.path.exists(p):
                        e = E_NOFIL
                    elif os.path.isdir(p):
                        e = E_DIRX
                    else:
                        try:
                            f = open(p, 'rb' if mode & 1 or self.wprot(p) else 'r+b')
                        except OSError:
                            f = open(p, 'rb')
                        h = self.add_file(f) or 0xFF
                else:
                    if not os.path.isdir(os.path.dirname(p)):
                        e = E_NODIR
                    elif os.path.exists(p) and attrs & 0x80:
                        e = E_FILEX
                    elif attrs & 0x10:
                        if os.path.exists(p):
                            e = E_DIRX if os.path.isdir(p) else E_FILEX
                        elif self.wprot(p):
                            e = E_WPROT
                        else:
                            os.mkdir(p)
                    elif os.path.isdir(p):
                        e = E_DIRX
                    elif self.wprot(p):
                        e = E_WPROT
                    else:
                        h = self.add_file(open(p, 'w+b')) or 0xFF
                        self.arch_reset.discard(os.path.abspath(p))
            LOG.write('  -> %02X handle %02X\n' % (e, h))
            return [bytes([e, h])]
        if func == 0x45:
            h = r.byte()
            f = self.files.pop(h, None)
            LOG.write('CLOSE %02X\n' % h)
            if f:
                f.close()
                return [bytes([E_OK])]
            return [bytes([E_IHAND])]
        if func == 0x48:
            h, n = r.byte(), r.word()
            f = self.files.get(h)
            data = f.read(n) if f and not f.closed else b''
            e = E_OK if f else E_IHAND
            LOG.write('READ %02X %d -> %d\n' % (h, n, len(data)))
            return [struct.pack('<BH', e, len(data))] + ([data] if data else [])
        if func == 0x49:
            h, n = r.byte(), r.word()
            data = r.take(n)
            f = self.files.get(h)
            LOG.write('WRITE %02X %d\n' % (h, n))
            if not f:
                return [struct.pack('<BH', E_IHAND, 0)]
            if self.wprot(f.name):
                return [struct.pack('<BH', E_WPROT, 0)]
            if self.on_ram(f):
                pos, size = f.tell(), os.path.getsize(f.name)
                old = (size + 511) // 512
                extra = ((max(size, pos + n) + 511) // 512 - old) * 512
                free = self.ram_free()
                if extra > free:
                    data = data[:max(0, old * 512 + free - pos)]
            f.write(data)
            f.flush()
            self.arch_reset.discard(os.path.abspath(f.name))
            return [struct.pack('<BH', E_OK if len(data) == n else E_DKFUL, len(data))]
        if func == 0x68:
            b = r.byte()
            e = E_OK
            if b == 0:
                self.ram_destroy()
            elif b != 0xFF:
                if self.ram_segs or self.served(RAM):
                    e = E_RAMDX
                else:
                    ROOTS[RAM] = tempfile.mkdtemp(prefix='JIOServer_RAM_')
                    atexit.register(shutil.rmtree, ROOTS[RAM], True)
                    self.ram_segs = b
            m = sum(1 << d for d in range(8) if self.served(d))
            LOG.write('RAMD %02X -> %02X %d %02X\n' % (b, e, self.ram_segs, m))
            return [bytes([e, self.ram_segs, m])]
        if func == 0x4A:
            h, m = r.byte(), r.byte()
            off = struct.unpack('<i', r.take(4))[0]
            f = self.files.get(h)
            if not f:
                return [struct.pack('<BI', E_IHAND, 0)]
            cur = f.tell()
            f.seek(0, 2)
            size = f.tell()
            base = {0: 0, 1: cur, 2: size}.get(m, 0)
            f.seek(max(0, base + off))
            LOG.write('SEEK %02X %d %d -> %d\n' % (h, m, off, f.tell()))
            return [struct.pack('<BI', E_OK, f.tell())]
        if func == 0x4D:
            path, fib = r.path_or_fib()
            e, d, p = self.target(path, fib)
            LOG.write('DELETE %r %s\n' % (path, p))
            if e == E_OK:
                if not os.path.exists(p):
                    e = E_NOFIL
                elif self.wprot(p):
                    e = E_WPROT
                elif os.path.isdir(p):
                    try:
                        os.rmdir(p)
                    except OSError:
                        e = E_DIRNE
                elif not os.access(p, os.W_OK):
                    e = E_FILRO             # read-only attribute
                else:
                    os.remove(p)
                    self.arch_reset.discard(os.path.abspath(p))
            return [bytes([e])]
        if func in (0x4E, 0x4F):
            path, fib = r.path_or_fib()
            new = r.string()
            e, d, p = self.target(path, fib)
            LOG.write('%s %s -> %r\n' % ('RENAME' if func == 0x4E else 'MOVE', p, new))
            if e == E_OK:
                e = self.rename_or_move(d, p, new, func == 0x4F)
            return [bytes([e])]
        if func in (0x50, 0x55):
            if func == 0x50:
                path, fib = r.path_or_fib()
                e, d, p = self.target(path, fib)
            else:
                h = r.byte()
                f = self.files.get(h)
                e, p = (E_OK, f.name) if f else (E_IHAND, None)
            st, na = r.byte(), r.byte()
            if e == E_OK and not os.path.exists(p):
                e = E_NOFIL
            if e == E_OK and st and self.wprot(p):
                e = E_WPROT
            if e == E_OK and st and not os.path.isdir(p):
                mode = os.stat(p).st_mode
                os.chmod(p, (mode & ~0o222) if na & 1 else (mode | 0o200))
                if na & 0x20:
                    self.arch_reset.discard(os.path.abspath(p))
                else:
                    self.arch_reset.add(os.path.abspath(p))
            LOG.write('ATTR %s set=%d attr=%02X -> %02X\n' % (p, st, na, self.attributes(p) if e == E_OK else 0))
            return [bytes([e, self.attributes(p) if e == E_OK else 0])]
        if func in (0x51, 0x56):
            f = None
            if func == 0x51:
                path, fib = r.path_or_fib()
                e, d, p = self.target(path, fib)
            else:
                h = r.byte()
                f = self.files.get(h)
                e, p = (E_OK, f.name) if f else (E_IHAND, None)
            st, nt, nd = r.byte(), r.word(), r.word()
            if e == E_OK and not os.path.exists(p):
                e = E_NOFIL
            if e == E_OK and st and self.wprot(p):
                e = E_WPROT
            if e == E_OK and st and not os.path.isdir(p):
                if f is not None and func == 0x56:
                    f.flush()
                m = datetime.datetime(1980 + (nd >> 9), (nd >> 5) & 15, nd & 31, nt >> 11, (nt >> 5) & 63, (nt & 31) * 2)
                os.utime(p, (os.stat(p).st_atime, m.timestamp()))
                LOG.write('FTIME %s set %s\n' % (p, m))
            t, dt = dos_datetime(os.stat(p).st_mtime) if e == E_OK else (0, 0)
            return [struct.pack('<BHH', e, t, dt)]
        if func in (0x52, 0x53, 0x54):
            h = r.byte()
            new = r.string() if func != 0x52 else ''
            f = self.files.get(h)
            LOG.write('H-function %02X %02X %r %s\n' % (func, h, new, f.name if f else None))
            if not f:
                return [bytes([E_IHAND])]
            p = f.name
            if func == 0x52:
                if self.wprot(p):
                    return [bytes([E_WPROT])]
                if not os.access(p, os.W_OK):
                    return [bytes([E_FILRO])]
                f.close()
                del self.files[h]
                os.remove(p)
                return [bytes([E_OK])]
            e = self.rename_or_move(self.handle_drive(p), p, new, func == 0x54)
            return [bytes([e])]
        if func == 0x59:
            d = r.byte()
            d = d - 1 if d else self.cur
            cwd = self.cwd.get(d, '')
            s = ((self.rel(d, os.path.join(ROOTS[d], cwd)) if cwd else '') + '\0').encode('latin1')
            LOG.write('GETCD %d -> %r\n' % (d, s))
            return [bytes([len(s)]), s]
        if func == 0x5A:
            path, fib = r.path_or_fib()
            e, d, p = self.target(path, fib)
            LOG.write('CHDIR %r -> %s\n' % (path, p))
            if e == E_OK:
                if not os.path.isdir(p):
                    e = E_NODIR
                else:
                    rel = os.path.relpath(p, ROOTS[d])
                    self.cwd[d] = '' if rel == '.' else rel
            return [bytes([e])]
        if func == 0x5E:
            s = (self.wpath + '\0').encode('latin1')
            pos = s.rfind(b'\\') + 1
            LOG.write('WPATH -> %r\n' % s)
            return [bytes([0, pos, len(s)]), s]
        if func == 0xE0:
            # JIO_GET_LONG_NAME (JIO extension): sub-function, size of the buffer of the program, FIB (1) or drive
            # (3); answer: error, size of the string with its 0 (0 if error), string (MSX character set)
            sub, size = r.byte(), r.word()
            e, s = E_OK, None
            if sub == 1:
                fib = r.take(50)
                p = self.finds.get(struct.unpack_from('<I', fib, 32)[0])
                if fib[0] != 0xFF or p is None:
                    e = E_NOFIL
                else:
                    s = os.path.basename(p)
            elif sub == 2:
                s = getattr(self, 'lwpath', '')
            elif sub == 3:
                d = r.byte()
                d = d - 1 if d else self.cur
                if not self.served(d):
                    e = E_IDRV
                else:
                    cwd = self.cwd.get(d, '')
                    s = self.rel(d, os.path.join(ROOTS[d], cwd), True) if cwd else ''
            else:
                e = 0xDC                                    # .IBDOS
            if e == E_OK:
                s = (msx_chars(s) + b'\0')
                if len(s) > size:
                    e = 0xD8                                # .PLONG: buffer too small
            LOG.write('LONGNAME %d -> %02X %r\n' % (sub, e, s if e == E_OK else b''))
            if e != E_OK:
                return [bytes([e, 0, 0])]
            return [bytes([e]) + struct.pack('<H', len(s)), s]
        LOG.write('*** unsupported function %02X\n' % func)
        return []


def main():
    global DROP
    srv = Server()
    ls = socket.socket()
    ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    ls.bind(('127.0.0.1', PORT))
    ls.listen(1)
    conn, _ = ls.accept()
    stream = b''
    answers = []

    def recv_exact(n):
        d = b''
        while len(d) < n:
            c = conn.recv(n - len(d))
            if not c:
                raise EOFError()
            d += c
        return d

    try:
        while True:
            kind = recv_exact(1)
            n = struct.unpack('<H', recv_exact(2))[0]
            if kind == b'T':
                stream += recv_exact(n)
                while True:
                    i = stream.find(b'JIO')
                    if i < 0:
                        stream = stream[-2:]
                        break
                    stream = stream[i:]
                    r = Reader(stream)
                    try:
                        r.take(3)
                        flags = r.byte()
                        cmd = r.byte()
                        if IMAGE and CMD_READ <= cmd <= CMD_CHANGED:
                            out = drive_command(r, flags, cmd)
                        elif cmd == CMD_INFO:
                            # directories (JIO.COM at install): flags of the server ("Auto retry", MOCK_AUTORETRY,
                            # default on as the C++ server; "TX blocks" of a Bluetooth link, MOCK_TX_BLOCKS), no drive,
                            # description
                            retry = os.environ.get('MOCK_AUTORETRY', '1') != '0'
                            blocks = bool(os.environ.get('MOCK_TX_BLOCKS'))
                            LOG.write('INFO (auto retry %s%s)\n' % ('on' if retry else 'off', ', TX blocks' if blocks else ''))
                            info = b'\r\nMock server: directories\r\n'
                            data = (bytes([(8 if retry else 0) | (32 if blocks else 0), 0, 0]) + info + b'\0' * 512)[:512]
                            out = [data] + ([struct.pack('<H', crc16(data))] if flags & FLAG_RX_CRC else [])
                        elif cmd == CMD_LOG:
                            LOG.write('MSX: %s\n' % r.string())
                            out = []
                        elif cmd == CMD_DATE_TIME:
                            date = os.environ.get('MOCK_DATE', '')
                            LOG.write('DATE TIME %s\n' % date)
                            if date == 'none':
                                out = []
                            else:
                                t = datetime.datetime.strptime(date, '%Y-%m-%d %H:%M:%S') if date else datetime.datetime.now()
                                out = [struct.pack('<HBBBBB', t.year, t.month, t.day, t.hour, t.minute, t.second)]
                        elif cmd in REPORTS:
                            LOG.write('*** report: %s\n' % REPORTS[cmd])
                            stream = stream[r.pos:]
                            continue
                        elif cmd != 22:
                            LOG.write('*** non BDOS command %d\n' % cmd)
                            stream = stream[r.pos:]
                            continue
                        else:
                            # numbered request (flags not 0) sent again with the same bytes: answers sent again,
                            # not executed again (as the server)
                            start = r.pos
                            prev = srv.requests.get(flags) if flags else None
                            if prev and prev[0].startswith(stream[start:]) and len(stream) - start < len(prev[0]):
                                raise Incomplete()
                            if prev and stream[start:start + len(prev[0])] == prev[0]:
                                r.pos = start + len(prev[0])
                                LOG.write('RESENT %d (answer sent again)\n' % flags)
                                out = list(prev[1])
                            else:
                                out = srv.command(r)
                                if flags:
                                    srv.requests.pop(flags, None)
                                    srv.requests[flags] = (bytes(stream[start:r.pos]), list(out))
                                    while len(srv.requests) > 16:
                                        srv.requests.pop(next(iter(srv.requests)))
                            if DROP and DROP in stream[:r.pos]:
                                LOG.write('DROPPED (answer not sent)\n')
                                DROP = b''
                                out = []
                    except Incomplete:
                        break
                    stream = stream[r.pos:]
                    answers += out
            else:
                if answers:
                    a = answers.pop(0)
                    if len(a) != n:
                        LOG.write('*** answer size mismatch: client waits %d, answer %d\n' % (n, len(a)))
                    a = (a + b'\0' * n)[:n]
                    conn.sendall(b'\1' + a)
                else:
                    conn.sendall(b'\0')
    except EOFError:
        pass


main()
