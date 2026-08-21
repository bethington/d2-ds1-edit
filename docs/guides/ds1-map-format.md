# DS1: The Map on Disk

> **Origin.** The DS1 format was documented for the modding community by
> **Paul Siramy** (`paul.siramy.free.fr`, `paul.siramy@free.fr`), who wrote
> `ds1edit` / `win_ds1edit` from 2002 onward and published its source beside
> every binary. His DS1 reader and writer — `ds1misc.c` and `ds1save.c` — are
> the ancestor of this repository's [`src/core/ds1.c`](../../src/core/ds1.c),
> which still carries his structure names (`prop1`…`prop4`, `lay_stream`,
> `dir_lookup`) and his control flow. That descent is recorded in
> [`NOTICE`](../../NOTICE) at the repository root and in the header comment of
> the file itself.
>
> **This matters for how the evidence in this chapter is weighed.** Because the
> parser derives from Siramy's, *the parser agreeing with Siramy is not two
> witnesses.* It is one witness wearing two hats. Every load-bearing claim below
> is therefore settled against sources that owe him nothing: the game's own
> loader in the retail binaries, and the shipped `.ds1` files themselves. Where
> those confirm him — and they very largely do — the chapter says so, and the
> credit is his.
>
> **Rights: the position taken.** No page in the Siramy archive carries a
> licence, a copyright notice, or a republication grant. `NOTICE` records this
> project's standing attribution position for *code* derived from
> `win_ds1edit`; separately, the project proceeds on an explicit fair-use
> judgment for republishing his prose and images in a book, recorded with its
> reasoning in [BOOK-STATUS.md](../BOOK-STATUS.md). If Paul Siramy objects,
> the terms change.

> **Provenance.** Verification performed 2026-08-21. Every non-obvious claim
> below carries a source tag:
>
> - **[G]** the retail binaries in Ghidra — `D2Common.dll` 1.13c (image base
>   `6fd50000`) and 1.09d, cross-checked against `D2Client.dll` (`6fab0000`)
>   and `D2CMP.dll` (`6fe10000`). Every published constant was read from the
>   **disassembly**, not the decompiler; four decompiler-rendered global
>   addresses in this database are wrong and are listed in the companion report.
> - **[D]** real data — **all 2,276 distinct `.ds1` files** in vanilla 1.13c's
>   own archives (`D2Data.mpq` + `D2Exp.mpq`, read through their `(listfile)`),
>   plus a `.ds1` version census across **all 38 catalogued builds** from
>   Classic 1.00a to LoD 1.14d.
> - **[S]** Paul Siramy's documentation, as transcribed into
>   [`docs/getting-started/manual.md`](../getting-started/manual.md).
> - **[R]** this repository's parser, [`src/core/ds1.c`](../../src/core/ds1.c)
>   — **derived from [S]**, so never counted as independent of it.
> - **[I]** inference, where nothing settles it directly.
>
> Data claims are checked against **vanilla archives only**. The tile tree under
> `assets/tiles/` is a local extraction that is *not* pure vanilla — of its 2,666
> files, 117 differ byte-for-byte from the 1.13c archives and 390 are not in them
> at all — so no number in this chapter comes from it.
>
> Companion audit: [ds1-map-format.verification.md](ds1-map-format.verification.md).

---

## A map file with no map in it

Open a `.dt1` and you find pictures: tiles, sub-tiles, runs of palette indices.
Open a `.ds1` and you find almost nothing you could look at. It is a grid of
**references** — a spreadsheet whose every cell says *put tile number such-and-such
here*, and never says what that tile looks like.

That division is the whole design. Diablo II's outdoor areas and dungeons are
assembled at run time by the level generator (DRLG) out of rectangular pieces.
A `.ds1` is one such piece — Blizzard's code calls it a **preset** — and it
carries five kinds of thing:

1. a small header: how big the piece is, and which act it was authored for;
2. a list of tileset filenames, which the game reads the length of and throws away;
3. the **cell layers** — up to four walls, two floors, one shadow, one tag —
   each a flat array of 32-bit references, one per tile;
4. an **object list**: monsters, NPCs, chests, waypoints, portals;
5. optional **groups** (rectangles over the tag layer) and **NPC paths**
   (waypoint routes the town characters walk).

Nothing in the file is compressed, checksummed, or aligned to anything but four
bytes. There is no magic number. The first four bytes are a version, and from
there the layout is a chain of conditionals on that one value.

Throughout this chapter one file is followed from its first byte to its last:

> **The worked example.** `data\global\tiles\ACT1\Town\townE1.ds1` — the Rogue
> Encampment, east variant. 57 691 bytes, read from `D2Exp.mpq`. It is version
> 18, 57 × 41 tiles, names nine tilesets, carries two wall layers, one floor,
> one shadow, 43 objects, three warps and five walking NPCs. Every offset quoted
> below is a real offset in that file.

---

## Eighteen versions, and no rejection

The first dword is the file-format version. Eight distinct values ship in retail
data — 3, 8, 12, 13, 15, 16, 17 and 18 — and the loader's entire structure is a
ladder of comparisons against it.

The first thing to know is what the loader does *not* do. There is no maximum.
`LoadAndParsePresetArchive` (D2Common 1.13c `6fd5b560`) loads the version into
`EBP` at `6fd5b57c` and compares it fifteen times; the largest constant in any
of those comparisons is `0x12` (18), at `6fd5ba2e`. There is no upper-bound
test, no assert, and no rejection path — and the file-reading routine that hands
it the buffer, `RESOURCE_AllocateAndOpen` (`6fd59900`), does not inspect the
header either **[G]**. A file claiming version 40 is parsed exactly as a version
18 file; a file claiming version 0 is parsed as a grid with no object block. The
comparisons are all signed (`JL`/`JGE`/`JLE`/`JG`), so a negative version takes
every "below" branch and still parses.

The second thing to know is that the version field does not track the game's
patch level. It tracks the age of the *file*. Every `.ds1` version that exists
in retail data already existed when Diablo II shipped:

| Build | `.ds1` files | v3 | v8 | v12 | v13 | v15 | v16 | v17 | v18 |
|---|---|---|---|---|---|---|---|---|---|
| Classic 1.00a – 1.03a, 1.10a – 1.14d | 1 771 | 1 | 6 | 14 | 36 | 15 | 229 | 139 | 1 331 |
| Classic 1.04b – 1.05b | 1 780 | 1 | 6 | 14 | 36 | 15 | 229 | 139 | 1 340 |
| Classic 1.06a – 1.06b | 1 788 | 1 | 6 | 14 | 36 | 15 | 229 | 139 | 1 348 |
| Classic 1.08a – 1.09d | 1 869 | 1 | 6 | 14 | 36 | 15 | 229 | 139 | 1 429 |
| LoD 1.07a, 1.10a – 1.14d | 2 374 | 1 | 6 | 14 | 36 | 15 | 229 | 147 | 1 926 |
| LoD 1.08a – 1.09d | 2 384 | 1 | 6 | 14 | 36 | 15 | 229 | 147 | 1 936 |

