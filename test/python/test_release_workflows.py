"""The two workflows stay parseable and keep the release gate wired together.

These are structural checks of .github/workflows/{build,release}.yml: the
steps that make evidence exist (build.yml) and consume it (release.yml) must
call the tools with flags those tools accept, name the artifacts the helper
downloads, and put the Waived checks into the draft release notes.
"""

from __future__ import annotations

import re
import shlex
import sys
import unittest
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parents[2]
if str(REPO) not in sys.path:
    sys.path.insert(0, str(REPO))

from scripts.equivalence import ci_evidence as CI  # noqa: E402
from scripts.equivalence import release_record as RR  # noqa: E402

WORKFLOWS = REPO / ".github" / "workflows"


def load(name):
    return yaml.safe_load((WORKFLOWS / name).read_text(encoding="utf-8"))


def steps(job):
    return job["steps"]


def step(job, name_part):
    """The first step whose name starts with (or, failing that, contains) *name_part*."""
    for s in job["steps"]:
        if str(s.get("name", "")).lower().startswith(name_part.lower()):
            return s
    for s in job["steps"]:
        if name_part.lower() in str(s.get("name", "")).lower():
            return s
    raise KeyError(name_part)


def commands(run: str) -> list:
    """Shell commands of a run block, continuation lines joined."""
    text = re.sub(r"\\\r?\n\s*", " ", run)
    return [l.strip() for l in text.splitlines() if l.strip() and not l.strip().startswith("#")]


def ci_evidence_calls(run: str) -> list:
    out = []
    for cmd in commands(run):
        if "scripts.equivalence.ci_evidence" in cmd:
            out.append(shlex.split(cmd.split("||")[0]))
    return out


def accepted_flags(sub: str) -> set:
    parser = CI.build_parser()
    action = next(a for a in parser._actions if a.dest == "cmd")
    return {o for a in action.choices[sub]._actions for o in a.option_strings}


class Parse(unittest.TestCase):
    def test_both_workflows_parse(self):
        for name in ("build.yml", "release.yml"):
            wf = load(name)
            self.assertIsInstance(wf, dict, name)
            self.assertTrue(wf.get("jobs"), name)
            self.assertTrue(wf.get("on") or wf.get(True), name)


