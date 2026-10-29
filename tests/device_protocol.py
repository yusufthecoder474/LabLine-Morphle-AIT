"""Independent encoder + real child IPC verifies simulator input safeguards."""
import csv
import socket
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path


def frame(seq, device=2, op=1, job=1):
    body = struct.pack('>BHIBBI', 1, 4, seq, device, op, job)
    return b'\xa5' + body + struct.pack('>I', zlib.crc32(body)) + b'\x5a'


def receive(sock):
    data = b''
    while len(data) < 4:
        data += sock.recv(4 - len(data))
    size = struct.unpack('>H', data[2:4])[0] + 15
    while len(data) < size:
        data += sock.recv(size - len(data))
    assert zlib.crc32(data[1:-5]) == struct.unpack('>I', data[-5:-1])[0]
    return data


with tempfile.TemporaryDirectory() as folder:
    parent, child = socket.socketpair()
    process = subprocess.Popen([sys.argv[1], str(child.fileno()), folder], pass_fds=[child.fileno()])
    child.close()
    parent.settimeout(2)
    try:
        bad = bytearray(frame(99)); bad[10] ^= 1
        # Send malformed + valid + duplicate + conflicting device use in one byte stream.
        parent.sendall(bad + frame(1) + frame(1) + frame(2))
        replies = [receive(parent), receive(parent)]
        assert sorted(r[9] for r in replies) == [0x80, 0x81]
        parent.sendall(frame(1)); cached = receive(parent)
        assert cached[9] == 0x80
        parent.sendall(frame(1, job=2)); assert receive(parent)[9] == 0x81
    finally:
        parent.close()
        assert process.wait(timeout=3) == 0
    with open(Path(folder) / 'simulator.csv') as f:
        rows = list(csv.DictReader(f))
    assert sum(r['event'] == 'START' for r in rows) == 1
    assert sum(r['event'] == 'COMPLETE' for r in rows) == 1
    assert all(r['sequence'] != '99' for r in rows)
print('corrupt request: no action; exclusivity NACK; in-flight and completed dedup; sequence collision NACK: PASS')
