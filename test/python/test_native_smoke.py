"""Tests for scripts/equivalence/native_smoke.py using fake subprocesses.

No real executable is launched here; the real-package run is recorded under
tmp/native-smoke/. The fakes emit the same text the real extracted
ds1edit.exe prints (captured during the real run).
"""
import copy
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from scripts.equivalence import native_smoke as N
from scripts.equivalence import qualify as Q

ROOT = Path(__file__).resolve().parents[2]
PROFILE = json.loads((ROOT / "test/equivalence/profiles/native-core.json")
                     .read_text(encoding="utf-8"))
PLAT = "windows-x86"
PKG_SHA = "ab" * 32

HELP = ("ds1edit -- Diablo II DS1 area editor (CLI mode)\n\nUsage:\n  ds1edit [verb]\n\n"
        "Verbs:\n  list-mpqs  x\n  export-raw  y\n")


def script_for(args, cwd, game):
    """What the real exe prints for these args (rc, stdout, stderr)."""
    a = list(args)
    if a in (["help"], ["--help"], ["-h"]):
        return 0, HELP, ""
    if a == ["bogusverb"]:
        return 3, "ds1edit: unknown verb 'bogusverb'\n" + HELP, ""
    if a == ["probe"]:
        return 3, ("ds1edit probe: missing path argument\n"
                   "Usage: ds1edit probe <virtual-path>\n"), ""
    if a == ["dump-txt-row"]:
        return 3, ("ds1edit dump-txt-row: too few arguments\n"
                   "Usage: ds1edit dump-txt-row <virtual-txt-path> <row-id>\n"), ""
    ini = Path(cwd, "Ds1edit.ini").read_text(encoding="utf-8")
    install = [l.split("=", 1)[1].strip() for l in ini.splitlines()
               if l.startswith("d2_install")][0]
    if a[0].startswith("assets/") and a[1] == "--selftest=5":
        return 255, ("d2install: using configured path <%s>\n\nds1edit_error() :\n"
                     "No Mod Directory and no MPQ files available : it can't work.\n"
                     % install), ""
    if a[0].startswith("assets/"):
        shot = [x.split("=", 1)[1] for x in a if x.startswith("--selftest-shot=")][0]
        Path(shot).write_bytes(N.PNG_MAGIC + b"fake")
        return 0, ("d2install: using configured path <%s>\nselftest: rendered 30 frame(s), "
                   "exiting\nselftest: input frames=30 events=9 timer=9\n\nds1edit_exit()\n"
                   % install), "display: 800x600, Direct3D driver\n"
    raise AssertionError("unexpected args %r" % (a,))


class FakeRunner:
    def __init__(self, hook=None):
        self.calls = []
        self.hook = hook

    def __call__(self, cmd, **kw):
        self.calls.append({"cmd": cmd, **kw})
        if self.hook:
            r = self.hook(cmd, kw)
            if r is not None:
                return r
        rc, out, err = script_for(cmd[1:], kw["cwd"], None)
        kw["stdout"].write(out.encode())
        kw["stderr"].write(err.encode())
        return SimpleNamespace(returncode=rc)


def make_package(base: Path, name="DS1Edit-2.1.1-win32", runtime=True):
    root = base / "extracted" / name
    root.mkdir(parents=True)
    (root / "ds1edit.exe").write_bytes(b"MZ-fake-exe")
    if runtime:
        for f in PROFILE["platforms"][PLAT]["runtime_files"]:
            (root / f).write_bytes(b"dll")
    inspection = {"status": "pass", "platform": PLAT, "expected_version": "2.1.1",
                  "archive": {"sha256": PKG_SHA}, "extracted_root": str(base / "extracted")}
    return root, inspection


