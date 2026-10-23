# Reviewed qualification records

This directory holds one file per qualified release candidate:
`release/qualification/<source_state_digest>.json`. Each is a
`ds1edit.qualification/1` report with `ready: true`, written by
`python -m scripts.equivalence.release_record` (or `qualify`) and committed
only after a maintainer reviews it. `.github/workflows/release.yml` refuses to
publish without one.

Nothing in this directory is part of the source state it describes:
`source_state_digest()` excludes `release/qualification/`, so the record can
arrive in a metadata-only follow-up commit and the tag can point at that commit.

## Cutting a release, end to end

1. **Candidate.** Merge to `main` (or push the release commit) and let
   `build.yml` finish green. Each platform job uploads `candidate-<platform>`
   (package, manifest, `package-inspection.json`, `evidence/*.json`); the Linux
   sanitizer and coverage jobs upload `evidence-linux-sanitizers` and
   `evidence-linux-coverage`. Note the run ID.
2. **Local lanes.** The CI evidence covers 106 of the 164 required entries (see
   [What CI proves](#what-ci-proves)). The rest need your machine: the corpus
   and live lanes (`corpus.*`, `ds1-113c`, `visual.*`, the R6 live
   comparisons), the native workflows (`native.<platform>.*`) and the
   performance baselines (`perf.*`). Run them against exactly the packages and
   executables of that run and collect their `ds1edit.case/1` files in one
   directory. Entries you cannot produce are either waived (below) or block the
   release.
3. **Waivers (optional).** Write `waivers.json` (below) for the entries a
   maintainer accepts as not run.
4. **Record.** Check out the candidate commit, clean, and run

   ```
   python -m scripts.equivalence.release_record --run-id <RUN> --version X.Y.Z \
       --waivers waivers.json --evidence-dir <LOCAL_LANES_DIR> \
       --out release/qualification/
   ```

   It checks that the run is a successful `build.yml` run of this repository,
   downloads its artifacts with `gh`, combines the three platform manifests
   into the candidate, refuses a checkout that is not the candidate's source
   state, qualifies it against `test/equivalence/profiles/release-core.json`
   and, only when ready, writes `release/qualification/<digest>.json`. When it
   is not ready it writes nothing there and lists the blocking entries
   (`tmp/release-record/<run>/qualification-not-ready.json` holds the full
   report). Exit 0 ready, 1 not ready, 2 invalid input or unusable evidence.
   `--reference reference.json` supplies the contract/baseline/adapter digests
   the repository does not carry (local lanes); by default they come from the
   committed `ds1edit.contract/1` and `ds1edit.capture/1` files.
5. **Review** the record: the `waived` list, `limits`, the per-entry reasons.
   Commit it (on its own, or with the version bump if the digest is unaffected).
6. **Dry run.** With the record committed and the tag's checkout present:

   ```
   python -m scripts.equivalence.release_record --run-id <RUN> --version X.Y.Z \
       --out release/qualification/ --dry-run-release
   ```

   runs what `release.yml` will run (the same functions): finds the record for
   this checkout, checks the run, downloads the artifacts again, verifies the
   manifests, packages, evidence files and every required result against the
   tag `vX.Y.Z`, and writes the draft release notes to
   `tmp/release-record/<run>/RELEASE_NOTES.md`. Nothing is published.
7. **Tag** `vX.Y.Z`. `release.yml` verifies again, stages the exact candidate
   packages, and creates a *draft* release whose body is the generated notes
   (qualification identity, package hashes and the **Waived checks** section).
   You publish the draft yourself.

If the candidate run's artifacts expire, build and qualify a new candidate;
changing any source input, package, contract, baseline, adapter or the profile
invalidates a record: requalify instead of editing it.

## What a record binds

| Field | Meaning |
| --- | --- |
| `source_state_digest` | Canonical digest of the declared build inputs (below). Also the file name. |
| `commit` | The Git commit the candidate build ran on (`build.yml`'s `head_sha`). |
| `candidate_run_id` | The successful `build.yml` run whose artifacts are released. |
| `release_version` | The version qualified; waivers expire with it. |
| `package_hashes` | SHA-256 of each platform's CPack package from that run. |
| `candidate` | The full `ds1edit.candidate/1` manifest: executables, build, approved contract/baseline/adapter digests, profile digest. |
| `profile` | ID and digest of `test/equivalence/profiles/release-core.json`. |
| `required` | Every required `id@lane/platform`, its status, reasons and whether it was waived. |
| `waived` | The waived entries, each with the waiver (case id, reason, reviewer, expires_version). Listed apart from `required`'s qualifying entries. |
| `review_dispositions` | Exact case/issue IDs accepted by review (performance, defect), each with rationale, evidence and reviewer. |
| `results` | Every evidence object the verdict was computed from (cases, review dispositions and waivers, defect lists). |

The release workflow recomputes the verdict from `results` with the tagged
profile and refuses the record if it is not ready, if its per-case results or
waived checks differ from the recomputation, if its digest does not match its
content, if the record was qualified for another version, if any evidence file
in a candidate artifact is not part of the reviewed record, or if any
downloaded package hash differs. Bulk evidence (screenshots, captures,
extracted packages, logs) stays in the retained local evidence bundle, not here.

## Waiver policy

A waiver is the maintainer saying "release without this check". It is the only
way to release with a required entry unqualified, and it is deliberately narrow.

```json
{
  "schema": "ds1edit.review/1",
  "dispositions": [
    {"kind": "waiver", "id": "corpus.ds1.read-all", "platform": "windows-x86",
     "reason": "no vanilla 1.13c install on the release machine this time",
     "reviewer": "maintainer", "expires_version": "2.2.1"}
  ]
}
```

(A bare list of such objects is accepted by `release_record --waivers` too.)

* **Exact.** `id` is one exact case ID (after `{platform}` expansion) and
  `platform` names the platform; wildcards (`* ? [ ]`) and surrounding
  whitespace are invalid, as is a waiver for an entry the profile does not
  require. Optional `lane` narrows it. `reason`, `reviewer` and
  `expires_version` are required and non-blank.
* **Per release.** `expires_version` must equal the version being released
  (`--version`, the tag without `v`); a waiver for another version is
  invalid, so a waiver can never carry over to the next release. A waiver with
  no release version to compare against is invalid.
* **Only for what never ran.** A waiver excuses a required entry only when its
  status is `missing`, `not_run` or `unavailable` (and, for a case that did
  not run, the consequences of that: no inputs compared, features or control
  not shown). It never excuses a `mismatch`, a `partial` result, a violated
  `rejected_expected`, a duplicate, or evidence that is stale for the
  candidate; such an entry stays blocking and the report lists the waiver
  under `rejected_waivers` with the reason. A waiver for an entry that
  qualifies is reported as unused.
* **Never these.** Waiving any of the following is invalid input (exit 2):
  every `package.*` check, `portable.ctest`, `portable.python-unittest`, the
  sanitizer lane (`sanitizer.*`, `control.sanitizer.*`), and any profile entry
  marked `"waivable": false`. A sanitizer failure is a mismatch and can never
  be waived either.
* **Visible.** `ready` stays true only if every non-waived required entry
  qualifies. The report lists waived entries apart (`waived`, `waivers`, the
  per-entry `waived` flag, `totals.*.waived`), and `release.yml` puts a
  **Waived checks** section (case ID, platform, status, reason, reviewer) into
  the draft release notes.

## What CI proves

`build.yml` turns each job's `ctest --output-junit` into `ds1edit.case/1`
evidence with `scripts/equivalence/ci_evidence.py` and uploads it in the
candidate artifact (`evidence/ci-<platform>.json`). Every case is bound to the
job's source digest and, where the profile requires an executable binding, to
the hash of the named test executable in that job's manifest. CI never claims
a local lane. For the portable and sanitizer entries of `release-core.json`:

| Required entry | Proved by |
| --- | --- |
| `portable.ctest` | every CTest of the job (count and failures from the JUnit file) |
| `portable.python-unittest` | the `py_*` CTests, with the counts `unittest` prints (skips are partial) |
| `palette-nearest-113c` (portable) | `equivalence_palette_replay`: the replay's own case, re-attested to the job |
| `editor.undo.tiles` | CTest `editor_undo` |
| `editor.edits.objects-paths` | CTest `object_path_edits` |
| `editor.map-isolation` | Unity `test_map_slots_keep_independent_histories` (`editor_undo`), `test_object_undo_is_per_map` (`object_path_edits`) |
| `ds1.save.backup` | `ds1_save`: numbered backup, existing slot left alone, failed backup rename, publish rollback |
| `ds1.save.tmp` | `ds1_save`: tmp save beside the map, failed tmp save |
| `ds1.save.write-failure` | `ds1_save`: short write, tile byte, close, denied open, backup rename, publish and rollback, tmp save, copy-on-save, legacy wrapper |
| `project.save-redirection` | CTest `resource_resolution`; `ds1_save` copy-on-save; `project` redirect tests |
| `ds1.format.matrix` | CTest `ds1_format` and `ds1_cases` (`test_reader_matches_every_authored_expectation`); the version features are the versions the fixture generator really contains |
| `dc6.*`, `dcc.*` (7 entries) | the per-case lines of `visual_reference_probe_decoder_cases` |
| `export.images` | `asset_export_smoke`, `visual_export_images` (png, apng, delays, transparency), `export_progress` (cancellation) |
| `adversarial.core` | `adversarial_core`, `adversarial_regressions`, `adversarial_codecs` |
| `control.palette-nearest-113c.*` (4), `control.visual-values-113c.seven-bit-mask`, `control.decoders-113c.*` (2), `control.compose.priority-reversal`, `control.export.wrong-delay` (Windows) | the named `unittest` methods of `py_equivalence_controls`, `py_visual_values`, `py_decoder_cases`, `py_export_images`, which feed a deliberately wrong model to the real comparison and require a difference |
| `sanitizer.core` (Linux) | the whole `-L portable` CTest run under ASan + UBSan |
| `control.sanitizer.detector` (Linux) | CTest `sanitizer_detector_control`: a seeded heap overflow that passes only when ASan reports it (`test/cmake/sanitizers.cmake`) |
| `coverage.production` (Linux) | the `ds1edit.coverage/1` report of the coverage job. A readable report is the evidence the entry asks for (`kind: report`); partial coverage is passed with the uninstrumented modules disclosed in the case, never hidden. An unreadable report is `unavailable` |
| `package.<platform>.*` | `package_check` (`package-inspection.json`) |

`ci_evidence` fails closed: a failing check is a `mismatch`, a check that did
not run or was ignored leaves the case `partial`, none run is `not_run`. A test
rename breaks `test_ci_evidence` (every named check must exist), not a release.

**Required entries no CI test proves**, which therefore stay missing until a
local lane (or a waiver) supplies them:

* `visual-values-113c` and `decoders-113c` (portable, all platforms): the
  reference comparison needs a replay adapter and a capture of the original for
  those contracts; the repository has neither (only palette-nearest has).
* the local lanes: `corpus.*` (10), `ds1-113c` and the R6 live comparisons plus
  `control.ds1-113c.*` (6), `visual.*` (in the corpus lane), `native.<platform>.*`
  (21) and `perf.*` (15).

## Source state

`source_state_digest(root)` hashes, in sorted order, the relative name and Git
blob ID of every declared build input: `src/`, `assets/ui/`, `assets/editor/`
(without the generated `pal*.bin`, `cmap*.bin`, `gamma.dat` and `version`),
the top-level `assets/*.ini` that CMake globs, `Ds1edit.ini.sample`,
`allegro5.cfg`, `cmake/`, `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`,
`test/`, `scripts/`, `tools/d2mpq.py`, `LICENSE`, `NOTICE` and `README.md`.
Python caches, `.git/` and this directory are excluded. A `${CMAKE_SOURCE_DIR}`
reference in the CMake files to anything outside that set is rejected as an
undeclared dependency.

In a Git checkout a tracked file contributes its **committed** blob ID
(`git ls-files -s`), not a hash of whatever the working tree holds, so
`core.autocrlf`, `core.eol` and editors cannot change it: Windows, Linux and
macOS checkouts of one commit agree. Only a tracked file that was really edited
and an untracked-but-declared input are hashed from their raw bytes
(`--no-filters`), which is why a dirty tree has a different digest from the CI
checkout of its commit. `.gitattributes` keeps the committed text inputs LF
(`*.ini`, sources, scripts, CMake, JSON, YAML); `assets/editor/obj.txt` is
stored as committed (its bytes are pinned by `visual-core.json`). Qualify a
candidate that CI built from committed sources.

## What `build.yml` must upload

Per platform job (`windows-x86`, `linux-x86_64`, `macos-arm64`), after CPack,
`package_check` and the CI evidence step, one artifact named
`candidate-<platform>` containing exactly:

- the CPack package, unrenamed: `DS1Edit-<version>-win32.zip`,
  `DS1Edit-<version>-linux-x86_64.tar.gz` or `DS1Edit-<version>-macos-arm64.tar.gz`;
- `candidate-<platform>.json`: a sealed `ds1edit.candidate-platform/1` from
  `qualify.platform_manifest(platform, package, executables, run_id, source, build)`
  where `run_id` is `${{ github.run_id }}`, `source` is
  `{commit, source_state_digest, dirty, dirty_sha256}` computed in that job
  before building, `build` names the configuration and compiler, and
  `executables` maps names to every binary whose results the record will cite
  (the packaged `ds1edit` and every `test_*` executable, which the evidence
  binds to);
- `package-inspection.json` from `package_check` (optional but recommended);
- `evidence/*.json` from `ci_evidence` (optional; `ds1edit.case-set/1`).

The sanitizer and coverage jobs upload `evidence-linux-sanitizers` and
`evidence-linux-coverage` (`ds1edit.case-set/1`), which `release_record`
downloads next to the candidates; they are not part of the released artifacts.

`release.yml` rejects extra files in the candidate artifacts (only the package,
its manifest, `package-inspection.json` and an `evidence/` directory of `.json`
files are allowed), evidence files that are not part of the reviewed record, a
manifest whose run ID, commit, source digest, package or executables differ from
the record, and a package name whose version is not the tag's.
