"""Tests for scripts/equivalence/replay.py.

Uses the tracked palette contract/baseline/inputs with a SYNTHETIC capture
(built in a temporary directory, never written to the repository) and a fake
Python probe. Never needs a game installation, the fleet, or mpyq.

    python -m unittest -v test.python.test_equivalence_replay
"""

import copy
import json
import os
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from scripts.equivalence import contracts as C  # noqa: E402
from scripts.equivalence import replay as R  # noqa: E402

EQ = REPO_ROOT / "test" / "equivalence"
CONTRACT = EQ / "contracts" / "palette-nearest-113c.json"
BASELINE = EQ / "fixtures" / "palette-nearest-baseline.json"
INPUTS = EQ / "fixtures" / "palette-nearest-inputs.json"

FAKE_PROBE = textwrap.dedent('''
    import json, sys
    MODE = %r
    a = dict(zip(sys.argv[1::2], sys.argv[2::2]))
    pal = open(a["--palette"], "rb").read()
    q = open(a["--queries"], "rb").read()
    if len(pal) != 1024 or len(q) %% 3:
        sys.exit(3)
    out = []
    for k in range(0, len(q), 3):
        r, g, b = q[k], q[k + 1], q[k + 2]
        if MODE == "swap":
            r, b = b, r
        best, bi = None, 0
        for i in range(1, 256):
            d = sum((pal[4 * i + c] - v) ** 2 for c, v in enumerate((r, g, b)))
            if best is None or d < best:
                best, bi = d, i
        out.append(3 if MODE == "constant" else bi)
    if MODE == "short":
        out = out[:-1]
    open(a["--out"], "w").write(json.dumps({"indices": out, "palette_unchanged": True}))
    if MODE == "fail":
        sys.exit(1)
''')


def synthetic_capture(contract, baseline, inputs, perturb=None):
    """A capture shaped like the real one, answers from an authoring model."""
    pals = {k: bytes.fromhex(v) for k, v in inputs["palettes"].items()}
    vectors = []
    for v in inputs["vectors"]:
        pal, rgb = pals[v["input"]["palette"]], v["input"]["rgb"]
        best, bi = None, 0
        for i in range(1, 256):
            d = sum((pal[4 * i + c] - rgb[c]) ** 2 for c in range(3))
            if best is None or d < best:
                best, bi = d, i
        vectors.append({"id": v["id"], "input": v["input"],
                        "output": {"index": bi - 1, "writes": []}})
    if perturb:
        perturb(vectors)
    adapter = {"name": "synthetic-test", "version": "0",
               "abi_recipe_digest": contract["reference"]["abi_recipe_digest"]}
    binary = C.baseline_binary(baseline, "D2CMP.dll")
    return C.seal({
        "schema": C.CAPTURE_SCHEMA, "contract_id": contract["id"],
        "contract_digest": contract["digest"], "baseline_digest": baseline["digest"],
        "adapter": adapter, "adapter_digest": C.payload_sha256(adapter),
        "original": {"module": "D2CMP.dll", "module_sha256": binary["sha256"],
                     "rva": "0x9d30",
                     "function_bytes_sha256": contract["reference"]["function_bytes_sha256"]},
        "captured_at": "2026-10-22T00:00:00Z",
        "evidence": [{"kind": "test", "ref": "synthetic, not original-game evidence"}],
        "vectors": vectors,
        "input_sha256": C.payload_sha256(C.vector_inputs_payload(vectors)),
        "output_sha256": C.payload_sha256(C.vector_outputs_payload(vectors)),
        "limitations": ["synthetic test capture"],
    })


class ReplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = C.load_contract(CONTRACT)
        cls.baseline = C.load_baseline(BASELINE)
        cls.inputs = json.loads(INPUTS.read_text(encoding="utf-8"))

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="eq replay with spaces ")
        self.d = Path(self.tmp.name)
        self.capture_path = self.d / "capture file.json"
        self.write_capture(synthetic_capture(self.contract, self.baseline, self.inputs))

    def tearDown(self):
        self.tmp.cleanup()

    def write_capture(self, cap):
        self.capture_path.write_text(json.dumps(cap), encoding="utf-8")

    def probe(self, mode="good"):
        p = self.d / ("fake probe %s.py" % mode)
        p.write_text(FAKE_PROBE % mode, encoding="utf-8")
        return p

    def run_replay(self, mode="good", probe=None, contract=CONTRACT):
        return R.replay(contract, BASELINE, self.capture_path,
                        probe or self.probe(mode), self.d / "out dir",
                        probe_prefix=[sys.executable])

    def test_good_candidate_passes_with_full_distinct_coverage(self):
        case = self.run_replay()
        self.assertEqual(case["status"], "pass", case["failures"])
        self.assertEqual(case["coverage"]["distinct_inputs_compared"],
                         len(self.inputs["vectors"]))
        self.assertTrue((self.d / "out dir" / "case.json").is_file())
        self.assertIn("sha256", case["provenance"]["candidate"]["executable"])
        self.assertIn("source_state", case["provenance"]["candidate"])

    def test_constant_and_channel_swap_controls_diverge(self):
        for mode in ("constant", "swap"):
            case = self.run_replay(mode)
            self.assertEqual(case["status"], "mismatch", mode)
            self.assertGreater(case["coverage"]["vectors_mismatched"], 100, mode)

    def test_missing_probe_is_unavailable(self):
        case = self.run_replay(probe=self.d / "no such probe.exe")
        self.assertEqual(case["status"], "unavailable")

    def test_nonzero_probe_with_output_present_is_unavailable(self):
        case = self.run_replay("fail")
        self.assertEqual(case["status"], "unavailable")
        self.assertIn("exited 1", case["failures"][0]["detail"])

    def test_wrong_output_cardinality_is_unavailable(self):
        case = self.run_replay("short")
        self.assertEqual(case["status"], "unavailable")
        self.assertIn("indices", case["failures"][0]["detail"])

    def test_reordered_capture_ids_still_map_by_id(self):
        cap = synthetic_capture(self.contract, self.baseline, self.inputs,
                                perturb=lambda vs: vs.reverse())
        self.write_capture(cap)
        self.assertEqual(self.run_replay()["status"], "pass")

    def test_changed_capture_payload_is_refused(self):
        cap = json.loads(self.capture_path.read_text(encoding="utf-8"))
        cap["vectors"][0]["output"]["index"] += 1
        self.write_capture(cap)
        case = self.run_replay()
        self.assertEqual(case["status"], "unavailable")

    def test_changed_input_fixture_is_refused(self):
        root = self.d / "root"
        fx = root / "test" / "equivalence" / "fixtures"
        fx.mkdir(parents=True)
        inp = copy.deepcopy(self.inputs)
        inp["vectors"][0]["input"]["rgb"] = [1, 2, 3]
        (fx / INPUTS.name).write_text(json.dumps(inp), encoding="utf-8")
        case = R.replay(CONTRACT, BASELINE, self.capture_path, self.probe(),
                        self.d / "out dir", probe_prefix=[sys.executable], inputs_root=root)
        self.assertEqual(case["status"], "unavailable")
        self.assertIn("digest", case["failures"][0]["detail"])

    def test_truncated_capture_is_refused(self):
        cap = synthetic_capture(self.contract, self.baseline, self.inputs,
                                perturb=lambda vs: vs.pop())
        self.write_capture(cap)
        case = self.run_replay()
        self.assertEqual(case["status"], "unavailable")

    def test_replay_does_not_import_fleet_or_mpq(self):
        code = textwrap.dedent('''
            import sys
            sys.modules["mpyq"] = None
            sys.path.insert(0, %r)
            import builtins
            real = builtins.__import__
            def guard(name, *a, **k):
                if name in ("mpyq",) or name.endswith("fleet_adapter") or name.endswith("baseline"):
                    raise ImportError("forbidden import " + name)
                return real(name, *a, **k)
            builtins.__import__ = guard
            from scripts.equivalence import replay as R
            case = R.replay(%r, %r, %r, %r, %r, probe_prefix=[sys.executable])
            assert "scripts.equivalence.fleet_adapter" not in sys.modules
            print(case["status"])
        ''') % (str(REPO_ROOT), str(CONTRACT), str(BASELINE), str(self.capture_path),
                str(self.probe()), str(self.d / "iso out"))
        r = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True,
                           cwd=str(self.d))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.strip(), "pass")


if __name__ == "__main__":
    unittest.main()
