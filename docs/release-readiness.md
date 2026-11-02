# Release readiness review

Started 2026-10-21; updated 2026-11-02 for the next release, DS1Edit **2.2.1**.
It is an assessment of the evidence collected, not a claim that every source line
has been reviewed. The "What was verified" table below records the 2026-10-22
evidence and has not been re-dated.

## Current verdict

**2.2.1 is in preparation and has no candidate yet.** Scope: what is on `main`
(the workable release gate, the `--selftest` fix), any fixes from
`release/113c-testing` that `main` lacks, and closing the gaps 2.2.0 shipped with.
The software renderer is not part of it; it is planned as 2.3.0.

2.2.0 was published on 2026-10-23 (tag `v2.2.0` at `3c72c16`). Its packages came
from `build.yml` run 39999757475 and were uploaded by hand, not through
`release.yml`, because the qualification gate could not pass at the time. No
qualification record exists for it.

Open for 2.2.1:

1. **`release/113c-testing` is triaged; two decisions remain.** The branch has 20
   commits that are not on `main` and conflicts with it in 97 files. `main`
   reimplemented its save, DS1 reader and DT1 and DC6 fixes. The `txt_read_in_mem`
   use-after-free and the DCC frame geometry check are ported, and saving a map older
   than version 4 is now refused because the writer would drop its tag layer. Still
   open: the small hardening items (path editing with no object, the duplicate COF layer
   check, `misc_load_mpq_file` input guards).
2. **macOS GUI launch is untested.** CI builds, packages, signs (ad hoc) and
   inspects `ds1edit.app`, but nobody has launched it on a Mac, so Input
   Monitoring, Gatekeeper behaviour and the window itself are unverified.
3. **Manual GUI checklist not run.** The
   [native release checklist](testing/native-release-checklist.md) has not been
   run against a candidate package on any platform. This includes the visual
   check of out-of-bounds objects (tint, picking, move, delete) on one of the
   38 affected vanilla maps.
4. **Windows clean-VM run not done** (no Visual C++ redistributable, no
   developer tools). The package ships `vcruntime140.dll`, which is a content
   check, not proof that a bare machine starts.
5. **Fullscreen exit hang on Direct3D: likely fixed, unverified.** Bitmaps are
   now freed before `al_destroy_display` (the reproduced hang was inside that
   call), but the fullscreen `--selftest` exit has not been re-run to confirm.
   The README keeps it as a known issue until it is.
6. **Qualification record pending.** A CI run supplies 106 of the 164 required
   entries (dry run on run 40032739659, 0 mismatches). The other 58 must be run
   locally or waived; see [Gate entries CI does not produce](#gate-entries-ci-does-not-produce).

Closed since 2.2.0: the intermittent early exit under parallel selftest load (the
frame-time rule compared wall time with 250 ms on a loaded host; it now uses the
main thread's CPU time per frame) and the gate problems that blocked `release.yml`
(line-ending digests, no CI evidence, no waiver policy).

## Gate entries CI does not produce

Waivers are per release and only cover entries that never ran
(`release/qualification/README.md`).

| Entries | Count | Where they can run |
| --- | --- | --- |
| `corpus.*`, `visual.*` | 10 | Windows host with vanilla 1.13c data |
| `ds1-113c`, R6 live comparisons, `control.ds1-113c.*` | 6 | Windows host with the live game |
| `native.windows-x86.*` | 7 | Windows host, except `clean-environment` (clean VM) and the two `gui-*` entries (a person) |
| `perf.*` on Windows | 5 | Windows host |
| `native.linux-x86_64.*`, `perf.*` on Linux | 12 | Needs a Linux desktop |
| `native.macos-arm64.*`, `perf.*` on macOS | 12 | Needs a Mac |
| `visual-values-113c`, `decoders-113c` (portable) | 6 | No replay adapter or capture exists; waive, or build them |

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

"Fixed" means a regression test
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
| Shutdown | Fullscreen Direct3D exit hang in `al_destroy_display` | `--selftest` with `full_screen = YES` | Bitmaps are now freed before the display; **unverified** in fullscreen |
| Packaging | Windows ZIP lacked `vcruntime140.dll`; `assets/editor/version` packaged | Package inspection | Fixed (needs the clean-VM run) |
| Packaging | macOS: `.app` not in the package; empty `Info.plist` version | Source review | Fixed in source, **unverified on a Mac** |
| Palette | Colour-map cache keyed without palette hash and gamma; cached palette could override the archive palette; PL2 channel order unconfirmed | Review; live-game comparison | Fixed / confirmed |
| Tiles | Tile-variant rarity selection (`misc_check_tiles_conflicts`); block-table scan out of bounds; DT1 flags bit 0, `roof_y` and 16-bit sub-tile length | User's pending changes | Fixed |
| Input | **I**-key repeat condition; Ctrl+U not edge-triggered | User's pending changes | Fixed |
| Documentation | README listed a deleted test; BUILDING said CMake 3.20 and ran a Release-only test preset after a dev build | Review 2026-10-21 | Fixed in this revision |

User-facing changes for 2.2.0 are in `CHANGELOG.md`; the input changes are: arrow keys and WASD
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
| Rendering | 42 stored references; six core comparisons (regenerated, passing); `--selftest` with timing | No CI rendering (needs a game install) |
| Packaging | CPack, `package_check.py`, `native_smoke.py` on Windows; CI now builds, packages and uploads candidate artifacts for all three platforms | Linux/macOS runs; Windows clean VM; macOS `.app` |
| Sanitizers and coverage | CMake options, MSVC ASan run, CI jobs written (Clang ASan+UBSan; GCC gcov JSON) | No Linux run; no coverage numbers |
| Performance | Baseline recorded locally on Windows | Linux/macOS baselines; reviewed budgets |
| Qualification | `qualify.py`, release workflow consuming a reviewed record | 58 of 164 required entries need local lanes or waivers |

## Next steps

1. Decide on the small hardening items from the `release/113c-testing` triage.
2. Re-run a fullscreen Direct3D `--selftest` to confirm the exit hang is gone,
   then drop the README's known-issue note (or keep it if it is not).
3. Run the clean-VM Windows check and the manual
   [GUI checklist](testing/native-release-checklist.md) on each platform
   (including out-of-bounds objects), and launch the macOS `.app` on a Mac.
4. Read the coverage numbers from the CI coverage job and decide which
   release-critical behaviour must be covered, rather than a percentage.
5. Build a candidate, run the local lanes, write the waivers for the rest,
   review and commit the record, then tag and let `release.yml` draft the release
   (`release/qualification/README.md`).

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
cover running without 3D hardware; that work is in development and is not part of
2.2.0 or its gating.
