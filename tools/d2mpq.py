#!/usr/bin/env python3
r"""Read Diablo II MPQ archives correctly -- including PKWARE-imploded files.

WHY THIS EXISTS
---------------
The obvious way to read a D2 archive from Python is ``mpyq``::

    import mpyq
    data = mpyq.MPQArchive("D2Data.mpq").read_file(r"data\global\excel\objects.txt")

That call **silently returns garbage** for most D2 data files, and this is the
single defect this module exists to prevent.

D2 flags its files ``MPQ_FILE_IMPLODE`` (0x00000100), meaning each sector is
compressed with the PKWARE Data Compression Library ("implode"/DCL) -- an
older scheme than the ``MPQ_FILE_COMPRESS`` (0x00000200) multi-method one.
``mpyq`` only implements the ``MPQ_FILE_COMPRESS`` path.  When it meets an
IMPLODE-flagged file it does not decompress, does not warn, and does not
raise: it concatenates the raw compressed sectors and hands them back as if
they were the file.  You get a ``bytes`` object of plausible-looking length
and every read "succeeds".

Measured on 1.13c ``Patch_D2.mpq``, ``data\global\excel\objects.txt``:

    mpyq.read_file()  ->  30,560 bytes of DCL bit-streams   (looks fine, is not)
    this module       -> 211,970 bytes of real tab-separated text, 573 records

The 30,560-byte answer parses as 87 "records" if you split it on newlines,
because compressed bytes contain 0x0A often enough to fool a line count.  A
book chapter that cites a number derived from that read is citing noise.

WHAT THIS MODULE DOES
---------------------
It reuses ``mpyq`` for the parts it gets right (header, hash table, block
table, key derivation, decryption) and reimplements ``read_file`` with a pure
Python PKWARE DCL ``explode`` applied per sector, transcribed from Mark
Adler's public-domain ``blast.c``.

It also fails loudly.  Every path that could return an unverified byte string
instead raises :class:`MpqReadError` naming the file, the block flags, and the
sector.  Decompressed output is length-checked against the block table.  See
``selfcheck`` for a standing proof that the reader still produces sane data.

LICENSING NOTE
--------------
This reader is ours.  The data it extracts is Blizzard's.  Extracted tables go
into a gitignored local cache (``.vanilla-cache/``) and are never committed;
regenerate them from your own installation with ``d2mpq.py tables``.

USAGE
-----
    python tools/d2mpq.py list    <archive.mpq|version-dir> [--pattern GLOB]
    python tools/d2mpq.py extract <archive.mpq|version-dir> <internal\path> [--out FILE]
    python tools/d2mpq.py tables  <version> [--out-dir DIR] [--table NAME ...]
    python tools/d2mpq.py counts  <file.txt> [...]
    python tools/d2mpq.py selfcheck [--version 1.13c]

The game trees are located by ``--d2-root`` / ``$D2_VERSIONS_ROOT``, defaulting
to the documented path in ``docs/vanilla-data.md``.  Nothing here assumes one
machine's layout: any argument that names a version may instead be a full path
to a version directory.
"""

from __future__ import annotations

import argparse
import bz2
import fnmatch
import hashlib
import json
import os
import struct
import sys
import zlib
from io import BytesIO

try:
    import mpyq
except ImportError:  # pragma: no cover - environment problem, not a data problem
    sys.stderr.write(
        "d2mpq: the 'mpyq' package is required (pip install mpyq).\n"
        "It supplies the MPQ header/hash/block parsing; this module supplies\n"
        "the PKWARE DCL decompression mpyq is missing.\n"
    )
    raise

__all__ = [
    "MpqReadError",
    "D2Archive",
    "explode",
    "read_excel",
    "record_count",
    "archive_chain",
    "version_dir",
    "EXCEL_TABLES",
    "KNOWN_GOOD",
]

# ---------------------------------------------------------------------------
# Configuration -- defaults only, every one of these is overridable.
# ---------------------------------------------------------------------------

#: Where the per-version vanilla game trees live.  Overridable with
#: ``--d2-root`` or the ``D2_VERSIONS_ROOT`` environment variable.
DEFAULT_D2_ROOT = r"F:\D2VersionChanger\VersionChanger\LoD"

#: Repo root, derived from this file's location -- not hardcoded.
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

#: Gitignored local cache for extracted tables.  Overridable with
#: ``--cache-dir`` or ``D2_VANILLA_CACHE``.
DEFAULT_CACHE_DIR = os.path.join(REPO_ROOT, ".vanilla-cache")

#: MPQ search order for vanilla Lord of Destruction, highest priority first.
#: Patch_D2 shadows the expansion archive, which shadows the base archive --
#: this is why a patched 1.13c ``objects.txt`` comes out of Patch_D2.mpq.
ARCHIVE_ORDER = (
    "Patch_D2.mpq",
    "D2Exp.mpq",
    "D2Xtalk.mpq",
    "D2XMusic.mpq",
    "D2Xvideo.mpq",
    "D2Data.mpq",
    "D2Char.mpq",
    "D2Sfx.mpq",
    "D2Music.mpq",
    "D2Speech.mpq",
    "D2Video.mpq",
)

EXCEL_DIR = "data\\global\\excel\\"

#: The tables a level/DS1 chapter is most likely to cite.  Not every version
#: ships every one of these; ``tables`` reports the absentees by name.
EXCEL_TABLES = (
    # level geometry and presets -- the DS1 editor's own domain
    "levels.txt",
    "lvlprest.txt",
    "lvltypes.txt",
    "lvlsub.txt",
    "lvlmaze.txt",
    "lvlwarp.txt",
    # objects
    "objects.txt",
    "objgroup.txt",
    "objtype.txt",
    "objmode.txt",
    "shrines.txt",
    # monsters
    "monstats.txt",
    "monstats2.txt",
    "monpreset.txt",
    "monplace.txt",
    "monseq.txt",
    "superuniques.txt",
    # items, for cross-referencing object drops
    "misc.txt",
    "armor.txt",
    "weapons.txt",
    "itemtypes.txt",
    "setitems.txt",
    "uniqueitems.txt",
    # misc
    "missiles.txt",
    "skills.txt",
    "sounds.txt",
    "treasureclassex.txt",
)

