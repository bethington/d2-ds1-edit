"""release_record: build the reviewed qualification record from a CI run, and
run release.yml's verification locally. Fakes only: a fake `gh`, a tiny source
tree, authored package files and generated evidence. No network, no build.
"""

from __future__ import annotations

import copy
import hashlib
import io
import json
import shutil
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
if str(REPO) not in sys.path:
    sys.path.insert(0, str(REPO))

from scripts.equivalence import contracts as C  # noqa: E402
from scripts.equivalence import qualify as Q  # noqa: E402
from scripts.equivalence import release_record as RR  # noqa: E402

PROFILE = REPO / "test" / "equivalence" / "profiles" / "release-core.json"
PLATS = ("windows-x86", "linux-x86_64", "macos-arm64")
PKG_NAMES = {"windows-x86": "DS1Edit-9.9.9-win32.zip",
             "linux-x86_64": "DS1Edit-9.9.9-linux-x86_64.tar.gz",
             "macos-arm64": "DS1Edit-9.9.9-macos-arm64.tar.gz"}
COMMIT = "a" * 40
REPOSITORY = "owner/d2-ds1-edit"


def H(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def write_json(path, obj):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=1, sort_keys=True) + "\n", encoding="utf-8")


class FakeGh:
    """The few `gh` operations release_record uses, served from a directory."""

    def __init__(self, base: Path, run: dict, repository=REPOSITORY):
        self.base, self.run, self.repo = Path(base), run, repository
        self.downloads = []

    def repository(self):
        return self.repo

    def run_json(self, run_id, repo):
        self.last_run = (run_id, repo)
        return copy.deepcopy(self.run)

    def download(self, run_id, name, dest, repo):
        self.downloads.append((str(run_id), name, repo))
        src = self.base / name
        if not src.is_dir():
            return False
        shutil.copytree(src, dest, dirs_exist_ok=True)
        return True


