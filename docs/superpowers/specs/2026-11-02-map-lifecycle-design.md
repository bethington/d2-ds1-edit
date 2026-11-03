# Map Lifecycle Design

Status: approved in conversation 2026-11-02, pending written-spec review.

## Goal

A map author can create, update and delete maps, build the changes into an MPQ
and test them in Diablo II, all from inside the editor, through menus and
hotkeys. The same workflow ships on Windows, Linux and macOS together.

Today the stages exist only in part and do not connect: maps opened from an MPQ
can save to a relative path that usually does not exist, nothing tracks unsaved
changes, nothing writes an MPQ, nothing launches the game, and there is no menu
system.

## Decisions

| Topic | Decision |
|---|---|
| Where edits live | In a project folder. Browsing needs no project. The first save or new map asks the user to pick or create one |
| Menus | A menu bar drawn by the editor, not native OS menus |
| Commands | One table drives menus and hotkeys |
| Test target | A separate test copy of the game, named by a launch profile. The editor never writes to the install it reads MPQs from |
| MPQ writing | The editor's own small writer, with no third-party library. It writes version 1 archives, stores files uncompressed at first, and keeps using the existing reader for all reads |
| Game versions | Classic Diablo II and Lord of Destruction 1.13c are the reference. D2R is out of scope |
| Rebindable keys | Out of scope. The command table leaves room for it |

## Map states

A map has one storage state and, separately, an unsaved flag. The editor derives
storage state by comparing the project with the game's MPQs; it stores no
separate list of states.

| State | Rule | Browser mark |
|---|---|---|
| Game | In the game's tables, with no project copy | none |
| Modified | In the game's MPQs, with a project copy at the same path | `M` |
| New | A project file not in the game's MPQs | `N` |
| Removed | In the game's `LvlPrest` row, but taken out of that row in the project's copy | dimmed and struck through |

Unsaved edits add `*` to any state.

To tell these apart, the editor reads `LvlPrest.txt` from the MPQ chain without
the project overlay, as well as through it. The browser lists the union, so
Removed maps stay visible and can be restored.

## Lifecycle operations

### Open and close a project

Opening or closing a project reloads the area browser and invalidates the
cached tables. Closing with unsaved maps offers Save All, Discard or Cancel.
Closing restores the `mod_dir` from the INI, instead of leaving it cleared.

`mod_dir` stays supported as a read overlay. It is no longer a save target, so
all writing logic has one model: the open project.

### Create

**Map > New** opens a dialog for name, area, width, height and tileset. It
defaults to the open map's area, size and tileset.

- The name must be unique in the area and valid on all three platforms.
- If the project has no copy of `LvlPrest.txt`, the editor copies it from the
  MPQ chain before registering the map.
- A map can only join a row whose `Files` column equals its filled cells. Create adds it
  at the end and raises `Files`. If the row is full, chooses its maps by its own rules, or
  the Def is not found, the dialog says so and writes nothing. Create is all-or-nothing: on a failed table write, the new `.ds1` is
  removed.

**Map > Clone** works the same way and copies the map as it is in memory, so it
includes unsaved edits and works on maps from an MPQ.

### Update

Saving always targets the project, using the existing staging file and numbered
backups in `ds1_save_io.c`.

- With no project open, the first save shows the project prompt. Cancelling
  writes nothing and keeps the edits in memory.
- The redirect handles the relative names the area browser uses, such as
  `Act1/Town/TownN1.ds1`. `project_redirect_ds1_save_path` treats these as
  tiles-relative instead of passing them through unchanged.
- **File > Save All** saves every unsaved map and lists any that failed.
- Ctrl+S with no map open is disabled, instead of reporting a false success.

**Unsaved tracking.** Each open map gets a dirty flag, set by tile, object, path
and DS1 property edits, cleared by a successful save, and restored to clean when
undo returns to the saved state. Opening a map from the area browser unloads every
open map, so it prompts first if any of them is unsaved; today those edits are lost
with no warning. `Ctrl+1`–`Ctrl+0` and the number keys only switch between loaded
maps, so they never prompt. Closing a project or quitting also prompts only if
something is unsaved.

### Delete

The action depends on storage state, because the game's MPQs are never changed.

| State | Menu command | Effect | Undo by |
|---|---|---|---|
| New | Delete Map | Moves the file to `<project>/backup/`, takes it out of its row and lowers `Files` | Restore from Backup |
| Modified | Revert to Game Version | Moves the project copy to `<project>/backup/` | Restore from Backup |
| Game | Remove from Project | Takes it out of its row in the project's `LvlPrest.txt` and lowers `Files` | Restore Slot |
| Removed | Restore Slot | Puts the game's value back in the row and raises `Files` | Remove from Project |

