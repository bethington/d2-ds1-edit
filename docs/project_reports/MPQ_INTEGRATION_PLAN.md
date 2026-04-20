# MPQ Integration Plan

Branch: `mpq-integration` (branched from `main`).

## Problem

Today the editor opens DS1 files directly from disk, but DS1 files are context-free — they don't carry their own `LevelType` or `LvlPrest` ID. Getting them to render correctly requires hunting for the right parameters, which is why the community uses a separate tool, **DrTester**, just to preview a DS1 with different IDs until one looks right.

If the editor can read the game's MPQ archives directly, it can derive those parameters itself from `LvlPrest.txt` and `LvlTypes.txt` — and as a side effect gain proper tileset, palette, and preview fidelity across the whole DS1 catalog.

We also want a clean authoring model for producing mods: edit content in the editor, export a mod MPQ that layers over the stock game.

## Design Decisions

Ten decisions locked in during design:

| # | Decision | Rationale |
|---|---|---|
| 1 | **Non-destructive output.** Edits produce a mod MPQ overlay; original Blizzard MPQs are never modified. | Same pattern Blizzard's own `patch_d2.mpq` uses. Protects against bricking the install; no need to deal with signed-archive semantics on write-back. |
| 2 | **Classic D2 / LoD only.** No D2R / CASC. | The existing editor targets classic. D2R support can be added later as a parallel read backend; output format (a mod MPQ) is already D2R-compatible. |
| 3 | **Working copy = loose files.** A project is a folder on disk with files at their in-game paths (e.g. `project/data/global/tiles/ACT1/CAVES/denent.ds1`). MPQ is only produced at build time. | Loose files diff in git, survive editor crashes, and compose with every other tool in the modding ecosystem. |
| 4 | **Per-project install path, seeded from a global default.** | Mods are version-specific; binding each project to its D2 install prevents wrong-data-silent-fail bugs. Global default kills the repetition cost. Registry/common-path auto-detect seeds the default. |
| 5 | **First milestone = invisible integration + preset picker.** Correct tilesets/palettes resolved automatically from MPQ, plus an "Open by Preset" panel listing `LvlPrest.txt` entries. Full asset browser deferred. | The correctness wins are where the user pain lives. The preset picker is mostly UI wiring on top of the same table parse. Full browser is a separate, larger effort. |
| 6 | **Patch chain = hardcoded Blizzard order + user-added mod MPQs stacked above.** Project stores an ordered list of extra mod MPQs. | Blizzard's order is invariant and shouldn't be reshufflable. User-extras support real workflows like building a submod for PlugY or a patch for a total conversion. |
| 7 | **Copy-on-save.** Open of an MPQ-originated file is a read-only view. Only save materializes a copy in the project folder. | Keeps the project directory as "exactly what my mod ships." Browsing tilesets does not litter the project with copies. |
| 8 | **StormLib, vendored.** | De facto standard, handles all D2 MPQ versions, supports write for the build step. One library to drop in, no system-package dependency. |
| 9 | **Background eager indexing.** On project open, lazily return immediately but kick off a worker thread that pre-parses `LvlPrest.txt`, `LvlTypes.txt`, `LvlDef.txt`, `Levels.txt`, palette list, and the MPQ listfile. | Preset picker needs the tables parsed to show its list. Background does it without a startup stall. |
| 10 | **Explicit "Build Mod MPQ" + "Build & Launch D2" actions.** | Auto-build on save is wasteful and risks mid-write corruption if D2 has the MPQ open. The "build + copy into D2 dir + launch" composite is where the real ergonomic win is. |

## Architecture Sketch

