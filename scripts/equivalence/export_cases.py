"""Authored export cases and independent verification of the written files.

The editor writes PNG/APNG files from authored inputs (fixtures generated
here); this module decodes those files with Pillow plus its own chunk reader
and compares frame count, order, size, RGBA pixels and serialized delays with
expectations computed here from the authored geometry -- never from the
editor's own output.

Commands (from the repository root)::

    python -m scripts.equivalence.export_cases write-fixtures
    python -m scripts.equivalence.export_cases check-fixtures
    python -m scripts.equivalence.export_cases verify --out-dir DIR

``verify`` exits 0 only when every expected output exists and matches.

Delay policy (declared): the compose export writes each frame's delay as the
rational ``ms/1000`` where ``ms`` is the editor's AnimData millisecond value;
the writer reduces a fraction that does not fit 16 bits by its gcd and
refuses one that still does not fit. Readers' millisecond values are checked
separately from the serialized numerator/denominator.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import zlib
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:  # pragma: no cover - direct execution
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import decoder_cases as dcases  # noqa: E402
from scripts.equivalence import visual_values  # noqa: E402

FIXTURE_DIR = REPO_ROOT / "test" / "equivalence" / "fixtures" / "exports"

# ---------------------------------------------------------------------------
# Palette (index 0 transparent)
# ---------------------------------------------------------------------------


def palette():
    """256 RGBA tuples; every index distinct in R so channel swaps show."""
    pal = []
    for i in range(256):
        pal.append((i, (255 - i) & 0xFF, (i * 3) & 0xFF, 0 if i == 0 else 255))
    return pal


def palette_bytes() -> bytes:
    return b"".join(bytes(c) for c in palette())


def rgba(index_rows, pal=None):
    pal = pal or palette()
    out = bytearray()
    for row in index_rows:
        for v in row:
            out += bytes(pal[v]) if v else b"\0\0\0\0"
    return bytes(out)


# ---------------------------------------------------------------------------
# Cases
# ---------------------------------------------------------------------------

def _solid(w, h, color, hole=False):
    rows = [[color] * w for _ in range(h)]
    if hole:
        rows[0][0] = 0
    return rows


def sprite_case():
    """A 2-direction, 2-frame DC6 with transparent borders; exported per
    direction/frame on the union of each direction's frames."""
    frames = [
        dcases.Frame([[0, 0, 0], [0, 50, 0], [0, 0, 0]], -1, -1, 0, 0),
        dcases.Frame([[60, 61]], 0, 0, 0, 1),
        dcases.Frame([[70], [71]], 2, -3, 1, 0),
        dcases.Frame([[80, 0, 81]], 0, -2, 1, 1),
    ]
    data = dcases.dc6_build(2, 2, frames)
    outputs = []
    for d in range(2):
        fr = [f for f in frames if f.direction == d]
        x0 = min(f.origin_x for f in fr)
        y0 = min(f.origin_y for f in fr)
        x1 = max(f.origin_x + f.width - 1 for f in fr)
        y1 = max(f.origin_y + f.height - 1 for f in fr)
        w, h = x1 - x0 + 1, y1 - y0 + 1
        for f in fr:
            canvas = [[0] * w for _ in range(h)]
            for y in range(f.height):
                for x in range(f.width):
                    if f.rows[y][x]:
                        canvas[f.origin_y - y0 + y][f.origin_x - x0 + x] = f.rows[y][x]
            outputs.append({
                "file": "sprite/Data/Test/sprite/dir_%02d/frame_%03d.png" % (d, f.frame),
                "kind": "png", "width": w, "height": h,
                "frames": [rgba(canvas).hex()], "delays": []})
    return data, outputs


