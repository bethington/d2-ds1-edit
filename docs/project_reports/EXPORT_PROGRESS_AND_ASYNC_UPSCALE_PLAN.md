# Export Progress And Async Upscale Plan

## Problem

The current export flow can look stalled during long native PNG exports and remote
upscale operations. The app does real work, but the user gets little or no
feedback between the initial choice dialog and the final message box.

This is now a product requirement, not just a polish task:

- Every export action should surface visible progress.
- Progress must be honest enough that the user can trust it.
- The first implementation must support cancellation.
- Remote upscale progress must come from a real async job model rather than a
  guessed client-side percentage.

## Scope

This plan covers:

- Native PNG export progress for all export actions.
- Local 2x and 4x upscale progress.
- Remote 2x and 4x upscale progress using a new async service API.
- Client cancellation, remote cancellation, partial-output handling, and final
  result messaging.

This plan does not yet cover:

- ETA display.
- Arbitrary app-wide long-task infrastructure beyond export and upscale.
- Multi-user auth or database-backed job persistence.

## Locked Product Decisions

The following decisions were chosen and should be treated as implementation
requirements unless explicitly revisited.

### Client UX

- Use a modal progress dialog for export work.
- Show the dialog for all export actions, including native PNG-only export.
- Show the dialog immediately when work starts.
- The dialog must include:
  - a percent bar
  - current stage text
  - item counts
  - current asset path/name
- Long asset paths should be middle-truncated.
- The first version must include a Cancel button.
- On cancel, keep already-written files and stop further work.
- On success, close the progress dialog immediately and then show the usual
  final summary message.
- On failure after partial work, show a detailed message such as:
  `Failed after N of M items; partial output was kept.`
- On cancel, show a distinct canceled summary such as:
  `Canceled after N of M items; partial output was kept.`
- Do not allow concurrent exports.

### Progress Model

- Use weighted multi-stage progress across the full workflow.
- Use exact totals for native export by doing a pre-scan before writing output.
- Do not show ETA in the first implementation.
- Poll remote jobs at a state-aware cadence: 1500 ms while `queued`, 500 ms
  while `running`, stop polling on first observation of any terminal state.
- If polling drops, retry every 1 second for up to 10 seconds. The dialog
  shows a `Reconnecting… (Ns)` status line during retries and the progress
  bar freezes (no fake forward motion). After 10 seconds of failed retries,
  prompt the user with Retry / Cancel rather than auto-failing or
  auto-falling-back to local — a poll drop means we lost contact, not that
  the job failed.

### Remote Service Behavior

- Add async job endpoints alongside the current blocking archive endpoint.
- Keep the current blocking endpoint for compatibility and smoke tests.
- Use in-memory job state with temp files on disk.
- Keep completed, canceled, or failed jobs for 60 minutes after the
  terminal state, then auto-clean (see Queueing Rules).
- Process one active GPU job at a time and queue the rest.
- Use FIFO queue ordering.
- Show queue position in the client while waiting.
- Warn the user before submitting when there are 2 or more jobs ahead.
- On client cancel, send a remote cancel request and stop polling.
- On async remote failure, keep the existing local fallback prompt.
- Add optional token-based auth via the standard `Authorization: Bearer
  <token>` header.

### Configuration

- Add an optional `upscale_service_token` entry to `Ds1edit.ini`.
- When set, the client sends `Authorization: Bearer <token>` on every
  request. When the service has a token configured and the header is
  missing or wrong, it returns `401 Unauthorized`. When the service has
  no token configured, the header is ignored. Token comparison on the
  service must be constant-time.

## Current Constraints

The current code path is synchronous in two important places:

- Export actions in [src/ui/project_menu.c](../../src/ui/project_menu.c)
  perform export work inline and then call `run_upscale_pipeline(...)`.
- Remote upscale in [src/core/upscale.c](../../src/core/upscale.c) is a single
  blocking ZIP upload and download operation.

That means truthful progress and cancellation require structural changes in both
the client and the service.

## Architecture Overview

### Client Side

Add an export-task controller that owns:

- progress state
- weighted stage accounting
- cancellation flag
- current item text
- final result classification: success, canceled, or failed

Add a modal export progress dialog specialized for export and upscale work.

The dialog should render:

- title
- stage label
- progress bar
- `current / total` counts
- current asset label
- cancel button

The client should continue pumping events while work proceeds so the UI remains
responsive enough to repaint and process cancel input.

