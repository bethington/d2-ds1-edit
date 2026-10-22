# Visual and Export Correctness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compare decoded assets, palette/animation behavior, composition, and exported images with identified vanilla 1.13c results, and resolve the existing screenshot mismatches.

**Architecture:** Compare metadata and raw palette-index buffers before RGBA conversion and final rendering. Reuse B's contracts/provenance and A's strict result/artifact handling. The representative visual case set maps every supported feature to a case ID; bulk frame sweeps are a separately reported extension.

**Tech Stack:** C/Unity/CTest, Allegro memory bitmaps, Python unittest/Pillow, zlib, B's original-game adapter and local MPQ manifest.

**Spec:** [Approved design](../specs/2026-10-21-vanilla-113c-testing-design.md), package D and visual/export rows. Depends on [B](2026-10-21-testing-b-equivalence.md), and [C](2026-10-21-testing-c-map-integrity.md) for map snapshots.

## Global Constraints

- Vanilla 1.13c reference bytes only; no mixed-tree or user-preference goldens.
- Exact indexed pixels and deterministic RGBA comparisons by default; tolerances/masks must be named contract rules.
- No inverse palette lookup to reconstruct supposedly original index buffers; duplicate colours make that lossy.
- Do not bless current screenshots or widen tolerance to conceal disagreement.
- Captured game artwork remains local; authored portable vectors exercise real production decoding/composition.
- Use B's `ds1edit.case/1` identity, coverage, and result semantics.

## Review Focus

- Colour channel assumptions in old prose/current code must be settled independently (task 1).
- Pixel-equal frames can still have wrong signed origins, dimensions, frame order, or timing (tasks 2–3).
- DCC repeated cells can pass isolated-frame tests while failing across a frame sequence (task 2).
- A valid PNG/APNG container can carry wrong pixels, alpha, or delays (task 3).
- A reference renderer and candidate reading different resources can make screenshot comparisons meaningless (task 4).

## Task 1: Palette, tile keys, and actual AnimData lookup

**Files:** Extend `test/test_palette.c`, `test/test_dt1_decode.c`; create `test/test_tile_properties.c`, `test/test_animdata_lookup.c`, `test/equivalence/contracts/visual-values-113c.json`, `test/equivalence/fixtures/animdata-authored.bin`, `scripts/equivalence/visual_values.py`, `test/python/test_visual_values.py`; modify `test/CMakeLists.txt` and proven production defects only.

**Interfaces:** Use real `wprop_2_block`/`fprop_2_block`/`sprop_2_block`, `animdata_load`, `animdata_hash_value`, `animdata_get_cof_info_quiet`, `animdata_get_frame_delay_ms`, and `palette_*` functions. Link production lookup/parse logic; the MPQ fetch alone may return authored bytes in portable cases.

- [ ] Test all 64×256 main/subindex combinations and independent flag-bit variants, all 25 legacy orientation inputs where applicable, raw flag preservation, and missing DT1 associations. Run wrong seven-bit-mask and wrong orientation-table controls through B's comparator.
- [ ] Build an authored 256-bucket AnimData file with 160-byte records, empty buckets, at least two colliding names, mixed-case queries, exact frame/speed values, and a miss. Test record traversal, case hashing, outputs on miss/null, and extension behavior as actually promised by the editor API. Keep millisecond rounding/fallback assertions separate from game fixed-point claims.
- [ ] Write palette cases with distinguishable red/blue values, index-0 transparency, gamma, all five act selection paths, duplicate nearest-colour entries, darkness/blend table indices, and short tables. Establish actual 1.13c channel order and applicable lookup tie policy using original bytes plus an observed game result; preserve that provenance. Do not copy disputed prose into expected values.
- [ ] Run new targeted tests red, investigate failures with runtime state, implement minimal fixes, run green. For tables the editor consumes but does not generate, compare consumed bytes/indexing and rendered results rather than adding an unused generator just to test a recovered formula.
- [ ] Run live reference cases through B's declared adapter and portable replay. Commit authored cases/small scalar expectations, contracts, tests, and supported fixes; keep PL2/AnimData game payloads local.

## Task 2: Capture true decoder outputs and metadata

**Files:** Create `test/test_visual_reference_probe.c`, `test/test_dcc_decode.c`, `test/test_dc6_decode.c`, `test/test_dt1_load.c`, `src/core/index_frame.c/.h`, `scripts/equivalence/decoder_cases.py`, `test/python/test_decoder_cases.py`, `test/equivalence/contracts/decoders-113c.json`; modify `src/core/dcc.c/.h`, `dc6.c/.h`, `dt1.c`, `dt1_decode.c`, and CMake only as required by tests/adapters.

**Interfaces:**
- Probe command `test_visual_reference_probe decode --kind dt1|dcc|dc6 --input PATH --direction N --out-dir PATH` produces `frames.json` and indexed frame buffers with explicit width/height/stride, signed origin, frame/direction ID, and transparency policy.
- Add shared `DS1EDIT_INDEX_FRAME` in `src/core/index_frame.h`: `int32_t width, height, stride, origin_x, origin_y, frame, direction`; `size_t pixels_len`; and owned `uint8_t *pixels`. Contracts explicitly declare index-0 transparency; any route with a distinct opacity mask must expose/compare it rather than discard it. `void ds1edit_index_frames_free(DS1EDIT_INDEX_FRAME *frames, size_t count)` releases all owned buffers and the array.
- Both `int dc6_decode_indices(const void *bytes, size_t len, int direction, DS1EDIT_INDEX_FRAME **frames, size_t *count, char *error, size_t error_cap)` and `int dcc_decode_indices(const void *bytes, size_t len, int direction, DS1EDIT_INDEX_FRAME **frames, size_t *count, char *error, size_t error_cap)` use the existing decoder algorithms factored to raw outputs; old bitmap consumers use that same path. Return 1 on success, 0 on failure, and initialize `*frames = NULL`, `*count = 0` before allocating. Do not create a second test decoder or recover indices from RGBA. If a smaller observational seam can expose existing raw buffers without parallel logic, use it and update this interface block and callers before implementation.
- DT1 probe uses actual `dt1_add`/header/block/subtile code with the fetch seam returning the selected bytes; pure isometric/RLE functions remain independently tested.

