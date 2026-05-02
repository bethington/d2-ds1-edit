# Export Presets And Menu Unification Plan

## Problem

The current export feature has four distinct menu actions, three keyboard
shortcuts, and a folder-prefix-only filter. The "items folder" use case
(export only inventory icons, not animations) cannot be expressed cleanly:

- The type filter is extension-based (`dt1`/`dc6`/`dcc`), so it cannot
  distinguish single-frame inventory icons from multi-frame animations
  that share the same `.dc6` extension.
- The folder filter matches recursively but offers no wildcard syntax,
  so a user wanting `data\global\items\inv*.dc6` has no expressive way
  to write that.
- The four menu actions (Area / Folder / Folder By Type / All) overlap in
  scope and produce a fragmented UX with three keybindings doing very
  similar things.

## Scope

This plan covers:

- A configurable single-frame DC6 filter for PNG export.
- A user-defined preset list with wildcard patterns in `Ds1edit.ini`.
- A unified export entry point bound to a single keyboard shortcut.
- A scope picker that replaces the existing folder picker step in the
  by-type flow.
- A cross-platform text-input modal for ad-hoc custom patterns.

This plan does not cover:

- Multi-frame export to alternative formats (animation as GIF / video /
  image sequence). Multi-frame DC6 and all DCC continue to export today
  via the legacy code paths when the single-frame filter is disabled.
- Preset persistence beyond `Ds1edit.ini` (no per-project preset files,
  no recently-used tracking).
- Renaming, sharing, or import/export of preset definitions.

## Locked Product Decisions

The following 14 decisions were chosen and should be treated as
implementation requirements unless explicitly revisited.

### 1. Preset Storage

Presets live in a `[export_presets]` section of `Ds1edit.ini`. Format is
`name = type | pattern`, one per line. Order is preserved by the existing
INI parser.

```ini
[export_presets]
items_inv = dc6 | data\global\items\inv*.dc6
```

### 2. Wildcard Syntax

Full glob:

