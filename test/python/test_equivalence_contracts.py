"""Tests for scripts/equivalence/contracts.py and compare.py.

Standard library only; no game installation, MPQ reader, or fleet access.

Run from the repository root:
    python -m unittest -v test.python.test_equivalence_contracts
or:
    python -m unittest discover -s test/python -p test_equivalence_contracts.py -v

The ``make_*`` builders below are reused by the other equivalence test modules.
"""

import copy
import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import compare as CMP  # noqa: E402
from scripts.equivalence import contracts as C  # noqa: E402

DLL_SHA = "a" * 64
FN_SHA = "b" * 64
RECIPE = {"convention": "test", "args": ["r", "g", "b"]}


# ---------------------------------------------------------------------------
# Builders (shared with other equivalence tests)
# ---------------------------------------------------------------------------

def make_baseline(dll_sha=DLL_SHA, **over):
    b = {
        "schema": C.BASELINE_SCHEMA,
        "build": C.BASELINE_BUILD,
        "binaries": [{"name": "D2CMP.dll", "sha256": dll_sha, "size": 1234}],
        "archives": [{"name": "Patch_D2.mpq", "priority": 0, "sha256": "c" * 64,
                      "size": 10}],
        "enumeration": {"sources": ["test"], "limits": [], "complete": True},
        "assets": [],
        "render": {},
    }
    b.update(over)
    return C.seal(b)


def make_contract(baseline, outputs=None, normalizations=None, axes=None,
                  recipe=RECIPE, **over):
    c = {
        "schema": C.CONTRACT_SCHEMA,
        "id": "test-contract",
        "kind": "function-call",
        "baseline_digest": baseline["digest"],
        "reference": {
            "module": "D2CMP.dll",
            "rva": "0x9d30",
            "function_bytes_sha256": FN_SHA,
            "abi_recipe_digest": C.payload_sha256(recipe),
            "abi_recipe": recipe,
        },
        "domain": {"description": "test domain",
                   "axes": axes if axes is not None else
                   {"query": {"coverage": "sampled", "required": True}}},
        "outputs": outputs if outputs is not None else [{"name": "index", "type": "int"}],
        "normalizations": normalizations or [],
        "limitations": ["synthetic test contract"],
        "required": True,
    }
    c.update(over)
    return C.seal(c)


def make_adapter(recipe=RECIPE):
    return {"name": "test-adapter", "version": "1",
            "abi_recipe_digest": C.payload_sha256(recipe)}


def make_capture(contract, baseline, vectors, adapter=None, **over):
    adapter = adapter or make_adapter()
    cap = {
        "schema": C.CAPTURE_SCHEMA,
        "contract_id": contract["id"],
        "contract_digest": contract["digest"],
        "baseline_digest": baseline["digest"],
        "adapter": adapter,
        "adapter_digest": C.payload_sha256(adapter),
        "original": {"module": "D2CMP.dll", "module_sha256": baseline["binaries"][0]["sha256"],
                     "rva": "0x9d30", "function_bytes_sha256": FN_SHA},
        "captured_at": "2026-10-22T00:00:00Z",
        "evidence": [{"kind": "test", "ref": "unit"}],
        "vectors": vectors,
        "input_sha256": C.payload_sha256(C.vector_inputs_payload(vectors)),
        "output_sha256": C.payload_sha256(C.vector_outputs_payload(vectors)),
        "limitations": [],
    }
    cap.update(over)
    return C.seal(cap)


def make_provenance():
    return {"source_state": {"commit": "0" * 40, "dirty_sha256": None},
            "executable": {"sha256": "d" * 64},
            "build": {"config": "Release", "compiler": "test"},
            "packages": []}


def make_actual(outputs_by_id, lane="portable", platform="test"):
    return {"lane": lane, "platform": platform, "provenance": make_provenance(),
            "vectors": [{"id": k, "output": v} for k, v in outputs_by_id.items()]}


def vec(vid, inp, **out):
    return {"id": vid, "input": inp, "output": out}


# ---------------------------------------------------------------------------

class DigestTests(unittest.TestCase):
    def test_digest_omits_only_own_digest_field(self):
        a = {"x": 1, "digest": "ignored", "nested": {"digest": 1}}
        b = {"nested": {"digest": 1}, "x": 1}
        self.assertEqual(C.canonical_digest(a), C.canonical_digest(b))
        c = {"x": 1, "nested": {"digest": 2}}
        self.assertNotEqual(C.canonical_digest(a), C.canonical_digest(c))

    def test_canonical_json_is_sorted_and_compact(self):
        self.assertEqual(C.canonical_json({"b": 1, "a": [1, 2]}), b'{"a":[1,2],"b":1}')