class NativeSmokeBase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="native smoke ")   # spaces on purpose
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.game = self.base / "game 113c"
        self.game.mkdir()
        for m in ("patch_d2.mpq", "d2exp.mpq", "d2data.mpq", "d2char.mpq"):
            (self.game / m).write_bytes(m.encode())
        self.pkg = self.base / "pkg"
        self.pkg.mkdir()
        self.root, self.inspection = make_package(self.pkg)
        self.out = self.base / "out dir"
        self.profile = copy.deepcopy(PROFILE)
        self.profile["selftest"]["maps"] = ["act1_town", "act5_town"]

    def run_smoke(self, runner=None, **kw):
        runner = runner or FakeRunner()
        kw.setdefault("game_install", self.game)
        kw.setdefault("inspection", self.inspection)
        cases = N.run_native_smoke(kw.pop("root", self.root), self.profile, self.out,
                                   platform=PLAT, runner=runner, **kw)
        return {c["id"].split(".", 2)[2]: c for c in cases}, runner


class HappyPath(NativeSmokeBase):
    def test_all_automated_cases_pass_and_manual_ones_do_not(self):
        cases, runner = self.run_smoke()
        self.assertEqual(set(cases), set(N.CASE_NAMES))
        for n in ("cli-help", "no-game-startup", "selftest", "platform-startup"):
            self.assertEqual(cases[n]["status"], "pass", (n, cases[n]["failures"]))
        self.assertEqual(cases["clean-environment"]["status"], "unavailable")
        self.assertTrue(any("developer host" in l for l in cases["clean-environment"]["limitations"]))
        for n in ("gui-edit-undo-save-reopen", "gui-modal-input"):
            self.assertEqual(cases[n]["status"], "not_run")

    def test_cases_are_valid_b_schema_and_bound_to_the_package(self):
        cases, _ = self.run_smoke()
        for c in cases.values():
            Q.validate_case(c)
            self.assertEqual(c["lane"], "native-workflow")
            self.assertEqual(c["platform"], PLAT)
            self.assertEqual(c["input_sha256"], PKG_SHA)
            self.assertEqual(c["provenance"]["candidate"]["packages"], [PKG_SHA])
        self.assertEqual(cases["cli-help"]["id"], "native.windows-x86.cli-help")
        sha = cases["cli-help"]["provenance"]["executable"]["sha256"]
        self.assertEqual(sha, N.sha256_file(self.root / "ds1edit.exe"))

    def test_results_are_written_for_qualify_to_load(self):
        self.run_smoke()
        files = sorted(p.name for p in (self.out / "results").glob("*.json"))
        self.assertEqual(len(files), len(N.CASE_NAMES))
        results, _ = Q.load_results_dir(self.out / "results")
        self.assertEqual(len(results), len(N.CASE_NAMES))

    def test_selftest_records_display_lines_and_every_map(self):
        cases, _ = self.run_smoke()
        subs = cases["selftest"]["actual"]["subcases"]
        self.assertEqual([s["name"] for s in subs], ["selftest-act1_town", "selftest-act5_town"])
        for s in subs:
            self.assertEqual(s["display"]["driver"], "Direct3D")
            self.assertEqual(s["returncode"], 0)
        self.assertEqual(cases["selftest"]["coverage"]["distinct_inputs_compared"], 2)
        self.assertEqual(len(cases["selftest"]["provenance"]["game"]["mpq"]), 4)

    def test_path_with_spaces_is_passed_unmangled_and_without_a_shell(self):
        _, runner = self.run_smoke()
        self.assertIn(" ", str(self.root))
        self.assertIn(" ", str(self.out))
        for call in runner.calls:
            self.assertEqual(call["cmd"][0], str(self.root / "ds1edit.exe"))
            self.assertIs(call["shell"], False)
            self.assertEqual(call["cwd"], str(self.root))
        shots = [a for c in runner.calls for a in c["cmd"] if a.startswith("--selftest-shot=")]
        self.assertTrue(shots and all(" " in s for s in shots))

    def test_extraction_is_left_without_our_ini(self):
        self.run_smoke()
        self.assertFalse((self.root / "Ds1edit.ini").exists())


