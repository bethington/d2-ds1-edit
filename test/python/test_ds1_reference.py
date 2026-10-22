"""
Mocked integration tests for the original-game DS1 loader adapter
(scripts/equivalence/ds1_reference.py).

No game, MPQ, fleet member or probe is used. A fake member holds a synthetic
D2Common image and a fake loader that decodes version-18 files into the
loader's record layout in fake memory (with a stand-in object-ID remap), so
the adapter's own logic -- identity guards, the patch/restore window, record
decoding, same-stage comparison and the never-success rules -- is exercised.
The fake is a test double for the adapter, not evidence about the game.

Run from the repository root:
    python -m unittest -v test.python.test_ds1_reference
"""

import copy
import hashlib
import importlib.util
import struct
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import contracts as C  # noqa: E402
from scripts.equivalence import ds1_reference as R  # noqa: E402

_spec = importlib.util.spec_from_file_location(
    "make_ds1_fixtures", REPO_ROOT / "test" / "fixtures" / "make_ds1_fixtures.py")
fx = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(fx)

BASE = 0x6FD50000
TEXT_RVA, TEXT_LEN = 0x1000, 0xC000
FN_LEN = 0x80


def remap_id(unit_type, raw):
    """Stand-in for the game's act/version tables: the point is only that the
    loader's IDs are NOT the file's IDs."""
    return raw + 1000 if unit_type == 2 else raw * 3


class FakeRefused(Exception):
    pass


