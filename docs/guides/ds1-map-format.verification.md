# Verification report — `ds1-map-format.md`

Companion audit for [ds1-map-format.md](ds1-map-format.md). Records the ground
truth used, every claim checked, every correction applied, and everything that
could not be settled.

**Date:** 2026-08-21
**Nature of the work:** *new chapter*, not a conversion. No archived page
describing the DS1 binary layout survives; the format material in the Siramy
archive is a tool manual, not a specification. This chapter is built from the
game's own loader and from the shipped data, with Siramy's parser as a third,
non-independent witness.

---

## Origin

| | |
|---|---|
| **Original author of the format documentation** | **Paul Siramy** — `paul.siramy.free.fr`, `paul.siramy@free.fr` / `siramy_paul@yahoo.com` |
| **Original pages** | `_divers/ds1/doc/index.html` (the `ds1edit` manual) and `_divers/ds1/dl_ds1edit.html`. **There is no DS1 *format* page in the archive** — no offsets table, no struct listing. The layout knowledge survived only as code. |
| **What survived as code** | `win_ds1edit_20111030_src.zip` → `ds1misc.c` / `ds1misc.h`, `ds1save.c` / `ds1save.h`. **These files are not in `docs/preservation/`** — only the DT1 tools' C sources were archived. |
| **Modernized copies** | `docs/guides/ds1-map-format.md` and this report — both **new files**. No file under `docs/preservation/` was modified; the tree was read-only throughout. |

### Shared ancestry — the determination this chapter was asked for

**Yes: `src/core/ds1.c` derives from Siramy's DS1 code.** Two independent
statements in the repository say so:

1. [`NOTICE`](../../NOTICE), section 1, "Files that derive directly from
   win_ds1edit (current path ← original file)", lists
   `src/core/ds1.c, .h ← ds1misc.c/h, ds1save.c/h`.
2. The file's own first line:
   `/* Derived from win_ds1edit by Paul Siramy (originally ds1misc.h and ds1save.h). */`
   — present in both [`src/core/ds1.h`](../../src/core/ds1.h) and
   [`src/core/ds1.c`](../../src/core/ds1.c).

The internal evidence agrees: `prop1`…`prop4`, `lay_stream[14]`,
`dir_lookup[25]`, `w_num`/`f_num`/`s_num`/`t_num`, and the `ds1edit_error()`
failure path are all his.

**Consequence, applied throughout:** the repository's parser and Siramy's
documentation are **one source, not two**. Nothing in the chapter is presented
as "confirmed by both the archive and the repo's parser". Every load-bearing
claim rests on at least one of two genuinely independent sources — the retail
binary, and the shipped `.ds1` files — and the tags **[S]** and **[R]** are
always written together to signal that they do not add up.

A direct diff against Siramy's `ds1misc.c` was **not possible**: that file is not
in the preservation tree. The derivation is therefore established from the
repository's own declarations, not by comparing sources.

### Rights — the position taken

No page in the Siramy archive carries a licence, a copyright notice, or a
republication grant. `NOTICE` records this project's standing position for
**code** derived from `win_ds1edit` and states that Siramy's original work
"remains his". Reusing his code under his documented source-release practice is
a different act from republishing his prose or images in a book; this chapter
does the former (it quotes short excerpts of `src/core/ds1.c`, which is derived
code) and does not reproduce any archive image or page text. **The project
proceeds on an explicit fair-use judgment** covering both, recorded with its
reasoning in [BOOK-STATUS.md](../BOOK-STATUS.md); if Paul Siramy objects, the
terms change.

### Attribution chain

Siramy is credited in the Origin block, and every place the chapter confirms one
of his findings names him: the `dir_lookup` table, the `IsShadow` and `Hidden`
and `Unwalkable` bits, the layer maxima, the `trees.ds1` truncation comment, the
"Layers priority, Type of layers" label on `prop1`, the path `action` value, and
the five-block ordering of pre-version-4 files. No correction in this chapter
erases a finding of his — the two corrections below are to *this project's*
prior text, not to him.

---

## Ground truth used

