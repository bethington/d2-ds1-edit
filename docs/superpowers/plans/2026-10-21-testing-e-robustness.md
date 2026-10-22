# Robustness and Performance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose unsafe inputs, save failures, resource growth, untested branches, and unexplained performance regressions in the exercised release-critical paths.

**Architecture:** Drive C/D production probes with deterministic generated corruptions and bounded subprocesses. Instrument supported native builds with sanitizers and coverage. Run repeated workloads in owned scratch environments and record hardware/build/workload identity with timing and resource samples.

**Tech Stack:** Python unittest/random/subprocess, existing C probes, GCC/Clang ASan/UBSan and coverage on supported Linux hosts, native resource observation on other platforms.

**Spec:** [Approved design](../specs/2026-10-21-vanilla-113c-testing-design.md), package E, robustness/performance rows, and sections 10–11. Depends on C/D probe interfaces and B result schemas.

## Global Constraints

- Deterministic seeds, input hashes, candidate identity, and reproduction commands accompany failures.
- Invalid-data rejection tests are editor requirements; unsafe original-game behavior is never the expected result.
- Known vanilla-compatible tails have exact case identities and cannot create broad parser exemptions.
- No unbounded fuzzing or tests against user originals. Outputs stay under owned temporary roots.
- No arbitrary universal FPS or memory-growth threshold. Performance comparisons require the same workload, host class, and build configuration.
- Required sanitizer failures, crashes, missing observations, and unreviewed regressions remain visible to F.

## Review Focus

- Length/count multiplication can overflow before allocation/bounds checks (task 1).
- EOF inside a word/string/bitstream can look like a valid zero value (task 1).
- Failed close/rename or allocation must leave recoverable state (task 1).
- A stable single run can hide leaks or stale state across map switches/exports (task 3).
- Debug/instrumented timings and repeated identical observations cannot establish a Release performance baseline or meaningful coverage (tasks 2–3).

## Task 1: Deterministic malformed-input and failure campaigns

**Files:** Create `scripts/equivalence/adversarial.py`, `test/python/test_adversarial.py`, `test/equivalence/profiles/adversarial-core.json`; extend C/D regression tests and contract invalid-input cases.

**Interfaces:** `mutations(kind: str, valid_bytes: bytes, seed: int) -> Iterable[tuple[str,bytes]]`; `run_adversarial(profile: dict, probes: dict[str,Path], out_dir: Path) -> list[dict]`. Each probe has a declared timeout and maximum generated payload size; no shell interpolation.

- [ ] Test generator determinism, distinct input hashing, size bounds, crash/timeout detection, and reproduction artifact survival. Generate truncations at every header/record boundary and final 1–3 bytes; DS1 missing NUL/count inflation; DT1 offset/length overflow; DCC bitstream and signed width extremes; DC6 frame-offset/RLE overrun; COF dimension/priority-length overflow; AnimData record/count truncation.
- [ ] Run generator tests red, implement bounded generation and result aggregation, run green. Known compatibility-tail cases are run as valid expected-success controls alongside invalid rejections.
- [ ] Feed generated cases into real C/D probes with a 10-second per-case limit and 1 MiB authored-payload cap for the portable campaign. Record partial if a campaign budget truncates selected required cases; do not silently cap the result count. Larger original files stay in separately declared local profiles.
- [ ] Add deterministic allocation and checked-save operation failure positions through existing C test seams; assert failure reporting, no out-of-root files, and ability to run the next valid operation.
- [ ] For every observed defect, minimize to a stable unit regression, investigate its root cause, fix, and rerun that case plus affected valid fixtures before broadening the campaign. Commit minimized authored regressions with supported fixes.

## Task 2: Sanitizer and instrumented coverage lane

**Files:** Modify `CMakeLists.txt`, `test/CMakeLists.txt`, `.github/workflows/build.yml`; create `scripts/equivalence/coverage_report.py`, `test/python/test_coverage_report.py`; document commands in `BUILDING.md`.

**Interfaces:** CMake options `DS1EDIT_ENABLE_SANITIZERS` and `DS1EDIT_ENABLE_COVERAGE`, both OFF by default. Sanitizers on supported GCC/Clang targets use address/undefined checks and frame pointers; reject unsupported requested configurations explicitly. Coverage produces JSON containing production file/line/branch totals and explicit exclusions, never a suite-count-derived percentage.

- [ ] Test coverage aggregation with absent files, zero executed branches, duplicate input reports, excluded Unity/stubs/generated adapters, and an uninstrumented production module. Required absent reports are unavailable; uninstrumented code is disclosed rather than counted covered.
- [ ] Run report tests red, implement parser/aggregation for the chosen compiler's native machine-readable coverage format, run green. Pin tool format/version in artifacts and avoid a new dashboard dependency.
- [ ] Configure a separate Linux build with sanitizer options, build the relevant production-backed probes/tests, and run the authored valid/adversarial C/D suites. Confirm a temporary deliberately invalid test program makes the sanitizer detector fire; remove that control from product targets after retaining detector-test evidence.
- [ ] Configure a separate coverage build, execute the same declared production scenarios, and produce line/branch reports with module-level gaps. Do not use sanitizer/coverage builds for Release timing claims.
- [ ] Add the bounded authored campaign to the Linux CI lane; local corpus sanitizer runs remain explicit. Required sanitizer diagnostics cause failure even if the child writes an output file.
- [ ] Review flags for unaffected Release builds on all three platforms, update commands and ledger, then commit.

## Task 3: Repeated-workload resource and performance records

**Files:** Create `scripts/equivalence/benchmarks.py`, `test/python/test_benchmarks.py`, `test/equivalence/profiles/performance-core.json`; add a narrowly scoped existing-editor selftest mode or test probe for repeat cycles only if current CLI cannot drive the workload.

**Interfaces:** `measure_workload(command: list[str], cwd: Path, repetitions: int, warmups: int, observer) -> dict`; record sample durations, workload/input hashes, Release build/compiler identity, host/OS/renderer, memory/resource observation scope, warm/cold classification, and outputs checked. Use 3 warmups and 10 measured repetitions for stable repeatable tasks; record separate cold-start samples rather than mixing them with warm-cache results.

- [ ] Test median/sample accounting, observer unavailable, timeout, changed host/build identity, wrong output despite good timing, monotonically increasing resource series, and cancellation cleanup. A missing memory observer cannot become zero observed growth.
- [ ] Implement measurements for first map open, switching a declared pair of maps with shared/different tilesets, repeated DCC/DT1 decode, repeated composition/export, and shutdown. Validate outputs each cycle with C/D checks; record wall time separately from rendering/present metrics already exposed by the editor.
- [ ] Capture an initial known-good Release baseline on a named host. Compare only like-for-like builds/workloads/host classes; report distributions and growth trends. Introduce a CI regression threshold only after baseline evidence supports a reviewed numeric budget; until then emit `unreviewed` comparisons for maintainer disposition in F, not a made-up performance pass.
- [ ] Run the repeated-workload resource checks on each supported native platform available for qualification. Check state/handles after cancellation and switching maps, and use allocator/leak diagnostics in the appropriate instrumented lane.
- [ ] Preserve baseline and candidate records separately, update coverage/resource limitations, review demonstrated defects, and commit code/authored profiles. Local game samples and hardware-specific results remain artifacts.
