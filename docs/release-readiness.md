# Release readiness review

Started 2026-10-21; rewritten 2026-10-22 to the state after the first day of
release-readiness work. It describes the working tree (commit `996bf5f` plus
uncommitted changes), not a tagged revision. No release has been published and
no version has been changed. It is an assessment of the evidence collected, not a
claim that every source line has been reviewed.

## Current verdict

**Hold release.** Much of yesterday's risk has been turned into tests and fixes,
but the release cannot be qualified yet. `python -m scripts.equivalence.qualify`
reports **NOT READY: 11 of 164 required entries qualify**
(`tmp/qualification/2026-10-22-working-tree/`; see the
[behavior ledger](testing/behavior-ledger.md)). The remaining blockers:

1. ~~**Golden reference decision.**~~ **Resolved 2026-10-22.** The maintainer
   kept the tile-rarity variant selection fix in `misc_check_tiles_conflicts`.
   `act1_town`, `act2_desert`, `act3_jungle`, `act4_fortress` and `act5_town`
   were regenerated from the pinned vanilla 1.13c inputs
   (`test/equivalence/profiles/visual-core.json`, no `mod_dir`, gamma 1.20,
   zoom 1:4, isolated preferences, two byte-identical renders); `act1_cave`
   already passed and is unchanged. Each new image equals the proposal in
   `tmp/visual-diagnosis/proposed/<map>/` pixel for pixel; rendering the old
   selection from vanilla reproduced the four old references exactly, and
   `act5_town`'s old reference was a PD2 overlay render. Tolerance is unchanged.
   `python scripts/run_golden_tests.py --core --tolerance 4` passes 6/6 and
   `render_cases run` passes all six cases against an owned vanilla runtime
   (`tmp/regen/`; evidence `tmp/visual-diagnosis/DIAGNOSIS.md`).
2. **Linux and macOS native build, test and package runs have not been done
   locally.** The Docker engine was not running, so nothing here has been built
   or tested on those platforms, and the macOS `.app` packaging fixes are
   unverified on a Mac. CI (`build.yml`) is written to do them and has not yet
   run the new steps.
3. **Windows clean-VM run** (no Visual C++ redistributable, no developer tools)
   has not been done. The package now ships `vcruntime140.dll`, which is a
   content check, not proof that a bare machine starts.
4. **Manual GUI checklist** ([native release checklist](testing/native-release-checklist.md))
   has not been run against a candidate package on any platform.
5. **Fullscreen exit hang on Direct3D (open defect).** The process does not exit
   after a fullscreen `--selftest`; see the table below. Fullscreen is
   deprioritised, but the defect is open.
6. **Coverage numbers are unavailable.** The instrumentation exists
   (`DS1EDIT_ENABLE_SANITIZERS`, `DS1EDIT_ENABLE_COVERAGE`) but no local tool
   produced a measurement; the CI coverage job will produce the first one. Only
   suite counts exist, and they are not coverage.
7. **Out-of-bounds objects: fixed, GUI check pending.** The editor dropped
   objects outside `width*5 x height*5` (80 objects in 38 vanilla maps). They
   are now kept, drawn with a warning tint and saved; regression tests pass
   and an offline recompare against the stored original-loader records gives
   2,276 of 2,276 identical (`tmp/equivalence/ds1-sweep-oob/`). Only the
   visual check of the tint and picking in the GUI remains (manual checklist).
8. **Qualification record.** No reviewed record exists in
   `release/qualification/`, and `release.yml` refuses to publish without one.
   The qualifier's current verdict is NOT READY (11/164).

Also open as a review concern (not a measured defect): removing the folder-Act
stamp in `area_browser_open_entry_into_slot` means an unlisted pre-v8 DS1 with
no preset and no inferable tileset now gets the Act 1 palette.

## What was verified (2026-10-22)

Evidence is local and gitignored under `tmp/`. Paths are relative to the
repository root.

