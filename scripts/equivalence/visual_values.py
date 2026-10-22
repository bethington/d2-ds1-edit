"""Authored vectors and independent expectations for palette, tile-key and
AnimData behaviour (package D, task 1).

Everything here is computed from the documented 1.13c rules
(docs/guides/ds1-map-format.md, animdata-d2.md, palettes-and-colour.md), not
from the editor. The editor's C tests consume the fixture this module writes;
the Python tests run the controls (a seven-bit main-index mask, a wrong
record stride, a case-sensitive hash) and show each one disagrees with the
expectation on at least one authored vector.

Commands (from the repository root)::

    python -m scripts.equivalence.visual_values write-fixtures
    python -m scripts.equivalence.visual_values check-fixtures

Standard library only.
"""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FIXTURE_DIR = REPO_ROOT / "test" / "equivalence" / "fixtures"
ANIMDATA_FIXTURE = FIXTURE_DIR / "animdata-authored.bin"
CONTRACT_PATH = REPO_ROOT / "test" / "equivalence" / "contracts" / "visual-values-113c.json"

# ---------------------------------------------------------------------------
# DS1 cell -> tile key (ds1-map-format.md, "The cell": D2Common 6fd5c32a..)
# ---------------------------------------------------------------------------


def cell_dword(prop1: int, prop2: int, prop3: int, prop4: int) -> int:
    return prop1 | (prop2 << 8) | (prop3 << 16) | (prop4 << 24)


def tile_key(prop1, prop2, prop3, prop4, mask=0x3F):
    """(main_index, sub_index) as D2Common extracts them; mask is the control
    knob (0x3F is the game's AND ESI,0x3f)."""
    d = cell_dword(prop1, prop2, prop3, prop4)
    return ((d >> 20) & mask, (d >> 8) & 0xFF)


#: The 25-entry v<7 orientation table at 0x6fddbcc0 (ds1-map-format.md).
LEGACY_ORIENTATION = [0, 1, 2, 1, 2, 3, 3, 5, 5, 6, 6, 7, 7, 8, 9, 10, 11, 12,
                      13, 14, 15, 16, 17, 18, 20]

# ---------------------------------------------------------------------------
# AnimData.d2 (animdata-d2.md)
# ---------------------------------------------------------------------------

RECORD_SIZE = 160
BUCKETS = 256


def animdata_hash(name: str, case_fold=True) -> int:
    """Uppercase a-z, sum the bytes, keep the low 8 bits."""
    total = 0
    for ch in name.encode("ascii"):
        if case_fold and 0x61 <= ch <= 0x7A:
            ch -= 0x20
        total += ch
    return total & 0xFF


#: Authored records: (name, frames_per_direction, speed, first event byte).
#: AMTNHTH, AMNTHTH and TNMAHTH share a hash (20); NUZZHTH is alone.
ANIMDATA_RECORDS = [
    ("AMTNHTH", 16, 80, 0),
    ("AMNTHTH", 8, 256, 1),
    ("TNMAHTH", 12, 128, 2),
    ("NUZZHTH", 20, 0, 0),
    ("OBOPHTH", 4, 512, 0),
]

#: Names that hash into a non-empty bucket but are not stored there.
ANIMDATA_MISSES = ["HTHAMTN", "AMTNHT", "ZZZZZZZ"]


def build_animdata(records=ANIMDATA_RECORDS, stride=RECORD_SIZE) -> bytes:
    buckets = [[] for _ in range(BUCKETS)]
    for rec in records:
        buckets[animdata_hash(rec[0])].append(rec)
    out = bytearray()
    for bucket in buckets:
        out += struct.pack("<i", len(bucket))
        for name, fpd, speed, event in bucket:
            body = bytearray(stride)
            raw = name.encode("ascii")
            body[0:len(raw)] = raw
            struct.pack_into("<ii", body, 8, fpd, speed)
            body[16] = event
            out += body
    return bytes(out)


def animdata_lookup(data: bytes, name: str, stride=RECORD_SIZE, case_fold=True):
    """Independent bucket walk: (fpd, speed) or None."""
    want = animdata_hash(name, case_fold)
    pos = 0
    for bucket in range(BUCKETS):
        (count,) = struct.unpack_from("<i", data, pos)
        pos += 4
        if bucket == want:
            for i in range(count):
                rec = data[pos + i * stride: pos + i * stride + stride]
                # 1.13c 6fd91dd0 uppercases the query; 6fd91d00 then compares
                # the whole 8-byte stored field exactly (two dwords).
                query = (name.upper() if case_fold else name).encode("ascii")
                if len(query) <= 8 and rec[:8] == query.ljust(8, b"\0"):
                    return struct.unpack_from("<ii", rec, 8)
            return None
        pos += count * stride
    return None


def frame_delay_ms(speed: int, default_ms: int) -> int:
    """Editor policy (not a game claim): 25 Hz ticks, speed/256 frames per tick,
    integer milliseconds truncated, 1..10000, default when speed is 0 or
    out of range."""
    if speed <= 0 or speed > 65535:
        return default_ms
    ms = 10240 // speed
    if ms <= 0:
        return default_ms
    return min(ms, 10000)


# ---------------------------------------------------------------------------
# Fixtures and contract
# ---------------------------------------------------------------------------


def write_fixtures() -> None:
    FIXTURE_DIR.mkdir(parents=True, exist_ok=True)
    ANIMDATA_FIXTURE.write_bytes(build_animdata())


def check_fixtures() -> list:
    problems = []
    if not ANIMDATA_FIXTURE.is_file():
        problems.append("missing %s" % ANIMDATA_FIXTURE.name)
    elif ANIMDATA_FIXTURE.read_bytes() != build_animdata():
        problems.append("%s differs from the generator" % ANIMDATA_FIXTURE.name)
    return problems


def load_contract(path: Path = CONTRACT_PATH) -> dict:
    return json.loads(Path(path).read_text("utf-8"))


def main(argv=None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    if argv == ["write-fixtures"]:
        write_fixtures()
        print("wrote", ANIMDATA_FIXTURE)
        return 0
    if argv == ["check-fixtures"]:
        problems = check_fixtures()
        for p in problems:
            print(p)
        return 1 if problems else 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
