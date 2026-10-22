# Reference Identity and Equivalence Pilot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish one independent original-game/editor comparison, portable replay, and the shared evidence interfaces used by later packages.

**Architecture:** Validate reference contracts and identities before execution. Use authored palettes/queries for a nearest-colour pilot through production `palette_find_closest`; obtain the reference independently from D2CMP. Keep corpus extraction lazy and local through the existing corrected `tools/d2mpq.py` reader. Reuse the existing fleet transport in a specialized adapter, without editing sibling repositories.

**Tech Stack:** Python unittest, JSON/hashlib, existing MPQ reader (`mpyq` only in local lane), C/Unity/CTest, existing Ghidra/REA evidence and d2-fleet transport.

**Spec:** [Approved design](../specs/2026-10-21-vanilla-113c-testing-design.md), sections 4–6 and package B.

## Global Constraints

- Vanilla 1.13c only; identify original DLLs and MPQs by SHA-256, not a path/version label.
- Portable replay cannot import MPQ/fleet dependencies or access a game installation.
- Original results and candidate results are independent; captured expected data cannot come from the candidate.
- Preserve finite-domain/partial/observational limits; verify wrong-model detection.
- Game bulk data stays local; do not change decomp/fleet sources or generated catalogs.
- All native calls require observed ABI/state feasibility and original-byte verification first.

## Review Focus

- A valid-looking capture from a different DLL, asset, or contract must be refused (task 1).
- A lower-priority MPQ or enabled user overlay cannot define vanilla expected bytes (task 2).
- A nearest-colour result's relative index and transparent-index policy must not be normalized implicitly (task 3).
- A wrong model producing a constant or swapping channels must fail a meaningful set of vectors (task 3).
- Missing fleet access and capped/empty captures must remain unavailable/partial, not passing evidence (tasks 3–4).

## Shared interface decisions

Create `scripts/equivalence/contracts.py` using standard-library validation and dictionaries:

- `canonical_digest(value: dict) -> str`: UTF-8 JSON, sorted keys, compact separators, SHA-256; omit only the object's own `digest` field when computing its digest.
- `load_contract(path: Path) -> dict`; schema `ds1edit.contract/1` requires `id`, `kind`, `baseline_digest`, `reference` (`module`, `rva`, `function_bytes_sha256`, `abi_recipe_digest`), `domain`, `outputs`, `normalizations`, `limitations`, `required`, and `digest`.
- `load_baseline(path: Path) -> dict`; schema `ds1edit.baseline/1` requires build `vanilla-113c`, original binary identities, ordered archive identities, enumeration sources/limits, effective `assets`, fixed render configuration, and `digest`.
- `validate_capture(capture: dict, contract: dict, baseline: dict) -> None`; schema `ds1edit.capture/1` includes contract/baseline/adapter digests, original identities, UTC capture timestamp, supporting evidence references, input/output payload hashes, `vectors`, `limitations`, and `digest`. Each vector has stable `id`, `input`, and independent reference `output`.
- `compare_case(contract: dict, expected: dict, actual: dict) -> dict`; returns `ds1edit.case/1` with `id`, `lane`, `platform`, `required`, `status`, `contract_digest`, `baseline_digest`, `input_sha256`, `expected_sha256`, `actual_sha256`, `coverage`, `failures`, `provenance`, and `limitations`.
- Case `status` is exactly `pass`, `mismatch`, `unavailable`, `not_run`, `partial`, or `rejected_expected`. Qualification accepts only a required contract's declared success outcome; expected rejections must be declared invalid-input cases. A passing comparison has at least one compared distinct input and no uncovered required axis.
- `provenance` binds candidate source-state, executable, build configuration/compiler, and applicable package identities. Optional lane results are visible but do not impersonate required lanes.

All payload hashes are of the exact bytes or canonical JSON named by the field.
Unknown normalization names and missing required metadata are validation errors.
Later packages may add contract `kind` adapters without changing these semantics.

## Task 1: Validate contracts, captures, and semantic comparisons

**Files:** Create `scripts/equivalence/__init__.py`, `contracts.py`, `compare.py`, `test/python/test_equivalence_contracts.py`, `test/equivalence/README.md`.