```
┌─────────────────────────────────────────┐
│               Editor UI                  │
│  (DS1 canvas, preset picker, panels)    │
└─────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────┐
│        GameContent (read API)            │
│  openAsset(path) → bytes                 │
│  findPresetForDs1(path) → (type, def)    │
└─────────────────────────────────────────┘
                    │
         ┌──────────┴──────────┐
         ▼                     ▼
┌─────────────────┐   ┌─────────────────┐
│  OverlayResolver│   │  IndexCache     │
│                 │   │                 │
│  1. project/    │   │  LvlPrest rows  │
│  2. user mods[] │   │  LvlTypes rows  │
│  3. Blizzard    │   │  Palette list   │
│     chain       │   │  MPQ listfile   │
└─────────────────┘   └─────────────────┘
         │
         ▼
┌─────────────────┐
│   StormLib       │
│  (read + build)  │
└─────────────────┘
```

- **Project** owns the install path, the extra-mods list, and the loose-file working copy on disk.
- **OverlayResolver** answers `openAsset(path)` by walking: project folder → user extra mods → Blizzard chain. First hit wins.
- **IndexCache** is populated on a background thread at project open. Its contents power the preset picker and auto-parameter-resolution.
- **StormLib** is isolated to two spots: MPQ reads (via OverlayResolver) and the build step.

## Phased Implementation

Each phase is a reviewable commit/PR's worth of work.

### Phase 1 — MPQ read backend