class Failures(NativeSmokeBase):
    def test_nonzero_exit_with_output_written_is_not_a_pass(self):
        def hook(cmd, kw):
            if any(a.startswith("--selftest-shot=") for a in cmd):
                shot = [a.split("=", 1)[1] for a in cmd if a.startswith("--selftest-shot=")][0]
                Path(shot).write_bytes(N.PNG_MAGIC + b"fake")      # plausible output...
                kw["stdout"].write(b"selftest: rendered 30 frame(s), exiting\n")
                return SimpleNamespace(returncode=1)               # ...but it failed
        cases, _ = self.run_smoke(FakeRunner(hook))
        self.assertEqual(cases["selftest"]["status"], "mismatch")
        kinds = {f["kind"] for f in cases["selftest"]["failures"]}
        self.assertIn("exit_code", kinds)

    def test_stale_output_present_before_launch_is_refused_not_trusted(self):
        # A prior run's shot at the exact path the run will use.
        shots = self.out / "evidence" / "shots"
        orig = N._run_process

        def seeded(ctx, label, args, timeout, **kw):
            for p in kw.get("require_files", ()):
                Path(p).parent.mkdir(parents=True, exist_ok=True)
                Path(p).write_bytes(N.PNG_MAGIC + b"stale")
            return orig(ctx, label, args, timeout, **kw)
        N._run_process = seeded
        try:
            runner = FakeRunner()
            cases, _ = self.run_smoke(runner)
        finally:
            N._run_process = orig
        self.assertNotEqual(cases["selftest"]["status"], "pass")
        self.assertIn("stale_output", {f["kind"] for f in cases["selftest"]["failures"]})
        self.assertFalse(any(any(a.startswith("--selftest-shot=") for a in c["cmd"])
                             for c in runner.calls))

    def test_selftest_exit_zero_without_writing_the_shot_fails(self):
        def hook(cmd, kw):
            if any(a.startswith("--selftest-shot=") for a in cmd):
                kw["stdout"].write(b"selftest: rendered 30 frame(s), exiting\n")
                kw["stderr"].write(b"display: 800x600, Direct3D driver\n")
                return SimpleNamespace(returncode=0)
        cases, _ = self.run_smoke(FakeRunner(hook))
        self.assertEqual(cases["selftest"]["status"], "mismatch")
        self.assertIn("missing_output", {f["kind"] for f in cases["selftest"]["failures"]})

    def test_timeout_is_a_named_mismatch(self):
        def hook(cmd, kw):
            if cmd[1:] == ["help"]:
                raise subprocess.TimeoutExpired(cmd, kw["timeout"])
        cases, _ = self.run_smoke(FakeRunner(hook))
        self.assertEqual(cases["cli-help"]["status"], "mismatch")
        self.assertIn("timeout", {f["kind"] for f in cases["cli-help"]["failures"]})
        self.assertEqual(cases["platform-startup"]["status"], "mismatch")

    def test_crash_exit_status_is_not_accepted_even_if_text_matches(self):
        def hook(cmd, kw):
            if cmd[1:] == ["help"]:
                kw["stdout"].write(HELP.encode())
                return SimpleNamespace(returncode=0xC0000005)
        cases, _ = self.run_smoke(FakeRunner(hook))
        self.assertEqual(cases["cli-help"]["status"], "mismatch")
        self.assertIn("crash", {f["kind"] for f in cases["cli-help"]["failures"]})

    def test_wrong_exit_code_and_missing_text_fail(self):
        def hook(cmd, kw):
            if cmd[1:] == ["bogusverb"]:
                kw["stdout"].write(b"nothing useful\n")
                return SimpleNamespace(returncode=0)
        cases, _ = self.run_smoke(FakeRunner(hook))
        kinds = {f["kind"] for f in cases["cli-help"]["failures"]}
        self.assertEqual(cases["cli-help"]["status"], "mismatch")
        self.assertTrue({"exit_code", "output"} <= kinds)

    def test_no_game_case_that_exits_zero_means_a_game_was_found_elsewhere(self):
        def hook(cmd, kw):
            if cmd[1:] == ["assets/act1_town.ini", "--selftest=5"]:
                kw["stdout"].write(b"d2install: auto-detected <C:\\Diablo2>\n")
                return SimpleNamespace(returncode=0)
        cases, _ = self.run_smoke(FakeRunner(hook))
        self.assertEqual(cases["no-game-startup"]["status"], "mismatch")

    def test_missing_game_install_makes_selftest_unavailable_never_pass(self):
        cases, _ = self.run_smoke(game_install=None)
        self.assertEqual(cases["selftest"]["status"], "unavailable")
        self.assertEqual(cases["cli-help"]["status"], "pass")

    def test_partial_map_failure_fails_the_whole_selftest_case(self):
        def hook(cmd, kw):
            if "assets/act5_town.ini" in cmd:
                kw["stdout"].write(b"selftest: FAIL\n")
                return SimpleNamespace(returncode=1)
        cases, _ = self.run_smoke(FakeRunner(hook))
        self.assertEqual(cases["selftest"]["status"], "mismatch")
        self.assertEqual(cases["selftest"]["coverage"]["distinct_inputs_compared"], 1)

    def test_executable_changed_during_run_demotes_passes(self):
        def hook(cmd, kw):
            if cmd[1:] == ["-h"]:
                (self.root / "ds1edit.exe").write_bytes(b"MZ-patched")
        cases, _ = self.run_smoke(FakeRunner(hook))
        self.assertNotEqual(cases["cli-help"]["status"], "pass")
        self.assertIn("executable_changed", {f["kind"] for f in cases["cli-help"]["failures"]})

    def test_unknown_package_identity_is_unavailable_not_pass(self):
        runner = FakeRunner()
        cases = N.run_native_smoke(self.root, self.profile, self.out, platform=PLAT,
                                   runner=runner, game_install=self.game, inspection=None)
        for c in cases:
            self.assertNotEqual(c["status"], "pass", c["id"])
        self.assertEqual(runner.calls, [])

    def test_failed_package_inspection_does_not_supply_identity(self):
        bad = dict(self.inspection, status="mismatch")
        runner = FakeRunner()
        cases = N.run_native_smoke(self.root, self.profile, self.out, platform=PLAT,
                                   runner=runner, game_install=self.game, inspection=bad)
        self.assertTrue(all(c["status"] != "pass" for c in cases))
        self.assertEqual(runner.calls, [])