class World:
    """A source tree, three candidate artifacts and a complete evidence set."""

    def __init__(self, tc: unittest.TestCase):
        self.tmp = Path(tempfile.mkdtemp(prefix="ds1edit rr "))
        tc.addCleanup(shutil.rmtree, self.tmp, True)
        self.root = self.tmp / "checkout"
        for rel, data in {"CMakeLists.txt": "project(x)\n", "CMakePresets.json": "{}",
                          "vcpkg.json": "{}", "LICENSE": "MIT", "NOTICE": "n",
                          "README.md": "r", "Ds1edit.ini.sample": "s", "allegro5.cfg": "c",
                          "src/main.c": "int main(void){return 0;}\n",
                          "test/t.c": "\n", "scripts/s.py": "\n"}.items():
            p = self.root / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(data, encoding="utf-8")
        self.profile = Q.load_profile(PROFILE)
        self.digest = Q.source_state_digest(self.root)
        self.artifacts = self.tmp / "artifacts"
        self.out = self.root / "release" / "qualification"
        self.work = self.tmp / "work"
        self.entries = Q.expand_profile(self.profile)
        self.reference = {"baseline_digest": H("baseline"), "contracts": {}, "adapters": {}}
        for e in self.entries:
            if "contract" in e["reference"]:
                self.reference["contracts"][e["id"]] = H("c:" + e["id"])
            if "adapter" in e["reference"]:
                self.reference["adapters"][e["id"]] = H("a:" + e["id"])
        self.reference_file = self.tmp / "reference.json"
        write_json(self.reference_file, self.reference)
        self.run_info = {"id": 1001, "path": ".github/workflows/build.yml",
                         "conclusion": "success", "head_sha": COMMIT,
                         "repository": {"full_name": REPOSITORY}}
        self._build_artifacts()
        self.gh = FakeGh(self.artifacts, self.run_info)

    # -- artifacts ----------------------------------------------------------
    def _build_artifacts(self):
        source = {"commit": COMMIT, "source_state_digest": self.digest, "dirty": False,
                  "dirty_sha256": None}
        manifests = []
        for p in PLATS:
            d = self.artifacts / ("candidate-%s" % p)
            d.mkdir(parents=True)
            pkg = d / PKG_NAMES[p]
            pkg.write_bytes(("package " + p).encode())
            exe = self.tmp / ("ds1edit-" + p)
            exe.write_bytes(("exe " + p).encode())
            probe = self.tmp / ("probe-" + p)
            probe.write_bytes(("probe " + p).encode())
            m = Q.platform_manifest(p, pkg, {"ds1edit": exe, "test_probe": probe}, "1001",
                                    source, {"config": "Release", "compiler": "unit"})
            write_json(d / ("candidate-%s.json" % p), m)
            manifests.append(m)
        self.candidate = Q.combine_candidate(manifests, REPOSITORY, self.profile["digest"],
                                             self.reference)
        per = {p: [] for p in PLATS}
        side = []
        for e in self.entries:
            case = self.passing_case(e)
            if e["lane"] == "sanitizer":
                side.append(case)
            else:
                per[e["platform"]].append(case)
        for p in PLATS:
            d = self.artifacts / ("candidate-%s" % p)
            pkgs = [c for c in per[p] if c["lane"] == "native-package"]
            rest = [c for c in per[p] if c["lane"] != "native-package"]
            write_json(d / "package-inspection.json",
                       {"schema": Q.PACKAGE_INSPECTION_SCHEMA, "cases": pkgs})
            write_json(d / "evidence" / ("ci-%s.json" % p),
                       {"schema": Q.CASE_SET_SCHEMA, "cases": rest})
        write_json(self.artifacts / "evidence-linux-sanitizers" / "ci.json",
                   {"schema": Q.CASE_SET_SCHEMA, "cases": side})

    def passing_case(self, e):
        plat = self.candidate["platforms"][e["platform"]]
        src = self.candidate["source"]
        cov = {"distinct_inputs_compared": 3, "uncovered_required_axes": []}
        if e["coverage"].get("complete_accounting"):
            cov.update(inputs_enumerated=2, inputs_accounted=2, unclassified=0)
        if e["coverage"].get("features"):
            cov["features_covered"] = list(e["coverage"]["features"])
        if e["kind"] == "negative_control":
            cov["control_detected"] = True
        if e["kind"] == "performance":
            cov["comparison"] = "baseline"
        ref = self.candidate["reference"]
        case = {
            "schema": C.CASE_SCHEMA, "id": e["id"], "lane": e["lane"],
            "platform": e["platform"], "required": True, "status": e["success_outcome"],
            "success_outcome": e["success_outcome"],
            "contract_digest": ref["contracts"].get(e["id"]),
            "baseline_digest": ref["baseline_digest"] if "baseline" in e["reference"]
            else None, "input_sha256": H(e["id"]), "expected_sha256": H("x"),
            "actual_sha256": H("x"), "coverage": cov, "failures": [], "limitations": [],
            "provenance": {"candidate": {
                "source_state": {"source_state_digest": src["source_state_digest"],
                                 "commit": src["commit"], "dirty": False,
                                 "dirty_sha256": None},
                "executable": {"sha256": plat["executables"]["ds1edit"]},
                "build": {}, "packages": []},
                "reference": {"adapter_digest": ref["adapters"].get(e["id"])}},
        }
        if "package" in e["binding"]:
            case["provenance"]["candidate"]["packages"] = [dict(plat["package"])]
        if e["kind"] == "package":
            case["input_sha256"] = plat["package"]["sha256"]
            case["provenance"] = {"tool": "package_check/1",
                                  "archive_sha256": plat["package"]["sha256"]}
        return case

    # -- editing the fake run ----------------------------------------------
    def evidence_files(self, platform=None):
        out = []
        for p in ([platform] if platform else PLATS):
            d = self.artifacts / ("candidate-%s" % p)
            out += [d / "package-inspection.json"] + sorted((d / "evidence").glob("*.json"))
        out.append(self.artifacts / "evidence-linux-sanitizers" / "ci.json")
        return out

    def drop_case(self, cid, platform):
        for f in self.evidence_files():
            doc = json.loads(f.read_text(encoding="utf-8"))
            keep = [c for c in doc["cases"] if not (c["id"] == cid and c["platform"] == platform)]
            if len(keep) != len(doc["cases"]):
                doc["cases"] = keep
                write_json(f, doc)
                return
        raise KeyError((cid, platform))

    def edit_case(self, cid, platform, fn):
        for f in self.evidence_files():
            doc = json.loads(f.read_text(encoding="utf-8"))
            for c in doc["cases"]:
                if c["id"] == cid and c["platform"] == platform:
                    fn(c)
                    write_json(f, doc)
                    return
        raise KeyError((cid, platform))

    def waivers_file(self, *items, name="waivers.json"):
        path = self.tmp / name
        write_json(path, {"schema": Q.REVIEW_SCHEMA, "dispositions": list(items)})
        return path

    # -- running the tool ---------------------------------------------------
    def args(self, *extra, version="9.9.9", dry=False):
        a = ["--run-id", "1001", "--version", version, "--out", str(self.out),
             "--work-dir", str(self.work), "--reference", str(self.reference_file),
             "--profile", str(PROFILE), "--root", str(self.root)]
        a += list(extra)
        if dry:
            a.append("--dry-run-release")
        return a

    def run(self, *extra, **kw):
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            rc = RR.main(self.args(*extra, **kw), gh=self.gh)
        self.stdout, self.stderr = out.getvalue(), err.getvalue()
        return rc

    def record_path(self):
        return self.out / ("%s.json" % self.digest)

    def record(self):
        return json.loads(self.record_path().read_text(encoding="utf-8"))


