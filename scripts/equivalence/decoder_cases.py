"""Authored DC6/DCC decoder cases: an independent encoder, fixtures, verification.

The editor's decoders are checked against *intended* frames. This module owns
the intent: it encodes authored palette-index frames into DC6/DCC bytes
following the documented formats (docs/guides/sprite-formats-dcc-dc6.md), and
records what a correct decoder must return. It never decodes with production
code and never derives indices from colours.

Commands (run from the repository root)::

    python -m scripts.equivalence.decoder_cases write-fixtures
    python -m scripts.equivalence.decoder_cases check-fixtures
    python -m scripts.equivalence.decoder_cases verify --probe PATH [--work DIR]

``verify`` runs the reference probe
(``test_visual_reference_probe decode --kind K --input F --direction N
--out-dir D``) on every case and compares its ``frames.json`` and raw index
buffers with the expectation. Exit 0 only when every case ran and matched.

Standard library only.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import struct
import subprocess
try:
    from scripts.winproc import no_window as _no_window
except ImportError:  # run as a plain script: put the repo root on the path
    import os as _os
    import sys as _sys
    _sys.path.insert(0, _os.path.dirname(_os.path.dirname(_os.path.dirname(_os.path.abspath(__file__)))))
    _sys.modules.pop("scripts", None)
    from scripts.winproc import no_window as _no_window
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FIXTURE_DIR = REPO_ROOT / "test" / "equivalence" / "fixtures" / "decoders"
CONTRACT_PATH = REPO_ROOT / "test" / "equivalence" / "contracts" / "decoders-113c.json"

ALL_DIRECTIONS = -1

# ---------------------------------------------------------------------------
# Frames
# ---------------------------------------------------------------------------


class Frame:
    """A palette-index frame: rows top-down, signed sprite-space origin of (0,0)."""

    def __init__(self, rows, origin_x, origin_y, direction=0, frame=0):
        self.rows = [list(r) for r in rows]
        self.height = len(self.rows)
        self.width = len(self.rows[0]) if self.rows else 0
        assert all(len(r) == self.width for r in self.rows)
        self.origin_x = origin_x
        self.origin_y = origin_y
        self.direction = direction
        self.frame = frame

    @property
    def pixels(self) -> bytes:
        return bytes(v for r in self.rows for v in r)

    def meta(self) -> dict:
        return {"direction": self.direction, "frame": self.frame,
                "width": self.width, "height": self.height,
                "stride": self.width, "origin_x": self.origin_x,
                "origin_y": self.origin_y}


def parse_art(text: str, legend: dict) -> list:
    """Rows of characters -> rows of palette indices ('.' is index 0)."""
    rows = []
    for line in text.strip().splitlines():
        line = line.strip()
        rows.append([0 if c == "." else legend[c] for c in line])
    return rows


# ---------------------------------------------------------------------------
# DC6 encoder
# ---------------------------------------------------------------------------


def dc6_encode_rows(rows) -> bytes:
    """Scanlines bottom-up; transparent runs become skips, rows end in 0x80."""
    out = bytearray()
    for row in reversed(rows):
        x, w = 0, len(row)
        # trailing transparent pixels are simply not encoded: the row ends early
        last = w
        while last > 0 and row[last - 1] == 0:
            last -= 1
        while x < last:
            if row[x] == 0:
                n = 0
                while x < last and row[x] == 0 and n < 0x7F:
                    x += 1
                    n += 1
                out.append(0x80 | n)
            else:
                start = x
                while x < last and row[x] != 0 and x - start < 0x7F:
                    x += 1
                out.append(x - start)
                out.extend(row[start:x])
        out.append(0x80)
    return bytes(out)


def dc6_build(directions: int, fpd: int, frames, streams=None) -> bytes:
    """frames: direction-major list of Frame. streams: optional raw override
    per frame index (to author opcode streams a simple encoder never emits)."""
    assert len(frames) == directions * fpd
    header = struct.pack("<iiiIii", 6, 1, 0, 0xEEEEEEEE, directions, fpd)
    table_size = 4 * directions * fpd
    body = bytearray()
    offsets = []
    base = len(header) + table_size
    for i, fr in enumerate(frames):
        data = streams[i] if streams and i in streams else dc6_encode_rows(fr.rows)
        off = base + len(body)
        offsets.append(off)
        offy = fr.origin_y + fr.height - 1
        next_block = off + 32 + len(data) + 3
        body += struct.pack("<iiiiiiii", 0, fr.width, fr.height, fr.origin_x,
                            offy, 0, next_block, len(data))
        body += data + b"\xee\xee\xee"
    return header + struct.pack("<%di" % len(offsets), *offsets) + bytes(body)


# ---------------------------------------------------------------------------
# DCC encoder
# ---------------------------------------------------------------------------

WIDTH_TABLE = [0, 1, 2, 4, 6, 8, 10, 12, 14, 16, 20, 24, 26, 28, 30, 32]


class BitWriter:
    """LSB-first within each byte, values LSB-first (the DCC bit order)."""

    def __init__(self):
        self.bits = []

    def put(self, value: int, nbits: int):
        value &= (1 << nbits) - 1 if nbits else 0
        for i in range(nbits):
            self.bits.append((value >> i) & 1)

    def extend(self, other: "BitWriter"):
        self.bits.extend(other.bits)

    def __len__(self):
        return len(self.bits)

    def to_bytes(self) -> bytes:
        out = bytearray((len(self.bits) + 7) // 8)
        for i, b in enumerate(self.bits):
            if b:
                out[i >> 3] |= 1 << (i & 7)
        return bytes(out)


def width_code(values, signed: bool) -> int:
    for code, bits in enumerate(WIDTH_TABLE):
        if bits == 0:
            if all(v == 0 for v in values):
                return code
            continue
        lo, hi = ((-(1 << (bits - 1)), (1 << (bits - 1)) - 1) if signed
                  else (0, (1 << bits) - 1))
        if all(lo <= v <= hi for v in values):
            return code
    raise ValueError("value does not fit 32 bits")


def split_cells(f_min: int, f_len: int, d_min: int):
    """Frame cell (offset relative to the direction box, length) along one axis."""
    first = 4 - ((f_min - d_min) % 4)
    if f_len - first <= 1:
        sizes = [f_len]
    else:
        tmp = f_len - first - 1
        n = 2 + tmp // 4
        if tmp % 4 == 0:
            n -= 1
        sizes = [first] + [4] * (n - 2) + [f_len - first - 4 * (n - 2)]
    out, pos = [], f_min - d_min
    for s in sizes:
        out.append((pos, s))
        pos += s
    return out


def encode_dcc_direction(frames, compression_flag=2, force_equal=()):
    """Encode one direction. frames share direction index; all top-down.

    force_equal: (frame, cell_index) pairs that are written as "equal cell"
    whatever their pixels -- used to author the clear-on-size-change rule.
    Returns (bytes, expected_frames) where expected frames are the direction
    box sized results a correct decoder returns.
    """
    xmin = min(f.origin_x for f in frames)
    ymin = min(f.origin_y for f in frames)
    xmax = max(f.origin_x + f.width - 1 for f in frames)
    ymax = max(f.origin_y + f.height - 1 for f in frames)
    bw, bh = xmax - xmin + 1, ymax - ymin + 1
    cells_w = 1 + (bw - 1) // 4

    used = sorted({0} | {v for f in frames for r in f.rows for v in r})
    code_of = {v: i for i, v in enumerate(used)}

    hdr_vals = {
        "width": [f.width for f in frames],
        "height": [f.height for f in frames],
        "xoffset": [f.origin_x for f in frames],
        # top-down frames: yoffset is the bottom row
        "yoffset": [f.origin_y + f.height - 1 for f in frames],
    }
    codes = {
        "variable0": 0,
        "width": width_code(hdr_vals["width"], False),
        "height": width_code(hdr_vals["height"], False),
        "xoffset": width_code(hdr_vals["xoffset"], True),
        "yoffset": width_code(hdr_vals["yoffset"], True),
        "optional": 0,
        "coded": 0,
    }

    eq = BitWriter()
    pm = BitWriter()
    pcd1 = BitWriter()
    pcd2 = BitWriter()
    dir_buf = [0] * (bw * bh)
    last = {}                       # buffer cell -> (x0, y0, w, h)
    expected = []

    for fi, fr in enumerate(frames):
        frm = [0] * (bw * bh)
        fx0, fy0 = fr.origin_x, fr.origin_y
        cols = split_cells(fx0, fr.width, xmin)
        rows = split_cells(fy0, fr.height, ymin)
        ci = 0
        for (y0, h) in rows:
            for (x0, w) in cols:
                bcell = (x0 // 4) + (y0 // 4) * cells_w
                want = [fr.rows[y0 + yy - (fy0 - ymin)][x0 + xx - (fx0 - xmin)]
                        for yy in range(h) for xx in range(w)]
                seen = bcell in last
                forced = (fi, ci) in force_equal
                same_geo = seen and last[bcell] == (x0, y0, w, h)
                same_px = same_geo and all(
                    dir_buf[(y0 + yy) * bw + x0 + xx] == want[yy * w + xx]
                    for yy in range(h) for xx in range(w))
                use_equal = seen and (compression_flag & 2) and (forced or same_px)
                if seen and (compression_flag & 2):
                    eq.put(1 if use_equal else 0, 1)
                if use_equal:
                    lx0, ly0, lw, lh = last[bcell]
                    if (lw, lh) == (w, h):
                        block = [dir_buf[(ly0 + yy) * bw + lx0 + xx]
                                 for yy in range(h) for xx in range(w)]
                        for yy in range(h):
                            for xx in range(w):
                                v = block[yy * w + xx]
                                dir_buf[(y0 + yy) * bw + x0 + xx] = v
                                frm[(y0 + yy) * bw + x0 + xx] = v
                    else:
                        for yy in range(h):
                            for xx in range(w):
                                dir_buf[(y0 + yy) * bw + x0 + xx] = 0
                else:
                    if seen:
                        pm.put(0xF, 4)
                    cell_codes = [code_of[v] for v in want]
                    nonzero = sorted({c for c in cell_codes if c != 0})
                    has_zero = 0 in cell_codes
                    if len(nonzero) > 4 or (has_zero and len(nonzero) > 3):
                        raise ValueError("cell has more than four colours")
                    prev = 0
                    for c in nonzero:
                        d = c - prev
                        while d >= 15:
                            pcd1.put(15, 4)
                            d -= 15
                        pcd1.put(d, 4)
                        prev = c
                    if len(nonzero) < 4:
                        pcd1.put(0, 4)      # repeat of the last code: stop
                    val = list(reversed(nonzero)) + [0] * (4 - len(nonzero))
                    if val[0] != val[1]:
                        nb = 1 if val[1] == val[2] else 2
                        for c in cell_codes:
                            pcd2.put(val.index(c), nb)
                    for yy in range(h):
                        for xx in range(w):
                            v = want[yy * w + xx]
                            dir_buf[(y0 + yy) * bw + x0 + xx] = v
                            frm[(y0 + yy) * bw + x0 + xx] = v
                last[bcell] = (x0, y0, w, h)
                ci += 1
        expected.append(Frame([frm[y * bw:(y + 1) * bw] for y in range(bh)],
                              xmin, ymin, fr.direction, fr.frame))

    bs = BitWriter()
    bs.put(0, 32)                               # outsize_coded (unused)
    bs.put(compression_flag, 2)
    for key in ("variable0", "width", "height", "xoffset", "yoffset",
                "optional", "coded"):
        bs.put(codes[key], 4)
    for fr, w_, h_, xo, yo in zip(frames, hdr_vals["width"], hdr_vals["height"],
                                  hdr_vals["xoffset"], hdr_vals["yoffset"]):
        bs.put(0, WIDTH_TABLE[codes["variable0"]])
        bs.put(w_, WIDTH_TABLE[codes["width"]])
        bs.put(h_, WIDTH_TABLE[codes["height"]])
        bs.put(xo, WIDTH_TABLE[codes["xoffset"]])
        bs.put(yo, WIDTH_TABLE[codes["yoffset"]])
        bs.put(0, WIDTH_TABLE[codes["optional"]])
        bs.put(0, WIDTH_TABLE[codes["coded"]])
        bs.put(0, 1)                            # bottom_up = 0 (top-down)
    if compression_flag & 2:
        bs.put(len(eq), 20)
    bs.put(len(pm), 20)
    if compression_flag & 1:
        raise ValueError("encoding-type streams are not authored here")
    for i in range(256):
        bs.put(1 if i in code_of else 0, 1)
    if compression_flag & 2:
        bs.extend(eq)
    bs.extend(pm)
    bs.extend(pcd1)
    bs.extend(pcd2)
    return bs.to_bytes(), expected


def dcc_build(directions):
    """directions: list of (frames, compression_flag, force_equal)."""
    fpd = len(directions[0][0])
    blobs, expected = [], []
    for frames, flag, force in directions:
        assert len(frames) == fpd
        data, exp = encode_dcc_direction(frames, flag, force)
        blobs.append(data)
        expected.extend(exp)
    head_len = 15 + 4 * len(directions)
    offsets, pos = [], head_len
    for b in blobs:
        offsets.append(pos)
        pos += len(b)
    head = struct.pack("<BBBiii", 0x74, 6, len(directions), fpd, 1, 0)
    head += struct.pack("<%di" % len(offsets), *offsets)
    return head + b"".join(blobs), expected


# ---------------------------------------------------------------------------
# Authored cases
# ---------------------------------------------------------------------------

LEGEND = {"R": 10, "B": 20, "G": 30, "Y": 40, "W": 200, "K": 251, "P": 77}


def _f(art, ox, oy, d=0, f=0, legend=LEGEND):
    return Frame(parse_art(art, legend), ox, oy, d, f)


def build_cases():
    """Return a list of case dicts: id, kind, filename, bytes, direction,
    expected (list of Frame), features."""
    cases = []

    # --- DC6 ---------------------------------------------------------------
    a = _f("""
        RR..B
        R.GGB
        .....
        YYYYY
    """, -3, -9)
    b = _f("""
        WK
        KW
        W.
    """, 4, -2, 0, 1)
    data = dc6_build(1, 2, [a, b])
    cases.append({"id": "dc6.rle.basic", "kind": "dc6", "file": "dc6_rle_basic.dc6",
                  "bytes": data, "direction": 0, "expected": [a, b],
                  "features": ["literal", "skip", "row_end", "bottom_up",
                               "negative_origin", "multi_frame",
                               "early_row_end", "empty_row"]})

    frames = []
    for d in range(2):
        for f in range(2):
            fr = _f("""
                R.B
                .G.
            """.replace("R", "RBGY"[(d * 2 + f) % 4]), d * 10 - 5, f * 3 - 7, d, f)
            frames.append(fr)
    data = dc6_build(2, 2, frames)
    cases.append({"id": "dc6.multi_direction", "kind": "dc6",
                  "file": "dc6_multi_direction.dc6", "bytes": data,
                  "direction": ALL_DIRECTIONS, "expected": frames,
                  "features": ["multi_direction", "direction_order"]})

    # A copy that runs past the right edge and a zero-length copy: the extra
    # pixels are dropped, never written outside the frame.
    clip = _f("""
        RB
        GY
    """, 0, 0)
    stream = bytes([0x00, 0x04, 30, 40, 99, 98, 0x80, 0x02, 10, 20, 0x80])
    data = dc6_build(1, 1, [clip], streams={0: stream})
    cases.append({"id": "dc6.clip_and_zero_copy", "kind": "dc6",
                  "file": "dc6_clip.dc6", "bytes": data, "direction": 0,
                  "expected": [clip], "features": ["clipping", "zero_copy"]})

    # --- DCC ---------------------------------------------------------------
    one = _f("""
        RRBB....
        RRBB.GG.
        ....GGGG
        Y......Y
        Y.W..W.Y
        YYYYYYYY
    """, -4, -12)
    data, exp = dcc_build([([one], 2, ())])
    cases.append({"id": "dcc.single_frame", "kind": "dcc", "file": "dcc_single.dcc",
                  "bytes": data, "direction": 0, "expected": exp,
                  "features": ["negative_origin", "one_bit_cells",
                               "two_bit_cells", "transparent_cells"]})

    # Repeated and changed cells across a sequence, wide signed fields that
    # straddle bytes, and a frame whose box is not cell-aligned.
    base = """
        RRRRBBBBGGGG
        RRRRBBBBGGGG
        RRRRBBBBGGGG
        RRRRBBBBGGGG
        YYYY....WWWW
        YYYY....WWWW
        YYYY....WWWW
        YYYY....WWWW
    """
    f0 = _f(base, -300, -120, 0, 0)
    lines = base.strip().splitlines()
    lines[1] = "RRRRBKBBGGGG"                 # one changed cell, the rest repeat
    f1 = _f("\n".join(lines), -300, -120, 0, 1)
    f2 = _f(base, -300, -120, 0, 2)          # back to frame 0: all cells reuse
    d0 = [f0, f1, f2]
    g0 = _f("""
        PPPP
        P..P
        PPPP
    """, 1000, 70000, 1, 0)
    g1 = _f("""
        ..PPPP
        ..P..P
        ..PPPP
    """, 998, 70000, 1, 1)                   # shifted by 2: cells change shape
    g2 = _f("""
        PPPP
        P..P
        PPPP
    """, 1000, 70000, 1, 2)
    d1 = [g0, g1, g2]
    data, exp = dcc_build([(d0, 2, ()), (d1, 2, ((2, 0),))])
    cases.append({"id": "dcc.sequence", "kind": "dcc", "file": "dcc_sequence.dcc",
                  "bytes": data, "direction": ALL_DIRECTIONS, "expected": exp,
                  "features": ["equal_cells", "changed_cells", "frame_sequence",
                               "byte_boundary_fields", "signed_fields",
                               "unaligned_frame_box", "clear_on_size_change",
                               "multi_direction"]})

    # No equal-cell stream (compression flag 0), and seventeen pixel codes so
    # displacements need the 15-continuation, including a step of exactly 15.
    pal = [5 + 13 * i for i in range(16)]         # codes 1..16 (0 is index 0)

    def cell_rows(cells):
        """cells: 2x2 grid of 4x4 blocks (lists of 16 codes) -> 8 rows."""
        rows = []
        for cy in range(2):
            for y in range(4):
                row = []
                for cx in range(2):
                    blk = cells[cy * 2 + cx]
                    row += [0 if c == 0 else pal[c - 1] for c in blk[y * 4:y * 4 + 4]]
                rows.append(row)
        return rows

    def four(a, b, c, d):
        return [a, b, c, d] * 4

    w0 = Frame(cell_rows([four(1, 2, 3, 4), four(5, 6, 7, 8),
                          four(9, 10, 11, 12), four(13, 14, 15, 16)]), 0, 0, 0, 0)
    w1 = Frame(cell_rows([[1, 16] * 8, [16] * 16, [15, 16, 0, 0] * 4, [0] * 16]),
               0, 0, 0, 1)
    data, exp = dcc_build([([w0, w1], 0, ())])
    cases.append({"id": "dcc.no_equal_cells", "kind": "dcc", "file": "dcc_flag0.dcc",
                  "bytes": data, "direction": 0, "expected": exp,
                  "features": ["compression_flag_0", "displacement_chain",
                               "four_colour_cells", "solid_cells"]})

    # A direction whose frames are entirely transparent.
    e0 = _f("""
        ....
        ....
    """, 3, 3, 0, 0)
    e1 = _f("""
        ....
        ....
    """, 3, 3, 0, 1)
    k0 = _f("""
        RB
        BR
    """, 0, 0, 1, 0)
    k1 = _f("""
        BR
        RB
    """, 0, 0, 1, 1)
    data, exp = dcc_build([([e0, e1], 2, ()), ([k0, k1], 2, ())])
    cases.append({"id": "dcc.empty_direction", "kind": "dcc", "file": "dcc_empty_dir.dcc",
                  "bytes": data, "direction": ALL_DIRECTIONS, "expected": exp,
                  "features": ["empty_direction"]})
    return cases


# ---------------------------------------------------------------------------
# Fixtures on disk
# ---------------------------------------------------------------------------


def expectation_record(case) -> dict:
    return {
        "id": case["id"],
        "kind": case["kind"],
        "file": case["file"],
        "input_sha256": hashlib.sha256(case["bytes"]).hexdigest(),
        "direction": case["direction"],
        "transparency": "index0",
        "features": case["features"],
        "frames": [dict(fr.meta(), pixels_hex=fr.pixels.hex())
                   for fr in case["expected"]],
    }


def write_fixtures(out_dir: Path = FIXTURE_DIR) -> list:
    out_dir.mkdir(parents=True, exist_ok=True)
    manifest = []
    for case in build_cases():
        (out_dir / case["file"]).write_bytes(case["bytes"])
        manifest.append(expectation_record(case))
    text = json.dumps({"schema": "ds1edit.decoder-cases/1", "cases": manifest},
                      indent=1, sort_keys=True) + "\n"
    (out_dir / "expected.json").write_text(text, encoding="utf-8", newline="\n")
    return manifest


def check_fixtures(fixture_dir: Path = FIXTURE_DIR) -> list:
    """Return a list of differences between the generator and the committed files."""
    problems = []
    try:
        committed = json.loads((fixture_dir / "expected.json").read_text("utf-8"))
    except OSError as exc:
        return ["expected.json unreadable: %s" % exc]
    by_id = {c["id"]: c for c in committed.get("cases", [])}
    for case in build_cases():
        path = fixture_dir / case["file"]
        if not path.is_file():
            problems.append("missing fixture %s" % case["file"])
        elif path.read_bytes() != case["bytes"]:
            problems.append("fixture %s differs from the generator" % case["file"])
        if by_id.get(case["id"]) != expectation_record(case):
            problems.append("expectation for %s differs from the generator" % case["id"])
    if len(by_id) != len(build_cases()):
        problems.append("expected.json lists %d cases, generator %d"
                        % (len(by_id), len(build_cases())))
    return problems


# ---------------------------------------------------------------------------
# Probe comparison
# ---------------------------------------------------------------------------

META_KEYS = ("direction", "frame", "width", "height", "stride", "origin_x", "origin_y")


def compare_frames(expected: list, actual: list) -> list:
    """Field and pixel differences between two lists of frame dicts carrying
    META_KEYS and raw ``pixels`` bytes. Empty when they agree."""
    diffs = []
    if len(expected) != len(actual):
        diffs.append("frame count %d != %d" % (len(actual), len(expected)))
    for i, (e, a) in enumerate(zip(expected, actual)):
        for k in META_KEYS:
            if e[k] != a.get(k):
                diffs.append("frame %d %s: %r != %r" % (i, k, a.get(k), e[k]))
        if e["pixels"] != a.get("pixels"):
            ep, ap = e["pixels"], a.get("pixels") or b""
            first = next((j for j in range(min(len(ep), len(ap))) if ep[j] != ap[j]),
                         min(len(ep), len(ap)))
            diffs.append("frame %d pixels differ (first at byte %d)" % (i, first))
    return diffs


def expected_frames(record: dict) -> list:
    return [dict({k: f[k] for k in META_KEYS}, pixels=bytes.fromhex(f["pixels_hex"]))
            for f in record["frames"]]


def read_probe_output(out_dir: Path) -> dict:
    doc = json.loads((out_dir / "frames.json").read_text("utf-8"))
    frames = []
    for f in doc.get("frames", []):
        data = (out_dir / f["file"]).read_bytes()
        if hashlib.sha256(data).hexdigest() != f.get("sha256"):
            raise ValueError("%s does not match its recorded sha256" % f["file"])
        frames.append(dict({k: f.get(k) for k in META_KEYS}, pixels=data))
    doc["frames"] = frames
    return doc


def probe_env(probe: Path, dll_dirs=()) -> dict:
    """Environment for the probe: its own directory and any runtime library
    directories first on PATH (Windows resolves DLLs through it)."""
    import os
    env = dict(os.environ)
    dirs = [str(Path(d)) for d in dll_dirs] + [str(Path(probe).resolve().parent)]
    env["PATH"] = os.pathsep.join(dirs + [env.get("PATH", "")])
    return env


def run_probe(probe: Path, record: dict, fixture_dir: Path, work: Path,
              dll_dirs=()) -> dict:
    out = work / record["id"]
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    cmd = [str(probe), "decode", "--kind", record["kind"],
           "--input", str(fixture_dir / record["file"]),
           "--direction", str(record["direction"]), "--out-dir", str(out)]
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=60,
                          env=probe_env(probe, dll_dirs), **_no_window())
    result = {"id": record["id"], "returncode": proc.returncode,
              "stderr": proc.stderr[-2000:]}
    if proc.returncode != 0:
        result["status"] = "not_run"
        result["diffs"] = ["probe exited %d" % proc.returncode]
        return result
    try:
        doc = read_probe_output(out)
    except (OSError, ValueError, KeyError) as exc:
        result["status"] = "not_run"
        result["diffs"] = ["probe output unreadable: %s" % exc]
        return result
    diffs = []
    if doc.get("transparency") != record["transparency"]:
        diffs.append("transparency %r != %r" % (doc.get("transparency"),
                                                record["transparency"]))
    if doc.get("input_sha256") != record["input_sha256"]:
        diffs.append("probe read a different input")
    diffs += compare_frames(expected_frames(record), doc["frames"])
    result["status"] = "pass" if not diffs else "mismatch"
    result["diffs"] = diffs
    return result


def verify(probe: Path, fixture_dir: Path = FIXTURE_DIR, work: Path | None = None,
           dll_dirs=()) -> list:
    committed = json.loads((fixture_dir / "expected.json").read_text("utf-8"))
    own = work is None
    work = Path(tempfile.mkdtemp(prefix="ds1edit decoder cases ")) if own else work
    try:
        return [run_probe(probe, rec, fixture_dir, work, dll_dirs)
                for rec in committed["cases"]]
    finally:
        if own:
            shutil.rmtree(work, ignore_errors=True)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("write-fixtures")
    sub.add_parser("check-fixtures")
    v = sub.add_parser("verify")
    v.add_argument("--probe", required=True, type=Path)
    v.add_argument("--work", type=Path)
    v.add_argument("--dll-dir", type=Path, action="append", default=[],
                   help="extra runtime library directory for the probe")
    args = ap.parse_args(argv)

    if args.cmd == "write-fixtures":
        for rec in write_fixtures():
            print("wrote %s (%s)" % (rec["file"], rec["id"]))
        return 0
    if args.cmd == "check-fixtures":
        problems = check_fixtures()
        for p in problems:
            print(p)
        return 1 if problems else 0
    results = verify(args.probe, work=args.work, dll_dirs=args.dll_dir)
    for r in results:
        print("%-28s %s" % (r["id"], r["status"]))
        for d in r["diffs"]:
            print("    " + d)
    if not results:
        print("no cases ran")
        return 2
    return 0 if all(r["status"] == "pass" for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
