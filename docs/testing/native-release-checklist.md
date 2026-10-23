# Native release checklist (manual GUI workflows)

This is the human half of release requirement R8 (design 10.8). The automated
half is `scripts/equivalence/native_smoke.py`, which runs the *extracted*
package for `cli-help`, `no-game-startup`, `selftest` and a Windows/Linux
`platform-startup`. A `--selftest` proves the window opens, frames draw and the
input pump runs. It proves nothing about selecting, editing, undoing, saving or
exporting, so those stay here and can only be marked done by a person who ran
them against the candidate package.

Nothing in this document is a pass until a result has been recorded as a
`ds1edit.case/1` file (see [Recording results](#recording-results)). A result
for one package says nothing about another build or platform.

## Status of this host

- **A clean-VM Windows run has not been done.** The development host has the
  Visual C++ redistributable and other tooling installed, so a run here cannot
  show that the package starts on a machine that has none. The case
  `native.windows-x86.clean-environment` therefore stays `unavailable`, and
  `native_smoke.py` emits it that way on purpose. Do not replace it with a
  developer-machine pass.
- The Windows package now ships `vcruntime140.dll` (checked by
  `package.windows-x86.runtime_closure`). That is a content check, not proof
  that a bare machine starts; only the clean-VM run is.
- Linux and macOS candidate packages have not been exercised by this
  checklist. Their cases are `unavailable`/`not_run` until someone runs them.
- **Open defect: fullscreen Direct3D exit hang (2026-10-22, Windows).**
  `--selftest` with `full_screen = YES` printed its final "rendered 30
  frame(s), exiting" lines and then the process did not exit; the harness
  killed it after 180 seconds. The same map with `full_screen = NO` exits 0.
  It reproduced 4 of 4 times on act1_town with fullscreen Direct3D. A sample of
  the hung process shows the main thread spinning in `al_destroy_display` ->
  `d3d_destroy_display_internals` -> `_al_convert_to_memory_bitmap`, that is,
  Allegro converting every remaining video bitmap to a memory bitmap while the
  display is torn down. The fault is not yet fixed. Candidate fixes: destroy
  the editor's bitmaps before the display, or skip `al_destroy_display` at
  process exit. Fullscreen is deprioritised for this release, the automated run
  uses a windowed owned INI, and item M4 below covers shutdown in both modes so
  the hang is looked at on a real machine until it is fixed.

## Ground rules for every run

1. **Test the candidate, extracted.** Obtain the exact archive the qualification
   candidate names. Extract it with
   `python -m scripts.equivalence.package_check --archive PKG --platform PLATFORM --version VERSION --out-dir OWNED_DIR`
   (or, on a machine without Python, unpack it yourself and record the archive's
   SHA-256). Run only the extracted executable. Never run `bin/`, a build tree,
   or a copy you have used before.
2. **Own the configuration.** The extraction must contain no `Ds1edit.ini`
   (the package forbids one). Create one beside the executable containing only:

   ```ini
   d2_install = <path to the vanilla 1.13c install>
   full_screen = NO
   screen_width = 1280
   screen_height = 720
   ```

   Preferences are stored per user (`%APPDATA%\ds1edit\preferences.ini`,
   `$XDG_CONFIG_HOME/ds1edit/preferences.ini` or `~/.config/ds1edit/`) and can
   seed `d2_install` on a later launch. Start from an account where that file
   does not exist, or launch with `APPDATA` / `XDG_CONFIG_HOME` pointed at an
   empty directory, and say which you did.
3. **Use a game install you can name.** Record the path and the four MPQ file
   sizes (`patch_d2.mpq`, `d2exp.mpq`, `d2data.mpq`, `d2char.mpq`). The
   reference build is Diablo II 1.13c (Lord of Destruction); other versions are
   out of scope.
4. **Work on copies.** Saving writes the map and a numbered backup. Edit maps
   you opened through a mod directory or a scratch copy, never the only copy of
   anything. Delete or re-extract between runs.
5. **Record what you saw, not what you expected.** A step that behaved
   differently, hung, flickered or needed a retry is a failure of the item
   unless the notes say why it is not.

## Environments

### Windows x86 (`windows-x86`)

- **Required for `clean-environment`:** a fresh VM or base image of Windows 10
  or 11 with no Visual C++ redistributable, no Visual Studio, no vcpkg, no
  Python, no Allegro, and no Diablo II registry key. Disclose its build number,
  any updates or runtimes it carries, and how you checked that
  `vcruntime140.dll` and friends are absent from `System32`/`SysWOW64`
  (`where vcruntime140.dll` finds nothing outside the extraction).
- The game files may live on a mapped or shared folder; say so.
- A developer machine may run sections E and M for functional coverage, but its
  results may be recorded only as `gui-edit-undo-save-reopen` /
  `gui-modal-input`, never as `clean-environment`.

### Linux x86_64 (`linux-x86_64`)

- A supported distribution with only the documented prerequisites installed:
  `liballegro5.2 liballegro-image5.2 liballegro-ttf5.2 liballegro-dialog5.2`
  (Debian/Ubuntu) or `allegro5 allegro5-addon-image allegro5-addon-ttf`
  (Fedora). No `-dev` packages and no compiler. Record `ldd ./ds1edit` and
  confirm nothing is `not found`.
- Run under a real desktop session; note X11 or Wayland.

### macOS arm64 (`macos-arm64`)

- Apple-silicon Mac with `brew install allegro` as the only prerequisite.
- Launch the **`ds1edit.app` bundle from the extracted package**, not just the
  bare `ds1edit` binary, because the bundle is the advertised entry point and
  the working-directory fix-up (`ds1_mac_fix_working_directory`) only matters
  there. The package is ad-hoc signed; the first launch needs
  right-click Open or `xattr -dr com.apple.quarantine`.
- Allegro's keyboard needs **Input Monitoring**. Record whether macOS prompted,
  whether you granted it to the app (System Settings, Privacy & Security), and
  whether the keyboard worked before and after. A build whose keys never reach
  the editor fails P-items even if the window draws.

## Case `gui-edit-undo-save-reopen` (items E1 to E13)

Use act 1 Town and at least two other maps from different acts.

- [ ] **E1 Launch.** Start the extracted executable with no arguments. The area
  browser lists areas; the window title and status bar render; no error box.
- [ ] **E2 Map browsing and switching.** Open three maps from three acts, switch
  between them and back. Each draws completely, the previous map's tiles and
  objects do not bleed into the next, and an edit made on map A is not visible
  on map B.
- [ ] **E3 Tile selection.** In tile mode: click, drag a rectangle, **Shift**
  adds, **Ctrl** removes. **I** selects identical tiles. **Alt+I** (strict
  identical) selects only cells whose complete visible floor, shadow and wall
  content matches the clicked cell; on a cell where plain **I** also picks up
  cells that merely share one tile, Alt+I selects fewer, and never more.
- [ ] **E4 Object selection.** In object mode: click, **Shift**/**Ctrl**,
  and **Alt-drag** from an unselected point box-selects every object whose
  centre is inside the rectangle. Alt-drag that starts on an already selected
  object moves it instead. Repeat for labels.
- [ ] **E5 Path selection.** In path mode select a path point, move it, and
  undo/redo the visible result; selection state is sane afterwards.
- [ ] **E6 Undo.** Make a tile edit and press **Ctrl+Z**: restored. Make another
  and press **Ctrl+U**: also restored. Undo several steps back to the start.
  After undoing and then editing, the old redo history is gone (per manual).
  Undo does not run while a paste preview is active.
- [ ] **E7 Properties panel ownership.** Open the properties panel and begin an
  edit (or leave edits pending): **Ctrl+Z** does *not* undo tiles; **Ctrl+U**
  still does. With the panel visible but idle, **Ctrl+Z** undoes tiles again.
- [ ] **E8 Zoom and resize.** Change zoom through each level and back, then
  resize the window by dragging an edge, maximise and restore. The map redraws
  at once, with no black map area, no crash, and the selection survives.
- [ ] **E9 Save and backup.** **Ctrl+S** after an edit. The save dialog or
  message appears, the file changes, and a numbered backup of the previous
  version (for example `name-000.ds1`) exists.
- [ ] **E10 Reopen.** Quit, relaunch the same extracted package, reopen the
  saved map: the edit is present and undo history starts empty.
- [ ] **E11 Unsaved-change handling.** With an unsaved edit, quit. The WARNING
  dialog offers (S) Save all and quit, (Q) Quit, (Esc) Cancel. Esc returns to
  the editor with the edit intact. Q quits and leaves a `.tmp` autosave and
  does not overwrite the map. S saves and quits.
- [ ] **E12 Failed save is non-fatal.** Make the target unwritable (read-only
  file, or a folder you cannot write), edit, press **Ctrl+S**. An error box
  titled "The map was not saved" appears with a reason; the editor stays open,
  the map still holds the edit, and the file on disk is byte-identical to
  before. Fix the permission and **Ctrl+S** succeeds. Do the same for the
  autosave (`.tmp`) path if you can.

- [ ] **E13 Scrolling and map navigation.** Arrow keys and **W A S D** scroll
  the map, including two keys at once (diagonals) and holding a key for a
  continuous scroll. **Ctrl+arrow** keys (not plain arrows) browse to the
  neighbouring map in the area browser; plain arrows never switch maps. With
  the default `mouse_edge_scroll = NO`, resting the mouse at the window edge
  does not scroll. Set `mouse_edge_scroll = YES` in `Ds1edit.ini`, restart, and
  edge scrolling works; set it back to `NO` afterwards. While a text field or
  modal is open, the arrow keys and **W A S D** do not scroll the map behind it.

Total items: 13.

## Case `gui-modal-input` (items M1 to M4)

- [ ] **M1 Modal key isolation.** Open a modal (a picker, text-input or the
  export dialog) with a tile selection active. Type **I**, **N**, **Z**, **U**,
  **Delete**, **S** and arrow keys. The selection, mode, tiles and undo state of
  the map behind it are unchanged; nothing is saved, deleted or undone.
- [ ] **M2 No stuck or replayed keys.** Close the modal with a key held; the map
  must not act on it afterwards. Switch to another application and back while a
  key is held; no key stays "down" in the editor.
- [ ] **M3 Export cancellation.** Start an export with many items. The progress
  window shows stage and counts. Press **Cancel**. The summary reads that the
  export was canceled with partial output kept, files already written are
  complete, and the editor remains responsive. Run a second export afterwards.
- [ ] **M4 Shutdown.** Quit normally with the window close button and again with
  the quit dialog. The process exits within a few seconds with status 0 (Windows:
  `echo %ERRORLEVEL%` after launching from a console), no process is left
  running, and `tmp/undo*.bin` is removed. Repeat once with `full_screen = YES`.
  On Windows with Direct3D this is the known fullscreen exit hang (see "Open
  defect" above: 4 of 4 reproductions, main thread in `al_destroy_display` ->
  `d3d_destroy_display_internals` -> `_al_convert_to_memory_bitmap`). Until the
  fix lands, record a fullscreen exit that does not complete within 10 seconds
  as a failure of M4 and name the process state you saw; after the fix, the
  fullscreen exit must meet the same few-second bound as windowed mode.

Total items: 4.

## Case `platform-startup` (items P1 to P3, per platform)

On Windows and Linux the automated run already starts `help` under a minimal
declared environment; these items cover what it cannot.

- [ ] **P1 First launch.** Windows: double-click the extracted `ds1edit.exe`.
  Linux: run `./ds1edit` from a desktop terminal and once from the file manager.
  macOS: open the extracted `ds1edit.app`. The window appears with no loader or
  missing-library error.
- [ ] **P2 Input reaches the editor.** Keyboard and mouse both work: move the
  mouse over the map, press **Tab** (mode change) and a letter hotkey.
  macOS: complete the Input Monitoring prompt and confirm keys work *after*
  granting, and say what happened before.
- [ ] **P3 Identity.** macOS: `codesign -dv ds1edit.app` shows the ad-hoc
  signature and bundle identifier from the package inspection; the version
  shown in Finder's Get Info matches the package version. Windows/Linux: the
  executable's embedded version (Windows: file Properties, Details; Linux: `strings
  ds1edit | grep X.Y.Z`) matches the package version.

Total items: 3.

## Case `clean-environment` (items C1 to C3, per platform)

Run only in the environments named above, and only after disclosing them.

- [ ] **C1** Start the package from the clean image with no development tools:
  `ds1edit.exe help` (or `./ds1edit help`) prints the usage text and exits 0.
- [ ] **C2** The GUI starts with the owned INI and reaches the area browser.
- [ ] **C3** Run the automated cases (`native_smoke.py run`) from that same
  image if Python is available, or `ds1edit --selftest=30 assets/act1_town.ini`
  with the owned INI and note the printed `display:` line and exit status.

Total items: 3.

## Recording results

Each case is recorded separately with `native_smoke.py manual`, which writes a
`ds1edit.case/1` file bound to the package hash and executable hash of the
automated run on the same extraction:

```text
python -m scripts.equivalence.native_smoke run ^
    --extracted-root OWNED\extracted ^
    --profile test/equivalence/profiles/native-core.json ^
    --out-dir OWNED\native-run --platform windows-x86 ^
    --game-install "<1.13c path>"

python -m scripts.equivalence.native_smoke manual ^
    --case gui-edit-undo-save-reopen --platform windows-x86 --result pass ^
    --smoke-report OWNED\native-run\native-smoke-report.json ^
    --tester "Your Name" ^
    --environment "Windows 11 23H2 clean VM, no VC++ redist, 1.13c on D:\" ^
    --notes "E1-E13 all done; E8 needed a second resize to redraw" ^
    --items 13 --total 13 --out RESULTS\native.windows-x86.gui-edit-undo-save-reopen.json
```

Rules the tool enforces and `qualify.py` then checks:

- `--items` below `--total` records `partial`, and `--result fail` records
  `mismatch`. Neither qualifies. Only all items done with `pass` becomes `pass`.
- The case ID is `native.<platform>.<case>` and the platform must match the
  package. The package SHA-256 and executable SHA-256 come from the smoke
  report, so a result for a different archive fails `qualify.py` as stale.
- `clean-environment` is only marked as a clean environment when you pass
  `--case clean-environment`; do that only from the image described above.
- Put the case files in the same directory `qualify.py --results-dir` reads, or
  copy them next to the automated results in `OWNED\native-run\results\`.
  Keep screenshots, console output and the disclosed environment in the
  retained evidence bundle, named in `--notes`.

Items you could not run are not recorded as done. Record the count you did,
say which were skipped in `--notes`, and the case stays `partial`.

## Where the data path is already covered

The portable C suites check the saved-bytes behavior that the GUI relies on, so
this checklist does not repeat those byte-level checks:

- `test_ds1_save` (`ds1.save.backup`, `ds1.save.tmp`, `ds1.save.write-failure`,
  `project.save-redirection`): backup naming, temporary file, atomic failure
  handling, no partial writes.
- `test_editor_undo` and `test_object_path_edits`
  (`editor.undo.tiles`, `editor.edits.objects-paths`, `editor.map-isolation`):
  tile undo, object/path edits, isolation between loaded maps.
- `test_input_edges`: key edge handling and the Ctrl+Z / Ctrl+U ownership rule
  (`input_undo_shortcut_hit`).
- `test_selection_logic`, `test_editor_selection`: selection rules including
  strict identical matching.

The GUI items above check that the shipped executable actually wires those
paths to the keyboard and mouse and shows the right messages.
