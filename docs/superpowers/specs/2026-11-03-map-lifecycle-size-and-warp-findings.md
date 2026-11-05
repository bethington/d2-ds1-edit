# Task 1b report: what a map must be for the game not to halt

Lord of Destruction 1.13c, `lod-113c` in fleet slot 0, `-w -direct -txt`, character AudNovBq. 8 game runs.

## Answer

A map added to a row halts the game for either of two separate reasons, and an empty map has neither:

1. **Its header size differs from the row's `SizeX` and `SizeY`, on a row with `Scan` or `Pops` set.** Halt at line 2239 (width) or 2240 (height), at load.
2. **It has a visible warp cell the room has no link for.** That is a wall cell of orientation 10 or 11, main index below 8, top bit clear. Halt at line 689, when the room's tiles are built during the walk.

So the two mass-run halts have two different causes. `size-swap` hit the first and `size-same` hit the second. Each is now reproduced on a single row.

An empty map of the row's exact size is safe from both halts. Its size matches and every orientation is 0, so neither test can fail. The runs agree, with the limits below.

## Recommendation

**(i) New on sized rows stays allowed.** Option (iii) would be backwards: Clone is the operation that can carry a warp cell into another row, so it is the one that can reach the line 689 halt.

Two follow-ups for the controller to rule on. Neither is a condition on New:

- **Clone needs the warp check.** Cloning a same-size map with a visible warp cell into another row passes today's size refusal and can halt. Refuse or warn on those cells when the target isn't the source's own row. An author can also paint such a cell by hand, so Build could run the same check.
- **The size refusal is no longer only a precaution.** The manual and the gate results say the cause isn't shown to be size. On `Scan` or `Pops` rows it now is, and on other rows the map draws wrongly. Their wording can be updated.

An empty map still isn't a good map. It is a hole in the level, and on a `Scan` row it removes whatever the row provided, such as a cave entrance. That can make a level's exit unreachable when the game picks it. This wasn't tested. It is a content problem, not a halt.

**How sure:** high on the two asserts and on the rule's shape. High for an empty map on a plain row. Medium for an empty map on a `Scan` row, because its room's tiles were never built in the run. `Pops` rows with an empty map weren't run.

## What each assert tests

Ghidra was not usable: the MCP server was up, headless, with no project open and no programs. I disassembled the slot's own `D2Common.dll` with capstone instead (md5 `ee1238806ef6d6d9801d12a09d128fe1`, base 6fd50000). No decompiler output or Ghidra xrefs back this. Callers were found by scanning `E8` and `E9` rel32 targets, so indirect calls are missed.

The "address" in the message is the return address of the Fog halt call. Several tests share each one, and the logged line number says which test fired. The log's FILE field is blank, so no source file name is known.

### 6fd5c664, line 2239 (and 2240)

In the function at `6fd5c060`, which reads a preset map's special cells. Its only direct caller found is `6fd5c745`.

```
6fd5c13f  mov eax,[ebp+0xc]     ; the parsed map
6fd5c142  mov edx,[eax+0xc]     ; map width
6fd5c145  cmp edx,[ebp+0x18]    ; room width
6fd5c14a  push 0x8bf            ; 2239 if different
6fd5c154  mov eax,[eax+0x10]    ; map height
6fd5c157  cmp eax,[ebp+0x1c]    ; room height
6fd5c15c  push 0x8c0            ; 2240 if different
```

- Map `+0xC` and `+0x10` are the header's stored width and height, unadjusted (`LoadAndParsePresetArchive` `6fd5b560`).
- Room `+0x18` and `+0x1C` are the row's `SizeX` and `SizeY` when both are nonzero (`6fd5bd19` to `6fd5bd2a`). Otherwise they come from the caller.
- The function returns before this test when the row's `+0x34` and `+0x38` are both zero (`6fd5c0f1` to `6fd5c10a`). The column map names those `Scan` and `Pops`, so the test only runs on rows with one of them set. In 1.13c's table that is 279 of the 1,054 sized rows.