class Build(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.wf = load("build.yml")
        cls.jobs = cls.wf["jobs"]

    PLATFORM_JOBS = {"windows": "windows-x86", "linux": "linux-x86_64",
                     "macos": "macos-arm64"}

    def test_each_platform_job_writes_junit_and_emits_evidence_into_the_candidate(self):
        for job_name, platform in self.PLATFORM_JOBS.items():
            with self.subTest(job=job_name):
                job = self.jobs[job_name]
                names = [s.get("name", "") for s in steps(job)]
                test = step(job, "Test")
                self.assertIn("--output-junit", test["run"])
                self.assertIn("ctest-junit.xml", test["run"])
                emit = step(job, "Emit CI evidence")
                calls = ci_evidence_calls(emit["run"])
                self.assertEqual(len(calls), 1)
                call = calls[0]
                self.assertEqual(call[call.index("--platform") + 1], platform)
                self.assertEqual(call[call.index("--out") + 1],
                                 "candidate/evidence/ci-%s.json" % platform)
                self.assertEqual(call[call.index("--manifest") + 1],
                                 "candidate/candidate-%s.json" % platform)
                self.assertEqual(call[call.index("--source-identity") + 1],
                                 "source-identity.json")
                # order: test -> assemble candidate (writes the manifest) -> evidence
                # -> upload
                i_test = names.index(test["name"])
                i_asm = names.index("Assemble the candidate artifact")
                i_emit = names.index("Emit CI evidence")
                self.assertLess(i_test, i_asm)
                self.assertLess(i_asm, i_emit)
                upload = steps(job)[-1]
                self.assertEqual(upload["with"]["name"], "candidate-%s" % platform)
                self.assertEqual(upload["with"]["path"], "candidate/*")
                self.assertGreater(len(steps(job)) - 1, i_emit)   # upload comes after

    def test_ci_evidence_flags_are_ones_the_tool_accepts(self):
        for job_name in list(self.PLATFORM_JOBS) + ["sanitizers", "coverage"]:
            for s in steps(self.jobs[job_name]):
                for call in ci_evidence_calls(s.get("run", "")):
                    sub = call[call.index("scripts.equivalence.ci_evidence") + 1]
                    flags = {t for t in call if t.startswith("--")}
                    self.assertTrue(flags <= accepted_flags(sub),
                                    (job_name, sub, flags - accepted_flags(sub)))
                    plat = call[call.index("--platform") + 1]
                    self.assertIn(plat, CI.ALL)

    def test_replay_case_path_is_where_the_palette_replay_writes_it(self):
        cmake = (REPO / "test" / "cmake" / "equivalence.cmake").read_text(encoding="utf-8")
        self.assertIn('"${CMAKE_CURRENT_BINARY_DIR}/equivalence/palette-nearest-113c"', cmake)
        for job_name in self.PLATFORM_JOBS:
            call = ci_evidence_calls(step(self.jobs[job_name], "Emit CI evidence")["run"])[0]
            self.assertEqual(call[call.index("--replay-case") + 1],
                             "build/test/equivalence/palette-nearest-113c/case.json")

    def test_sanitizer_job_records_identity_and_uploads_its_evidence(self):
        job = self.jobs["sanitizers"]
        names = [s.get("name", "") for s in steps(job)]
        self.assertLess(names.index("Record source identity"),
                        names.index("Configure (Clang, Debug, ASan + UBSan)"))
        test = step(job, "Test")
        self.assertIn("--output-junit", test["run"])
        self.assertIn("-L portable", test["run"])
        self.assertEqual(test["id"], "test")
        emit = step(job, "Emit sanitizer evidence")
        self.assertIn("always()", emit["if"])
        call = ci_evidence_calls(emit["run"])[0]
        self.assertEqual(call[call.index("scripts.equivalence.ci_evidence") + 1], "sanitizer")
        up = steps(job)[-1]
        self.assertEqual(up["with"]["name"], "evidence-linux-sanitizers")
        self.assertEqual(up["with"]["path"], "evidence-out/*.json")
        self.assertEqual(call[call.index("--out") + 1], "evidence-out/ci-sanitizers.json")

    def test_coverage_job_emits_coverage_production_evidence(self):
        job = self.jobs["coverage"]
        names = [s.get("name", "") for s in steps(job)]
        self.assertLess(names.index("Record source identity"),
                        names.index("Configure (GCC, Debug, coverage)"))
        emit = step(job, "Emit coverage evidence")
        self.assertIn("||", emit["run"])          # unavailable evidence must not fail the job
        call = ci_evidence_calls(emit["run"])[0]
        self.assertEqual(call[call.index("--coverage") + 1], "coverage/coverage.json")
        self.assertEqual(call[call.index("--out") + 1], "evidence-out/ci-coverage.json")
        ups = [s for s in steps(job) if s.get("uses", "").startswith("actions/upload-artifact")]
        self.assertIn("evidence-linux-coverage", [u["with"]["name"] for u in ups])

    def test_evidence_artifact_names_are_the_ones_release_record_downloads(self):
        found = set()
        for job in self.jobs.values():
            for s in steps(job):
                n = (s.get("with") or {}).get("name", "")
                if n.startswith("evidence-"):
                    found.add(n)
        self.assertEqual(found, set(RR.EVIDENCE_ARTIFACTS))
        cands = {(s.get("with") or {}).get("name") for job in self.jobs.values()
                 for s in steps(job) if (s.get("with") or {}).get("name", "").startswith(
                     "candidate-")}
        self.assertEqual(cands, {"candidate-%s" % p for p in RR.PLATFORMS})

    def test_embedded_python_snippets_compile(self):
        for key in ("SOURCE_PY", "CANDIDATE_PY", "VERSION_PY"):
            compile(self.wf["env"][key], key, "exec")

    def test_candidate_executables_include_every_test_executable(self):
        self.assertIn('rglob("test_*")', self.wf["env"]["CANDIDATE_PY"])

    def test_ctest_output_is_not_truncated(self):
        # junit output feeds the Unity/unittest line parsing; CTest cuts passed
        # test output at 1 KiB unless told otherwise.
        cmake = (REPO / "test" / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE", cmake)
        self.assertIn("CTestCustom.cmake", cmake)


class Release(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.wf = load("release.yml")
        cls.verify = cls.wf["jobs"]["verify"]
        cls.publish = cls.wf["jobs"]["publish"]

    def test_platforms_match_the_helper(self):
        self.assertEqual(self.wf["env"]["PLATFORMS"].split(), list(RR.PLATFORMS))

    def test_verification_is_the_shared_helper_not_inline_logic(self):
        runs = "\n".join(s.get("run", "") for s in steps(self.verify))
        for sub in ("find-record", "check-run", "verify-release"):
            self.assertIn("scripts.equivalence.release_record %s" % sub, runs)
        self.assertNotIn("verify_release_record", runs)

    def test_helper_flags_are_ones_the_helper_accepts(self):
        for s in steps(self.verify):
            for cmd in commands(s.get("run", "")):
                m = re.search(r"release_record (find-record|check-run|verify-release)\b(.*)",
                              cmd)
                if not m:
                    continue
                flags = set(re.findall(r"(--[a-z-]+)", m.group(2)))
                parser = RR._sub_parser(m.group(1))
                known = {o for a in parser._actions for o in a.option_strings}
                self.assertTrue(flags <= known, (m.group(1), flags - known))

    def test_notes_with_waived_checks_reach_the_draft_release(self):
        v = step(self.verify, "Verify identities")
        self.assertIn("--notes-out", v["run"])
        notes_path = re.search(r"--notes-out\s+(\S+)", v["run"]).group(1).strip('"')
        up = [s for s in steps(self.verify) if (s.get("with") or {}).get("name") ==
              "release-notes"]
        self.assertEqual(len(up), 1)
        self.assertEqual(up[0]["with"]["path"], notes_path)
        dl = [s for s in steps(self.publish) if (s.get("with") or {}).get("name") ==
              "release-notes"]
        self.assertEqual(len(dl), 1)
        pub = [s for s in steps(self.publish)
               if str(s.get("uses", "")).startswith("softprops/action-gh-release")][0]
        self.assertEqual(pub["with"]["body_path"],
                         "%s/%s" % (dl[0]["with"]["path"], Path(notes_path).name))
        self.assertTrue(pub["with"]["draft"])
        self.assertFalse(pub["with"]["generate_release_notes"])
        self.assertEqual(pub["with"]["files"], "dist/*")       # notes are not an asset

    def test_candidate_artifacts_are_downloaded_without_rebuilding(self):
        runs = "\n".join(s.get("run", "") for s in steps(self.verify))
        self.assertIn('gh run download "$RUN_ID"', runs)
        self.assertNotIn("cmake --build", runs)

    def test_allowed_artifact_contents_are_enforced_by_the_helper(self):
        # evidence/ is allowed in the candidate artifact; the helper checks it
        text = (REPO / "scripts" / "equivalence" / "release_record.py").read_text(
            encoding="utf-8")
        self.assertIn('"evidence"}', text)
        self.assertIn("is not part of the reviewed record", text)


if __name__ == "__main__":
    unittest.main()
