"""Package E task 1: the malformed-input campaign generator and runner.

Generator properties (determinism, distinct hashing, size bounds, boundary
coverage) and the runner's classification (crash, timeout, stray files,
silent success, controls, budget) with a mocked subprocess. The real probes
are driven by CTest ``adversarial_core``.

    python -m unittest -v test.python.test_adversarial
"""

import hashlib
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import adversarial as adv  # noqa: E402


def _names(kind, data, seed=7):
    return [n for n, _ in adv.mutations(kind, data, seed)]


class Generators(unittest.TestCase):
    def test_deterministic_for_a_seed_and_different_across_seeds(self):
        for kind in adv.KINDS:
            for sid, data in adv.valid_samples(kind).items():
                a = list(adv.mutations(kind, data, 11))
                b = list(adv.mutations(kind, data, 11))
                self.assertEqual(a, b, (kind, sid))
                c = list(adv.mutations(kind, data, 12))
                self.assertNotEqual(a, c, (kind, sid))

    def test_every_variant_is_distinct_and_differs_from_the_input(self):
        for kind in adv.KINDS:
            for sid, data in adv.valid_samples(kind).items():
                seen = {hashlib.sha256(data).hexdigest()}
                names = set()
                for name, blob in adv.mutations(kind, data, 3):
                    h = hashlib.sha256(blob).hexdigest()
                    self.assertNotIn(h, seen, (kind, sid, name))
                    self.assertNotIn(name, names, (kind, sid, name))
                    seen.add(h)
                    names.add(name)

    def test_variants_stay_within_the_size_bounds(self):
        for kind in adv.KINDS:
            for sid, data in adv.valid_samples(kind).items():
                for name, blob in adv.mutations(kind, data, 5):
                    self.assertLessEqual(len(blob), len(data) + 8, (kind, sid, name))
                    self.assertLessEqual(len(blob), adv.DEFAULT_MAX_PAYLOAD)

    def test_truncations_cover_boundaries_and_the_final_bytes(self):
        for kind in adv.KINDS:
            for sid, data in adv.valid_samples(kind).items():
                names = set(_names(kind, data))
                n = len(data)
                for cut in (n - 1, n - 2, n - 3, 0, 1):
                    self.assertIn("trunc@%d" % cut, names, (kind, sid))
                for b in adv._boundaries(kind, data):
                    self.assertIn("trunc@%d" % b, names, (kind, sid, b))

    def test_ds1_layout_matches_the_encoder(self):
        from test.fixtures import make_ds1_fixtures as m
        for case in m.build_cases():
            if case["id"].startswith("bad_") or case["truncate"] or case["append"]:
                continue
            data = m.case_bytes(case)
            _, want = m.encode(case["spec"])
            got = adv.ds1_layout(data)
            self.assertIsNotNone(got, case["id"])
            for key in ("files_at", "layers_at", "objects_at", "groups_at", "paths_at"):
                self.assertEqual(got[key], want[key], (case["id"], key))

    def test_ds1_missing_nul_and_count_inflation(self):
        data = adv.valid_samples("ds1")["v18_default"]
        lay = adv.ds1_layout(data)
        names = _names("ds1", data)
        self.assertTrue(any(n.startswith("nul_missing@") for n in names))
        self.assertTrue(any(n.startswith("count_inflate@%d+" % lay["counts"][0])
                            for n in names))
        for name, blob in adv.mutations("ds1", data, 1):
            if name.startswith("nul_missing@"):
                pos = int(name.split("@")[1])
                self.assertEqual(data[pos], 0)
                self.assertNotEqual(blob[pos], 0)
                break
        else:
            self.fail("no nul_missing variant")

    def test_dt1_offset_and_length_overflow(self):
        data = adv.valid_samples("dt1")["synthetic_floor_wall"]
        got = dict(adv.mutations("dt1", data, 1))
        for name in ("u32@272=0xffffffff", "u32@268=0x7fffffff", "u32@268=0x80000000"):
            self.assertIn(name, got)
        for off in (276 + 72, 276 + 76, 276 + 80):    # tiles_ptr, length, number
            self.assertIn("u32@%d=0xffffffff" % off, got)
        self.assertTrue(any(n.startswith("u32@") and n.endswith("=0x80000000") for n in got))

    def test_sprite_variants_hit_widths_and_offsets(self):
        dcc = adv.valid_samples("dcc")["dcc_single"]
        names = set(_names("dcc", dcc))
        self.assertIn("u32@15=0xffffffff", names)           # direction offset
        self.assertTrue(any(n.startswith("u8@") and n.endswith("=0x80") for n in names))
        self.assertTrue(any(n.startswith("u8@") and n.endswith("=0x7f") for n in names))
        dc6 = adv.valid_samples("dc6")["dc6_rle_basic"]
        names = set(_names("dc6", dc6))
        off = struct.unpack_from("<I", dc6, 24)[0]
        self.assertIn("u32@24=0xffffffff", names)           # frame pointer
        self.assertIn("u32@%d=0xffffffff" % (off + 24), names)   # next/length field
        self.assertTrue(any(n.startswith("u8@%d=" % (off + 32)) for n in names))  # RLE head

    def test_cof_and_animdata_variants(self):
        cof = adv.valid_samples("cof")["synthetic_3layer"]
        names = set(_names("cof", cof))
        for pos in (0, 1, 2):
            self.assertIn("u8@%d=0xff" % pos, names)         # layers / fpd / dirs
        self.assertIn("trunc@%d" % (28 + 9 * 3), names)      # at the frame flags
        ad = adv.valid_samples("animdata")["authored"]
        names = set(_names("animdata", ad))
        self.assertIn("trunc@%d" % (len(ad) - 1), names)
        self.assertTrue(any(n.startswith("count@") for n in names))
        self.assertIn("u32@0=0xffffffff", names)

    def test_unknown_kind_is_refused(self):
        with self.assertRaises(adv.AdversarialError):
            list(adv.mutations("png", b"x", 1))

    def test_controls_are_exact_named_tails(self):
        ctl = adv.compat_controls("ds1")
        self.assertEqual(sorted(ctl), ["v13_group_block_cut_after_tile_x",
                                       "v13_trailing_48_zero_bytes",
                                       "v18_missing_npc_count"])
        for kind in adv.KINDS:
            if kind != "ds1":
                self.assertEqual(adv.compat_controls(kind), {})


