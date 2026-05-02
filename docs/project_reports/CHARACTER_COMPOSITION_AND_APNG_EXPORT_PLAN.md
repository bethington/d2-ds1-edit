# Character Composition And APNG Export Plan

## Problem

D2 characters, monsters, NPCs, and animated objects are not stored in
the MPQs as fully-composed images. They are layered: each visible body
part (head, torso, legs, arms, hands, shield, equipment slots) lives in
its own DCC file, and a COF (Component Object File) describes which
layers go in which order for a given (asset, mode, weapon-class) combo.
At runtime D2 picks the right COF, decodes each referenced DCC, and
blends the layers in order to render one frame.

The current export feature can produce raw DCC frames (one PNG per
direction × frame) but cannot produce a fully-composed view of a
character or monster doing something. There is no way today to look at
a single file and see "this is what the necromancer's walk cycle looks
like."

This plan adds a composition export path that produces APNG animations
with fully-blended layers, one APNG per (asset, mode, weapon-class,
direction) tuple.

## Scope

This plan covers:

- A new export path under the unified Ctrl+Shift+A action triggered by
  picking DCC or All from the type picker.
- Layer composition for player characters, monsters, NPCs, and animated
  objects, individually selectable or all together.
- APNG output with frame timing read from `animdata.d2`.
- Per-asset palette resolution (chars use Act 1, monsters/objects use
  their natural act).
- Configurable mode and weapon-class preset selection via
  `Ds1edit.ini` sections.
- A summary modal at the end reporting successes and skipped tuples.

This plan does not cover:

- Layer color tinting (used by D2 for unique items, set items,
  monster sub-variants like Rakanishu). Deferred to a follow-up; the
  default-palette output is the canonical "vanilla" appearance.
- Cropping/trimming output canvas. Each APNG uses its COF-declared
  canvas size for fidelity to D2 rendering.
- Custom equipment (specific helmets, armors, shields). The composer
  uses the bare-character layer set; equipment slot layers (S1-S8)
  are zero by default.
- Grid composites (all 8 directions in one APNG). One APNG per
  direction is the locked output shape.
- Async or progress-bar UX changes beyond what already exists in the
  export progress dialog -- composition reuses the existing progress
  state controller (the long inner loop is per-(asset,mode,weapon,direction)).

## Locked Product Decisions

### 1. Scope categories

The composition feature handles four categories. The user picks one in
a scope picker, or selects "All composed" to do everything:

- Player characters (7 hardcoded codes: AI, AM, BA, DZ, NE, PA, SO)
- Monsters (enumerated from `monstats.txt`)
- NPCs (entries in `monstats.txt` with `npc=1`)
- Animated objects (enumerated from `objects.txt`)

### 2. Animation modes: configurable preset list + "All"

A new `[char_mode_presets]` section in `Ds1edit.ini` defines named
collections of animation modes. After scope selection the user picks
one preset from a multi-select picker that also always includes an
"All modes" entry as a graceful fallback.

Default presets shipped in `Ds1edit.ini.sample`:

```ini
[char_mode_presets]
idle_only       = NU
idle_walk       = NU, WL
standard_combat = NU, WL, A1, DT
```

Power users can add their own preset rows. The "All modes" entry is a
hardcoded fallback so the picker is never empty even with a missing or
malformed config.

### 3. Weapon classes: configurable preset list + "All"

A new `[char_weapon_presets]` section in `Ds1edit.ini`. Same picker
shape as the mode preset picker. Only shown when the scope includes
player characters (monsters, NPCs, and objects don't have weapon-class
variants).

Default presets shipped:

```ini
[char_weapon_presets]
bare_hands  = HTH
melee       = HTH, 1HS, 2HS
ranged      = HTH, BOW, XBW
magic       = HTH, STF
common      = HTH, 1HS, BOW, STF
```

"All weapons" is a hardcoded fallback always present in the picker.

### 4. Per-direction APNGs

For each (asset, mode, weapon-class) tuple the composer emits one APNG
per direction. 8 directions for most assets; 16 for some monsters.
Filename includes `dir<N>` for the direction.

Grid composites (all directions in one image) are explicitly out of
scope for v1.

### 5. Menu integration: follow-up modal after DCC/All

The existing type picker is unchanged in shape (still shows
`all`/`dt1`/`dc6`/`dcc`). When the user picks DCC or All, a follow-up
modal appears asking "Compose mode? [x] (default checked)". For
DT1/DC6 selections no follow-up is shown.

Compose mode ON branches into:
1. Compose-category picker (chars / monsters / NPCs / objects / all)
2. Mode preset picker
3. Weapon preset picker (only if chars in scope)
4. Output folder picker (existing)
5. Upscale mode picker (existing)

Compose mode OFF preserves the existing scope-picker flow exactly.

### 6. Output naming: source-path-mirrored, all axes in filename

