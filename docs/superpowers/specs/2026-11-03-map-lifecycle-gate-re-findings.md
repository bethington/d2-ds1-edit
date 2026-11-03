# DS1Edit test-launch RE findings: MPQ, -direct, -txt, DS1 lookup, Storm, PD2

Date: 2026-11-03. Static analysis only. Nothing was changed or saved in Ghidra, the game was not run,
and no scripts ran inside Ghidra (run_script_inline is disabled on this server).

Legend: **[R]** read directly (disassembly, decompile, table bytes, or file bytes). **[I]** inferred from
what was read. **[RT]** needs a runtime test. Every conclusion in this file is static-only unless a later
runtime test says otherwise.

## Corrections to what the brief treated as settled

1. **d2exp is priority 3000, not 5000. patch_d2.mpq is the 5000 mount.** [R] D2Win `6f8eaa20`:
   `6f8eaace PUSH 6f8fcfec "patch_d2.mpq"` / `6f8eaadd MOV EAX,0x1388` / `6f8eaae2 CALL 6f8e7e60`;
   `6f8eaaf0 PUSH 6f8fcfd4 "d2exp.mpq"` / `6f8eaaff MOV EAX,0xbb8` / `6f8eab04 CALL 6f8e7e60`.
   `docs/guides/mods-and-hooking-1.13c.md` is wrong in its table near lines 1028-1030: it labels
   `6f8eaae2` as d2exp and calls `6f8eab04` "last of the base set". Its Stage-one list (near line 222)
   also omits patch_d2 from the "seven calls". The guide was not edited (read-only brief).
2. **`C:\Diablo2\Game.exe` is 1.14b, not 1.14d.** [R] Its SHA-256 `0680e266...` equals Ghidra
   `/Vanilla/1.14b/Game.exe` (FileVersion 1.14.1.68). Ghidra `/Vanilla/1.14d/Game.exe` is FileVersion
   1.14.3.71, SHA-256 `631066c1...`, 3,618,792 bytes.
3. The Ghidra names on many of these functions are wrong. `HasItemType3InInventory` is the table loader,
   `GetSkillBitFlag1` is the .bin *writer* (its comment says read, but it opens with "wb" and fwrites),
   Fog `GetLastError` is the "is -direct on" getter, and `SetUnitAllocatedFlag` is the -txt setter.
   Addresses in this file are reliable. Names are only the project's labels.

## Build check

All 1.13c programs used are under `/Vanilla/1.13c/`. [R] Ghidra's stored SHA-256 equals
`C:\D2Fleet\versions\lod-113c` for each:

| File | SHA-256 prefix | Note |
|---|---|---|
| D2Win.dll | a9afb52d | |
| Fog.dll | 96c05770 | |
| Storm.dll | 4b5fcaf8 | FileVersion string says 1.08b, normal for this DLL |
| D2Common.dll | 59fa5928 | |
| D2Client.dll | dd8bc602 | |
| D2Game.dll | 24b293b8 | |
| D2Gfx.dll | 50284647 | |
| Game.exe | 74fe9c09 | 1, 0, 13, 60 |

Bnclient and D2Launch in the same folder were used for call sites. Their hashes were not compared; only
D2Launch's and Bnclient's import tables on disk (reference copies) were read for the ordinals.

`/Mods/PD2-S14/ProjectDiablo.dll` is **not** the installed Live build: string addresses differ and the
guide's mount addresses (`102ad5f0`) do not line up. PD2 facts below come from the on-disk Live files in
`C:\Diablo2\ProjectD2`, by bytes.

---

## 1. MPQ set and order

**Answer.** The set is fixed, and 13 names are known. patch_d2.mpq is mounted at 5000, above d2exp (3000),
and is optional. No code path reads the key argument and no archive is hash or signature checked at mount.
A modified or third-party patch_d2.mpq will load. An extra MPQ of arbitrary name cannot load without code
changes: there is no switch, registry value or config for it.

**Mount table.** [R] All through D2Win `6f8e7e60` (arg1 "D2Win.DLL", arg2 name, arg3 key, arg4 flag selector,
arg5 retry callback, priority in EAX).