*(**[D]**, `.ds1` entries enumerated from each build's own MPQ `(listfile)`;
counts are per-archive entries, so the 98 names that appear in both `D2Data.mpq`
and `D2Exp.mpq` are counted twice. 1.13c LoD has 2 276 **distinct** files.)*

Read that table sideways. The counts for versions 3 through 16 are **identical
in every build Blizzard ever shipped**, from 1.00a in 2000 to 1.14d in 2016.
Patches added and removed version-18 files and nothing else. So the older
versions in the archives are not the fossils of old patches — they are files
Blizzard authored early, never re-saved, and shipped unchanged for sixteen
years. The loader's back-compatibility ladder exists to read Blizzard's own
stale content.

Here is the whole ladder, every gate confirmed from the instruction that
implements it **[G]**:

| Gate | Instruction (1.13c) | What it turns on |
|---|---|---|
| v ≥ 2 | `6fd5b704 CMP EBP,0x1` / `JLE` | the object block exists |
| v ≥ 3 | `6fd5b5cb CMP EBP,0x3` / `JL` | the tileset-filename list |
| v ≥ 4 | `6fd5b610 CMP EBP,0x4` / `JL` | explicit wall-layer count |
| v ≥ 5 | `6fd5b74f`, `6fd5b84e` | type-1 objects remap through `monpreset`; type-4 objects appear |
| v ≥ 6 | `6fd5b980 CMP EAX,0x5` / `JLE` | per-object flags dword; type-2 objects remap |
| v < 7 | `6fd5b6a1 CMP EBP,0x7` / `JGE` | orientation values pass through a 25-entry table |
| v ≥ 8 | `6fd5b58f CMP EBP,0x8` / `JL` | the `act` dword |
| 9 ≤ v ≤ 13 | `6fd5b603 CMP EBP,0x9` + `6fd5b608 CMP EBP,0xe` | two dwords skipped |
| v ≥ 10 | `6fd5b5b9 CMP EBP,0xa` / `JL` | the `tagType` dword |
| v ≥ 12 | `6fd5ba0c CMP …,0xc` / `JL` | the group block |
| v ≥ 13 | `6fd5ba9e CMP EDX,0xd` / `JL` | a fifth dword per group record |
| v ≥ 14 | `6fd5bac2 CMP EBX,0xe` / `JL` | the NPC path block |
| v ≥ 15 | `6fd5bb05`, `6fd5bb81 CMP EBX,0xf` | a third dword per path point (`action`) |
| v ≥ 16 | `6fd5b61a CMP EBP,0x10` / `JL` | explicit floor-layer count |
| v ≥ 18 | `6fd5ba2e CMP …,0x12` / `JL` | one extra dword before the group count |

> **Version note (1.09d):** the ladder is identical — same fifteen thresholds,
> same order, same signedness, same absence of a maximum (`6fd76576`,
> `6fd76594`, `6fd765a6`, `6fd765cf`, `6fd765e0`, `6fd765ea`, `6fd7666c`,
> `6fd766e7`, `6fd768d0`, `6fd76927`, `6fd76942`, `6fd769a6`, `6fd769c6`,
> `6fd76a0b`) **[G]**. The DS1 version repertoire did not change between 1.09d
> and 1.13c.

---

## The header

Everything before the cell data is a short run of dwords, three of them
unconditional and the rest gated. In the worked example it is 447 bytes, of
which 415 are filename strings and only 32 are fields.

```
offset  bytes                     field
0000    12 00 00 00               version    = 18
0004    38 00 00 00               width  − 1 = 56   → 57 tiles
0008    28 00 00 00               height − 1 = 40   → 41 tiles
000c    00 00 00 00               act        = 0    → Act 1
0010    00 00 00 00               tagType    = 0
0014    09 00 00 00               fileCount  = 9
0018    5c 64 32 5c 64 61 74 61…  "\d2\data\global\tiles\act1\outdoors\treegroups.dt1"
  …     (nine NUL-terminated strings, 415 bytes in all, ending at 01b6)
01b7    02 00 00 00               wall layers  = 2
01bb    01 00 00 00               floor layers = 1
01bf    ← cell data begins
```

| File offset | Size | Field | Gate | Where it goes **[G]** |
|---|---|---|---|---|
| `+0x00` | 4 | **version** | — | register only, never stored |
| `+0x04` | 4 | **width − 1** | always | `preset+0x0C` |
| `+0x08` | 4 | **height − 1** | always | `preset+0x10` |
| next | 4 | **act**, 0-based, clamped to 4 | v ≥ 8 | stack local, never stored in the preset |
| next | 4 | **tagType** | v ≥ 10 | `preset+0x00` |
| next | 4 + strings | **tileset filename list** | v ≥ 3 | **discarded** |
| next | 8 | two unknown dwords | 9 ≤ v ≤ 13 | skipped (`ADD ESI,0x8`) |
| next | 4 | **wall-layer count** | v ≥ 4 | `preset+0x14` |
| next | 4 | **floor-layer count** | v ≥ 16 | `preset+0x18` (else forced to 1) |

### Width and height are stored one short

The two size dwords hold `width − 1` and `height − 1`. The loader adds one to
each before multiplying them into a cell count:

```
6fd5b5f8  MOV EBX,dword ptr [EDI + 0x10]    ; height−1
6fd5b5fb  MOV ECX,dword ptr [EDI + 0xc]     ; width−1
6fd5b5fe  INC EBX
6fd5b5ff  INC ECX
6fd5b600  IMUL EBX,ECX                      ; cells = (h−1+1) * (w−1+1)
```

**[G]**, and the data is unambiguous about it. Parsing the whole vanilla corpus
under the "+1" rule, **2 234 of 2 276 files consume to exactly the last byte**;
parsing it under the alternative — the fields hold the real width and height —
**229 files fail outright and only 5 of the remaining 2 047 land on the end of
the file** **[D]**. There is no third possibility to consider.

Which dword is width is settled by the scan loop in
`ProcessPresetRoomObjectsAndTiles` (`6fd5c060`), where `preset+0x0C` is the
inner (x) bound and `preset+0x10` the outer (y) bound
(`6fd5c517`/`6fd5c51b` against `6fd5c527`/`6fd5c52b`) **[G]**.

Across vanilla, widths run from 2 to 101 tiles and heights from 2 to 103; file
sizes run from 178 bytes (`ACT1\OUTDOORS\waysmall.ds1`, a 2 × 2 waypoint stub)
to 204 836 **[D]**.

### The act field is zero-based, and it is clamped to 4

```
6fd5b5a3  MOV EAX,dword ptr [ESI]
6fd5b5a8  CMP EAX,0x4
6fd5b5ab  MOV dword ptr [ESP + 0x1c],EAX
6fd5b5af  JLE 0x6fd5b5b9
6fd5b5b1  MOV dword ptr [ESP + 0x1c],0x4      ; clamp to 4, not 5
```