Every one of these confirms first, naming the map and what will happen.

Taking a map out of a row moves later cells back and lowers `Files`, because the game
crashes on a blank cell below `Files`. A row's only map can't be removed, and neither can a
map in a row that chooses its maps by its own rules: the command says so and changes nothing.

The backup folder moves from the working directory into the project. A backup is
only recorded after the map data is copied, and an operation with nothing to back
up stops with a message. Permanent deletion from the Backup group also confirms.

The Delete key fix on `claude/xenodochial-nightingale-38462e` is a prerequisite.
It makes `Del` act only on the selection and routes map deletion through a browser
right-click with confirmation. This design adds the state rules above to that
path, and `Ctrl+Del` as a second way to reach it.

### Build

**Build > Build MPQ** packs the project's changed files into
`<project>/build/<project name>.mpq`, replacing the previous output only after the
new file is complete.

- It includes `Global/Tiles` and `Global/Excel`, stored under the `data\global\...`
  virtual paths, with a listfile. Backups, `build/` and `project.ini` are excluded.
- It refuses to run with unsaved maps, offering Save All and Build, or Cancel.
- It validates first: every map named in the project's tables must resolve, every
  project map must be registered, and each file's path must fit an MPQ virtual
  path. Failures list the map and the problem, and nothing is written.
- It reports files added, total size and duration.

### Test

**Build > Build & Test** builds, installs the result in the test copy and starts
the game. The editor stays open while the game runs.

The launch profile is stored in `project.ini`:

| Key | Meaning |
|---|---|
| `test_game_dir` | The separate game folder used for testing |
| `test_exe` | Executable, relative to `test_game_dir`. Default `Game.exe` |
| `test_args` | Arguments. The default comes from the gate below |
| `test_wrapper` | Optional launcher such as `wine`. Empty on Windows |

**Build > Launch Profile** edits these. Until the profile is complete, Build &
Test opens that dialog. The editor refuses a `test_game_dir` that resolves to the
same folder as the configured `d2_install`.

- **First-use validation:** the folder must contain `test_exe`, and the install
  step must be able to write there.
- **Game already running:** if writing the output fails because the file is in
  use, the editor says to close the game and changes nothing else.
- **Launch failure:** a missing wrapper or executable shows the reason and the full
  command line.

## Gate: how the game loads the output

Step 4 must not be designed in detail until this is answered. The older plan
copied an MPQ next to `Game.exe` and ran `-direct -txt`, but that was never
verified. Diablo II 1.13c may not load an extra MPQ of arbitrary name, and may read
compiled `.bin` tables rather than the `.txt` files a project edits.

Verify against the game binaries and a real test copy:

1. Which MPQs does 1.13c load, in what order, and can an extra one be added without
   third-party loaders?
2. What exactly does `-direct` read, from which folder?
3. What does `-txt` do with an edited `LvlPrest.txt`, and is a stale `.bin` used
   otherwise?
4. Which loader mechanism does Project Diablo 2 use, and can the same profile fields
   describe it?
5. Does the game's own MPQ code accept an archive from the editor's writer, with
   uncompressed files? Test this with a small hand-built archive before writing the
   full build step. If it rejects them, the writer must add the compression the game
   needs, using an existing reference for the format rather than a new dependency.

Outcomes:

- **A supported way to load an extra MPQ exists:** install it into the test copy and
  use the verified arguments as the `test_args` default.
