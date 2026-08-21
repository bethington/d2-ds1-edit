# Palettes and Colour

*Reverse-engineered from the Diablo II 1.13c game DLLs, and from the game's own
data archives. The addresses below are 1.13c addresses, so they hold for any
mod built on that version — mods of this era ship Blizzard's DLLs unchanged.*

> **Provenance.** Verified against the retail 1.13c binaries in Ghidra and
> against the game's own vanilla archives, 2026-08-21: `D2CMP.dll` (image base
> `6fe10000`, SHA-256 `2ee205f4…4dc34975`) and `D2Client.dll` (`6fab0000`,
> SHA-256 `dd8bc602…8836d906`), both from the stock 1.13c LoD install; and the
> ten 1.13c MPQ archives, read through `tools/d2mpq.py`. Every function address
> below was confirmed to start exactly there. The palette and palette-transform
> **byte layouts** were not just decompiled — they were independently
> reproduced in Python from the decompiled formulas and diffed against the
> real shipped files, byte for byte, across sixteen palette files and eight
> item-transform files. A companion audit records every check, every
> reproduction run, and everything left open — see
> [palettes-and-colour.verification.md](palettes-and-colour.verification.md).

## Why a 256-colour engine bothers with any of this

A Diablo II sprite frame, decoded, is not pixels. It is a grid of single
bytes, each one an index from 0 to 255 into a palette — a fact the
[sprite-formats chapter](sprite-formats-dcc-dc6.md#from-a-decoded-frame-to-pixels-on-screen)
establishes and hands off, unfinished, right here: *"the bytes in a decoded
frame are palette indices; which palette, and which remapping table on top of
it, is chosen per component and per blit."* This chapter is that choice.

The economy is the point. A monster's DCC file stores one set of index
values. Whether that monster is drawn at full daylight brightness in the
Rogue Encampment or three tiles into shadow in the Catacombs is not a second
copy of the sprite — it is the *same* indices read through a different
256-byte table. Whether a piece of Ring Mail looks tin-grey or ash-brown is
not a second `.dcc` — it is the same torso cel, the same indices, through a
different table again. One set of pixels, reinterpreted as many times as the
game needs, at the cost of a table lookup per pixel instead of a decode per
variant. On 2000-era hardware, with a fixed 256-colour screen mode to begin
with, this was not an aesthetic choice; it was the only way to afford as much
visual variety as the game has.

That reinterpretation happens in exactly two kinds of file, and this chapter
follows both from disk bytes to a verified pixel: the **`.dat` palette**, 256
colours and nothing else, and the **`.pl2` transform table**, a half-megabyte
of every way the engine has learned to reinterpret those 256 colours before a
pixel reaches the screen.

## The worked example

Everything below is anchored to one real value, checked against one real
file, so the abstractions stay honest. Act I's palette —
`data\global\palette\ACT1\pal.dat`, read from `Patch_D2.mpq`/`D2Data.mpq` with
`tools/d2mpq.py` — assigns index **150** the colour **RGB(188, 120, 84)**, a
warm leather tan. Its neighbouring transform file, `Pal.PL2` in the same
folder, is 443,175 bytes. By the end of this chapter that number, that index,
and that colour will each have been traced to an exact byte offset and back
again.

## The `.dat` palette: 256 colours, 768 bytes, nothing else

A `.dat` palette is the plainest file this chapter covers: 256 entries, 3
bytes each — red, green, blue, in that order, no header, no padding, no
count. `pal.dat` in Act I's folder is 768 bytes exactly, and every one of
the sixteen palette files this chapter checked was 768 bytes exactly.

The loader confirms the shape of its own input. `LoadPaletteFile`
(`D2CMP.dll @ 6fe1a2e0`) validates its entry-range arguments, then opens the
named file and calls `GetFileSize`. **If the size is exactly `0x300`
(768)**, it reads the file as a raw RGB triplet array; anything else falls
through to `LoadPCXPaletteFromFile` (`@ 6fe19b00`), a second path that reads
a classic 256-colour `.pcx`'s embedded VGA palette instead (validated by the
PCX magic byte `0x0A`, version `5`, depth `8`, and the trailing `0x0C`
palette marker). Every screen-palette file in the archives is the 768-byte
form; the PCX path exists, and is real code, but this pass did not find a
`.dat` palette in vanilla 1.13c that takes it.

