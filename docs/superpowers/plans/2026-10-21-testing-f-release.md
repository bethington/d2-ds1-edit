# Native Distribution and Release Qualification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Verify the actual distributed packages and bind all required testing evidence into one fail-closed release qualification report.

**Architecture:** Inspect and extract CPack artifacts in owned directories, test the extracted executable and native workflows, and aggregate A–E results against an explicit behavior/profile ledger. The build workflow uploads immutable candidate packages; local qualification uses those exact artifacts. A reviewed metadata-only qualification record selects that candidate run, and the release workflow downloads/verifies those packages without rebuilding them before its existing draft-release job.

**Tech Stack:** Python unittest/zipfile/tarfile/subprocess, CPack/CMake, existing platform CI, B schemas, native Windows/Linux/macOS environments.

**Spec:** [Approved design](../specs/2026-10-21-vanilla-113c-testing-design.md), package F and all ten section-10 requirements. Depends on A and consumes B–E outputs.

## Global Constraints

- Qualify Windows x86, Linux x86_64, and macOS arm64 packages, independently of the Windows reference-game host.
- Every required result must match the exact candidate source/build/package and reference/contract identities.
- Required unavailable, not-run, partial, empty, or mismatching evidence blocks readiness.
- Package inspection must not distribute game archives, extracted tiles/tables, generated palettes/caches, or local configuration.
- Clean-environment checks must disclose installed prerequisites; a developer host with hidden dependencies is insufficient evidence.
- Do not tag, publish, change versions, or trigger release workflows as a side effect of tests.

## Review Focus

- An archive can contain correct-looking names yet have traversal, symlinks, wrong architecture, or missing runtime closure (task 1).
- Windows CI machines already carrying MSVC runtimes cannot establish clean-machine startup alone (tasks 1–2).
- macOS CLI startup does not establish the advertised app/input workflow (task 2).
- A green report can be stale, copied from another package, or missing an entire lane/platform (task 3).
- Documentation and GUI workflows must exercise the extracted candidate, not `bin/` or a user's existing config (tasks 2, 4).

## Task 1: Safe package inventory, dependency closure, and version assertions

**Files:** Create `scripts/equivalence/package_check.py`, `test/python/test_package_check.py`; modify `CMakeLists.txt`, `cmake/macos/Info.plist.in`, and `.github/workflows/build.yml`.

**Interfaces:** `inspect_package(archive: Path, platform: str, expected_version: str, out_dir: Path) -> dict`; CLI `python -m scripts.equivalence.package_check --archive PATH --platform windows-x86|linux-x86_64|macos-arm64 --version VERSION --out-dir PATH`. Return B case results plus archive hash and exact extracted entry inventory.

- [ ] Write archive-fixture tests for missing executable/UI/editor asset/sample INI/LICENSE/NOTICE/README, forbidden MPQ/tiles/excel/cache/user INI, duplicate members, traversal/absolute/drive/UNC names, symlink escapes, wrong executable architecture, empty archives, mismatched version, and required macOS app entry. Extract only after validating all members and enforce a resolved owned destination.
- [ ] Run package tests red, implement inventory/extraction validation and executable architecture/header checks, then run green. Use synthetic archive members rather than checking in actual game assets or large executables.
- [ ] Generate a real CPack package for each native build and compare its runtime closure with declared OS prerequisites. On Windows, ensure any required MSVC redistributable DLLs are collected from the correct-architecture toolchain redistribution location instead of broadly discarding all System32-resolved DLLs as OS-supplied. Use installed CMake runtime-distribution facilities after verifying their local behavior; test the resulting import closure.
- [ ] Verify macOS bundle version strings use the actual CMake project version and the required `.app` is installed alongside the assets/CLI layout expected by `ds1_mac_fix_working_directory`. Preserve its intended ad-hoc signature and input-monitoring identity; test artifact contents on macOS.
- [ ] Add CPack and package inspection to each platform's candidate-build job in `build.yml`. Upload packages plus per-platform candidate manifests only after checks pass. Record workflow run ID, built-from commit, canonical source-state digest, build configuration/compiler, and exact executable/package hashes. Test a missing DLL/asset and mismatched version and show the check fails. Do not dispatch a publishing workflow during verification.
- [ ] Review and commit task-owned checks and demonstrated packaging fixes.