- [ ] Author small streams for DC6 literal/skip/row-end, empty runs, clipping, negative origins/flip, multiple frames/directions; DT1 diamond/RLE, block offsets and lengths, variants and animation; DCC signed/byte-boundary fields, repeated and changed cells across multiple frames, empty direction, and frame bounds. Assert both raw pixels and metadata. Poison output borders to detect writes outside declared bounds.
- [ ] Run tests red, add the smallest shared raw-output seam, then run green through both raw and existing bitmap entry points. Confirm old consumers produce the same intended output on known valid authored cases.
- [ ] For each original target, establish ABI/state/initialization and observable output through B's feasibility pattern. Existing DCC RVAs `0x140a0`, `0x13b70`, `0x13780` are discovery seeds, not assumed callable APIs. Bind each proven route into the decoder contract; missing original observability yields unavailable/partial evidence.
- [ ] Capture and compare representative vanilla frames spanning the authored feature matrix. Wrong origin, wrong row direction, dropped delta reuse, and transparent-skip controls must each fail a designated case.
- [ ] Run decoder Unity tests and portable frame replay on all three platforms; run the local original comparison separately. Commit only authored fixture bytes, safe small expectations, adapters, and proven fixes.

## Task 3: Composition and independent export decoding

**Files:** Create `test/test_compose_render.c`, `test/test_export_progress.c`, `test/python/test_export_images.py`, `scripts/equivalence/export_cases.py`; extend `test/test_compose_cof.c`, `test/test_compose_blit.c`, `test/test_compose_bbox.c`, `test/test_apng_writer.c`, `test/test_asset_export_smoke.c`, and relevant contracts/CMake. Modify composition/export production code only for measured failures.

**Interfaces:** Existing `compose_render`, `compose_render_free`, `compose_cof_parse`, `compose_cof_priority_at`, `asset_export_*_buffer_png`, APNG writer, and `export_progress_*`. Python `verify_export(path: Path, expected_frames: list[dict], expected_delays: list[tuple[int,int]]) -> dict` independently decodes using Pillow and verifies frame count, order, dimensions, RGBA data, and timing against declared rational-delay policy.

- [ ] Add real composition cases with two differently coloured overlapping layers, direction/frame-dependent priority, duplicate component IDs, signed offsets, absent optional component, zero loadable components, palette remap, and supported blend modes. Assert loaded resources and intermediate priority/bounds before final pixels.
- [ ] Add export cases with distinguishable frame colours, transparent borders, nonuniform delays, one-frame output, several directions, output-name collision, partial write failure, cancellation between items, and progress cleanup allowing the next export. Check cancellation/failure never claims success or leaves active singleton state.
- [ ] Run tests red; implement only supported fixes; run green. Decode the actual produced files independently rather than only counting PNG chunks/signatures. Exact rational-delay normalization must be declared and tested against serialized numerators/denominators as well as reader values.
- [ ] Extend original composition contracts with observed component/order/palette selections for the representative cases. A priority reversal, dropped alpha, swapped frame order, and wrong delay must fail controls.
- [ ] Run all composition/export native and Python tests, review, and commit; preserve local game-derived image artifacts outside public fixtures.

## Task 4: Deterministic six-map diagnosis and representative visual ledger

**Files:** Create `test/equivalence/profiles/visual-core.json`, `scripts/equivalence/render_cases.py`, `test/python/test_render_cases.py`; update `scripts/run_golden_tests.py` only through A's interfaces, `test/golden/` only after reference review, and `docs/release-readiness.md` with measured outcomes.

**Interfaces:** `render_cases(profile: dict, baseline: dict, candidate: Path, out_dir: Path) -> list[dict]` returns B case results. Required profile cases map all five acts, all supported decoder/composition features, and the six named current core maps to explicit asset hashes, configuration/gamma, frame/time, seed, and zoom.

- [ ] Test wrong input hash, accidental overlay/preferences, unpinned animation/frame, missing case-feature coverage, and all-selected-but-unexecuted result. Assert required inputs are resolved against B's manifest and A's strict process outcomes.
- [ ] Re-render the six core maps from the pinned original asset chain and owned configuration. Save parsed map/resource selection, indexed buffers, palettes, RGBA frames, and final screenshots to locate the first difference. Use the existing five mismatches as investigation inputs, not proof that either old reference or new renderer is correct.
- [ ] Resolve each discrepancy with recorded evidence: candidate defect, wrong baseline input, old reference defect, or declared nondeterministic display effect. Fix demonstrated code defects with regression cases. Propose a reference/tolerance change only as a separate before/after diff supported by reference identity and behavior.
- [ ] Require every profile feature to have executed case IDs and passing declared comparisons. Run the local visual profile plus portable replay; optionally sweep remaining decoder frames with coverage reported separately.
- [ ] Update the behavior ledger and assessment with exact counts and remaining unknowns; commit reviewed reference changes separately from fixes and never auto-run `--update` to clear failures.