| Check | Result | Evidence |
| --- | --- | --- |
| CTest inventory | About 70 suites (was 29), including `py_*` Python suites registered from `test/python` by `test/cmake/python.cmake` (label `portable`; a missing Python registers the failing `py_unavailable`). Python dependencies: `test/python/requirements.txt` (Pillow); local lane only: `test/equivalence/requirements-local.txt` (mpyq) | `tmp/rr2-all-ctest.log`, `tmp/build-pkg*-*.log` |
| Golden runner strictness | `scripts/run_golden_tests.py` exits 0 only if every selected map passes; 1 for a failure or skip; 2 for a bad configuration; artifacts in `tmp/golden-runs/run-*/results.json`; `--update` is atomic | `tmp/golden-runs/` |
| Palette nearest-colour vs the original game | Editor agrees with original D2CMP 1.13c on 336 of 336 vectors; replay is CTest `equivalence_palette_replay` | `tmp/equivalence/capture-palette-nearest-113c.json` |
| PL2 channel order | Red, green, blue, confirmed against the live game; `docs/guides/palettes-and-colour.md` corrected | `tmp/equivalence/` |
| Vanilla DS1 corpus sweep | All 2,276 vanilla 1.13c DS1 files read and resave cleanly; 2,238 identical to the original game loader; the other 38 were the out-of-bounds-object drop, now fixed: 2,276 of 2,276 identical | `tmp/equivalence/ds1-sweep/`, `tmp/equivalence/ds1-sweep-run.log` |
| Adversarial campaign | 6,717 malformed-input cases (CTest `adversarial_core`) found and fixed a DCC sub-stream overrun and two DT1 32-bit wraparounds | `tmp/adv-run1*`, `tmp/adv-run2*` |
| MSVC AddressSanitizer | Found and fixed a `dcc_destroy` overflow; the detector itself was checked with a known-bad control | `tmp/asan-detector/`, `tmp/build-asan*` |
| Golden diagnosis | The mismatch causes above, established by rendering old and new code | `tmp/visual-diagnosis/DIAGNOSIS.md` |
| Direct3D frame rate | Under one frame per second was the builtin-font glyph bitmaps staying MEMORY bitmaps; fixed with `al_convert_memory_bitmaps` after display creation. `--selftest` now prints the render average and fails above 250 ms per frame | `tmp/fps/` |
| Fullscreen Direct3D exit hang | Reproduced 4 of 4 times on `act1_town`; the main thread spins in `al_destroy_display` -> `d3d_destroy_display_internals` -> `_al_convert_to_memory_bitmap` | `tmp/fs-hang/` (`stack1.txt`, `logs/`) |
| Windows package | CPack ZIP now ships the x86 `vcruntime140.dll`; `assets/editor/version` is no longer packaged; `package_check.py` inspects it; `native_smoke.py` runs the extracted package | `tmp/native-smoke/`, `tmp/build-pkgF*` |
| Performance baseline | Baseline recorded for the Windows build | `tmp/perf/baseline-1` |
| Qualification | `qualify.py` verdict NOT READY, 11 of 164 required entries (the Windows package archive checks) | `tmp/qualification/2026-10-22-working-tree/` |

Not verified: any Linux or macOS build, test or package; the new `build.yml`
steps (validated for YAML syntax only); the clean-VM Windows start-up; the
manual GUI items; coverage percentages.

## Defects found and fixed

All fixes are in the working tree, uncommitted. "Fixed" means a regression test
exists and passes locally on Windows unless the Status column says otherwise.

| Area | Defect | How found | Status |
| --- | --- | --- | --- |
| Golden runner | Exit 0 with every map skipped; renderer failures accepted | Review, reproduced 2026-10-21 | Fixed (strict exit codes, artifacts) |
| DS1 read | Header fields used before size checks; unbounded filename reads; missing range checks | Source review | Fixed (`ds1_read_checked`) |
| DS1 save | Ignored `fwrite`/`fclose` failures; failed save was fatal | Source review | Fixed (`ds1_save_checked`: staging file, backup, rollback; failure reported, not fatal) |
| DS1 load | Objects outside `width*5 x height*5` dropped (38 vanilla files) | Corpus sweep against the original loader | Fixed; 2,276/2,276 identical; GUI check pending |
| Undo | 8-bit counts and coordinates: runs over 255 and maps wider than 256 broke | Direct undo tests | Fixed (32-bit) |
| Objects | Delete with no objects created a ghost object | Direct object/path tests | Fixed |
| Project | Copy-on-save path bugs: similar-prefix sibling dirs, path overflow, trailing separators; project copy did not find its preset | Review and tests | Fixed |
| DCC | 32-bit signed fields; crash in header validation; sub-stream overrun | Authored decoder cases; adversarial campaign | Fixed |
| DT1 | RLE overrun; offset crash; stale transparent pixels; two 32-bit wraparounds | Decoder cases; adversarial campaign | Fixed |
| AnimData | Lookup: extension, bucket position, overrun (names compared exactly after uppercasing, confirmed in D2Common) | Authored vectors against the documented rules | Fixed |
| Decoding | RGBA-to-index recovery path | Design review (package D) | Fixed (one raw index-frame path for DCC/DC6/DT1) |
| Export | APNG delay overflow; partial file left on failure; non-deterministic transparent PNG pixels | Export cases | Fixed |
| Memory | `ds1_free` leaks; `dcc_destroy` overflow; `mod_load_in_mem` prototype/argument mismatch (undefined behaviour), path buffer too small | Review; MSVC AddressSanitizer | Fixed |
| Corrupt maps | A corrupt map opened from the area browser exited the program | Adversarial/robustness work | Fixed (error shown) |
| Rendering | Direct3D under 1 FPS: builtin glyph bitmaps stayed MEMORY bitmaps | Frame-rate investigation | Fixed |
| Shutdown | Fullscreen Direct3D exit hang in `al_destroy_display` | `--selftest` with `full_screen = YES` | **Open.** Candidate fixes: destroy bitmaps before the display, or skip `al_destroy_display` at exit |
| Packaging | Windows ZIP lacked `vcruntime140.dll`; `assets/editor/version` packaged | Package inspection | Fixed (needs the clean-VM run) |
| Packaging | macOS: `.app` not in the package; empty `Info.plist` version | Source review | Fixed in source, **unverified on a Mac** |
| Palette | Colour-map cache keyed without palette hash and gamma; cached palette could override the archive palette; PL2 channel order unconfirmed | Review; live-game comparison | Fixed / confirmed |
| Tiles | Tile-variant rarity selection (`misc_check_tiles_conflicts`); block-table scan out of bounds; DT1 flags bit 0, `roof_y` and 16-bit sub-tile length | User's pending changes | Fixed in working tree |
| Input | **I**-key repeat condition; Ctrl+U not edge-triggered | User's pending changes | Fixed in working tree |
| Documentation | README listed a deleted test; BUILDING said CMake 3.20 and ran a Release-only test preset after a dev build | Review 2026-10-21 | Fixed in this revision |