| Exported from | Archive | Priority | Handle global | Checked? |
|---|---|---|---|---|
| D2Win 10086 `6f8eaa20` | d2data.mpq | 1000 | 6f9a9ccc | Fatal |
| | d2sfx.mpq | 1000 | 6f9a9cb8 | Fatal |
| | d2speech.mpq | 1000 | 6f9a9cd4 | Fatal |
| | d2delta.mpq | 1000 | 6f9a9cbc | Optional, never checked |
| | d2kfixup.mpq | 1000 | 6f9a9cd0 | Optional, never checked |
| | patch_d2.mpq | 5000 | 6f9a9ce0 | Optional, never checked |
| | d2exp.mpq | 3000 | 6f9a9ce4 | Fatal only if expansion detected |
| D2Win 10005 `6f8ea890` | d2char.mpq | 1000 | 6f9a9cc0 | Fatal |
| | d2music.mpq | 1000 | 6f9a9cd8 | Fatal |
| | d2Xmusic.mpq | 3000 | 6f9a9cb4 | Fatal in expansion branch |
| | d2Xtalk.mpq | 3000 | 6f9a9cc8 | Fatal in expansion branch |
| | d2Xvideo.mpq | 3000 | 6f9a9cdc | Fatal in expansion branch |
| D2Win 10173 `6f8ea820` | d2video.mpq or d2Xvideo.mpq | 1000 | 6f9a9cc4 / 6f9a9cdc | Returns 0 to caller; callers not read |

- Game.exe calls 10086 through thunk `4074c6` (`JMP [409054]`) and 10005 through `4074e4`
  (`JMP [40904c]`), both from `RunGameMainLoop` `00407600` at `0040763f` and `00407654`. [R]
- `d2delta.mpq` and `d2kfixup.mpq` are absent from the lod-113c folder, and from `C:\Diablo2`. [R]

**Fatal logic.** [R] D2Win `6f8eab0e`-`6f8eab4c`:
1. Call Fog 10117 (returns the -direct flag). Non-zero jumps straight to success. So with -direct the 10086
   checks are all skipped, although the mounts are still attempted.
2. Otherwise d2data, d2sfx and d2speech must be non-null.
3. Then if Fog 10227 (expansion detect) is true, d2exp must be non-null.

Failure in Game.exe goes to `00407673`, calling thunk `4074d2` and returning 0. [I] That is a quiet exit,
with no message seen on this path. [RT]

10005 is stricter and does **not** skip checks for -direct: it only clears the retry callbacks
(`6f8ea896`-`6f8ea8a1`). So d2char and d2music, plus d2Xmusic, d2Xtalk and d2Xvideo in expansion, are required
even with -direct. [R] Without -direct, Game.exe passes callbacks (`0040764a`/`0040764f`), so a missing file
likely prompts or retries rather than exiting. [I] [RT]

**Search order.** [R] Storm 266 `6fc1a0b0` keeps a linked list and inserts before the first archive whose
priority is `<=` the new one. So the list is descending and, among equals, the later-opened archive comes
first. Storm 267/268 walk the head onward and take the first hit. [I] The effective stock order is:
patch_d2 (5000); then d2Xvideo, d2Xtalk, d2Xmusic, d2exp (3000); then the 1000 group, with d2data searched
last.

**Key argument.** [R] `6f8e7e60` reads arg2 (`MOV EAX,[ESP+0x18]` after four pushes), arg4 (EBP) and arg5 only.
arg1 "D2Win.DLL" and arg3 ("D2EXPANSION", "PATCH_D2", ...) are never read. `6f8e7dc0` receives only the name,
the flag selector and EBX (priority). 1.14 removed the key argument entirely from these calls (see Q7), which
agrees.

**Signature or hash checks.** [R] Storm 266 contains no authenticate call. Its only checks are the signature
dword `MPQ\x1a` and header-size field `> 0x1f`. The `(signature)` string (`6fc370c0`) is used by Storm 251
`6fc17560` only. Callers of 251: Storm's own `ApplyPatchesFromRegistry` and `LoadCapsDataFromArchive`, and
Bnclient `PerformVersionCheckRevision` `6ff2d320`, `ProcessPatchDownloadSequence` `6ff2d610`,
`ProcessDownloadedFile` `6ff2e2e0`. Those are Battle.net download paths. No D2 module other than Bnclient
imports 251.
- Runtime rejection exists for one thing only: per-file CRC when an archive has a valid `(attributes)`
  (see Q5). A patch_d2 without `(attributes)` has no CRC check. Retail patch_d2 has none. [R]

**Expansion dependence.** [R] The 10086 list is the same either way; the mode only changes whether d2exp is
fatal. 10005 branches: Fog 10227 true, then the callback at `cfg+0x20d`. Game.exe sets that to an always-true
stub (`InitializeAndStartGame` `00408250`: `pcStack_2d3`, offset 0x4e0-0x2d3 = 0x20d). So with d2exp.mpq
found, the expansion set is opened.

Fog 10227 `6ff6f9b0` looks for d2exp.mpq via `GetInstallPath` first, then as a relative name in the CWD, and
caches the result. So a registry InstallPath pointing at another install can make the game think it is in
expansion mode even if the test folder has no d2exp.mpq. [I]