## Task 2: Extracted startup and native edit workflows

**Files:** Create `scripts/equivalence/native_smoke.py`, `test/python/test_native_smoke.py`, `test/equivalence/profiles/native-core.json`, `docs/testing/native-release-checklist.md`; extend CLI smoke tests and production input handling only for demonstrated failures.

**Interfaces:** `run_native_smoke(extracted_root: Path, profile: dict, out_dir: Path) -> list[dict]`. The process uses the extracted executable, an owned working/config directory with explicit 1.13c paths when needed, a declared environment, bounded timeouts, and captured diagnostics.

- [ ] Test process failure with stale output, timeout, path with spaces, executable from outside extraction, missing declared runtime, and accidental user INI discovery. Authored unit cases use fake subprocesses; native evidence must execute real packages.
- [ ] Execute extracted `help`, `--help`, unknown-verb and missing-argument cases with expected codes/output. Run a no-game-data startup/error case separately from game-backed GUI tests so a local installation cannot hide broken setup behavior.
- [ ] Run game-backed `--selftest=30` for declared map inputs and record actual renderer/display initialization and clean exit. Automate the portable data parts of open/edit/undo/save/reopen using C probes; record remaining GUI actions in the native checklist: map browsing/switching, modal key isolation, selection/object/path operations, zoom/resize, unsaved-change handling, export cancellation, and shutdown. Attach results to package/source identity and platform; a selftest alone cannot mark manual workflow cases complete.
- [ ] Run Windows in a clean VM or equivalent declared base without development tools/undeclared redistributables; Linux in an environment with documented system Allegro prerequisites; macOS with documented Allegro and the packaged app/input entry point. If one environment is unavailable, keep that platform's qualification unavailable rather than substituting a developer machine pass.
- [ ] Confirm saved edits through C's independent game checks and exports through D's decoder checks. Record input/config hashes and limitations; review and commit scripts/checklist/tests.

## Task 3: Behavior ledger and fail-closed qualification aggregation

**Files:** Create `test/equivalence/profiles/release-core.json`, `scripts/equivalence/qualify.py`, `test/python/test_qualification.py`, `docs/testing/behavior-ledger.md`, `release/qualification/README.md`; modify `.github/workflows/release.yml` to consume validated candidate evidence without rebuilding packages or embedding game data/secrets.

**Interfaces:**
- `qualify(profile: dict, candidate: dict, results: list[dict]) -> dict` produces schema `ds1edit.qualification/1`: candidate/reference identities, profile digest, `ready` boolean, required IDs, per-lane/platform totals, missing/mismatching/stale/partial IDs, coverage/limits, review dispositions, and input artifact hashes.
- CLI `python -m scripts.equivalence.qualify --profile PATH --candidate PATH --results-dir PATH --out PATH`; exits 0 ready, 1 not ready, 2 invalid evidence.
- Candidate manifest records source tree plus dirty-diff digest, compiler/configuration, each executable/package hash, approved contract/baseline/profile digests, and the portable/native platform set.
- `source_state_digest(root: Path) -> str` hashes sorted relative names and bytes of all declared build inputs (`src/`, `assets/ui/`, `assets/editor/` excluding generated caches/version files, the actual area INIs selected by CMake, sample INI, source renderer config, `cmake/`, `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`, `test/`, `scripts/`, `tools/d2mpq.py`, notices/license and distributed README). Enumerate tracked plus relevant nonignored untracked inputs and the explicit CMake asset set; reject undeclared dependencies. Exclude `.git/`, Python/compiled caches, local outputs, private data, and `release/qualification/` metadata to prevent a report from changing the code identity it describes. Preserve built-from Git commit separately.
- Qualification output binds `candidate_run_id` and all exact platform artifact hashes. The reviewed metadata-only record is stored at `release/qualification/<source_state_digest>.json`; bulk payloads/screenshots stay in the retained evidence bundle/local artifacts. The record contains each required case result, provenance and artifact hashes, not just `ready: true`.
- Release profile maps each of the ten approved design requirements to required case IDs/lane/platform and finite coverage dimensions. It includes C's catalogued DS1 accounting, writer feature cases, D's visual feature set/six maps, E's sanitizer/resource evidence, live revalidation, and native package workflows.