**Interfaces:** Shared interfaces above; `compare_case` lives in `compare.py`. CLI `python -m scripts.equivalence.compare --contract PATH --baseline PATH --capture PATH --actual PATH --out PATH`, exits 0 agreement, 1 mismatch, 2 invalid/unavailable inputs.

- [ ] Write tests for matching one-vector outputs, first differing integer/byte, return equal but memory write different, missing output, wrong baseline/contract/adapter digest, modified payload, duplicate vector ID, no vectors, unknown normalization, partial axes, and masked pointer-address differences only under an explicit rule. Assert status, first failure field/path, and bounded coverage.
- [ ] Run `python -m unittest discover -s test/python -p test_equivalence_contracts.py -v` and confirm missing interfaces fail.
- [ ] Implement strict schema/digest validation, recursive field comparison, explicit normalization registry, and CLI. Compare relevant memory buffers even when returns agree. Required unknown/partial input axes yield `partial` instead of `pass`.
- [ ] Run tests green, then replace actual output with a constant and verify at least one nonconstant expected case returns `mismatch`. Preserve a test proving raw call count does not inflate distinct-vector coverage.
- [ ] Commit task-owned files after review.

## Task 2: Build a local, reproducible vanilla corpus manifest

**Files:** Create `scripts/equivalence/baseline.py`, `test/python/test_equivalence_baseline.py`, `test/equivalence/requirements-local.txt` containing `mpyq`; use existing `tools/d2mpq.py` without changing its decompressor.

**Interfaces:**
- `build_baseline(game_dir: Path, extra_names: list[str], reader_factory=None) -> dict`.
- CLI `python -m scripts.equivalence.baseline --game-dir PATH --out PATH [--extra-names PATH]`.
- Use `D2Archive.namelist(extra_names=...)`, `has`, and `read` via a lazy local import. Select explicit ordered `patch_d2.mpq`, `d2exp.mpq`, `d2data.mpq`, `d2char.mpq` for editor resource resolution; record additional archives only when a declared case consumes them.
- Normalize virtual paths for hashing/lookup to lower-case backslashes; preserve the original spelling as metadata. Never convert those to host paths for archive lookup. Reject filesystem traversal/absolute extracted member paths.

- [ ] Test with fake archives: same name in patch/base resolves patch; case variants deduplicate; mod overlay is absent; changed DLL or archive changes baseline digest; missing required archives/binaries fails; unreadable listfile records an enumeration limitation and blocks a required incomplete corpus; supplementary named paths can be hash-resolved despite listfile omission; invalid extracted names cannot escape owned scratch.
- [ ] Run baseline tests red; implement deterministic sorting, streaming file hashes, effective-byte hashes, dependency table/tileset path inventory, and explicit enumeration limitations. Do not hardcode the historical 2,276 DS1 count as acceptance.
- [ ] Run tests green without importing `mpyq`; then run `python tools/d2mpq.py selfcheck --version <explicit-1.13c-directory>` in the local lane and generate a manifest from that same directory. Record effective asset counts and unresolved paths, not just the label 1.13c.
- [ ] Verify the existing mixed `assets/tiles/` is never read by the baseline generator; inspect the manifest for overlays/preferences and local-only payload locations.
- [ ] Commit code/tests/documented local dependency, keeping manifests with local paths and game payloads under `tmp/` unless sanitized and deliberately selected for a tracked identity-only fixture.

## Task 3: Prove one original-game comparison and capture authored vectors

**Files:** Create `test/test_palette_reference_probe.c`, `scripts/equivalence/fleet_adapter.py`, `scripts/equivalence/capture_palette.py`, `test/equivalence/contracts/palette-nearest-113c.json`, `test/equivalence/fixtures/palette-nearest-inputs.json`, `test/python/test_palette_capture.py`; modify `test/CMakeLists.txt` to build `test_palette_reference_probe` with `src/core/palette.c`.

