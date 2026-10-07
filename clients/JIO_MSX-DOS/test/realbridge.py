#!/usr/bin/env python3
"""Adapter between the openMSX bridge (tcl/bridge.tcl) and the real JIO server (JIOServerCLI).

The bridge sends framed requests on TCP (same as for mockserver.py):
  'T' len16 data     bytes transmitted by the MSX: written to the pseudo terminal
  'R' len16          MSX waits for an answer packet of len bytes: the next packet of the server is read from the
                     pseudo terminal (FFh.. F0h sync, then len bytes), answered with status 1 + data, or status 0
                     if the server sends nothing (time-out)
The server opens the slave side of the pseudo terminal as its serial port (--port).

Usage: realbridge.py <tcp port> <file receiving the pseudo terminal path> [time-out in seconds]
"""
import os
import pty
import select
import socket
import struct
import sys
import tty

PORT = int(sys.argv[1])
PATH_FILE = sys.argv[2]
TIMEOUT = float(sys.argv[3]) if len(sys.argv) > 3 else 3.0

master, slave = pty.openpty()
tty.setraw(slave)
tty.setraw(master)
with open(PATH_FILE, 'w') as f:
    f.write(os.ttyname(slave))

ls = socket.socket()
ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
ls.bind(('127.0.0.1', PORT))
ls.listen(1)
conn, _ = ls.accept()


def recv_exact(n):
    d = b''
    while len(d) < n:
        c = conn.recv(n - len(d))
        if not c:
            raise EOFError()
        d += c
    return d


pending = b''


def read_byte(timeout):
    """Next byte of the server, None after the time-out"""
    global pending
    if not pending:
        r, _, _ = select.select([master], [], [], timeout)
        if not r:
            return None
        try:
            pending = os.read(master, 4096)
        except OSError:
            return None
        if not pending:
            return None
    b = pending[0]
    pending = pending[1:]
    return b


try:
    while True:
        kind = recv_exact(1)
        n = struct.unpack('<H', recv_exact(2))[0]
        if kind == b'T':
            data = recv_exact(n)
            try:
                os.write(master, data)
            except OSError:
                pass                    # no server on the pseudo terminal: the bytes are lost
        else:
            # sync of the packet, then its data
            b = read_byte(TIMEOUT)
            while b is not None and b != 0xF0:
                b = read_byte(TIMEOUT)
            data = b''
            while b is not None and len(data) < n:
                b = read_byte(TIMEOUT)
                if b is not None:
                    data += bytes([b])
            if len(data) == n:
                conn.sendall(b'\1' + data)
            else:
                conn.sendall(b'\0')
except EOFError:
    pass