def waiver(**kw):
    w = {"id": "corpus.ds1.read-all", "kind": "waiver", "platform": "windows-x86",
         "reason": "the vanilla corpus is not available on this machine",
         "reviewer": "maintainer", "expires_version": "9.9.9"}
    w.update(kw)
    return w


# ---------------------------------------------------------------------------
# Producing the record
# ---------------------------------------------------------------------------

class Producing(unittest.TestCase):
    def setUp(self):
        self.w = World(self)

    def test_complete_evidence_writes_the_record_named_by_the_source_digest(self):
        self.assertEqual(self.w.run(), 0, self.w.stdout + self.w.stderr)
        rec = self.w.record()
        self.assertTrue(rec["ready"])
        self.assertEqual(rec["schema"], Q.QUALIFICATION_SCHEMA)
        self.assertEqual(rec["source_state_digest"], self.w.digest)
        self.assertEqual(rec["release_version"], "9.9.9")
        self.assertEqual(rec["candidate_run_id"], "1001")
        self.assertEqual(rec["candidate"]["repository"], REPOSITORY)
        self.assertEqual(rec["waived"], [])
        self.assertEqual(rec["digest"], C.canonical_digest(rec))
        self.assertEqual(sorted(p.name for p in self.w.out.iterdir()),
                         ["%s.json" % self.w.digest])
        self.assertFalse(self.w.record_path().read_bytes().count(b"\r"))

    def test_downloads_the_three_candidates_and_the_evidence_artifacts(self):
        self.w.run()
        names = sorted(n for _, n, _ in self.w.gh.downloads)
        self.assertEqual(names, sorted(["candidate-%s" % p for p in PLATS]
                                       + list(RR.EVIDENCE_ARTIFACTS)))
        self.assertTrue(all(run == "1001" and repo == REPOSITORY
                            for run, _, repo in self.w.gh.downloads))

    def test_the_record_verifies_exactly_as_release_yml_would(self):
        self.w.run()
        arts = {p: self.w.work / "candidate" / p / PKG_NAMES[p] for p in PLATS}
        self.assertEqual(Q.verify_release_record(self.w.record(), self.w.profile,
                                                 self.w.root, arts, version="9.9.9"), [])

    def test_waivers_make_a_record_whose_waived_checks_are_listed(self):
        self.w.drop_case("corpus.ds1.read-all", "windows-x86")
        self.assertEqual(self.w.run(), 1)
        self.assertFalse(self.w.record_path().exists())
        wf = self.w.waivers_file(waiver())
        self.assertEqual(self.w.run("--waivers", str(wf)), 0, self.w.stdout + self.w.stderr)
        rec = self.w.record()
        self.assertTrue(rec["ready"])
        self.assertEqual([x["id"] for x in rec["waived"]], ["corpus.ds1.read-all"])
        self.assertEqual(rec["waived"][0]["waiver"]["reason"], waiver()["reason"])
        self.assertIn("waived: corpus.ds1.read-all@corpus/windows-x86", self.w.stdout)

    def test_a_non_waivable_missing_entry_cannot_be_waived_into_a_record(self):
        self.w.drop_case("package.linux-x86_64.archive", "linux-x86_64")
        wf = self.w.waivers_file(waiver(id="package.linux-x86_64.archive",
                                        platform="linux-x86_64"))
        self.assertEqual(self.w.run("--waivers", str(wf)), 2)
        self.assertIn("cannot be waived", self.w.stderr)
        self.assertFalse(self.w.record_path().exists())

    def test_a_waiver_for_another_version_is_invalid(self):
        self.w.drop_case("corpus.ds1.read-all", "windows-x86")
        wf = self.w.waivers_file(waiver(expires_version="9.9.8"))
        self.assertEqual(self.w.run("--waivers", str(wf)), 2)
        self.assertIn("9.9.8", self.w.stderr)
        self.assertFalse(self.w.record_path().exists())

    def test_a_waiver_never_hides_a_mismatch(self):
        self.w.edit_case("corpus.ds1.read-all", "windows-x86",
                         lambda c: c.update(status="mismatch"))
        wf = self.w.waivers_file(waiver())
        self.assertEqual(self.w.run("--waivers", str(wf)), 1)
        self.assertIn("waiver not applied: corpus.ds1.read-all@corpus/windows-x86",
                      self.w.stdout)
        self.assertFalse(self.w.record_path().exists())

    def test_waivers_accept_a_bare_list_of_dispositions(self):
        self.w.drop_case("corpus.ds1.read-all", "windows-x86")
        path = self.w.tmp / "bare.json"
        write_json(path, [waiver()])
        self.assertEqual(self.w.run("--waivers", str(path)), 0, self.w.stderr)

    def test_not_ready_without_a_waiver_writes_no_record(self):
        self.w.drop_case("perf.first-open", "linux-x86_64")
        self.assertEqual(self.w.run(), 1)
        self.assertFalse(self.w.out.exists() and any(self.w.out.iterdir()))
        self.assertIn("perf.first-open", self.w.stdout)

    def test_local_lane_evidence_dirs_are_included(self):
        self.w.drop_case("corpus.ds1.read-all", "windows-x86")
        case = World.passing_case(self.w, next(
            e for e in self.w.entries if e["id"] == "corpus.ds1.read-all"))
        local = self.w.tmp / "local"
        write_json(local / "corpus.json", case)
        self.assertEqual(self.w.run("--evidence-dir", str(local)), 0, self.w.stderr)
        self.assertTrue(self.w.record()["ready"])

    def test_the_checkout_must_be_the_candidates_source_state(self):
        (self.w.root / "src" / "main.c").write_text("int main(void){return 1;}\n",
                                                    encoding="utf-8")
        self.assertEqual(self.w.run(), 2)
        self.assertIn("not the candidate's source state", self.w.stderr)
        self.assertFalse(self.w.record_path().exists())

    def test_a_missing_candidate_artifact_is_an_error(self):
        shutil.rmtree(self.w.artifacts / "candidate-linux-x86_64")
        self.assertEqual(self.w.run(), 2)
        self.assertIn("candidate-linux-x86_64", self.w.stderr)

    def test_a_missing_evidence_artifact_leaves_its_entries_missing(self):
        shutil.rmtree(self.w.artifacts / "evidence-linux-sanitizers")
        self.assertEqual(self.w.run(), 1)
        self.assertIn("sanitizer.core", self.w.stdout)
        self.assertIn("warning", self.w.stderr)

    def test_platform_manifests_that_disagree_on_source_are_refused(self):
        f = self.w.artifacts / "candidate-macos-arm64" / "candidate-macos-arm64.json"
        m = json.loads(f.read_text(encoding="utf-8"))
        m["source"]["source_state_digest"] = H("a different tree")
        write_json(f, C.seal({k: v for k, v in m.items() if k != "digest"}))
        self.assertEqual(self.w.run(), 2)
        self.assertIn("disagree on source", self.w.stderr)

    def test_bad_arguments_exit_two(self):
        for bad in (["--version", "2.2"], ["--version", "v2.2.1"], ["--run-id", "x1"]):
            out, err = io.StringIO(), io.StringIO()
            args = self.w.args()
            i = args.index(bad[0])
            args[i + 1] = bad[1]
            with redirect_stdout(out), redirect_stderr(err):
                self.assertEqual(RR.main(args, gh=self.w.gh), 2, bad)

    def test_evidence_that_is_invalid_exits_two(self):
        write_json(self.w.artifacts / "evidence-linux-sanitizers" / "junk.json",
                   {"schema": "not/a-schema"})
        self.assertEqual(self.w.run(), 2)

    def test_a_run_that_is_not_a_successful_build_is_refused_before_any_record(self):
        for k, v in (("conclusion", "failure"), ("head_sha", "b" * 40),
                     ("path", ".github/workflows/other.yml")):
            with self.subTest(k=k):
                good = self.w.run_info[k]
                self.w.run_info[k] = v
                self.assertEqual(self.w.run(), 2, k)
                self.assertIn("candidate run 1001", self.w.stderr)
                self.assertFalse(self.w.record_path().exists())
                self.w.run_info[k] = good

    def test_repository_defaults_to_gh(self):
        self.w.gh.repo = "other/repo"
        self.w.run_info["repository"] = {"full_name": "other/repo"}
        self.w.gh.run = self.w.run_info
        self.w.run()
        self.assertEqual(self.w.gh.last_run, ("1001", "other/repo"))