### Threading Model

Use a hybrid model that pays the threading cost only where it actually buys
something:

- **Native export and local upscale** stay on the main thread. Their inner
  loops in [src/core/asset_export.c](../../src/core/asset_export.c) and
  [src/core/upscale.c](../../src/core/upscale.c) call a shared
  `progress_pump(ctx)` helper between items. The helper drains pending
  Allegro events, repaints the dialog, and checks the cancel flag. This
  avoids auditing the MPQ handle, palette caches, and Allegro memory
  bitmap state for thread-safety.
- **Remote upload, polling, and download** run on a worker thread (Allegro
  `al_create_thread`). The main thread keeps pumping the event loop and
  the dialog reads progress state through atomic counters plus a small
  mutex for the current-item string buffer.

The progress state struct is designed for concurrent access (atomic ints
for counts, mutex-protected current-item buffer) so the boundary between
threaded and non-threaded code does not leak.

### Service Side

Add an async archive-job model:

1. Client submits a ZIP archive and receives a job id.
2. Client polls job status.
3. Service reports queue state, running state, item counts, current stage,
   and any failure message. The client computes percent from item counts —
   the service does not emit `progress_percent`.
4. Client downloads the completed result archive.
5. Client may cancel the job while queued or running.

The current blocking endpoint remains intact for backward compatibility and
smoke validation.

## Proposed Client Modules

### 1. Export Progress State

Add a focused export-progress module, likely under `src/ui/` or `src/core/`,
that defines:

- progress stages
- weighted progress calculation
- status text formatting
- cancellation state
- current item tracking
- result summary data

Candidate responsibilities:

- `begin_stage(...)`
- `advance_items(...)`
- `set_current_item(...)`
- `request_cancel(...)`
- `is_cancel_requested(...)`
- `finalize_success(...)`
- `finalize_canceled(...)`
- `finalize_failure(...)`

### 2. Export Progress Dialog

Add a dedicated modal dialog, similar in spirit to the new upscale picker but
with a more stable layout and a clickable cancel action.

Requirements:

- clamp and clip all text
- repaint continuously while active
- update from shared progress state
- expose cancel input through keyboard and mouse
- avoid overflowing on long labels

### 3. Export Enumeration Pass

Refactor export discovery so each export action can pre-scan exact work totals
before writing output. The discovery pass walks the MPQ asset listing exactly
once and produces an in-memory plan that the emission pass consumes.

Introduce an `asset_export_plan_t` struct holding:

- a heap-allocated array of `(asset_path, asset_kind)` entries, where
  `asset_kind` is one of `dt1`, `dc6`, `dcc`, etc., resolved during discovery
- a count

Plus the corresponding API in [src/core/asset_export.c](../../src/core/asset_export.c):

- `asset_export_plan_*` builders, one per current `asset_export_*` entry
  point (area-group, prefix, prefix-by-type, all)
- `asset_export_run_plan(plan, output_dir, progress_ctx)` — single emitter
  that iterates the plan and reports progress per item
- `asset_export_plan_free(plan)`

The current `asset_export_*` functions either become thin wrappers
(`plan + run + free`) or get replaced outright at the call sites in
[src/ui/project_menu.c](../../src/ui/project_menu.c). Resolving `asset_kind`
during discovery avoids re-sniffing the asset type at emission time and gives
the dialog enough information to label stages (e.g.
"Exporting tiles 12 / 47").

## Proposed Service API

Add new endpoints under the existing service.

### Submit Job

`POST /jobs/upscale/archive`

Behavior:

- accepts ZIP archive body
- accepts `method` and `scale`
- accepts optional `Authorization: Bearer <token>` header (see Configuration)
- creates a queued job
- returns job id, initial queue position, and current status

### Poll Job Status

`GET /jobs/{job_id}`

Behavior:

- returns status object with fields:
  - `state`: queued, running, completed, failed, canceled
  - `stage`: queued, unpacking, upscaling, packing, done
  - `items_done`
  - `items_total`
  - `queue_position`
  - `current_item`
  - `error`

The service does **not** emit a `progress_percent` field. Item counts are
the single source of truth and the client computes percent from them via
the weighted progress model. This eliminates the risk of `progress_percent`
disagreeing with `items_done`/`items_total` and keeps weight calibration on
the client side where it can be tuned without a service redeploy.

Stage taxonomy (must agree client-side and service-side):