Zero means Act 1. The proof is not in the parser but in the table it indexes:
the `monpreset.txt` loader (`6fda5490`) groups records so that group *k* holds
the rows whose `Act` column reads *k + 1* (`6fda5524 MOVZX EAX,byte ptr [EDX]`,
`6fda5527 LEA EDI,[ECX + 0x1]`, `6fda552a CMP EAX,EDI`), and the parser then
indexes that array with the raw DS1 act value, bounds-checked `0 ≤ act < 5`
(`6fd5b864`, `6fd5b868`) **[G]**.

The stored value agrees with the directory the file lives in for 2 098 of the
2 274 files that sit under an act-named directory **[D]** — good enough to
confirm the base, not good enough to treat the field as reliable. The 176
disagreements cluster in tilesets reused across acts (49 in `ACT2\Maggot`, 28 in
`ACT2\Outdoors`, 21 in `ACT2\BigCliff`).

And 51 vanilla files store **5** — a sixth act, which does not exist: two in
`Arena\Tombs\` and 49 in `expansion\Siege\` **[D]**. The clamp is what rescues
them. For the 49 Siege files it produces exactly the right answer (act 4 = Act 5,
which is where that content lives, written one-based by whoever authored it); for
the two unused Arena files it is harmless. A line that looks like defensive
paranoia is load-bearing for 49 shipped files.

> **Version note (DS1 file versions ≤ 7):** there is no act dword at all, and the
> loader leaves its local at 0 — Act 1. One vanilla file is affected
> (`ACT1\COURT\court1b.ds1`, version 3) **[D]**.

### `tagType` gates two blocks and nothing else

The `tagType` dword (v ≥ 10) is stored at `preset+0x00` (`6fd5b5c6`). It is
tested in exactly two places, both requiring `1 ≤ tagType ≤ 2`: whether a tag
layer is consumed (`6fd5b6f6`, `6fd5b6fa`, `6fd5b6ff`) and whether the group
block is read (`6fd5ba1f`, `6fd5ba25`) **[G]**. Vanilla: 2 125 files at 0,
73 at 1, 78 at 2 **[D]**.

---

## The tileset list the game throws away

After the header, a version-3-or-later file names the `.dt1` tilesets it draws
from: a count dword, then that many NUL-terminated strings. The worked example
names nine:

```
 0  \d2\data\global\tiles\act1\outdoors\treegroups.dt1
 1  \d2\data\global\tiles\act1\outdoors\stonewall.dt1
 2  \d2\data\global\tiles\act1\town\floor.dt1
 3  \d2\data\global\tiles\act1\outdoors\bridge.dt1
 4  \d2\data\global\tiles\act1\barracks\warp.dt1
 5  \d2\data\global\tiles\act1\outdoors\river.dt1
 6  \d2\data\global\tiles\act1\town\objects.dt1
 7  \d2\data\global\tiles\act1\town\fence.dt1
 8  \d2\data\global\tiles\act1\outdoors\objects.dt1