**No extra-MPQ mechanism.** [R] The full Game.exe table is 57 rows at `0x40bc08`, stride 0x3c, read from disk.
It has no mod or mpq option. Rows with FILEIO or TXT sections: npl, direct, lem, nocompress, txt. `usemod`,
`-mpq`, `modpath` do not occur as strings in the local Game.exe; in 1.13c's string list nothing similar
appeared. `D2.ini` (`00407a80`) only maps these same section/key names, so it adds nothing. Registry reads in
Game.exe are CmdLine, UseCmdLine, SvcCmdLine, Fixed Aspect Ratio, Resolution, Render.
- Only ways in without code: replace `patch_d2.mpq` (5000), or use `d2delta.mpq` or `d2kfixup.mpq` (1000).
  **A file named d2delta.mpq loads, but at 1000 it loses to every other archive holding the same name, so it
  can only add new paths.** [I]
- **Replacing patch_d2.mpq means carrying its 209 entries.** [R] Retail tables live there
  (`data\global\excel\LvlPrest.bin` 471,316 bytes, a clean `4 + 1091 * 0x1b0`). Without them the game falls to
  d2exp's LvlPrest.bin at 466,524 bytes, which does not divide by 0x1b0. [I] Blocks are `0x80000100` with no
  encryption, so a writer can copy raw block bytes. [I]

