# Behavior ledger: release-core profile

This ledger maps each release requirement in the approved design
([section 10](../superpowers/specs/2026-10-21-vanilla-113c-testing-design.md#10-first-release-qualification-profile))
to the case IDs that must pass, the lane and platforms that must run them, and
what evidence exists today. The machine-readable source of truth is
[`test/equivalence/profiles/release-core.json`](../../test/equivalence/profiles/release-core.json)
(`ds1edit.release-profile/1`, 164 required `id@lane/platform` entries);
[`scripts/equivalence/qualify.py`](../../scripts/equivalence/qualify.py)
evaluates it. If this page and the profile disagree, the profile wins and this
page is out of date.

A row is "missing" until a tool emits a `ds1edit.case/1` with that exact ID,
lane and platform, bound to the candidate. A source review, a byte match, a
passing suite on another build, or an empty run does not count.

## How a case is judged

Each required entry qualifies only when all of these hold:

- exactly one result has that ID, lane and platform, and is marked required;
- its status equals the profile's success outcome (`pass`, or
  `rejected_expected` for declared invalid-input cases) and the case declares
  the same outcome;
- it is bound to the candidate: source state (digest, or commit plus dirty-diff
  digest), an executable recorded for that platform, and/or that platform's
  package hash, as the profile's `binding` requires;
- contract, baseline and adapter digests equal the candidate's approved ones
  where the profile requires them; any recorded baseline must match;
- coverage: at least one distinct input, no uncovered required axis, not
  truncated or capped, complete accounting where required
  (`inputs_enumerated == inputs_accounted > 0`, nothing unclassified), every
  declared feature in `features_covered`, `control_detected: true` for
  negative controls, and a reviewed performance comparison.

Candidate-wide (requirement 10): the candidate approved this profile digest,
its packages were built from its own source state, it has a package for every
profile platform, and no open defect is unresolved. Data-loss defects cannot be
waived. Review dispositions name exact case or issue IDs with rationale,
evidence and reviewer; they apply only to performance comparisons and
non-data-loss defects, never to a required mismatch.

## Current measured state

Measured 2026-10-22 against the working tree (commit `996bf5f` plus uncommitted
changes, source state `981a50f4...`) with
`python -m scripts.equivalence.qualify`; report in
`tmp/qualification/2026-10-22-working-tree/qualification.json`, blocking list in
`blocking-ids.txt` beside it.

**Verdict: not ready.** 11 of 164 required entries qualify; 153 block (152
missing, 1 partial). Candidate-wide blockers: `unattested_build` (nothing
records which source state `tmp/build-pkgF` was built from) and
`candidate_missing_platform` (no Linux or macOS package was available locally).

| Lane / platform | Required | Qualifying |
| --- | ---: | ---: |
| portable / windows-x86 | 31 | 0 |
| portable / linux-x86_64 | 22 | 0 |
| portable / macos-arm64 | 22 | 0 |
| corpus / windows-x86 | 10 | 0 |
| live / windows-x86 | 6 | 0 |
| native-package / windows-x86 | 11 | 11 |
| native-package / linux-x86_64 | 11 | 0 |
| native-package / macos-arm64 | 12 | 0 |
| native-workflow / each platform | 7 x 3 | 0 |
| sanitizer / linux-x86_64 | 3 | 0 |
| performance / each platform | 5 x 3 | 0 |

## Requirements

Status key: **qualifies** (bound, passing evidence in the report above),
**evidence, unbound** (a real result exists but cannot be bound to a
candidate), **missing** (no case emitted).

### R1. Portable tests pass on all three platforms

| Case ID | Lane / platforms | Owner | Status |
| --- | --- | --- | --- |
| `portable.ctest` | portable / win, linux, mac | A | missing: no CTest run bound to a candidate build (`tmp/build-pkgF` has no test targets built) |
| `portable.python-unittest` | portable / win, linux, mac | A | windows-x86 **partial**: 248 run, 0 failed, 1 skipped (`test_ds1_cases.ProbeIntegrationTests`, `DS1EDIT_PROBE_EXE` not set). Linux/macOS missing |
| `palette-nearest-113c` | portable / win, linux, mac; contract+baseline+adapter | B | **evidence, unbound**: replay passes 336/336 distinct inputs, but `replay.platform_id()` labels it `windows-amd64` (not `windows-x86`) and its probe (`tmp/build-pkgB`) was built from an unrecorded source state |
| `control.palette-nearest-113c.{constant,channel-swap,last-tie,relative-index}` | portable / win | B | missing: the controls run inside `test_equivalence_controls.py` but emit no `ds1edit.case/1` with `control_detected` |

### R2. Corpus manifest accounts for every catalogued DS1

| Case ID | Lane / platforms | Owner | Status |
| --- | --- | --- | --- |
| `corpus.baseline.manifest` | corpus / win; baseline; complete accounting | B | missing (a local baseline manifest exists under `tmp/equivalence/`, but no case result) |
| `corpus.ds1.read-all` | corpus / win; baseline; complete accounting | C task 4 | missing |
| `corpus.ds1.assets-resolved` | corpus / win; baseline; complete accounting | C task 4/5 | missing |

### R3. DS1 version/feature matrix and game-side writer features

| Case ID | Lane / platforms | Coverage dimension | Owner | Status |
| --- | --- | --- | --- | --- |
| `ds1.format.matrix` | portable / all three | versions v2-v10, v12-v16, v18, optional-tail | C task 1 | missing |
| `ds1-113c` | live / win; contract+baseline+adapter | tile-layers, orientations, objects, paths, groups-tags, format-upgrade, edited-values | C task 4 | missing |
| `control.ds1-113c.omitted-field`, `control.ds1-113c.wrong-layer-order` | live / win | control detected | C task 4 | missing |

### R4. Editing, undo, isolation, backup, temporary save, redirection, write failure

| Case ID | Lane / platforms | Owner | Status |
| --- | --- | --- | --- |
| `editor.undo.tiles` | portable / all three | C task 3 | missing |
| `editor.edits.objects-paths` | portable / all three | C task 3 | missing |
| `editor.map-isolation` | portable / all three | C task 3 | missing |
| `ds1.save.backup` | portable / all three | C task 2 | missing |
| `ds1.save.tmp` | portable / all three | C task 2 | missing |
| `project.save-redirection` | portable / all three | C task 5 | missing |
| `ds1.save.write-failure` | portable / all three (also R7) | C task 2 | missing |

### R5. Representative visual set, decoder/composition features, six core maps

| Case ID | Lane / platforms | Coverage dimension | Owner | Status |
| --- | --- | --- | --- | --- |
| `visual.core.{act1_town,act1_cave,act2_desert,act3_jungle,act4_fortress,act5_town}` | corpus / win; baseline | one pinned render each | D task 4 | missing (the golden runner's `ds1edit.golden/1` results carry no candidate or baseline identity and are not accepted; five of six previously mismatched; diagnosed 2026-10-22: act1_town, act2_desert, act3_jungle and act4_fortress differ only through the intentional tile-rarity variant selection fix in `misc_check_tiles_conflicts`, whose old references encode the old defect, and act5_town's reference was made with a PD2 mod overlay; references NOT regenerated, proposal in `tmp/visual-diagnosis/`) |
| `visual.representative` | corpus / win; baseline | act1-act5, dt1-isometric, dt1-rle, dt1-animation, dcc, dc6, palette-act, palette-transparency, animdata, cof-priority, cof-blend, compose-offsets, export-png, export-apng | D task 4 | missing |
| `dc6.rle.basic`, `dc6.multi_direction`, `dc6.clip_and_zero_copy`, `dcc.single_frame`, `dcc.sequence`, `dcc.no_equal_cells`, `dcc.empty_direction` | portable / all three | authored frames | D task 2 | missing (`decoder_cases verify` reports its own dicts, not `ds1edit.case/1`) |
| `visual-values-113c` | portable / all three; contract+baseline+adapter | tile keys, AnimData, palette | D task 1 | missing |
| `decoders-113c` | portable / all three; contract+baseline+adapter | original decoder frames | D task 2 | missing |
| `export.images` | portable / all three | export-png, export-apng, delays, transparency, cancellation | D task 3 | missing |
| `control.visual-values-113c.seven-bit-mask`, `control.decoders-113c.wrong-origin`, `control.decoders-113c.dropped-delta-reuse`, `control.compose.priority-reversal`, `control.export.wrong-delay` | portable / win | control detected | D | missing |

### R6. Live revalidation of every required reference contract

| Case ID | Lane / platforms | Owner | Status |
| --- | --- | --- | --- |
| `palette-nearest-113c` | live / win; contract+baseline+adapter | B | missing (a historical live capture exists; replaying it is the portable lane, not live revalidation of this candidate) |
| `visual-values-113c` | live / win | D task 1 | missing |
| `decoders-113c` | live / win | D task 2 | missing |
| `ds1-113c` | live / win (shared with R3) | C task 4 | missing |

### R7. Malformed input, failed saves, sanitizer

| Case ID | Lane / platforms | Owner | Status |
| --- | --- | --- | --- |
| `adversarial.core` | portable / all three | E task 1 | missing as a bound case (the 6 717-case campaign ran locally as CTest `adversarial_core` and its defects are fixed; not yet bound to a candidate) |
| `ds1.save.write-failure` | portable / all three (shared with R4) | C task 2 | missing |
| `sanitizer.core` | sanitizer / linux-x86_64 | E task 2 | missing |
| `control.sanitizer.detector` | sanitizer / linux-x86_64 | E task 2 | missing |
| `coverage.production` | sanitizer / linux-x86_64 | E task 2 | missing |

### R8. Extracted packages and native workflows

| Case ID | Lane / platforms | Owner | Status |
| --- | --- | --- | --- |
| `package.<platform>.{archive,member_names,member_uniqueness,member_types,layout,required_members,forbidden_members,executable_header,executable_version,runtime_closure,extraction}` | native-package / all three | F task 1 | windows-x86 **qualifies** (all 11 pass on `DS1Edit-2.1.1-win32.zip`, sha256 `16cc91c6...`), though the candidate as a whole is unattested; Linux/macOS missing |
| `package.macos-arm64.macos_app` | native-package / macos-arm64 | F task 1 | missing |
| `native.<platform>.{cli-help,no-game-startup,selftest,clean-environment,gui-edit-undo-save-reopen,gui-modal-input,platform-startup}` | native-workflow / all three | F task 2 | missing as qualifying cases (`scripts/equivalence/native_smoke.py` now exists; no run bound to a candidate package yet; the clean-VM, GUI and Linux/macOS runs have not been done) |

### R9. Performance baselines and reviewed regressions

| Case ID | Lane / platforms | Owner | Status |
| --- | --- | --- | --- |
| `perf.{first-open,map-switch,decode-repeat,export-repeat,shutdown}` | performance / all three; comparison must be `baseline`/`within_budget` or carry an exact review disposition | E task 3 | missing |

### R10. One report binds everything to the candidate

Enforced by `qualify()` itself rather than by cases: `profile_identity`,
`candidate_identity` (built-from source state equals the candidate's),
`package_hashes` (a package for every platform), and `defects`. Release
consumption (`.github/workflows/release.yml`) recomputes the verdict from the
reviewed record, checks the candidate run and commit, and verifies every
downloaded package hash. Currently **not met** (see candidate-wide blockers above).

## Known gaps in the evidence producers

These are reasons current results cannot qualify even when they pass; each
belongs to the package that owns the producer.

- `replay.platform_id()` reports the Python host (`windows-amd64`), not the
  editor build platform (`windows-x86`); a mapping is a decision for package B.
- `replay.source_state()` records the source state at replay time, not the
  state the probe was built from; only the candidate manifest's
  `built_from_source_state_digest` (written by the build job) attests that.
- Golden screenshot results (`ds1edit.golden/1`), decoder verification and
  wrong-model controls do not yet emit `ds1edit.case/1`.
- Linux and macOS packages, native workflow runs, the corpus/live lanes for
  this candidate, sanitizer/coverage and performance runs have not been
  produced.