```

They look authoritative. They are not read.

The loader's entire handling of this block is a strlen-and-advance loop whose
only effect is to move the file cursor. Nothing is copied, and no pointer to the
strings is kept:

```
6fd5b5d0  MOV EAX,dword ptr [ESI]                 ; count
6fd5b5d2  ADD ESI,0x4
6fd5b5e8  MOV DL,byte ptr [EAX + ESI*0x1 + 0x1]   ; strlen scan
6fd5b5ec  INC EAX
6fd5b5ef  JNZ 0x6fd5b5e8
6fd5b5f2  LEA ESI,[ESI + EAX*0x1 + 0x1]           ; cursor += strlen + 1
6fd5b5f6  JNZ 0x6fd5b5e0
```

**[G]**; identical in 1.09d at `6fd765a6`–`6fd765c2`, which calls a strlen
helper instead of inlining it. The `.dt1` filenames the engine actually loads
come from `LvlTypes.txt`, whose 32 `File` columns are parsed by
`LoadLevelTypesData` (`6fdbd3a0`, record size `0x788`, 60-byte name fields at
`0x00, 0x3C, 0x78 … 0x744`) and resolved under `DATA\GLOBAL\TILES` **[G]**.

Three independent signs say the same thing from the data side **[D]**:

- **The extension is usually wrong.** Of the 10 500 entries across the vanilla
  corpus, 6 891 end in `.tg1` and only 3 609 in `.dt1` — and there is no `.tg1`
  file anywhere in the archives. `.tg1` was the extension of Blizzard's internal
  tile-project format; the list preserves authoring-time paths.
- **The prefix is usually wrong too.** 8 861 entries begin `\d2\`, 1 567 begin
  `\D2\`, and 72 begin `C:\D2\` — someone's local drive letter, shipped to
  retail.
- **The list is incomplete.** Resolving every non-empty cell in the corpus
  against only the tilesets its own file names accounts for **94.81 %** of
  885 185 cells. Resolving the same cells against every `.dt1` in the archives
  accounts for **99.96 %** (884 845 / 885 185), and takes the count of files
  in which *every* cell resolves from 1 793 to 2 237 of 2 276.

A DS1 editor that rewrites this list changes nothing the game will ever look at.
The list is still worth reading — it is the best available hint about a preset's
intended tilesets — but it is a comment, not a dependency.

> **Reconciliation.** [DT1: Tiles on Disk](dt1-tile-format.md) says "the DS1
> names none of them". That is right about the *effect* and wrong about the
> *bytes*: every DS1 from version 3 onward does carry a tileset list — 10 500
> entries across vanilla 1.13c — and the game does not read it. Both statements
> describe the same fact from opposite ends.

---

## The layers

Immediately after the header come the cell layers, back to back, with no
separator and no lengths. Each is exactly `width × height` dwords, row-major, x
fastest. Knowing how many layers there are is therefore the difference between
parsing the file and parsing noise.

A file at version 4 or later states its wall count; at version 16 or later it
also states its floor count. A shadow layer is always present and is never
counted in the file. A tag layer is present when `tagType` is 1 or 2.

The order is fixed, and the walls are **interleaved with their orientations**:

```
wall 1, orientation 1, wall 2, orientation 2, … ,
floor 1 … floor N, shadow, [tag]
```

which the loader lays down as a straight run of pointer stores — there is no
ordering table anywhere in `.data` **[G]**:

```
6fd5b63c  LEA ECX,[EBX*0x4 + 0x0]         ; layer size in BYTES = cells * 4
6fd5b643  LEA EAX,[EDI + 0x1c]
6fd5b646  MOV dword ptr [EAX + 0x10],ESI  ; preset+0x2C+4i  ← wall i
6fd5b649  ADD ESI,ECX
6fd5b64b  MOV dword ptr [EAX],ESI         ; preset+0x1C+4i  ← orientation i
6fd5b64d  ADD ESI,ECX
```

> **Version note (DS1 file versions ≤ 3):** there are no layer counts. The
> loader takes exactly five blocks in a **different** order — wall 1, floor 1,
> orientation 1, tag, shadow — and forces the wall count to 1
> (`6fd5b686` → `preset+0x2C`, `6fd5b68b` → `+0x3C`, `6fd5b690` → `+0x1C`,
> `6fd5b695` → `+0x48`, `6fd5b698 MOV …+0x14,0x1`, `6fd5b6f1` → `+0x44`)
> **[G]**. A tag layer is consumed even though `tagType` does not exist below
> version 10. Siramy's parser encodes the same five-block order **[S] [R]**, and
> the one vanilla file at this version, `ACT1\COURT\court1b.ds1`, parses to
> exactly its last byte under it **[D]**.

### The maxima are a struct shape, not a check

The preset structure is 92 bytes (`6fd5bedf MOV EDX,0x5c`), zeroed on allocation
(`6fd5bef1 MOV ECX,0x17` + `STOSD.REP`), and its pointer arrays are sized:
four orientation slots at `+0x1C`, four wall slots at `+0x2C`, two floor slots at
`+0x3C`, one shadow at `+0x44`, one tag at `+0x48` **[G]**.

**Nothing clamps the counts read from the file.** The wall loop at `6fd5b646`
and the floor loop at `6fd5b670` are bounded only by the file's own numbers, so
a file declaring five walls writes its fifth orientation pointer over wall 1 and
its fifth wall pointer over floor 1; three floors overwrites the shadow pointer.
The published maxima — **4 walls, 2 floors, 1 shadow, 1 tag** — are therefore the
limits of a *well-formed* file, enforced by the struct's shape and by nothing
else. Siramy's `WALL_MAX_LAYER 4` / `FLOOR_MAX_LAYER 2` / `SHADOW_MAX_LAYER 1`
/ `TAG_MAX_LAYER 1` ([`src/structs.h:30-33`](../../src/structs.h)) **[S] [R]** state
the same four numbers, and no vanilla file exceeds any of them **[D]**:

| | walls | floors |
|---|---|---|
| 0 | 51 | 3 |
| 1 | 1 155 | 1 724 |
| 2 | 763 | 549 |
| 3 | 137 | — |
| 4 | 170 | — |

Zero is legal and occurs: 51 vanilla files have no wall layer, 3 have no floor
layer, and the loader guards the empty case with `6fd5b638 TEST EAX,EAX / JLE`
**[G] [D]**.

**In the worked example** the header's last two dwords are `02` and `01`, so the
layer run is `w0, o0, w1, o1, f0, s0` — six layers of 57 × 41 × 4 = 9 348 bytes
each, 56 088 bytes total, at file offsets `0x01BF`, `0x2643`, `0x4AC7`,
`0x6F4B`, `0x93CF` and `0xB853`.

---

## The cell

Every cell in every layer except orientation is one little-endian 32-bit word.
Siramy's parser reads it as four bytes named `prop1`…`prop4`; the game reads it
as a dword and shifts. They describe the same bits, and the dword view is the
one the binary states:

```
6fd5c32a  MOV EAX,dword ptr [ESI + EAX*0x4]   ; the cell
6fd5c32d  MOV ESI,EAX                         ; working copy
6fd5c32f  SHR ESI,0x14                        ; >> 20
6fd5c332  MOVZX EBX,AH                        ; sub_index  = byte 1
6fd5c335  AND ESI,0x3f                        ; main_index = (cell >> 20) & 0x3F
6fd5c338  AND EAX,0x80000000                  ; the hidden flag
```

**[G]**. `main_index = (cell >> 20) & 0x3F` is algebraically identical to
Siramy's `(prop3 >> 4) + ((prop4 & 0x03) << 4)`
([`src/core/ds1.c:137`](../../src/core/ds1.c)) **[S] [R]** — the binary simply
states it in one operation. The same extraction appears twice more in D2Common,
in `GenerateRoomColumnsAndBorders` (`6fd5bdf6`/`6fd5bdf9`, the floor path) and
`LinkTileToRoom` (`6fdb938b`/`6fdb9391`) **[G]**.

Together with the orientation, that gives the three-part key a `.dt1` answers
to: `(orientation, main_index, sub_index)`. See
[DT1: Tiles on Disk](dt1-tile-format.md) for the other side of the lookup.

### The full bit map

Every one of the 885 185 non-empty cells in vanilla 1.13c was decomposed bit by
bit **[D]**. The table below gives, for each bit, how many cells set it,
split by layer.

| Bit | Mask (`propN`) | Meaning | floor 1 | floor 2 | shadow | wall 1 | wall 2 | wall 3 | wall 4 |
|---|---|---|---|---|---|---|---|---|---|
| 0 | `prop1 & 0x01` | **wall-layer tag** | 0 | 0 | 0 | 160 240 | 7 922 | 4 043 | 1 232 |
| 1 | `prop1 & 0x02` | **floor-layer tag** | 666 155 | 30 447 | 0 | 0 | 0 | 0 | 0 |
| 2 | `prop1 & 0x04` | unknown | 301 | 0 | 0 | 296 | 4 | 0 | 0 |
| 3 | `prop1 & 0x08` | unknown | 10 | 0 | 1 | 238 | 9 | 29 | 2 |
| 4 | `prop1 & 0x10` | unknown | 0 | 0 | 0 | 2 523 | 78 | 22 | 2 |
| 5 | `prop1 & 0x20` | **never set** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 6 | `prop1 & 0x40` | **floor-layer tag** | 666 155 | 30 447 | 0 | 0 | 0 | 0 | 0 |
| 7 | `prop1 & 0x80` | **tile is in the file's own tileset list** — see below | 626 852 | 30 447 | 15 146 | 160 240 | 7 922 | 4 043 | 1 232 |
| 8–13 | `prop2` | **`sub_index`** | — | — | — | — | — | — | — |
| 14–15 | `prop2 & 0xC0` | **never set** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 16 | `prop3 & 0x01` | unknown | 239 | 0 | 1 | 20 | 2 | 0 | 0 |
| 17 | `prop3 & 0x02` | **Unwalkable** | 152 115 | 185 | 150 | 9 062 | 26 | 65 | 20 |
| 18–19 | `prop3 & 0x0C` | **never set** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 20–25 | `prop3>>4`, `prop4 & 0x03` | **`main_index`** | — | — | — | — | — | — | — |
| 26 | `prop4 & 0x04` | unknown — **not part of `main_index`** | 1 | 0 | 1 | 44 | 37 | 0 | 0 |
| 27 | `prop4 & 0x08` | **IsShadow** | 0 | 0 | **15 146** | 0 | 0 | 0 | 0 |
| 28 | `prop4 & 0x10` | unknown | 30 059 | 248 | 504 | 2 238 | 8 | 1 | 0 |
| 29 | `prop4 & 0x20` | unknown | 0 | 0 | 1 | 35 | 0 | 0 | 0 |
| 30 | `prop4 & 0x40` | **never set** | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 31 | `prop4 & 0x80` | **Hidden** | 4 733 | 1 957 | 7 | 2 371 | 86 | 145 | 142 |

Six things fall out of that table that no source states in words.

**An empty cell is a zero dword — exactly.** Across all 2 761 752 cells whose
`prop1` byte is zero, not one has any other bit set **[D]**. "`prop1 == 0` means
no tile here", which is how both Siramy's parser and this repository's decide
occupancy, is not a convention: it is a property of the shipped data.

**`prop1` is a layer-type tag, and the game never reads it.** Bit 0 is set on
every wall cell and no other; bit 1 and bit 6 are set on every floor cell and no
other. The low byte takes only eleven non-zero values in all of vanilla —
`0x81`, `0x85`, `0x89`, `0x91`, `0x99` on walls; `0x42`, `0xC2`, `0xC6`, `0xCA`
on floors; `0x80`, `0x88` on shadows. This is exactly what Siramy's bitfield
dialog labels *"Layers priority, Type of layers, and unknown"* **[S]**, and the
census pins down the "type of layers" half of it. None of the three cell
decoders in D2Common reads bits 0–7 **[G]**.

**Bit 7 marks a tile the file's own tileset list cannot supply.** It is set on
every wall and shadow cell, and on all but 39 303 floor cells — the ones whose
`prop1` reads `0x42` rather than `0xC2`. **Every single one of those 39 303
cells names a tile that is not in any tileset the DS1 itself lists** **[D]**.
The implication runs one way only — a further 3 022 floor cells and 3 637 wall
cells *with* bit 7 set are also unresolvable against their own file's list — but
in that direction it is exact, 39 303 out of 39 303. Given that the game ignores
the list entirely (below), the most economical reading is that bit 7 was the
authoring tool's own bookkeeping, and that these are edits made after the list
was last regenerated *(unverified: no code was found that reads the bit)*.

**Bit 27 really is `IsShadow`.** It is set on **all 15 146** shadow cells and on
**none** of the 870 039 wall and floor cells **[D]**. Siramy's improved 2004
dialog added a column labelled `IsShadow` between *Hidden* and *Main-index*
**[S]**; this is it, and the correspondence is perfect.

**Bit 26 is not part of `main_index`.** It sits immediately above the six
`main_index` bits and is set on 83 cells. The disassembly's `AND ESI,0x3f`
**[G]** settles the width at six bits, and the data agrees: reading `main_index`
as seven bits instead resolves 83 fewer cells against the tile catalogue and
none more **[D]**.

**Six of the thirty-two bits are never used at all** — 5, 14, 15, 18, 19 and 30
**[D]**. `sub_index` is nominally a whole byte but never exceeds 63.

### Following one cell

In the worked example, the first non-empty floor cell is the top-left corner,
file offset `0x93CF`:

```
c2 00 50 00      →  dword 0x005000C2
   prop1 = 0xC2  bits 1,6,7 → a floor cell
   prop2 = 0x00  sub_index  = 0
   prop3 = 0x50  main_index = (0x005000C2 >> 20) & 0x3F = 5
   prop4 = 0x00  Hidden 0, IsShadow 0, Unwalkable 0