class Refusals(NativeSmokeBase):
    def assertRefused(self, cases, runner, kind):
        for n in ("cli-help", "no-game-startup", "selftest", "platform-startup"):
            self.assertEqual(cases[n]["status"], "unavailable", n)
            self.assertIn(kind, {f["kind"] for f in cases[n]["failures"]})
        self.assertEqual(runner.calls, [])

    def test_executable_outside_extraction_via_dotdot_is_refused(self):
        self.profile["platforms"][PLAT]["executable"] = "../outside.exe"
        (self.root.parent / "outside.exe").write_bytes(b"MZ")
        cases, runner = self.run_smoke()
        self.assertRefused(cases, runner, "executable_outside_extraction")

    def test_absolute_executable_path_is_refused(self):
        evil = self.base / "evil.exe"
        evil.write_bytes(b"MZ")
        self.profile["platforms"][PLAT]["executable"] = str(evil)
        cases, runner = self.run_smoke()
        self.assertRefused(cases, runner, "executable_outside_extraction")

    def test_symlinked_executable_is_refused(self):
        outside = self.base / "real.exe"
        outside.write_bytes(b"MZ")
        (self.root / "ds1edit.exe").unlink()
        try:
            os.symlink(outside, self.root / "ds1edit.exe")
        except (OSError, NotImplementedError):
            self.skipTest("symlinks unavailable on this host")
        cases, runner = self.run_smoke()
        self.assertRefused(cases, runner, "executable_outside_extraction")

    def test_missing_executable_is_unavailable(self):
        (self.root / "ds1edit.exe").unlink()
        cases, runner = self.run_smoke()
        self.assertRefused(cases, runner, "executable_missing")

    def test_missing_declared_runtime_blocks_launch_and_fails_startup(self):
        (self.root / "vcruntime140.dll").unlink()
        cases, runner = self.run_smoke()
        self.assertEqual(runner.calls, [], "must not launch: the loader would show a dialog")
        st = cases["platform-startup"]
        self.assertEqual(st["status"], "mismatch")
        self.assertEqual(st["failures"][0]["kind"], "missing_runtime")
        self.assertEqual(st["failures"][0]["path"], "vcruntime140.dll")
        for n in ("cli-help", "no-game-startup", "selftest"):
            self.assertEqual(cases[n]["status"], "not_run")

    def test_user_ini_already_in_extraction_is_refused(self):
        (self.root / "Ds1edit.ini").write_text("d2_install = C:\\Diablo2\n")
        cases, runner = self.run_smoke()
        self.assertRefused(cases, runner, "user_ini_in_extraction")
        self.assertTrue((self.root / "Ds1edit.ini").exists(), "must not delete the user's file")

    def test_out_dir_must_be_fresh(self):
        self.out.mkdir()
        with self.assertRaises(N.SmokeError):
            self.run_smoke()


