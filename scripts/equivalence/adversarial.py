"""Package E task 1: deterministic malformed-input campaigns for the C probes.

Generates corrupted variants of authored, valid inputs (DS1, DT1, DCC, DC6,
COF, AnimData), feeds each to the real production probe in a bounded
subprocess, and classifies the result.

* A case is **controlled** when the probe exits with its declared accept code
  or its declared rejection code, writes only inside its own output directory,
  and finishes inside the time limit.
* A **defect** is anything else: a crash (any other exit code, including
  0xC0000005 or a signal), a timeout, a file outside the case's own output
  directory, or an accepted run whose output is unreadable.
* Known vanilla-compatible tails run as **controls** with an exact case
  identity and must be accepted. Nothing exempts a mutation from the crash
  rules, and only DS1 truncations that cut a required section have a
  "must reject" expectation (silent success there is a defect too).

Everything is deterministic: the profile seed plus the kind and sample id seed
each generator, so a failing case id and the artifacts written for it
(``failures/<id>/input.bin`` and ``repro.json``) reproduce it exactly. No
shell is involved; commands are argv lists.

Commands (repository root)::

    python -m scripts.equivalence.adversarial run --profile P
        --probe-ds1 EXE --probe-visual EXE --probe-robust EXE --out-dir DIR

Exit 0 only when every case was controlled and none was truncated or missing.

Standard library only.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import random
import shutil
import struct
import subprocess
import sys
from pathlib import Path
from typing import Callable, Iterable

REPO_ROOT = Path(__file__).resolve().parents[2]
CASE_SCHEMA = "ds1edit.case/1"
PROFILE_SCHEMA = "ds1edit.adversarial-profile/1"

KINDS = ("ds1", "dt1", "dcc", "dc6", "cof", "animdata")

DEFAULT_TIMEOUT_S = 10.0
DEFAULT_MAX_PAYLOAD = 1 << 20

#: 32-bit values that break length/count/offset arithmetic.
EXTREMES32 = (0, 0x80, 0xFFFF, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF)
#: Byte values for bitstreams and signed widths.
EXTREMES8 = (0x00, 0xFF, 0x7F, 0x80)

STATUS_DLL_NOT_FOUND = 0xC0000135
_MUTATION_CAP = 4096          # sanity bound per (kind, sample): never silent, see below


class AdversarialError(ValueError):
    """The profile or the probes given to the campaign are unusable."""


# ---------------------------------------------------------------------------
# sample bytes (authored, valid)
# ---------------------------------------------------------------------------

def _ds1_cases() -> dict:
    sys.path.insert(0, str(REPO_ROOT))
    from test.fixtures import make_ds1_fixtures as m  # noqa: E402
    return {c["id"]: m.case_bytes(c) for c in m.build_cases()}


def _decoder_fixture(name: str) -> bytes:
    return (REPO_ROOT / "test" / "equivalence" / "fixtures" / "decoders" / name).read_bytes()


def synth_cof() -> bytes:
    """3 layers, 2 frames per direction, 4 directions; header is 28 bytes."""
    layers, fpd, dirs = 3, 2, 4
    out = bytearray(28)
    out[0], out[1], out[2], out[3] = layers, fpd, dirs, 20
    for idx, wc in ((0, b"HTH "), (1, b"1HS "), (2, b"\0\0\0\0")):
        out += bytes([idx, 0x10 + idx, 0x20 + idx, idx, idx]) + wc
    out += bytes(fpd)
    for _ in range(dirs * fpd):
        out += bytes([1, 2, 0])
    return bytes(out)


def synth_dt1() -> bytes:
    """Two blocks: an isometric floor tile and an RLE wall tile (276-byte header,
    96-byte block headers, 20-byte sub-tile headers)."""
    def p16(v): return struct.pack("<H", v & 0xFFFF)
    def p32(v): return struct.pack("<i", v) if v < 0x80000000 else struct.pack("<I", v)

    iso = bytes(1 + (i % 160) for i in range(256))
    rle = bytes([2, 3, 11, 12, 13, 1, 1, 14, 0, 0, 0, 2, 21, 22, 0, 0])
    blocks = [  # direction, roof_y, animated, size_y, size_x, orient, main, sub, rarity, format, data
        (3, 0, 0, -80, 160, 0, 5, 2, 7, 1, iso),
        (0, 40, 1, -96, 160, 1, 9, 4, 1, 0, rle),
    ]
    nb = len(blocks)
    head = bytearray(276 + 96 * nb)
    head[0:4] = p32(7)
    head[4:8] = p32(6)
    head[268:272] = p32(nb)
    head[272:276] = p32(276)
    body = bytearray()
    pos = len(head)
    for b, (dirn, roof, anim, sy, sx, ori, main, sub, rar, fmt, data) in enumerate(blocks):
        bh = 276 + 96 * b
        tiles_ptr = pos
        sub_hdr = bytearray(20)
        sub_hdr[0:2] = p16(64 if fmt else 10)
        sub_hdr[2:4] = p16(0 if fmt else (-64) & 0xFFFF)
        sub_hdr[8:10] = p16(fmt)
        sub_hdr[10:12] = p16(len(data))
        sub_hdr[16:20] = p32(20)
        chunk = bytes(sub_hdr) + data
        head[bh + 0:bh + 4] = p32(dirn)
        head[bh + 4:bh + 6] = p16(roof)
        head[bh + 7] = anim
        head[bh + 8:bh + 12] = p32(sy)
        head[bh + 12:bh + 16] = p32(sx)
        head[bh + 20:bh + 24] = p32(ori)
        head[bh + 24:bh + 28] = p32(main)
        head[bh + 28:bh + 32] = p32(sub)
        head[bh + 32:bh + 36] = p32(rar)
        head[bh + 72:bh + 76] = p32(tiles_ptr)
        head[bh + 76:bh + 80] = p32(len(chunk))
        head[bh + 80:bh + 84] = p32(1)
        body += chunk
        pos += len(chunk)
    return bytes(head) + bytes(body)


def valid_samples(kind: str) -> dict:
    """Authored valid inputs, ``{sample_id: bytes}``."""
    if kind == "ds1":
        cases = _ds1_cases()
        return {k: cases[k] for k in ("v03_default", "v06_default", "v13_default",
                                      "v18_default", "v18_four_walls")}
    if kind == "dt1":
        return {"synthetic_floor_wall": synth_dt1()}
    if kind == "dcc":
        return {n[:-4]: _decoder_fixture(n) for n in
                ("dcc_single.dcc", "dcc_sequence.dcc", "dcc_flag0.dcc", "dcc_empty_dir.dcc")}
    if kind == "dc6":
        return {n[:-4]: _decoder_fixture(n) for n in
                ("dc6_rle_basic.dc6", "dc6_multi_direction.dc6", "dc6_clip.dc6")}
    if kind == "cof":
        return {"synthetic_3layer": synth_cof(),
                "authored_amtnhth": (REPO_ROOT / "test" / "equivalence" / "fixtures"
                                     / "exports" / "compose" / "AMTNHTH.cof").read_bytes()}
    if kind == "animdata":
        return {"authored": (REPO_ROOT / "test" / "equivalence" / "fixtures"
                             / "animdata-authored.bin").read_bytes()}
    raise AdversarialError("unknown kind %r" % kind)


def compat_controls(kind: str) -> dict:
    """Known vanilla-compatible inputs the reader must accept, by exact id.

    Each id names one documented tail (docs/guides/ds1-map-format.md); none of
    them widens what a mutation may do.
    """
    if kind == "ds1":
        cases = _ds1_cases()
        return {k: cases[k] for k in ("v13_group_block_cut_after_tile_x",
                                      "v18_missing_npc_count",
                                      "v13_trailing_48_zero_bytes")}
    return {}


# ---------------------------------------------------------------------------
# structure probes (boundaries and field offsets, derived from the bytes)
# ---------------------------------------------------------------------------

def _u32(data: bytes, pos: int):
    if pos < 0 or pos + 4 > len(data):
        return None
    return struct.unpack_from("<I", data, pos)[0]


def ds1_layout(data: bytes) -> dict | None:
    """Section offsets of a well-formed DS1 (``None`` if it is not one)."""
    v = _u32(data, 0)
    w = _u32(data, 4)
    h = _u32(data, 8)
    if v is None or w is None or h is None or v > 64:
        return None
    pos = 12
    lay = {"version": v, "header_dwords": [0, 4, 8]}
    tag_type = 0
    if v >= 8:
        lay["header_dwords"].append(pos)
        pos += 4
    if v >= 10:
        tag_type = _u32(data, pos) or 0
        lay["header_dwords"].append(pos)
        pos += 4
    lay["files_at"] = pos
    lay["counts"] = []
    if v >= 3:
        n = _u32(data, pos)
        if n is None or n > 1000:
            return None
        lay["counts"].append(pos)
        pos += 4
        for _ in range(n):
            nul = data.find(b"\0", pos)
            if nul < 0:
                return None
            pos = nul + 1
        lay["strings_end"] = pos
    if 9 <= v <= 13:
        pos += 8
    wall_num, floor_num = 1, 1
    if v >= 4:
        wall_num = _u32(data, pos) or 0
        lay["counts"].append(pos)
        pos += 4
        if v >= 16:
            floor_num = _u32(data, pos) or 0
            lay["counts"].append(pos)
            pos += 4
    lay["layers_at"] = pos
    cells = (w + 1) * (h + 1)
    has_tags = v < 4 or (v >= 10 and tag_type in (1, 2))
    nlayers = 5 if v < 4 else 2 * wall_num + floor_num + 1 + (1 if has_tags else 0)
    pos += nlayers * cells * 4
    lay["objects_at"] = pos
    obj_rec = 20 if v >= 6 else 16
    if v >= 2:
        n = _u32(data, pos)
        if n is None:
            return None
        lay["counts"].append(pos)
        pos += 4 + n * obj_rec
    lay["groups_at"] = pos
    if v >= 12 and tag_type in (1, 2):
        if v >= 18:
            pos += 4
        n = _u32(data, pos)
        if n is None:
            return None
        lay["counts"].append(pos)
        pos += 4 + n * (20 if v >= 13 else 16)
    lay["paths_at"] = pos
    if v >= 14:
        n = _u32(data, pos)
        if n is not None:
            lay["counts"].append(pos)
            lay["npc_count"] = n
            q = pos + 4
            for _ in range(min(n, 64)):
                m = _u32(data, q)
                if m is None:
                    break
                lay["counts"].append(q)
                q += 12 + (12 if v >= 15 else 8) * m
    lay["end"] = len(data)
    return lay


def _boundaries(kind: str, data: bytes) -> list[int]:
    n = len(data)
    pts = {0, 1, 2, 3, 4}
    if kind == "ds1":
        lay = ds1_layout(data)
        if lay:
            for k in ("files_at", "strings_end", "layers_at", "objects_at",
                      "groups_at", "paths_at"):
                if k in lay:
                    pts.add(lay[k])
            pts.update(lay["header_dwords"])
            pts.update(lay["counts"])
    elif kind == "dt1":
        pts.update({8, 268, 272, 276})
        nb = _u32(data, 268) or 0
        for b in range(min(nb, 64)):
            bh = 276 + 96 * b
            pts.update({bh, bh + 96})
            tp = _u32(data, bh + 72)
            if tp is not None:
                pts.update({tp, tp + 20})
    elif kind == "dcc":
        dirs = data[2] if len(data) > 2 else 0
        pts.update({15, 15 + 4 * dirs})
        for d in range(dirs):
            off = _u32(data, 15 + 4 * d)
            if off is not None:
                pts.update({off, off + 4, off + 6, off + 12, off + 24})
    elif kind == "dc6":
        pts.update({8, 12, 16, 20, 24})
        dirs, fpd = _u32(data, 16) or 0, _u32(data, 20) or 0
        nptr = min(dirs * fpd, 256)
        pts.add(24 + 4 * nptr)
        for i in range(nptr):
            off = _u32(data, 24 + 4 * i)
            if off is not None:
                pts.update({off, off + 8, off + 32})
    elif kind == "cof":
        lays = data[0] if data else 0
        fpd = data[1] if len(data) > 1 else 0
        dirs = data[2] if len(data) > 2 else 0
        pts.update({27, 28, 29})
        for i in range(lays + 1):
            pts.add(28 + 9 * i)
        flags = 28 + 9 * lays
        pts.update({flags, flags + fpd, flags + fpd + dirs * fpd * lays - 1})
    elif kind == "animdata":
        pos = 0
        for _ in range(256):
            c = _u32(data, pos)
            if c is None:
                break
            pts.update({pos, pos + 4})
            if c:
                pts.update({pos + 4 + 160, pos + 4 + 160 * c - 1})
            pos += 4 + 160 * c
            if pos > n:
                break
    return sorted(p for p in pts if 0 <= p < n)


def _u32_fields(kind: str, data: bytes) -> list[int]:
    """Offsets of 32-bit length/count/offset fields worth driving to extremes."""
    f: list[int] = []
    if kind == "ds1":
        lay = ds1_layout(data)
        if lay:
            f = list(lay["header_dwords"]) + list(lay["counts"])
    elif kind == "dt1":
        f = [0, 4, 268, 272]
        nb = min(_u32(data, 268) or 0, 8)
        for b in range(nb):
            bh = 276 + 96 * b
            f += [bh + o for o in (0, 8, 12, 20, 24, 28, 32, 72, 76, 80)]
            tp = _u32(data, bh + 72)
            if tp is not None:
                f += [tp + 16]
                # sub-tile x/y/format/length as dwords-with-neighbours
                f += [tp, tp + 8, tp + 10]
    elif kind == "dcc":
        dirs = data[2] if len(data) > 2 else 0
        f = [3, 7, 11] + [15 + 4 * d for d in range(dirs)]
        for d in range(dirs):
            off = _u32(data, 15 + 4 * d)
            if off is not None:
                f.append(off)
    elif kind == "dc6":
        dirs, fpd = _u32(data, 16) or 0, _u32(data, 20) or 0
        f = [0, 4, 8, 12, 16, 20]
        nptr = min(dirs * fpd, 32)
        f += [24 + 4 * i for i in range(nptr)]
        for i in range(nptr):
            off = _u32(data, 24 + 4 * i)
            if off is not None:
                f += [off + 4 * k for k in range(8)]
    elif kind == "animdata":
        pos = 0
        for _ in range(256):
            c = _u32(data, pos)
            if c is None:
                break
            if c:
                f.append(pos)
            pos += 4 + 160 * c
            if pos > len(data):
                break
        f.append(0)
    return sorted({p for p in f if 0 <= p and p + 4 <= len(data)})


def _byte_windows(kind: str, data: bytes) -> list[tuple[int, int]]:
    """(start, stop) ranges where single bytes are driven to 8-bit extremes."""
    if kind == "dcc":
        wins = [(0, 15)]
        dirs = data[2] if len(data) > 2 else 0
        for d in range(dirs):
            off = _u32(data, 15 + 4 * d)
            if off is not None:
                wins.append((off, min(off + 24, len(data))))     # direction header + widths
        return wins
    if kind == "dc6":
        wins = []
        dirs, fpd = _u32(data, 16) or 0, _u32(data, 20) or 0
        for i in range(min(dirs * fpd, 32)):
            off = _u32(data, 24 + 4 * i)
            if off is not None:
                wins.append((off + 32, min(off + 32 + 12, len(data))))  # first RLE bytes
        return wins
    if kind == "cof":
        lays = data[0] if data else 0
        wins = [(0, 4)]
        wins += [(28 + 9 * i, 28 + 9 * i + 9) for i in range(min(lays, 16))]
        wins.append((28 + 9 * lays, min(len(data), 28 + 9 * lays + 6)))
        return wins
    if kind == "dt1":
        return [(276 + 20 * 0, min(len(data), 276 + 96 * 2 + 40))]
    return []


# ---------------------------------------------------------------------------
# mutation generator
# ---------------------------------------------------------------------------

def _derive_seed(seed: int, *parts: str) -> int:
    h = hashlib.sha256(("%d:%s" % (seed, ":".join(parts))).encode()).digest()
    return int.from_bytes(h[:8], "little")


def mutations(kind: str, valid_bytes: bytes, seed: int) -> Iterable[tuple[str, bytes]]:
    """Deterministic corrupted variants of ``valid_bytes`` as ``(name, bytes)``.

    Names are stable and unique; the bytes are pairwise distinct and differ
    from the input. The same ``(kind, valid_bytes, seed)`` always yields the
    same sequence. No variant is longer than the input plus 8 bytes.
    """
    if kind not in KINDS:
        raise AdversarialError("unknown kind %r" % kind)
    data = bytes(valid_bytes)
    n = len(data)
    seen = {hashlib.sha256(data).hexdigest()}
    rng = random.Random(_derive_seed(seed, kind, hashlib.sha256(data).hexdigest()))

    def emit(name: str, blob: bytes):
        h = hashlib.sha256(blob).hexdigest()
        if h in seen:
            return None
        seen.add(h)
        return name, blob

    out: list[tuple[str, bytes]] = []

    def add(name: str, blob: bytes) -> None:
        r = emit(name, blob)
        if r is not None:
            out.append(r)

    # truncations: header/record boundaries, and the last 1-3 bytes
    cuts = set()
    for b in _boundaries(kind, data):
        cuts.update({b - 1, b, b + 1})
    cuts.update({n - 1, n - 2, n - 3, 0, 1})
    if n <= 400:
        cuts.update(range(n))
    for c in sorted(cuts):
        if 0 <= c < n:
            add("trunc@%d" % c, data[:c])

    # 32-bit length/count/offset extremes
    for pos in _u32_fields(kind, data):
        for val in EXTREMES32 + (n + 1,):
            blob = bytearray(data)
            blob[pos:pos + 4] = struct.pack("<I", val & 0xFFFFFFFF)
            add("u32@%d=0x%x" % (pos, val & 0xFFFFFFFF), bytes(blob))

    # byte extremes in bitstream/width/priority windows
    for start, stop in _byte_windows(kind, data):
        for pos in range(start, min(stop, n)):
            for val in EXTREMES8:
                blob = bytearray(data)
                blob[pos] = val
                add("u8@%d=0x%02x" % (pos, val), bytes(blob))

    if kind == "ds1":
        # missing NUL: turn each filename terminator into a printable byte;
        # then drop the last terminator by cutting the tail.
        lay = ds1_layout(data)
        if lay and "strings_end" in lay:
            for pos in range(lay["files_at"] + 4, lay["strings_end"]):
                if data[pos] == 0:
                    blob = bytearray(data)
                    blob[pos] = 0x41
                    add("nul_missing@%d" % pos, bytes(blob))
            # count inflation: more names than the file contains
            if lay["counts"]:
                pos = lay["counts"][0]
                cnt = _u32(data, pos) or 0
                for extra in (1, 2, 1000, 0x10000000):
                    blob = bytearray(data)
                    blob[pos:pos + 4] = struct.pack("<I", cnt + extra)
                    add("count_inflate@%d+%d" % (pos, extra), bytes(blob))
    if kind == "animdata":
        # a bucket count that fits one record fewer/more than the file holds
        for pos in _u32_fields(kind, data):
            cnt = _u32(data, pos) or 0
            for delta in (-1, 1, 1000):
                val = cnt + delta
                if 0 <= val < 0x100000000:
                    blob = bytearray(data)
                    blob[pos:pos + 4] = struct.pack("<I", val)
                    add("count@%d%+d" % (pos, delta), bytes(blob))

    # seeded random corruption across the whole payload: bit flips, byte sets
    for i in range(24):
        blob = bytearray(data)
        for _ in range(rng.choice((1, 1, 2, 4))):
            pos = rng.randrange(n) if n else 0
            if not n:
                break
            if rng.random() < 0.6:
                blob[pos] ^= 1 << rng.randrange(8)
            else:
                blob[pos] = rng.choice(EXTREMES8 + (rng.randrange(256),))
        add("rand#%d" % i, bytes(blob))
    for i in range(4):  # trailing junk (bounded)
        add("tail#%d" % i, data + bytes(rng.randrange(256) for _ in range(1 + 2 * i)))

    if len(out) > _MUTATION_CAP:
        raise AdversarialError("generator for %s produced %d cases, above its fixed "
                               "bound %d" % (kind, len(out), _MUTATION_CAP))
    return iter(out)


# ---------------------------------------------------------------------------
# running a case
# ---------------------------------------------------------------------------

def _sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def _safe(name: str) -> str:
    return "".join(c if c.isalnum() or c in "._-@+=#" else "_" for c in name)


def _command(kind: str, probes: dict, inp: Path, out: Path) -> tuple[list[str], tuple, tuple]:
    """argv, accepted exit codes, declared rejection codes."""
    if kind == "ds1":
        return ([str(probes["ds1"]), "dump", str(inp), "--out", str(out / "snap.json")],
                (0,), (2,))
    if kind in ("dcc", "dc6", "dt1"):
        return ([str(probes["visual"]), "decode", "--kind", kind, "--input", str(inp),
                 "--direction", "-1" if kind != "dt1" else "0", "--out-dir", str(out)],
                (0,), (1,))
    return ([str(probes["robust"]), kind, "--input", str(inp)], (0,), (1,))


def _describe_rc(rc: int) -> str:
    u = rc & 0xFFFFFFFF
    return "exit code %d (0x%08X)" % (rc, u)


def _tree(root: Path) -> set[str]:
    out = set()
    if root.is_dir():
        for p in root.rglob("*"):
            out.add(str(p.relative_to(root)))
    return out


def _check_output(kind: str, out: Path) -> str | None:
    """None if the accepted run left readable output, else a reason."""
    try:
        if kind == "ds1":
            doc = json.loads((out / "snap.json").read_text("utf-8"))
            if doc.get("status") != "ok":
                return "accepted run wrote status %r" % doc.get("status")
        elif kind in ("dcc", "dc6", "dt1"):
            doc = json.loads((out / "frames.json").read_text("utf-8"))
            for fr in doc.get("frames", []):
                if not (out / fr["file"]).is_file():
                    return "frame file %s missing" % fr["file"]
    except (OSError, ValueError, KeyError) as exc:
        return "accepted run left unreadable output: %s" % exc
    return None


def _must_reject(kind: str, valid: bytes, name: str) -> bool:
    """DS1 truncations inside the header or tile layers cannot be a valid map."""
    if kind != "ds1" or not name.startswith("trunc@"):
        return False
    lay = ds1_layout(valid)
    return bool(lay) and int(name[6:]) < lay["objects_at"]


def run_case(case: dict, probes: dict, work: Path, timeout_s: float,
             roundtrip: bool = False, env: dict | None = None) -> dict:
    """Run one case; returns the ds1edit.case/1 record (see module doc)."""
    kind = case["kind"]
    work = Path(work).resolve()
    work.mkdir(parents=True, exist_ok=True)
    inp = work / "input.bin"
    inp.write_bytes(case["bytes"])
    out = work / "out"
    cwd = work / "cwd"
    out.mkdir(exist_ok=True)
    cwd.mkdir(exist_ok=True)
    argv, accept, reject = _command(kind, probes, inp, out)

    rec = {
        "schema": CASE_SCHEMA, "id": case["id"], "lane": "portable",
        "platform": sys.platform, "required": True, "status": "mismatch",
        "success_outcome": None, "input_sha256": _sha(case["bytes"]),
        "kind": kind, "sample": case["sample"], "mutation": case["mutation"],
        "control": case["control"], "seed": case["seed"], "bytes": len(case["bytes"]),
        "command": argv, "exit_code": None, "outcome": None,
        "failures": [], "limitations": [],
    }

    def fail(outcome: str, detail: str, **extra) -> dict:
        rec["status"] = "mismatch"
        rec["outcome"] = outcome
        rec["failures"].append({"kind": outcome, "detail": detail})
        rec.update(extra)
        return rec

    try:
        proc = subprocess.run(argv, cwd=str(cwd), capture_output=True,
                              timeout=timeout_s, shell=False, env=env)
    except subprocess.TimeoutExpired:
        return fail("timeout", "no exit within %.0f s" % timeout_s)
    except OSError as exc:
        return fail("probe_unavailable", "could not start %s: %s" % (argv[0], exc))

    rc = proc.returncode
    rec["exit_code"] = rc
    rec["stderr_tail"] = (proc.stderr or b"")[-512:].decode("utf-8", "replace")
    if (rc & 0xFFFFFFFF) == STATUS_DLL_NOT_FOUND:
        rec["status"] = "unavailable"
        rec["outcome"] = "environment_error"
        rec["failures"].append({"kind": "environment_error", "detail":
                                "probe could not load a DLL (%s); the build tree is "
                                "incomplete, this is not a reader result" % _describe_rc(rc)})
        return rec
    if rc not in accept and rc not in reject:
        return fail("crash", "%s; declared accept %s, reject %s"
                    % (_describe_rc(rc), list(accept), list(reject)))

    accepted = rc in accept
    stray = sorted(_tree(cwd))
    extra = sorted(p.name for p in work.iterdir()
                   if p.name not in ("input.bin", "out", "cwd"))
    if stray or extra:
        return fail("stray_file", "files outside the case output directory: %s"
                    % ", ".join((["cwd/" + s for s in stray] + extra)[:8]))

    if accepted:
        bad = _check_output(kind, out)
        if bad:
            return fail("bad_output", bad)

    if case["control"] and not accepted:
        return fail("control_rejected",
                    "known-compatible input %s was rejected (exit %d)" % (case["id"], rc))
    if not case["control"] and accepted and case["must_reject"]:
        return fail("silent_accept",
                    "truncated input %s inside a required section was accepted" % case["id"])

    if accepted and kind == "ds1" and roundtrip:
        resaved = out / "resaved.ds1"
        try:
            p2 = subprocess.run([argv[0], "resave", str(inp), "--out", str(resaved)],
                                cwd=str(cwd), capture_output=True, timeout=timeout_s,
                                shell=False, env=env)
        except subprocess.TimeoutExpired:
            return fail("timeout", "resave exceeded %.0f s" % timeout_s)
        if p2.returncode != 0 or not resaved.is_file():
            return fail("resave_failed", "resave of an accepted map exited %d" % p2.returncode)
        p3 = subprocess.run([argv[0], "dump", str(resaved), "--out", str(out / "snap2.json")],
                            cwd=str(cwd), capture_output=True, timeout=timeout_s, shell=False,
                            env=env)
        if p3.returncode != 0:
            return fail("resave_unreadable", "reading the resaved map exited %d" % p3.returncode)
        if sorted(_tree(cwd)):
            return fail("stray_file", "resave wrote into the working directory")

    rec["outcome"] = "accepted" if accepted else "rejected"
    rec["status"] = "pass" if accepted else "rejected_expected"
    rec["success_outcome"] = rec["status"]
    return rec


# ---------------------------------------------------------------------------
# campaign
# ---------------------------------------------------------------------------

def load_profile(path: Path) -> dict:
    prof = json.loads(Path(path).read_text("utf-8"))
    validate_profile(prof)
    return prof


def validate_profile(prof: dict) -> None:
    if not isinstance(prof, dict) or prof.get("schema") != PROFILE_SCHEMA:
        raise AdversarialError("profile schema must be %s" % PROFILE_SCHEMA)
    for key in ("id", "seed", "kinds"):
        if key not in prof:
            raise AdversarialError("profile lacks %r" % key)
    if not isinstance(prof["seed"], int):
        raise AdversarialError("profile seed must be an integer")
    for kind, spec in prof["kinds"].items():
        if kind not in KINDS:
            raise AdversarialError("profile names unknown kind %r" % kind)
        if not isinstance(spec.get("samples", []), list):
            raise AdversarialError("kind %s: samples must be a list" % kind)


def build_cases(profile: dict, sample_source: Callable[[str], dict] = valid_samples,
                control_source: Callable[[str], dict] = compat_controls) -> list[dict]:
    """Every case the profile selects, controls first, in a stable order."""
    validate_profile(profile)
    cap = int(profile.get("max_payload_bytes", DEFAULT_MAX_PAYLOAD))
    seed = profile["seed"]
    cases: list[dict] = []
    for kind in KINDS:
        spec = profile["kinds"].get(kind)
        if spec is None:
            continue
        samples = sample_source(kind)
        wanted = spec.get("samples") or sorted(samples)
        for sid in wanted:
            if sid not in samples:
                raise AdversarialError("kind %s has no sample %r" % (kind, sid))
        for cid, blob in sorted(control_source(kind).items()):
            if spec.get("controls", True):
                cases.append(_case(kind, cid, "control", blob, seed, True, False, cap))
        for sid in wanted:
            valid = samples[sid]
            cases.append(_case(kind, sid, "valid", valid, seed, True, False, cap))
            for name, blob in mutations(kind, valid, _derive_seed(seed, kind, sid)):
                cases.append(_case(kind, sid, name, blob, seed, False,
                                   _must_reject(kind, valid, name), cap))
    return cases


def _case(kind, sample, mutation, blob, seed, control, must_reject, cap) -> dict:
    return {"id": "%s.%s.%s" % (kind, sample, _safe(mutation)), "kind": kind,
            "sample": sample, "mutation": mutation, "bytes": blob, "seed": seed,
            "control": control, "must_reject": must_reject,
            "oversize": len(blob) > cap}


def _persist_failure(rec: dict, case: dict, work: Path, root: Path) -> None:
    dest = root / "failures" / _safe(case["id"])
    dest.mkdir(parents=True, exist_ok=True)
    (dest / "input.bin").write_bytes(case["bytes"])
    repro = {
        "id": case["id"], "kind": case["kind"], "seed": case["seed"],
        "input_sha256": rec["input_sha256"], "outcome": rec["outcome"],
        "exit_code": rec["exit_code"], "failures": rec["failures"],
        "command": [str(dest / "input.bin") if a == str(work / "input.bin") else a
                    for a in rec["command"]],
        "note": "command uses %s as its input; regenerate with mutations(%r, <sample>, seed)"
                % (dest / "input.bin", case["kind"]),
    }
    (dest / "repro.json").write_text(json.dumps(repro, indent=2, sort_keys=True) + "\n",
                                     encoding="utf-8")
    rec["artifact"] = str(dest)


def _suppress_error_dialogs() -> None:
    """Windows: report loader errors and crashes as exit codes, never as modal
    dialogs on the desktop (the setting is inherited by child processes)."""
    if sys.platform == "win32":
        import ctypes
        # SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)


def run_adversarial(profile: dict, probes: dict, out_dir: Path,
                    cases: list[dict] | None = None,
                    dll_dir: Path | None = None) -> list[dict]:
    """Run the campaign; returns one ds1edit.case/1 record per case.

    A kind whose probe is missing yields ``unavailable`` records, never
    silence. If ``profile['campaign_budget_cases']`` truncates the selection,
    a ``partial`` record names how many cases were not run; the results never
    omit that fact.
    """
    validate_profile(profile)
    _suppress_error_dialogs()
    out_dir = Path(out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    probes = {k: (str(Path(v).resolve()) if v else None) for k, v in probes.items()}
    env = None
    if dll_dir:
        env = dict(os.environ)
        env["PATH"] = str(Path(dll_dir).resolve()) + os.pathsep + env.get("PATH", "")
    timeout_s = float(profile.get("timeout_s", DEFAULT_TIMEOUT_S))
    roundtrip = bool(profile.get("roundtrip", False))
    selection = cases if cases is not None else build_cases(profile)
    budget = profile.get("campaign_budget_cases")
    skipped = 0
    if isinstance(budget, int) and len(selection) > budget:
        skipped = len(selection) - budget
        selection = selection[:budget]

    needs = {"ds1": "ds1", "dt1": "visual", "dcc": "visual", "dc6": "visual",
             "cof": "robust", "animdata": "robust"}
    def one(item) -> dict:
        i, case = item
        base = {"schema": CASE_SCHEMA, "id": case["id"], "lane": "portable",
                "platform": sys.platform, "required": True, "kind": case["kind"],
                "sample": case["sample"], "mutation": case["mutation"],
                "seed": case["seed"], "input_sha256": _sha(case["bytes"]),
                "success_outcome": None, "failures": [], "limitations": []}
        key = needs[case["kind"]]
        if case["oversize"]:
            base.update(status="partial", outcome="oversize", failures=[{
                "kind": "oversize", "detail": "payload is %d bytes, above the %d byte cap"
                % (len(case["bytes"]), profile.get("max_payload_bytes", DEFAULT_MAX_PAYLOAD))}])
            return base
        probe = probes.get(key)
        if not probe or not Path(probe).exists():
            base.update(status="unavailable", outcome="probe_unavailable", failures=[{
                "kind": "probe_unavailable", "detail": "probe %r is not available" % key}])
            return base
        work = out_dir / "work" / ("%05d" % i)
        rec = run_case(case, probes, work, timeout_s, roundtrip, env)
        if rec["status"] == "mismatch":
            _persist_failure(rec, case, work, out_dir)
        shutil.rmtree(work, ignore_errors=True)
        return rec
    jobs = int(profile.get("jobs") or min(8, os.cpu_count() or 2))
    if jobs > 1 and len(selection) > 1:
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
            results = list(pool.map(one, enumerate(selection)))
    else:
        results = [one(item) for item in enumerate(selection)]
    shutil.rmtree(out_dir / "work", ignore_errors=True)

    if skipped:
        results.append({
            "schema": CASE_SCHEMA, "id": "adversarial.%s.budget" % profile["id"],
            "lane": "portable", "platform": sys.platform, "required": True,
            "status": "partial", "success_outcome": None,
            "failures": [{"kind": "budget", "detail": "%d of %d selected cases were not run"
                          % (skipped, skipped + len(selection))}],
            "limitations": ["campaign budget truncated the selection"]})
    return results


def summarize(results: list[dict]) -> dict:
    by_kind: dict = {}
    for r in results:
        k = r.get("kind", "campaign")
        d = by_kind.setdefault(k, {"total": 0})
        d["total"] += 1
        d[r["status"]] = d.get(r["status"], 0) + 1
    bad = [r for r in results if r["status"] not in ("pass", "rejected_expected")]
    return {"total": len(results), "by_kind": by_kind,
            "defects": [r["id"] for r in bad]}


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="python -m scripts.equivalence.adversarial",
                                 description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    run = sub.add_parser("run", help="run a campaign profile against the probes")
    run.add_argument("--profile", required=True, type=Path)
    run.add_argument("--probe-ds1", type=Path)
    run.add_argument("--probe-visual", type=Path)
    run.add_argument("--probe-robust", type=Path)
    run.add_argument("--dll-dir", type=Path,
                     help="directory prepended to PATH for the probes (Allegro DLLs)")
    run.add_argument("--out-dir", required=True, type=Path)
    args = ap.parse_args(argv)

    profile = load_profile(args.profile)
    probes = {"ds1": args.probe_ds1, "visual": args.probe_visual,
              "robust": args.probe_robust}
    results = run_adversarial(profile, probes, args.out_dir, dll_dir=args.dll_dir)
    summary = summarize(results)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / "results.json").write_text(
        json.dumps({"profile": profile["id"], "summary": summary, "cases": [
            {k: v for k, v in r.items() if k not in ("command",)} for r in results]},
            indent=1, sort_keys=True) + "\n", encoding="utf-8")
    shown = dict(summary, defects=summary["defects"][:40],
                 defect_count=len(summary["defects"]))
    print(json.dumps(shown, indent=1, sort_keys=True))
    return 1 if summary["defects"] else 0


if __name__ == "__main__":
    sys.exit(main())