class ContractValidationTests(unittest.TestCase):
    def setUp(self):
        self.baseline = make_baseline()

    def test_valid_contract_loads_from_file(self):
        c = make_contract(self.baseline)
        with tempfile.TemporaryDirectory(prefix="eq contracts ") as d:
            p = Path(d) / "c.json"
            p.write_text(json.dumps(c), encoding="utf-8")
            self.assertEqual(C.load_contract(p)["id"], "test-contract")

    def test_unknown_normalization_is_refused(self):
        c = make_contract(self.baseline, normalizations=[{"name": "close_enough", "params": {}}])
        with self.assertRaisesRegex(C.ValidationError, "unknown normalization"):
            C.validate_contract(c)

    def test_missing_required_metadata_is_refused(self):
        for key in ("id", "kind", "baseline_digest", "reference", "domain", "outputs",
                    "normalizations", "limitations", "required"):
            c = make_contract(self.baseline)
            del c[key]
            c = C.seal(c)
            with self.assertRaises(C.ValidationError, msg=key):
                C.validate_contract(c)

    def test_tampered_contract_digest_is_refused(self):
        c = make_contract(self.baseline)
        c["required"] = False
        with self.assertRaisesRegex(C.ValidationError, "digest mismatch"):
            C.validate_contract(c)

    def test_embedded_recipe_must_match_its_digest(self):
        c = make_contract(self.baseline)
        c["reference"]["abi_recipe"] = {"convention": "other"}
        c = C.seal(c)
        with self.assertRaisesRegex(C.ValidationError, "abi_recipe"):
            C.validate_contract(c)

    def test_normalization_must_name_declared_output(self):
        c = make_contract(self.baseline, normalizations=[
            {"name": "index_offset", "params": {"output": "nope", "offset": 1}}])
        with self.assertRaisesRegex(C.ValidationError, "undeclared output"):
            C.validate_contract(c)


class CaptureValidationTests(unittest.TestCase):
    def setUp(self):
        self.baseline = make_baseline()
        self.contract = make_contract(self.baseline)
        self.vectors = [vec("v1", [1, 2, 3], index=7)]

    def cap(self, **over):
        return make_capture(self.contract, self.baseline, copy.deepcopy(self.vectors), **over)

    def test_valid_capture_is_accepted(self):
        C.validate_capture(self.cap(), self.contract, self.baseline)

    def test_wrong_baseline_digest_is_refused(self):
        other = make_baseline(dll_sha="e" * 64)
        with self.assertRaisesRegex(C.ValidationError, "baseline"):
            C.validate_capture(self.cap(), self.contract, other)
        with self.assertRaisesRegex(C.ValidationError, "baseline_digest"):
            C.validate_capture(self.cap(baseline_digest="f" * 64), self.contract, self.baseline)

    def test_capture_from_different_dll_is_refused(self):
        cap = self.cap()
        cap["original"]["module_sha256"] = "e" * 64
        cap = C.seal(cap)
        with self.assertRaisesRegex(C.ValidationError, "sha256"):
            C.validate_capture(cap, self.contract, self.baseline)

    def test_capture_against_different_function_bytes_is_refused(self):
        cap = self.cap()
        cap["original"]["function_bytes_sha256"] = "e" * 64
        cap = C.seal(cap)
        with self.assertRaisesRegex(C.ValidationError, "function bytes"):
            C.validate_capture(cap, self.contract, self.baseline)

    def test_wrong_contract_digest_is_refused(self):
        with self.assertRaisesRegex(C.ValidationError, "contract_digest"):
            C.validate_capture(self.cap(contract_digest="f" * 64), self.contract, self.baseline)

    def test_wrong_adapter_recipe_is_refused(self):
        adapter = make_adapter(recipe={"convention": "guessed"})
        with self.assertRaisesRegex(C.ValidationError, "ABI recipe"):
            C.validate_capture(self.cap(adapter=adapter), self.contract, self.baseline)

    def test_tampered_adapter_digest_is_refused(self):
        with self.assertRaisesRegex(C.ValidationError, "adapter_digest"):
            C.validate_capture(self.cap(adapter_digest="f" * 64), self.contract, self.baseline)

    def test_adapter_not_on_contract_allowlist_is_refused(self):
        contract = make_contract(self.baseline, accepted_adapter_digests=["f" * 64])
        cap = make_capture(contract, self.baseline, copy.deepcopy(self.vectors))
        with self.assertRaisesRegex(C.ValidationError, "not accepted"):
            C.validate_capture(cap, contract, self.baseline)

    def test_modified_payload_is_refused(self):
        cap = self.cap()
        cap["vectors"][0]["output"]["index"] = 8
        cap = C.seal(cap)  # even a re-sealed capture: payload hash no longer matches
        with self.assertRaisesRegex(C.ValidationError, "output_sha256"):
            C.validate_capture(cap, self.contract, self.baseline)
        cap = self.cap()
        cap["vectors"][0]["input"] = [9, 9, 9]
        with self.assertRaisesRegex(C.ValidationError, "input_sha256"):
            C.validate_capture(cap, self.contract, self.baseline)

    def test_duplicate_vector_id_is_refused(self):
        self.vectors = [vec("v1", [1, 2, 3], index=7), vec("v1", [4, 5, 6], index=8)]
        with self.assertRaisesRegex(C.ValidationError, "duplicates vector id"):
            C.validate_capture(self.cap(), self.contract, self.baseline)

    def test_no_vectors_is_refused(self):
        self.vectors = []
        with self.assertRaisesRegex(C.ValidationError, "empty"):
            C.validate_capture(self.cap(), self.contract, self.baseline)

    def test_truncated_capture_is_refused(self):
        with self.assertRaisesRegex(C.ValidationError, "truncated"):
            C.validate_capture(self.cap(truncated=10), self.contract, self.baseline)

    def test_vector_missing_declared_output_is_refused(self):
        self.vectors = [{"id": "v1", "input": [1], "output": {}}]
        with self.assertRaisesRegex(C.ValidationError, "lacks declared output"):
            C.validate_capture(self.cap(), self.contract, self.baseline)

    def test_non_utc_timestamp_is_refused(self):
        with self.assertRaisesRegex(C.ValidationError, "UTC"):
            C.validate_capture(self.cap(captured_at="2026-10-22T00:00:00+02:00"),
                               self.contract, self.baseline)