class Reference(unittest.TestCase):
    def test_build_reference_reads_contracts_baseline_and_captured_adapters(self):
        ref = RR.build_reference(REPO)
        self.assertEqual(ref["contracts"]["palette-nearest-113c"],
                         json.loads((REPO / "test/equivalence/contracts/"
                                     "palette-nearest-113c.json").read_text("utf-8"))["digest"])
        self.assertRegex(ref["baseline_digest"], r"^[0-9a-f]{64}$")
        cap = json.loads((REPO / "test/equivalence/fixtures/"
                          "palette-nearest-capture.json").read_text("utf-8"))
        self.assertEqual(ref["adapters"]["palette-nearest-113c"], cap["adapter_digest"])

    def test_build_reference_without_contracts_is_empty_not_invented(self):
        d = Path(tempfile.mkdtemp(prefix="ds1edit rr ref "))
        self.addCleanup(shutil.rmtree, d, True)
        ref = RR.build_reference(d)
        self.assertEqual((ref["contracts"], ref["adapters"], ref["baseline_digest"]),
                         ({}, {}, None))


# ---------------------------------------------------------------------------
# release.yml's verification, locally
# ---------------------------------------------------------------------------

class DryRun(unittest.TestCase):
    def setUp(self):
        self.w = World(self)
        self.assertEqual(self.w.run(), 0, self.w.stdout + self.w.stderr)

    def dry(self, version="9.9.9", *extra, tag=None):
        a = list(extra) + (["--tag", tag] if tag else [])
        return self.w.run(*a, version=version, dry=True)

    def test_a_good_record_and_artifacts_verify(self):
        self.assertEqual(self.dry(), 0, self.w.stdout + self.w.stderr)
        self.assertIn("verified candidate run 1001", self.w.stdout)

    def test_dry_run_writes_the_release_notes_with_the_waived_checks(self):
        self.w.drop_case("corpus.ds1.read-all", "windows-x86")
        wf = self.w.waivers_file(waiver())
        self.assertEqual(self.w.run("--waivers", str(wf)), 0)
        self.assertEqual(self.dry(), 0, self.w.stdout + self.w.stderr)
        notes = (self.w.work / "RELEASE_NOTES.md").read_text(encoding="utf-8")
        self.assertIn("## Waived checks", notes)
        self.assertIn("`corpus.ds1.read-all`", notes)
        self.assertIn(waiver()["reason"], notes)

    def test_a_tag_that_is_not_the_recorded_version_fails(self):
        self.assertEqual(self.dry(tag="v9.9.8"), 1)
        self.assertIn("9.9.8", self.w.stderr + self.w.stdout)

    def test_a_run_that_did_not_succeed_or_is_for_another_commit_fails(self):
        for k, v in (("conclusion", "failure"), ("head_sha", "b" * 40),
                     ("path", ".github/workflows/other.yml")):
            with self.subTest(k=k):
                self.w.run_info[k] = v
                self.w.gh.run = self.w.run_info
                self.assertEqual(self.dry(), 1)
                self.w.run_info[k] = {"conclusion": "success", "head_sha": COMMIT,
                                      "path": ".github/workflows/build.yml"}[k]
        self.w.run_info["repository"] = {"full_name": "someone/else"}
        self.assertEqual(self.dry(), 1)

    def test_extra_files_in_a_candidate_artifact_fail(self):
        (self.w.artifacts / "candidate-windows-x86" / "notes.txt").write_text("x")
        self.assertEqual(self.dry(), 1)
        self.assertIn("unexpected files", self.w.stderr + self.w.stdout)

    def test_a_tampered_package_fails(self):
        (self.w.artifacts / "candidate-linux-x86_64" / PKG_NAMES["linux-x86_64"]).write_bytes(b"x")
        self.assertEqual(self.dry(), 1)

    def test_evidence_changed_after_the_record_was_made_fails(self):
        f = self.w.artifacts / "candidate-windows-x86" / "evidence" / "ci-windows-x86.json"
        doc = json.loads(f.read_text(encoding="utf-8"))
        doc["cases"][0]["limitations"].append("edited after review")
        write_json(f, doc)
        self.assertEqual(self.dry(), 1)
        self.assertIn("not part of the reviewed record", self.w.stderr + self.w.stdout)

    def test_a_manifest_that_disagrees_with_the_record_fails(self):
        f = self.w.artifacts / "candidate-windows-x86" / "candidate-windows-x86.json"
        m = json.loads(f.read_text(encoding="utf-8"))
        m["candidate_run_id"] = "999"
        write_json(f, C.seal({k: v for k, v in m.items() if k != "digest"}))
        self.assertEqual(self.dry(), 1)

    def test_a_missing_record_fails(self):
        self.w.record_path().unlink()
        self.assertEqual(self.dry(), 1)
        self.assertIn("No reviewed qualification record", self.w.stderr + self.w.stdout)

    def test_a_record_that_is_not_ready_fails(self):
        rec = self.w.record()
        rec["ready"] = False
        write_json(self.w.record_path(), C.seal({k: v for k, v in rec.items() if k != "digest"}))
        self.assertEqual(self.dry(), 1)