```

So: *orientation 0, main 5, sub 0* — a floor tile, which the file's own list
suggests comes from `act1\town\floor.dt1`, and which the game will find in
whatever `LvlTypes.txt` loaded for the level.

---

## Orientation: a byte in a four-byte slot

Wall layers alone carry a second, parallel stream. Every entry is a dword of
which only the low byte is meaningful: across all **1 486 161** orientation
records in vanilla 1.13c, the upper three bytes are zero **[D]**. Floors and
shadows have no orientation stream at all — the loader supplies the orientation
implicitly, 0 for floors and 13 for shadows **[R] [D]**, and the floor decoder at
`6fd5bdf6` has no orientation term in it **[G]**.

The stream is not a parallel array of decoration. It is load-bearing, and it is
perfectly correlated with occupancy: orientation is **non-zero on exactly the
173 437 occupied wall cells and zero on exactly the 1 312 724 empty ones**, with
no exceptions in 1 486 161 records **[D]**.

| orientation | 1 | 2 | 3 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| cells | 54 416 | 49 946 | 7 921 | 9 507 | 9 653 | 5 434 | 1 373 | 1 299 | 654 | 292 | 9 027 | 8 | 15 164 | 3 599 | 1 998 | 2 421 | 632 | 93 |

Orientation **0 and 4 never appear on an occupied wall cell** **[D]**. Thirteen
appears eight times, which is odd given 13 is the shadow orientation; those eight
cells are noted as unexplained in the companion report.

> **Version note (DS1 file versions ≤ 6):** orientation values are not stored
> directly. Each is used as an index into a 25-entry table and replaced by the
> result:
>
> ```
> 6fd5b6c0  MOV EBP,dword ptr [EAX]
> 6fd5b6c2  MOV EBP,dword ptr [EBP*0x4 + 0x6fddbcc0]
> 6fd5b6c9  MOV dword ptr [EAX],EBP
> ```
>
> The table at `0x6fddbcc0` reads
> `0, 1, 2, 1, 2, 3, 3, 5, 5, 6, 6, 7, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20`
> **[G]** — **byte for byte the `dir_lookup[25]` in
> [`src/core/ds1.c:362-366`](../../src/core/ds1.c)** **[S] [R]**. Since the
> binary owes Siramy nothing, that is a genuine independent confirmation of his
> table. There is no bounds check on the index, so an out-of-range orientation
> in such a file reads past the end of the table. Exactly one vanilla file is
> affected (`court1b.ds1`, version 3) **[D]**. In 1.09d the remap is a call to
> `6fd791d0` rather than an inline table (`6fd76697`) **[G]**; that helper was
> not decompiled.

### Orientation 10 and 11: the tiles that are not tiles

Two orientation values mean *this cell is not scenery*. The entire warp and
special-tile scan in `ProcessPresetRoomObjectsAndTiles` runs behind a pair of
comparisons:

```
6fd5c33d  CMP ECX,0xb          ; orientation == 11 ?
6fd5c346  CMP ECX,0xa          ; orientation == 10 ?
```

and inside it the cell's `main_index` selects what kind of marker it is **[G]**:

| `main_index` | Instructions | Meaning |
|---|---|---|
| 0 – 7 | `6fd5c35b CMP ESI,0x7` | collision / sub-tile marking, via an indirect call at `6fd5c3bd` |
| 8 – 29 | `6fd5c3d3 CMP ESI,0x8`, `6fd5c3d8 CMP ESI,0x1d` | appended to the room's **special-tile** list (16-byte records at `room+0x50`, `6fd5c403 SHL EAX,0x4`) |
| 30 – 33 | `6fd5c45f CMP ESI,0x1e`, `6fd5c468 CMP ESI,0x21` | **a warp** — 12-byte records into `level+0x2C`, count at `level+0x1D8` |

The warp's *direction* comes from a jump table at `0x6fd5c67c` **[G]**:

| `main_index` | direction written |
|---|---|
| 30 | `sub_index` (`6fd5c4ba`) |
| 31 | `sub_index + 5` (`6fd5c4c4`) |
| 32 | `10` (`6fd5c4df`) |
| 33 | `11` (`6fd5c4f2`) |
| other | `12` (`6fd5c505`) |

Vanilla contains **96 warp cells across 45 files**, 273 special-tile cells and
577 collision cells **[D]** — a tiny fraction of 173 437 wall cells, which is
why they are easy to miss and catastrophic to delete.

> **A correction to the mental model, not to Siramy.** There is no "special"
> layer on disk. A special tile is an ordinary wall-layer cell whose orientation
> is 10 or 11; *Special* is an editing and rendering category, not a stored one.

**In the worked example**, three cells on wall layer 1 carry orientation 10, and
all three are warps:

| offset | bytes | cell | `main` | `sub` | Hidden | direction |
|---|---|---|---|---|---|---|
| `0x00DCB` | `81 00 e0 81` | (30, 13) | 30 | 0 | yes | `sub` = 0 |
| `0x00DD3` | `81 00 00 82` | (32, 13) | 32 | 0 | yes | 10 |
| `0x01417` | `81 00 10 82` | (34, 20) | 33 | 0 | yes | 11 |

All three have bit 31 set. That is the whole recipe Siramy's warp tutorial
describes **[S]**: a special tile, on a wall layer, at orientation 10, marked
Hidden so the green placeholder graphic never appears in the game.

---

## Objects

After the last cell layer comes a count dword and then that many object records.
The block exists from version 2. Records are four dwords, or five from version 6:

| Offset | Size | Field | Gate |
|---|---|---|---|
| `+0x00` | 4 | **type** | always |
| `+0x04` | 4 | **id** | always |
| `+0x08` | 4 | **x**, in sub-tiles | always |
| `+0x0C` | 4 | **y**, in sub-tiles | always |
| `+0x10` | 4 | **DS1 flags** | v ≥ 6 |

**[G]** (`6fd5b720`, `6fd5b727`, `6fd5b96f`, `6fd5b971`, `6fd5b989`); record size
16 bytes below version 6 and 20 bytes at 6 and above. Coordinates are in
sub-tiles — five per tile in each direction — so a preset 57 tiles wide spans
x = 0…284.

The parser turns each record into a 32-byte node (`6fd5b9aa MOV EDX,0x20`) on a
linked list whose head is `preset+0x54`; this is the structure community headers
call `PresetUnit` **[G]**:

| Node offset | Contents |
|---|---|
| `+0x00` | class derived from type: 1 for type 1, 0 for type 2, 3 for type 4 |
| `+0x04` | resolved id |
| `+0x08` | x |
| `+0x0C` | next pointer |
| `+0x10` | NPC path block, filled in later |
| `+0x14` | the DS1 type |
| `+0x18` | y |
| `+0x1C` | the DS1 flags |

### What `type` and `id` mean

Only two types occur in retail data: **15 103 of type 2 and 2 676 of type 1**,
out of 17 779 objects in 2 276 files **[D]**. The parser recognises a third.

**Type 1 — a monster or NPC.** From version 5 the id is a **zero-based index
into the `monpreset.txt` rows for this file's act** **[G] [D]**. The in-memory
monpreset record is four bytes — act, kind, and a 16-bit value
(`6fd5b88b MOV CL,byte ptr [EAX + EDI*0x4 + 0x1]`,
`6fd5b88f MOVZX EDI,word ptr [EAX + EDI*0x4 + 0x2]`) — and the kind byte selects
one of three resolutions, or −1 (`6fd5b8a1 OR EDI,0xffffffff`) for an unknown
one. Act-specific fixups follow for acts 2 and 4 (`6fd5b8fe`, `6fd5b922`,
`6fd5b957`, `6fd5b962`), which rewrite the node's type to 2.

**Type 2 — an object.** There is no `objpreset.txt` in the archives; the mapping
is a table compiled into the DLL. From version 6:

```
6fd5b80f  CMP EDI,0x96                             ; id vs 150
6fd5b815  JGE 0x6fd5b82f
6fd5b81b  IMUL EDX,EDX,0x96                        ; act * 150
6fd5b821  ADD EDX,EDI
6fd5b823  MOV EDI,dword ptr [EDX*0x4 + 0x6fdee2c8] ; the table
6fd5b82f  SUB EDI,0x96                             ; id >= 150: direct index
```

so `objectsTxtId = id < 150 ? table[act * 150 + id] : id - 150`. The table at
`0x6fdee2c8` is **5 acts × 150 int32**, occupying `0x6fdee2c8`–`0x6fdeee80`
exactly, with no slack before the next object **[G]**. The `act` index is *not*
bounds-checked here; only the earlier clamp to 4 keeps it in range.

> **Version note (DS1 file versions ≤ 5):** neither remap runs. A type-1 id is
> used as-is, and a type-2 id is used as a direct `Objects.txt` index, rejected
> if it reaches **573** (`6fd5b83a CMP EDI,0x23d`) **[G]** — which is exactly
> the vanilla `Objects.txt` record count **[D]**.

**Type 4 — an item.** The parser handles it: the id indexes a table of
three-character item codes at `0x6fdeee80`, which are upper-cased, padded to
four bytes with a space, and binary-searched (`6fd5b75a`, `6fd5b786`,
`6fd5b7ea`) **[G]**. **No vanilla `.ds1` contains one** — zero records of type 4
in 17 779 **[D]**.

The flags dword is almost always zero: 17 756 of 17 779 records are 0 and 23 are
1 **[D]**. Its meaning is not established here.

### Following one object

The worked example's first object record reads `type = 2, id = 110,
x = 164, y = 81, flags = 0`. Resolving it: the file's act field is 0, so
`table[0 * 150 + 110]` = **385**, and `Objects.txt` Id 385 is
`Dummy / cain start position` **[G] [D]**. Sub-tile (164, 81) is tile
(32, 16), sub-cell (4, 1).

The same file's type-1 objects resolve through the Act-1 `monpreset` rows,
which read in order `gheed, cain1, akara, chicken, rogue1, kashya, cow,
warriv1, charsi, …` **[D]**. Its ids 0, 2, 3, 4, 5, 7, 8 and 13 are therefore
Gheed, Akara, a chicken, a rogue, Kashya, Warriv, Charsi and a cow — which is
the Rogue Encampment, exactly.

---

## Groups

A version-12-or-later file whose `tagType` is 1 or 2 carries a group block:
rectangles laid over the tag layer. It begins with a count, preceded from
version 18 by one skipped dword.

| Offset | Size | Field | Gate |
|---|---|---|---|
| `+0x00` | 4 | x | always |
| `+0x04` | 4 | y | always |
| `+0x08` | 4 | width | always |
| `+0x0C` | 4 | height | always |
| `+0x10` | 4 | unknown | v ≥ 13 |

**[G]** (`6fd5ba76`, `6fd5ba82`, `6fd5ba92`, `6fd5baa1`, `6fd5baac`). The file
record is 16 bytes at version 12 and 20 bytes from 13; the in-memory record is
24 (`6fd5bab7 ADD EAX,0x18`), and its `+0x10` slot is never written — it holds
whatever the allocator left. Vanilla declares 907 groups across 151 files, of
which 893 are actually present in the files — the missing 14 are all in
`ACT1\OUTDOORS\trees.ds1`, which stops mid-block **[D]**.

The data says something the sources do not: **tag-layer values look like
one-based group indices.** In **143 of the 151** vanilla files that have both a
tag layer and groups, the largest value in the tag layer is *exactly* the group
count — which is what you would see if 0 meant "no group" and *n* meant group
*n − 1*. Seven files exceed the count and one falls short **[D]**. This is
consistent rather than proven: nothing in the loader was found that reads a tag
cell and indexes the group array *(unverified: the tag layer's consumer was not
located in D2Common)*.

The group record's fifth dword, present from version 13, is **not** always zero:
101 of the 893 readable vanilla group records carry a non-zero value **[D]**.
Its meaning is unknown.

The worked example has `tagType = 0`, so it has neither a tag layer nor groups.

---

## NPC paths

The last block, from version 14, is what makes town NPCs walk. It is a count of
path blocks, then per block three dwords — the number of points, and the owning
object's sub-tile x and y — then the points themselves.

There is a trap here that no source states as a rule: **paths are keyed by
coordinate, not by object index.** The loader walks the object list comparing
the block's x and y against each node's `+0x08` and `+0x18`
(`6fd5baf4`, `6fd5baf9`) **[G]**. Two objects on the same sub-tile are therefore
indistinguishable — Siramy's parser detects that case and discards the paths
outright ([`src/core/ds1.c:1091-1152`](../../src/core/ds1.c)) **[S] [R]**.

A path point is two dwords, or three from version 15:

| Offset | Size | Field | Gate |
|---|---|---|---|
| `+0x00` | 4 | x | always |
| `+0x04` | 4 | y | always |
| `+0x08` | 4 | **action** | v ≥ 15 |

**[G]** (`6fd5bb08 LEA ESI,[ESI + EBP*0x8]`, `6fd5bb0d LEA ESI,[ESI + EBP*0x4]`
for the skip path; `6fd5bb6e`, `6fd5bb84`, `6fd5bb92` for the stored one).
Note that in memory the order is different — action first, at `+0x00`, then x
and y (`6fd5bba7 ADD EAX,0xc`). Below version 15 the action is defaulted to 1
(`6fd5bba0`).

Vanilla is sparing with this: **17 files, 136 path blocks, 511 points**, with
`action` taking five values — 1 (328×), 2 (105×), 3 (43×), 4 (34×) and 5 (once)
**[D]**. So the "hardcoded Action" Siramy describes **[S]** really is a small
enumeration.

**In the worked example**, five NPCs walk, and each block's anchor coordinate
matches an object exactly:

| block | points | anchor | object | who |
|---|---|---|---|---|
| 0 | 3 | (114, 57) | #21, type 1 id 8 | Charsi |
| 1 | 4 | (171, 66) | #1, type 1 id 5 | Kashya |
| 2 | 4 | (146, 75) | #2, type 1 id 7 | Warriv |
| 3 | 4 | (212, 46) | #14, type 1 id 2 | Akara |
| 4 | 4 | (116, 118) | #42, type 1 id 0 | Gheed |

Charsi's route is `(115, 51) action 4`, `(118, 60) action 2`, `(114, 59) action 2`.

---

## Reading a DS1 without falling over

The whole worked example now accounts for itself, byte for byte:

| Region | Bytes | Running total |
|---|---|---|
| header, including nine filenames | 447 | 447 |
| six cell layers, 57 × 41 × 4 each | 56 088 | 56 535 |
| object count + 43 records × 20 | 864 | 57 399 |
| NPC count + 5 blocks × 12 + 19 points × 12 | 292 | **57 691** |

which is the file's size. **2 234 of 2 276 vanilla files close like that** —
the layout consumes exactly the bytes present, with nothing left over and
nothing missing **[D]**. Three groups of exceptions are worth knowing, because
a strict reader will trip over all of them.

**Forty-two files end four bytes late.** Every one is version 12 or 13, and in
every one the extra dword is zero **[D]**. The discriminator is the NPC-path
block, not the group block: two of them (`ACT2\Outdoors\palm.ds1` and
`scrub.ds1`, both version 13 with `tagType = 1`) consume their group block in
full and *still* leave a spare zero dword, and the version-12/13 files with
`tagType = 0` leave four bytes and not eight. The authoring tool wrote an NPC
path count unconditionally; a loader seeing a version below 14 stops before it
(`6fd5bac2 CMP EBX,0xe / JL`) **[G]**. The bytes are simply never read.

**One file ends 48 bytes late.** `ACT1\OUTDOORS\swamp2.ds1` (version 13,
`tagType = 1`, 11 declared groups, and no wall layer at all) has a tail that
decodes as two further well-formed group records — `(1, 5, 3, 3, 0)` and
`(15, 0, 0, 0, 0)` — followed by two zero dwords **[D]**. Its group count
understates the records present. Nothing reads them.

**Two files declare more data than they contain, and the game reads past their
end.** `ACT1\OUTDOORS\trees.ds1` runs 12 bytes past EOF and
`expansion\Siege\xtransition.ds1` 4 bytes past **[G] [D]**. What saves them is
that `RESOURCE_AllocateAndOpen` allocates `filesize + 800` bytes for every file
it reads (`6fd59900`, tagged `..\Source\D2Hell\SRC\Archive.cpp:0xCF`) **[G]**.
That slack is the only reason two shipped presets do not read unmapped memory —
and it is the reason a reimplementation that bounds-checks strictly will reject
two files the real game loads without complaint. Siramy hit this in 2002 and
left a comment about `trees.ds1` in the parser
([`src/core/ds1.c:1013-1018`](../../src/core/ds1.c)) **[S] [R]**.

Two further edges the loader guards and a naive reader will not: the wall and
floor counts may be zero, and the special-tile scan never visits the last row or
column of a preset — its bounds are the *stored* `width − 1` and `height − 1`,
not the real ones (`6fd5c517`, `6fd5c527`) **[G]**.

---

## What is not settled

- **The purpose of `prop1`.** Its layer-type bits are certain; bits 2, 3 and 4
  appear on 601, 289 and 2 625 cells with no discernible pattern, and no
  D2Common decoder reads the byte at all. Whether some other module does is
  *(unverified: only the three DRLG decoders were audited)*.
- **Cell bits 16, 26, 28 and 29.** Present in real data, read by nothing found.
- **The two dwords in version 9–13 files.** Skipped by `ADD ESI,0x8`
  (`6fd5b60d`) **[G]**, and every one of the 43 vanilla files in that window
  stores the same pair, `(5000, 5000)` **[D]**. What they meant to the authoring
  tool is unknown.
- **The dword skipped before the group count at version 18** (`6fd5ba35`). Zero
  in all 117 vanilla files that have it **[D]**.
- **The group block's fifth dword** — non-zero in 101 of 893 vanilla records —
  and which code, if any, reads a tag-layer cell and indexes the group array.
- **The object flags dword.** Set to 1 on 23 of 17 779 records.
- **Eight wall cells at orientation 13**, the shadow orientation.
- **The runtime cell→tile resolution path.** The *key* is proven — D2Common
  builds `(orientation, main_index, sub_index)` in three places, and D2CMP
  hashes exactly that triple into 128 buckets in `ComputeTileHashIndex`
  (`6fe22e10`) and `InsertTileIntoLookupTable` (`6fe22f20`) **[G]**. The handoff
  from D2Common to the renderer was not traced; D2Client contains no
  `(cell >> 20) & 0x3F` decode anywhere, so it must receive decoded values.
- **340 cells (0.04 %) that resolve against no `.dt1` in the archives** **[D]**.
  They cluster in `expansion\Siege\` border and sample presets.

---

## Version differences

The DS1 file version is the axis that matters; the game's patch level barely
moves. Both tables follow.

### By DS1 file version

Version 18 is the modern case and the one to read the middle column against.
"Below that" states what is true of every earlier version, and only the versions
in the second column ever behaved differently.

| What | On version 18 | Below that |
|---|---|---|
| object block | present | **absent below v2** |
| tileset filename list | present (and ignored) | **absent below v3** |
| wall-layer count | read from the file | **forced to 1 below v4**, with a five-block fixed layer order (wall, floor, orientation, tag, shadow) instead of the interleaved one |
| type-1 object id | index into `monpreset` for the act | **used raw below v5** |
| type-4 (item) objects | recognised | **not recognised below v5** |
| object record size | 20 bytes | **16 bytes below v6** — no flags dword |
| type-2 object id | `table[act*150 + id]`, or `id − 150` at 150 and above | **below v6** a direct `Objects.txt` index, rejected at 573 and above |
| orientation values | stored as-is | **below v7** each value is replaced by `dir_lookup[value]`, a 25-entry table |
| act dword | present, 0-based, clamped to 4 | **absent below v8** — Act 1 assumed |
| two unknown dwords | absent | **present on v9–v13 only**, always `(5000, 5000)`, skipped |
| `tagType` dword | present | **absent below v10** — treated as 0, so no tag layer and no group block |
| group block | present when `tagType` is 1 or 2 | **absent below v12** |
| group record size | 20 bytes | **16 bytes on v12** — no fifth dword |
| NPC path block | present | **absent below v14** |
| path point size | 12 bytes, with `action` | **8 bytes below v15**; `action` defaults to 1 |
| dword before the group count | present | **absent below v18** |
| floor-layer count | read from the file | **forced to 1 below v16** |

Note the two out-of-order rows: the floor count arrives at version 16, *after*
the group and path blocks at 12 and 14, and the extra pre-group dword arrives
last of all at 18.

### By game version

| What | 1.13c | 1.09d | 1.00a – 1.12a, 1.13d – 1.14d |
|---|---|---|---|
| DS1 versions accepted | all; no maximum | all; no maximum | *(unverified: only 1.09d and 1.13c disassembled)* |
| version-gate constants | 15 gates, listed above | identical, same order | *(unverified)* |
| act clamp | `≤ 4` @ `6fd5b5a8` | `≤ 4` @ `6fd76583` | *(unverified)* |
| tileset list | skipped @ `6fd5b5f2` | skipped @ `6fd765b8` | *(unverified)* |
| layer struct offsets | `+0x1C/+0x2C/+0x3C/+0x44/+0x48` | identical | *(unverified)* |
| orientation remap (v < 7) | inline table @ `0x6fddbcc0` | call to `6fd791d0` | *(unverified)* |
| type-2 object table | `0x6fdee2c8`, 5 × 150 | `0x6fdd35c0`, 5 × 150 | *(unverified)* |
| **type-2 id ≥ 150** | **accepted**, `id − 150` @ `6fd5b82f` | **rejected**, −1 @ `6fd76800` | *(unverified)* |
| `PresetUnit` x/y offsets | `+0x08` / `+0x18` | `+0x0C` / `+0x10` | *(unverified)* |
| `.ds1` files shipped | 2 374 entries / 2 276 distinct | 2 384 entries (LoD) | see the census table above |

The single behavioural difference between the two disassembled versions is the
type-2 object id: a preset naming object id 150 or above loads on 1.13c and is
dropped on 1.09d **[G]**. No vanilla file exercises it.

---

## Appendix: the layout in one page

Read top to bottom. Every field is a little-endian `int32` unless marked.

```
int32   version
int32   width  - 1
int32   height - 1
int32   act                                   if version >= 8   (0-based, clamped to 4)
int32   tagType                               if version >= 10
int32   fileCount                             if version >= 3
char[]  fileCount NUL-terminated strings      if version >= 3   (never read by the game)
int32   unknown                               if 9 <= version <= 13
int32   unknown                               if 9 <= version <= 13
int32   wallLayerCount                        if version >= 4   (else 1)
int32   floorLayerCount                       if version >= 16  (else 1)

  -- cell layers, each width*height int32, row-major, x fastest --
  version >= 4:  wall 1, orientation 1, ... wall N, orientation N,
                 floor 1 ... floor M, shadow, [tag if 1 <= tagType <= 2]
  version <  4:  wall 1, floor 1, orientation 1, tag, shadow

