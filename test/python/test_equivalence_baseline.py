"""Tests for scripts/equivalence/baseline.py using fake archives.

Never imports mpyq or reads a real game installation.

    python -m unittest -v test.python.test_equivalence_baseline
"""

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import baseline as B  # noqa: E402
from scripts.equivalence import contracts as C  # noqa: E402

LVLTYPES = ("Name\tId\tFile 1\tFile 2\n"
            "Act 1 - Town\t1\tAct1\\Town\\floor.dt1\tAct1\\Town\\missing.dt1\n")
LVLPREST = "Name\tDef\tFile1\tFile2\nTown\t1\tAct1\\Town\\towne1.ds1\t0\n"


class FakeArchive:
    def __init__(self, files, listfile=True, broken_listfile=False):
        self.files = {k.lower(): (k, v) for k, v in files.items()}
        self.listfile = listfile
        self.broken = broken_listfile
        self.closed = False

    def has(self, name):
        if name.lower() == "(listfile)":
            return self.listfile
        return name.lower().replace("/", "\\") in self.files

    def namelist(self, extra_names=()):
        if self.broken:
            raise RuntimeError("listfile decompression failed")
        return [orig for orig, _ in self.files.values()] if self.listfile else []

    def read(self, name):
        hit = self.files.get(name.lower().replace("/", "\\"))
        return None if hit is None else hit[1]

    def close(self):
        self.closed = True


class BaselineTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="eq baseline ")
        self.game = Path(self.tmp.name) / "Diablo II 1.13c"
        self.game.mkdir()
        for b in B.REQUIRED_BINARIES:
            (self.game / b).write_bytes(b"binary " + b.encode())
        for a in B.ARCHIVES:
            (self.game / a).write_bytes(b"archive " + a.encode())
        # Things the generator must never consult.
        (self.game / "mod.mpq").write_bytes(b"mod")
        loose = self.game / "data" / "global" / "tiles"
        loose.mkdir(parents=True)
        (loose / "overlay.ds1").write_bytes(b"overlay")
        self.contents = {
            "patch_d2.mpq": FakeArchive({
                "data\\global\\tiles\\Act1\\Town\\towne1.ds1": b"patched ds1",
                "data\\global\\excel\\LvlTypes.txt": LVLTYPES.encode(),
            }, listfile=False),
            "d2exp.mpq": FakeArchive({}),
            "d2data.mpq": FakeArchive({
                "data\\global\\tiles\\Act1\\Town\\towne1.ds1": b"base ds1",
                "DATA\\GLOBAL\\TILES\\ACT1\\TOWN\\FLOOR.DT1": b"floor",
                "data\\global\\excel\\lvlprest.txt": LVLPREST.encode(),
                "data\\global\\excel\\LvlTypes.txt": b"old lvltypes",
                "data\\global\\sfx\\ignored.wav": b"not an editor resource",
            }),
            "d2char.mpq": FakeArchive({
                "data\\global\\tiles\\act1\\town\\floor.dt1": b"case variant",
            }),
        }
        self.opened = []

    def tearDown(self):
        self.tmp.cleanup()

    def factory(self, path):
        self.opened.append(Path(path).name.lower())
        return self.contents[Path(path).name.lower()]

    def build(self, extra=(), **kw):
        return B.build_baseline(self.game, list(extra), reader_factory=self.factory, **kw)

    def asset(self, b, path):
        return next((a for a in b["assets"] if a["path"] == path), None)

    def test_patch_shadows_base_and_case_variants_dedupe(self):
        b = self.build(extra=["data\\global\\tiles\\Act1\\Town\\towne1.ds1",
                              "data\\global\\excel\\LvlTypes.txt"])
        C.validate_baseline(b)
        ds1 = self.asset(b, "data\\global\\tiles\\act1\\town\\towne1.ds1")
        self.assertEqual(ds1["archive"], "Patch_D2.mpq")
        self.assertEqual(ds1["sha256"], hashlib.sha256(b"patched ds1").hexdigest())
        self.assertEqual(ds1["original_path"], "data\\global\\tiles\\Act1\\Town\\towne1.ds1")
        dt1 = self.asset(b, "data\\global\\tiles\\act1\\town\\floor.dt1")
        self.assertEqual((dt1["archive"], dt1["sha256"]),
                         ("d2data.mpq", hashlib.sha256(b"floor").hexdigest()))
        self.assertEqual(len([a for a in b["assets"] if a["path"].endswith("floor.dt1")]), 1)
        self.assertIsNone(self.asset(b, "data\\global\\sfx\\ignored.wav"))
        self.assertEqual([a["name"].lower() for a in b["archives"]],
                         [n.lower() for n in B.ARCHIVES])
        self.assertEqual(sorted(self.opened), sorted(n.lower() for n in B.ARCHIVES))

    def test_mod_overlay_and_loose_files_are_absent(self):
        b = self.build()
        text = json.dumps(b).lower()
        self.assertNotIn("mod.mpq", text)
        self.assertNotIn("overlay.ds1", text)
        self.assertNotIn(str(self.game).lower().replace("\\", "\\\\"), text)
        self.assertNotIn("assets\\\\tiles", text)

    def test_supplementary_names_resolve_despite_missing_listfile(self):
        without = self.build()
        self.assertEqual(self.asset(without, "data\\global\\excel\\lvltypes.txt")["archive"],
                         "d2data.mpq")
        self.assertFalse(without["enumeration"]["complete"])
        self.assertTrue(any("Patch_D2.mpq" in l for l in without["enumeration"]["limits"]))
        withx = self.build(extra=["data\\global\\excel\\LvlTypes.txt"])
        self.assertEqual(self.asset(withx, "data\\global\\excel\\lvltypes.txt")["archive"],
                         "Patch_D2.mpq")
        src = withx["enumeration"]["sources"][0]
        self.assertEqual(src["supplementary_resolved"], 1)

    def test_unreadable_listfile_is_a_limit_and_blocks_required_corpus(self):
        self.contents["d2data.mpq"].broken = True
        b = self.build()
        self.assertFalse(b["enumeration"]["complete"])
        self.assertTrue(any("listfile unreadable" in l for l in b["enumeration"]["limits"]))
        with self.assertRaisesRegex(B.BaselineError, "incomplete"):
            self.build(require_complete=True)

    def test_dependency_inventory_lists_unresolved(self):
        b = self.build(extra=["data\\global\\excel\\LvlTypes.txt"])
        d = b["dependencies"]["data\\global\\excel\\lvltypes.txt"]
        self.assertEqual(d["referenced"], 2)
        self.assertEqual(d["unresolved"], ["data\\global\\tiles\\act1\\town\\missing.dt1"])
        p = b["dependencies"]["data\\global\\excel\\lvlprest.txt"]
        self.assertEqual((p["referenced"], p["unresolved"]), (1, []))

    def test_changed_dll_or_archive_changes_digest(self):
        d0 = self.build()["digest"]
        self.assertEqual(self.build()["digest"], d0)  # deterministic
        (self.game / "D2CMP.dll").write_bytes(b"patched dll")
        d1 = self.build()["digest"]
        self.assertNotEqual(d1, d0)
        (self.game / "d2exp.mpq").write_bytes(b"other archive")
        self.assertNotEqual(self.build()["digest"], d1)

    def test_missing_required_archive_or_binary_fails(self):
        (self.game / "d2char.mpq").unlink()
        with self.assertRaisesRegex(B.BaselineError, "d2char.mpq"):
            self.build()
        (self.game / "d2char.mpq").write_bytes(b"x")
        (self.game / "D2CMP.dll").unlink()
        with self.assertRaisesRegex(B.BaselineError, "D2CMP.dll"):
            self.build()

    def test_identity_scope_hashes_no_members(self):
        b = B.build_baseline(self.game, [], reader_factory=lambda p: self.fail("opened"),
                             scope="identity")
        C.validate_baseline(b)
        self.assertEqual(b["assets"], [])
        self.assertEqual(b["enumeration"]["scope"], "identity")
        self.assertEqual(C.baseline_binary(b, "d2cmp.dll")["sha256"],
                         hashlib.sha256(b"binary D2CMP.dll").hexdigest())

    def test_invalid_member_names_cannot_escape_scratch(self):
        root = Path(self.tmp.name) / "scratch"
        root.mkdir()
        ok = B.safe_scratch_path(root, "data\\global\\tiles\\a.ds1")
        self.assertTrue(str(ok).startswith(str(root.resolve())))
        for bad in ("..\\..\\evil.dll", "data\\..\\..\\x", "\\abs\\x", "C:\\x",
                    "c:evil", "/etc/passwd", ""):
            with self.assertRaises(B.BaselineError, msg=bad):
                B.safe_scratch_path(root, bad)

    def test_tests_do_not_import_mpyq(self):
        self.build()
        self.assertNotIn("mpyq", sys.modules.keys() - _PRELOADED)

    def test_pe_sections_parse_minimal_image(self):
        p = Path(self.tmp.name) / "tiny.dll"
        p.write_bytes(_tiny_pe())
        pe = B.pe_sections(p)
        self.assertEqual(pe["sections"][".text"]["bytes"], 4)
        self.assertEqual(pe["sections"][".text"]["sha256"],
                         hashlib.sha256(b"\xc3\x90\x90\x90").hexdigest())
        self.assertIsNone(B.pe_sections(self.game / "Game.exe"))


_PRELOADED = set(sys.modules)


def _tiny_pe():
    import struct
    data = bytearray(0x400)
    data[0:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x40)
    data[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<HH", data, 0x44, 0x14c, 1)          # machine, 1 section
    struct.pack_into("<H", data, 0x54, 0xE0)               # optional header size
    struct.pack_into("<I", data, 0x58 + 28, 0x10000000)    # image base
    struct.pack_into("<I", data, 0x58 + 56, 0x2000)        # size of image
    sec = 0x58 + 0xE0
    data[sec:sec + 8] = b".text\0\0\0"
    struct.pack_into("<IIII", data, sec + 8, 4, 0x1000, 0x200, 0x200)
    data[0x200:0x204] = b"\xc3\x90\x90\x90"
    return bytes(data)


if __name__ == "__main__":
    unittest.main()
