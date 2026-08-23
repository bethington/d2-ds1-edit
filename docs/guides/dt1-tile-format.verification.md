# Verification report — `dt1-tile-format.md`

Companion audit for [dt1-tile-format.md](dt1-tile-format.md). Records the ground
truth used, every claim checked, every correction applied, and everything that
could not be settled.

**Date:** 2026-08-21
**Nature of the work:** *reconstruction*, not conversion. The source page was
never archived. See "Origin" below.

---

## Origin

| | |
|---|---|
| **Original author** | **Paul Siramy** — `paul.siramy.free.fr`, `paul.siramy@free.fr` / `siramy_paul@yahoo.com` |
| **Original page** | **Lost.** A DT1 tile-format reference page existed on `paul.siramy.free.fr`; its image directory `_divers/dt1_doc/dt1doc_data/` survives in the preservation tree with **no HTML page anywhere in the archive referencing it**. The Wayback index showed the material existed; the pages did not come back. |
| **What survived** | 24 GIFs in `_divers/dt1_doc/dt1doc_data/`; four C programs in `_divers/dt1/` (`dt1make.c`, `dt1extr.c`, `dt1info.c`, `dt1debug.c`) |
| **Archive dates** | The surrounding Siramy material spans 2001–2011; the DT1 tools identify themselves as `v 0.97 beta` |
| **Modernized copy** | `docs/guides/dt1-tile-format.md` — a **new file**. No file under `docs/preservation/` was modified, read-only throughout. |

### Rights — the position taken

No page in the Siramy archive carries a license, a copyright notice, or a
republication grant. This is personal fan-site documentation.

- The chapter reproduces **nine of Siramy's images by relative path** into the
  preservation tree (`floor_flags`, `or_1_to_9`, `system1/2/3`, `floor_grid`,
  `random_tiles`, plus references to `or_1`–`or_9`, `box_*`, `fence_grid*`).
- The chapter **quotes short excerpts of his C source** (`dt1make.c`,
  `dt1extr.c`) as evidence.
- `NOTICE` at the repository root already records this project's standing
  attribution position for code derived from `win_ds1edit`, and states that
  Siramy's original work "remains his". That covers the *code*; it does not
  cover republication of his images or prose in a book.

**The project proceeds on an explicit fair-use judgment** for the images and
prose this chapter reproduces, recorded with its reasoning in
[BOOK-STATUS.md](../BOOK-STATUS.md). If Paul Siramy objects, the terms
change.

### Attribution chain

The chapter credits Siramy in the Origin block, names each of his four sources in
the appendix with line counts and roles, and attributes every geometric claim
about orientation to his figures. Two of his labels are corrected in the chapter
(`frame` → `rarity`, and the `length` field width); both corrections name him and
state what the evidence shows, per the skill's rule that "corrections never erase
authorship".

---

## Ground truth used

| Source | Detail | Availability |
|---|---|---|
| **Ghidra — 1.13c binaries** | `/Vanilla/1.13c/D2CMP.dll` (image base `6fe10000`), `/Vanilla/1.13c/D2Client.dll` (`6fab0000`), `/Vanilla/1.13c/D2Common.dll` (`6fd50000`) | Available; all three used |
| **Siramy's C sources** | `docs/preservation/siramy/paul.siramy.free.fr/_divers/dt1/{dt1make,dt1extr,dt1info,dt1debug}.c` — 2,913 lines total | `dt1make.c` (990), `dt1extr.c` (726) and `dt1info.c` (301) read in full. **`dt1debug.c` (896) was NOT read** — an interactive inspector, judged to add nothing the other three do not state. If a claim here later needs a fourth witness, that is where to look |
| **This repo's parser** | `src/core/dt1.c`, `src/core/dt1_draw.c`, `src/structs.h`, `src/misc.c`, `src/core/ds1.c` | Read in full |
| **Real data** | Every `.dt1` under `assets/`: **360 files**, 265 Blizzard game tiles, 94 Project Diablo 2 mod tiles, 1 editor tile | All 360 opened; 354 parsed |
| **Live fleet member** | Not used | Not applicable — DT1 is a static file format; the static image and the on-disk data are stronger and cheaper here |
| **`/mpq/*` reach-in** | Not used | The 360 files on disk are already extracted MPQ contents; reading them directly is equivalent and complete |

### Ground-truth staging notes