class ReleaseWorkflowHelpers(unittest.TestCase):
    """The pieces release.yml calls directly."""

    def setUp(self):
        self.w = World(self)
        self.assertEqual(self.w.run(), 0)
        self.record = self.w.record()
        self.cdir = self.w.work / "candidate"

    def test_find_record(self):
        digest, path = RR.find_record(self.w.root, self.w.out)
        self.assertEqual((digest, path), (self.w.digest, self.w.record_path()))
        self.w.record_path().unlink()
        self.assertEqual(RR.find_record(self.w.root, self.w.out), (self.w.digest, None))

    def test_verify_candidate_dir_accepts_the_downloaded_artifacts(self):
        self.assertEqual(RR.verify_candidate_dir(self.record, self.w.profile, self.w.root,
                                                 self.cdir, "v9.9.9"), [])

    def test_verify_candidate_dir_checks_the_tag_against_package_names(self):
        problems = RR.verify_candidate_dir(self.record, self.w.profile, self.w.root,
                                           self.cdir, "v9.9.10")
        self.assertTrue([p for p in problems if "is not version" in p], problems)
        problems = RR.verify_candidate_dir(self.record, self.w.profile, self.w.root,
                                           self.cdir, "9.9.9")
        self.assertTrue(problems)

    def test_check_run(self):
        run = {"path": ".github/workflows/build.yml", "conclusion": "success",
               "head_sha": COMMIT, "repository": {"full_name": REPOSITORY}}
        self.assertEqual(RR.check_run(self.record, run, REPOSITORY), [])
        self.assertTrue(RR.check_run(self.record, dict(run, conclusion=None), REPOSITORY))
        self.assertTrue(RR.check_run(self.record, run, "other/repo"))
        self.record["candidate"]["candidate_run_id"] = "12x"
        self.assertTrue(RR.check_run(self.record, run, REPOSITORY))

    def test_release_notes_have_a_waived_checks_section(self):
        notes = RR.release_notes(self.record, "v9.9.9")
        self.assertIn("## Waived checks", notes)
        self.assertIn("None", notes.split("## Waived checks", 1)[1])
        self.assertIn(self.w.digest, notes)
        for p in PLATS:
            self.assertIn(self.record["package_hashes"][p], notes)

    def test_release_notes_list_each_waiver_with_case_id_and_reason(self):
        self.record["waived"] = [
            {"key": "corpus.ds1.read-all@corpus/windows-x86", "id": "corpus.ds1.read-all",
             "platform": "windows-x86", "status": "missing",
             "waiver": waiver()},
            {"key": "perf.first-open@performance/linux-x86_64", "id": "perf.first-open",
             "platform": "linux-x86_64", "status": "not_run",
             "waiver": waiver(id="perf.first-open", platform="linux-x86_64",
                              reason="no timing runner on linux")}]
        section = RR.release_notes(self.record, "v9.9.9").split("## Waived checks", 1)[1]
        self.assertIn("`corpus.ds1.read-all` (windows-x86, missing)", section)
        self.assertIn(waiver()["reason"], section)
        self.assertIn("`perf.first-open` (linux-x86_64, not_run)", section)
        self.assertIn("no timing runner on linux", section)
        self.assertNotIn("None", section)

    def test_subcommands_print_github_output_style_lines(self):
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            rc = RR.main(["find-record", "--root", str(self.w.root),
                          "--qualification-dir", str(self.w.out)])
        self.assertEqual(rc, 0, err.getvalue())
        lines = dict(l.split("=", 1) for l in out.getvalue().splitlines() if "=" in l)
        self.assertEqual(lines["run_id"], "1001")
        self.assertEqual(lines["commit"], COMMIT)
        self.assertEqual(lines["repository"], REPOSITORY)
        self.assertEqual(lines["record"], str(self.w.record_path()))
        self.w.record_path().unlink()
        with redirect_stdout(io.StringIO()), redirect_stderr(err):
            self.assertEqual(RR.main(["find-record", "--root", str(self.w.root),
                                      "--qualification-dir", str(self.w.out)]), 1)
        self.assertIn("::error::No reviewed qualification record", err.getvalue())

    def test_verify_release_and_notes_subcommands(self):
        notes = self.w.tmp / "notes.md"
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            rc = RR.main(["verify-release", "--record", str(self.w.record_path()),
                          "--candidate-dir", str(self.cdir), "--tag", "v9.9.9",
                          "--profile", str(PROFILE), "--root", str(self.w.root),
                          "--notes-out", str(notes)])
        self.assertEqual(rc, 0, err.getvalue())
        self.assertIn("## Waived checks", notes.read_text(encoding="utf-8"))
        with redirect_stdout(io.StringIO()), redirect_stderr(err):
            rc = RR.main(["verify-release", "--record", str(self.w.record_path()),
                          "--candidate-dir", str(self.cdir), "--tag", "v9.9.10",
                          "--profile", str(PROFILE), "--root", str(self.w.root)])
        self.assertEqual(rc, 1)
        self.assertIn("::error::", err.getvalue())

    def test_check_run_subcommand(self):
        runfile = self.w.tmp / "run.json"
        write_json(runfile, self.w.run_info)
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            self.assertEqual(RR.main(["check-run", "--record", str(self.w.record_path()),
                                      "--run-json", str(runfile),
                                      "--repository", REPOSITORY]), 0)
            self.assertEqual(RR.main(["check-run", "--record", str(self.w.record_path()),
                                      "--run-json", str(runfile),
                                      "--repository", "x/y"]), 1)


if __name__ == "__main__":
    unittest.main()