int32   objectCount                           if version >= 2
  per object:
    int32 type, int32 id, int32 x, int32 y
    int32 flags                               if version >= 6

if version >= 12 and 1 <= tagType <= 2:
  int32 unknown                               if version >= 18
  int32 groupCount
  per group:
    int32 x, int32 y, int32 width, int32 height
    int32 unknown                             if version >= 13

int32   npcCount                              if version >= 14
  per npc:
    int32 pointCount, int32 anchorX, int32 anchorY
    per point:
      int32 x, int32 y
      int32 action                            if version >= 15
```

### The cell dword

| Bits | Field |
|---|---|
| 0–7 | layer-type tag (`prop1`) — `0x01` wall, `0x02`+`0x40` floor; **zero means empty** |
| 8–15 | `sub_index` (`prop2`) |
| 16 | unknown |
| 17 | **Unwalkable** |
| 18–19 | unused |
| 20–25 | `main_index` |
| 26 | unknown |
| 27 | **IsShadow** |
| 28–29 | unknown |
| 30 | unused |
| 31 | **Hidden** |

### The tile key

| Layer | orientation | `main_index` | `sub_index` |
|---|---|---|---|
| wall *n* | the paired orientation stream, low byte | bits 20–25 | bits 8–15 |
| floor *n* | implicitly 0 | bits 20–25 | bits 8–15 |
| shadow | implicitly 13 | bits 20–25 | bits 8–15 |

### Corpus at a glance

| | |
|---|---|
| vanilla 1.13c `.ds1` files (distinct) | 2 276 |
| total bytes | 21 664 789 |
| cells across all layers | 5 174 713 |
| non-empty cells | 885 185 |
| objects | 17 779 in 1 643 files |
| groups | 907 declared / 893 present, in 151 files |
| NPC path blocks / points | 136 / 511, in 17 files |
| tileset-list entries | 10 500 |

---

*Companion verification report:
[ds1-map-format.verification.md](ds1-map-format.verification.md).*