| Source | Detail | Availability |
|---|---|---|
| **Ghidra — 1.13c** | `/Vanilla/1.13c/D2Common.dll` (image base `6fd50000`), `D2Client.dll` (`6fab0000`), `D2CMP.dll` (`6fe10000`) | All three used |
| **Ghidra — 1.09d** | `/Vanilla/1.09d/D2Common.dll` | Used, for the full version-gate comparison |
| **Real data — primary** | Every `.ds1` in vanilla 1.13c's own archives: `D2Data.mpq` (1 771 entries) + `D2Exp.mpq` (603), enumerated from each archive's `(listfile)`; **2 276 distinct files**, 21 664 789 bytes, all parsed | Complete |
| **Real data — census** | `.ds1` version dword read from **all 38 catalogued builds** under `F:\D2Catalog` — Classic 1.00a–1.14d and LoD 1.07a–1.14d | Complete |
| **Real data — DT1** | 263 `.dt1` files from the same archives, 17 576 distinct `(orientation, main_index, sub_index)` keys | Complete for listed files |
| **Vanilla tables** | `Objects.txt` (573 records), `MonPreset.txt` (229 rows), `ObjType.txt`, `ObjGroup.txt`, `LvlTypes.txt`, `LvlPrest.txt`, `Levels.txt` — read through the MPQ, never from `assets/excel/` | Complete |
| **Siramy's parser** | via [`src/core/ds1.c`](../../src/core/ds1.c) and [`src/structs.h`](../../src/structs.h) | Read in full; **not independent** |
| **Live fleet member** | Not used | Not applicable — DS1 is a static file format; the static image plus the shipped data are stronger and cheaper |

### Ground-truth staging notes

- **The repository's tile tree is not vanilla and was not used for any published
  number.** `assets/tiles/` holds 2 666 `.ds1` files. Compared byte-for-byte
  against the vanilla 1.13c archives: **2 159 identical, 117 different, 390 not
  present in vanilla at all**. The differing set is dominated by
  `ACT2\Arcane\sanct*.ds1`, whose sizes differ by hundreds of bytes — a mod
  signature. `assets/tiles/` is gitignored and untracked, i.e. a local
  extraction of whatever install the user last pointed the editor at.
- **`tools/d2mpq.py` does not exist in this repository** (`tools/` contains only
  `ai/`). The working copy in the session scratchpad was used, extended with a
  `read_head()` that decompresses only an archived file's first sector — which is
  what made a 38-build census tractable.
- **`objpreset.txt` does not exist in the archives.** The `(act, id) →
  Objects.txt Id` mapping had to come from the binary; it is a compiled-in
  table, not data.

### Verification scale

| | |
|---|---|
| `.ds1` files parsed end to end | 2 276 (vanilla 1.13c) |
| `.ds1` files version-sampled | 38 builds × up to 2 384 entries |
| cells walked across all layers | 5 174 713 |
| non-empty cells decomposed bit by bit | 885 185 |
| cells resolved against the DT1 catalogue | 885 185, five extraction rules each |
| tileset-list entries resolved to archive paths | 10 500 |
| objects decoded | 17 779 |
| Ghidra functions identified | 14 |
| constants confirmed against disassembly | every published one |

---

## Claim tally

| Type | Checked | Confirmed | Corrected | Unverified / open |
|---|---|---|---|---|
| **F — format/layout** (offsets, widths, gates, bit meanings) | 61 | 55 | 2 | 4 |
| **A — mechanical** (function at address, table at address, counts) | 24 | 24 | 0 | 0 |
| **B — interpretive** (what a function does) | 14 | 13 | 0 | 1 |
| **C — contextual** (provenance, tooling, conventions) | 7 | 5 | 2 | 0 |
| **D — data/asset** (table contents, corpus statistics) | 31 | 31 | 0 | 0 |
| **Totals** | **137** | **128** | **4** | **5** |

"Corrected" counts errors found in *this project's own prior text or in the
draft*, not in Siramy.

---

## The headline results

### 1. The layout is right, and the data proves it 2 234 times

Parsing all 2 276 vanilla files under the documented layout, **2 234 consume to
exactly the last byte** — no slack, no shortfall. The 42 exceptions are all
explained below and none of them is a layout error.

The one place where a wrong guess would have been plausible is the width and
height fields. Testing the alternative — that they hold the real dimensions
rather than dimensions minus one — **229 files fail to parse at all and only 5
of the surviving 2 047 land on EOF**. The disassembly agrees
(`6fd5b5fe INC EBX` / `6fd5b5ff INC ECX` / `6fd5b600 IMUL EBX,ECX`).

### 2. The cell extraction rule is confirmed three ways