```
<output>/Data/global/chars/<class>/<class>_<mode>_<weapon>_dir<N>.png
<output>/Data/global/monsters/<monster>/<monster>_<mode>_dir<N>.png
<output>/Data/global/npc/<npc>/<npc>_<mode>_dir<N>.png
<output>/Data/global/objects/<object>/<object>_<mode>_dir<N>.png
```

`<N>` zero-pads to 1 digit for 8-direction assets and 2 digits for 16+
direction monsters. Modes and weapon classes use their D2 2-3-char
codes (NU, WL, A1, HTH, 1HS, BOW, etc.).

### 7. Frame timing from `animdata.d2`

APNG per-frame `delay_num`/`delay_den` is computed from D2's own
animation rate table. The codebase already parses `animdata.d2` via
`core/animdata.c`. Conversion: D2 stores a `speed` integer; per-frame
delay in milliseconds is `(256 / speed) * 40ms` (one game tick = 40ms,
25 FPS). Modes not present in the table fall back to fixed 40ms.

### 8. Failure handling: collect + summary modal

Missing COFs (the most common case -- many mode/weapon combos
legitimately don't exist) and layer load failures are recorded and
reported in a summary modal at the end:

```
Exported 847 APNGs to <output_path>.
Skipped 156:
  142 (no COF for the combo)
  14 (layer load failed -- see stderr.txt)
```

Per-asset skip detail goes to `stderr.txt` for debugging. Export does
not abort on missing COFs -- they are expected.

### 9. Asset discovery: TXT tables + per-asset COF directory walk

- Player chars: hardcoded list of 7 class codes.
- Monsters / NPCs: enumerated from `monstats.txt` (NPC = `npc=1` rows;
  others = monster rows). Read via the existing `core/txtread.c` /
  `core/mpq_index.c` infrastructure, possibly via a new
  `core/compose_index.c`.
- Objects: enumerated from `objects.txt`.

For each asset, the composer walks the `<asset_root>/<asset_code>/COF/`
directory (overlay + MPQ chain) to find which (mode, weapon) tuples
have COFs. That set is intersected with the user's mode/weapon preset
selections.

### 10. Optional descriptive folder names

A new `[compose]` section in `Ds1edit.ini`:

```ini
[compose]
; When YES, folder names use full descriptive names from the relevant
; TXT tables (e.g. "Necromancer", "Andariel", "TownPortal"). Filenames
; keep their compact 2-char-code form for compatibility with the
; source MPQ structure. When NO (default), folder names also use the
; 2-char codes so the output mirrors the MPQ structure exactly.
use_full_folder_names = NO
```

When YES, folder structure becomes:

```
<output>/Data/global/chars/Necromancer/NE_WL_1HS_dir0.png
<output>/Data/global/monsters/Andariel/AN_NU_dir0.png
```

Filenames stay the same. This keeps grep / move / rename workflows
unaffected by the flag.

Sanitization for full names: any character not in `[A-Za-z0-9_]` is
replaced with `_`, and runs of consecutive underscores collapse to
one. Example: `"Skeleton Archer"` becomes `Skeleton_Archer`.

### 11. Per-asset palette resolution; no layer tints

Each asset gets the palette appropriate for its category:

- Player chars: Act 1 palette (chars look identical across acts; Act 1
  is the canonical view).
- NPCs: Act 1 palette.
- Monsters: act derived from the monster's primary `Level` field in
  `monstats.txt` -> linked to `levels.txt` -> act number.
- Objects: act derived from `objects.txt` linkage to a level.

No layer tints are applied. Tinted monster sub-variants (Rakanishu,
Bone Fetish, etc.) render in their base color. Adding tints is a
follow-up if real users find the current output insufficient.

### 12. Canvas size: per-direction COF canvas, no trim

Each direction's APNG uses the COF-declared canvas dimensions for that
direction. Layers are blitted at their per-frame COF offsets within
the canvas. No transparent-margin trimming is applied -- the canvas
matches D2's own rendering exactly.

PNG/APNG compresses transparent regions efficiently; the file-size
cost of the unused padding is negligible. Trimming is a follow-up if
storage measurements show it matters.

## Architecture Overview

### New modules

- `src/core/apng_writer.{c,h}` -- minimal APNG encoder. Built on top
  of the existing libpng dependency (Allegro 5 already pulls libpng
  for `al_save_bitmap`). Implements the APNG fcTL/fdAT chunk emission
  per the apng spec; full RGBA frames; per-frame delay.
- `src/core/compose_index.{c,h}` -- enumerates compose-eligible
  tokens per category from TXT tables.
- `src/core/composer.{c,h}` -- per-tuple composition. Loads the COF,
  resolves layers via `core/cof.c` + `core/dcc.c`, blits each frame
  into a canvas at the COF's per-frame offsets, writes the APNG via
  `apng_writer`.
- `src/core/compose_naming.{c,h}` -- code-to-full-name lookup for
  chars/monsters/NPCs/objects/modes/weapons. Hardcoded class+mode+
  weapon maps; TXT-driven monster/NPC/object names. Sanitization
  helper.
- `src/ui/compose_mode_modal.{c,h}` -- the small follow-up modal
  shown after picking DCC or All in the type picker.
- `src/ui/compose_category_picker.{c,h}` -- list-style picker for
  chars/monsters/NPCs/objects/all.
- `src/ui/multi_select_picker.{c,h}` -- generic multi-select modal
  used by both mode and weapon preset pickers. Re-usable.

### Modified modules

- `src/config.c` and `src/structs.h` -- add `compose_*` config keys
  and parse the `[char_mode_presets]`, `[char_weapon_presets]`,
  `[compose]` sections.
- `src/core/animdata.c` -- add a `animdata_get_frame_delay_ms(token,
  mode)` helper if not already present.
- `src/core/cof.c` -- expose composition-friendly query helpers if
  needed (read-only access to layer count, per-direction canvas dims,
  per-frame offsets).
- `src/ui/project_menu.c` -- extend `action_export_unified` to branch
  into compose mode when the follow-up modal returns YES.

### Reused unchanged

- `core/dcc.c` -- DCC decoding. Already used by the editor preview.
- `core/cof.c` -- COF parsing. Already used by the editor preview.
- `core/animdata.c` -- animdata.d2 parsing.
- `core/export_progress.c` -- progress dialog reuses the existing
  controller; compose loop calls `export_progress_pump()` between
  tuples.
- `core/upscale.c` -- composed APNGs fed into the upscale pipeline
  the same way raw PNGs are. Per-asset palette resolution happens
  before staging; staging contains the composed APNGs ready to
  upscale.

## Implementation Order

Bottom-up, minimum 14 commits. Mirroring the discipline used in the
preset/menu work.

1. **APNG writer + unit tests.** Standalone encoder, no deps on the
   rest of the project. Tests round-trip a known set of frames.
2. **animdata frame-delay helper.** Function that returns delay in ms
   for a given (token, mode), with fixed-40ms fallback.
3. **compose_naming module + tests.** Hardcoded class/mode/weapon
   maps; sanitization helper. Pure logic, easy to unit-test.
4. **compose_index TXT enumerator + tests.** Enumerate compose-eligible
   tokens per category from monstats.txt / objects.txt.
5. **Compose config additions.** Parse `[char_mode_presets]`,
   `[char_weapon_presets]`, `[compose]` sections. Default presets
   shipped in `Ds1edit.ini.sample` and the `ini_create()` template.
6. **Composer module: skeleton.** Function that takes a
   (token, mode, weapon, direction) and produces an in-memory
   ALLEGRO_BITMAP[N_FRAMES] composed result. Drives existing cof_load
   + dcc_decode + dc6 blit primitives.
7. **Composer: per-asset palette resolution.** Look up palette index
   from monstats.txt/objects.txt/levels.txt; apply before composition.
8. **Composer: integrate APNG writer.** Take the composed bitmap
   array and write it to disk as an APNG with animdata-derived
   frame delays.
9. **Generic multi-select picker (UI module).** Re-usable for the
   mode and weapon preset pickers.
10. **Compose mode modal + category picker (UI modules).** The
    follow-up "Compose mode?" modal and the category list picker.
11. **Wire compose flow into action_export_unified.** Type picker -> 
    follow-up modal -> category picker -> mode preset picker ->
    weapon preset picker -> output folder -> upscale mode -> compose
    loop.
12. **Discovery + per-tuple iteration.** Use compose_index to
    enumerate tokens; for each, walk the COF directory to find
    available variants; cross-reference with user's mode/weapon
    presets; iterate through valid tuples calling the composer.
13. **Failure summary modal.** Track skipped tuples by reason; show
    the summary modal at the end.
14. **End-to-end smoke against user MPQ chain.** Compose a sorceress
    walk + idle in 8 directions; verify output and timing match D2.
    Document the workflow in the docs/ tree.

## Acceptance Criteria

- Ctrl+Shift+A -> DCC -> "Compose mode? Yes" reaches the compose
  category picker.
- Compose mode = OFF preserves the existing raw export behavior
  exactly.
- Player char composition emits one APNG per (class, mode, weapon,
  direction) for the user-selected presets, with frame timing from
  animdata.d2 and Act 1 palette.
- Monster/NPC/object composition uses the per-asset palette resolved
  from the TXT tables.
- Missing COFs are silently skipped during the loop and counted; a
  summary modal at the end reports counts by reason.
- `use_full_folder_names = YES` swaps folder names to full descriptive
  forms; filenames remain in code form.
- The `[char_mode_presets]` and `[char_weapon_presets]` sections are
  parsed; the picker shows them in INI order plus a hardcoded "All"
  entry at the top.
- Output paths mirror the source MPQ structure under
  `Data/global/chars/`, `Data/global/monsters/`, etc.
- The composed APNGs are recognizable as the expected D2 character or
  monster (verified by visual smoke test against an in-game
  screenshot reference).