If the filename is empty, `LoadPaletteFile` does not fail — it synthesizes a
grey ramp, entry *i* set to RGB(*i*, *i*, *i*) for all 256 indices. That is
the fallback the engine reaches for when there is nothing else to draw: pure
greyscale.

**Checked across all sixteen `.dat`/`Pal.PL2` folders** in the 1.13c
archives (`ACT1`–`ACT5`, `EndGame`, `EndGame2`, `Sky`, `Trademark`, `fechar`,
`loading`, `Menu0`–`menu4`, plus the palette-only `STATIC` and `Units`): every
`.dat` file is exactly 768 bytes, and its 256 RGB triplets are copied
verbatim — same order, same byte count — into the front of its companion
`Pal.PL2`, confirmed by direct comparison of every one of the 256×3 bytes in
all sixteen files. `STATIC` and `Units` ship a `pal.dat` with no matching
`Pal.PL2` at all; nothing in this pass explains why those two screens skip
the transform buffer, and it is left as an open question in the companion
report.

## Loading the palette writes a second copy, reversed and padded

Inside `Pal.PL2`, the copied palette is not 768 bytes — it is 1,024. Each of
the 256 entries grows a fourth byte, and the first three change order.
`LoadPaletteFile`'s own copy loop, read at the instruction level, writes each
RGB triplet as **blue, green, red, pad** — a 4-byte, DWORD-aligned entry that
reads back as the little-endian value `0x00RRGGBB`. Index 150's tan
`(188, 120, 84)` is stored as the four bytes `54 78 BC 00` (blue 0x54, green
0x78, red 0xBC, pad 0x00) — confirmed by reading `Pal.PL2` at offset
`150 × 4 = 600 (0x258)` directly and comparing it against `pal.dat`'s own
triplet at offset `450`. Every one of the 256 entries, in all sixteen files,
matches this pattern with zero exceptions: `pl2[i*4] = pal[i*3+2]`,
`pl2[i*4+1] = pal[i*3+1]`, `pl2[i*4+2] = pal[i*3]`, `pl2[i*4+3] = 0`.

This padded, reordered copy is the working palette every transform table in
the rest of the file measures itself against — the fixed point every `nearest
colour` search below resolves to.

## The `.pl2` file is a cache of a function's own output

The file is not authored by hand. `BuildPaletteTransformTables`
(`D2CMP.dll @ 6fe1b930`) takes a loaded 256-entry palette, `VirtualAlloc`s a
buffer of exactly `0x6C327` bytes — **443,175, the exact size of every
full-length `Pal.PL2` this pass found** — and fills it by calling nine
generator functions in sequence: darkness, highlight, additive blend,
multiply blend, a family of hue/lightness/saturation rotations, a saturation
boost, a screen blend, and a set of colour tints. Every one of those
generators calls `FindNearestPaletteColor` (`@ 6fe19d30`) to snap a computed
RGB value back onto the nearest of the loaded palette's own 256 entries —
because every table in this file, however it was derived, still has to end
up as a single byte a blitter can use as a sprite index.

That is a strong claim to make about a shipped data file — that it is
nothing but this function's memoized output — so it was tested, not assumed.
Every generator's formula was reimplemented independently in Python and
diffed against the real bytes of `ACT1\Pal.PL2`, using `ACT1\pal.dat` as the
only input, exactly as the function does:

| Table | Formula source | Bytes checked | Result |
|---|---|---:|---|
| Base palette copy | `LoadPaletteFile` copy loop | 1,024 | **1,024 / 1,024 exact** |
| Darkness (32 levels) | `GenerateDarknessPaletteTable` | 8,192 | **8,192 / 8,192 exact** |
| Highlight (16 levels) | `GenerateHighlightPaletteTable` | 4,096 | **4,096 / 4,096 exact** |
| Alpha blend (3 ratios, full 256×256 each) | `GenerateAlphaBlendTables` | 196,608 | **196,608 / 196,608 exact** |
| Additive blend (256×256) | `GenerateAdditiveBlendTable` | 65,536 | **65,536 / 65,536 exact** |
| Multiply blend (256×256) | `GenerateMultiplyBlendTable` | 65,536 | **65,536 / 65,536 exact** |
| Colour tints (12 tables) | `GenerateTintPaletteTable` | 3,072 | **3,072 / 3,072 exact** |
| **Total independently reproduced** | | **344,064 / 443,175 (77.6%)** | |