`main_index = (cell >> 20) & 0x3F`, `sub_index = (cell >> 8) & 0xFF`:

- **Disassembly:** `6fd5c32f SHR ESI,0x14` + `6fd5c335 AND ESI,0x3f` +
  `6fd5c332 MOVZX EBX,AH`, in `ProcessPresetRoomObjectsAndTiles`. The same pair
  appears in `GenerateRoomColumnsAndBorders` (`6fd5bdf6`/`6fd5bdf9`) and
  `LinkTileToRoom` (`6fdb938b`/`6fdb9391`).
- **Data, against the DT1 catalogue:** resolving all 885 185 non-empty cells,
  the published rule reaches **99.96 %** (884 845). Four alternatives were run
  over the same cells:

  | rule | resolved | of 885 185 |
  |---|---|---|
  | `(cell >> 20) & 0x3F` (published) | 839 223 | 94.81 % |
  | `(cell >> 20) & 0x7F` (7-bit main) | 839 140 | 94.80 % |
  | `(cell >> 20) & 0x1F` (5-bit main) | 796 241 | 89.95 % |
  | `main = prop4` | 508 930 | 57.49 % |
  | `main = prop3` | 241 829 | 27.32 % |
  | `sub = prop1` | 0 | 0.00 % |

  *(the 94.81 % figures use only the tilesets each DS1 itself names; 99.96 % is
  against every `.dt1` in the archives — see finding 3.)* The 7-bit variant is
  separated from the published rule by only 83 cells, so the data alone would
  not have settled the field width; **the `AND …,0x3f` in the disassembly is
  what settles it**, and the 83 cells are then evidence that bit 26 is a
  separate, unread flag.
- **Siramy's parser** computes `(prop3 >> 4) + ((prop4 & 0x03) << 4)`, which is
  the same expression. Recorded, not counted as independent.

### 3. The DS1's own tileset list is decorative

Nine instructions handle it, and their only effect is to advance the file cursor
(`6fd5b5d0`–`6fd5b5f6`). No pointer is stored, nothing is copied. The real
`.dt1` names come from `LvlTypes.txt` via `LoadLevelTypesData` (`6fdbd3a0`).