**Scope revision after code audit.** The existing codebase already contains
both an MPQ reader (`src/mpq/MpqView.c`, supports D2's compression methods) and
a working chain-with-mod-dir-overlay at [src/misc.c:1150 `misc_load_mpq_file()`](../../src/misc.c#L1150).
The loop walks `glb_config.mod_dir[]` first, then every open `glb_mpq_struct[i]`,
first-hit wins — that's precisely the `OverlayResolver` the design doc
described. What was missing was the ergonomics: users had to paste four
absolute MPQ paths into Ds1edit.ini manually, and DrTester existed partly to
help them figure out which paths they needed.

Phase 1 therefore focuses on install-path ergonomics rather than rewriting the
reader:

- New module [src/core/d2install.c](../../src/core/d2install.c) /
  [src/core/d2install.h](../../src/core/d2install.h).
- `d2install_detect()` probes `HKLM\SOFTWARE\Blizzard Entertainment\Diablo II\InstallPath`
  in both the native and `KEY_WOW64_32KEY` views, then falls back to a short
  list of common install paths (`C:\Diablo II`, `C:\Program Files\Diablo II`,
  `C:\Program Files (x86)\Diablo II`, `C:\Games\Diablo II`). Each candidate
  must contain a recognisable D2 MPQ to be accepted.
- `d2install_resolve_mpqs()` runs at the tail of `ini_read()`: for every empty
  `glb_config.mpq_file[i]` slot, it tries `<install>\<slot_name>.mpq` and, if
  the file exists, allocates and assigns the path. Explicit per-MPQ INI entries
  always win — users who already had a custom config keep it unchanged.
- New INI key `d2_install = <dir>`. When set, supplies the install path
  directly. When empty, auto-detection runs.
- CMake wires `src/core/d2install.c` into the build and adds `advapi32` to the
  Windows link set (registry access).
- `Ds1edit.ini.sample` and the `ini_create()` template were updated to make
  `d2_install` the primary one-liner and downgrade the per-MPQ slots to
  optional overrides.

**Unit test deferred.** A DS1-round-trip test against a real install needs
access to copyrighted MPQs that can't be checked in. Testing the path-joining
and existence-check logic alone would be over-engineering. Real correctness
validation happens manually against a configured install; a scripted
integration test can come later if we set up a reference-MPQ fixture outside
the repo.

**StormLib deferred to Phase 7.** The existing reader handles all D2 MPQ reads
we need today. StormLib enters the codebase when we need *write* support to
pack the project's loose files into a mod MPQ; that's Phase 7's scope, and the
vendoring plus CMake integration fit cleanly there.

### Phase 2 — Project model

Split into 2a (backend) and 2b (UI wiring) to keep commits focused.

**Phase 2a — backend (shipped):**

- [src/core/preferences.c](../../src/core/preferences.c) /
  [preferences.h](../../src/core/preferences.h): per-user prefs stored at
  `%APPDATA%\ds1edit\preferences.ini`. Holds `last_d2_install` and an
  LRU list of recent project paths (cap = `PREFS_RECENT_MAX`). Explicit-path
  variants (`prefs_load_from`, `prefs_save_to`) exposed for tests so the real
  user appdata is never touched.
- [src/core/project.c](../../src/core/project.c) /
  [project.h](../../src/core/project.h): project model and persistence.
  Project = a directory with a `project.ini` manifest. Manifest keys:
  `name`, `editor_version`, `d2_install`, `extra_mod_mpq_N`.
  `project_apply_to_config()` points `glb_config.mod_dir[0]` at the project
  dir and fills `glb_config.d2_install` from the project if not already set.
- **INI rather than JSON.** The design originally said `project.json`; C
  parsing overhead for JSON (vendoring cJSON or jsmn) outweighed the
  nested-schema benefit given our flat data. Allegro's `al_config` already
  parses INI correctly, so both files use it.
- Startup wiring in [main.c](../../src/main.c): `ini_read` → `prefs_load` →
  seed `glb_config.d2_install` from prefs if empty → `d2install_resolve_mpqs`
  → record resolved install back into prefs. Shutdown writes prefs via
  `prefs_save()`.
- Unit tests in [test/test_project.c](../../test/test_project.c) cover
  preferences round-trip, recent-project LRU semantics, and project
  create/load/save against a scratch directory. Real APPDATA isn't touched.

**Phase 2b — UI wiring (shipped):**

The editor uses an immediate-mode custom UI with no native menus and no
free-text input widgets, so the plan landed on keyboard shortcuts plus the
OS's own file dialogs (Allegro 5's `allegro_dialog` addon) rather than
building a text-input dialog from scratch.

- [src/ui/project_menu.c](../../src/ui/project_menu.c) /
  [project_menu.h](../../src/ui/project_menu.h):
  - `project_menu_handle_shortcuts()` polled once per frame from the main
    input loop in [interface.c](../../src/ui/interface.c).
  - `project_menu_draw_indicator()` renders a "Project: \<name\>" label in
    the top-left from [misc_draw_screen()](../../src/misc.c) when a project
    is open.
- Shortcuts:
  - `Ctrl+Shift+N` — New Project. Opens a native folder picker; project
    name derived from the folder's basename; `d2_install` seeded from
    `glb_prefs.last_d2_install`. Refuses to overwrite an existing
    `project.ini`.
  - `Ctrl+Shift+O` — Open Project. Native folder picker; validates
    `project.ini` exists; errors via native message box if not.
  - `Ctrl+Shift+W` — Close Project. Native yes/no confirm.
- On successful create/open: applies the project to `glb_config`
  (`mod_dir[0]` = project dir), records the path into the recent-projects
  LRU, mirrors `glb_project.d2_install` into `glb_prefs.last_d2_install`,
  and persists prefs immediately via `prefs_save()`.
- CMake: links `allegro_dialog` and ships `allegro_dialog-5.2.dll`
  alongside the other Allegro DLLs; also links `comctl32`+`comdlg32` for
  the underlying Win32 dialogs the addon calls into.

**Not yet implemented (deferred follow-ups):**
- Recent Projects picker UI — the LRU data is already persisted in prefs,
  but no in-editor surface shows the list yet. Right now a user picks
  recents by using the OS folder picker's own history.
- Rename project / edit `extra_mod_mpqs` / reconfigure `d2_install` from
  inside the editor — all supported by the data model, no UI yet.
- Re-resolving the MPQ chain mid-session when `glb_config.d2_install`
  changes via a project open. Currently MPQs opened at startup stay open
  for the session; only the loose-file overlay (`mod_dir[0]`) swaps. This
  is correct for overlay semantics but means a project that points at a
  different D2 install than the one detected at startup won't see that
  install's MPQs until restart. Fine for the common case; revisit when
  the preset picker (Phase 5) needs it.

### Phase 3 — Table parsing + background indexer
- Tab-separated text table reader (D2 `.txt` files are TSV with one header row).
- Parsers for `LvlPrest.txt`, `LvlTypes.txt`, `LvlDef.txt`, `Levels.txt`.
- `IndexCache`: structs for each parsed row, ds1-path → (type_id, def_id) reverse index.
- Background worker: spawned on project open, populates cache, posts a "ready" event to the UI.
- UI indicator while indexing (spinner in status bar is enough).

### Phase 4 — Invisible MPQ-aware DS1 loading
- On open DS1: query `IndexCache.findPresetForDs1(path)` → `(type_id, def_id)`.
- Auto-load the referenced `.dt1` files and palette via `OverlayResolver`.
- DrTester-style parameter hunting is no longer needed — remove/mark-obsolete any docs that instruct users to use DrTester for this.
- Fallback: if the path isn't in the index (user's own custom DS1), fall back to current behavior (manual params or defaults).