#: Known-good vanilla record counts.  ``selfcheck`` asserts these; a change
#: here without a corresponding game-data change means the reader regressed.
#: "records" excludes the header row and the ``Expansion`` separator row --
#: see :func:`record_count`.
KNOWN_GOOD = {
    "1.13c": {
        "objects.txt": 573,
        "monstats.txt": 734,
        "monpreset.txt": 229,
        "levels.txt": 137,
        "lvlprest.txt": 1091,
        "lvltypes.txt": 36,
        "superuniques.txt": 66,
        "monplace.txt": 37,
    },
    "1.09d": {
        "objects.txt": 573,
        "monstats.txt": 575,
        "superuniques.txt": 66,
    },
}

#: Names that appear in Project Diablo 2's tables and in no vanilla table.
#: If any of these turn up in a supposedly-vanilla extraction, the extraction
#: came from a modded tree.
PD2_MARKERS = ("UberAncientAltar", "PVPTome1", "LucionChest")

# ---------------------------------------------------------------------------
# MPQ block flags
# ---------------------------------------------------------------------------

MPQ_FILE_IMPLODE = 0x00000100      # PKWARE DCL -- the one mpyq ignores
MPQ_FILE_COMPRESS = 0x00000200     # multi-method, leading mask byte
MPQ_FILE_ENCRYPTED = 0x00010000
MPQ_FILE_FIX_KEY = 0x00020000
MPQ_FILE_PATCH_FILE = 0x00100000
MPQ_FILE_SINGLE_UNIT = 0x01000000
MPQ_FILE_DELETE_MARKER = 0x02000000
MPQ_FILE_SECTOR_CRC = 0x04000000
MPQ_FILE_EXISTS = 0x80000000

_FLAG_NAMES = (
    (MPQ_FILE_IMPLODE, "IMPLODE"),
    (MPQ_FILE_COMPRESS, "COMPRESS"),
    (MPQ_FILE_ENCRYPTED, "ENCRYPTED"),
    (MPQ_FILE_FIX_KEY, "FIX_KEY"),
    (MPQ_FILE_PATCH_FILE, "PATCH_FILE"),
    (MPQ_FILE_SINGLE_UNIT, "SINGLE_UNIT"),
    (MPQ_FILE_DELETE_MARKER, "DELETE_MARKER"),
    (MPQ_FILE_SECTOR_CRC, "SECTOR_CRC"),
    (MPQ_FILE_EXISTS, "EXISTS"),
)


def describe_flags(flags: int) -> str:
    """``0x81000200 (EXISTS|SINGLE_UNIT|COMPRESS)`` -- for error messages."""
    names = [n for bit, n in _FLAG_NAMES if flags & bit]
    unknown = flags & ~sum(bit for bit, _ in _FLAG_NAMES)
    if unknown:
        names.append("unknown:0x%08X" % unknown)
    return "0x%08X (%s)" % (flags, "|".join(names) or "none")


class MpqReadError(Exception):
    """A read could not be completed *correctly*.

    Raised in preference to returning bytes whose correctness is unproven.
    Always names the file and the block flags, because the flags are what
    tell you which decompressor should have run.
    """

    def __init__(self, message, filename=None, flags=None, archive=None,
                 sector=None):
        parts = [message]
        if filename:
            parts.append("file=%s" % filename)
        if flags is not None:
            parts.append("flags=%s" % describe_flags(flags))
        if sector is not None:
            parts.append("sector=%d" % sector)
        if archive:
            parts.append("archive=%s" % archive)
        super().__init__("; ".join(parts))
        self.filename = filename
        self.flags = flags
        self.archive = archive
        self.sector = sector


# ---------------------------------------------------------------------------
# PKWARE Data Compression Library "explode", from Mark Adler's blast.c
# (public domain).  This is the piece mpyq does not have.
# ---------------------------------------------------------------------------

_LITLEN = bytes([
    11, 124, 8, 7, 28, 7, 188, 13, 76, 4, 10, 8, 12, 10, 12, 10, 8, 23, 8,
    9, 7, 6, 7, 8, 7, 6, 55, 8, 23, 24, 12, 11, 7, 9, 11, 12, 6, 7, 22, 5,
    7, 24, 6, 11, 9, 6, 7, 22, 7, 11, 38, 7, 9, 8, 25, 11, 8, 11, 9, 12,
    8, 12, 5, 38, 5, 38, 5, 11, 7, 5, 6, 21, 6, 10, 53, 8, 7, 24, 10, 27,
    44, 253, 253, 253, 252, 252, 252, 13, 12, 45, 12, 45, 12, 61, 12, 45,
    44, 173])
_LENLEN = bytes([2, 35, 36, 53, 38, 23])
_DISTLEN = bytes([2, 20, 53, 230, 247, 151, 248])
_BASE = (3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264)
_EXTRA = (0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8)

#: Length code 519 is the DCL end-of-stream marker.
_DCL_END = 519


class _Huffman:
    """A canonical Huffman decoding table built from blast.c's compact form."""

    __slots__ = ("count", "symbol")

    def __init__(self, rep):
        # Each byte is (repeat-1) << 4 | bit-length.  Expand to a flat list.
        lengths = []
        for byte in rep:
            left = (byte >> 4) + 1
            ln = byte & 15
            lengths.extend([ln] * left)

        count = [0] * 16
        for ln in lengths:
            count[ln] += 1
        count[0] = 0

        offs = [0] * 16
        for i in range(1, 15):
            offs[i + 1] = offs[i] + count[i]

        symbol = [0] * len(lengths)
        for sym, ln in enumerate(lengths):
            if ln:
                symbol[offs[ln]] = sym
                offs[ln] += 1

        self.count = count
        self.symbol = symbol