**Search path.** [R] D2Win `6f8e7dc0`, then Storm 266:
1. Storm 266 with the bare name: `GetFileAttributesA(name)`, CWD-relative. If not found, `BuildGameFilePath`
   `6fc16bd0` gives `_fullpath(base + name)`. Base is `.\` (Fog 10101 sets it from `6ff76fe8`, read as `2e 5c`),
   so CWD again.
2. `GetModuleFileNameA(NULL)` directory + name, i.e. Game.exe's folder.
3. `SearchDrivesForArchive` `6f8e7b30`: Game.exe's folder again, then the root of each CD-ROM drive
   (`GetDriveTypeA == 5`).

Registry InstallPath is **not** used for mounting. It is used by Fog 10116 `6ff67e60` (expansion detect, save
path). Fog 10116 also **writes** InstallPath to the registry when absent or invalid, and Fog 10115 does the
same for "Save Path". Game.exe does not call SetCurrentDirectoryA except in service mode
(`MigrateBetaRegistryKeys` `00407ee0`, gated on `DAT_0040cf34`). [R]

So `test_game_dir` must be the launch CWD, or Game.exe must sit next to the archives. A launch writes to the
registry. [I]

**Other config.** `D2.ini` is read from the CWD cut to at most two backslashes (`GetInstallRootDirectory`
`00407960`), so `C:\D2Fleet\versions\lod-113c` looks for `C:\D2Fleet\D2.ini`. It can set `[FILEIO] DIRECT=1`
and `[TXT] TXT=1`; the command line is parsed after it. [R] Stray D2.ini files could change behaviour.

Parser: `ParseAllCommandLineOptions` `00407c90` finds `-`, lowercases, tries progressively shorter prefixes
against the switch column via `FindConfigOptionIndex`, then writes to `cfg + offset`. [R]

**Unverified.** Callers of D2Win 10173 and whether failure there is fatal. Whether anything mounts archives
later from D2Launch or D2Client beyond these three functions was not exhaustively searched; D2Launch and
D2Client import 10005 and 10173. Exact UX of missing-file failures. [RT]

---

## 2. -direct

**Answer.** Loose first, then falling back to MPQs for what's missing, from the CWD, for every file loaded
through the Fog file layer. That includes .ds1, .dt1, .txt, .bin, .tbl. [R] for the mechanism; [I] for the list
of types (via importers); [RT] for each type in practice.

**Evidence.** [R]
- Table row `0x40c6d0`: section FILEIO, key DIRECT, switch `direct`, bool, offset 0x200.
- `RunGameMainLoop`: `00407607 MOVZX ECX, byte ptr [EDI+0x200]`, `XOR EDX,EDX`, `0040761b CALL 4074ae`, which is
  Fog 10101 `6ff67ef0`.
- Fog 10101: Storm 270 (SFileSetBasePath) with `.\` always; Storm 263 `6fc15d70` with the direct byte, stored at
  Storm `6fc43130`; and `MOV [6ffa6e1c],ESI`. Fog 10117 `6ff67bc0` returns `[6ffa6e1c]`.
- Fog 10102 `6ff67e40` calls Storm 267 `6fc18da0`. Storm 267: bit0 of `6fc43130` becomes open flag 1, bit1 becomes
  flag 2. If no archive is open and the value is 0, flag 1 is forced. Then `268(NULL, path, flags, out)`.
- Storm 268 `6fc18960`, flag 1: absolute or UNC path as given, else `BuildGameFilePath(,0)`, i.e.
  `_fullpath(".\" + path)`. If `GetFileAttributesA` says it exists and is not a directory, `CreateFileA`. If that
  succeeds, a loose handle is returned. **Otherwise it falls through to the archive search.** With -direct the
  value is 1 (Game.exe passes the byte), so flag 2 (base + filename-only) does not apply.

**Folder.** CWD, because base path is `.\`. Not Game.exe's folder unless that is the CWD. Loose path for a DS1:
`<cwd>\DATA\GLOBAL\TILES\Act1\Town\TownN1.ds1`. [R] [I]

**File types.** Importers of Fog 10102 on disk: D2CMP, D2Client, D2Common, D2Lang, D2Launch, D2Sound, D2Win. [R]
So tables and DS1 (D2Common), DT1 and sprites (D2CMP, by import only), TBL (D2Lang, by import only). The
per-module use in D2CMP and D2Lang was not read. [I]

**What it can't serve.** Callers of Storm 268 with explicit flags 0 bypass direct and read archives only: [R]
D2Client `LoadLocaleDataFile` `6fb39770`, D2Launch `LoadLocaleDataFiles` `6fa54920`,
`InitializeIntroVideoAndRegistry` `6fa52770` (flags 0). Renderers (D2DDraw, D2Direct3D, D2Glide) also import 268,
and Bnclient; not read. [I]

**Case and path.** Loose lookup is Windows `GetFileAttributesA`/`CreateFileA`, so case follows the filesystem:
insensitive on NTFS. Under a `test_wrapper` such as wine it depends on wine's mapping. [I] [RT] No slash
normalisation in 268; D2Common has already normalised DS1 cells.

**A trap.** A stale loose file silently wins over any MPQ, because no timestamp comparison exists. [R]

**Unverified.** That the game fully boots and plays with -direct and no loose tree. [RT] Performance of loose
reads is irrelevant here. D2Sound and video paths not read.

---

## 3. -txt

**Answer.**
- With -txt: every table is compiled from `DATA\GLOBAL\EXCEL\<name>.txt` through the file layer (so MPQ too),
  and `<name>.bin` is written to `<cwd>\DATA\GLOBAL\EXCEL\`.
- **The game then always loads the .bin through the file layer.** So -txt without -direct compiles and throws
  the result away: the next open reads the MPQ's .bin. **Use `-direct -txt` together, and the folder must exist.**
- Without -txt: `.bin` only, first hit in the file layer, no staleness check, .txt ignored.

**Evidence.** [R]
- Table row `0x40c874`: section TXT, key TXT, switch `txt`, bool, offset 0x211.
- Setter chain: D2Client `6faf1c30` (`MOV DL,[ECX+0x211]`, `SETZ AL`, `PUSH EAX`, `CALL 6fabce5c`, which is D2Common
  10563 `6fdad750`, `flag = (arg == 0)`, stored at `6fdf145c`). So `6fdf145c = txt`. The handler is entry 50 of the
  52-pointer table at `6fba4898` (`6fba4960`), called by `InitializeClientStartup` `6fac0b60`
  (`6fac0b9d MOV ECX,EDI; CALL EAX`). That runs before `InitializeClientMainLoop` `6fac0c80` calls
  `LoadAllDataTables` (D2Common 10943 `6fdb6160`) at `6fac0d12`.
- Only D2Client imports 10943 and 10563 among the reference DLLs. D2Game imports neither. [R]
- Loader `6fdaef40` (D2Common 10849):
  - `6fdf145c != 0`: open `DATA\GLOBAL\EXCEL\<name>.txt` (`leveldefs` remapped to `levels`) via `6fd59900`. Missing
    is **fatal** (assert line 0x894). Parse, build records, write via `6fdad770` with the *original* name + `.bin`.
  - Then `6fde9e20` decides the format. `6fde9e20` is **1 in .data and no code writes it** (every byte-scan hit is a
    read or compare). So it always opens `.bin` via `6fd59900`, and a missing one is **fatal** (0x8b9). First dword =
    record count, records follow.
- The .bin writer `6fdad770` (D2Common 10903): `wsprintf("%s\%s","DATA\GLOBAL\EXCEL",name)`, `fopen(...,"wb")` (mode at
  `6fde1548`), `fwrite` count dword, `fwrite` records, `fclose`. If `fopen` fails it **returns silently**. No mkdir. So
  a read-only folder or a missing `data\global\excel` does not fail the game: it just leaves whatever .bin the file
  layer next finds. CWD-relative. [I] from CRT fopen.

| Switches | .txt read from | .bin written | .bin loaded from |
|---|---|---|---|
| none | not read | no | Archives by priority |
| -direct | not read | no | Loose first, else archives |
| -txt | Archives (loose only if no archive is open) | CWD, if folder exists | **Archives** (fresh compile ignored) |
| -direct -txt | Loose first, else archives | CWD, if folder exists | Loose first, i.e. the fresh one; else archives |

**LvlPrest.** [R] `LoadLevelPresetData` `6fdc0110`. Row size 0x1b0. Columns: Def +0, LevelId +4, Populate +8,
Logicals +0xc, Outdoors +0x10, Animate +0x14, KillEdge +0x18, FillBlanks +0x1c, Expansion +0x20, SizeX +0x28,
SizeY +0x2c, AutoMap +0x30, Scan +0x34, Pops +0x38, PopPad +0x3c, Files +0x40, File1-6 at +0x44 + i*0x3c (string,
0x3b wide), Dt1Mask +0x1ac. The `Name` column is not in the descriptor, so it isn't kept. The two names at
`6fddf754`, `6fddf74c` were not printed; `Scan`/`Pops` are [I].

So **File1..File6 are read from the compiled table**, without -txt from the .bin, with -direct -txt from the fresh
.bin. Never directly from .txt at runtime.

**File column "0".** [R] `DATATBLS_NormalizeFilePath` `6fdbc710` turns `/` into `\` and prefixes only if `strlen > 1`.
The cache preload at `6fdc05a9` also requires `strlen > 1`. So "0" (and empty) stays as-is and is not opened at load.
At generation the file index comes from `AllocatePresetWithRandomSubEntry` `6fd5bbc0`: if `Files == 0`, index -1;
otherwise random. [I] A "0" cell reached by an index below `Files` would open a file named "0", and
`LoadAndParsePresetArchive` `6fd5b560` dereferences the result with no null check (`6fd5b577 MOV ESI,EAX`,
`6fd5b57c MOV EBP,[ESI]`), so a crash. [RT] A writer must keep `Files` consistent with non-"0" cells.

**Other tables alongside.** [I] Without -txt, a LvlPrest.bin alone is enough for the table itself: its descriptor has
only int and string fields with no lookup into another table. A new row is only reached via its `LevelId`:
`DATATBLS_GetLvlPrestRecordById` `6fdbc950` returns the first row whose +4 equals the id, or via `Def`/index paths in
`FUN_6fd5cd10`, which were not read. So a brand-new level also needs Levels data, which was not traced here.

With -txt there is no per-table option: all tables are compiled, so all their .txt files must be reachable or the
game aborts. Retail MPQs have them for the 25 names sampled (compcode, playerclass, bodylocs, storepage, elemtypes,
hitclass, colors, hiredesc, monmode, plrmode, monai, monplace, skillcalc, misscalc, skills, events, lvlprest, levels,
lvlmaze, lvlsub, lvlwarp, lvltypes, itemtypes, states), with `leveldefs.txt` correctly absent (remapped). Fifteen of
those (`InitializeCommonDataTables` `6fdb16d0`) are compiled only under -txt. [R] All tables in `LoadAllDataTables` were
not enumerated; other readers such as `DATATBLS_OpenExcelWithDebugSave` `6fdae710` have their own gate on the same
flag, and cubeserver has its own names.

**Write failure.** No error, per above. A stale loose .bin from an earlier run would then be loaded under -direct with
nothing telling you. [I] [RT]

**Other dependencies.** -txt only needs D2Client to be the loaded client (it's what sets the flag and calls 10943).
Whether a -txt run leaves a save or data incompatible, or whether bins compiled by it are byte-identical to retail bins,
is unverified. [RT]

---

## 4. How a DS1 is found

**Answer.** Row to open is: `DATA\GLOBAL\TILES\<cell>` through Fog 10102 and Storm 267, so the normal archive search (or
-direct loose first). A .ds1 in a higher-priority MPQ wins with no table change. **A modified existing map is enough
with no -txt.** A new map needs a LvlPrest edit, i.e. a new LvlPrest.bin in the MPQ at a winning priority, or
-direct -txt.

**Path.** [R]
1. After load, `LoadLevelPresetData` walks all rows, skipping rows with Expansion != 0 when not in expansion mode
   (`6fdc057d`). For each of 6 cells, `6fdc0589` calls `DATATBLS_NormalizeFilePath` with `"DATA\GLOBAL\TILES"`
   (`6fddf7b4`): `/` to `\`, and if longer than 1 char, `wsprintf(local_3c,"%s\%s",...)` then copied back into the cell.
   Result: `DATA\GLOBAL\TILES\Act1\Town\TownN1.ds1`, with the cell's own case.
2. Client passes `fLoadLevelFiles = 0` (D2Client `InitializeClientMainLoop`: `uVar4 = 0`), so DS1s are not opened at
   startup. [R] for the call; the argument order is from the decompile's stack order. So a bad DS1 only fails when its
   level is generated.
3. At generation: `LoadPresetAndGenerateRoomContent` `6fd5bf70` takes `piVar1[1]*0x3c + 0x44 + row`, calls
   `GetOrCreatePresetFileCacheEntry` `6fd5be50`. The cache key is an exact byte compare, so it's case-sensitive; the
   same file spelled two ways is two entries, harmlessly.
4. `LoadAndParsePresetArchive` `6fd5b560` calls `RESOURCE_AllocateAndOpen` `6fd59900`, then Fog 10102 (thunk), Storm 267,
   as in Q2. Same path used by LvlSub (`6fdbfba0`, also Normalize + cache) and `ProcessPresetRoomObjectsAndTiles`,
   `InitializeDrlgPresetRoomGrids`.

**Length limit.** [I] `local_3c` is 60 bytes and the prefix is 18 characters, so a cell over 41 characters overflows the
stack buffer (wsprintf, no bound) even though the column allows 0x3b. Keep map paths to 41 characters or fewer relative to
`data\global\tiles\`.

**Case.** MPQ lookup is insensitive (Q5). Retail spellings were checked: `data\global\tiles\act1\town\townn1.ds1` and the
upper-case form both hit in D2Data.mpq; the forward-slash form misses. [R] by re-implementing the hash in a read-only script
(table and hash read from `6fc17fc0`, `6fc15ed0`) against real archives.

**Where retail keeps maps.** `townn1.ds1` is in D2Data (`0x80000200`), not in patch_d2 or D2Exp. [R] So an edit in
patch_d2.mpq at 5000 wins over it. [I] [RT]

**Unverified.** Multiplayer or server path, and whether anything else caches DS1 across games: the cache is refcounted
(`DecrementAndFreePresetData`), so a restart between edits is the safe assumption. [I]

---

## 5. Virtual path, archive format, and Storm API

**Name in the hash table.** `data\global\tiles\act1\town\townn1.ds1` works, any case, **backslashes only**. [R]
`ComputeStringHash` `6fc15ed0` uppercases via CRT `toupper` (`6fbf6659`) and does not map `/`. D2Common normalises cells to
`\` before the lookup but the name in the archive must be backslashed. [R] The real test against D2Data.mpq confirms this.

**Listfile and attributes.** [R]
- `(listfile)` is never referenced: no such string in Storm.dll. Retail patch_d2.mpq has none.
- `(attributes)` is optional. `LoadArchiveAttributes` `6fc19f60` tries to open it; absent means CRCs stay zero. Valid only if
  `size == 8 + n*(4 if flags&1) + n*(8 if flags&2)`, where n is the block count; otherwise ignored. MD5-bearing StormLib
  attribute files are therefore ignored by size. [I]
- **If valid, it arms a fatal CRC check**: `ReadArchiveFile` `6fc195b0` calls `DisplayErrorAndTerminate(0x85100083,...)` when a
  sequential whole-file read's CRC differs from the block's value and that value is non-zero. So never edit an archive and
  leave old `(attributes)` (d2exp.mpq has one with flags 3: `8 + 9888*12`). [R] for size match.

**Header.** [R] Storm 266 scans at 0x200 steps for `MPQ\x1a` with header-size field > 0x1f. Sector size is
`0x200 << word at +0xe`. Hash count at +0x18, block count at +0x1c. Format version (+0xc) and archive size (+8) are not read in
this function. Hash count must be a power of two: lookup uses `count - 1` as a mask (`6fc16ac0`).

Retail values, mirror these: header size 32, format version 0, shift 3 (sector 4096), header at offset 0. [R] from
patch_d2, d2data, d2exp, and pd2data, pd2maps.

Table encryption: standard `(hash table)` and `(block table)` keys. Hash slot: nameA, nameB, locale (word), platform (byte),
block index; empty `0xFFFFFFFF`, deleted `0xFFFFFFFE`. Use locale 0 and platform 0: zero is treated as wildcard, and nothing
imports Storm 272 SetLocale in the module list. [R] for lookup; [I] for default locale.

**Stored files are accepted.** [R]
- `flags & 0x80000000` is required on open, else error 0x3ee (`6fc18960`).
- Sector offset table only if `flags & 0xff00` (`6fc18ca8`; reader `6fc18450`). With no such flag, `ReadCircularBufferData`
  reads raw at `block offset + file offset` (`6fc18604`-`6fc18615`), and no sector table is read.
- Retail evidence: 5,321 blocks in D2Data.mpq and 6,914 in D2Exp.mpq are `0x80000000` exactly. [R]
- **`MPQ_FILE_SINGLE_UNIT` (0x01000000) is not supported**: no test of that bit in the SFILE range; hits for that immediate are
  all elsewhere. For a stored file it wouldn't matter; a compressed single-unit file would be read through a sector table that
  isn't there. Don't set it. [I]
- Also recognised: 0x100 implode, 0x200 multi-compress, 0x10000 encrypted, 0x20000 key adjust. Compressed tail sectors use the
  size check as usual.

So a minimal writer: flags `0x80000000`, compressed size = size, no sector table, no `(listfile)`, no `(attributes)`.
**Note retail d2exp and patch archives' hash tables hold real entries at 16,384 or 65,536 slots; a small table is allowed by
the code but size was not exercised.** [I] [RT]

**Ordinals.** [R] From the PE export table of `lod-113c\Storm.dll` (ordbase 101, no names, base `6fbf0000`), checked against
Ghidra's ordinal comments.

| Ordinal | Function | Address | Signature (Ghidra-recovered, all `__stdcall`) |
|---|---|---|---|
| 266 | SFileOpenArchive | 6fc1a0b0 | `BOOL (const char* name, DWORD priority, DWORD flags, HANDLE* out)` |
| 252 | SFileCloseArchive | 6fc16cb0 | `BOOL (HANDLE archive)` |
| 268 | SFileOpenFileEx | 6fc18960 | `BOOL (HANDLE archiveOrNull, const char* path, DWORD openFlags, HANDLE* out)` |
| 269 | SFileReadFile | 6fc19be0 | `BOOL (HANDLE f, void* buf, DWORD toRead, DWORD* read, LPOVERLAPPED ov)` |
| 265 | SFileGetFileSize | 6fc162d0 | `DWORD (HANDLE f, DWORD* high)` |
| 253 | SFileCloseFile | 6fc16e20 | `BOOL (HANDLE f)` |

Also 267 SFileOpenFile `6fc18da0` `(path, out)`, 263 sets the direct value `6fc15d70`, 270 SetBasePath `6fc16190`, 251
AuthenticateArchive `6fc17560`.

Test-harness notes:
- `SFileOpenArchive` flags: **use 2, as the game does** (`6f8e7dd0`: mode 1, async worker path). Flags 0 selects the synchronous
  path, so testing with 0 doesn't test the game's route. Bit 0 requires a CD-style drive check and returns error 0xf on a fixed
  disk.
- `SFileOpenFileEx`: `hMpq` NULL searches all open archives by priority; openFlags 0 = archives only, 1 = loose first, 4 = return a
  raw file handle positioned at the block (only for stored, unencrypted files).
- `SFileReadFile` arg5 must be NULL: non-null is treated as an async context.
- Passing an absolute archive path avoids the base-path logic. With a bare name, the default base is the exe's directory (`6fc16bd0`
  calling `GetExecutableDirectoryPath`) because the harness won't call Fog 10101. [I]
- Build the harness as a 32-bit process; Storm.dll is `IMAGE_FILE_32BIT` at preferred base `6fbf0000`, so it also needs its
  imports resolvable. [I]

**Unverified.** The async worker `ProcessGameEventLoop` `6fc19200` was seen to call `ReadCircularBuffer`, but not read in full. Header
fields other than those above. Hash-table minimum size. **All of this must be proved by the 32-bit harness. [RT]**

---

## 6. Project Diablo 2

**Load mechanism.** Per the guide, verified there from shipped binaries, and consistent with what I read: Storm.dll's import table is
patched from VERSION to `PD2_EXT` (my import scan: PD2 Storm imports `pd2_ext.dll`, hash differs from vanilla), and `PD2_EXT` splices
`Game.exe+0x82dd`, then `LoadLibraryA("ProjectDiablo.dll")`. D2Win also has a second route, the tail patch on `6f8eaa20`, and the string
`ProjectDiablo.dll`. [R] for strings and imports. No launcher is needed for the DLL to load. Only `Storm.dll`, `D2Win.dll` and `glide3x.dll`
differ among same-named files compared with vanilla in `C:\Diablo2\ProjectD2`; `Game.exe`, `D2Common.dll`, `D2Client.dll`, `D2Game.dll`,
`Fog.dll`, `D2gfx.dll` are byte-identical. [R]

**Launcher.** `PD2Launcher.exe` is a .NET app holding UTF-16 `Game.exe` and `-3dfx`, plus ProcessStartInfo and ShellExecuteW names; a
launcher image layer mentions `-w -3dfx -nofixaspect -ns -skiptobnet -sndbkg -cpufix -widescre...`. [R] `SteamPD2.exe` is a Steam wrapper
(UpdateUtility's log starts PD2Launcher). What exact arguments it passes was not read. [I]

**Can the profile describe it?** Yes, statically: `test_game_dir=C:\Diablo2\ProjectD2`, `test_exe=Game.exe`, `test_args` as wanted,
`test_wrapper` empty. The CWD must be that folder, because PD2's D2Win uses `..\\d2data.mpq` and friends (12 strings, read from the file)
relative to it, with patch_d2 left bare. Base archives come from the parent, `C:\Diablo2`. [R] Whether it works offline without launcher
state, or tries to reach the PD2 realm, is [RT]. Note the launcher re-syncs files against `local_metadata.json` checksums, so a modified
patch_d2.mpq there could be overwritten on update. [I]

**Mount priorities.** [R] Live `ProjectDiablo.dll` (`2ff6c327...`, not S14): four mounts at `10344601`, `1034461e`, `1034463b`, `10344658`
for pd2data, pd2assets, pd2maps, pd2monchars, each `PUSH 0x1770`. So 6000, and search order (latest first) is pd2monchars, pd2maps,
pd2assets, pd2data, then patch_d2 (5000).

**Consequences.**
- pd2data.mpq holds its own `LvlPrest.bin` (503,716 bytes) and `lvlprest.txt`; pd2maps.mpq holds 453 entries besides its listfile (contents not enumerated). Their
  blocks are `0x80000100`, and both archives have a `(listfile)` and no `(attributes)`. [R]
- **A patch_d2.mpq edit under PD2 loses for any path PD2's archives hold.** It still wins for paths they don't: `townn1.ds1` misses in pd2data
  and pd2maps. [R] for those lookups. [I] for precedence.
- Getting ahead of 6000 needs -direct loose files, or a change to PD2's own archives.

**Table path.** Static evidence is that the D2Common loader path is not replaced: `(503716-4)` divides by 0x1b0 (1166 rows), so the row layout
is unchanged. [I] The guide's 84 D2Common records patch things like the count check at `D2Common+0x82cb5`, after a `CALL 6fdaef40`. [R, guide]
A literal scan of ProjectDiablo.dll for the RVAs 0x5ef40, 0x5d770, 0x5d750, 0x66160 gave zero hits. This is **weak** evidence: the records are
built at runtime and could be encoded differently. Treat it as "no evidence of a change, not proof". [RT]

**-direct and -txt under PD2** were not examined. `PD2_EXT` also splices `Fog+0x17ea7`, inside Fog 10116 `GetInstallPath`. [R] per guide + Fog address
range. The effect on -direct is unknown; base path comes from 10101, a different function, so I expect none. [I] [RT]

---

## 7. 1.14

**`C:\Diablo2\Game.exe` is 1.14b** (see corrections). The true 1.14d is in Ghidra and on `F:`. Both were scanned by bytes; 1.14b's mount sites were
read in the local file and 1.14d's in the F: copy. [R]

**Mount list:** same names, same priorities, in one call chain. Each mount is `PUSH imm32; PUSH 0; XOR EDX,EDX; MOV ECX,<name>; CALL`:

| Archive | 1.14b site | 1.14d site | Priority |
|---|---|---|---|
| d2data | 4f816a | 4fab9a | 1000 |
| d2sfx | 4f817d | 4fabad | 1000 |
| d2speech | 4f8195 | 4fabc5 | 1000 |
| d2delta | 4f81ad | 4fabdd | 1000 |
| d2kfixup | 4f81c5 | 4fabf5 | 1000 |
| patch_d2 | 4f81dd | 4fac0d | 5000 |
| d2exp | 4f81f5 | 4fac25 | 3000 |

d2char and d2music are 1000; d2Xmusic and d2Xtalk 3000, as in 1.13c. The key argument and its strings are gone. 1.14 also has a second, unexplained
set of handles at `522102`/`52211e`/`522152` (1.14b) opening d2data, patch_d2 and d2exp at 1000. Its purpose wasn't read.

**Switches.** [R] `direct` and `txt` rows exist in both. 1.14b: DIRECT present, TXT at offset 0x211. 1.14d: NOCOMPRESS 0x206, TXT 0x215 (config struct
shifted). Whether their behaviour matches 1.13c was not decompiled. Storm and D2Common are inside Game.exe there, so the ordinal harness of Q5 does
not apply to 1.14. [I]

**Implication for the default install.** `C:\Diablo2\patch_d2.mpq` is 3,915,305 bytes with 209 live blocks and no listfile; same layout as 1.13c's. [R] The
editor's default target is not the build all of this analysis is for. Addresses above do not transfer. [I]

---

## What most needs a runtime test

1. **A writer-built stored archive read by Storm 268/269 through a 32-bit harness, opened with flags 2.** Covers Q5, including small hash tables, the async
   path, and non-sector-size file tails.
2. **Replacing patch_d2.mpq with retail entries + one edited DS1 in a 1.13c test folder, launched with the CWD set.** Proves precedence, lazy DS1 open,
   and that no check fires.
3. **`-direct -txt` when `data\global\excel` is absent, present, and read-only.** Confirms -txt alone discards its compile, and that write failure is silent.
4. **A new LvlPrest row through a new .bin, including what else (Levels) must reference it.** That linkage wasn't traced.
5. **Missing-archive behaviour** (d2char, d2data, d2exp) with a registry InstallPath pointing elsewhere. Note a launch also writes InstallPath and Save Path
   to the registry.
6. **PD2**: launching Game.exe directly, offline behaviour, and whether -direct works there.
