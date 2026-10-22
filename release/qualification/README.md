# Reviewed qualification records

This directory holds one file per qualified release candidate:
`release/qualification/<source_state_digest>.json`. Each is a
`ds1edit.qualification/1` report with `ready: true`, written by
`python -m scripts.equivalence.qualify` and committed only after a maintainer
reviews it. `.github/workflows/release.yml` refuses to publish without one.

Nothing in this directory is part of the source state it describes:
`source_state_digest()` excludes `release/qualification/`, so the record can
arrive in a metadata-only follow-up commit and the tag can point at that commit.

## What a record binds

| Field | Meaning |
| --- | --- |
| `source_state_digest` | Canonical digest of the declared build inputs (below). Also the file name. |
| `commit` | The Git commit the candidate build ran on (`build.yml`'s `head_sha`). |
| `candidate_run_id` | The successful `build.yml` run whose artifacts are released. |
| `package_hashes` | SHA-256 of each platform's CPack package from that run. |
| `candidate` | The full `ds1edit.candidate/1` manifest: executables, build, approved contract/baseline/adapter digests, profile digest. |
| `profile` | ID and digest of `test/equivalence/profiles/release-core.json`. |
| `required` | Every required `id@lane/platform`, its status and reasons. |
| `results` | Every evidence object the verdict was computed from (cases, review dispositions, defect lists). |
| `review_dispositions` | Exact case/issue IDs accepted by review, each with rationale, evidence and reviewer. |

The release workflow recomputes the verdict from `results` with the tagged
profile and refuses the record if it is not ready, if its per-case results
differ from the recomputation, if its digest does not match its content, or if
any downloaded package hash differs. Bulk evidence (screenshots, captures,
extracted packages, logs) stays in the retained local evidence bundle, not here.

## Source state

`source_state_digest(root)` hashes, in sorted order, the relative name and Git
blob ID of every declared build input: `src/`, `assets/ui/`, `assets/editor/`
(without the generated `pal*.bin`, `cmap*.bin`, `gamma.dat` and `version`),
the top-level `assets/*.ini` that CMake globs, `Ds1edit.ini.sample`,
`allegro5.cfg`, `cmake/`, `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`,
`test/`, `scripts/`, `tools/d2mpq.py`, `LICENSE`, `NOTICE` and `README.md`.
In a Git checkout it enumerates tracked plus non-ignored untracked files and
lets `git hash-object` apply the checkout's line-ending conversion, so a
Windows `autocrlf` checkout and a Linux checkout of the same commit agree.
Python caches, `.git/` and this directory are excluded. A `${CMAKE_SOURCE_DIR}`
reference in the CMake files to anything outside that set is rejected as an
undeclared dependency.

A local working tree with uncommitted or untracked inputs has a different
digest from the CI checkout of its commit. Qualify a candidate that CI built
from committed sources.

## Producing a record

1. Push the candidate commit and let `build.yml` finish successfully. Each
   platform job uploads an artifact `candidate-<platform>` (see below).
2. Download those artifacts, combine the three platform manifests with
   `qualify.combine_candidate(...)` (supplying the approved contract, baseline
   and adapter digests and the profile digest), and save `candidate.json`.
3. Run the portable, corpus, live, native, sanitizer and performance lanes
   against exactly those packages/executables, collecting every
   `ds1edit.case/1` (or `ds1edit.case-set/1`, `ds1edit.package_inspection/1`)
   plus any `ds1edit.review/1` and `ds1edit.defects/1` files into one flat
   directory.
4. `python -m scripts.equivalence.qualify --profile test/equivalence/profiles/release-core.json --candidate candidate.json --results-dir RESULTS --out qualification.json`
   exits 0 ready, 1 not ready, 2 invalid evidence.
5. Review the report. If ready, copy it to
   `release/qualification/<source_state_digest>.json` and commit it on its own.

Changing any source input, package, contract, baseline, adapter or the profile
invalidates the record; requalify instead of editing it. If the candidate run's
artifacts expire, build and qualify a new candidate.

## What `build.yml` must upload

Per platform job (`windows-x86`, `linux-x86_64`, `macos-arm64`), after CPack
and `package_check` pass, one artifact named `candidate-<platform>` containing
exactly:

- the CPack package, unrenamed: `DS1Edit-<version>-win32.zip`,
  `DS1Edit-<version>-linux-x86_64.tar.gz` or `DS1Edit-<version>-macos-arm64.tar.gz`;
- `candidate-<platform>.json`: a sealed `ds1edit.candidate-platform/1` from
  `qualify.platform_manifest(platform, package, executables, run_id, source, build)`
  where `run_id` is `${{ github.run_id }}`, `source` is
  `{commit, source_state_digest, dirty, dirty_sha256}` computed in that job
  before building, `build` names the configuration and compiler, and
  `executables` maps names to every binary whose results the record will cite
  (the packaged `ds1edit`, and the test probes used by portable replays);
- `package-inspection.json` from `package_check` (optional but recommended).

`release.yml` rejects extra files in these artifacts, a manifest whose run ID,
commit, source digest, package or executables differ from the record, and a
package name whose version is not the tag's.