Zero mismatches, on every table attempted, reproducing better than three
quarters of a 443 KB file's bytes from formulas read out of the disassembly.
The remaining fifth — a hue-rotation family, a saturation adjustment, and a
screen-blend table, all described below — was *located* (the generating function and its
buffer offset are confirmed) but not independently reproduced, because their
formulas round-trip through HSL colour space or a magic-number integer
division this pass did not re-derive exactly. That distinction — confirmed
by byte-for-byte reproduction versus confirmed only by reading the
disassembly — is kept explicit in the table below and the companion report;
nothing here upgrades a decompile into a fact it was not checked against.

Darkness and highlight were then re-checked on five more files spanning
every kind of screen this format ships — `ACT2`, `ACT3`, `ACT5`, `menu0`,
`Sky` — with the same result: exact, every time. The claim below holds for
the format, not for one lucky file.

### The worked example: index 150 gets darker as the light does

`GenerateDarknessPaletteTable` (`@ 6fe19e60`) builds 32 tables, one per
distance-darkening level, at offset `0x400` through `0x23FF`. Its formula
per index *i*, level *L* (1–32): scale each channel by `L`, divide by 32
(`channel × L >> 5`), then find the nearest of the palette's own 256 colours
to that darkened target. Level 32 is full brightness — the palette's own
colour, unscaled. Level 1 is the far edge of visibility.

Run against index 150's tan, RGB(188, 120, 84), the formula and the real
file agree at every level tried:

| Level | Darkened RGB | Nearest index (computed) | `Pal.PL2` byte at `0x400+(L-1)×256+150` | Match |
|---:|---|---:|---|:---:|
| 32 (full) | (188, 120, 84) | 150 | 150 | yes |
| 24 | (141, 90, 63) | 146 | 146 | yes |
| 16 | (94, 60, 42) | 141 | 141 | yes |
| 8 | (47, 30, 21) | 244 | 244 | yes |
| 1 (near-black) | (5, 3, 2) | 172 | 172 | yes |

A monster standing in full torchlight and the same monster three rooms into
the dark are the same sprite, the same index 150, read through a different
row of this one 8,192-byte table — and that table's exact contents are now
confirmed, not assumed, at this offset in this file.

## The buffer, table by table

With the worked example anchoring the darkness table, here is the rest of
the 443,175-byte buffer, offset by offset. "Confirmed" means independently
reproduced and byte-diffed against the real file with zero mismatches, as
above; "Located" means the generating function and its write offset were
read from the disassembly but the exact output was not separately
reproduced.

| Offset | Size | Contents | Generator (`D2CMP.dll`) | Status |
|---|---:|---|---|---|
| `0x0000` | 1,024 B | Base palette, reordered to blue/green/red/pad | `LoadPaletteFile @ 6fe1a2e0` | **Confirmed**, 16 files |
| `0x0400` | 8,192 B | Distance darkness, 32 levels × 256 | `GenerateDarknessPaletteTable @ 6fe19e60` | **Confirmed**, 6 files |
| `0x2400` | 4,096 B | Highlight (brighten-toward-white), 16 levels × 256 | `GenerateHighlightPaletteTable @ 6fe19dc0` | **Confirmed**, 6 files |
| `0x3400` | 256 B | Single saturation-boosted variant (+0.2 HSL saturation) | `AdjustPaletteSaturation @ 6fe1aad0` | Located |
| `0x3500` | 65,536 B | Alpha blend, ~75% (`0xBF`/255) | `GenerateAlphaBlendTables @ 6fe1a070` | **Confirmed**, ACT1 |
| `0x13500` | 65,536 B | Alpha blend, ~50% (`0x7F`/255) | (same) | **Confirmed**, ACT1 |
| `0x23500` | 65,536 B | Alpha blend, ~25% (`0x3F`/255) | (same) | **Confirmed**, ACT1 |
| `0x33500` | 65,536 B | Additive blend (channel-clamped sum) | `GenerateAdditiveBlendTable @ 6fe1a640` | **Confirmed**, ACT1 |
| `0x43500` | 65,536 B | Multiply blend (channel product ÷ 255) | `GenerateMultiplyBlendTable @ 6fe19fa0` | **Confirmed**, ACT1 |
| `0x53500`–`0x5B500` | 32,768 B | Hue-rotation / lightness / saturation family — several sub-tables (multiple 24-step hue rings at different HSL settings, a 12-step fixed-lightness ring, three pure-channel R/G/B ramps) | `GenerateColorTransformTables @ 6fe1ab60` | Located; internal sub-offsets not reconciled against file bytes this pass |
| `0x5B500` | 65,536 B | Screen blend (full 256×256) | `GenerateScreenBlendTable @ 6fe1a490` | Located |
| `0x6B500` | 256 B | Self-screen-blend ramp | (same) | Located |
| `0x6B600`–`~0x6B627` | ~39 B | Small fixed data block; its bytes (from offset `0x6B603`) are the 12 source colours the tint table below reads | `BuildPaletteTransformTables` (copies from `g_dwData_3fe8`) | Content confirmed indirectly — see tint table |
| `0x6B727` | 3,072 B | 12 named colour tints × 256 | `GenerateTintPaletteTable @ 6fe19ed0` | **Confirmed**, ACT1 |