The other two pushes on this path, `0xdd` and `0xde`, are a bounds check on an object id against the objects table. They aren't about the row.

### 6fdb9400, line 689 (0x2b1)

In `6fdb9360`, reached when a wall cell's orientation is 10 or 11.

```
6fdb9380  call 6fd69950         ; the level's array at record +0x68
6fdb9389  mov edx,ecx ; shr edx,0x14 ; and edx,0x3f   ; cell's main index
6fdb9393  mov eax,[eax+edx*4]   ; array[main index]
6fdb9398  mov edx,[esi+0x10]    ; walk the room's list at [room+0x4c]
6fdb939b  cmp [edx],eax         ; entry's record id equal?
6fdb93a6  push 0x2b1            ; 689 if none matched, or the list is empty
```

- Line 715 (0x2cb) shares this address and is a different test. The logs say 689.
- `6fd69950` returns `+0x68` of a level record, and the array one step before it is `+0x48`. Levels.txt has `Vis0-7` and `Warp0-7` in that order, so this is `Warp[main index]`. That mapping is inferred from the layout and values, not from a table descriptor.
- For Blood Moor, `Warp0-7` is `-1 -1 -1 0 1 2 3 -1`.
- Filters before it (`6fdb9841`, `6fdb9863`): main index 8 or more leaves early. A cell with the top bit set goes down another path through `6fdb9090` and `6fdb91c0`, which returns quietly if the warp record isn't found.
- Who fills `[room+0x4c]` was **not traced**. The link list's source is inferred to be the level's warp connections.

The feeding values are all in the DS1: orientation, and the packed cell's main index and top bit. The row adds `Scan`, `Pops`, `SizeX`, `SizeY`.

### Across 1.13c's table and maps

- 109 rows have a visible warp cell and 90 have a top-bit one. **All of them have `Scan` set, and no row with `Scan` of 0 has either.** This is from `assets/tiles`, which holds the map files.
- Rebuilding `size-same`'s substitutions for its Act 1 rows: 4 rows got a map with a visible warp cell, Defs 24, 25, 51 and 52, all taking `clfcave` or `clfcave2`. Which of them fired in that run isn't shown, since it changed everything at once.
- Those 4 rows have `Scan` set, so `Scan` alone doesn't excuse a warp cell. R4 shows the halt on a `Scan` 0 row, and `size-same` suggests it on `Scan` 1 rows too.

## Runs

Row: Def 46, Act 1 Pond, `SizeX` and `SizeY` 8, `Scan` 0, `Pops` 0, `Files` 1. Found by R0, then R1 on that row alone. In Blood Moor its room is at tiles (1128, 928), read from the level's preset list in R2's memory. Second row: Def 52, DOE Entrance, 8 by 8, `Scan` 1, `Files` 2, room at tiles (1136, 984).

| Run | Row | Counted cells | Result | Row confirmed generated? |
|---|---|---|---|---|
| R0 locate | 209 sized `Scan` 0 `Pops` 0 Act 1 rows | Each its own missing file | Crash at load, access violation at `6FD5B57C`, names `GateR46.ds1` | Yes, Def 46 |
| R1 control | 46 alone | `GateNoSuchMap.ds1` | Same crash at load | Yes, that row alone is the cause |
| R2 (a) | 46 | Empty map, 9 by 9, 1,336 bytes | **No halt.** Entered town and Blood Moor to subtile (5742, 4700), then (5700, 4669). Quit at life 10 of 55 under attack, no crash | File: yes, memory shows it parsed, width 8, height 8, 1 wall layer. Tiles: inferred, see below. Not stood inside |
| R3 (b) | 46 | `Caves/CaveDr1.ds1`, same size, top-bit warp cell | **No halt.** Stood inside the room at (5673, 4648), part of it drawn black | Yes, stood in the room |
| R4 (b) | 46 | `Caves/clfcave.ds1`, same size, visible warp cell, orientation 10, main index 3 | **Halt, line 689, `6fdb9400`,** during the walk at (5734, 4561), still in town | Yes, the halt itself |
| R5 (c) | 46 | `Outdoors/Swamp5.ds1`, 16 by 16, no warp cell | **No halt.** Walked past R4's point to the room's edge at (5676, 4638). Terrain drawn wrongly, with bands of black. Character died there, no crash | Yes, the wrong drawing was at the room's place. Not stood inside |
| R6 (a) | 52 | Empty map, 9 by 9 | **No halt** at load, entered Blood Moor to (5743, 4656), then died | File: yes by R7. Tiles: **no**, room never near |
| R7 (c) | 52 | `Caves/CaveW.ds1`, 24 by 24 | **Halt, line 2239, `6fd5c664`,** at load | Yes, the halt itself |

