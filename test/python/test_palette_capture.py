"""Tests for the palette-nearest capture path with a MOCKED fleet member.

Mocked evidence proves the adapter's refusals and bookkeeping only; it is never
original-game evidence. No network, game, or d2-fleet checkout is used.

    python -m unittest -v test.python.test_palette_capture
"""

import copy
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import capture_palette as P  # noqa: E402
from scripts.equivalence import compare as CMP  # noqa: E402
from scripts.equivalence import contracts as C  # noqa: E402
from scripts.equivalence import fleet_adapter as F  # noqa: E402

TOKEN = "secret-token-value-for-tests"
TEXT_RVA, TEXT_LEN, FN_RVA = 0x1000, 0x9000, 0x9d30
BASE = 0x6FE10000


class Refused(Exception):
    pass


def fake_image(fn_bytes=None):
    img = bytearray(TEXT_RVA + TEXT_LEN)
    fb = bytes.fromhex(fn_bytes or P.FUNCTION_BYTES)
    img[FN_RVA:FN_RVA + len(fb)] = fb
    return img


def fake_baseline(image):
    text = bytes(image[TEXT_RVA:TEXT_RVA + TEXT_LEN])
    b = {
        "schema": C.BASELINE_SCHEMA, "build": C.BASELINE_BUILD,
        "binaries": [{"name": "D2CMP.dll", "sha256": "1" * 64, "size": 1,
                      "pe": {"sections": {".text": {
                          "rva": "0x%x" % TEXT_RVA, "bytes": TEXT_LEN,
                          "sha256": hashlib.sha256(text).hexdigest()}}}}],
        "archives": [], "render": {},
        "enumeration": {"sources": [], "limits": [], "complete": False},
        "assets": [],
    }
    return C.seal(b)


class MockMember:
    """Implements the original routine's observed semantics over fake memory."""

    def __init__(self, image, tier="privileged", mode="ok", fail_after=None):
        self.image = image
        self.tier, self.mode, self.fail_after = tier, mode, fail_after
        self.mem = {}
        self.next = 0x10000000
        self.calls = 0
        self.alive = True

    def instance(self):
        if not self.alive:
            return {"ok": False, "error": "connection refused"}
        return {"ok": True, "tier": self.tier, "slot": 1, "version": "lod-113c",
                "exeSha256": "2" * 64, "pid": 1}

    def modules(self):
        return {"modules": [{"name": "D2CMP.dll", "base": "0x%08X" % BASE,
                             "preferred_base": "0x%08X" % BASE}]}

    def read_rva(self, module, rva, n):
        return bytes(self.image[rva:rva + n])

    def alloc(self, n):
        a = self.next
        self.mem[a] = bytearray(n)
        self.next += 0x1000
        return a

    def write(self, addr, data):
        self.mem[addr][:len(data)] = data

    def call(self, module, rva, ecx=0, edx=0, stack=(), cdecl=False, watch=(),
             restore=False, timeout_ms=0):
        self.calls += 1
        if self.fail_after is not None and self.calls > self.fail_after:
            if self.mode == "death":
                self.alive = False
                raise Refused("connection reset")
            if self.mode == "timeout":
                raise Refused("timed out")
            if self.mode == "fault":
                return {"ok": True, "fn": "0x%08X" % (BASE + rva), "eax": 0,
                        "seh": {"code": "0xC0000005"}}
        entries, count, c0, c1, c2 = stack
        scratch = max(a for a in self.mem if a <= entries)
        buf = self.mem[scratch]
        off = entries - scratch
        best, bi = 0xFFFFFFFF, 0
        for i in range(count):
            e = buf[off + 4 * i: off + 4 * i + 3]
            d = ((e[0] - c0) ** 2 + (e[1] - c1) ** 2 + (e[2] - c2) ** 2) & 0xFFFFFFFF
            if d < best:
                best, bi = d, i
        eax = (count & 0xFFFFFF00) | bi
        if self.mode == "out_of_range":
            eax = 0xFF
        reply = {"ok": True, "fn": "0x%08X" % (BASE + rva), "eax": eax, "edx": 0,
                 "memdiff": []}
        if self.mode == "writes":
            reply["memdiff"] = [{"addr": "0x%08X" % scratch, "after": "00"}]
        return reply