class FakeMember:
    """Flat memory + a fake loader reachable only through the patch site."""

    def __init__(self, tier="privileged", mapped=True, corrupt_text=False,
                 fault=False, break_restore=False):
        self.mem = {}
        self.tier, self.mapped, self.fault = tier, mapped, fault
        self.break_restore = break_restore
        self.next_alloc = 0x10000000
        image = bytearray(TEXT_RVA + TEXT_LEN)
        for i in range(TEXT_RVA, TEXT_RVA + TEXT_LEN):
            image[i] = (i * 7) & 0xFF
        image[R.PATCH_RVA:R.PATCH_RVA + 12] = R.PATCH_ORIGINAL
        self._poke(BASE, bytes(image))
        self.text_sha = hashlib.sha256(bytes(image[TEXT_RVA:])).hexdigest()
        self.fn_sha = hashlib.sha256(bytes(image[R.LOADER_RVA:R.LOADER_RVA + FN_LEN])).hexdigest()
        if corrupt_text:
            self._poke(BASE + 0x2000, b"\xcc")
        self.writes_to_site = 0

    # memory -------------------------------------------------------------
    def _poke(self, addr, data):
        for i, b in enumerate(data):
            self.mem[addr + i] = b

    def read(self, addr, n):
        return bytes(self.mem.get(addr + i, 0) for i in range(n))

    def read_rva(self, module, rva, n):
        if not self.mapped:
            raise FakeRefused("module not loaded")
        return self.read(BASE + rva, n)

    def write(self, addr, data):
        site = BASE + R.PATCH_RVA
        if site <= addr < site + 12:
            self.writes_to_site += 1
            if self.break_restore and data == R.PATCH_ORIGINAL:
                return {"ok": True}
        self._poke(addr, data)
        return {"ok": True}

    def alloc(self, n):
        a = self.next_alloc
        self.next_alloc += (n + 0xFFFF) & ~0xFFFF
        return a

    def instance(self):
        return {"ok": True, "tier": self.tier, "slot": 1, "version": "lod-113c",
                "exeSha256": "0" * 64, "pid": 1}

    def modules(self):
        if not self.mapped:
            return {"modules": []}
        return {"modules": [{"name": "D2Common.dll", "base": "%08x" % BASE,
                             "preferred_base": "%08x" % BASE}]}

    # the fake loader ----------------------------------------------------
    def call(self, module, rva, ecx=0, edx=0, stack=(), cdecl=False, timeout_ms=0, **kw):
        assert rva == R.LOADER_RVA and not cdecl and len(stack) == 2
        site = self.read(BASE + R.PATCH_RVA, 12)
        assert site[0] == 0xB8 and site[5:] == b"\x90" * 7, "called without the patch"
        if self.fault:
            return {"ok": True, "faulted": True, "faultCode": "0xc0000005"}
        buf = struct.unpack_from("<I", site, 1)[0]
        self._fake_load(stack[0], buf)
        return {"ok": True, "eax": "0x0"}

    def _alloc_put(self, data):
        a = self.alloc(len(data) or 4)
        self._poke(a, data)
        return a

    def _fake_load(self, rec, buf):
        d = self.read(buf, 1 << 16)
        u = lambda o: struct.unpack_from("<i", d, o)[0]  # noqa: E731
        v, w1, h1, act0, tt = u(0), u(4), u(8), u(12), u(16)
        assert v == 18, "the fake loader only models version 18"
        p = 20
        nfiles = u(p); p += 4
        for _ in range(nfiles):
            p = d.index(b"\0", p) + 1
        nw, nf = u(p), u(p + 4); p += 8
        cells = (w1 + 1) * (h1 + 1)
        r = [0] * 23
        r[0], r[1], r[3], r[4], r[5], r[6] = tt, buf, w1, h1, nw, nf
        for i in range(nw):
            r[11 + i] = buf + p; p += 4 * cells
            r[7 + i] = buf + p; p += 4 * cells
        for i in range(nf):
            r[15 + i] = buf + p; p += 4 * cells
        r[17] = buf + p; p += 4 * cells
        if tt in (1, 2):
            r[18] = buf + p; p += 4 * cells
        nobj = u(p); p += 4
        head = 0
        nodes = []
        for _ in range(nobj):
            t, i, x, y, fl = (u(p + 4 * k) for k in range(5)); p += 20
            node = self._alloc_put(struct.pack("<8i", t, remap_id(t, i), x, head, 0, t, y, fl))
            head = node
            nodes.append((node, x, y))
        r[21] = head
        if tt in (1, 2):
            p += 4  # v18 pre-group dword
            ng = u(p); p += 4
            garr = bytearray()
            for _ in range(ng):
                gx, gy, gw, gh, unk = (u(p + 4 * k) for k in range(5)); p += 20
                garr += struct.pack("<6i", gx, gy, gw, gh, 0, unk)
            r[19], r[20] = ng, self._alloc_put(bytes(garr))
        npc = u(p); p += 4
        for _ in range(npc):
            n, x, y = u(p), u(p + 4), u(p + 8); p += 12
            pts = bytearray()
            for _ in range(n):
                px, py, act = u(p), u(p + 4), u(p + 8); p += 12
                pts += struct.pack("<3i", act, px, py)
            for node, ox, oy in reversed(nodes):  # list order: newest first
                if (ox, oy) == (x, y):
                    hdr = self._alloc_put(struct.pack("<iI", n, self._alloc_put(bytes(pts))))
                    self._poke(node + 0x10, struct.pack("<I", hdr))
                    break
        self._poke(rec, b"".join(struct.pack("<I", x & 0xFFFFFFFF) for x in r))


def make_baseline(member):
    b = {
        "schema": C.BASELINE_SCHEMA, "build": C.BASELINE_BUILD,
        "binaries": [{"name": "D2Common.dll", "sha256": "1" * 64, "size": 1,
                      "pe": {"sections": {".text": {"rva": "0x%x" % TEXT_RVA,
                                                    "bytes": TEXT_LEN,
                                                    "sha256": member.text_sha}}}}],
        "archives": [], "enumeration": {"sources": [], "limits": [], "complete": True},
        "assets": [], "render": {},
    }
    return C.seal(b)


def make_contract(baseline, fn_sha, axes_cov="sampled"):
    recipe = {"kind": R.RECIPE_KIND, "function_length": FN_LEN, "timeout_ms": 1000}
    c = {
        "schema": C.CONTRACT_SCHEMA, "id": "ds1-loader-test", "kind": "ds1-loader",
        "baseline_digest": baseline["digest"],
        "reference": {"module": "D2Common.dll", "rva": "0xb560",
                      "function_bytes_sha256": fn_sha, "abi_recipe": recipe,
                      "abi_recipe_digest": C.payload_sha256(recipe)},
        "domain": {"description": "test", "axes": {
            "objects_and_flags": {"coverage": axes_cov, "required": True}}},
        "outputs": [{"name": "game_invariant", "type": "json"}],
        "normalizations": [], "limitations": ["test"], "required": True,
        "success_outcome": "pass",
    }
    return C.seal(c)