_LITCODE = _Huffman(_LITLEN)
_LENCODE = _Huffman(_LENLEN)
_DISTCODE = _Huffman(_DISTLEN)


class _BitReader:
    __slots__ = ("data", "pos", "bitbuf", "bitcnt")

    def __init__(self, data):
        self.data = data
        self.pos = 0
        self.bitbuf = 0
        self.bitcnt = 0

    def bits(self, need):
        val = self.bitbuf
        while self.bitcnt < need:
            if self.pos >= len(self.data):
                raise ValueError("explode: ran out of input needing %d bits" % need)
            val |= self.data[self.pos] << self.bitcnt
            self.pos += 1
            self.bitcnt += 8
        self.bitbuf = val >> need
        self.bitcnt -= need
        return val & ((1 << need) - 1)

    def decode(self, huff):
        code = first = index = 0
        length = 1
        count_tbl = huff.count
        bitbuf = self.bitbuf
        left = self.bitcnt
        while True:
            while left:
                code |= (bitbuf & 1) ^ 1  # DCL stores codes inverted
                bitbuf >>= 1
                count = count_tbl[length]
                if code - count < first:
                    self.bitbuf = bitbuf
                    self.bitcnt = left - 1
                    return huff.symbol[index + (code - first)]
                index += count
                first += count
                first <<= 1
                code <<= 1
                length += 1
                left -= 1
                if length > 15:
                    raise ValueError("explode: invalid Huffman code (>15 bits)")
            if self.pos >= len(self.data):
                raise ValueError("explode: ran out of input mid-symbol")
            bitbuf = self.data[self.pos]
            self.pos += 1
            left = 8


def explode(data: bytes) -> bytes:
    """Decompress one PKWARE DCL ("implode") stream.

    Raises ``ValueError`` on any malformed input.  It never returns a partial
    or best-effort result -- a truncated stream is an error, not a short read.
    """
    r = _BitReader(data)
    lit = r.bits(8)
    if lit > 1:
        raise ValueError("explode: bad literal flag %d (expected 0 or 1)" % lit)
    dict_bits = r.bits(8)
    if not 4 <= dict_bits <= 6:
        raise ValueError(
            "explode: bad dictionary size %d (expected 4..6)" % dict_bits)

    out = bytearray()
    while True:
        if r.bits(1):
            symbol = r.decode(_LENCODE)
            length = _BASE[symbol] + r.bits(_EXTRA[symbol])
            if length == _DCL_END:
                break
            nbits = 2 if length == 2 else dict_bits
            dist = (r.decode(_DISTCODE) << nbits) + r.bits(nbits) + 1
            if dist > len(out):
                raise ValueError(
                    "explode: back-reference distance %d exceeds %d bytes "
                    "produced so far" % (dist, len(out)))
            start = len(out) - dist
            for i in range(length):
                out.append(out[start + i])
        else:
            out.append(r.decode(_LITCODE) if lit else r.bits(8))
    return bytes(out)


#: ``MPQ_FILE_COMPRESS`` method bits, for naming what we cannot do.
_MASK_NAMES = (
    (0x01, "Storm Huffman"),
    (0x02, "zlib"),
    (0x08, "PKWARE DCL"),
    (0x10, "bzip2"),
    (0x20, "sparse"),
    (0x40, "IMA ADPCM mono"),
    (0x80, "IMA ADPCM stereo"),
)


def describe_mask(mask):
    names = [n for bit, n in _MASK_NAMES if mask & bit]
    return "+".join(names) if names else "none"


def _decompress_masked(data, filename, flags, archive, sector):
    """A ``MPQ_FILE_COMPRESS`` sector: leading byte is the method mask."""
    if not data:
        raise MpqReadError("empty compressed sector", filename, flags, archive, sector)
    mask = data[0]
    body = data[1:]
    try:
        if mask == 0:
            return body
        if mask & 0x08:
            return explode(body)
        if mask & 0x02:
            return zlib.decompress(body, 15)
        if mask & 0x10:
            return bz2.decompress(body)
    except Exception as exc:
        raise MpqReadError(
            "decompression failed for mask 0x%02X (%s): %s"
            % (mask, describe_mask(mask), exc),
            filename, flags, archive, sector) from exc
    if mask & 0xC1:
        # Huffman + ADPCM: Blizzard's WAVE codec. Deliberately not implemented
        # -- this tool exists for the data tables, and audio is out of scope.
        raise MpqReadError(
            "compression mask 0x%02X (%s) is Blizzard's WAVE codec, which this "
            "tool does not implement -- it reads data tables, not audio. "
            "Use StormLib/MPQEditor for these." % (mask, describe_mask(mask)),
            filename, flags, archive, sector)
    raise MpqReadError(
        "unsupported compression mask 0x%02X (%s) -- refusing to return the "
        "compressed bytes as if they were the file" % (mask, describe_mask(mask)),
        filename, flags, archive, sector)


# ---------------------------------------------------------------------------
# The archive
# ---------------------------------------------------------------------------