def compose_case():
    """Two DC6 layers through a COF, 1 direction x 3 frames, priority
    reversed on frame 1; written by compose_apng_write as an APNG whose
    delay comes from the AnimData fixture (AMTNHTH: speed 80 -> 128 ms)."""
    # HD: 2x2 at (0,0), colours 10/11/12 by frame, frame 1 has a hole.
    hd = [dcases.Frame(_solid(2, 2, 10 + f, hole=(f == 1)), 0, 0, 0, f) for f in range(3)]
    # TR: 3x3 at (-2,-1), colour 20+frame.
    tr = [dcases.Frame(_solid(3, 3, 20 + f), -2, -1, 0, f) for f in range(3)]
    priority = [[1, 0], [0, 1], [1, 0]]                 # composit index per order
    layers = {0: hd, 1: tr}
    x0, y0, w, h = -2, -1, 4, 3
    expected = []
    for f in range(3):
        canvas = [[0] * w for _ in range(h)]
        for slot in priority[f]:
            fr = layers[slot][f]
            for y in range(fr.height):
                for x in range(fr.width):
                    v = fr.rows[y][x]
                    if v:
                        canvas[fr.origin_y - y0 + y][fr.origin_x - x0 + x] = v
        expected.append(rgba(canvas).hex())
    cof = bytearray(28)
    cof[0], cof[1], cof[2], cof[3] = 2, 3, 1, 20
    for slot in (0, 1):
        cof += bytes([slot, 1, 1, 0, 0]) + b"HTH\0"
    cof += bytes(3)                                     # frame flags
    for f in range(3):
        cof += bytes(priority[f])
    files = {
        "compose/AMTNHTH.cof": bytes(cof),
        "compose/HD.dc6": dcases.dc6_build(1, 3, hd),
        "compose/TR.dc6": dcases.dc6_build(1, 3, tr),
    }
    outputs = [{
        "file": "compose/AMTNHTH.apng", "kind": "apng", "width": w, "height": h,
        "frames": expected,
        "delays": [[128, 1000]] * 3,
    }, {
        # one-frame output; unknown COF name -> the 40 ms default
        "file": "compose/one_frame.apng", "kind": "apng", "width": w, "height": h,
        "frames": [expected[0]],
        "delays": [[40, 1000]],
    }]
    return files, outputs


def build():
    sprite, sprite_out = sprite_case()
    files, compose_out = compose_case()
    files = dict(files)
    files["sprite.dc6"] = sprite
    files["palette.rgba"] = palette_bytes()
    files["animdata.d2"] = visual_values.build_animdata()
    return files, sprite_out + compose_out