- The Ghidra work was delegated to a subagent with an explicit instruction to
  pass `program=` on every call and to confirm the exact program paths via
  `list_open_programs` first. All three modules were open at the expected image
  bases.
- **The circularity trap does not apply in the usual way here**, and it is worth
  stating why. This repository's `src/core/dt1.c` is *derived from Siramy's
  `dt1misc.c`*. It is therefore **not independent evidence of Siramy's claims** —
  agreement between `dt1make.c` and `src/core/dt1.c` is largely agreement between
  one author and his own later code. This was the survey's framing ("two
  independent implementations agreeing is strong evidence") and it is **partly
  wrong**; the chapter says so and does not lean on that agreement.
  The genuinely independent evidence is the 354 real files and the 1.13c
  binaries, and every load-bearing claim rests on at least one of those.

### Verification scale

| Measure | Count |
|---|---|
| `.dt1` files opened | 360 |
| Files parsed as DT1 7.6 | 354 |
| Files rejected (version 4.1) | 6 |
| Block (tile) headers walked byte by byte | 26,005 |
| Sub-tile headers walked byte by byte | 564,457 |
| Sub-tile pixel streams decoded to verify framing | 564,457 |
| Sub-tile flag bytes examined | 650,125 |

Type-F (format/layout) claims were **not sampled**. Every field of every header
in every file was read.

---

## Claim tally

| Type | Checked | Confirmed | Corrected | Unverified |
|---|---|---|---|---|
| **F** — byte offsets, widths, magic numbers, strides | 39 | 34 | 5 | 0 |
| **B** — behavioral (what the loader/renderer does) | 11 | 11 | 0 | 0 |
| **D** — data/asset (what real files contain) | 14 | 14 | 0 | 0 |
| **C** — contextual | 4 | 1 | 1 | 2 |
| **Total** | **68** | **60** | **6** | **2** |

Plus **7 open questions** recorded in the chapter's "What is not settled"
section, which are not claims but absences.

---

## The headline resolution: 276 vs 272

