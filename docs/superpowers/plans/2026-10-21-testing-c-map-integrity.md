# Map, Editing, and Persistence Integrity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish that vanilla maps load correctly, real editing/undo preserves state, and saved edits survive independent game-side interpretation and controlled I/O failures.

**Architecture:** Exercise the actual DS1/editing implementations with authored minimal fixtures and isolated editor state. Use explicit canonical snapshots for persistence comparisons and a dedicated original-game adapter for independent validation. Introduce only narrow production seams needed to observe errors and control I/O; avoid a general editor rewrite.

**Tech Stack:** C/Unity/CTest, Python fixture generation/orchestration, package B's contracts and transport, existing Allegro memory-mode support.

**Spec:** [Approved design](../specs/2026-10-21-vanilla-113c-testing-design.md), matrix map/edit/project/resource rows and package C. Depends on A; original comparisons consume [B](2026-10-21-testing-b-equivalence.md).

## Global Constraints

- Include the historical DS1 formats that ship in vanilla 1.13c, plus authored threshold-boundary fixtures.
- Real DS1 parsing, saving, editing, and undo are under test; never replace them with successful stubs.
- Unknown stored fields need preservation checks without invented semantics. Declare intentional writer transformations before comparison.
- All files, backups, and fault injection operate under owned temporary roots; never touch original MPQs or source maps.
- Graceful rejection of dangerous inputs is an editor requirement; known vanilla short optional tails have narrowly identified compatibility cases.
- Reuse B's exact `ds1edit.case/1` statuses and provenance; unavailable original-loader state cannot count as a pass.

## Review Focus

- An old-format reader and writer can agree with each other while both disagree with the game (tasks 1, 4).
- A failed close/rename can leave a plausible partial file and falsely cleared dirty state (task 2).
- Selection tests with stubbed undo do not establish actual restoration or map isolation (task 3).
- Object IDs and positions from different loader stages must not be compared without normalization (task 4).
- Basename collisions, explicit archive paths, path capacity, and overlays must not redirect a user's save to the wrong source (tasks 2, 5).

## Task 1: Authored DS1 matrix and canonical production snapshots

**Files:** Create `test/fixtures/make_ds1_fixtures.py`, `test/support/editor_test_state.c/.h`, `test/test_ds1_format.c`, `test/test_ds1_reference_probe.c`, `scripts/equivalence/ds1_cases.py`, `test/python/test_ds1_cases.py`; modify `test/CMakeLists.txt`.

**Interfaces:**
- `editor_test_state_init(int map_count, int width, int height)` and `editor_test_state_destroy(void)` allocate/reset only owned test maps, globals, object/path storage, and scratch undo files. Non-tested rendering/asset-label hooks can be stubs; document them.
- Probe `test_ds1_reference_probe dump INPUT.ds1 --out snapshot.json` calls real `ds1_read`; snapshot schema `ds1edit.map/1`: `version`, `width`, `height`, `act`, layer counts and per-cell raw properties/orientations, `objects`, `groups`, `paths`, plus uninterpreted fields the writer supports. Stable order is row/column/layer and source object/path order; no pointers or UI caches.
- `canonical_map(snapshot: dict, policy: dict) -> dict` applies only named B contract normalizations. Writer output version 18 is an explicit upgrade, not a raw-byte equality requirement.

- [ ] Generate small 3×2 fixtures at each relevant threshold's preceding/current version: 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 18. Cover missing act, orientation remap, skipped dword pair, tag/groups, path action, explicit floor count, and pre-group dword. Include multiple layers, zero objects, one object/path, distinct nonzero property bytes, and a named compatible optional-tail case. Generator assertions independently check expected byte offsets/lengths.
- [ ] Run fixture unit tests and new Unity cases red against production DS1 code. Check plus-one dimensions, no LP64 `long` on-disk stride assumptions, exact raw payloads, and distinct layer order. Investigate failures using runtime evidence when practical.
- [ ] Implement the state fixture, adapter snapshot writer, and minimal read-boundary fixes supported by failed tests. Trap expected errors in tests without invoking GUI error dialogs. A new bounds failure must stop before consuming or allocating from invalid lengths/counts.
- [ ] Run `ctest --preset default -R 'ds1_format|ds1_cases' --output-on-failure` and equivalent portable platform jobs; verify no runtime game install is consulted by authored cases.
- [ ] Commit task-owned fixtures/adapters/tests and any narrowly justified reader fix.

