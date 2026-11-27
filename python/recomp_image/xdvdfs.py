# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Matthew Stanley. See PROVENANCE.md.
"""Read-only XDVDFS geometry, physical extents, directory index and source lifetime."""
from dataclasses import dataclass
import os
from pathlib import Path
import re
import stat
import struct
SECTOR=2048
MAGIC=b"MICROSOFT*XBOX*MEDIA"
PARTITIONS=(0,0x18300000,0x0fd90000,0x02080000)
MAX_TABLE=64*1024*1024
MAX_ENTRIES=100000
def safe_name(raw):
    try:
        name = raw.decode('cp1252')
    except UnicodeDecodeError as error:
        raise ValueError('Disc filename has an unsupported encoding') from error
    if (not name or name in ('.', '..') or name[-1] in '. ' or
            any(ord(c) < 32 or ord(c) == 127 or c in '<>:"/\\|?*' for c in name) or
            re.fullmatch(r'(CON|PRN|AUX|NUL|COM[1-9¹²³]|LPT[1-9¹²³])', name.split('.')[0], re.I)):
        raise ValueError('Disc contains an unsafe filename')
    return name


def source_identity(path, value):
    # A retained cache is tied to this source file and its version, not its name
    # or a title ID. No full-ISO hashing or payload scan is repeated on launch.
    identity = dict(device=value.st_dev, file=value.st_ino, size=value.st_size,
                    modified=value.st_mtime_ns)
    # Python's Windows fstat/stat can disagree on legacy st_ctime for sparse
    # files. File ID, length and precise write time agree across both APIs.
    if os.name != 'nt':
        identity['changed'] = value.st_ctime_ns
    if not value.st_ino:
        identity['path'] = str(path)
    return identity


@dataclass(frozen=True)
class Entry:
    path: str
    offset: int
    size: int
    directory: bool


class Volume:
    def __init__(self, path):
        self.path = Path(path).resolve(strict=True)
        self.file = self.path.open('rb')
        try:
            info = os.fstat(self.file.fileno())
            if not stat.S_ISREG(info.st_mode):
                raise ValueError('Choose an Xbox ISO or XISO file')
            self.identity = source_identity(self.path, info)
            self.size = info.st_size
            self.base = None
            for base in PARTITIONS:
                if base + 33 * SECTOR > self.size:
                    continue
                header = self.read(base + 32 * SECTOR, SECTOR)
                if header[:20] == MAGIC and header[-20:] == MAGIC:
                    self.base = base
                    root_sector, root_size = struct.unpack_from('<II', header, 20)
                    self.root = (base + root_sector * SECTOR, root_size)
                    self.extent(*self.root)
                    if not 14 <= root_size <= MAX_TABLE:
                        raise ValueError('Xbox disc root directory is invalid')
                    break
            if self.base is None:
                raise ValueError('Not an Xbox XDVDFS disc image. Select an Xbox ISO/XISO or extracted XBE.')
        except BaseException:
            self.file.close()
            raise

    def __enter__(self): return self
    def __exit__(self, *_): self.file.close()

    def extent(self, offset, size):
        if offset < 0 or size < 0 or offset > self.size or size > self.size - offset:
            raise ValueError('Xbox disc contains a truncated or out-of-bounds extent')

    def read(self, offset, size):
        self.extent(offset, size)
        self.file.seek(offset)
        data = self.file.read(size)
        if len(data) != size:
            raise ValueError('Xbox disc is truncated')
        return data

    def unchanged(self):
        if source_identity(self.path, os.fstat(self.file.fileno())) != self.identity:
            raise ValueError('Disc image changed during import. Its partial import was discarded.')
        # Also detect replacement of the pathname while the original handle is open.
        if source_identity(self.path, self.path.stat()) != self.identity:
            raise ValueError('Disc image was replaced during import')

    def table(self, offset, size):
        if not 14 <= size <= MAX_TABLE:
            raise ValueError('Xbox disc directory table is invalid')
        data = self.read(offset, size)
        pending, seen, names, intervals, result = [0], set(), set(), [], []
        while pending:
            at = pending.pop()
            if at in seen or at + 14 > size or len(seen) >= MAX_ENTRIES:
                raise ValueError('Xbox disc directory has a cycle or invalid node')
            seen.add(at)
            left, right, sector, length, attributes, n = struct.unpack_from('<HHIIBB', data, at)
            end = at + 14 + n
            if not n or end > size:
                raise ValueError('Xbox disc directory record is truncated')
            name = safe_name(data[at + 14:end])
            if name.casefold() in names:
                raise ValueError('Xbox disc contains colliding filenames')
            names.add(name.casefold());intervals.append((at, end))
            start = self.base + sector * SECTOR
            self.extent(start, length)
            result.append(Entry(name, start, length, bool(attributes & 0x10)))
            for child in (left, right):
                if child:
                    pending.append(child * 4)
        intervals.sort()
        if any(b[0] < a[1] for a, b in zip(intervals, intervals[1:])):
            raise ValueError('Xbox disc directory records overlap')
        return result

    def entries(self, check=lambda: None):
        pending = [('', *self.root, 0)]
        visited, result, extents = set(), [], [(self.root[0], self.root[0] + self.root[1])]
        while pending:
            prefix, offset, size, depth = pending.pop()
            check()
            if depth > 32 or (offset, size) in visited:
                raise ValueError('Xbox disc has a directory cycle or excessive depth')
            visited.add((offset, size))
            for e in self.table(offset, size):
                path = prefix + e.path
                if len(path) > 30000 or len(result) >= MAX_ENTRIES:
                    raise ValueError('Xbox disc exceeds import path or entry limits')
                result.append(Entry(path, e.offset, e.size, e.directory))
                if e.size:
                    extents.append((e.offset, e.offset + e.size))
                if e.directory and e.size:
                    pending.append((path + '/', e.offset, e.size, depth + 1))
        extents.sort()
        if any(b[0] < a[1] for a, b in zip(extents, extents[1:])):
            raise ValueError('Xbox disc file or directory extents overlap')
        self.unchanged()
        return result