def intended_map(**over):
    spec = fx.default_spec(18)
    spec.update(over)
    return fx.expected_snapshot(spec)


class Harness:
    def __init__(self, **member_kw):
        self.member = FakeMember(**member_kw)
        self.baseline = make_baseline(self.member)
        self.contract = make_contract(self.baseline, self.member.fn_sha)
        self.session = R.LoaderSession(self.member, self.baseline, refused=FakeRefused)


class OpenGuardTests(unittest.TestCase):
    def test_open_succeeds_on_matching_identity(self):
        h = Harness()
        info = h.session.open(h.contract, 4096)
        self.assertEqual(info["module_base"], "0x%08x" % BASE)

    def test_wrong_function_bytes_are_refused(self):
        h = Harness()
        bad = make_contract(h.baseline, "f" * 64)
        with self.assertRaises(R.ReferenceRefused):
            h.session.open(bad, 4096)

    def test_wrong_baseline_text_is_refused(self):
        h = Harness(corrupt_text=True)
        with self.assertRaises(R.ReferenceRefused):
            h.session.open(h.contract, 4096)

    def test_contract_for_another_baseline_is_refused(self):
        h = Harness()
        other = dict(h.baseline, render={"x": 1})
        other = C.seal(other)
        with self.assertRaises(R.ReferenceRefused):
            h.session.open(make_contract(other, h.member.fn_sha), 4096)

    def test_unprivileged_member_is_unavailable(self):
        h = Harness(tier="read")
        with self.assertRaises(R.ReferenceUnavailable):
            h.session.open(h.contract, 4096)

    def test_module_not_loaded_is_unavailable(self):
        h = Harness(mapped=False)
        with self.assertRaises(R.ReferenceUnavailable):
            h.session.open(h.contract, 4096)

    def test_recipe_kind_must_be_declared(self):
        h = Harness()
        c = copy.deepcopy(h.contract)
        c["reference"]["abi_recipe"]["kind"] = "guess"
        c["reference"]["abi_recipe_digest"] = C.payload_sha256(c["reference"]["abi_recipe"])
        with self.assertRaises(R.ReferenceRefused):
            h.session.open(C.seal(c), 4096)


class LoadTests(unittest.TestCase):
    def test_patch_is_restored_and_record_decoded(self):
        h = Harness()
        h.session.open(h.contract, 1 << 14)
        exp = intended_map()
        g = h.session.load(R.encode_reference(exp))
        self.assertEqual(h.member.read(BASE + R.PATCH_RVA, 12), R.PATCH_ORIGINAL)
        self.assertEqual(h.member.writes_to_site, 2)
        self.assertEqual(R.diff(R.editor_invariant(exp, 18), R.game_invariant(g)), [])
        # file order restored from the prepended list, ids at the game's stage
        self.assertEqual([o["id"] for o in g["objects"]],
                         [remap_id(o["type"], o["id"]) for o in exp["objects"]])

    def test_unrestored_patch_is_fatal(self):
        h = Harness(break_restore=True)
        h.session.open(h.contract, 1 << 14)
        with self.assertRaises(R.ReferenceRefused):
            h.session.load(R.encode_reference(intended_map()))

    def test_fault_is_unavailable_not_an_output(self):
        h = Harness(fault=True)
        h.session.open(h.contract, 1 << 14)
        with self.assertRaises(R.ReferenceUnavailable):
            h.session.load(R.encode_reference(intended_map()))
        self.assertEqual(h.member.read(BASE + R.PATCH_RVA, 12), R.PATCH_ORIGINAL)

    def test_patch_site_already_changed_is_refused(self):
        h = Harness()
        h.session.open(h.contract, 1 << 14)
        h.member._poke(BASE + R.PATCH_RVA, b"\xcc")
        with self.assertRaises(R.ReferenceRefused):
            h.session.load(R.encode_reference(intended_map()))

    def test_load_before_open_is_refused(self):
        h = Harness()
        with self.assertRaises(R.ReferenceRefused):
            h.session.load(b"\0" * 64)


