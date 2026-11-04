# Map Lifecycle Step 4 Gate: How the Game Loads the Output

Answers the "Gate" section of `2026-11-02-map-lifecycle-design.md`, for Lord of Destruction 1.13c. Runtime results come from `lod-113c` in a D2Fleet slot, windowed, on 2026-11-03. Static results and their addresses are in `2026-11-03-map-lifecycle-gate-re-findings.md`, read from the 1.13c binaries, whose hashes match that install.

## Outcome

There is no supported way to load an extra MPQ of an arbitrary name, so the spec's second outcome applies:

- **Build & Test** installs loose files under the test copy's `DATA\GLOBAL\...` and starts the game with `-direct -txt`.
- **Build MPQ** still produces an MPQ for distribution. Its name is `patch_d2.mpq`, the only top-priority slot with no code changes, so it must carry the retail `patch_d2` entries as well as the project's.

## Answers

| # | Question | Answer | Evidence |
|---|---|---|---|
| 1 | Which MPQs load, and can one be added? | A fixed list of 13 names. `patch_d2` is top at priority 5000, above `d2exp` at 3000. It is optional, and nothing checks a signature. No switch, registry value or config adds an archive | Static |
| 2 | What does `-direct` read? | `DATA\...` loose files relative to the working folder first, then the MPQs | Flag set and a loose-only file opened through Storm: runtime. Fallback: runtime, the game ran with almost nothing loose |
| 3 | What does `-txt` do with an edited `LvlPrest.txt`? | At game start, not at launch, it compiles all tables and writes `.bin` files to `DATA\GLOBAL\EXCEL`. An edited loose `LvlPrest.txt` reached `lvlprest.bin`, and the game entered town | Runtime |
| 3a | Is the folder made for us? | No. With no `DATA\GLOBAL\EXCEL` the game still runs, and the edits are silently ignored. Build & Test must create the folder | Runtime |
| 3b | Stale `.bin`? | The game loads `.bin` with no staleness check. `-txt` rebuilds them each game start | Static, plus the rebuild at runtime |
| 4 | Project Diablo 2? | Its DLLs call the same mount function at priority 6000, so its archives beat `patch_d2` for what they hold. The launch profile can describe it: `test_game_dir` is the PD2 folder and `test_exe` is `Game.exe` | Static. Not run with `-direct` |
| 5 | Does the game's own MPQ code accept our archive? | Yes. A hand-built version 1 archive with two stored files, flag `0x80000000`, and no `(listfile)`, was mounted by the game's `Storm.dll` in the running game. It read back whole, took priority over the real `TownN1.ds1`, and matched in any case and either slash | Runtime |

## How the Game Uses File Cells

**Step 3 as first merged didn't match this, and the plan `2026-11-03-map-lifecycle-3b-files-column` fixes it.** As merged, Create filled an empty cell and left `Files` alone, and Remove and Delete wrote `0` into a cell and left `Files` alone. Now Create adds the map at the end and raises `Files`, and Remove and Delete move later cells back and lower `Files`.

**Released mods may hold unsafe tables.** 2.2.0 wrote both unsafe kinds: a `0` below `Files`, from Remove and Delete, and a map past `Files`, from Create. The editor now refuses to change a row with a hole, says which cell, and leaves the fix to the author. The game's own table has one such row too: Def 915, Act 5 Barricade 1, has `Files` of 3 and two maps. That row hasn't been tested.

Tested on 1.13c with `-direct -txt`, using the four Act 1 Wild Border rows (Defs 4-7), which are `Files=3` with five filled cells. Each run used the same character, so the same map seed.

| Table change | What Step 3 does | Result |
|---|---|---|
| Sixth cell names a missing map, `Files` stays 3 | Create | No crash, on a walk past the point where the next row crashed. The cell wasn't opened |
| The three counted cells set to `0`, `Files` stays 3 | Remove, Delete | **Game crash** when those rooms generate: access violation at `D2Common+0xA57C`, the unchecked pointer in `LoadAndParsePresetArchive`. A level can load first, so the crash comes later, while walking |
| Sixth cell filled, `Files` raised to 6, with a real map or a missing one | Not done today | **Game crash** while loading, at `D2Common+0x3C97C`, a different site. These rows' extra cells have their own meaning |

**Raising `Files` on ordinary rows works.** Tested on the 995 rows where `Files` equals the filled cells and is 5 or less:

| Table change | Result |
|---|---|
| New last cell is a copy of the row's first map, `Files` raised by one | No crash, entering town and walking about 100 subtiles through Blood Moor |
| New last cell names a missing map, `Files` raised by one | No crash on the same walk, so no new cell was picked on that route. This run alone proves nothing |
| `Files` set to 6, first map kept, cells 2 to 6 missing | **Crash while loading,** at `D2Common+0xA57C`. The random pick does read cells past the original count |

So a map added at the end with `Files` raised is reachable, and doesn't disturb the row's loading.

**Tables the fixed editor writes.** A table from the editor's Create, with `BarW5` added to Def 168 and `Files` at 5, compiled and entered the game. The Barracks rooms weren't generated in that run, so the new map itself wasn't drawn. Remove's edit, first map out, the rest moved up and `Files` lowered, applied to all 400 rows with two or more maps, ran from town into Blood Moor with no crash.