`0x6B727 + 3,072 = 0x6C327`, the buffer's own declared size — the tint table
is the last thing written, and it lands exactly on the end of the
allocation. A roughly 256-byte span between the small data block and the
tint table, and the exact internal split of the 32 KB hue-rotation region,
were not independently accounted for in this pass and are left open.

### The twelve tints, decoded

`GenerateTintPaletteTable`'s formula reads a fixed RGB triple per tint,
scales it by each palette entry's own brightness (taken from that entry's
stored blue byte — the first byte of the four-byte copy at offset 0), and
snaps the result to the nearest palette colour. Reproducing that formula and
reading the 12 source triples straight out of `ACT1\Pal.PL2`'s own bytes —
no guessing, no external reference — recovers colours that read like a
palette of status-effect and elemental washes: a blue-violet, a pure green,
a salmon red, a steel blue-grey, neutral grey, black, cyan, sky blue, pale
cyan, dark green, magenta, and a second green. What consumes each of the 12
tint *indices* (0–11) — which skill or state picks which — was not traced in
this pass; only that the table exists, at this offset, with these colours,
confirmed byte for byte.

### What was not reproduced, and why that's honest rather than lazy

Three regions — the saturation table, the hue-rotation family, and the
screen-blend table — round-trip every palette entry through HSL colour
space or an integer approximation of division by three, using floating-point
and fixed-point arithmetic this pass chose not to re-derive bit-for-bit. Their
existence, purpose, and offsets are as solid as anything else in this
chapter (read straight out of the disassembly, with concrete literal
offsets); their *exact output* is not independently confirmed the way the
75% of the file above is. Marking that distinction, rather than quietly
presenting a decompile-only claim with the same confidence as a byte-verified
one, is the entire discipline this chapter tries to hold to.

## Item colour: eight files, one shared array, one colour code per item

Item recoloring — the reason a suit of Ring Mail can be tin-grey on one
character and ash-brown on the next without two copies of its sprite — runs
through a completely different, much smaller mechanism than the screen
palettes above.