**Interfaces:**
- Probe CLI: `test_palette_reference_probe --palette palette.rgba --queries queries.rgb --out actual.json`. Palette is exactly 256 RGBA byte entries, queries are packed RGB triples. Output is JSON `{ "indices": [integer, ...] }` from real `palette_find_closest`; malformed lengths return nonzero. No new C JSON parser/library is required.
- `FleetReferenceSession(fleet_root: Path, host: str, port: int, token: str, baseline: dict)` imports the installed checkout's existing `reimpl/prove.py` transport/byte-comparison helpers by explicit path and records their file digest; no copied HTTP client.
- `invoke(contract: dict, vector: dict) -> dict` verifies target module/function bytes and relocations, uses declared scratch state/ABI, returns named values and watched writes.
- Capture CLI requires explicit `--baseline`, `--fleet-root`, `--host`, `--port`, `--out`; authentication comes from the existing environment variable and is never printed or stored in artifacts.

- [ ] Before native invocation, inspect original `D2CMP.dll` nearest-colour routine at RVA `0x9d30` using identified Ghidra/REA evidence. Record the actual calling convention, arguments, palette memory order, search-count semantics, return index, tie policy, relocations, and hidden state in the contract's ABI recipe. Do not trust source parameter names as channel-order evidence.
- [ ] Write mocked-adapter tests refusing wrong original bytes, wrong ABI recipe, missing privileged/reference capability, process death, timeout, empty vectors, truncated vector sets, and out-of-range indices. Also test a compare with equal returns but unexpected writes.
- [ ] Generate authored inputs: 256 entries with distinct RGB values `(i, (73*i)%256, (151*i)%256)`; opaque nonzero entries, transparent index 0; exact-colour queries for indices 1–255; RGB grid using channel values 0/127/255; separate duplicate/equidistant-colour cases. Deduplicate identical queries in coverage.
- [ ] Declare the editor's exclusion of transparent index 0 explicitly. If the original routine can be asked to search entries 1–255, normalize its relative index by the declared offset and test that normalization. If this input domain or ABI cannot be established, record the exact unavailable capability and replan this native task from observed evidence. Do not quietly substitute a different routine while keeping a nearest-colour contract or claim the mocked pilot complete.
- [ ] Implement probe/capture through existing transport. Capture original answers independently, run the editor probe, and compare outputs. An observed difference is investigated and regression-tested; never overwrite expected results with candidate answers.
- [ ] Run a constant-result control and a channel-swap control, showing divergent vector IDs. Controls with no observable effect are rejected as ineffective controls.
- [ ] Commit authored inputs, contract, probe/tests, and small original scalar-result capture with provenance; keep original DLLs out of the fixture set. Package B cannot finish on mocked evidence alone.

## Task 4: Replay reference cases portably and expose later adapters

**Files:** Create `scripts/equivalence/replay.py`, `test/python/test_equivalence_replay.py`; modify `test/CMakeLists.txt`, `.github/workflows/build.yml`, `test/equivalence/README.md`.

**Interfaces:** `replay(contract_path: Path, baseline_path: Path, capture_path: Path, probe_exe: Path, out_dir: Path) -> dict`; CLI `python -m scripts.equivalence.replay --contract PATH --baseline PATH --capture PATH --probe EXE --out-dir PATH`. All native probe execution uses argument lists and owned temporary paths.

- [ ] Test replay with fleet/MPQ imports disabled, a missing probe, paths containing spaces, changed input/capture hash, nonzero probe status with an output file present, reordered query IDs, and wrong output cardinality.
- [ ] Implement replay using only captured reference results, authored fixtures, and the candidate probe. Record candidate executable/source identity in the result and reject missing required capture metadata.
- [ ] Register `equivalence_palette_replay` with CTest label `portable` on all three platforms. Run targeted Python tests, build the probe, then `ctest --preset default -R 'equivalence_palette_replay|palette' --output-on-failure` and equivalent CI presets.
- [ ] Run one live revalidation against the same original baseline and a known-good candidate, followed by one controlled wrong candidate/result; archive pass and mismatch evidence under different run IDs.
- [ ] Document exact replay/capture commands and the finite tested domain, update the behavior ledger, and commit. Downstream plans consume the shared result/schema interfaces without inventing new verdict meanings.
