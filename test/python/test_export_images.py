"""Package D task 3: independent export decoding and its controls.

``verify_export`` decodes a PNG/APNG with Pillow and its own chunk reader.
The real editor outputs are checked by the CTest ``visual_export_images``;
here files written by a minimal independent APNG encoder show the verifier
accepts a correct file and rejects a priority reversal, dropped alpha, a
swapped frame order and a wrong delay.

    python -m unittest -v test.python.test_export_images
"""

import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import export_cases as ec  # noqa: E402


def _chunk(ctype, body):
    return (struct.pack(">I", len(body)) + ctype + body
            + struct.pack(">I", zlib.crc32(ctype + body) & 0xFFFFFFFF))


def write_apng(path, w, h, frames, delays):
    """frames: RGBA bytes each; delays: (num, den). Blend SOURCE, dispose NONE."""
    out = b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    out += _chunk(b"acTL", struct.pack(">II", len(frames), 0))
    seq = 0
    for i, (px, (num, den)) in enumerate(zip(frames, delays)):
        out += _chunk(b"fcTL", struct.pack(">IIIIIHHBB", seq, w, h, 0, 0, num, den, 0, 0))
        seq += 1
        raw = b"".join(b"\0" + px[y * w * 4:(y + 1) * w * 4] for y in range(h))
        data = zlib.compress(raw)
        if i == 0:
            out += _chunk(b"IDAT", data)
        else:
            out += _chunk(b"fdAT", struct.pack(">I", seq) + data)
            seq += 1
    out += _chunk(b"IEND", b"")
    Path(path).write_bytes(out)


def _compose_expectation():
    doc = json.loads((ec.FIXTURE_DIR / "expected.json").read_text("utf-8"))
    o = next(x for x in doc["outputs"] if x["file"] == "compose/AMTNHTH.apng")
    frames = [bytes.fromhex(f) for f in o["frames"]]
    return o["width"], o["height"], frames, [tuple(d) for d in o["delays"]]


class ExportImages(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ds1edit export images ")
        self.dir = Path(self.tmp.name)
        self.w, self.h, self.frames, self.delays = _compose_expectation()
        self.expected = [{"width": self.w, "height": self.h, "rgba": f} for f in self.frames]

    def tearDown(self):
        self.tmp.cleanup()

    def check(self, frames, delays):
        p = self.dir / "case.apng"
        write_apng(p, self.w, self.h, frames, delays)
        return ec.verify_export(p, self.expected, self.delays)

    def test_fixtures_are_the_generator_output(self):
        self.assertEqual(ec.check_fixtures(), [])

    def test_expectation_exercises_overlap_alpha_and_order(self):
        # distinct frames, some transparency, and frames 0/1 differ at the
        # overlap (priority reversal is observable)
        self.assertEqual(len(set(self.frames)), 3)
        self.assertTrue(any(self.frames[0][i + 3] == 0 for i in range(0, len(self.frames[0]), 4)))
        self.assertNotEqual(self.frames[0], self.frames[1])

    def test_correct_file_passes(self):
        r = self.check(self.frames, self.delays)
        self.assertEqual(r["status"], "pass", r["diffs"])

    def test_priority_reversal_fails(self):
        # frame 1 drawn with frame 0's order (TR under HD) instead
        swapped = list(self.frames)
        swapped[1] = self.frames[0]
        self.assertEqual(self.check(swapped, self.delays)["status"], "mismatch")

    def test_dropped_alpha_fails(self):
        opaque = [bytes(b if (i % 4) != 3 else 255 for i, b in enumerate(f)) for f in self.frames]
        self.assertEqual(self.check(opaque, self.delays)["status"], "mismatch")

    def test_swapped_frame_order_fails(self):
        order = [self.frames[1], self.frames[0], self.frames[2]]
        self.assertEqual(self.check(order, self.delays)["status"], "mismatch")

    def test_wrong_delay_fails_serialized_and_reader_checks(self):
        r = self.check(self.frames, [(40, 1000)] * 3)
        self.assertEqual(r["status"], "mismatch")
        self.assertTrue(any("serialized" in d for d in r["diffs"]))
        # same duration, different rational: still a declared-policy mismatch
        r = self.check(self.frames, [(16, 125)] * 3)
        self.assertEqual(r["status"], "mismatch")

    def test_missing_file_is_not_run(self):
        r = ec.verify_export(self.dir / "absent.apng", self.expected, self.delays)
        self.assertEqual(r["status"], "not_run")

    def test_truncated_file_is_a_mismatch(self):
        p = self.dir / "cut.apng"
        write_apng(p, self.w, self.h, self.frames, self.delays)
        p.write_bytes(p.read_bytes()[:-12])        # drop IEND
        self.assertEqual(ec.verify_export(p, self.expected, self.delays)["status"], "mismatch")


if __name__ == "__main__":
    unittest.main()