class D2Archive:
    """An MPQ archive read with IMPLODE support and no silent fallbacks."""

    def __init__(self, path):
        self.path = os.path.abspath(path)
        if not os.path.isfile(self.path):
            raise MpqReadError("archive not found", archive=self.path)
        # listfile=False: mpyq would read (listfile) with its own broken
        # read_file and hand us garbage before we ever got control.
        self.a = mpyq.MPQArchive(self.path, listfile=False)
        self._names = None

    # -- lifecycle ---------------------------------------------------------

    def close(self):
        try:
            self.a.file.close()
        except Exception:
            pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        return False

    def __repr__(self):
        return "<D2Archive %s, %d blocks>" % (
            os.path.basename(self.path), len(self.a.block_table))

    # -- internals ---------------------------------------------------------

    def _dec(self, data, key):
        """mpyq's ``_decrypt`` drops a non-dword tail; Storm leaves it clear."""
        n = len(data) - (len(data) % 4)
        return self.a._decrypt(data[:n], key) + data[n:]

    def _block_for(self, filename):
        entry = self.a.get_hash_table_entry(filename)
        if entry is None:
            return None
        block = self.a.block_table[entry.block_table_index]
        if not block.flags & MPQ_FILE_EXISTS:
            return None
        if block.flags & MPQ_FILE_DELETE_MARKER:
            return None
        return block

    # -- public API --------------------------------------------------------

    def has(self, filename) -> bool:
        """Is *filename* present in this archive?"""
        return self._block_for(filename) is not None

    def flags_for(self, filename):
        """Block flags for *filename*, or ``None`` if absent."""
        block = self._block_for(filename)
        return None if block is None else block.flags

    def read(self, filename, required=False):
        """Return the decompressed bytes of *filename*, or ``None`` if absent.

        Every failure mode other than "not in this archive" raises
        :class:`MpqReadError`.  This function never returns compressed bytes.
        """
        block = self._block_for(filename)
        if block is None:
            if required:
                raise MpqReadError("file not present in archive",
                                   filename, archive=self.path)
            return None

        flags = block.flags
        if block.archived_size == 0:
            if block.size == 0:
                return b""
            raise MpqReadError(
                "block table claims %d uncompressed bytes stored in 0 bytes"
                % block.size, filename, flags, self.path)

        self.a.file.seek(block.offset + self.a.header["offset"])
        raw = self.a.file.read(block.archived_size)
        if len(raw) != block.archived_size:
            raise MpqReadError(
                "archive truncated: wanted %d bytes at 0x%X, got %d"
                % (block.archived_size, block.offset + self.a.header["offset"],
                   len(raw)),
                filename, flags, self.path)

        key = None
        if flags & MPQ_FILE_ENCRYPTED:
            base = filename.rsplit("\\", 1)[-1]
            key = self.a._hash(base, "TABLE")
            if flags & MPQ_FILE_FIX_KEY:
                key = (key + block.offset) & 0xFFFFFFFF
                key ^= block.size

        imploded = bool(flags & MPQ_FILE_IMPLODE)
        compressed = bool(flags & MPQ_FILE_COMPRESS)

        # A guard against a future refactor amputating explode(): if a file is
        # IMPLODE-flagged we must have a working exploder, and saying so here
        # is cheaper than debugging the garbage it would otherwise return.
        if imploded and not callable(globals().get("explode")):
            raise MpqReadError(
                "file is IMPLODE-flagged but no PKWARE DCL decompressor is "
                "available -- refusing to return compressed bytes",
                filename, flags, self.path)

        if flags & MPQ_FILE_SINGLE_UNIT:
            data = self._read_single_unit(raw, block, filename, key,
                                          imploded, compressed)
        else:
            data = self._read_sectors(raw, block, filename, key,
                                      imploded, compressed)

        if len(data) != block.size:
            raise MpqReadError(
                "decompressed to %d bytes but the block table says %d -- the "
                "output is not the file" % (len(data), block.size),
                filename, flags, self.path)
        return data

    def _read_single_unit(self, raw, block, filename, key, imploded, compressed):
        flags = block.flags
        if key is not None:
            raw = self._dec(raw, key)
        if block.size <= block.archived_size:
            return raw[:block.size]  # stored, not compressed
        try:
            if imploded:
                return explode(raw)
            if compressed:
                return _decompress_masked(raw, filename, flags, self.path, 0)
        except MpqReadError:
            raise
        except Exception as exc:
            raise MpqReadError("single-unit decompression failed: %s" % exc,
                               filename, flags, self.path) from exc
        raise MpqReadError(
            "block is smaller than its stated size (%d < %d) but carries no "
            "compression flag -- cannot recover the file"
            % (block.archived_size, block.size),
            filename, flags, self.path)

    def _read_sectors(self, raw, block, filename, key, imploded, compressed):
        flags = block.flags
        a = self.a
        sector_size = 512 << a.header["sector_size_shift"]
        has_crc = bool(flags & MPQ_FILE_SECTOR_CRC)

        # A file with no compression flag has no sector offset table at all --
        # its sectors are fixed-size and contiguous, so the bytes are the file.
        # Reading an offset table here would interpret the file's own leading
        # bytes as offsets; that is what produces "sector offsets out of range"
        # on .dcc and .bik files.
        if not (imploded or compressed):
            if has_crc:
                raise MpqReadError(
                    "SECTOR_CRC without a compression flag is not supported",
                    filename, flags, self.path)
            if key is not None:
                out = BytesIO()
                left = block.size
                for i in range((block.size + sector_size - 1) // sector_size):
                    chunk = raw[i * sector_size:i * sector_size + min(sector_size, left)]
                    out.write(self._dec(chunk, (key + i) & 0xFFFFFFFF))
                    left -= len(chunk)
                return out.getvalue()
            return raw[:block.size]

        # Sector count is a ceiling, not a floor-plus-one: a file whose size is
        # an exact multiple of the sector size has no trailing partial sector,
        # and counting one anyway walks off the end of the offset table.
        sectors = (block.size + sector_size - 1) // sector_size or 1
        if has_crc:
            sectors += 1

        table_bytes = 4 * (sectors + 1)
        if table_bytes > len(raw):
            raise MpqReadError(
                "sector offset table (%d bytes) does not fit in the %d stored "
                "bytes" % (table_bytes, len(raw)), filename, flags, self.path)
        table_raw = raw[:table_bytes]
        if key is not None:
            table_raw = self._dec(table_raw, (key - 1) & 0xFFFFFFFF)
        positions = struct.unpack("<%dI" % (sectors + 1), table_raw)

        out = BytesIO()
        left = block.size
        n_data = len(positions) - (2 if has_crc else 1)
        for i in range(n_data):
            start, end = positions[i], positions[i + 1]
            if not (0 <= start <= end <= len(raw)):
                raise MpqReadError(
                    "sector offsets out of range: [%d, %d) in %d stored bytes"
                    % (start, end, len(raw)), filename, flags, self.path, i)
            sector = raw[start:end]
            want = min(sector_size, left)
            if key is not None:
                sector = self._dec(sector, (key + i) & 0xFFFFFFFF)
            if len(sector) < want:
                # Shorter than the plaintext it must yield => it is compressed.
                if imploded:
                    try:
                        sector = explode(sector)
                    except Exception as exc:
                        raise MpqReadError(
                            "PKWARE DCL explode failed: %s" % exc,
                            filename, flags, self.path, i) from exc
                elif compressed:
                    sector = _decompress_masked(sector, filename, flags,
                                                self.path, i)
                else:
                    raise MpqReadError(
                        "sector is %d bytes but must expand to %d, and the "
                        "block carries no compression flag" % (len(sector), want),
                        filename, flags, self.path, i)
            if len(sector) != want:
                raise MpqReadError(
                    "sector expanded to %d bytes, expected %d"
                    % (len(sector), want), filename, flags, self.path, i)
            left -= len(sector)
            out.write(sector)
        return out.getvalue()

    # -- enumeration -------------------------------------------------------

    def namelist(self, extra_names=()):
        """Filenames in this archive, from its ``(listfile)`` where present.

        MPQ hash tables store hashes, not names, so an archive without a
        ``(listfile)`` cannot be fully enumerated.  1.13c's ``Patch_D2.mpq`` is
        exactly such an archive.  For those, pass *extra_names* (the ``list``
        command supplies the listfiles of the sibling archives) and this
        returns the subset that actually resolves.
        """
        if self._names is not None and not extra_names:
            return self._names

        names = []
        raw = self.read("(listfile)")
        if raw is not None:
            text = raw.decode("latin-1").replace("\r\n", "\n")
            names = [n.strip() for n in text.split("\n") if n.strip()]

        if extra_names:
            known = {n.lower() for n in names}
            for n in extra_names:
                if n.lower() not in known and self.has(n):
                    names.append(n)
                    known.add(n.lower())

        names.sort(key=str.lower)
        if not extra_names:
            self._names = names
        return names

    def entries(self, names=None):
        """``(name, flags, size, archived_size)`` for each nameable file."""
        out = []
        for n in (names if names is not None else self.namelist()):
            entry = self.a.get_hash_table_entry(n)
            if entry is None:
                continue
            b = self.a.block_table[entry.block_table_index]
            if b.flags & MPQ_FILE_EXISTS and not b.flags & MPQ_FILE_DELETE_MARKER:
                out.append((n, b.flags, b.size, b.archived_size))
        return out

    @property
    def sector_size(self):
        return 512 << self.a.header["sector_size_shift"]

    @property
    def block_count(self):
        return len(self.a.block_table)

    @property
    def has_listfile(self):
        return self.has("(listfile)")


# ---------------------------------------------------------------------------
# Locating archives
# ---------------------------------------------------------------------------

def d2_root(override=None):
    """Root directory holding the per-version game trees."""
    return override or os.environ.get("D2_VERSIONS_ROOT") or DEFAULT_D2_ROOT


def version_dir(version, root=None):
    """Resolve a version name (or an explicit directory) to a game tree."""
    if os.path.isdir(version):
        return os.path.abspath(version)
    path = os.path.join(d2_root(root), version)
    if not os.path.isdir(path):
        raise MpqReadError(
            "no game tree for version %r under %s -- pass --d2-root or set "
            "D2_VERSIONS_ROOT, or give a full directory path instead of a "
            "version name" % (version, d2_root(root)))
    return path


def archive_chain(version, root=None):
    """Archive paths for *version*, in MPQ search order, existing ones only."""
    d = version_dir(version, root)
    actual = {n.lower(): n for n in os.listdir(d)}
    chain = []
    for want in ARCHIVE_ORDER:
        real = actual.get(want.lower())
        if real:
            chain.append(os.path.join(d, real))
    if not chain:
        raise MpqReadError("no known D2 MPQ archives in %s" % d)
    return chain


def _as_chain(target, root=None):
    """Accept an .mpq path, a version directory, or a version name."""
    if os.path.isfile(target):
        return [os.path.abspath(target)]
    return archive_chain(target, root)


def read_excel(archives, name, required=False):
    """Read ``data\\global\\excel\\<name>`` from the first archive that has it.

    Returns ``(archive_path, data)``; ``(None, None)`` when absent everywhere,
    unless *required*.
    """
    path = EXCEL_DIR + name
    for arc in archives:
        with D2Archive(arc) as a:
            data = a.read(path)
        if data is not None:
            return arc, data
    if required:
        raise MpqReadError("not found in any archive of the chain",
                           filename=path,
                           archive=os.pathsep.join(archives))
    return None, None


# ---------------------------------------------------------------------------
# Excel table accounting
# ---------------------------------------------------------------------------

def record_count(data):
    """``(line_count, record_count)`` for a D2 tab-separated excel table.

    D2's tables are CRLF, tab-separated, with a header row.  Expansion tables
    additionally carry a single ``Expansion`` separator row (at line 412 in
    1.09d-1.14d) marking where classic ends -- the game skips it and so do we.

    ``record_count`` therefore = non-empty lines - 1 header - separator rows.
    Both numbers are returned so a citation can say which it means.
    """
    if isinstance(data, bytes):
        data = data.decode("latin-1", "replace")
    lines = [l for l in data.replace("\r\n", "\n").split("\n") if l != ""]
    if not lines:
        return 0, 0
    separators = sum(
        1 for l in lines[1:] if l.split("\t")[0].strip().lower() == "expansion")
    return len(lines), len(lines) - 1 - separators


def _looks_like_text(data, sample=4096):
    """Heuristic sanity gate -- a real excel table is overwhelmingly ASCII."""
    chunk = data[:sample]
    if not chunk:
        return False
    ok = sum(1 for c in chunk if 32 <= c < 127 or c in (9, 10, 13))
    return ok / len(chunk) > 0.95


# ---------------------------------------------------------------------------
# Cache
# ---------------------------------------------------------------------------

def cache_dir(override=None):
    return override or os.environ.get("D2_VANILLA_CACHE") or DEFAULT_CACHE_DIR


def version_label(version):
    """A safe directory name for a version that may have been given as a path.

    ``os.path.join(cache, r"F:\\...\\1.13c")`` returns the absolute path, which
    would write the cache *into the game tree*.  Reduce a path to its last
    component before it is ever used as a name.
    """
    if os.path.isdir(version) or os.path.isabs(version) or "\\" in version \
            or "/" in version:
        return os.path.basename(os.path.normpath(version)) or "unknown"
    return version


def cache_path_for(version, override=None):
    return os.path.join(cache_dir(override), version_label(version), "excel")


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def cmd_list(args):
    targets = _as_chain(args.archive, args.d2_root)
    single = len(targets) == 1

    # For a listfile-less archive, borrow candidate names from its siblings.
    borrowed = []
    if single:
        sibdir = os.path.dirname(targets[0])
        for name in ARCHIVE_ORDER:
            p = os.path.join(sibdir, name)
            if os.path.exists(p) and os.path.abspath(p) != targets[0]:
                try:
                    with D2Archive(p) as sib:
                        borrowed.extend(sib.namelist())
                except MpqReadError:
                    pass
        borrowed.extend(EXCEL_DIR + t for t in EXCEL_TABLES)

    rc = 0
    for path in targets:
        with D2Archive(path) as a:
            listed = a.has_listfile
            names = a.namelist(extra_names=() if listed else borrowed)
            if args.pattern:
                pat = args.pattern.lower().replace("/", "\\")
                names = [n for n in names if fnmatch.fnmatch(n.lower(), pat)]
            print("== %s -- %d blocks, %s" % (
                os.path.basename(path), a.block_count,
                "(listfile) present -- note D2's listfiles are themselves "
                "incomplete (monstats2/monpreset/monplace/monseq are real "
                "files no listfile names)" if listed
                else "NO (listfile): names below were recovered by probing "
                     "sibling archives' listfiles, so this list is incomplete"))
            for n in names:
                flags = a.flags_for(n)
                print("  %-60s %s" % (n, describe_flags(flags) if flags else "?"))
            print("   %d name(s)%s" % (
                len(names),
                "" if listed else " of %d blocks -- %d unnamed"
                                  % (a.block_count,
                                     max(0, a.block_count - len(names)))))
            if not listed and not names:
                rc = 1
    return rc


def cmd_extract(args):
    targets = _as_chain(args.archive, args.d2_root)
    internal = args.path.replace("/", "\\")
    for path in targets:
        with D2Archive(path) as a:
            flags = a.flags_for(internal)
            data = a.read(internal)
        if data is None:
            continue
        if args.out:
            os.makedirs(os.path.dirname(os.path.abspath(args.out)) or ".",
                        exist_ok=True)
            with open(args.out, "wb") as fh:
                fh.write(data)
            sys.stderr.write(
                "%s <- %s (%d bytes, flags %s, sha256 %s)\n"
                % (args.out, os.path.basename(path), len(data),
                   describe_flags(flags), hashlib.sha256(data).hexdigest()[:16]))
            if internal.lower().endswith(".txt"):
                lines, recs = record_count(data)
                sys.stderr.write("   %d lines, %d records\n" % (lines, recs))
        else:
            sys.stdout.buffer.write(data)
        return 0
    sys.stderr.write("d2mpq: %r not found in: %s\n"
                     % (internal, ", ".join(os.path.basename(p) for p in targets)))
    return 1


def _all_excel_names(chain):
    """Every ``excel\\*.txt`` any archive in the chain *declares*.

    D2's ``(listfile)``s are incomplete -- monstats2.txt, monpreset.txt,
    monplace.txt and monseq.txt are all real files that no listfile mentions.
    So this is a floor, not a census; the curated :data:`EXCEL_TABLES` list
    covers the ones the listfiles miss.
    """
    names = set()
    for arc in chain:
        with D2Archive(arc) as a:
            for n in a.namelist():
                low = n.lower()
                if low.startswith(EXCEL_DIR) and low.endswith(".txt"):
                    names.add(low.rsplit("\\", 1)[-1])
    names.update(t.lower() for t in EXCEL_TABLES)
    return sorted(names)


def cmd_tables(args):
    chain = archive_chain(args.version, args.d2_root)
    out_dir = args.out_dir or cache_path_for(args.version, args.cache_dir)
    os.makedirs(out_dir, exist_ok=True)

    if args.table:
        wanted = list(args.table)
    elif args.all_excel:
        wanted = _all_excel_names(chain)
    else:
        wanted = list(EXCEL_TABLES)
    explicit = bool(args.table)
    label = version_label(args.version)
    known = KNOWN_GOOD.get(label, {})

    manifest = {"version": label,
                "tree": version_dir(args.version, args.d2_root),
                "chain": [os.path.basename(p) for p in chain],
                "tables": {}}
    missing, failed = [], []

    print("chain: %s" % " > ".join(os.path.basename(p) for p in chain))
    print("out:   %s" % out_dir)
    print()
    print("%-22s %-14s %9s %7s %7s  %s"
          % ("table", "source", "bytes", "lines", "records", "check"))

    for name in wanted:
        try:
            src, data = read_excel(chain, name)
        except MpqReadError as exc:
            failed.append((name, str(exc)))
            print("%-22s %s" % (name, "READ FAILED: %s" % exc))
            continue
        if data is None:
            missing.append(name)
            print("%-22s %-14s %9s" % (name, "-", "not in this version"))
            continue

        lines, recs = record_count(data)
        note = ""
        if not _looks_like_text(data):
            failed.append((name, "output is not text -- decompression is wrong"))
            note = "NOT TEXT"
        elif name in known:
            note = "ok" if recs == known[name] else \
                   "MISMATCH expected %d" % known[name]
            if recs != known[name]:
                failed.append((name, "expected %d records, got %d"
                               % (known[name], recs)))

        dest = os.path.join(out_dir, name)
        with open(dest, "wb") as fh:
            fh.write(data)
        manifest["tables"][name] = {
            "source": os.path.basename(src),
            "bytes": len(data),
            "lines": lines,
            "records": recs,
            "sha256": hashlib.sha256(data).hexdigest(),
        }
        print("%-22s %-14s %9d %7d %7d  %s"
              % (name, os.path.basename(src), len(data), lines, recs, note))

    with open(os.path.join(out_dir, "manifest.json"), "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=2, sort_keys=True)

    print()
    print("wrote %d table(s) to %s" % (len(manifest["tables"]), out_dir))
    if missing:
        print("absent from %s (not an error unless you asked for them by name): %s"
              % (args.version, ", ".join(missing)))
    if failed:
        print()
        print("FAILURES:")
        for name, why in failed:
            print("  %s: %s" % (name, why))
        return 1
    if missing and explicit:
        print("requested table(s) not found: %s" % ", ".join(missing))
        return 1
    return 0


def cmd_counts(args):
    for path in args.files:
        with open(path, "rb") as fh:
            data = fh.read()
        lines, recs = record_count(data)
        hits = [m for m in PD2_MARKERS if m.encode("latin-1") in data]
        print("%-40s %8d bytes %6d lines %6d records%s"
              % (os.path.basename(path), len(data), lines, recs,
                 "  PD2 MARKERS: " + ",".join(hits) if hits else ""))
    return 0


def cmd_selfcheck(args):
    """Prove the reader still works, and show what mpyq would have said."""
    version = args.version
    expected = KNOWN_GOOD.get(version_label(version))
    if expected is None:
        print("selfcheck: no known-good counts recorded for %s (have: %s)"
              % (version, ", ".join(sorted(KNOWN_GOOD))))
        return 1

    print("selfcheck %s" % version)
    try:
        chain = archive_chain(version, args.d2_root)
    except MpqReadError as exc:
        print("  FAIL: %s" % exc)
        return 1
    print("  chain: %s" % " > ".join(os.path.basename(p) for p in chain))
    print()

    failures = []

    # 1. Round-trip explode on a synthetic stream is not possible (there is no
    #    Python DCL compressor), so the standing proof is real game data.
    print("%-16s %10s %8s %8s   %10s %8s %8s   %s"
          % ("table", "d2mpq B", "lines", "records",
             "mpyq B", "lines", "records", "verdict"))
    for name, want in sorted(expected.items()):
        ipath = EXCEL_DIR + name
        src = None
        good = None
        for arc in chain:
            with D2Archive(arc) as a:
                try:
                    good = a.read(ipath)
                except MpqReadError as exc:
                    failures.append("%s: %s" % (name, exc))
                    good = None
                    break
            if good is not None:
                src = arc
                break
        if good is None:
            failures.append("%s: not readable from the chain" % name)
            print("%-16s  UNREADABLE" % name)
            continue

        gl, gr = record_count(good)

        # What plain mpyq returns for the exact same file.
        raw_archive = mpyq.MPQArchive(src, listfile=False)
        try:
            bad = raw_archive.read_file(ipath)
        except Exception as exc:
            bad_desc = "raised %s" % type(exc).__name__
            bl = br = -1
            bad = None
        finally:
            raw_archive.file.close()
        if bad is not None:
            bl, br = record_count(bad)
            bad_desc = "%10d %8d %8d" % (len(bad), bl, br)
        else:
            bad_desc = "%28s" % bad_desc

        ok = (gr == want)
        if not ok:
            failures.append("%s: expected %d records, got %d" % (name, want, gr))
        if not _looks_like_text(good):
            failures.append("%s: output is not text" % name)
            ok = False
        print("%-16s %10d %8d %8d   %s   %s"
              % (name, len(good), gl, gr, bad_desc,
                 "ok" if ok else "FAIL (expected %d)" % want))

    # 2. Structural coverage.  The excel tables are all sector-compressed and
    #    none of their sizes is an exact multiple of the sector size, so they
    #    never exercise the stored-uncompressed path or the sector-count
    #    ceiling.  Both of those were wrong once; this keeps them honest by
    #    reading the smallest real example of each shape in the archives.
    print()
    print("  structural coverage (paths the excel tables never reach):")
    shapes = {}
    wav = None
    # One pass: each hash-table lookup is three pure-Python hashes, so scanning
    # every name in every archive is minutes of work for no extra coverage.
    SCAN_LIMIT = 1200
    for arc in chain:
        try:
            with D2Archive(arc) as a:
                if not a.has_listfile:
                    continue
                ss = a.sector_size
                names = a.namelist()
                if len(names) > SCAN_LIMIT:
                    stride = len(names) / float(SCAN_LIMIT)
                    names = [names[int(i * stride)] for i in range(SCAN_LIMIT)]
                for name, flags, size, arch in a.entries(names):
                    if size == 0 or size > 4 << 20:
                        continue
                    if name.lower().endswith((".wav", ".bik")):
                        # Blizzard WAVE codec: out of scope, asserted below.
                        if wav is None and name.lower().endswith(".wav"):
                            wav = (arc, name)
                        continue
                    comp = flags & (MPQ_FILE_IMPLODE | MPQ_FILE_COMPRESS)
                    single = bool(flags & MPQ_FILE_SINGLE_UNIT)
                    kind = ("stored" if not comp else
                            "imploded" if flags & MPQ_FILE_IMPLODE else "compressed")
                    if single:
                        kind += "/single-unit"
                    elif size > ss and size % ss == 0:
                        kind += "/exact-sector-multiple"
                    else:
                        kind += "/sectored"
                    if flags & MPQ_FILE_ENCRYPTED:
                        kind += "/encrypted"
                    prev = shapes.get(kind)
                    if prev is None or size < prev[2]:
                        shapes[kind] = (arc, name, size)
        except MpqReadError:
            continue

    if not shapes:
        print("    (no enumerable archives -- coverage not checked)")
    for kind, (arc, name, size) in sorted(shapes.items()):
        try:
            with D2Archive(arc) as a:
                got = a.read(name, required=True)
            verdict = "ok" if len(got) == size else "FAIL wrong length %d" % len(got)
            if len(got) != size:
                failures.append("%s: read %d bytes, expected %d" % (name, len(got), size))
        except MpqReadError as exc:
            verdict = "FAIL %s" % exc
            failures.append("%s (%s): %s" % (name, kind, exc))
        print("    %-40s %-46s %8d  %s"
              % (kind, name[-46:], size, verdict))

    # 3. The known gap must stay loud.  Audio uses Blizzard's Huffman+ADPCM
    #    WAVE codec, which this tool does not implement -- what matters is that
    #    it says so instead of returning the compressed bytes.
    if wav:
        arc, name = wav
        try:
            with D2Archive(arc) as a:
                got = a.read(name, required=True)
                want = dict((n, s) for n, _f, s, _a in a.entries([name]))[name]
            # Not every .wav uses the WAVE codec -- some are stored or zlib.
            # Either outcome is correct; what must never happen is a wrong
            # length, which the block-size check already forbids.
            ok = len(got) == want
            print("  known gap: %s read cleanly (%d bytes) -- not WAVE-coded%s"
                  % (name, len(got), "" if ok else "  FAIL wrong length"))
            if not ok:
                failures.append("%s: read %d bytes, expected %d"
                                % (name, len(got), want))
        except MpqReadError as exc:
            if "WAVE codec" in str(exc):
                print("  known gap: audio (Huffman+ADPCM WAVE) raises rather "
                      "than returning compressed bytes -- ok")
            else:
                print("  known gap: %s -- FAIL" % exc)
                failures.append("%s: %s" % (name, exc))

    # 4. No mod contamination.
    print()
    _, objects = read_excel(chain, "objects.txt")
    if objects is None:
        failures.append("objects.txt unreadable for the marker check")
    else:
        for marker in PD2_MARKERS:
            n = objects.count(marker.encode("latin-1"))
            print("  marker %-18s occurrences in vanilla objects.txt: %d %s"
                  % (marker, n, "ok" if n == 0 else "FAIL -- this tree is modded"))
            if n:
                failures.append("%s present in objects.txt (%d)" % (marker, n))

    print()
    if failures:
        print("SELFCHECK FAILED (%d):" % len(failures))
        for f in failures:
            print("  %s" % f)
        return 1
    print("SELFCHECK PASSED -- %d tables at their known-good record counts, "
          "no mod markers." % len(expected))
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def build_parser():
    p = argparse.ArgumentParser(
        prog="d2mpq.py",
        description="Read Diablo II MPQ archives, including PKWARE-imploded "
                    "files that mpyq silently returns compressed.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Game trees are located by --d2-root / $D2_VERSIONS_ROOT "
               "(default: %s).\nExtracted tables are cached in %s, which is "
               "gitignored: the reader is ours, the data is Blizzard's."
               % (DEFAULT_D2_ROOT, os.path.relpath(DEFAULT_CACHE_DIR, REPO_ROOT)))
    p.add_argument("--d2-root", default=None,
                   help="directory holding the per-version game trees")
    p.add_argument("--cache-dir", default=None,
                   help="override the local extraction cache")
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("list", help="list an archive's files")
    s.add_argument("archive", help="path to an .mpq, or a version name/dir")
    s.add_argument("--pattern", help=r"glob filter, e.g. 'data\global\excel\*'")
    s.set_defaults(func=cmd_list)

    s = sub.add_parser("extract", help="extract one file")
    s.add_argument("archive", help="path to an .mpq, or a version name/dir")
    s.add_argument("path", help=r"internal path, e.g. data\global\excel\objects.txt")
    s.add_argument("--out", help="write here instead of stdout")
    s.set_defaults(func=cmd_extract)

    s = sub.add_parser("tables", help="bulk-extract the common excel tables")
    s.add_argument("version", help="version name (1.13c) or a game tree path")
    s.add_argument("--out-dir", help="default: the gitignored .vanilla-cache/")
    s.add_argument("--table", action="append", default=[],
                   help="extract only this table (repeatable)")
    s.add_argument("--all-excel", action="store_true",
                   help="every excel\\*.txt the archives' listfiles declare, "
                        "plus the curated set (D2 listfiles are incomplete)")
    s.set_defaults(func=cmd_tables)

    s = sub.add_parser("counts", help="line/record counts for extracted .txt files")
    s.add_argument("files", nargs="+")
    s.set_defaults(func=cmd_counts)

    s = sub.add_parser("selfcheck",
                       help="verify known files decompress to sane record counts")
    s.add_argument("--version", default="1.13c")
    s.set_defaults(func=cmd_selfcheck)
    return p


def main(argv=None):
    args = build_parser().parse_args(argv)
    try:
        return args.func(args)
    except MpqReadError as exc:
        sys.stderr.write("d2mpq: %s\n" % exc)
        return 2


if __name__ == "__main__":
    sys.exit(main())