## Task 2: Checked, recoverable saving with I/O-failure tests

**Files:** Create `src/core/ds1_save_io.c/.h`, `test/test_ds1_save.c`; modify `src/core/ds1.c/.h`, `src/ui/interface.c` and any actual save callers found by `rg 'ds1_save\(' src`, and `test/CMakeLists.txt`.

**Interfaces:** Add `int ds1_save_checked(int ds1_idx, int is_tmp_file, char *error, size_t error_cap)`; preserve existing `void ds1_save(int,int)` as a compatibility/reporting wrapper until callers migrate. Internal `DS1_SAVE_IO` wraps open/write/close/rename/remove operations, with production defaults and a test-only supplied-ops entry `ds1_save_with_io(int,int,const DS1_SAVE_IO*,char*,size_t)`.

`DS1_SAVE_IO` uses function-pointer fields `open(const char*,const char*) -> FILE*`,
`write(const void*,size_t,size_t,FILE*) -> size_t`, `write_byte(int,FILE*) -> int`,
`close(FILE*) -> int`, `rename(const char*,const char*) -> int`, and
`remove(const char*) -> int`. Every serialization write, including the current
`fputc` tile-byte paths, must use the checked operations. Test close failures
must still close their owned file handles before reporting failure.

- [ ] Write failures-first tests for short write, failed close, denied open, failed destination publish, failed backup rename, and project-parent creation failure. Assert an existing destination's bytes are recoverable, failure is reported, no success/dirty/save-count/name transition occurs, and a later valid save still works. Also test fresh save, second save with numbered backup, existing backup slot, `.tmp`, paths with spaces, and copy-on-save leaving original bytes unchanged.
- [ ] Run the save suite red. Inspect actual save state mutations and every I/O result before deciding a fix; record existing naming/autosave behavior in tests rather than inventing new policy.
- [ ] Factor checked serialization and publish through a sibling staging file, validate every write and close, then perform the existing backup/publish policy with rollback on failure. Commit map name/counters/dirty transitions only after successful publish. Do not delete backups or user data during cleanup; remove only owned staging files.
- [ ] Update actual UI/CLI callers so failed saves cannot be presented as success. The legacy wrapper must not hide the checked result from callers that update state.
- [ ] Run save tests plus project/preferences and DS1 fixture suites. Read a successful output with an independent fixture parser and real `ds1_read`; compare all preserved fields, not just file existence/length.
- [ ] Commit the checked-save change and tests together after review.

## Task 3: Real tile/object/path editing and undo

**Files:** Create `test/test_editor_undo.c`, `test/test_object_path_edits.c`; extend `test/support/editor_test_state.*`; link real `src/editor/tiles.c`, `undo.c`, `objects.c`, and `paths.c` in `test/CMakeLists.txt`.

**Interfaces:** Use existing `edittile_*`, `undo_new_tile_buffer`, `undo_close_tile_buffer`, `undo_apply_tile_buffer`, `editobj_prepare_undo`, `editobj_del_obj`, `editobj_copy_obj`, and `editobj_undo`. For path edits driven through `editpath_draw`, drive existing input-edge state in tests; extract a narrowly named pure data operation only if otherwise untestable, retaining the production caller.