**How R2's tiles are inferred.** R4 shows the room's tile code runs by the time the character is at (5734, 4561), on the same route. R2 went well past that point on the same character and seed. R3 and R5 passed it too.

**Plain same-size map not run.** A map with no warp cells on this row was left out to spend the runs on warp cells. R3 covers it from the harder side.

**Files.** Every run changed one row. Logs and frames are in `C:\Users\benam\AppData\Local\Temp\claude\C--Users-benam-source-cpp-d2-ds1-edit\ac9993b9-4c5c-4f31-9e23-406958575d71\scratchpad\t1b\`, as `r0-locate.txt` to `r7-size-scan1.txt`, with `r3-warpmap.png` and `r5-swamp.png`.

## Proved and inferred

**Proved, by code and a single-row run:**

- Line 2239 compares the map's stored width to the room's, and it fires for a wrong-size map on a `Scan` row.
- Line 689 fires for `clfcave.ds1` on a row with no warp cell of its own.
- The empty map loads on both rows, and the game carries on into Blood Moor.

**Proved by a run only:**

- A wrong-size map on a `Scan` 0 row doesn't halt on the way to the room's edge, and draws wrongly.
- A same-size map with a top-bit warp cell doesn't halt on a `Scan` 0 row, standing inside the room.

**Inferred:**

- That `size-same` halted because of the visible warp cells. Its line and address match R4, and 4 of its substitutions fit, but that run wasn't taken apart.
- That the array at `+0x68` is `Warp0-7`, and that `[room+0x4c]` holds the level's warp links.
- That a wrong-size map **smaller** than a `Scan` 0 row would read past its buffer. The tile code wasn't read and R5's map was larger. This could crash rather than just draw wrongly, so the refusal stays right on those rows.
- That an empty map on a `Scan` row is safe when its room's tiles are built. From the code, no orientation-10 or 11 cell means line 689 can't be reached. R6 didn't get there.

**Not tested:** an empty map on a row with `Pops` set. Standing inside an empty room. Anything outside Act 1. A row with several counted cells where only some are the new map. Whether a level with its entrance emptied stays playable.

## What would change the recommendation

- An empty map halting when its room's tiles are built, on any row: go to (ii), with the failing row type refused.
- A halt with an empty map on a `Pops` row, or a level whose generator requires a warp from a `Scan` row: refuse New on those rows.
- Clone being restricted some other way doesn't change New.

## Harness notes

- **AudNovBq's save changed.** I quit R2 while it was being killed, and from R3 on it loaded with life 0 of 55. It walks, but died early in R5 and R6, which limited those walks. Map seed and layout looked the same, since R3 to R7 behaved consistently with R0 to R2. Someone may want to restore that save before relying on it. I didn't edit it.
- A click at `y` 460 lands on the control panel and doesn't walk, so the earlier brief's extra clicks there may do nothing.
- `tmp/gate/slot_table.py` (git-ignored) gained `locate`, `single` and `empty`. No source or docs were changed, and nothing was committed. No writes went to `C:\Diablo2` or to any MPQ. The fleet server is stopped.