- `queued` — no items yet, only `queue_position` is meaningful
- `unpacking` — extracting submitted ZIP into temp workspace
- `upscaling` — running the model over images; `items_done`/`items_total`
  report per-image progress
- `packing` — building the result archive
- `done` — terminal; client downloads next

When `items_total` is 0 (e.g. early in `unpacking` before file count is
known), the client treats the stage's contribution as a fixed bump on
entry, not as 0%.

### Download Result

`GET /jobs/{job_id}/result`

Behavior:

- returns finished ZIP archive for completed jobs

### Cancel Job

`POST /jobs/{job_id}/cancel`

Behavior:

- cancels queued or running jobs
- returns resulting state

### Health / Queue Snapshot / Version Probe

Extend `GET /health` to advertise capabilities and queue depth. Response
shape:

```json
{
  "version": "2.0",
  "features": ["async_jobs"],
  "queue_depth": 3
}
```

The client probes `/health` once on first remote use of a session, caches
the result on the upscale module, and invalidates it only on explicit user
retry after a failure. Probing on first use rather than at startup avoids
network traffic for users who launch DS1Edit offline.

When the response omits `async_jobs` from `features` (or returns a
schema-incompatible shape), the client falls back to the existing blocking
endpoint for the rest of the session. The progress dialog still appears in
this fallback path, but with a single coarse "Uploading and processing..."
stage and an indeterminate-style heartbeat — not fake percentage progress.

`queue_depth` lets the client warn before submission when 2 or more jobs
are already ahead.

The `version` field uses semver. The `features` list is additive — new
capabilities appear as new feature strings; existing strings keep their
meaning. This contract is public once shipped.

## Weighted Progress Model

Use a stable two-tier model so the percent does not jump backward.

**Tier 1 — Stage weights (fixed, recalibrated):**

- Prepare / pre-scan: 5%
- Native PNG export or local staging write: 35%
- Package and upload: 10%  *(byte-weighted within the stage)*
- Remote queue and processing: 25%
- Download result: 15%  *(byte-weighted within the stage)*
- Extract result: 10%

**Tier 2 — Within-stage progress:**

- Native export and local upscale: progress within the stage advances per
  item, using `items_done / items_total` from the export plan.
- Package upload and download: progress within the stage advances per byte
  transferred (`bytes_done / bytes_total`). Reuse libcurl progress
  callbacks.
- Remote queue and processing: progress within the stage advances per item,
  using the server's `items_done` / `items_total`.
- Prepare and extract: each treated as a single step contributing its full
  weight on completion.

Notes:

- Native PNG-only exports use only the relevant local stages and normalize
  the total to 100%.
- Local fallback uses the same dialog and weighted model, replacing remote
  stages with local upscale stages.

The tier-1 weights are a starting point and should be calibrated after the
first real runs. Tier-2 within-stage progress is honest by construction
(real bytes, real items) and should not need calibration. The original
weights overstated upload (10%) relative to download (5%) for typical
upscale workloads, where the result archive is materially larger than the
input archive — the recalibrated 10/15 split better reflects observed
asymmetry.

## Cancellation Semantics

### Native Export / Local Upscale

- Cancellation is cooperative.
- Loops must check a shared cancel flag between items.
- Files already written remain in place.
- Final result is `canceled`, not `failed`.

### Remote Upscale

- If the job is queued, cancel should remove it from the queue.
- If the job is running, cancel should set a stop flag the worker respects
  between file operations.
- DS1Edit stops polling after sending cancel, then confirms final canceled
  state if the service returns one.

**Cancel during download or extract** is treated as cancel-and-discard:

- Close the HTTP stream.
- Delete the partial result archive and any partially-extracted files in
  `output_path`.
- Send a courtesy `POST /jobs/{id}/cancel` so the service can clean up its
  result archive sooner. Fire-and-forget — do not block the client on a
  service that may itself be unreachable.
- Summary: `Canceled during download. No upscaled output was kept.`

Once the job state transitions to `done` and the client begins downloading,
the Cancel button label changes to `Cancel (discards completed result)`
and clicking it shows a confirmation prompt: `Remote work is already done.
Cancel will discard the upscaled result. Continue?` (Yes / No). This gives
the user an informed choice when the bottleneck has shifted from compute
to download.

### Multi-Stage Cancel

The export pipeline writes native PNGs to a staging directory before
upscaling them into the user's chosen `output_path`. Cancellation behavior
depends on which stage was active at the moment of cancel:

- **Cancel during native export:** Files already written to the staging
  directory remain. Upscale is never started. Summary uses the standard
  form: `Canceled after N of M items; partial output was kept.`
- **Cancel during upscale (local or remote):** Both the partial
  `output_path` and the staging directory are kept. The summary explicitly
  names both locations:
  `Canceled during upscale after N of M images. Native PNGs are at <staging_path>; partially upscaled files are at <output_path>.`

The success path may clean up the staging directory after upscale
completes; the cancel path must not. Doubling disk usage for a canceled
run is acceptable — the user paid for the native export work and may want
those PNGs even without upscale.

In the remote download/extract case (cancel after `done`), the staging
directory is also kept by the same rule, since the user still has access
to the native PNGs that were the input to the upscale.

## Queueing Rules

- Single active GPU job.
- FIFO queue.
- Show queue position while queued.
- Warn before submit when 2 or more jobs are already ahead.
- Keep job metadata and results for a TTL, then clean up automatically.

TTL semantics:

- TTL starts when the job enters any terminal state (`completed`, `failed`,
  `canceled`).
- TTL value: 60 minutes. (Picked over 15 minutes so AFK users — meeting,
  lunch, errand — do not lose their result before they return to download
  it.)
- On expiry: delete the result archive, delete the temp workspace, drop
  the job record from the in-memory registry.
- A `GET /jobs/{id}/result` request after expiry returns `410 Gone` (not
  `404`), with a body that distinguishes "this job existed but was cleaned
  up" from "this job ID never existed." The client uses this to produce a
  precise error message.

Cleanup runs from a single sweeper thread that wakes every ~30 seconds
and processes anything past its expiry. One sweeper is cheaper to reason
about than per-job timers and is plenty for the expected job rate.

The blocking endpoint is unaffected by these rules — it is request-scoped
and no job state outlives the response.

## Rollout Plan

### Phase 1: Client Progress Framework

- Add export progress state model.
- Add modal export progress dialog.
- Add middle-truncation helper for long asset names.
- Add a global guard preventing concurrent exports, exposed via
  `export_task_is_active(void)`.
- Grey out export menu items in [src/ui/project_menu.c](../../src/ui/project_menu.c)
  while a job is active. This is the discoverable UX layer; the guard
  above is the source of truth.
- Add a defensive `if (export_task_is_active()) return;` at the top of each
  `action_export_*` function to catch any path that bypasses the menu
  greying (future keyboard accelerators, scripted invocation, race during
  state transition).
- Non-export menu items (Save Project, etc.) remain enabled during export
  work — the modal blocks interaction in practice, but nothing else should
  be administratively disabled.

Validation:

- Build the client.
- Manually verify the dialog renders correctly at current supported window
  sizes.
- Verify cancel input is accepted and the dialog stays responsive.
- Verify export menu items grey out while the dialog is up and re-enable
  when it closes.

### Phase 2: Exact Native Export Progress

- Refactor export enumeration to pre-scan exact totals.
- Thread progress updates through native PNG export paths.
- Return detailed canceled and failed summaries.

Validation:

- Run native export flows for area, folder, folder-by-type, and all-assets.
- Verify percent is monotonic and reaches 100%.
- Verify cancel leaves partial output and reports exact counts.

### Phase 3: Local Upscale Progress

- Thread progress and cancellation through local upscale recursion.
- Reuse the same dialog and result summaries.

Validation:

- Run 2x and 4x local upscale.
- Cancel during local upscale and verify partial results remain.

### Phase 4: Async Remote Service

- Add queued async archive-job endpoints.
- Add in-memory job registry and temp workspace handling.
- Add FIFO queue and single-worker processing.
- Add token-header support.
- Add TTL cleanup.

Validation:

- Unit-test job state transitions: queued → running → completed,
  queued → canceled, running → canceled, running → failed.
- Smoke-test the happy path end-to-end: submit a small known PNG archive,
  poll, download, verify result archive contents.
- **Two-client queue scenario:** submit two jobs back-to-back, verify the
  second reports `queue_position: 1` while waiting, transitions to
  `running` after the first completes, completes successfully.
- **Cancel-during-download scenario:** submit, wait for `done`, begin
  download, abort mid-stream, verify no partial output remains in
  `output_path`, verify a fresh re-submit still works.
- **Poll-drop scenario:** submit, pause the service container during a
  `running` poll for 5 s, verify the client retries and recovers when the
  service resumes.