**What `SizeX` and `SizeY` hold.** They are the numbers a DS1 header stores, the map's width and height minus one. `barW.ds1` stores 10 and 14, and its row, Def 168, has `SizeX` 10 and `SizeY` 14, so the editor shows it as 11 by 15. All 1,821 filled cells below `Files` in rows with a size name a map that matches its row this way, in 1.13c's table and the map files, read on 2026-11-03. So New defaults to `SizeX` plus one and `SizeY` plus one. This says what the columns hold, not what the game does with a map of another size.

From the binaries, a row with `Files` of 0 takes index -1 and the generator chooses the cell itself: town rows and transitions work this way. Otherwise the game picks a random index, and the row's other cells are not random choices. Of the 1,088 rows with any cell filled, 1,007 have `Files` equal to their filled cells, 34 have `Files` of 0, and 47 differ, like Wild Border.

So:

- **Create:** a map in an empty cell is never chosen by the random pick. Reaching it means raising `Files`, which works where `Files` already equals the filled cells, as the table above shows. It's unsafe on rows like Wild Border, and meaningless on `Files=0` rows.
- **Remove and Delete:** clearing a counted cell can crash the game for anyone who enters that level. They must instead move the later cells down and lower `Files`. Removing a row's only map leaves `Files=0`, whose generator was not traced, so it needs its own rule.
- **Restore Slot:** must restore `Files` too. It's the same state change in reverse.
- **Build MPQ:** carries this table to players, so the same crash would ship.

The crash test ran with every counted cell cleared. A single cleared cell should crash only when that index is picked, so it may pass a short test and fail later.

**A map of a different size in a row.** Four mass runs (`size-control`, `size-swap`, `size-same`, and 1b's R0 over 209 rows) and seven single-row runs (1b's R1 to R7) on 1.13c, the 1b ones read from `task-1b-report.md`. `size-control` (182 sized Act 1 rows, every counted cell a missing file) crashed at load with `Error opening file` and an access violation at `D2Common+0xA57C`, so those rows are picked. Two separate things then halt the game, and an empty map has neither.

- **A map whose size differs from its row, on a row with `Scan` or `Pops` set, halts at load.** `D2Common+0xC664` (`6fd5c664`), line 2239 for width and 2240 for height. Proved by code and by a single-row run (Def 52, `Scan` 1, with `CaveW.ds1`, 24 by 24 into an 8 by 8 row). It is the halt `size-swap` hit. In 1.13c's table 279 of the 1,054 sized rows have `Scan` or `Pops`.
- **On a `Scan` 0 row a map of another size doesn't halt on the way to the room, and draws wrongly** (Def 46 with `Swamp5.ds1`, 16 by 16: bands of black). Proved by a run only. A map smaller than the row may read past its buffer. That is inferred, not run.
- **A wall cell of orientation 10 or 11, main index below 8, top bit clear, with no matching link in its room, halts during the walk,** `D2Common` `6fdb9400`, line 689, when the room's tiles are built. Proved by code and by a single-row run (Def 46 with `clfcave.ds1`, same size). The mapping of the array it checks to `Warp0-7`, and of the room's list to the level's warp links, is inferred from layout, and who fills that list wasn't traced. A top-bit warp cell (`CaveDr1.ds1`, same size) didn't halt on a `Scan` 0 row.
- **`size-same`'s halt at `6fdb9400` is probably this second one,** since its line and address match, and 4 of its Act 1 substitutions (Defs 24, 25, 51, 52, taking `clfcave` or `clfcave2`) carried such a cell. That run wasn't taken apart, so it is not proved.
- **An empty map of the row's size loaded and reached Blood Moor** on Defs 46 and 52 with no halt, though neither room was stood in. An empty map has orientation 0 everywhere and its size matches, so neither test can fail. Its tiles were built on Def 46 by inference only.
The Ghidra server had no project open, so the addresses come from disassembling the slot's `D2Common.dll` (md5 `ee1238806ef6d6d9801d12a09d128fe1`) with capstone, not from a decompiler.

## What Step 4 Must Do

- Write files under `data\global\...` in the archive with flag `0x80000000` only, format 0, sector shift 3, and no `(attributes)`. A stale `(attributes)` arms a fatal CRC check.
- Rebuild `patch_d2.mpq` by appending to a copy of retail's, keeping its 209 entries in place.
- Never write to a file in a hard-linked game folder. A fleet slot's MPQs are links into its shared pool, and writing through one changes every copy.
- Always create `DATA\GLOBAL\EXCEL` and `DATA\GLOBAL\TILES` folders in the test copy.
- Set the game's working folder to the test copy. `-direct` and `-txt` both resolve from it.
- Remove the previous install's loose files from the test copy, since a loose file wins over any MPQ.

## End-to-End Run, 2026-11-03