def session_for(member, baseline):
    return F.FleetReferenceSession(Path("unused"), "mock", 1, TOKEN, baseline,
                                   member_factory=lambda h, p, t: member,
                                   transport=(type("T", (), {"Refused": Refused}), "3" * 64))


class CaptureTests(unittest.TestCase):
    def setUp(self):
        self.image = fake_image()
        self.baseline = fake_baseline(self.image)
        self.inputs = P.generate_inputs()
        self.contract = P.build_contract(self.baseline, self.inputs)

    def capture(self, member, contract=None):
        return P.capture(contract or self.contract, self.baseline, self.inputs,
                         session_for(member, self.baseline))

    def test_mocked_capture_is_valid_and_independent_of_editor(self):
        m = MockMember(self.image)
        cap = self.capture(m)
        C.validate_capture(cap, self.contract, self.baseline)
        self.assertEqual(len(cap["vectors"]), len(self.inputs["vectors"]))
        self.assertEqual(m.calls, len(self.inputs["vectors"]))
        first = cap["vectors"][0]
        self.assertEqual(first["id"], "main-exact-001")
        self.assertEqual(first["output"], {"index": 0, "writes": []})  # relative to 1
        self.assertNotIn(TOKEN, json.dumps(cap))
        self.assertEqual(cap["adapter"]["transport_sha256"], "3" * 64)

    def test_generated_inputs_are_deterministic_and_deduplicated(self):
        again = P.generate_inputs()
        self.assertEqual(again["digest"], self.inputs["digest"])
        keys = [(v["input"]["palette"], tuple(v["input"]["rgb"]))
                for v in self.inputs["vectors"]]
        self.assertEqual(len(keys), len(set(keys)))
        groups = {v["group"] for v in self.inputs["vectors"]}
        self.assertTrue({"exact", "grid", "duplicate", "equidistant"} <= groups)
        main = bytes.fromhex(self.inputs["palettes"]["main"])
        rgbs = {main[4 * i:4 * i + 3] for i in range(256)}
        self.assertEqual(len(rgbs), 256)
        self.assertEqual(main[3], 0)
        self.assertTrue(all(main[4 * i + 3] == 255 for i in range(1, 256)))

    def test_tracked_fixture_matches_generator(self):
        tracked = json.loads((REPO_ROOT / P.INPUTS_REL).read_text(encoding="utf-8"))
        self.assertEqual(tracked["digest"], self.inputs["digest"])

    def test_wrong_original_bytes_are_refused(self):
        img = fake_image(P.FUNCTION_BYTES[:-6] + "c20800")
        m = MockMember(img)
        # Module identity agrees with its own baseline; the function does not.
        other = fake_baseline(img)
        contract = P.build_contract(other, self.inputs)
        with self.assertRaisesRegex(F.ReferenceRefused, "bytes hash"):
            P.capture(contract, other, self.inputs, session_for(m, other))
        self.assertEqual(m.calls, 0)

    def test_wrong_module_section_is_refused(self):
        img = fake_image()
        img[TEXT_RVA] = 0xCC
        with self.assertRaisesRegex(F.ReferenceRefused, ".text"):
            self.capture(MockMember(img))

    def test_wrong_or_missing_abi_recipe_is_refused(self):
        c = copy.deepcopy(self.contract)
        c["reference"]["abi_recipe"]["kind"] = "cdecl-guess/1"
        c["reference"]["abi_recipe_digest"] = C.payload_sha256(c["reference"]["abi_recipe"])
        c = C.seal(c)
        with self.assertRaisesRegex(F.ReferenceRefused, "unsupported ABI"):
            self.capture(MockMember(self.image), contract=c)
        c = copy.deepcopy(self.contract)
        del c["reference"]["abi_recipe"]
        c = C.seal(c)
        with self.assertRaisesRegex(F.ReferenceRefused, "no ABI recipe"):
            self.capture(MockMember(self.image), contract=c)

    def test_capture_with_different_recipe_digest_is_refused_by_validation(self):
        cap = self.capture(MockMember(self.image))
        cap["adapter"]["abi_recipe_digest"] = "9" * 64
        cap["adapter_digest"] = C.payload_sha256(cap["adapter"])
        cap = C.seal(cap)
        with self.assertRaisesRegex(C.ValidationError, "ABI recipe"):
            C.validate_capture(cap, self.contract, self.baseline)

    def test_missing_privileged_capability_is_unavailable(self):
        with self.assertRaisesRegex(F.ReferenceUnavailable, "privileged"):
            self.capture(MockMember(self.image, tier="open"))

    def test_process_death_is_unavailable_and_writes_nothing(self):
        with self.assertRaisesRegex(F.ReferenceUnavailable, "died"):
            self.capture(MockMember(self.image, mode="death", fail_after=5))

    def test_timeout_and_fault_are_unavailable(self):
        with self.assertRaisesRegex(F.ReferenceUnavailable, "refused"):
            self.capture(MockMember(self.image, mode="timeout", fail_after=3))
        with self.assertRaisesRegex(F.ReferenceUnavailable, "faulted"):
            self.capture(MockMember(self.image, mode="fault", fail_after=0))

    def test_out_of_range_index_is_refused(self):
        with self.assertRaisesRegex(F.ReferenceRefused, "outside"):
            self.capture(MockMember(self.image, mode="out_of_range"))

    def test_empty_and_truncated_vector_sets_are_refused(self):
        empty = dict(self.inputs, vectors=[])
        empty = C.seal(empty)
        c = P.build_contract(self.baseline, empty)
        with self.assertRaisesRegex(C.ValidationError, "no vectors"):
            P.capture(c, self.baseline, empty, session_for(MockMember(self.image),
                                                          self.baseline))
        short = C.seal(dict(self.inputs, vectors=self.inputs["vectors"][:-1]))
        with self.assertRaisesRegex(C.ValidationError, "not the one"):
            P.capture(self.contract, self.baseline, short,
                      session_for(MockMember(self.image), self.baseline))

    def test_equal_returns_with_unexpected_writes_mismatch(self):
        cap = self.capture(MockMember(self.image, mode="writes"))
        actual = {"lane": "portable", "platform": "test",
                  "provenance": {"source_state": {}, "executable": {"sha256": "4" * 64},
                                 "build": {}, "packages": []},
                  "vectors": [{"id": v["id"],
                               "output": {"index": v["output"]["index"] + 1, "writes": []}}
                              for v in cap["vectors"]]}
        case = CMP.compare_case(self.contract, cap, actual)
        self.assertEqual(case["status"], "mismatch")
        self.assertTrue(case["failures"][0]["path"].endswith(".writes.length"))
        clean = self.capture(MockMember(self.image))
        self.assertEqual(CMP.compare_case(self.contract, clean, actual)["status"], "pass")

    def test_transport_loader(self):
        with tempfile.TemporaryDirectory(prefix="fleet root ") as d:
            with self.assertRaises(F.ReferenceUnavailable):
                F.load_transport(Path(d))
            (Path(d) / "reimpl").mkdir()
            src = b"class Member: pass\nclass Refused(Exception): pass\n" \
                  b"def bytes_agree(*a): return True, ''\n"
            (Path(d) / "reimpl" / "prove.py").write_bytes(src)
            mod, sha = F.load_transport(Path(d))
            self.assertEqual(sha, hashlib.sha256(src).hexdigest())
            self.assertTrue(hasattr(mod, "Member"))


if __name__ == "__main__":
    unittest.main()