- [ ] Add a real delete/undo test on 3×2 maps with distinct floor/wall/shadow data; assert properties, flags, block indices, selection, and neighboring cells before/after. Add two successive edits/two undos, no-op boundaries, hidden layers, segment edges, and independent map slots. Do not use `test_editor_selection.c`'s undo stubs in this target.
- [ ] Add object create/copy/delete/move and path insert/move/delete tests with attached path data, selection flags, bounded positions, object identity, and object undo. Drive actual data-mutating functions; capture pre-edit snapshots and assert unaffected fields/maps remain equal.
- [ ] Run tests red, implement only defects demonstrated by those operations, then run green. If runtime GUI/global initialization is needed, use memory-mode test fixtures; document remaining native-only input scenarios for F rather than silently skipping them.
- [ ] Save and reload each mutated fixture with task 2's checked writer; assert restored/modified state matches the expected canonical snapshot.
- [ ] Run `ctest --preset default -R 'editor_selection|editor_undo|object_path_edits|ds1_save' --output-on-failure` and portable CI, review, and commit.

## Task 4: Original-game loader validation and catalogued-map sweep

**Files:** Create `scripts/equivalence/ds1_reference.py`, `test/equivalence/contracts/ds1-113c.json`, `test/python/test_ds1_reference.py`; extend B's baseline/adapter modules and local behavior ledger.

**Interfaces:** `compare_saved_map(input_path: Path, expected_map: dict, contract: dict, session, probe_exe: Path, out_dir: Path) -> dict`; CLI `python -m scripts.equivalence.ds1_reference --baseline PATH --cases PATH --probe EXE --out-dir PATH` uses an explicit declared live session when reference execution is selected.

- [ ] Locate original D2Common DS1 loader RVA `0xb560`, confirm actual custom entry/register/global state and initialization through call-site/disassembly/runtime evidence. Establish an invocation or natural-load observation recipe in the disposable game environment. Store the observed recipe and accessible outputs in the contract; partial observation cannot satisfy whole-map equivalence.
- [ ] Write mocked integration tests for wrong baseline/function bytes, unavailable state, absent reference fields, object-ID processing-stage mismatch, rejected new optional tail, and one altered saved object/path/tile. Assert required unavailable/partial results are not success.
- [ ] Implement the proven reference adapter using B's fleet seam. Compare canonical game-observable fields at the same processing stage; raw uninterpreted editor fields remain covered by preservation tests separately. Record native quirks with exact asset/contract identities and evidence.
- [ ] Run a live feature matrix covering every writer feature named in design section 10, including v18 upgrade, two floor/four wall layers, objects, flags, groups/tags, and paths/actions. Demonstrate omitted-field and wrong-layer-order controls fail.
- [ ] Enumerate every DS1 in B's manifest; invoke the real editor reader, resolve required effective TXT/DT1 assets, and record parsed dimensions/features/errors. Account for each input exactly once and classify all unresolved asset keys. Run original comparisons on the declared version/feature cases; do not claim every corpus map was game-compared unless it actually was.
- [ ] Persist per-case and aggregate results in B's schemas, commit code/contracts/portable expectations, and leave local corpus bytes and live captures under owned `tmp/` runs. If the native route is not feasible, report the exact missing capability and keep this required release item blocked.

## Task 5: Installation, MPQ/TXT precedence, and project-path integration

**Files:** Create `test/test_resource_resolution.c`, `test/test_d2install.c`; extend `test/test_project.c`, `test/test_mpq_index.c`, and `test/CMakeLists.txt`; modify production resource/install/project code only for failing cases.

- [ ] Test explicit MPQ paths are not overridden by automatic install resolution; missing/partial installs fail usefully; synthetic patch/base duplicates select the declared higher-priority bytes; user overlays override only in the overlay test lane; missing/malformed TXT columns have a clear bounded fallback; full virtual paths distinguish duplicate basenames.
- [ ] Add project path capacity, trailing separator, case normalization, spaces, and similar-prefix sibling directories. Feed the resulting save path through the actual checked DS1 writer and prove the original source remains unchanged.
- [ ] Run new tests red, use existing loaders' transport seams for synthetic archive bytes while retaining resolution logic, fix measured defects, and run green. Add local cases comparing the editor's effective resource hashes with B's independent MPQ manifest, including encrypted/compressed listfile handling. An unsupported listfile cannot be treated as a complete empty export/index.
- [ ] Run the whole portable suite plus the resource corpus comparison; update the ledger with distinct paths/archives and unresolved enumeration limits; review and commit.