class CompareTests(unittest.TestCase):
    def setUp(self):
        self.baseline = make_baseline()

    def run_case(self, contract, vectors, actual_outputs):
        cap = make_capture(contract, self.baseline, vectors)
        C.validate_capture(cap, contract, self.baseline)
        return CMP.compare_case(contract, cap, make_actual(actual_outputs))

    def test_matching_one_vector_passes(self):
        c = make_contract(self.baseline)
        case = self.run_case(c, [vec("v1", [1, 2, 3], index=7)], {"v1": {"index": 7}})
        self.assertEqual(case["schema"], C.CASE_SCHEMA)
        self.assertEqual(case["status"], "pass")
        self.assertEqual(case["coverage"]["distinct_inputs_compared"], 1)
        self.assertEqual(case["failures"], [])
        self.assertTrue(CMP.qualifies(case))
        for key in ("id", "lane", "platform", "required", "status", "contract_digest",
                    "baseline_digest", "input_sha256", "expected_sha256", "actual_sha256",
                    "coverage", "failures", "provenance", "limitations"):
            self.assertIn(key, case)

    def test_first_differing_integer_is_reported(self):
        c = make_contract(self.baseline)
        vs = [vec("v1", [1], index=1), vec("v2", [2], index=2), vec("v3", [3], index=3)]
        case = self.run_case(c, vs, {"v1": {"index": 1}, "v2": {"index": 9},
                                     "v3": {"index": 8}})
        self.assertEqual(case["status"], "mismatch")
        f = case["failures"][0]
        self.assertEqual((f["vector"], f["path"], f["expected"], f["actual"]),
                         ("v2", "vectors[v2].output.index", 2, 9))
        self.assertEqual(case["coverage"]["vectors_mismatched"], 2)
        self.assertFalse(CMP.qualifies(case))

    def test_first_differing_byte_is_reported(self):
        c = make_contract(self.baseline, outputs=[{"name": "buf", "type": "bytes"}])
        case = self.run_case(c, [vec("v1", [0], buf="00010203")], {"v1": {"buf": "00010903"}})
        self.assertEqual(case["status"], "mismatch")
        self.assertEqual(case["failures"][0]["path"], "vectors[v1].output.buf[2]")
        self.assertEqual((case["failures"][0]["expected"], case["failures"][0]["actual"]), (2, 9))

    def test_equal_return_with_different_memory_write_is_mismatch(self):
        c = make_contract(self.baseline, outputs=[{"name": "eax", "type": "int"},
                                                  {"name": "mem_out", "type": "bytes"}])
        case = self.run_case(c, [vec("v1", [0], eax=1, mem_out="aabb")],
                             {"v1": {"eax": 1, "mem_out": "aabc"}})
        self.assertEqual(case["status"], "mismatch")
        self.assertEqual(case["failures"][0]["path"], "vectors[v1].output.mem_out[1]")

    def test_missing_output_and_missing_vector_are_mismatches(self):
        c = make_contract(self.baseline)
        case = self.run_case(c, [vec("v1", [1], index=1)], {"v1": {}})
        self.assertEqual(case["status"], "mismatch")
        self.assertEqual(case["failures"][0]["kind"], "missing_output")
        case = self.run_case(c, [vec("v1", [1], index=1), vec("v2", [2], index=2)],
                             {"v1": {"index": 1}})
        self.assertEqual(case["status"], "mismatch")
        self.assertEqual(case["failures"][0]["kind"], "missing_actual")

    def test_extra_actual_vector_is_mismatch(self):
        c = make_contract(self.baseline)
        case = self.run_case(c, [vec("v1", [1], index=1)],
                             {"v1": {"index": 1}, "v9": {"index": 1}})
        self.assertEqual(case["status"], "mismatch")
        self.assertEqual(case["failures"][0]["kind"], "unexpected_actual")

    def test_empty_actual_is_not_run(self):
        c = make_contract(self.baseline)
        case = self.run_case(c, [vec("v1", [1], index=1)], {})
        self.assertNotEqual(case["status"], "pass")
        case2 = CMP.compare_case(c, make_capture(c, self.baseline, [vec("v1", [1], index=1)]),
                                 make_actual({}))
        self.assertIn(case2["status"], ("mismatch", "not_run"))
        self.assertFalse(CMP.qualifies(case2))

    def test_required_partial_axis_yields_partial(self):
        c = make_contract(self.baseline, axes={
            "query": {"coverage": "sampled", "required": True},
            "hidden_state": {"coverage": "unknown", "required": True},
            "optional": {"coverage": "partial", "required": False}})
        case = self.run_case(c, [vec("v1", [1], index=1)], {"v1": {"index": 1}})
        self.assertEqual(case["status"], "partial")
        self.assertEqual(case["coverage"]["uncovered_required_axes"], ["hidden_state"])
        self.assertEqual(CMP.exit_code(case), 2)
        self.assertFalse(CMP.qualifies(case))

    def test_pointer_difference_masked_only_under_explicit_rule(self):
        outs = [{"name": "ptr", "type": "int"}]
        plain = make_contract(self.baseline, outputs=outs)
        case = self.run_case(plain, [vec("v1", [1], ptr=0x1000)], {"v1": {"ptr": 0x2000}})
        self.assertEqual(case["status"], "mismatch")
        masked = make_contract(self.baseline, outputs=outs, normalizations=[
            {"name": "mask_pointer",
             "params": {"outputs": ["ptr"], "reason": "heap address differs per process"}}])
        case = self.run_case(masked, [vec("v1", [1], ptr=0x1000)], {"v1": {"ptr": 0x2000}})
        self.assertEqual(case["status"], "pass")
        case = self.run_case(masked, [vec("v1", [1], ptr=0x1000)], {"v1": {"ptr": 0}})
        self.assertEqual(case["status"], "mismatch")

    def test_index_offset_is_explicit(self):
        outs = [{"name": "index", "type": "int"}]
        plain = make_contract(self.baseline, outputs=outs)
        case = self.run_case(plain, [vec("v1", [1], index=4)], {"v1": {"index": 5}})
        self.assertEqual(case["status"], "mismatch")
        off = make_contract(self.baseline, outputs=outs, normalizations=[
            {"name": "index_offset", "params": {"output": "index", "offset": 1}}])
        case = self.run_case(off, [vec("v1", [1], index=4)], {"v1": {"index": 5}})
        self.assertEqual(case["status"], "pass")

    def test_constant_actual_fails_nonconstant_expected(self):
        c = make_contract(self.baseline)
        vs = [vec("v%d" % i, [i], index=i) for i in range(1, 6)]
        case = self.run_case(c, vs, {"v%d" % i: {"index": 3} for i in range(1, 6)})
        self.assertEqual(case["status"], "mismatch")
        self.assertEqual(case["coverage"]["vectors_mismatched"], 4)

    def test_repeated_calls_do_not_inflate_distinct_coverage(self):
        c = make_contract(self.baseline)
        vs = [vec("a", [1, 1, 1], index=1), vec("b", [1, 1, 1], index=1),
              vec("c", [1, 1, 1], index=1)]
        case = self.run_case(c, vs, {k: {"index": 1} for k in "abc"})
        self.assertEqual(case["status"], "pass")
        self.assertEqual(case["coverage"]["vectors_compared"], 3)
        self.assertEqual(case["coverage"]["distinct_inputs_compared"], 1)

    def test_actual_duplicate_id_and_missing_provenance_are_invalid(self):
        c = make_contract(self.baseline)
        cap = make_capture(c, self.baseline, [vec("v1", [1], index=1)])
        act = make_actual({"v1": {"index": 1}})
        act["vectors"].append({"id": "v1", "output": {"index": 1}})
        with self.assertRaisesRegex(C.ValidationError, "duplicates"):
            CMP.compare_case(c, cap, act)
        act = make_actual({"v1": {"index": 1}})
        del act["provenance"]["executable"]
        with self.assertRaisesRegex(C.ValidationError, "executable"):
            CMP.compare_case(c, cap, act)

    def test_invalid_input_contract_reports_rejected_expected(self):
        c = make_contract(self.baseline, success_outcome="rejected_expected")
        vs = [vec("bad", [1], rejected=True)]
        case = self.run_case(c, vs, {"bad": {"rejected": True}})
        self.assertEqual(case["status"], "rejected_expected")
        self.assertTrue(CMP.qualifies(case))
        case = self.run_case(c, vs, {"bad": {"index": 3}})
        self.assertEqual(case["status"], "mismatch")

    def test_optional_contract_never_qualifies(self):
        c = make_contract(self.baseline, required=False)
        case = self.run_case(c, [vec("v1", [1], index=1)], {"v1": {"index": 1}})
        self.assertEqual(case["status"], "pass")
        self.assertFalse(case["required"])
        self.assertFalse(CMP.qualifies(case))


class CompareCliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="eq cli ")
        self.d = Path(self.tmp.name)
        self.baseline = make_baseline()
        self.contract = make_contract(self.baseline)
        self.capture = make_capture(self.contract, self.baseline,
                                    [vec("v1", [1], index=1), vec("v2", [2], index=2)])
        for name, obj in (("contract", self.contract), ("baseline", self.baseline),
                          ("capture", self.capture)):
            (self.d / ("%s.json" % name)).write_text(json.dumps(obj), encoding="utf-8")

    def tearDown(self):
        self.tmp.cleanup()

    def cli(self, actual):
        (self.d / "actual.json").write_text(json.dumps(actual), encoding="utf-8")
        out = self.d / "out dir" / "case.json"
        with redirect_stdout(io.StringIO()):
            code = CMP.main(["--contract", str(self.d / "contract.json"),
                             "--baseline", str(self.d / "baseline.json"),
                             "--capture", str(self.d / "capture.json"),
                             "--actual", str(self.d / "actual.json"),
                             "--out", str(out)])
        return code, json.loads(out.read_text(encoding="utf-8"))

    def test_exit_codes(self):
        code, case = self.cli(make_actual({"v1": {"index": 1}, "v2": {"index": 2}}))
        self.assertEqual((code, case["status"]), (0, "pass"))
        code, case = self.cli(make_actual({"v1": {"index": 1}, "v2": {"index": 1}}))
        self.assertEqual((code, case["status"]), (1, "mismatch"))
        bad = make_actual({"v1": {"index": 1}})
        del bad["provenance"]
        code, case = self.cli(bad)
        self.assertEqual((code, case["status"]), (2, "unavailable"))

    def test_capture_for_other_baseline_exits_2(self):
        other = make_baseline(dll_sha="e" * 64)
        (self.d / "baseline.json").write_text(json.dumps(other), encoding="utf-8")
        code, case = self.cli(make_actual({"v1": {"index": 1}, "v2": {"index": 2}}))
        self.assertEqual((code, case["status"]), (2, "unavailable"))


if __name__ == "__main__":
    unittest.main()