- TTL cleanup smoke: submit a job, let it complete, advance the sweeper
  past 60 minutes (or use a test override), verify the result archive is
  removed and `GET /jobs/{id}/result` returns `410 Gone`.
- Verify compatibility by keeping the existing blocking endpoint working
  under the same harness, and verify the `/health` probe correctly causes
  an old-service client to fall back to blocking.

Scripts live under `tools/upscale-service-tests/` and are invoked manually
or in CI. Each scenario is a standalone script keyed by name (e.g.
`test_queue.sh`, `test_cancel_download.sh`, `test_poll_drop.sh`). The
racy scenarios (queue, cancel-during-download, poll-drop) target classes
of bug that unit tests cannot see.

### Phase 5: DS1Edit Remote Async Client

- Replace blocking remote archive flow in the main export path with:
  - queue check / optional warning
  - async job submit
  - polling
  - cancel request
  - result download and extract
- Keep existing local fallback prompt on remote failure.

Validation:

- Run end-to-end export against the live service.
- Verify queue position display.
- Verify cancel reaches the service.
- Verify retry-on-poll-failure behavior.
- Verify fallback prompt still works when the remote job fails.

### Phase 6: Docs And Regression Coverage

- Update service README and DS1Edit remote upscale docs.
- Document the new config token field.
- Add at least one regression-oriented smoke script or test path for async jobs.

Validation:

- Re-run local and remote smoke checks from docs.

## Risks

### 1. Export Refactor Size

Current export functions are oriented around direct execution, not enumerated
task plans. The exact-total requirement may force more restructuring than the
UI work alone would suggest.

### 2. Event Pumping During Work

The dialog must remain responsive while long export loops run. If the work loop
does not yield cleanly enough, cancellation and repaint behavior will feel
broken.

### 3. Honest Remote Progress

Remote progress quality depends on the worker reporting meaningful
`items_done` / `items_total` updates within the `upscaling` stage. The
service does not emit a `progress_percent` field — the client computes
percent from item counts via the weighted progress model — so the failure
mode is now narrowly scoped to "the worker only updates item counts at
stage boundaries instead of per image." Validate during Phase 4 that
`items_done` increments per image, not per archive operation.

### 4. Cancellation Cleanup

Cancel is easy to promise and easy to get subtly wrong. Partial files,
temporary archives, queued jobs, and running jobs all need consistent cleanup
rules.

### 5. Backward Compatibility

The existing blocking endpoint must keep working while the async API is added.
That needs explicit smoke coverage during rollout.

## Why This Order

The natural temptation is to start with the service: the API shape is the
most novel piece, and locking it down first feels like reducing risk.

Resist that. Start with the client.

Three reasons:

1. **The client knows what it needs.** What progress fields the API should
   expose, what cancel semantics matter, what queue state is worth
   surfacing — these are all client-driven questions. Designing them
   service-first means guessing.

2. **Visible progress is the deliverable users see.** Phase 2 (exact
   native export progress) ships value to every user the moment it merges,
   even before the async service exists. Service-first delays that win
   until the entire stack lands.

3. **Local upscale (Phase 3) before remote (Phase 4) validates the
   progress model under real long-running work without adding network
   failure modes.** If the dialog/cancel/percent contract is wrong, we
   find out against a known-good local pipeline first — not while also
   debugging HTTP, polling, and queue state.

## Acceptance Criteria

The work is complete when all of the following are true:

- Every export action shows a progress dialog.
- Native export uses exact totals from a pre-scan.
- Local upscale updates percent and supports cancel.
- Remote upscale uses async job submission and polling.
- The dialog shows queue position while remote work is queued.
- Cancel stops local work and sends remote cancel when needed.
- Partial output is preserved on cancel.
- Success, canceled, and failed runs produce distinct final summaries.
- Existing blocking remote endpoint still works.
- Docs and at least one repeatable async-job smoke path are updated.

### Quality Measurables

These are calibrated against the first real runs. Numbers are starting
targets, not hard contracts:

- **Monotonic progress.** The progress bar never moves backward during a
  successful run. Stage transitions and item completions only advance
  percent.
- **Repaint frequency.** During active work (native export or local
  upscale), the dialog repaints at ≥10 Hz. The cancel button must remain
  visually responsive throughout.
- **Cancel responsiveness.** After the user clicks Cancel, work stops
  within one item-iteration. Target: <1 s for native export, <2 s for
  local upscale, <2 s for remote (one cancel request + acknowledgment).