`LoadAllItemPaletteTransforms` (`D2CMP.dll @ 6fe250d0`) loads exactly eight
files from `data\global\items\Palette\` — confirmed present, by that exact
name, in the 1.13c and 1.09d archives — into eight numbered slots of one
shared array:

| Slot | File | `GetPaletteBrightnessTable` accepts it? | Used by any armor/weapon row? |
|---:|---|:---:|:---:|
| 1 | `grey.dat` | yes | yes — 123 records |
| 2 | `grey2.dat` | yes | yes — 104 records |
| 3 | `gold.dat` | **no** | never |
| 4 | `brown.dat` | **no** | never |
| 5 | `greybrown.dat` | yes | yes — 140 records |
| 6 | `invgrey.dat` | yes | **never** |
| 7 | `invgrey2.dat` | yes | yes — 18 records |
| 8 | `invgreybrown.dat` | yes | yes — 54 records |

Each file is 5,376 bytes — 21 rows of 256 bytes. That number is not a
coincidence: `GetPaletteBrightnessTable` (`@ 6fe24ec0`) indexes the *same*
shared array (`&DAT_6fe34aa8`, the identical base symbol `LoadItemPaletteFile`
writes into) with the formula `(hueRotation × 0x69 + brightnessLevel) × 0x100`
— a 105-row-per-slot stride, of which only the first 21 rows (0–20) pass its
own bounds check. `LoadItemPaletteFile`'s slot stride, read independently
from its own disassembly, is `0x6900` bytes — `0x69 × 0x100`, the exact same
105-row reservation, confirmed by two unrelated functions agreeing on one
constant. Each 5,376-byte file fills the *first* 21 of the 105 rows its slot
reserves; the remaining 84 rows per slot go unpopulated in vanilla.

**Two of the eight loaded files are unreachable by construction.**
`GetPaletteBrightnessTable`'s own range check explicitly excludes hue
rotation `0`, `3`, and `4` — and slots 3 and 4 are `gold` and `brown`. Every
armor and weapon row's numeric `Transform` field was swept (508 records
across `armor.txt` and `weapons.txt`) and the values found are exactly
`{0, 1, 2, 5, 7, 8}` — `0` meaning "no transform," and 3, 4, and 6 never
appearing at all. Code and data agree on the same two dead slots.

That agreement runs one level deeper. `grey.dat`, `gold.dat`, and `brown.dat`
are **byte-identical** — same SHA-256, all 5,376 bytes — in the shipped
1.13c archive. Nobody differentiated `gold.dat` or `brown.dat` from `grey.dat`,
and given that nothing in the retail data can ever reach either slot through
this path, there was no reason to. **Slot 6, `invgrey.dat`, is the one live
gap**: structurally valid, loadable, distinct from the other seven files by
content — and never once selected by any vanilla armor or weapon row. It is
the one free lane in a shared, global table.

> **Mod note.** Every item that sets `Transform` to the same value shares the
> same 5,376-byte file — editing `greybrown.dat` recolors all 140 vanilla
> records that reference slot 5 at once, not just the one you meant to
> change. Slot 6 (`invgrey.dat`) is the only slot this pass found that is
> both usable and unclaimed by any vanilla item; it is the safest place to
> add a new item colour without touching an existing one. Slots 3 and 4
> (`gold.dat`, `brown.dat`) cannot be reached through the numeric `Transform`
> column at all in retail 1.13c — the accessor function that reads this array
> rejects both indices outright.

### The worked example, continued: Ring Mail's two colour codes

Armor and weapons carry the colour code directly, in `armor.txt` and
`weapons.txt`'s `Transform` and `InvTrans` columns — read straight from the
1.13c archive, not from a secondary reference. `Ring Mail` (`code rng`,
`component 1` — the `TR`, torso, layer in the
[COF chapter's layer order](cof-pipeline-1.13c.md#stage-3-componentlayer-system))
carries **`Transform = 7`** and **`InvTrans = 8`**: slot 7 (`invgrey2.dat`)
recolors the torso sprite while it is worn, and a *different* slot, 8
(`invgreybrown.dat`), recolors the inventory icon. The same piece of gear
routes through two different 5,376-byte tables depending on whether it is on
a body or in a grid square — one colour code isn't even always one code.

The reading that turns `Transform = 7` into a recolored torso sprite runs
through machinery the [COF chapter's Stage 3](cof-pipeline-1.13c.md#stage-3-componentlayer-system)
already names: `GetComponentListColorIndex` (`@ 6fb1d690`) walks a
component-list entry's stored colour indices (a small array, 9 bytes per
entry, keyed by component id) and returns the one that matches;
`CopyItemColorComponents` (`@ 6fb1d840`) copies six colour-component bytes
out of the item's own record for the renderer to use per layer. This chapter
does not re-derive that plumbing — it supplies the data-side half Stage 3
assumes: where the colour code actually comes from, and which file it
resolves to.

### A separate, richer system for unique items

`uniqueitems.txt` does not use the numeric `Transform` column at all. It
carries its own **`chrtransform`** and **`invtransform`** fields — four-letter
string codes, not numbers — and the vocabulary is far larger than the eight
item-transform files above: `blac`, `bwht`, `cblu`, `cgrn`, `cred`, `dblu`,
`dgld`, `dgrn`, `dgry`, `dpur`, `dred`, `dyel`, `lblu`, `lgld`, `lgrn`,
`lgry`, `lpur`, `lred`, `lyel`, `oran`, `whit` — 21 distinct codes across 402
unique items, each read directly from the archive. Whatever resolves a code
like `dgld` ("dark gold," by the naming pattern) to an actual palette
operation is a separate mechanism from the one this chapter traces above; it
was not located in this pass, and the 21-code vocabulary above is reported
as **(unverified beyond its existence in the data)** rather than guessed at.

## Monster colour: `palshift.dat`, one file per token

A third mechanism, smaller and more local than either of the above, recolors
individual monsters. `data\global\monsters\<token>\Cof\palshift.dat` — the
convention the string `R\Cof\palshift.dat` at `D2Client.dll @ 6fb85e27`
names generically (the game's own listfiles show real folders as `cof`,
`Cof`, and `COF` in varying case) — exists for **70 of the 210 distinct
monster tokens** in `monstats.txt`. Every one found is exactly **2,048
bytes**: eight tables of 256 bytes, confirmed by direct inspection.

Skeleton's own copy, `data\global\monsters\SK\cof\palshift.dat`, shows the
shape plainly. Table 2 of the 8 is the identity permutation — byte *i*
equals *i*, a no-op remap. The other seven are not: most low indices (0, 1,
8–11 — likely the engine's UI-safe or transparent reserved slots) pass
through unchanged in every table, while a broad middle range of indices
collapses toward a handful of repeated values (172 recurs across three of
the seven). That shape — most of the palette untouched, a targeted band
remapped toward one or two flat colours — is a silhouette or hit-flash
effect: the kind of thing that needs every pixel of a sprite to read as one
colour for a frame, without touching the handful of indices the engine
reserves for something else. This pass did not trace which of the 8 tables
maps to which in-game state; only that a real, non-trivial 8-table structure
exists, at this exact size, for 70 named monsters.

Loading one is `LoadMonsterPaletteShiftFile` (`D2Client.dll @ 6fb03c30`): it
reads two bytes out of a skill's own data record, builds the
`Data\Global\Monsters\<token>\Cof\palshift.dat` path from them, and — this is
the part that connects monster colour to the skill system rather than only
to monster rendering — registers the loaded 2,048-byte table into
`DAT_6fbc7804`, **the same per-skill array `GetSkillPaletteData`
(`@ 6fb03a30`) reads from at render time**. `InitializePaletteShiftSystem`
(`@ 6fb038f0`) allocates that array sized to the skill count read from the
game's own loaded skill table, one slot per skill id. A monster's
palette-shift table, in other words, is not reached only by rendering that
monster — it is reachable through *any skill* whose data record points at
that monster token, which is how a missile or an overlay effect cast by (or
representing) a monster can carry that monster's own colour shift.

The loaded bytes pass through one more step before use:
`ApplyPaletteShiftTransform` (`@ 6fb03b20`) remaps the freshly-read 2,048
bytes in place through a second, shared 256-byte table (`DAT_6fbcc2d8`,
itself loaded once from a default resource named by the string at
`D2Client.dll @ 6fb85e54`) before the result is registered for use. What
that second indirection does to any individual byte was not fully traced in
this pass; that it exists, and runs on every loaded `palshift.dat` before
the game uses it, is confirmed from the disassembly.

## Unit glow and render-mode colour

The last piece — how a unit's current *state*, not its equipment, picks a
colour — sits in three small D2Client functions, and this pass has less
confidence in the fine detail here than anywhere else in the chapter: their
decompiled bodies route through a heavily-shared internal dispatch helper
(the decompiler names it `Unwind_6fb7dc30`; it is not an unwind routine, and
the name should not be trusted — it is a generic per-unit property accessor
called from hundreds of sites across the DLL, the same pattern the COF
chapter's own audit flagged for a different symbol). What is solid is the
arithmetic each function performs around that call:

- **`GetUnitPaletteTransform`** (`@ 6fb1dee0`) takes a transform-type
  argument bounded to `0`–`15` — sixteen possible values, a larger space than
  the eight item-transform slots above — and resolves it against the unit's
  own graphics-override chain if one exists, falling back to the unit's base
  graphics record otherwise. One resolved value, the four-character constant
  `0x2074696C` ("lit " read as bytes), short-circuits to "no transform" —
  a literal sentinel this pass did not chase further.
- **`GetUnitGlowType`** (`@ 6fb1dfe0`) checks a small set of state values
  (5, 6, and 7 among them) against the unit's current mode and, when one
  matches, writes a glow-type code (1 or 2) that the caller uses to decide
  *whether* a unit glows at all, before any transform table is chosen.
- **`GetUnitRenderColorByMode`** (`@ 6fb02eb0`) dispatches on a render mode
  read from the unit: mode 8 routes to a fade-color calculation
  (`CalculateFadeColorValues`), modes `0x6B`–`0x6C` route to
  `GetUnitHighlightColor`, and every other mode falls through to a single
  fixed RGB-and-alpha default read from four global bytes
  (`DAT_6fbc9610`–`DAT_6fbc9616`).

Read together, these three answer "does this unit glow, and if so what
colour" as a small state machine layered *on top of* the palette-transform
and colour-tint machinery above, rather than as a fourth independent colour
system — `GetUnitRenderColorByMode`'s fade and highlight paths are the most
likely consumers of the tint table's 12 rows and the alpha-blend tables'
three ratios, though this pass did not trace a direct call edge from either
into the `Pal.PL2` buffer to confirm it. That gap — the exact wire from "unit
is glowing" to "which offset in `Pal.PL2` gets read" — is the single largest
open question this chapter leaves for a future pass.

## Recoloring without breaking the shared tables

Pulling every mechanism above into one answer to "how do I safely recolor
something":

- **A screen palette (`pal.dat`) and its transform buffer (`Pal.PL2`) travel
  together.** `LoadLoadingPaletteFiles` (`@ 6fb6e020`) builds *both* paths
  and passes them to a single loader call in one step — this pass observed
  that dual-path call directly, though it did not trace whether the engine
  ever regenerates `Pal.PL2` from `pal.dat` on its own at load time (the
  code to do so, `BuildPaletteTransformTables`, demonstrably exists and
  demonstrably reproduces the shipped file exactly). Do not assume it will:
  edit the `.dat`, and either regenerate a matching `.pl2` by the formulas
  above or replace it with one, rather than shipping a `.dat` whose
  `Pal.PL2` was computed from different colours.
- **`palshift.dat` is scoped to one monster token.** At 2,048 bytes and a
  confirmed per-folder existence (70 of 210 tokens), editing one is about as
  local a change as this chapter covers — it cannot touch any other
  monster's colour.
- **The eight item-transform files are global and shared.** Every item whose
  `Transform` or `InvTrans` column names a slot shares that slot's file with
  every other item that names the same slot — up to 140 vanilla records for
  one file. Slot 6 (`invgrey.dat`) is confirmed unclaimed by any vanilla
  armor or weapon row and is the safest slot to repurpose. Slots 3 and 4
  (`gold.dat`, `brown.dat`) are dead in retail — the accessor function
  rejects both indices outright — so editing them changes bytes nothing
  reads; reaching a genuinely new "gold" or "brown" item look needs a
  different route than the numeric `Transform` column.
- **Unique items use a separate, string-coded system** (`chrtransform`/
  `invtransform` in `uniqueitems.txt`) with a larger, 21-name vocabulary this
  pass did not resolve to a mechanism. Do not assume the numeric-slot
  guidance above applies to uniques; verify separately before editing one.

## Reference: function addresses

| Function | Address | Module | Role |
|---|---|---|---|
| `LoadPaletteFile` | `6fe1a2e0` | D2CMP.dll | Loads a 768-byte `.dat` or falls back to PCX |
| `LoadPCXPaletteFromFile` | `6fe19b00` | D2CMP.dll | Reads a palette from a classic 256-colour `.pcx` |
| `BuildPaletteTransformTables` | `6fe1b930` | D2CMP.dll | Builds the full 443,175-byte transform buffer from a loaded palette |
| `InitPaletteTransformTables` | `6fe1ba00` | D2CMP.dll | Sets the buffer's size constant and calls the builder above |
| `GenerateDarknessPaletteTable` | `6fe19e60` | D2CMP.dll | 32-level distance-darkening table |
| `GenerateHighlightPaletteTable` | `6fe19dc0` | D2CMP.dll | 16-level brighten-toward-white table |
| `AdjustPaletteSaturation` | `6fe1aad0` | D2CMP.dll | Single saturation-boosted palette variant |
| `GenerateAlphaBlendTables` | `6fe1a070` | D2CMP.dll | Three full 256×256 alpha-blend LUTs |
| `GenerateAdditiveBlendTable` | `6fe1a640` | D2CMP.dll | Full 256×256 additive-blend LUT |
| `GenerateMultiplyBlendTable` | `6fe19fa0` | D2CMP.dll | Full 256×256 multiply-blend LUT |
| `GenerateColorTransformTables` | `6fe1ab60` | D2CMP.dll | Hue-rotation / lightness / saturation table family |
| `GenerateScreenBlendTable` | `6fe1a490` | D2CMP.dll | Full 256×256 screen-blend LUT plus a self-blend ramp |
| `GenerateTintPaletteTable` | `6fe19ed0` | D2CMP.dll | 12 named colour-tint tables |
| `GetPaletteBrightnessTable` | `6fe24ec0` | D2CMP.dll | Resolves an item-transform slot + brightness level to a table pointer |
| `FindNearestPaletteColor` | `6fe19d30` | D2CMP.dll | Snaps a computed RGB value to the nearest of the 256 loaded colours |
| `ConvertRGBToHSLPalette` | `6fe1a830` | D2CMP.dll | Full-palette RGB→HSL conversion, used by the hue-rotation family |
| `ConvertHSLToRGBPalette` | `6fe1a710` | D2CMP.dll | The inverse conversion |
| `LoadItemPaletteFile` | `6fe24f20` | D2CMP.dll | Loads one of the 8 named item-transform files into its numbered slot |
| `LoadAllItemPaletteTransforms` | `6fe250d0` | D2CMP.dll | Loads all 8 item-transform files in slot order 1–8 |
| `LoadLoadingPaletteFiles` | `6fb6e020` | D2Client.dll | Builds and passes the loading screen's `.dat` + `.pl2` paths together |
| `InitializePaletteShiftSystem` | `6fb038f0` | D2Client.dll | Allocates the per-skill palette-shift array |
| `LoadPaletteShiftData` | `6fb03850` | D2Client.dll | Loads the shared default palette-shift resource |
| `LoadMonsterPaletteShiftFile` | `6fb03c30` | D2Client.dll | Loads one monster's `palshift.dat` and registers it by skill id |
| `ApplyPaletteShiftTransform` | `6fb03b20` | D2Client.dll | Re-maps a loaded `palshift.dat` through the shared default table |
| `GetSkillPaletteData` | `6fb03a30` | D2Client.dll | Looks up a skill's registered palette-shift/hue-rotation data at render time |
| `GetUnitPaletteTransform` | `6fb1dee0` | D2Client.dll | Resolves a unit's active palette-transform slot (0–15) |
| `GetUnitGlowType` | `6fb1dfe0` | D2Client.dll | Decides whether, and how, a unit glows |
| `GetUnitRenderColorByMode` | `6fb02eb0` | D2Client.dll | Picks fade/highlight/default colour by render mode |
| `GetComponentListColorIndex` | `6fb1d690` | D2Client.dll | Reads a component's stored colour index (see COF chapter, Stage 3) |
| `CopyItemColorComponents` | `6fb1d840` | D2Client.dll | Copies an item's colour-component bytes for its rendered layer |

## Version differences

| What | 1.13c | 1.09d |
|---|---|---|
| `pal.dat` (256×3 RGB, 768 B) | present, this layout | identical file — same SHA-256 as 1.13c for `ACT1\pal.dat` |
| `Pal.PL2` (443,175 B transform buffer) | present, this layout | identical file — same SHA-256 as 1.13c for `ACT1\Pal.PL2` |
| `palshift.dat` (2,048 B, 8×256) | present for 70/210 monster tokens | `SK\cof\palshift.dat` checked — identical SHA-256 to 1.13c |
| Item transform files (8×5,376 B) | present, this naming | present, this naming, in the archive |
| `D2CMP.dll` function addresses above | as listed | not separately mapped this pass |

Every file-level comparison above came back byte-identical between 1.09d and
1.13c wherever it was checked (`ACT1`'s `pal.dat` and `Pal.PL2`, and the
Skeleton's `palshift.dat`) — this establishes that the **data format** is
unchanged across these two patches, not that every function lives at the
same address; 1.09d's `D2CMP.dll` was not separately imported and mapped in
this pass, so the address table above is 1.13c-specific and not asserted for
1.09d.

---

*Companion report, with the full claim ledger, every reproduction run's exact
pass/fail counts, and everything left open: [palettes-and-colour.verification.md](palettes-and-colour.verification.md).*
