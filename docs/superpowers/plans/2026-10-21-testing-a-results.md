# Trustworthy Runner Results Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make screenshot and selected test-lane results fail closed, with preserved reproduction evidence.

**Architecture:** Keep `compare_images` as the pixel comparator. Refactor the golden runner into an explicit argument-driven function that reports every selected case, requires valid fresh renderer output, and preserves failures in a caller-selected artifact directory. Exercise all orchestration with temporary fixtures and subprocess mocks; no game installation is needed.

**Tech Stack:** Python 3, unittest/mock, Pillow, CMake/CTest, existing platform CI.

**Spec:** [Approved design](../specs/2026-10-21-vanilla-113c-testing-design.md), packages A and result semantics in section 8.

## Global Constraints

- Vanilla 1.13c is the game reference; A's unit tests use authored images only.
- Portable tests run on Windows, Linux, and macOS without original game assets.
- Missing selected fixtures, zero selected cases, child failures, timeouts, and malformed output cannot return success.
- Preserve reference images unless `--update` is explicitly selected; failed cases never update references.
- Changes are limited to the runner, its tests, CMake/CI integration, and matching documentation.

## Review Focus

- Stale output from an earlier successful process cannot make a failing process pass (task 1).
- One skipped case among passing cases must still fail the selected required suite (task 2).
- Unreadable/corrupt images, missing executable, and paths with spaces need deterministic diagnostics (tasks 1–2).
- An invalid tolerance or unknown argument must be rejected before launching a renderer (task 2).
- A failed golden update must leave the previous reference intact (task 3).

## Task 1: Observe renderer outcomes and fresh valid images

**Files:** Modify `scripts/run_golden_tests.py`; create `test/python/test_golden_runner.py`.

**Interfaces:**
- Preserve `render_headless(lvltype_id, lvlprest_def, ds1_path, output_path) -> bool` for existing callers.
- Add `RenderResult(ok: bool, reason: str, returncode: int | None, stdout: str, stderr: str)` and `render_case(exe: Path, cwd: Path, ids: tuple[str, str], ds1_path: str, output: Path, timeout_s: float = 120) -> RenderResult`.
- Use an output path in an owned per-case temporary directory. Refuse any pre-existing output path before process launch; do not delete arbitrary caller files.

- [ ] Write tests `test_nonzero_exit_with_nonempty_output_fails`, `test_zero_exit_without_output_fails`, `test_stale_output_is_refused`, `test_invalid_image_fails`, `test_timeout_and_oserror_are_reported`, `test_valid_fresh_image_passes`, and `test_executable_and_output_paths_with_spaces`. Assert `ok`, reason, return code, captured diagnostics, and no shell command construction. Mock `subprocess.run` with side effects writing a valid 2×2 BMP only for fresh-output cases.
- [ ] Run `python -m unittest discover -s test/python -p test_golden_runner.py -v`; confirm the original false-success behavior is detected before implementing the refactor.
- [ ] Implement `render_case` using an argument list, `shell=False`, explicit cwd, timeout, return-code check, and Pillow image verification/loading. Keep the compatibility wrapper thin. Return all captured diagnostics on failure.
- [ ] Rerun the named tests; all cases pass without touching `bin/`, user INIs, or `test/golden/`.
- [ ] Review the diff and commit only the runner/test changes belonging to this task.

## Task 2: Require the complete selected suite and validate arguments

**Files:** Modify `scripts/run_golden_tests.py`; extend `test/python/test_golden_runner.py`.

**Interfaces:**
- `main(argv: list[str] | None = None) -> int`; preserve `--core`, `--full`, `--tolerance N`, and `--update`.
- Add `--exe PATH`, `--assets-dir PATH`, `--golden-dir PATH`, `--artifacts-dir PATH`; native default executable is `ds1edit.exe` on Windows and `ds1edit` elsewhere.
- Exit 0 only for a nonempty fully successful selected suite; 1 for case failures/missing required fixtures; 2 for invalid CLI or suite configuration. `--core` and `--full` are mutually exclusive; tolerance is an integer 0–255.
- `run_suite(names: list[str], options: argparse.Namespace) -> dict` returns `schema: "ds1edit.golden/1"`, `selected`, `passed`, `failed`, `skipped`, and `cases` entries with `id`, `status`, `reason`, and artifact paths. Any skipped selected case prevents success.

- [ ] Add tests for all missing references, one missing INI among passes, invalid INI, empty full selection, missing assets directory, duplicate requested names, malformed INI numeric IDs, invalid tolerance `-1`/`256`/text, unknown flags, incompatible mode flags, and a complete matching suite. Assert no launch for configuration errors. INI entries require two integer IDs and a nonempty remaining path; support a path containing spaces without changing virtual-path separators.
- [ ] Run the targeted unittest suite and observe failures against existing all-skipped and ad-hoc argument behavior.
- [ ] Implement argparse validation, explicit path parameters, stable unique selection, numeric INI parsing, and per-case exception reporting. Preserve the six current core map names and zoom `1:4`.
- [ ] Run all Python runner tests. Inject one differing pixel at tolerance 0 and assert exit 1; repeat a matching image and assert exit 0 with selected/passed both 1.
- [ ] Review and commit task-owned changes.

## Task 3: Preserve diagnostics and integrate the portable lane

**Files:** Modify `scripts/run_golden_tests.py`, `test/CMakeLists.txt`, `.github/workflows/build.yml`, `README.md`, `BUILDING.md`; extend `test/python/test_golden_runner.py`; create `test/python/requirements.txt` containing the existing Pillow dependency (no pytest dependency).

**Interfaces:**
- Under `--artifacts-dir`, write a unique owned run directory containing `results.json`, child stdout/stderr, actual images, and expected-image copies for failures. Report the run path; leave golden inputs unchanged.
- Explicit `--update` writes a same-directory temporary PNG and replaces the reference only after render/decode/save succeeds. It produces an update result, not a qualification-equivalence claim.
- CTest test name `golden_runner_unit`, label `portable`, timeout 60; tests run with discovered `Python3_EXECUTABLE`. Configure can remain available without Python, but mandatory portable CI explicitly installs Python/Pillow and verifies this test was registered and executed.

- [ ] Add tests for failed run artifact survival, repeated runs not overwriting prior evidence, JSON totals matching cases, output-write failure returning nonzero, failed update preserving reference bytes, successful explicit update, and ordinary mismatch never changing a golden.
- [ ] Run targeted unittest tests red, implement owned-directory diagnostics and atomic update, then run green.
- [ ] Add Python discovery/CTest registration without launching real game renders in public CI. In all three existing CI jobs install Python/Pillow and run `python -m unittest discover -s test/python -v`; CTest remains the native-suite entry point. Fail CI if Python tests are absent, not by treating absent dependencies as passing skips.
- [ ] Validate `ctest --preset default -N` includes `golden_runner_unit` in a configured Python-capable build; run that test with `ctest --preset default -R '^golden_runner_unit$' --output-on-failure`. Use the corresponding existing presets on Linux/macOS CI.
- [ ] Update test instructions, remove the deleted placeholder listing, state CMake minimum 3.21, pair the example Release build with the Release test preset, and distinguish authored runner unit tests from licensed-data screenshot comparisons.
- [ ] Rerun Python tests and the existing native suites affected by CMake changes; do not regenerate the five mismatching golden maps. Record the remaining real-map mismatches as unresolved evidence.
- [ ] Review and commit only this package's changes. Package A is complete when the two reproduced false-success paths are regression-tested and all selected failures remain visible.