class CompareTests(unittest.TestCase):
    """compare_saved_map with the fake loader and an injected editor reader."""

    def run_case(self, saved_bytes, expected, editor_snap=None, harness=None,
                 session="open"):
        h = harness or Harness()
        if session == "open":
            h.session.open(h.contract, 1 << 15)
            sess = h.session
        else:
            sess = session
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "saved map.ds1"
            p.write_bytes(saved_bytes)
            reader = (lambda _p: copy.deepcopy(editor_snap if editor_snap is not None
                                               else expected))
            return R.compare_saved_map(p, expected, h.contract, sess, Path("unused"),
                                       Path(d) / "out", editor_reader=reader)

    def test_faithful_save_passes(self):
        exp = intended_map()
        case = self.run_case(R.encode_reference(exp), exp)
        self.assertEqual(case["status"], "pass", case["failures"])
        self.assertEqual(case["schema"], C.CASE_SCHEMA)

    def test_unavailable_session_is_not_success(self):
        exp = intended_map()
        case = self.run_case(R.encode_reference(exp), exp, session=None)
        self.assertEqual(case["status"], "unavailable")
        self.assertNotEqual(case["status"], case["success_outcome"])

    def test_partial_axis_is_not_success(self):
        h = Harness()
        h.contract = make_contract(h.baseline, h.member.fn_sha, axes_cov="partial")
        exp = intended_map()
        case = self.run_case(R.encode_reference(exp), exp, harness=h)
        self.assertEqual(case["status"], "partial")

    def _altered(self, mutate):
        exp = intended_map()
        changed = copy.deepcopy(exp)
        mutate(changed)
        # the editor reads its own (wrong) save faithfully; only the game
        # comparison against the intended map can catch it
        return self.run_case(R.encode_reference(changed), exp, editor_snap=exp)

    def test_altered_tile_fails(self):
        def m(s):
            s["cells"][3]["walls"][1][2] ^= 0x40
        case = self._altered(m)
        self.assertEqual(case["status"], "mismatch")
        self.assertTrue(any("cells[3].walls[1]" in f.get("path", "") for f in case["failures"]))

    def test_altered_object_fails(self):
        def m(s):
            s["objects"][1]["y"] += 1
        case = self._altered(m)
        self.assertEqual(case["status"], "mismatch")
        self.assertTrue(any(f.get("path", "").startswith("objects") for f in case["failures"]))

    def test_altered_raw_object_id_fails_at_the_same_stage(self):
        def m(s):
            s["objects"][0]["id"] += 1
        case = self._altered(m)
        self.assertEqual(case["status"], "mismatch")
        self.assertTrue(all(f["kind"] == "game_same_stage" for f in case["failures"]))
        self.assertTrue(any(f["path"].endswith(".id") for f in case["failures"]))

    def test_altered_path_fails(self):
        def m(s):
            s["paths"][0]["points"][1][2] += 1
        case = self._altered(m)
        self.assertEqual(case["status"], "mismatch")
        self.assertTrue(any(f.get("path", "").startswith("paths") for f in case["failures"]))

    def test_object_ids_are_never_compared_across_stages(self):
        # the loader's IDs differ from the file's for every object; comparing
        # the raw intended IDs with the record would report a false mismatch
        exp = intended_map()
        case = self.run_case(R.encode_reference(exp), exp)
        self.assertEqual(case["status"], "pass")
        inv = R.game_invariant({"version": 18, "width": 1, "height": 1, "tag_type": 0,
                                "walls": 0, "floors": 0, "cells": [], "groups": None,
                                "objects": [{"x": 1, "y": 2, "flags": 0, "id": 99,
                                             "unit_type": 2, "type_field": 2,
                                             "path": None}]})
        self.assertEqual(inv["objects"], [[1, 2, 0]])

    def test_absent_reference_field_fails(self):
        # the game never reads groups of a tagType-0 map: an editor that kept
        # them would be writing something the game does not see
        exp = intended_map()
        saved = copy.deepcopy(exp)
        saved["tag_type"] = 0
        case = self.run_case(R.encode_reference(saved), exp, editor_snap=exp)
        self.assertEqual(case["status"], "mismatch")
        self.assertTrue(any(f["path"].startswith("groups") or f["path"] == "tag_type"
                            for f in case["failures"]))

    def test_editor_rejecting_its_own_save_fails(self):
        # e.g. a new optional tail the editor's reader rejects
        exp = intended_map()
        rejected = {"schema": "ds1edit.map/1", "status": "error", "error": "tail"}
        case = self.run_case(R.encode_reference(exp) + b"\x01\x02", exp, editor_snap=rejected)
        self.assertEqual(case["status"], "mismatch")
        self.assertTrue(any(f["kind"] == "editor_read" for f in case["failures"]))

    def test_controls_fail(self):
        exp = intended_map(wall_num=4, floor_num=2)
        good = R.encode_reference(exp)
        swapped = R._swap_wall_layers(good, exp)
        omitted = R._omit_first_object_flags(good, exp)
        for bad in (swapped, omitted):
            case = self.run_case(bad, exp)
            self.assertEqual(case["status"], "mismatch")
            self.assertEqual(R.control_verdict(case), "failed_as_required")
        ok = self.run_case(good, exp)
        self.assertEqual(R.control_verdict(ok), "CONTROL_DID_NOT_FAIL")