The survey (`docs/preservation/siramy/INVENTORY.md` §"Non-HTML load-bearing
sources" and item 4 of the priority list) recorded these as a possible
disagreement: `dt1make.c` declares `dt1_head_size 276`, while `src/core/dt1.c`
reads from `+272`.

**Verdict: not a disagreement. Same fact, two questions.**

| Evidence | Finding |
|---|---|
| **[S]** `dt1make.c` `write_dt1_header()` | Writes 4 + 4 + 260 + 4 + 4 = **276** bytes, and stores `head_start = dt1_head_size = 276` into the last of them. Comment: `// start of block header, always 0x114`. `0x114` = 276 |
| **[S]** `dt1extr.c` `read_dt1_header()` | `fread(&x1, 1, 4, in); if (x1 != 0x114) is_dt1 = FALSE;` — hard-asserts 276 |
| **[R]** `src/core/dt1.c:15` | `#define DT1_FIXED_HEADER_SIZE 276` — the repo declares **the same 276**, three lines above the code that reads offset 272 |
| **[R]** `src/core/dt1.c:573-574` | `block_num = *(int32*)(ptr + 268); bh_start = *(int32*)(ptr + 272);` — 272 is the *offset of the field*, 276 is its *value* |
| **[D]** 354 valid files | The int32 at file offset 272 holds **276** in every one. Zero exceptions |
| **[G]** `OpenResourceAndCalculateSize` @ `6fe1bad0` | `SUB ESP,0x118` / `MOV EDI,0x114` (read 276) / `MOV EAX,[ESP+0x114]` (dword at 0x10C = 268) / `LEA EAX,[EAX+EAX*2]; SHL EAX,5` (×0x60 = 96) / `ADD EAX,0x114` (+276). Header size, count offset, block stride, array offset — all four in eight instructions |
| **[G]** `SerializeCelDataToBuffer` @ `6fe25e20` | `puVar6 = puVar2 + 0x45` (0x45 dwords = 0x114) written **unconditionally** as the block-header offset |

The number **272** never appears in `dt1make.c` because a writer never needs it —
it seeks by field order. The number **276** appears in both. Both implementations
and the game agree on all three strides: **276 / 96 / 20**. The survey's framing
was a category error and is corrected in the chapter.

---

## Corrections applied

Six claims in the surviving sources (Siramy's, this repo's, or the survey's) were
found wrong and corrected in the chapter.

### C1 — Offset 4 is a flags dword, not a version minor

| | |
|---|---|
| **Before** | Every reader treats the file as "version 7.6" and rejects anything else. `dt1extr.c`: `if ((x1 != 7) \|\| (x2 != 6)) is_dt1 = FALSE;`. `src/core/dt1.c` `dt1_headers_are_valid()`: `if (glb_dt1[i].x1 != 7 \|\| glb_dt1[i].x2 != 6) return FALSE;` |
| **After** | Offset 0 is a magic value checked by the game (`7`). Offset 4 is a **flags dword**. The value 6 is what Blizzard's serializer emits, not a version requirement |
| **Evidence** | **[G]** `SerializeCelDataToBuffer` `6fe25e20`: `puVar2[1] \|= 6`. `BuildTileCelDescriptor` `6fe26750` sets bit 0 conditionally. `FixupCelDataLayout` `6fe25d10` tests `(*(byte*)(p+1) & 3)` then rewrites `p[1] = p[1] & ~2 \| 4`. Bit 1 = flat/serialized, bit 2 = pointers fixed up. **No code anywhere compares offset 4 against 6.** The `7` at offset 0 *is* checked twice — written by `BuildTileCelDescriptor`, and `FixupCelDataLayout` aborts at `Tilecmp.cpp:840` if absent |
| **Impact** | None for readers (all 354 files carry 6, **[D]**). Real for anyone writing a generator or a future-format reader |
| **Action** | Documented in the chapter; **no code change made** — the existing check is safe and this task owns only the two doc files |

### C2 — Block `+32` is `rarity`, not `frame`

| | |
|---|---|
| **Before** | `dt1make.c`, `dt1extr.c` and `dt1info.c` all name it `frame` and write it to the INI as `frame = %08lX` |
| **After** | It is a **selection weight**. `src/core/dt1.c` and `src/structs.h` already call it `rarity` — the repository was right and Siramy's label was wrong |
| **Evidence** | **[G]** D2Common `SelectRandomTileVariant` `6fdb8b90` calls the D2CMP getter for `block+0x20` twice: at `6fdb8c29` to accumulate `total += weight` over all candidates, at `6fdb8ca5` to walk `remaining -= weight` against `rand() % total`. Textbook weighted selection. **[R]** `src/misc.c` `misc_check_tiles_conflicts()` implements the same algorithm. **[D]** Distribution is weight-shaped: 21,438 blocks at 0, 3,783 at 1, 379 at 2, thin tail to 30 |
| **Impact** | A tool treating `+32` as an animation frame mis-sorts the catalogue silently |

### C3 — Sub-tile `+10` is `uint16`, not `int32`

| | |
|---|---|
| **Before** | `dt1make.c` declares `long data_length`; `dt1info.c` reads `fread(..., 1, 4, in)`; `src/core/dt1.c:246` reads `*(const int32_t *)(st_ptr + 10)` |
| **After** | 16-bit. Bytes `+12..+15` are a separate, never-written reserved pair |
| **Evidence** | **[G]** Every D2CMP access is a `ushort` — `CalculateTileResourceSize` (`*(ushort*)(p+10)`, and the pointer walk `puVar1 += 10` on a `ushort*` = +20 bytes), `FixupCelDataLayout`, `SerializeSubtileData`. Two independent aborts if length reaches `0x10000`: `ConvertAndCompressSubtile` at `Tilecmp.cpp:1077`, `CompressSubtileToRLE` at `SubTile.cpp:358`. **[D]** `+12..+15` zero in all 564,457 sub-tiles, which is why the 32-bit read works |
| **Impact** | Reads are safe on all existing files. Writers must emit two bytes |

### C4 — Block `+84` is three runtime pointer slots, not padding

| | |
|---|---|
| **Before** | `dt1info.c` labels it `zeros_3`; `src/core/dt1.c:195` comments `// skip 12 bytes : zeros3`; `dt1extr.c` reads them into `int zeros2[12]` and ignores them |
| **After** | `+84` = sub-tile array pointer, `+88` = filename pointer, `+92` = LRU cache handle. Zeroed by the serializer, filled by the loader |
| **Evidence** | **[G]** `SerializeCelDataToBuffer` explicitly zeroes `piVar8[3]`; `FixupCelDataLayout` sets `+0x58` to `cel + 8` (a getter is exported, ordinal 10035); `LoadCelDataCached` `6fe1bc90` reads `+0x48`, `+0x4c`, `+0x54`, `+0x58`, `+0x5c` as seek target, length, buffer, filename and cache handle respectively |
| **Impact** | Explains why they exist, and predicts the finding below |

### C5 — The 260 "zero bytes" at offset 8 are a `char[260]` filename buffer

| | |
|---|---|
| **Before** | `dt1make.c` writes 260 literal zeros; `dt1extr.c` *validates* they are zero; `src/core/dt1.c` skips them |
| **After** | A `MAX_PATH` buffer. `0x10C − 0x008 = 0x104 = 260` exactly |
| **Evidence** | **[G]** `FixupCelDataLayout` copies the file's path into `cel + 8` at load time. **[D]** All 354 valid files have them zeroed on disk, so validating zero is correct for files but wrong as a description of the field |

### C6 — The survey's "two independent implementations agreeing"

| | |
|---|---|
| **Before** | `INVENTORY.md` §"Non-HTML load-bearing sources" and priority item 4: "The repo's own `src/core/dt1.c` independently uses header `+272` and a 96-byte block stride — **two independent implementations agreeing is strong evidence**" |
| **After** | They are **not independent**. `src/core/dt1.c` opens with `/* Derived from win_ds1edit by Paul Siramy (originally dt1misc.c). */`, and `NOTICE` lists it among the files that derive directly from Siramy's code |
| **Evidence** | The file header comment; `NOTICE` §1 |
| **Impact** | The chapter does not use that agreement as evidence. Every load-bearing claim rests on the 354 real files, the 1.13c binaries, or both. Where `[S]` and `[R]` agree it is noted as a shared lineage, not corroboration |

---

## Findings the sources do not contain

Three results in the chapter are not in Siramy's material, not in this repo, and
not in any published DT1 reference the author is aware of. Each is stated with its
evidence.

### F1 — 1,378 tiles ship with a leaked heap pointer at block `+88`

Reading all 26,005 block headers found the twelve "padding" bytes at `+84` are
**not** always zero (**[D]**). Exactly one of the three slots is ever non-zero:

| Slot | Non-zero blocks |
|---|---|
| `+84` (`0x54`) sub-tile array pointer | 0 |
| `+88` (`0x58`) filename pointer | **1,378** |
| `+92` (`0x5C`) cache handle | 0 |

16 distinct values, all 4-byte aligned, all in `0x01CD0048 … 0x0B37004C` — the low
user heap of a 32-bit Windows process. Concentrated entirely in Lord of
Destruction tilesets (`expansion\Town\buildings.dt1` 222, `expansion\Siege\trees.dt1`
143, `expansion\Siege\building.dt1` 121, …), and recurring in runs.

The decompilation (**[G]**, C4 above) says `+0x58` is where the loader writes a
pointer to the file's own name. The empirical survey found stale pointers in
exactly that slot and nowhere else. Neither result identifies the field alone;
together they do. Harmless — the loader overwrites all three slots on load, which
is why it shipped.

### F2 — `direction` is a pure function of `orientation` in Blizzard's files

Across 17,230 blocks in the 259 readable Blizzard tilesets, **all twenty
orientation values map to exactly one direction value at 100%** (**[D]**):

```
orientation:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
direction:    3  1  2  3  3  1  2  4  1  2  1  2  3  3  3  5  6  7  8  9
```

The exceptions are all in `assets/tiles/PD2assets/` (mod-authored): orientations
7, 9, 16, 17, 18 and 19 each carry a minority with a different direction — 18% of
the lower walls, and orientation 19 splits three ways. Nothing in 1.13c branches
on `direction` (**[G]**), so the mod tiles work anyway.

### F3 — The `floor_flags.gif` numbering decoded, and triple-confirmed

The lost page's most load-bearing figure numbers a floor tile's 25 cells 0–24. The
chapter establishes:

> flag byte `t` (at block offset `+40 + t`) belongs to sub-tile grid cell
> `(x_grid = t % 5, y_grid = 4 − t / 5)`

**Three independent confirmations:**

1. **The diagram** (**[S]**). All four corners: left `(0,4)` = 0, bottom `(4,4)` = 4,
   top `(0,0)` = 20, right `(4,0)` = 24. Cross-referenced against the pixel→grid
   map derived empirically from 248,132 real floor sub-tile headers.
2. **The real files** (**[D]**). The mapping makes a falsifiable prediction: a left
   wall (orientation 1) must flag `x_grid = 0`, a right wall (orientation 2) must
   flag `y_grid = 0`. Counting non-zero flags across every Blizzard tileset:
   orientation 1 → 5,619 on the upper-left edge vs 292 on the opposite edge;
   orientation 2 → 5,916 on the upper-right vs 254 opposite. Orientations 8 and 9
   flag their opposite edge exactly **zero** times. Orientation 7 (corner post)
   sets `t = 20` — grid `(0,0)`, the top corner — in 185 of its 188 flagged blocks.
3. **The game's collision code** (**[G]**). D2Common imports D2CMP ordinal 10011
   (`GetTileDirExtraDataPtr` `6fe251e0`, returns `block + 0x28`). Its only three
   callers — `COLL_CopyTemplateToGrid` `6fd9b617`, `COLL_ClearTemplateFromGrid`
   `6fd9b6d7`, `COLL_SetTemplateToGrid` `6fd9b7a7` — start at flag offset `4×5 = 20`
   for grid row 0 and walk *backwards* five bytes per row, OR-ing each byte into a
   16-bit-per-cell grid. That is `y_grid = 4 − t/5`, from the game itself.

**An important negative:** the first attempt at this mapping (`y_grid = t/5`) was
**falsified** by check 2 — orientation 2's flags landed on the wrong edge. The
prediction was designed to be able to fail, and it did, which is why the final
mapping is trustworthy.

---

## Type-F claims confirmed (no correction needed)

Each row was checked against every source listed. `[G]` = 1.13c binaries,
`[S]` = Siramy's C, `[R]` = this repo, `[D]` = 354 real files.

| Claim | Sources agreeing | Notes |
|---|---|---|
| Fixed header = 276 bytes | G S R D | See the 276/272 section |
| Magic at `+0` = 7 | G S R D | The only magic the game checks |
| 260 bytes at `+8` zero on disk | G D | Semantics corrected — C5 |
| `number_of_blocks` at `+268` | G S R D | `MOV EAX,[ESP+0x114]` reads `0x10C` |
| `block_header_offset` at `+272` = 276 always | G S R D | 354/354 files, and hard-coded in the game |
| Block header stride = 96 (`0x60`) | G S R D | `LEA/SHL` ×0x60 in the sizer; `iVar4 * 0x60 + 0x114` in the writer |
| Sub-tile header stride = 20 (`0x14`) | G S R D | `iVar5 += iVar2 * 0x14` in `CalculateTileResourceSize` |
| Block `+8` `size_y` always ≤ 0 | G S R D | 25,971 negative, 34 zero, **0 positive**. `CloneTileDirection`: `param_1[2] = in_EAX[3] - in_EAX[1]`, structurally negative |
| Block `+16` reserved, never written | G D | `CloneTileDirection` skips `dst[4]` entirely; zero in all 26,005 blocks |
| Block `+40` = 25 flag bytes | G S R D | `CloneTileDirection` copies 6 dwords + 1 byte = 25; getter ordinal 10011 returns `block+0x28` |
| Block `+65` 7 bytes zero on disk | G D | Zero in all 26,005 blocks. **[G]** notes byte `+0x46` is used as *runtime* scratch by the compiler (`ProcessTileSubtileEntries` `6fe21270`), not file content |
| `tiles_ptr` at `+72` is an **absolute** file offset | G S R D | `FixupCelDataLayout`: `*(p+0x54) = *(p+0x48) + fileBase` |
| `data_offset` at sub-tile `+16` is **relative to `tiles_ptr`** | G S R D | `SerializeSubtileData`: `*local_8 = (int)puVar6 - (int)in_EAX`; `RelocateSubtileDataPointers` adds the same base back |
| Sub-tile `+4` and `+14` reserved, zero | G D | Zero in all 564,457 sub-tiles |
| Sub-tile `+6`/`+7` = `x_grid`/`y_grid`, two separate bytes | G S R D | `GenerateFloorSubtiles` `6fe268a0` sets `i%5` and `i/5`; `CloneTileDirection` moves them as two separate byte writes (unlike block `+6`/`+7`) |
| `format == 0x0001` ⟺ exactly 256 bytes | G S R D | **365,702 of 365,702**. `ExtractDiamondSubtile` ends `format=1; length=0x100` |
| Isometric row table `xjump[15]`/`nbpix[15]`, sum 256 | G S R D | Byte-identical in `dt1make.c`, `dt1extr.c`, `src/core/dt1_draw.c`, and D2CMP `6fe20aa0` |
| RLE grammar: `(skip, run)` pairs, `(0,0)` = end of row | S R D | **198,755 of 198,755** streams decode to a clean end. **[G]** adds the encoder detail that a maximal transparent run may be a lone `0x7F` byte |
| Floor tile = 25 sub-tiles on a fixed 5×5 table | G S D | **248,132 floor sub-tiles, 0 outside the table, exactly 25 distinct positions.** D2CMP's copy is `DAT_6fe32bf8` (Siramy's is the same set in reverse order) |
| Floor sub-tiles stored in Siramy's `pos[]` order | S D | **10,293 of 10,293** floor blocks — full sequence when 25 present, in-order subsequence otherwise. Zero exceptions |
| Floor and roof blocks are always 160 × −128 | S R D | 16,194/16,194 floors; 936/936 roofs |
| `tiles_length` = `20 × tiles_number` + Σ data lengths | S D | **25,933 blocks, 0 mismatches** |
| Sub-tile data is contiguous, headers then data | S D | **564,457 of 564,457** `data_offset` values equal the previous sub-tile's end |
| Three `y_pos` origin conventions | S R D | Floor/roof `0…64`; upper wall `−832…−32`; lower wall `−96…864`. The `−96` floor is exact across every file, matching `dt1make.c`'s `ypos = y - 96` and `dt1.c`'s `y_add = 96` |
| `x_pos` ∈ {0,32,64,96,128} | S D | Five 32-px columns across a 160-px tile, all classes |
| Orientation range is 0–19 | G S R D | 26,005 blocks, no value outside 0–19 |
| `roof_y` at `+4` is **unsigned** 16-bit | G D | D2Client `6fb2a6a5`: `MOVZX EDX, word ptr [ECX+0x4]`, then `tileY - (uint)*(ushort*)(block+4)`. One outlier value (56,376) reads as nonsense signed |

---

## Type-B claims (behavioral) — sampling policy and verdicts

Policy: **every** function named in the chapter as doing something specific was
decompiled. No generic-accessor sampling was needed, because the DT1 surface in
D2CMP is small (about a dozen functions).

| Claim | Function | Verdict |
|---|---|---|
| Loader reads 276 bytes then `count × 96` | `OpenResourceAndCalculateSize` `6fe1bad0` | Confirmed from disassembly |
| Pixel data loads lazily, per block, LRU-cached | `LoadCelDataCached` `6fe1bc90` (ord 10106) | Confirmed; reads `+0x48/+0x4c/+0x54/+0x58/+0x5c` |
| Writer emits header + `N×96` + per-block (`M×20` + data) | `CalculateTileResourceSize` `6fe25830`, `SerializeCelDataToBuffer` `6fe25e20` | Confirmed; all three strides in one expression |
| Sub-tile offsets are relocated at load | `RelocateSubtileDataPointers` `6fe25c10`, `FixupCelDataLayout` `6fe25d10` | Confirmed |
| Catalogue is keyed on (orientation, main, sub) | `InsertTileIntoLookupTable` `6fe22f20` | Confirmed; hash `(sub*2 − orient + main) & 0x7F`, 128 buckets |
| Rarity drives weighted random selection | `SelectRandomTileVariant` `6fdb8b90` | Confirmed — see C2 |
| The 25 flags become the collision grid | `COLL_{Copy,Clear,Set}TemplateToGrid` | Confirmed — see F3 |
| Orientation 15 has its own render path | `ProcessTilesForWallRendering` `6fb2a5d0` | Confirmed; same branch applies `roof_y` |
| Orientations 16–19 are one contiguous group | same | Confirmed: `0x10 ≤ v ≤ 0x13` |
| Orientations 8, 9 are the doors | `DRLG_SetTileFlagsFromType` `6fdb8a90` | Confirmed: `TriggerTileSoundEffect` iff type is 8 or 9 |
| Orientations 10, 11 are a left/right pair | `DRLG_GetTileTypeCode` `6fdb8280` | Confirmed: `'r'` for 11, `'l'` otherwise |

D2CMP still links Blizzard's offline **tile compiler** — four assertion source
paths survive with their original line numbers: `..\Source\D2CMP\SRC\Tilecmp.cpp`
(`6fe2d354`, 29 xrefs), `SubTile.cpp` (`6fe2fe68`, 12), `FindTiles.cpp`
(`6fe2fdc0`, 8), `TileProjects.cpp` (`6fe30050`). That is why the writer's view of
the format is available at all, and it is the single biggest reason this
reconstruction could be settled rather than inferred.

---

## Unverified, and why

| # | Item | Why it could not be settled |
|---|---|---|
| 1 | Meaning of the 8 sub-tile flag bits | No 1.13c code path names any bit. The collision system ORs the byte wholesale and lets downstream consumers test what they like. **[D]** establishes that bits 3, 6 and 7 are never set in 650,125 bytes — that is the strongest available statement. The community reading (0 = block walk, 1 = block light/LoS, 2 = block jump) is *consistent* with the distribution but is not evidence and is **not adopted** |
| 2 | Individual meanings of orientations 1–7 and 12 | 1.13c routes them all through one generic wall path. Rests on Siramy's figures plus two independent real-data checks (flag edges, `x_pos` columns), which agree — but no binary names them |
| 3 | Purpose of `direction` at block `+0` | Stored, exported (ordinal 10099 `6fe25240`), never read for a decision in D2CMP/D2Client/D2Common. Its 1:1 map to `orientation` is established (F2); *why the field exists* is inference and marked as such |
| 4 | The `0xFF00FF00` value at block `+36` | The field is provably inherited from a per-file parent (`CreateTileDirectionEntry`: `piVar1[13] = *(int*)(pTileHeader + 4)`), so it is constant within a file. The literal never appears in D2CMP. **[D]** shows 13,644 blocks with `0x00FF00FF` and a long tail of others — so "always `0xFF00FF00`" is false as written in the archive |
| 5 | The `sound`/`animated` byte split at block `+6`/`+7` | D2CMP only ever touches those two bytes as one 16-bit unit (getter ordinal 10051 `6fe25300` returns `*(short*)(p+6)`; `CloneTileDirection` moves the pair). The split is documented by the readers, not by the game. **[D]** values are consistent with either reading — `animated` takes 0, 1 and 4 (273 blocks at 4), which readers checking `if (animated)` handle but readers checking `== 1` do not |
| 6 | The six version-4.1 files | The header shape is characterised (see below) but the 44-byte record was not decoded, and no 1.13c path accepting a `4/1` magic was traced |
| 7 | `Game.exe` | Not searched. 1.13c uses the split-DLL layout and the loader is unambiguously in D2CMP, so there was no reason to — but "not searched" is not "not there" |

### The six version-4.1 files

Six of the 360 `.dt1` files begin `04 00 00 00 01 00 00 00` instead of `07/06`:
`ACT1/BARRACKS/barracks.dt1`, `ACT1/BARRACKS/gargtrap.dt1`,
`ACT1/CATACOMB/Catacombs.dt1`, `ACT1/CATHEDRL/Cathedrl.dt1`,
`ACT1/COURT/Court.dt1`, `ACT1/OUTDOORS/Outdoor1.dt1`.

They are **not corrupt**. In all six, the int32 at `+20` is a record count `N` and
the int32 at `+8` equals exactly `24 + 4N` — the end of a table of `N` 32-bit file
offsets beginning at `+24`:

| File | `+8` | `+20` = N | `24 + 4N` | Size |
|---|---|---|---|---|
| `barracks.dt1` | 160 | 34 | 160 ✓ | 541,406 |
| `gargtrap.dt1` | 28 | 1 | 28 ✓ | 10,789 |
| `Catacombs.dt1` | 112 | 22 | 112 ✓ | 440,972 |
| `Cathedrl.dt1` | 284 | 65 | 284 ✓ | 1,210,193 |
| `Court.dt1` | 328 | 76 | 328 ✓ | 1,180,690 |
| `Outdoor1.dt1` | 240 | 54 | 240 ✓ | 432,252 |

Those offsets point at fixed **44-byte** records near end-of-file; `barracks.dt1`'s
34 records run 539,910 → 541,406, which is the file size to the byte.
`Catacombs.dt1`'s 22 records are byte-identical to each other, so 44 bytes is not
simply a shorter block header. Both Siramy's tools and this repository hard-reject
them on the magic. **[I]** They are plausibly an earlier revision of the format
that survived into the shipped archive; that is inference and marked as such in
the chapter.

---

## Enrichment: what was added beyond the format tables

Per the skill's step 6, and with `narrative-nonfiction` loaded before the prose
pass. Every number, address and behavioral statement introduced by the prose was
re-verified by the same standard as the tables.

- **A through-line**: one real tile — block 0 of `assets/tiles/ACT1/TOWN/floor.dt1`
  — followed from the header through the block header, the sub-tile headers, and
  into the pixel bytes, with the arithmetic closing exactly (`tiles_ptr` = 14,100 =
  the end of the block-header array; `data_offset` 500 = `25 × 20`; last sub-tile
  ends at 6,900 = `tiles_length`).
- **"Why it exists" grounded in evidence, not invented**: the DS1→DT1 key
  computation from `src/core/ds1.c:136` and the matching hash from
  `InsertTileIntoLookupTable` `6fe22f20`.
- **Load path narrative** from the two-stage loader, the LRU cache, and the two
  D2Client cache-report strings (`6fb85950`, `6fb85908`).
- **No fabricated history.** The one place the chapter speculates about people —
  the leaked heap pointer implying a build-tool bug — is stated as what the bytes
  show plus what the decompiled writer says the slot is for. No claim is made
  about who wrote it or when beyond "Lord of Destruction tilesets", which is where
  the files are.

---

## Reconciled contradictions

| Contradiction | Resolution |
|---|---|
| `dt1_head_size 276` vs `+272` | Not a contradiction — offset vs value. See above |
| `frame` (Siramy) vs `rarity` (this repo) at `+32` | **Repo right.** `SelectRandomTileVariant` proves the weight semantics |
| `long data_length` (Siramy, repo) vs 16-bit (game) at sub-tile `+10` | **Game right.** Two compile-time asserts at `0x10000` |
| `zeros3` (Siramy, repo) vs three pointers (game) at `+84` | **Game right**, and the 1,378 leaked pointers on disk prove the slot is live |
| `dt1make.c` stores flags at memory index `i` = file byte `i`; `src/core/dt1.c` remaps through `idxtable[]` | Both correct; they are different *in-memory* conventions in two of Siramy's own tools. The chapter documents the **file** order and the grid mapping, which is what an implementer needs |
| `dt1extr.c`'s `is_floor &2`, `is_wall &4` … macros | **Not a file field.** These are the tool's own classification bits, computed from `direction` and `orientation`. The chapter says so explicitly to stop them being mistaken for a DT1 flags byte |
| "Always `0xFF00FF00`" at `+36` | False as written. 13,644 of 26,005 blocks, with a long tail |

---

## Open questions for the author

1. ~~**Rights.**~~ Settled — see the Rights section above: the project
   proceeds on a fair-use judgment for the images and prose, not a grant from
   the author.
2. **The six version-4.1 files.** Where did this repository's `assets/tiles/` tree
   come from, and are these six present in a retail 1.13c MPQ, or artifacts of the
   extraction? That determines whether the finding is about Blizzard's data or
   about this tree.
3. **Flag bit semantics.** Is there a Blizzard-side artifact (a `.txt`, a level
   editor, a leaked tool) that names them? The binaries do not.
4. **Should `src/core/dt1.c` be changed** to read `+10` as `uint16` and to stop
   describing `+84` as `zeros3`? Both are cosmetic on existing data, but the
   comments are now known to be wrong. This task owns only the two documentation
   files and made no code change.

---

## Files touched

**Created:**
- `docs/guides/dt1-tile-format.md`
- `docs/guides/dt1-tile-format.verification.md`

**Read only, never modified:** everything under `docs/preservation/`, all 360
`.dt1` files, `src/core/dt1.c`, `src/core/dt1_draw.c`, `src/core/ds1.c`,
`src/misc.c`, `src/structs.h`, `src/ui/tile_picker.c`, `NOTICE`.

**Not touched:** `docs/guides/monsters-and-objects.md`,
`docs/guides/cof-pipeline-1.13c.md`, `docs/getting-started/`, `docs/tutorials/`,
and the `d2-fleet` repository.