class UserConfigIsolation(NativeSmokeBase):
    def test_user_profile_and_cwd_ini_are_never_consulted(self):
        user_home = self.base / "real user home"
        (user_home / "AppData" / "Roaming" / "ds1edit").mkdir(parents=True)
        prefs = user_home / "AppData" / "Roaming" / "ds1edit" / "preferences.ini"
        prefs.write_text("last_d2_install=C:\\Users\\me\\SECRET-GAME\n")
        user_cwd = self.base / "user cwd"
        user_cwd.mkdir()
        (user_cwd / "Ds1edit.ini").write_text("d2_install = C:\\Users\\me\\SECRET-GAME\n")
        hostile = {"SystemRoot": "C:\\Windows", "APPDATA": str(user_home / "AppData" / "Roaming"),
                   "USERPROFILE": str(user_home), "HOME": str(user_home),
                   "PATH": "C:\\Python;C:\\tools", "DS1EDIT_CONFIG": "SECRET"}
        old = os.getcwd()
        os.chdir(user_cwd)
        try:
            _, runner = self.run_smoke(environ=hostile)
        finally:
            os.chdir(old)
        owned = (self.out / "home").resolve()
        for call in runner.calls:
            env = call["env"]
            self.assertEqual(Path(call["cwd"]), self.root)
            self.assertNotEqual(Path(call["cwd"]).resolve(), user_cwd.resolve())
            for k in ("APPDATA", "LOCALAPPDATA", "USERPROFILE", "TEMP", "TMP"):
                self.assertTrue(Path(env[k]).resolve() == owned or owned in Path(env[k]).resolve().parents,
                                (k, env[k]))
            self.assertNotIn("DS1EDIT_CONFIG", env)
            self.assertNotIn("HOME", env)
            self.assertNotIn("C:\\Python", env["PATH"])
            self.assertTrue(set(env) <= {"SystemRoot", "windir", "PATH", "USERPROFILE", "APPDATA",
                                         "LOCALAPPDATA", "HOMEDRIVE", "HOMEPATH", "TEMP", "TMP"})
        self.assertEqual(prefs.read_text(), "last_d2_install=C:\\Users\\me\\SECRET-GAME\n")
        self.assertNotIn("SECRET", (user_cwd / "Ds1edit.ini").read_text().replace("SECRET-GAME", ""))

    def test_the_ini_the_process_sees_is_ours_and_names_the_requested_install(self):
        seen = []

        def hook(cmd, kw):
            p = Path(kw["cwd"]) / "Ds1edit.ini"
            if p.exists():
                seen.append(p.read_text())
        self.run_smoke(FakeRunner(hook))
        self.assertTrue(seen)
        installs = {l.split("=", 1)[1].strip() for t in seen for l in t.splitlines()
                    if l.startswith("d2_install")}
        self.assertIn(str(self.game), installs)
        self.assertTrue(any("no-game-install" in i for i in installs))
        self.assertEqual(len(installs), 2)
        for t in seen:
            self.assertIn("full_screen = NO", t)

    def test_cli_cases_run_without_any_ini_in_the_working_directory(self):
        flags = []

        def hook(cmd, kw):
            if cmd[1] in ("help", "--help", "-h", "bogusverb", "probe", "dump-txt-row"):
                flags.append((Path(kw["cwd"]) / "Ds1edit.ini").exists())
        self.run_smoke(FakeRunner(hook))
        self.assertEqual(len(flags), 7)          # six CLI cases + platform-startup
        self.assertFalse(any(flags))