- [ ] Write tests for an entirely empty result set; missing platform/lane/feature; required `not_run`/`partial`/`unavailable`; mismatched or duplicate case IDs; stale source/executable/package/contract/baseline/adapter/profile identity; zero distinct inputs; capped corpus; forged passing totals; invalid expected-rejection declaration; absent negative-control detection; unresolved defects and unreviewed performance regressions. Also test source edits change the digest while qualification metadata alone does not, and replacement/rebuilt ZIP bytes fail the previously qualified artifact hash. Assert `ready == false` and named reasons.
- [ ] Run aggregation tests red; implement strict B schema validation and derive totals from individual required cases rather than trusting summary counters. Optional passes cannot replace required missing cases. Require complete accounting of every enumerated DS1 input and declared visual/writer feature axis.
- [ ] Add a fully authored complete-profile unit fixture that qualifies, then individually corrupt one input identity/status and prove the result fails. Keep review dispositions exact case/issue IDs with rationale/evidence, not wildcard exceptions or suppressed mismatches.
- [ ] Assemble fresh portable/corpus/live/native results for one candidate revision. Revalidate every required reference contract live and bind it to the candidate before accepting release evidence. Any candidate code/package change invalidates affected qualification results; do not claim a historical reference replay executed the live lane.
- [ ] Replace the release workflow's rebuild path with validation/consumption of the candidate run selected by the maintainer-reviewed record in `release/qualification/`. Compute tagged source identity, load its exact record, download artifacts from that repository's successful `build.yml` run using the recorded run ID, and verify original built commit/source identity, every package hash, all individual required results, and profile/contract/baseline identities before the existing draft-release publish job. A metadata-only follow-up commit may carry the record because source identity is separately checked; arbitrary reports from an unreviewed URL/input are not accepted. If artifacts have expired, require a new candidate and requalification. No automatic report acceptance, tag creation, or workflow dispatch is added.
- [ ] Run `python -m unittest discover -s test/python -p test_qualification.py -v`, validate an actual combined report, update the ledger with measured coverage and gaps, review, and commit.

## Task 4: Documentation and final review of the candidate

**Files:** Modify `README.md`, `BUILDING.md`, `CHANGELOG.md`, `docs/release-readiness.md`, and `docs/testing/native-release-checklist.md`; add `docs/testing/qualification.md` documenting the actual implemented commands.

- [ ] Execute documented configure/build/test/package/replay commands on their stated platforms or attach the native job evidence. Correct obsolete placeholder/data-folder guidance and state the actual vanilla 1.13c compatibility scope and documented platform prerequisites.
- [ ] Cross-check release version strings, sample INI paths, package entry points, test dependency instructions, and no-game setup behavior against the extracted artifacts. Preserve attribution/notices and keep user-specific paths out of committed instructions.
- [ ] Review all changed production code for unresolved save/parse/runtime risks and compare the behavior ledger to the approved spec row by row. Record missing checks as missing, never as implicit success.
- [ ] Run the final required profile against the unchanged candidate and retain the complete report and artifacts. Request a whole-change code review using the existing review skill before claiming the candidate qualified. A passing qualification is evidence for a release decision; it does not publish/tag the release.
- [ ] Commit only reviewed task-owned documentation and fixes; present the candidate identities, actual pass/failure/coverage results, and any limitations to the maintainer.