Unreleased user-facing changes (also in `CHANGELOG.md`): arrow keys and WASD
scroll; Ctrl+arrows browse maps; edge-of-window mouse scrolling off by default
(`mouse_edge_scroll`); TXT-first palette per map; exact `LvlPrest` path for
unlisted maps; `.tg1` fallback tile loading; Ctrl+Z map undo; Alt+I strict
identical selection; Alt-drag object box select.

## Testing completeness

This is an inventory, **not a line or branch coverage percentage**. No coverage
measurement exists locally. Counts are approximate: about 70 CTest suites (was
29) after the Python suites were registered.

| Area | Current evidence | Significant gap |
| --- | --- | --- |
| Compose/export helpers | Many Unity suites, APNG structure checks, authored export cases verified with an independent decoder | Real MPQ integration |
| Palette/cache/DT1/DCC/DC6 | Unity suites, authored fixtures, original-game replay (336/336), adversarial campaign | Live revalidation of the decoder contract for this candidate |
| Selection, input, undo, objects, paths | Unity suites that run the real edit and undo code | GUI event integration (manual checklist, not run) |
| DS1 persistence | Read, checked save, backup, temporary save, write-failure suites; 2,276-file corpus sweep | GUI check of out-of-bounds objects; live DS1 contract lane |
| Project/preferences | File round-trip and the path fixes above | Save-path integration through GUI |
| CLI | Smoke cases; extracted-package `help` | More verbs and quoted paths |
| Rendering | 42 stored references; six core comparisons; `--selftest` with timing | Core references need a maintainer decision; no CI rendering |
| Packaging | CPack, `package_check.py`, `native_smoke.py` on Windows; CI now builds, packages and uploads candidate artifacts for all three platforms | Linux/macOS runs; Windows clean VM; macOS `.app` |
| Sanitizers and coverage | CMake options, MSVC ASan run, CI jobs written (Clang ASan+UBSan; GCC gcov JSON) | No Linux run; no coverage numbers |
| Performance | Baseline recorded locally on Windows | Linux/macOS baselines; reviewed budgets |
| Qualification | `qualify.py`, release workflow consuming a reviewed record | Evidence for 153 of 164 required entries |

## Next steps

1. Push the work and let `build.yml` run on all three platforms; fix what the
   first Linux and macOS runs show (`candidate-<platform>` artifacts and the new
   sanitizer and coverage jobs have never executed).
2. Check out-of-bounds objects in the GUI (tint, picking, move, delete) on one
   of the 38 affected vanilla maps.
3. ~~Decide the golden references.~~ Done: regenerated from pinned vanilla 1.13c
   inputs (see blocker 1); tolerance unchanged.
4. Fix the fullscreen Direct3D exit hang or document it as a known issue for the
   release.
5. Run the clean-VM Windows check and the manual
   [GUI checklist](testing/native-release-checklist.md) on each platform, and
   test the macOS `.app` on a Mac.
6. Collect the first coverage numbers from CI and decide which release-critical
   behaviour must be covered, rather than a percentage.
7. Qualify a CI-built candidate (`release/qualification/README.md`), review the
   record, commit it, then tag.
8. Resolve the review concern about the Act 1 palette for unlisted pre-v8 maps.

## Compatibility target and design

The maintainer confirmed vanilla Diablo II 1.13c as the compatibility target;
other game versions and mod distributions are outside the initial equivalence
baseline. Historical DS1 versions present in the vanilla 1.13c archives remain
part of it. Reference comparisons identify the original binaries and archives by
hash; the mixed local `assets/tiles/` tree is not a reference corpus. The
platforms (Windows x86, Linux x86_64, macOS arm64) ship together.

The [vanilla 1.13c testing design](superpowers/specs/2026-10-21-vanilla-113c-testing-design.md)
(approved 2026-10-21) and its [roadmap](superpowers/plans/2026-10-21-vanilla-113c-testing.md)
and six package plans define the behaviour matrix and the qualification profile.
A separate [software renderer design](superpowers/specs/2026-10-22-software-renderer-design.md)
and [phase 1 plan](superpowers/plans/2026-10-22-software-renderer-phase1.md)
cover running without 3D hardware; it is not part of this release's gating.