class ManualCases(unittest.TestCase):
    ARGS = dict(archive_sha256="a" * 64, executable_sha256="b" * 64, tester="T",
                environment={"os": "Windows 11 clean VM", "vcredist": "absent"},
                notes="ran sections 1-9")

    def test_complete_pass_is_a_valid_bound_case(self):
        c = N.manual_case("gui-modal-input", PLAT, result="pass",
                          checklist_items=list(range(5)), checklist_total=5, **self.ARGS)
        Q.validate_case(c)
        self.assertEqual(c["status"], "pass")
        self.assertEqual(c["id"], "native.windows-x86.gui-modal-input")
        self.assertEqual(c["provenance"]["candidate"]["packages"], ["a" * 64])

    def test_incomplete_checklist_is_partial_and_failure_is_mismatch(self):
        c = N.manual_case("gui-modal-input", PLAT, result="pass",
                          checklist_items=[0, 1], checklist_total=5, **self.ARGS)
        self.assertEqual(c["status"], "partial")
        c = N.manual_case("gui-modal-input", PLAT, result="fail",
                          checklist_items=list(range(5)), checklist_total=5, **self.ARGS)
        self.assertEqual(c["status"], "mismatch")

    def test_rejects_missing_identity_and_blank_tester(self):
        bad = dict(self.ARGS, archive_sha256="")
        with self.assertRaises(N.SmokeError):
            N.manual_case("gui-modal-input", PLAT, result="pass",
                          checklist_items=[0], checklist_total=1, **bad)
        bad = dict(self.ARGS, tester="")
        with self.assertRaises(N.SmokeError):
            N.manual_case("gui-modal-input", PLAT, result="pass",
                          checklist_items=[0], checklist_total=1, **bad)


class ProfileTests(unittest.TestCase):
    def test_shipped_profile_validates_and_names_release_core_ids(self):
        N.validate_profile(PROFILE)
        rc = json.loads((ROOT / "test/equivalence/profiles/release-core.json")
                        .read_text(encoding="utf-8"))
        wanted = {c["id"] for r in rc["requirements"] for c in r["cases"]
                  if c["lane"] == "native-workflow"}
        for plat in PROFILE["platforms"]:
            for n in N.CASE_NAMES:
                self.assertIn("native.{platform}.%s" % n, wanted)
        self.assertEqual(len(wanted), len(N.CASE_NAMES))

    def test_bad_schema_rejected(self):
        with self.assertRaises(N.SmokeError):
            N.validate_profile({"schema": "x"})


if __name__ == "__main__":
    unittest.main()