class DecodeTests(unittest.TestCase):
    def test_reference_encoding_matches_closed_form_layout(self):
        exp = intended_map(wall_num=4, floor_num=2)
        data = R.encode_reference(exp)
        self.assertEqual(len(data), R.v18_layout(exp)["end"])
        # orientation padding is zero, as the editor's writer emits it
        lay = R.v18_layout(exp)
        cells = exp["width"] * exp["height"]
        o = lay["layers_at"] + cells * 4
        self.assertEqual(data[o + 1:o + 4], b"\0\0\0")

    def test_legacy_v3_floor_is_projected_as_unlisted(self):
        snap = fx.expected_snapshot(fx.default_spec(3))
        inv = R.editor_invariant(snap)
        self.assertEqual(inv["floors"], 0)
        self.assertEqual(inv["cells"][0]["floors"], [])
        self.assertEqual(inv["cells"][0]["floor0_unlisted"],
                         R._dw(snap["cells"][0]["floors"][0]))
        # the same map re-saved at v18 lists its floor layer
        self.assertEqual(R.editor_invariant(snap, version=18)["floors"], 1)

    def test_out_of_bounds_objects_are_classified_not_hidden(self):
        snap = intended_map()
        h = Harness()
        h.session.open(h.contract, 1 << 15)
        with_oob = copy.deepcopy(snap)
        with_oob["objects"].append({"type": 2, "id": 17, "x": 194, "y": 19, "flags": 0})
        g = h.session.load(R.encode_reference(with_oob))
        self.assertTrue(R.compare_vanilla(snap, g), "the difference is still reported")
        self.assertEqual(R.classify_vanilla_mismatch(snap, g),
                         "editor_drops_out_of_bounds_objects")
        other = copy.deepcopy(with_oob)
        other["cells"][0]["walls"][0][0] ^= 1
        g2 = h.session.load(R.encode_reference(other))
        self.assertIsNone(R.classify_vanilla_mismatch(snap, g2))

    def test_patch_bytes_are_stack_neutral_mov(self):
        b = R.patch_bytes(0x12345678)
        self.assertEqual(b[:5], b"\xb8\x78\x56\x34\x12")
        self.assertEqual(b[5:], b"\x90" * 7)
        self.assertEqual(len(b), len(R.PATCH_ORIGINAL))

    def test_cache_walk_terminates(self):
        m = FakeMember()
        a, b = m.alloc(0x110), m.alloc(0x110)
        m._poke(a, b"DATA\\A.ds1\0")
        m._poke(a + 0x104, struct.pack("<III", 2, 0x1234, b))
        m._poke(b, b"DATA\\B.ds1\0")
        m._poke(b + 0x104, struct.pack("<III", 1, 0x5678, 0))
        m._poke(BASE + R.CACHE_HEAD_RVA, struct.pack("<I", a))
        got = R.walk_cache(m, BASE)
        self.assertEqual([e["name"] for e in got], ["DATA\\A.ds1", "DATA\\B.ds1"])
        m._poke(b + 0x10C, struct.pack("<I", a))  # a cycle must not hang
        with self.assertRaises(R.ReferenceRefused):
            R.walk_cache(m, BASE, limit=10)


if __name__ == "__main__":
    unittest.main()