The data corroborates from three directions: 6 891 of 10 500 entries carry the
extension `.tg1`, which no file in any archive has; 72 entries begin `C:\D2\`;
and resolving cells against only the DS1's own list drops coverage from 99.96 %
to 94.81 %, and the count of fully-resolved files from 2 237 to 1 793.

### 4. The `.ds1` version field does not track the game's patch level

The version histogram for DS1 versions 3–16 is **byte-identical across all 38
catalogued builds**, 1.00a through 1.14d. Only the version-18 count moves.
This reframes the whole compatibility ladder: it exists to read Blizzard's own
stale files, not files from older patches.

### 5. Two bit-level findings the sources do not state

- **Bit 27 (`prop4 & 0x08`) is `IsShadow`, exactly.** Set on all 15 146 shadow
  cells; set on none of the 870 039 wall and floor cells. Siramy's 2004 dialog
  carries an `IsShadow` column label; this identifies which bit it is.
- **Orientation is perfectly correlated with wall occupancy.** Across
  1 486 161 orientation records, orientation is non-zero on exactly the 173 437
  occupied wall cells and zero on exactly the 1 312 724 empty ones, with no
  exceptions.

- **`prop1` bit 7 correlates exactly with tileset-list membership.** All 39 303
  floor cells whose `prop1` reads `0x42` (bit 7 clear) rather than `0xC2` name a
  tile absent from the DS1's own tileset list — 39 303 of 39 303. The converse
  fails (3 022 floor and 3 637 wall cells with bit 7 set are also absent from
  their file's list), so this is a one-way exact correlation, published as such.
  No code was found that reads the bit.

Also: **six of the thirty-two cell bits are never set anywhere in vanilla** —
5, 14, 15, 18, 19 and 30 — and an empty cell is a fully zero dword in all
2 761 752 cases. `prop1` takes only eleven non-zero values across the whole
corpus: `0x81 0x85 0x89 0x91 0x99` (walls), `0x42 0xC2 0xC6 0xCA` (floors),
`0x80 0x88` (shadows).

### 6. Siramy's `dir_lookup` is confirmed by the binary

`src/core/ds1.c:362-366` declares a 25-entry orientation remap for
pre-version-7 files. The table the loader actually indexes lives at
`0x6fddbcc0` and reads
`0, 1, 2, 1, 2, 3, 3, 5, 5, 6, 6, 7, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20`
— **identical, all 25 entries**. Since the binary owes Siramy nothing, this is
one of the few places where his work is independently corroborated outright.

*(The Ghidra pass initially flagged this table as differing from "the
community's published `dir_lookup`". It differs from some circulated variant;
it does not differ from this repository's, which is Siramy's.)*

### 7. The `(act, id) → Objects.txt` map is compiled into the DLL

Table at `0x6fdee2c8`, **5 acts × 150 `int32`**, spanning `0x6fdee2c8`–`0x6fdeee80`
with no slack before the next object. Formula:
`objectsTxtId = id < 150 ? table[act * 150 + id] : id - 150`
(`6fd5b80f`, `6fd5b81b`, `6fd5b823`, `6fd5b82f`). Eight Act-1 entries were read
from memory and cross-checked against vanilla `Objects.txt`:

| DS1 id | table | `Objects.txt` Id | name |
|---|---|---|---|
| 0 | 12 | 12 | `Dummy / RogueFountain` |
| 1 | 37 | 37 | `Dummy / Torch1 Tiki` |
| 3 | 35 | 35 | flag 1 |
| 4 | 36 | 36 | flag 2 |
| 33 | 78 | 78 | invisible town sound |
| 52 | 119 | 119 | `Waypoint / waypoint portal` |
| 102 | 267 | 267 | `bank / bank` |
| 110 | 385 | 385 | `Dummy / cain start position` |

All eight match `assets/editor/obj.txt` (Siramy's transcription of the same
table) exactly — which retroactively validates that file as an accurate copy of
a binary table, something nothing in the repository previously stated.

The pre-version-6 path rejects a type-2 id at **573** (`6fd5b83a CMP EDI,0x23d`),
which is exactly the vanilla `Objects.txt` record count.

### 8. The `+800` slack is load-bearing

`RESOURCE_AllocateAndOpen` (`6fd59900`) allocates `filesize + 800` for every
archive read. Two shipped presets need it: `ACT1\OUTDOORS\trees.ds1` reads 12
bytes past EOF and `expansion\Siege\xtransition.ds1` 4 bytes past. **A
reimplementation that bounds-checks strictly rejects two files the retail game
loads without complaint.** Siramy hit `trees.ds1` in 2002 and left a comment
about it; the *reason* it works is in the allocator, not the parser.

---

## Corrections applied

### C1 — `assets/tiles/` is not a vanilla corpus

**Before (in the draft, and implicitly in a sibling chapter):** structural
statistics quoted over "all 2 666 `.ds1` files under `assets/tiles/`".
**After:** every number in this chapter comes from the 2 276 distinct `.ds1`
files in vanilla 1.13c's own archives.
**Evidence:** byte-comparing `assets/tiles/` against `D2Data.mpq` + `D2Exp.mpq`:
2 159 identical, **117 different**, **390 absent from vanilla**. The differing
set is concentrated in `ACT2\Arcane\sanct*.ds1`.
**Consequence for existing text:** the verified block at
`docs/getting-started/manual.md:775-823` quotes corpus figures from the same
2 666-file tree (17.5 M wall cells, Hidden set 10 562 times, and so on). Those
numbers are inflated by ~17 % of non-vanilla files. That file is outside this
chapter's ownership and was **not edited**; the divergence is recorded here.
The corresponding vanilla figures are: 1 486 161 wall cells; Hidden set on
9 441 cells in total (2 744 wall, 6 690 floor, 7 shadow).

### C2 — "Orientation 10/11 files are dominated by `*warp*.ds1`" overstates it

**Before:** `manual.md:809-814` — orientation 10 and 11 "concentrated in 546
files whose names are dominated by `*warp*.ds1`".
**After:** in vanilla 1.13c, orientation 10 occurs 654 times in 326 files and
orientation 11 292 times in 160 files; of the union, **65 filenames contain
"warp"**. Narrowing to actual warps — orientation 10/11 with `main_index` 30–33,
which is the loader's own test — there are **96 cells in 45 files, exactly one
of which is named `*warp*`**. The values are special; the naming convention is
not what identifies them.
**Evidence:** `6fd5c45f CMP ESI,0x1e` / `6fd5c468 CMP ESI,0x21` **[G]**, plus
the corpus census **[D]**. Again, `manual.md` was not edited.

### C3 — The group record's fifth dword is not always zero

**Before (draft):** "the `unknown` fifth dword is zero in every vanilla group".
**After:** 101 of the 893 readable vanilla group records carry a non-zero value.
**Evidence:** direct scan of the group blocks in all 151 group-bearing files.

### C4 — Tag values and group counts

**Before (draft):** "tag values run to 134 while group counts run to 24 … so tag
values are not plain zero-based group indices" — the group-count figure was
taken from the non-vanilla tree and was wrong.
**After:** both run to 134, and in **143 of 151** files the maximum tag value
*equals* the group count — consistent with one-based indexing where 0 means "no
group". Seven files exceed it, one falls short. Stated as consistent, not proven,
because the tag layer's consumer was not located in the binary.

---

## Reconciled contradictions

**`dt1-tile-format.md` vs. the bytes.** That chapter states "the DS1 names none
of them", of the `.dt1` files a level loads. Literally, every DS1 from version 3
onward names tilesets — 10 500 entries across vanilla 1.13c. Functionally, the
DT1 chapter is right: the loader skips them and `LvlTypes.txt` supplies the real
names. Both describe the same fact; the DS1 chapter carries an explicit
reconciliation note so a reader meeting the two sentences does not conclude one
of them is wrong.

**Siramy's parser vs. the loader, on the pre-version-4 layer order.** The two
agree exactly — wall 1, floor 1, orientation 1, tag, shadow — which is worth
recording because it is a five-way ordering that only one vanilla file exercises,
and getting it wrong would be invisible in a size check.

**Siramy's parser vs. the loader, on layer maxima.** `src/structs.h` declares
4/2/1/1 and the preset struct's pointer arrays are 4/2/1/1. The difference is
that the loader **does not enforce** them — nothing clamps the counts read from
the file. The chapter states the maxima as a property of well-formed files
rather than as a validated constraint.

---

## Type-B claims: sampling policy and verdicts

Every function whose stated purpose carried a behavioural claim was read in
disassembly, not decompiled-and-skimmed:

| Function | Address | Verdict |
|---|---|---|
| `LoadAndParsePresetArchive` | `6fd5b560` | Confirmed — the DS1 parser; read in full (518 instructions) |
| `GetOrCreatePresetFileCacheEntry` | `6fd5be50` | Confirmed — allocates and zeroes the 0x5C preset struct |
| `RESOURCE_AllocateAndOpen` | `6fd59900` | Confirmed — `size + 800`, no header validation |
| `ProcessPresetRoomObjectsAndTiles` | `6fd5c060` | Confirmed — wall/orientation scan, warp and special-tile extraction |
| `CreateDrlgRoomsFromPreset` | `6fd5c690` | Confirmed — splits a preset into 8×8 rooms |
| `LoadPresetAndGenerateRoomContent` | `6fd5bf70` | Confirmed — cache lookup by `lvlprest` File index |
| `ProcessDrlgPresetObjectsWithRandom` | `6fd5b320` | Confirmed — consumes the object list, dispatches on node `+0x14` |
| `LoadLevelPresetData` | `6fdc0110` | Confirmed — `lvlprest.txt`, `File1`=`0x44` … `File6`=`0x170`, record `0x1B0` |
| `LoadLevelTypesData` | `6fdbd3a0` | Confirmed — `lvltypes.txt`, 32 × 60-byte name fields, record `0x788` |
| `GenerateRoomColumnsAndBorders` | `6fd5bd60` | Confirmed — an independent floor-cell decoder |
| `LinkTileToRoom` | `6fdb9360` | Confirmed — a third independent cell decoder |
| monpreset loader | `6fda5490` | Confirmed — builds the per-act row pointers the parser indexes |
| `ComputeTileHashIndex` | `6fe22e10` (D2CMP) | Confirmed — `(sub*2 − orient + main) & 0x7F` |
| `InsertTileIntoLookupTable` | `6fe22f20` (D2CMP) | **Open** — reads tile-header `+0x14/+0x18/+0x1C` and hashes them, but its only in-database caller sits in D2CMP's tile-*compiler* cluster. The runtime cell→tile resolution path was not traced. |

---

## Unverified, and why

1. **The runtime DS1-cell → DT1-tile handoff.** The key construction is proven
   in three D2Common functions; the path from there to a blit was not traced.
   D2Client contains no `(cell >> 20) & 0x3F` anywhere, so it must receive
   decoded values, but the call was not followed.
2. **`prop1`'s bits 2, 3 and 4** (601, 289 and 2 625 cells). None of the three
   DRLG decoders reads the low byte at all. Whether some other module does was
   not audited.
3. **Cell bits 16, 26, 28 and 29.** Present in real data, read by nothing found.
4. **The version 9–13 dword pair.** Skipped by `ADD ESI,0x8`. All 43 vanilla
   files in that window store `(5000, 5000)`; what the authoring tool meant by
   it is unknown.
5. **The version-18 pre-group dword.** Zero in all 117 vanilla files that have it.
6. **The group record's fifth dword**, and which code reads a tag-layer cell.
7. **The object flags dword.** Value 1 on 23 of 17 779 records; no consumer
   identified.
8. **Eight wall cells at orientation 13** (the shadow orientation).
9. **340 cells (0.04 %) that resolve against no `.dt1` in the archives.** They
   cluster in `expansion\Siege\` border and sample presets. Note the DT1 side is
   limited to the 263 `.dt1` names that appear in the archives' `(listfile)`;
   MPQ lookup is by hash, so unlisted files can exist and would not have been
   included.
10. **Every game version except 1.09d and 1.13c.** The chapter's game-version
    table marks 1.00a–1.12a and 1.13d–1.14d `(unverified)` rather than assuming
    the two disassembled versions bracket them.
11. **`FUN_6fd791d0`**, the 1.09d orientation-remap helper — located, not
    decompiled, so the chapter does not claim 1.09d's remap table matches
    1.13c's.

---

## Decompiler errors encountered

The Ghidra database's decompiler rendered four global base addresses wrongly.
Each would have propagated into the chapter as a published constant if the
decompiled C had been trusted:

| Decompiler output | Actual instruction | Delta |
|---|---|---|
| `&g_adwData_6fde1fc4_61_ + …` | `6fd5b6c2 MOV EBP,[EBP*0x4 + 0x6fddbcc0]` | −0x6304 |
| `*(int *)(&g_dwPad_6fdf6304 + …)` | `6fd5b875 MOV EAX,[EBX*0x4 + 0x6fdf0984]` | +0x5980 |
| `(&g_dwData_0998)[local_c]` | `6fd5b87e MOV ECX,[EBX*0x4 + 0x6fdf0998]` | truncated symbol |
| `&g_adwData_6fdef62c_1_ + …` | `6fd5b823 MOV EDI,[EDX*0x4 + 0x6fdee2c8]` | +0x1364 |

The decompiler also renders every layer-pointer advance as plain pointer
arithmetic, hiding the ×4 that `LEA ECX,[EBX*0x4 + 0x0]` makes explicit — the
one place a reader could plausibly have concluded the cell stride was one byte.

Two constants in the chapter come from reading the disassembly *sequence* rather
than a single instruction, and are marked as such here: the object file-record
size (16 / 20 bytes — there is no `imul`; the stride is the sum of the
`ADD ESI,0x4`s along each path) and the `+800` allocator slack.

---

## Open questions for the author

1. **`assets/tiles/` provenance.** Which install was it extracted from? 117
   files differ from vanilla 1.13c and 390 are absent from it. If it is a PD2
   extraction, the sibling `manual.md` verification block should be re-run
   against vanilla.
2. **`tools/d2mpq.py`.** The chapter's data work needed it and it is not in the
   repository yet. The scratchpad copy plus a first-sector `read_head()` is what
   made the 38-build census possible; that helper is worth landing with the tool.
3. **Should the DS1 `act` field be trusted by the editor?** It disagrees with the
   file's own directory in 176 of 2 274 vanilla files, and it selects the
   object-id remap table. An editor that shows object names using the directory's
   act rather than the file's act will disagree with the game on those files.
4. **The `.tg1` extension.** Nothing in the archive explains it. It is almost
   certainly the extension of Blizzard's internal tile-project format, but that
   is inference, not evidence.

---

## Files touched

| File | Action |
|---|---|
| `docs/guides/ds1-map-format.md` | **created** |
| `docs/guides/ds1-map-format.verification.md` | **created** (this file) |
| everything else | read-only — no file under `docs/preservation/`, `docs/getting-started/`, `docs/tutorials/`, `src/`, `tools/` or `assets/` was modified |