- `*` matches any run of characters except `\` (matches the empty string).
- `?` matches exactly one character except `\`.
- `**` as a complete path component matches any number of path components
  including zero.
- `**` anywhere else (e.g. `inv**.dc6`) is treated as `*`.
- Matching is case-insensitive.
- Patterns are normalized to backslash separators before matching.

### 3. When the Single-Frame DC6 Check Runs

During discovery, only when the single-frame filter is enabled. Discovery
opens each candidate `.dc6` file, reads the header (24 bytes), and
excludes multi-frame files from the plan. When the filter is disabled,
discovery does not open DC6 files.

### 4. Single-Frame Toggle Exposure

Config flag `export_dc6_single_frame_only = YES/NO`, default `YES`. No UI
control. Set-and-forget; matches the stated policy that PNG export is
single-frame by default.

### 5. Menu Integration

Single unified entry point bound to `Ctrl+Shift+A`. Flow:

1. Type picker (existing) — `all` / `dt1` / `dc6` / `dcc`
2. Scope picker (new layer)
3. Output folder picker (existing)
4. Upscale mode picker (existing)

### 5b. Scope Picker Contents

After type selection, the scope picker shows:

```
All <type> assets
Current area's assets         (greyed when no DS1 is loaded)
Choose folder…
Type custom pattern…
─────────────────────
[user presets from Ds1edit.ini, type-filtered]
```

### 6. Text-Input Modal

Cross-platform Allegro modal in `src/ui/text_input_modal.c/h`. ~200 lines
of new code. Renders a centered modal with a single-line text field,
cursor, OK/Cancel buttons. Handles standard editing keys (typing,
Backspace, Delete, Left/Right arrows, Home/End, Ctrl+V via
`al_get_clipboard_text`). ESC cancels, Enter submits.

### 7. Default Presets

Shipped in `Ds1edit.ini.sample`:

```ini
[export_presets]
items_all   = dc6 | data\global\items\*.dc6
items_inv   = dc6 | data\global\items\inv*.dc6
items_potions = dc6 | data\global\items\pot*.dc6
tiles_all   = dt1 | data\global\tiles\**\*.dt1
tiles_act1  = dt1 | data\global\tiles\ACT1\**\*.dt1
tiles_act2  = dt1 | data\global\tiles\ACT2\**\*.dt1
tiles_act3  = dt1 | data\global\tiles\ACT3\**\*.dt1
tiles_act4  = dt1 | data\global\tiles\ACT4\**\*.dt1
tiles_act5  = dt1 | data\global\tiles\ACT5\**\*.dt1
```

`monsters_all` and `chars_all` are deferred until verified against the
user's MPQ chain (cannot ship guesses; the smoke output we inspected
covered DC6 and DT1 only).

### 8. Single-Frame Definition

`directions == 1 AND frames_per_direction == 1` (i.e. total frames == 1).
Both header fields must equal 1. Captures static inventory icons exactly;
excludes per-direction animations and multi-frame UI sprites alike.

### 9. DCC and Filter Visibility

Type picker keeps all four entries (`all`, `dt1`, `dc6`, `dcc`). The
`export_dc6_single_frame_only` flag remains in config (not removed).
Both serve as power-user escape hatches when multi-frame export is
intentionally desired.

### 10. Old Menu Entries

Deleted entirely:

- `action_export_area_assets`
- `action_export_folder_assets`
- `action_export_folder_assets_of_type`
- `action_export_all_assets`
- The `Ctrl+Shift+R` and `Ctrl+Shift+T` keybindings

Only `Ctrl+Shift+A` remains as the unified entry point.

### 11. Output Folder Structure

Preserve source paths under the output folder (current behavior). All
scopes (presets, custom patterns, "Choose folder…", "Current area")
share the same layout — the source asset path is mirrored under the
chosen output folder.

### 12. Zero-Match Behavior

Two distinct diagnostic paths:

- **No matches found by discovery:**
  `Pattern "<name>" matched no files. Check the pattern or your mod_dir setting.`
- **Matches found but all filtered out:**
  `Found N candidates but skipped all of them: N multi-frame DC6 files (single-frame filter is on). Disable export_dc6_single_frame_only or pick a different preset.`

The plan struct gains a `total_candidates` field separate from
`items_total` to drive this distinction.

### 13. Preset Display Order

INI file order. The existing INI parser already preserves order; the
preset-display code simply iterates the parsed array. Users group
presets by editing the INI directly.

### 14. Implementation Order

Bottom-up, in 11 commits:

1. Glob matcher + unit tests
2. Preset config parser + tests
3. Plan struct + discovery refactor (with `total_candidates`)
4. Single-frame DC6 filter (interim wired to existing by-type action)
5. Glob-based discovery (replaces prefix-only matching)
6. Text-input modal
7. Scope picker
8. Unified `Ctrl+Shift+A` action
9. Delete old `action_export_*` and old keybindings
10. Zero-match diagnostic messages
11. Default presets in `Ds1edit.ini.sample`

End-to-end smoke against the user's MPQ chain after commit 11.

## Architecture Overview

### New Modules

- `src/core/glob_match.c/h` — pure pattern matcher. Stateless. Unit-testable.
- `src/ui/text_input_modal.c/h` — cross-platform single-line text input.
- `src/ui/scope_picker.c/h` — list-style modal showing built-in scopes
  plus type-filtered presets.

### Modified Modules

- `src/config.c` and `src/structs.h` — add `export_dc6_single_frame_only`
  flag and `[export_presets]` section parsing into a new
  `EXPORT_PRESET_S` array stored on `glb_config`.
- `src/core/asset_export.c/h` — split discovery from emission. Introduce
  `asset_export_plan_t` carrying the candidate list, pattern, type
  filter, and `total_candidates`. Replace prefix-only matching with the
  glob matcher.
- `src/core/dc6.c` (or a new helper) — add `dc6_is_single_frame(const
  void *header_bytes, long len)`.
- `src/ui/project_menu.c` — replace the four `action_export_*` functions
  with a single unified action invoking the scope picker.

### Removed

- `src/ui/project_menu.c` — `action_export_area_assets`,
  `action_export_folder_assets`, `action_export_folder_assets_of_type`,
  `action_export_all_assets`, and the `Ctrl+Shift+R` and `Ctrl+Shift+T`
  key handlers.

## Acceptance Criteria

The work is complete when all of the following are true:

- A single `Ctrl+Shift+A` invokes the unified export flow.
- The four old export actions and their keybindings are gone.
- Type picker still offers all four types (`all`, `dt1`, `dc6`, `dcc`).
- Scope picker shows built-ins plus type-filtered presets in INI file
  order.
- Custom-pattern entry opens a working text-input modal.
- Glob patterns match correctly per the syntax rules in decision 2 (unit
  tests pass).
- `export_dc6_single_frame_only = YES` (default) excludes multi-frame DC6
  from PNG output during discovery.
- `export_dc6_single_frame_only = NO` falls back to the prior behavior
  (all DC6 exported).
- Zero-match runs produce the appropriate diagnostic message
  distinguishing "no matches" from "all filtered out."
- `Ds1edit.ini.sample` ships with the verified default preset list.
- End-to-end export of `items_inv` against the user's MPQ chain produces
  inventory-icon PNGs only, with multi-frame `inv*` files excluded.