PROFILE = {"schema": adv.PROFILE_SCHEMA, "id": "unit", "seed": 1,
           "timeout_s": 10, "max_payload_bytes": 1 << 20,
           "kinds": {"ds1": {"samples": ["v18_default"]}}}


class Runner(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)
        exe = self.tmp / "probe.exe"
        exe.write_bytes(b"x")
        self.probes = {"ds1": exe}
        self.one = [self._case("trunc@5")]

    def tearDown(self):
        self._tmp.cleanup()

    @staticmethod
    def _case(mutation, control=False, must_reject=False, kind="ds1",
              blob=b"\x12\x00\x00\x00abcdef"):
        return {"id": "%s.s.%s" % (kind, mutation), "kind": kind, "sample": "s",
                "mutation": mutation, "bytes": blob, "seed": 1, "control": control,
                "must_reject": must_reject, "oversize": False}

    def _run(self, cases, side_effect, profile=None):
        calls = []

        def fake(argv, **kw):
            calls.append((argv, kw))
            return side_effect(argv, kw)
        with mock.patch.object(adv.subprocess, "run", side_effect=fake):
            res = adv.run_adversarial(profile or PROFILE, self.probes, self.tmp / "out",
                                      cases=cases)
        return res, calls

    @staticmethod
    def _proc(rc, err=b""):
        return subprocess.CompletedProcess([], rc, b"", err)

    @staticmethod
    def _snapshot(argv, status="ok"):
        Path(argv[argv.index("--out") + 1]).write_text(json.dumps({"status": status}), "utf-8")

    def _accept(self, argv, kw):
        self._snapshot(argv)
        return self._proc(0)

    def test_declared_rejection_and_acceptance_are_controlled(self):
        res, calls = self._run(self.one, self._accept)
        self.assertEqual(res[0]["status"], "pass")
        self.assertEqual(res[0]["outcome"], "accepted")
        self.assertIs(calls[0][1]["shell"], False)
        self.assertEqual(calls[0][1]["timeout"], 10.0)
        res, _ = self._run(self.one, lambda a, k: self._proc(2))
        self.assertEqual(res[0]["status"], "rejected_expected")
        self.assertEqual(res[0]["success_outcome"], "rejected_expected")

    def test_access_violation_is_a_crash_with_a_surviving_artifact(self):
        res, _ = self._run(self.one, lambda a, k: self._proc(0xC0000005, b"boom"))
        r = res[0]
        self.assertEqual(r["status"], "mismatch")
        self.assertEqual(r["outcome"], "crash")
        self.assertIn("0xC0000005", r["failures"][0]["detail"])
        art = Path(r["artifact"])
        data = (art / "input.bin").read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(), r["input_sha256"])
        repro = json.loads((art / "repro.json").read_text("utf-8"))
        self.assertEqual(repro["seed"], 1)
        self.assertEqual(repro["exit_code"], 0xC0000005)
        self.assertIn(str(art / "input.bin"), repro["command"])
        # the scratch directory is gone, the failure is not
        self.assertFalse((self.tmp / "out" / "work").exists())

    def test_missing_dll_is_an_environment_error_not_a_result(self):
        res, _ = self._run(self.one, lambda a, k: self._proc(0xC0000135))
        self.assertEqual(res[0]["status"], "unavailable")
        self.assertEqual(res[0]["outcome"], "environment_error")
        self.assertNotIn("artifact", res[0])
        self.assertEqual(adv.summarize(res)["defects"], [res[0]["id"]])

    def test_the_wrong_nonzero_exit_code_is_a_crash(self):
        for rc in (1, 3, 64, -11):
            res, _ = self._run(self.one, lambda a, k, rc=rc: self._proc(rc))
            self.assertEqual(res[0]["outcome"], "crash", rc)

    def test_timeout_is_a_defect_and_keeps_the_input(self):
        def hang(argv, kw):
            raise subprocess.TimeoutExpired(argv, kw["timeout"])
        res, _ = self._run(self.one, hang)
        self.assertEqual(res[0]["outcome"], "timeout")
        self.assertEqual(res[0]["status"], "mismatch")
        self.assertTrue((Path(res[0]["artifact"]) / "input.bin").is_file())

    def test_a_file_outside_the_case_output_is_a_defect(self):
        def stray(argv, kw):
            (Path(kw["cwd"]) / "leak.bin").write_bytes(b"x")
            return self._proc(2)
        res, _ = self._run(self.one, stray)
        self.assertEqual(res[0]["outcome"], "stray_file")
        self.assertIn("leak.bin", res[0]["failures"][0]["detail"])

    def test_accepted_run_without_readable_output_is_a_defect(self):
        res, _ = self._run(self.one, lambda a, k: self._proc(0))
        self.assertEqual(res[0]["outcome"], "bad_output")

    def test_a_control_must_be_accepted(self):
        ctl = [self._case("control", control=True)]
        res, _ = self._run(ctl, lambda a, k: self._proc(2))
        self.assertEqual(res[0]["outcome"], "control_rejected")
        res, _ = self._run(ctl, self._accept)
        self.assertEqual(res[0]["status"], "pass")

    def test_silent_success_on_a_required_section_cut_is_a_defect(self):
        res, _ = self._run([self._case("trunc@5", must_reject=True)], self._accept)
        self.assertEqual(res[0]["outcome"], "silent_accept")

    def test_must_reject_applies_to_ds1_layer_cuts_only(self):
        data = adv.valid_samples("ds1")["v18_default"]
        lay = adv.ds1_layout(data)
        self.assertTrue(adv._must_reject("ds1", data, "trunc@%d" % (lay["layers_at"] + 10)))
        self.assertFalse(adv._must_reject("ds1", data, "trunc@%d" % lay["paths_at"]))
        self.assertFalse(adv._must_reject("ds1", data, "u32@0=0x0"))
        self.assertFalse(adv._must_reject("dcc", data, "trunc@3"))

    def test_roundtrip_runs_the_next_valid_operation(self):
        seen = []

        def ok(argv, kw):
            seen.append(argv[1])
            if argv[1] == "resave":
                Path(argv[argv.index("--out") + 1]).write_bytes(b"x")
            else:
                self._snapshot(argv)
            return self._proc(0)
        res, _ = self._run(self.one, ok, dict(PROFILE, roundtrip=True))
        self.assertEqual(res[0]["status"], "pass")
        self.assertEqual(seen, ["dump", "resave", "dump"])

    def test_missing_probe_is_unavailable_not_silent(self):
        self.probes = {}
        res, calls = self._run(self.one, lambda a, k: self._proc(0))
        self.assertEqual(res[0]["status"], "unavailable")
        self.assertEqual(calls, [])

    def test_budget_truncation_is_recorded_as_partial(self):
        cases = [self._case("trunc@%d" % i) for i in range(5)]
        res, _ = self._run(cases, lambda a, k: self._proc(2),
                           dict(PROFILE, campaign_budget_cases=3))
        self.assertEqual(len(res), 4)
        self.assertEqual(res[-1]["status"], "partial")
        self.assertIn("2 of 5", res[-1]["failures"][0]["detail"])

    def test_oversize_payload_is_partial_and_not_run(self):
        big = self._case("trunc@5")
        big["oversize"] = True
        res, calls = self._run([big], lambda a, k: self._proc(0))
        self.assertEqual(res[0]["status"], "partial")
        self.assertEqual(calls, [])

    def test_summary_lists_every_defect(self):
        res, _ = self._run(self.one, lambda a, k: self._proc(0xC0000005))
        s = adv.summarize(res)
        self.assertEqual(s["defects"], [res[0]["id"]])
        self.assertEqual(s["by_kind"]["ds1"]["mismatch"], 1)

    def test_profile_validation(self):
        for bad in ({}, dict(PROFILE, schema="x"), dict(PROFILE, seed="1"),
                    dict(PROFILE, kinds={"png": {}})):
            with self.assertRaises(adv.AdversarialError):
                adv.validate_profile(bad)

    def test_build_cases_order_and_ids_are_stable(self):
        a = adv.build_cases(PROFILE)
        b = adv.build_cases(PROFILE)
        self.assertEqual([c["id"] for c in a], [c["id"] for c in b])
        self.assertEqual(len({c["id"] for c in a}), len(a))
        self.assertTrue(a[0]["control"])
        self.assertTrue(a[0]["id"].startswith("ds1.v13_group_block"))
        self.assertTrue(all(not c["oversize"] for c in a))


class CommittedProfile(unittest.TestCase):
    def test_core_profile_is_valid_and_bounded(self):
        prof = adv.load_profile(REPO_ROOT / "test" / "equivalence" / "profiles"
                                / "adversarial-core.json")
        self.assertEqual(prof["timeout_s"], 10)
        self.assertEqual(prof["max_payload_bytes"], 1048576)
        self.assertEqual(set(prof["kinds"]), set(adv.KINDS))
        cases = adv.build_cases(prof)
        self.assertEqual(len({c["id"] for c in cases}), len(cases))
        self.assertLessEqual(len(cases), prof["campaign_budget_cases"])


if __name__ == "__main__":
    unittest.main()
