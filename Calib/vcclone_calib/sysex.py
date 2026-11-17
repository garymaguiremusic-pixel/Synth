"""Virus single-dump helpers (pure Python, mirrors PresetIO).

The byte layout comes from reference/virusC_single_256byte_map.csv, the same file the
C++ build uses, so both sides always agree on offsets and ranges.
"""
from __future__ import annotations

import csv
import os
import struct
from dataclasses import dataclass, field

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CSV_PATH = os.path.join(REPO, 'reference', 'virusC_single_256byte_map.csv')


@dataclass
class ParamDef:
    offset: int
    name: str
    min: int
    max: int
    to_text: str


def _load_layout():
    params = []
    with open(CSV_PATH, encoding='utf-8-sig') as f:
        for row in csv.DictReader(f):
            mn = int(row['min']) if row['min'] else 0
            mx = int(row['max']) if row['max'] else 127
            params.append(ParamDef(int(row['offset']), (row['name'] or '').strip(), mn, mx, row['toText'] or ''))
    # ranges the CSV leaves empty (filled from the JSON by the C++ generator)
    fill = {112: 26, 117: 127, 118: 127, 123: 127, 144: 127, 148: 16, 122: 1}
    for o, mx in fill.items():
        if params[o].max == 127 and mx != 127:
            params[o].max = mx
    return params


LAYOUT = _load_layout()
BY_NAME = {p.name.lower(): p.offset for p in LAYOUT if p.name}


def offset_of(name_or_offset) -> int:
    if isinstance(name_or_offset, int):
        return name_or_offset
    key = str(name_or_offset).strip().lower()
    if key in BY_NAME:
        return BY_NAME[key]
    alt = key.replace('_', ' ')
    if alt in BY_NAME:
        return BY_NAME[alt]
    if key.isdigit():
        return int(key)
    raise KeyError(f'unknown Virus parameter {name_or_offset!r}')


def checksum(msg: bytes | bytearray, first: int, last: int) -> int:
    return sum(msg[first:last + 1]) & 0x7F


@dataclass
class Single:
    raw: bytearray = field(default_factory=lambda: bytearray(256))
    bank: int = 0
    program: int = 0

    @property
    def name(self) -> str:
        return ''.join(chr(b) if 32 <= b < 127 else ' ' for b in self.raw[240:250]).rstrip()

    def set_name(self, n: str):
        n = (n + ' ' * 10)[:10]
        self.raw[240:250] = bytes(c if 32 <= c < 127 else 32 for c in n.encode('ascii', 'replace'))

    def set(self, name, value: int) -> 'Single':
        self.raw[offset_of(name)] = max(0, min(127, int(value)))
        return self

    def get(self, name) -> int:
        return self.raw[offset_of(name)]

    def copy(self) -> 'Single':
        return Single(bytearray(self.raw), self.bank, self.program)

    def dump(self, bank: int = 0, program: int = 0, device: int = 0x10) -> bytes:
        m = bytearray([0xF0, 0x00, 0x20, 0x33, 0x01, device, 0x10, bank & 0x7F, program & 0x7F])
        m += bytes(b & 0x7F for b in self.raw)
        m.append(checksum(m, 5, len(m) - 1))
        m.append(0xF7)
        return bytes(m)

    def key(self) -> bytes:
        return bytes(self.raw)


def split_sysex(data: bytes):
    out, i = [], 0
    while i < len(data):
        if data[i] != 0xF0:
            i += 1
            continue
        j = i + 1
        while j < len(data) and data[j] not in (0xF0, 0xF7):
            j += 1
        if j < len(data) and data[j] == 0xF7:
            out.append(bytes(data[i:j + 1]))
            i = j + 1
        else:
            i = j
    return out


def _smf_sysex(data: bytes):
    """Minimal SMF SysEx extractor (running status, meta events, F7 continuation)."""
    if data[:4] != b'MThd':
        return None
    hlen = struct.unpack('>I', data[4:8])[0]
    ntracks = struct.unpack('>H', data[10:12])[0]
    pos, out = 8 + hlen, []
    for _ in range(ntracks):
        if pos + 8 > len(data):
            break
        cid, ln = data[pos:pos + 4], struct.unpack('>I', data[pos + 4:pos + 8])[0]
        pos += 8
        trk, pos = data[pos:pos + ln], pos + ln
        if cid != b'MTrk':
            continue
        i, running, pending = 0, 0, None

        def vlq():
            nonlocal i
            v = 0
            for _ in range(4):
                b = trk[i]
                i += 1
                v = (v << 7) | (b & 0x7F)
                if not b & 0x80:
                    break
            return v
        try:
            while i < len(trk):
                vlq()
                st = trk[i]
                i += 1
                if st == 0xFF:
                    t = trk[i]
                    i += 1
                    ln2 = vlq()
                    i += ln2
                    if t == 0x2F:
                        break
                elif st in (0xF0, 0xF7):
                    ln2 = vlq()
                    body = trk[i:i + ln2]
                    i += ln2
                    running = 0
                    if st == 0xF0:
                        pending = bytearray([0xF0]) + body
                    elif pending is not None:
                        pending += body
                    else:
                        out += split_sysex(body)
                        continue
                    if pending and pending[-1] == 0xF7:
                        out.append(bytes(pending))
                        pending = None
                else:
                    if st & 0x80:
                        running = st
                        i += 1
                    else:
                        st = running
                    if (st & 0xF0) not in (0xC0, 0xD0):
                        i += 1
        except IndexError:
            pass
    return out


def load_singles(path: str):
    data = open(path, 'rb').read()
    msgs = _smf_sysex(data)
    if msgs is None:
        msgs = split_sysex(data)
    singles = []
    for m in msgs:
        if len(m) >= 267 and m[1:5] == b'\x00\x20\x33\x01' and m[6] == 0x10:
            s = Single(bytearray(m[9:9 + 256]), m[7], m[8])
            singles.append(s)
    return singles