### Phase 5 — Preset picker panel
- New dockable panel "Open by Preset."
- Rows: preset name (from `LvlPrest.txt` `Name`), level type name (from `LvlTypes.txt`), DS1 path, act.
- Filter box (substring match on name + path).
- Double-click: opens the DS1 with correct params via Phase 4 path.
- No preview thumbnail in this phase — defer to the full browser milestone.

### Phase 6 — Copy-on-save semantics
- Save of a document whose path resolves outside the project folder: write to `project/<path>` instead of the origin.
- UI affordance: window-title suffix "(read from MPQ — save will copy to project)" until first save makes it project-owned.
- Reload/refresh invalidates `OverlayResolver`'s caching of that path.

### Phase 7 — Build Mod MPQ
- Menu: File → Build Mod MPQ.
- Dialog: output path (defaults to `<project>/build/<projectname>.mpq`), compression toggle.
- Walks project folder, adds each file to a new MPQ via StormLib write API, writes listfile, closes.
- Shows build log (files added, total size, duration).

### Phase 8 — Build & Launch D2
- Menu: File → Build & Launch D2.
- Builds as Phase 7, copies output MPQ next to `Game.exe` in the configured install, launches `Game.exe` with `-direct -txt` (documented D2 mod-loading flags).
- User-configurable launch args per project (defaults to `-direct -txt`).
- Non-blocking: editor stays up while D2 runs.

### Optional follow-up phases (not part of initial milestone)

- **Phase 9 — User extra mod MPQs UI.** Wire the project.json `extra_mod_mpqs[]` list into an "Edit → Mod Layering" dialog.
- **Phase 10 — Full asset browser.** Tree, preview, extract, context menus. Separate scoped effort.
- **Phase 11 — D2R support.** Add a `CascChain` backend alongside `BlizzardChain`, keep `GameContent` API unchanged.

## Open Questions

- **Which D2 version is the reference target?** 1.13c and 1.14d are the common mod targets; the MPQ format is identical between them but some table columns differ. Pick one as the default test install and document it.
- **Case sensitivity in paths.** MPQ internal paths are Windows-style with `\`. Project folder on non-Windows (if cross-platform is ever in scope) needs a normalization layer. On Windows this is free.
- **Listfile quality.** Some MPQs ship without a complete listfile. For enumeration we may need a bundled community listfile (common in D2 modding); for name-addressed reads the hash lookup works regardless.
- **Palette selection UX.** A DS1 references a `LvlPrest` which references a `LvlTypes` row that names a palette. When the user opens a DS1 *not* in `LvlPrest` (a custom one), how do we pick a palette? Probably inherit from nearest-named preset or let user pick; revisit in Phase 4.

## Relationship to Other Branches

- Branched from `main` at merge `539656b` (tile-picker merge).
- Unrelated to `ai-tile-gen` branch. AI work remains on its own branch.
- The AI dataset pipeline (on `ai-tile-gen`) also benefits from MPQ access — being able to read the full set of stock DS1s through the overlay resolver means dataset prep can point at an install instead of a pre-extracted folder. That's a downstream win, not a dependency.