def write_fixtures(out_dir: Path = FIXTURE_DIR):
    files, outputs = build()
    for name, data in files.items():
        p = out_dir / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    (out_dir / "expected.json").write_text(
        json.dumps({"schema": "ds1edit.export-cases/1", "outputs": outputs},
                   indent=1, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    return outputs


def check_fixtures(out_dir: Path = FIXTURE_DIR):
    files, outputs = build()
    problems = []
    for name, data in files.items():
        p = out_dir / name
        if not p.is_file() or p.read_bytes() != data:
            problems.append("fixture %s differs from the generator" % name)
    try:
        doc = json.loads((out_dir / "expected.json").read_text("utf-8"))
        if doc.get("outputs") != outputs:
            problems.append("expected.json differs from the generator")
    except (OSError, ValueError) as exc:
        problems.append("expected.json unreadable: %s" % exc)
    return problems


# ---------------------------------------------------------------------------
# Independent file reading
# ---------------------------------------------------------------------------

def read_chunks(data: bytes):
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG signature")
    pos, chunks = 8, []
    while pos + 12 <= len(data):
        (length,) = struct.unpack_from(">I", data, pos)
        ctype = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        (crc,) = struct.unpack_from(">I", data, pos + 8 + length)
        if zlib.crc32(ctype + body) & 0xFFFFFFFF != crc:
            raise ValueError("bad CRC in %r" % ctype)
        chunks.append((ctype.decode("ascii"), body))
        pos += 12 + length
        if ctype == b"IEND":
            break
    if not chunks or chunks[-1][0] != "IEND":
        raise ValueError("no IEND chunk")
    return chunks


def serialized_delays(data: bytes):
    """(num, den, dispose, blend) per fcTL, in file order."""
    out = []
    for ctype, body in read_chunks(data):
        if ctype == "fcTL":
            num, den = struct.unpack_from(">HH", body, 20)
            out.append((num, den, body[24], body[25]))
    return out


def decoded_frames(path: Path):
    """Pillow-decoded RGBA frames and their reported durations (ms)."""
    from PIL import Image, ImageSequence
    frames, durations = [], []
    with Image.open(path) as im:
        n = getattr(im, "n_frames", 1)
        for i, fr in enumerate(ImageSequence.Iterator(im)):
            frames.append((fr.size, fr.convert("RGBA").tobytes()))
            durations.append(fr.info.get("duration"))
            if i + 1 >= n:
                break
    return frames, durations


def verify_export(path: Path, expected_frames: list, expected_delays: list) -> dict:
    """Compare one written PNG/APNG with expectations.

    expected_frames: [{"width", "height", "rgba": bytes}] in display order.
    expected_delays: [(num, den)] per frame; [] for a still PNG.
    Returns {"status": "pass"|"mismatch"|"not_run", "diffs": [...]}.
    """
    path = Path(path)
    if not path.is_file():
        return {"status": "not_run", "diffs": ["%s was not written" % path]}
    data = path.read_bytes()
    diffs = []
    try:
        chunks = read_chunks(data)
        frames, durations = decoded_frames(path)
    except Exception as exc:  # noqa: BLE001 - any decode failure is a finding
        return {"status": "mismatch", "diffs": ["unreadable: %s" % exc]}

    names = [c for c, _ in chunks]
    if expected_delays:
        actl = [b for c, b in chunks if c == "acTL"]
        if not actl:
            diffs.append("no acTL chunk")
        else:
            nf, _plays = struct.unpack(">II", actl[0])
            if nf != len(expected_frames):
                diffs.append("acTL frames %d != %d" % (nf, len(expected_frames)))
        if names.index("acTL") > names.index("IDAT") if "acTL" in names else False:
            diffs.append("acTL after IDAT")
        ser = serialized_delays(data)
        if [(n, d) for n, d, _, _ in ser] != [tuple(x) for x in expected_delays]:
            diffs.append("serialized delays %s != %s"
                         % ([(n, d) for n, d, _, _ in ser], expected_delays))
        for i, (n, d, dispose, blend) in enumerate(ser):
            if blend != 0:
                diffs.append("frame %d blends over the previous frame" % i)
        for i, (exp, got) in enumerate(zip(expected_delays, durations)):
            want = exp[0] * 1000.0 / (exp[1] or 100)
            if got is None or abs(float(got) - want) > 0.5:
                diffs.append("frame %d reader duration %r != %.1f ms" % (i, got, want))
    elif "acTL" in names:
        diffs.append("still image carries acTL")

    if len(frames) != len(expected_frames):
        diffs.append("frame count %d != %d" % (len(frames), len(expected_frames)))
    for i, (exp, (size, px)) in enumerate(zip(expected_frames, frames)):
        if size != (exp["width"], exp["height"]):
            diffs.append("frame %d size %s != %s" % (i, size, (exp["width"], exp["height"])))
            continue
        if px != exp["rgba"]:
            first = next(j for j in range(len(px)) if px[j] != exp["rgba"][j])
            diffs.append("frame %d pixel %d differs (%s != %s)" % (
                i, first // 4, px[first // 4 * 4:first // 4 * 4 + 4].hex(),
                exp["rgba"][first // 4 * 4:first // 4 * 4 + 4].hex()))
    return {"status": "pass" if not diffs else "mismatch", "diffs": diffs}


def verify_outputs(out_dir: Path, fixture_dir: Path = FIXTURE_DIR) -> list:
    doc = json.loads((fixture_dir / "expected.json").read_text("utf-8"))
    results = []
    for o in doc["outputs"]:
        exp = [{"width": o["width"], "height": o["height"], "rgba": bytes.fromhex(f)}
               for f in o["frames"]]
        r = verify_export(Path(out_dir) / o["file"], exp, o["delays"])
        r["file"] = o["file"]
        results.append(r)
    return results


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("write-fixtures")
    sub.add_parser("check-fixtures")
    v = sub.add_parser("verify")
    v.add_argument("--out-dir", required=True, type=Path)
    args = ap.parse_args(argv)
    if args.cmd == "write-fixtures":
        for o in write_fixtures():
            print("expects", o["file"])
        return 0
    if args.cmd == "check-fixtures":
        problems = check_fixtures()
        for p in problems:
            print(p)
        return 1 if problems else 0
    results = verify_outputs(args.out_dir)
    for r in results:
        print("%-48s %s" % (r["file"], r["status"]))
        for d in r["diffs"]:
            print("    " + d)
    return 0 if results and all(r["status"] == "pass" for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