- **It does not:** Build MPQ still produces the MPQ for distribution. Build & Test
  installs by the verified route, such as loose files under `data\` with `-direct`,
  and the spec is updated to name that route.

Whichever applies, a test must show a changed map appearing in the running game
before step 4 ships.

## Menus

The bar is drawn with the editor's existing UI primitives, in a new row above the
HUD rows. The bar overlays the top of the map view rather than moving its edge,
and hit testing treats the bar's rows as the bar's. It must keep working under the software renderer planned for 2.3.0.

Input rules:

- `Alt+F`, `E`, `M`, `V` and `B` open a menu. Arrow keys move, `Enter` runs a command
  and `Esc` closes.
- An open menu owns all input, so nothing reaches the map behind it.
- `Esc` closes a menu first; only with no menu open does it reach the quit dialog.
- Unavailable commands appear dimmed, and show their hotkey either way.

| Menu | Commands |
|---|---|
| File | New Project `Ctrl+Shift+N`, Open Project `Ctrl+Shift+O`, Close Project `Ctrl+Shift+W`, Save `Ctrl+S`, Save All `Ctrl+Shift+S`, Project Settings, Quit |
| Edit | Undo `Ctrl+Z`, Cut `Ctrl+X`, Copy `Ctrl+C`, Delete Selection `Del` |
| Map | New `Ctrl+N`, Clone `Ctrl+D`, Revert to Game Version, Delete Map `Ctrl+Del`, Remove from Project, Restore Slot, Restore from Backup, Properties `]` |
| View | Layer toggles `F1`–`F12`, Zoom, Grid `G`, Areas Panel `[`, Properties Panel `]` |
| Build | Build MPQ `Ctrl+B`, Build & Test `Ctrl+Shift+B`, Launch Profile |

Map commands that do not apply to the selected map's state are dimmed, so Revert is
only available for Modified maps. `Ctrl+Del` runs whichever of Delete Map, Revert or
Remove applies to that state.

## Commands and hotkeys

A new `src/ui/commands.c` holds a table of commands. Each entry has an ID, label,
menu placement, key, exact modifiers, availability test and handler. The menu bar
and the keyboard dispatcher both read it, so a menu cannot disagree with a hotkey.

- **Exact modifiers:** `Ctrl+N` does not fire plain `N` (night mode), and `Ctrl+S`
  does not scroll. This replaces the polling that lets those collide today.
- **Mode awareness:** availability tests include edit mode and modal state. `Insert`
  returns to its object-mode role only, and neither it nor `Shift+Insert` creates
  maps any more.
- **macOS:** `Cmd` is accepted wherever `Ctrl` is, on that platform only.
- **Migration:** lifecycle, project and build commands move into the table in step 2.
  Scrolling, layer toggles and selection stay polled in `interface.c`, and move only
  when they conflict with a command.
- **Conflict test:** a unit test fails if two available commands share a key, modifiers
  and mode.
- **Docs:** the README and manual control tables are updated from the table, including
  `Ctrl+Shift+A`, `Ctrl+Shift+P` and every lifecycle key.

Existing keys unchanged: layers, zoom, grid, `Tab`, `Ctrl+Z`, `Ctrl+U`, `Ctrl+C`,
`Ctrl+X` and the project shortcuts.

## Components

| Unit | Responsibility | Depends on |
|---|---|---|
| `core/map_state.c` | Derive a map's storage state and dirty flag | project, MPQ view, tables |
| `core/project.c` | Project folder, `project.ini`, save redirect, launch profile | filesystem |
| `core/ds1_manager.c` | Create, clone, delete, restore; all-or-nothing table updates | map_state, project |
| `core/mpq_build.c` | Validate, then choose the files to pack; no UI | `core/mpq_write.c`, project, filesystem |
| `core/mpq_write.c` | Write a version 1 MPQ from a list of files: header, encrypted hash and block tables, stored file data, `(listfile)` | hashing and encryption shared with `src/mpq/MpqView.c` |
| `core/game_launch.c` | Install to the test copy and start the game; platform differences live here | `platform.h` |
| `ui/commands.c` | Command table and dispatch | none in core beyond handlers |
| `ui/menu_bar.c` | Draw the bar, and handle menu input | commands |

`mpq_build` and `game_launch` take explicit paths and report errors as text, so
they can be tested without a display and used later from the CLI.

The writer adds no library, so packages gain no runtime file or system dependency.
It needs only stored files: maps and tables are small, and compression can be added
later if the output size becomes a problem.

## Build order

Each step ships separately, on all three platforms together.

1. **Projects and saving:** project prompt, redirect fix, map states, browser marks,
   unsaved tracking, reload on project change.
2. **Commands and menus:** command table, menu bar, new bindings, updated docs.
3. **Create and delete:** new-map dialog, clone, the state-based delete rules, backups
   in the project.
4. **Build and test:** the gate, then the MPQ writer, Build MPQ, launch profile and
   Build & Test.

## Testing

| Area | Coverage |
|---|---|
| Map states | Unit tests over a fake project and table for each state and transition |
| Save redirect | Relative, absolute, in-project and too-long paths, with no game folder writes |
| Unsaved tracking | Set by each edit type, cleared by save, restored by undo to the saved state |
| Create, clone, delete, restore | Temp-folder tests, including failure cleanup and a full round trip back to the original table |
| Command table | Conflict test, exact-modifier matching, availability by mode |
| MPQ build | Write an MPQ, read it back with the editor's own reader, and compare bytes; validation failures write nothing |
| Launch | Fake executable records arguments, working directory and wrapper; a game folder equal to `d2_install` is refused |
| Menus | Input-model unit tests, plus additions to the native release checklist on each platform |
| End to end | A manual checklist item per platform: create a map, edit, delete, build, and see it in the game |

All GUI runs stay windowed. Tests never write to a real game install.

## Out of scope

Rebinding keys, an in-editor build log viewer, multiple test profiles per project,
`extra_mod_mpqs` UI, editing arbitrary non-map tables, and installing or copying the
game for the user.