Windows 11, 1.13c copies, windowed. The editor was `bin\ds1edit.exe` run from a scratch folder with `d2_install` set to the reference install. Its windows were driven with window messages, and real keyboard input (`SendInput`) was used twice, to type the test folder into Launch Profile and the new map's name. The game ran in D2Fleet slot 0 for the frames, because the registry `Save Path` of this machine is the 1.14b install's save folder and a plain launch can't enter a game without writing there. Details and screenshots are in `task-8-report.md` beside the plan.

| What | Result |
|---|---|
| `Game.exe -w` runs from a real copy (`patch_d2.mpq` a separate file) | Yes, to the main menu. A first try failed with "Only one copy of Diablo II may run at a time" because another game was open |
| Build & Test from the editor | Installed `DATA\GLOBAL\TILES\...\TownN1.ds1`, `BarW5.ds1` and `LvlPrest.txt` and started the game from the test copy, windowed. It stopped at the main menu, since no character could be used there |
| A new table, `BarW5` added by **Map > New** on Barracks W | With those files copied into the slot after launch, the game compiled `DATA\GLOBAL\EXCEL\lvlprest.bin` (471,316 bytes, 90 `.bin` files in all) at the start of a game, which holds `Barracks/BarW5.ds1`. It entered town with no crash. The new map was not drawn: a Barracks room wasn't generated |
| Build MPQ before a compiled table exists in the test copy | Refused, with "Nothing was built. LvlPrest.txt has changed since the game last compiled it. Run Build & Test and start a game first." |
| Build MPQ after copying the slot's 90 `.bin` files into the test copy | Built, "4 files, 2775684 bytes, 2.5 seconds". That copy was done by hand, standing in for a game started in the test copy |
| The built archive, read with a throwaway reader | 212 hash entries in a table of 16,384. The 209 retail entries are all there and 207 are byte-identical; `lvlprest.bin` and `LvlPrest.txt` are the project's. Also `data\global\tiles\Act1\Barracks\BarW5.ds1`, `...\Town\TownN1.ds1` and a `(listfile)`. No `(attributes)`. Every file has flag `0x80000000` |
| The built archive mounted in the running game, with the loose `TownN1` removed first | `SFileOpenArchive` (`Storm.dll+0x2a0b0`, priority 5000, flags 2) returned 1. A read of `data\global\tiles\act1\town\townn1.ds1` was 57,652 bytes before and 57,801 after, and its first 48 and last 101 bytes match the project's file. So the archive mounts into a running game and wins over `d2data.mpq` |

### A changed map: not shown

The saved `TownN1` differs from the game's in 14 wall cells (x 36 to 39, y 9 to 12, two wall layers). In the game those cells lie in subtiles x 4520 to 4540, y 4635 to 4655, by the gate in the south fence. This was found by diffing the two files, since the object lists of the file and the game's town line up at a fixed offset (game subtile = file subtile + (4340, 4590)).

With the loose file in the slot, the same character at the same spot showed the same frame as with no loose file, and the walkable map around it was the same. A stronger test then put a `TownN1` with every wall cell cleared (a byte-edited copy of the retail file) in the slot: the town at the start of the game still had its stone wall and its tent. So in this run the game did not read the loose `TownN1.ds1` at all, which is not what the earlier static reading expected, and Build & Test's map path stays unproved. The reason wasn't found. The file's last-access time also did not move after a game was entered, which points the same way but proves little.

## Not Proved Yet

- **A changed map seen in the game.** See above: not shown, and a loose `TownN1` was not read in the run. Until that is understood, the sentence "the game reads a project's maps from loose files" is not proved. A new table from a loose `LvlPrest.txt` did work.
- **A rebuilt `patch_d2.mpq` mounted at startup.** Only mounting into a running game was shown. Starting `Game.exe` from a copy whose `patch_d2.mpq` is the built one, without `-direct -txt`, was not run, so the archive winning at startup is not proved.
- **A game entered from the editor's own launch.** The editor started the game and it reached its menu; entering a game was done in a fleet slot instead, with files copied in by hand.
- **The new map drawn in the game.** A new map joins the random pick, and none was generated.
- **An empty map on a `Scan` or `Pops` row.** On a `Scan` row it may remove what the row provided, such as a cave entrance, and so lose the level's exit. That wasn't tested. An empty map on a `Pops` row wasn't run at all, and neither was standing inside an empty room, or anything outside Act 1.
- **File cell length.** Static reading infers a stack buffer limit of 41 characters, and `MAP_OPS_TABLE_PATH_MAX` is now 41. The limit itself is not yet tried in the game.
- **Read-only excel folder.** Static reading says a failed write is silent.
- **`wine` on Linux and macOS.** Case handling of `DATA\GLOBAL\...` wasn't run.
- **1.14 installs.** `C:\Diablo2` is 1.14b, not 1.13c. Its list and priorities match statically, and its `-direct` and `-txt` behaviour wasn't read.

## Corrections Found Along the Way

- `docs/guides/mods-and-hooking-1.13c.md` near lines 219-222 and 1028-1030 leaves out `patch_d2` and gives `d2exp` the wrong priority: `d2exp` is 3000 and `patch_d2` is 5000.
- Calling `Storm.dll` from a plain test program loads archives but finds no files, even real ones, and Storm's hash table stays encrypted there. The cause wasn't found. Test through a running member instead.
