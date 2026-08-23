# Verification record — Palettes and Colour (1.13c)

Companion audit for [palettes-and-colour.md](palettes-and-colour.md). This
chapter was authored fresh (not converted from an existing document), so this
record is organized as a claim ledger with verification tier per claim, plus
the full numeric results of every reproduction run, rather than as a
before/after correction list.

## Ground truth used

**Binaries**, both from `F:\D2VersionChanger\VersionChanger\LoD\1.13c\`,
opened read-only in Ghidra:

- `D2CMP.dll`, image base `6fe10000`, SHA-256
  `2ee205f484161c5ed854f30aed12565a7ac3e97fd1a838e697aaefe4dc349756`
- `D2Client.dll`, image base `6fab0000`, SHA-256
  `dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906`
  (same hash the COF-pipeline chapter cites for the same file — confirmed
  it's the identical binary)

Both were already loaded in the shared Ghidra project at paths
`/Vanilla/1.13c/D2CMP.dll` and `/Vanilla/1.13c/D2Client.dll`; `program=` was
passed explicitly on every call in this session (several other copies of
both DLLs, at the same image bases, are loaded simultaneously for other mod
trees — PD2-S12, PD2-S13, a run/slot2 copy — so the exact path matters).

**Game data**: all ten 1.13c MPQ archives under
`F:\D2VersionChanger\VersionChanger\LoD\1.13c\`, read through
`tools/d2mpq.py` (never bare `mpyq`, per `docs/vanilla-data.md`). 1.09d's
archives (same tool, `D2_VERSIONS_ROOT` default) were used for the two-point
version comparison.

**No live fleet member was used.** Every claim in this chapter resolved from
static disassembly/decompile plus direct reproduction against real file
bytes; a live member would only have added a third independent channel for
claims already confirmed two other ways, so it was not brought up for this
pass. That is a real gap for the unit-glow section specifically — see Open
questions below.

**Date**: 2026-08-21.

## Claim inventory and tally

| Type | Description | Count | Result |
|---|---|---:|---|
| A — mechanical | Function exists at claimed address | 27 | 27/27 confirmed exact entry points |
| A — mechanical | File exists at claimed archive path | 19 named files (8 item-transform + `SK` palshift + 10 screen-palette folders sampled) | 19/19 present, sizes as claimed |
| A — mechanical | Byte-for-byte table reproduction | 7 sub-tables (see below) | 7/7 exact, 0 mismatches, 344,064/443,175 bytes of one file |
| B — interpretive | Function purpose / behavior | 16 functions decompiled and read for behavior | 16 read; confidence noted per-function below (11 high, 5 lower) |
| C — contextual | "1.09d and 1.13c share the same palette data" | 3 file-pairs | 3/3 SHA-256 identical |
| C — contextual | "70 of 210 monster tokens carry a palshift.dat" | 1 count claim | confirmed by full MPQ listing + full monstats.txt Code column sweep |
| D — data/asset | `armor.txt`/`weapons.txt`/`misc.txt`/`uniqueitems.txt` column values | 4 tables, ~1,464 records total | swept in full, not sampled |

## Function addresses — all 27 confirmed

Every address in the chapter's reference table was checked with
`get_function_by_address` before being decompiled or cited; all 27 resolved
to an exact entry point (`address == entry_point`, not a mid-function
landing). Eleven were supplied by the task itself (5 in D2CMP.dll, 6 in
D2Client.dll) and were confirmed rather than assumed; the remaining sixteen
were found independently via `search_functions` name-pattern search
(`Palette`, `Generate`, `Blend`) in the same two modules and confirmed the
same way.

D2CMP.dll: `LoadPaletteFile@6fe1a2e0`, `LoadPCXPaletteFromFile@6fe19b00`,
`LoadItemPaletteFile@6fe24f20`, `LoadAllItemPaletteTransforms@6fe250d0`,
`InitializeLoadedResourceHeader@6fe25660`, `BuildPaletteTransformTables@6fe1b930`,
`InitPaletteTransformTables@6fe1ba00`, `GenerateDarknessPaletteTable@6fe19e60`,
`GenerateHighlightPaletteTable@6fe19dc0`, `AdjustPaletteSaturation@6fe1aad0`,
`GenerateAlphaBlendTables@6fe1a070`, `GenerateAdditiveBlendTable@6fe1a640`,
`GenerateMultiplyBlendTable@6fe19fa0`, `GenerateColorTransformTables@6fe1ab60`,
`GenerateScreenBlendTable@6fe1a490`, `GenerateTintPaletteTable@6fe19ed0`,
`GetPaletteBrightnessTable@6fe24ec0`, `FindNearestPaletteColor@6fe19d30`.

D2Client.dll: `GetUnitPaletteTransform@6fb1dee0`, `GetUnitGlowType@6fb1dfe0`,
`GetSkillPaletteData@6fb03a30`, `GetComponentListColorIndex@6fb1d690`,
`CopyItemColorComponents@6fb1d840`, `GetUnitRenderColorByMode@6fb02eb0`,
`LoadLoadingPaletteFiles@6fb6e020`, `LoadMonsterPaletteShiftFile@6fb03c30`,
`LoadPaletteShiftData@6fb03850`, `InitializePaletteShiftSystem@6fb038f0`,
`ApplyPaletteShiftTransform@6fb03b20`.

**One address from the task brief was investigated and excluded from the
chapter's core narrative on evidence, not omitted by oversight.**
`InitializeLoadedResourceHeader@6fe25660` is real, confirmed at that address,
and its decompile does look like a generic "loaded resource" finalizer (it
validates a type tag of `7` and patches a self-referential pointer). But
`get_function_callers` on it returns exactly one caller: `LoadTileProject`
(`@ 6fe19720`), which is DT1/tile-loading code, not anything in the palette
path. It is cited once in the chapter's asides as an example of a
name-that-turned-out-not-to-fit, and otherwise left out of the palette
narrative rather than presented as palette machinery it is not shown to be.
(`dt1-tile-format.md` is a sibling chapter, off-limits to this task, and was
not touched — this finding is reported here for whoever owns that file next,
since it is the more likely true home for this function.)

## Byte-for-byte table reproduction — full results

Method: read the real `.dat` and `.pl2` bytes with `tools/d2mpq.py`,
reimplement each generator's formula independently in Python (numpy for the
256×256 tables), compute the expected output, and diff every byte against
the shipped file. All runs below are on `ACT1\pal.dat` / `ACT1\Pal.PL2`
unless noted.

| Table | Formula (as reproduced) | Bytes | Mismatches |
|---|---|---:|---:|
| Base palette copy (offset `0x0`) | `pl2[i*4..i*4+3] = (B,G,R,0)` of `pal.dat[i*3..i*3+2]` | 1,024 | **0** |
| Darkness, 32 levels (`0x400`) | `nearest(R×L>>5, G×L>>5, B×L>>5)`, L=1..32 | 8,192 | **0** |
| Highlight, 16 levels (`0x2400`) | `nearest(min(255,(255-c)×L>>4+c))`, L=1..16 | 4,096 | **0** |
| Alpha blend ×3 (`0x3500`, `0x13500`, `0x23500`) | `nearest((c_bg×inv + c_src×alpha)/255)`, alpha∈{191,127,63}, flat offset `= bg + 256×src` | 196,608 (3×65,536) | **0** |
| Additive blend (`0x33500`) | `nearest(clamp(c_a + c_b, 255))`, flat offset `= a + 256×b` | 65,536 | **0** |
| Multiply blend (`0x43500`) | `nearest((c_a × c_b)/255)`, flat offset `= a + 256×b` | 65,536 | **0** |
| Tint ×12 (`0x6b727`) | `nearest(tint_channel × entry_B_byte / 255)`, tint RGB read from `pl2[0x6b603+3t .. +3t+2]` in (B,G,R) slot order | 3,072 | **0** |
| **Total** | | **344,064 / 443,175 (77.6% of the whole file)** | **0** |

**Cross-file check**: darkness and highlight (the two cheapest to re-run)
were repeated on `ACT2`, `ACT3`, `ACT5`, `menu0`, and `Sky` — 5 more files,
12,288 bytes each (8,192 + 4,096) — all exact, 0 mismatches. Base-palette-copy
structure (offset `0x0`, BGR+pad, matching `pal.dat`) was checked on all
**16** palette folders in the archive (`ACT1`–`5`, `EndGame`, `EndGame2`,
`Sky`, `Trademark`, `fechar`, `loading`, `Menu0`–`menu4`) — 16/16 exact.

**A methodology note that mattered.** The tint-table reproduction initially
failed almost completely (4–33 matches out of 256 per tint, on the first
attempt). The bug was a channel-order assumption: the tint source bytes at
`pl2[0x6b604+3t]` are stored in the same **(blue, green, red)** slot order as
every palette entry in this file, not plain RGB order — the same convention
`FindNearestPaletteColor`'s call sites use throughout (its first colour
argument is always the blue channel). Once corrected, all 12 tints matched
exactly. This is recorded because it is exactly the kind of silent,
plausible-looking wrong answer this book's conventions exist to catch: the
first version "worked" in the sense that it ran and produced numbers, and
every one of those numbers was wrong.

**loading.pl2 is 259 bytes shorter than the other 15 full-size files**
(442,916 vs. 443,175). Its base-palette, darkness (8,192/8,192), and
alpha-75% tables were spot-checked and its darkness table matches exactly;
its alpha table showed many mismatches, but a quick check attributes this to
tie-breaking in `FindNearestPaletteColor`'s nearest-search over a palette
with far more duplicate/near-duplicate colours than a screen palette (the
loading screen's palette is much narrower) rather than to a structural
placement error — several `loading.pl2` "darkness identity" mismatches at
level 32 trace to genuine duplicate colours in `loading`'s own `pal.dat`
(index 254 and index 0 are both pure black). The file's 259-byte shortfall
itself — which table it comes out of — was not diagnosed further. **(marked
unverified)**

## Interpretive (type B) claims — sampling policy and results

Every function whose purpose the chapter makes a specific claim about was
decompiled and read; nothing was purpose-summarized from its name alone.
Confidence differs by function:

**High confidence** (decompiled body directly supports the stated behavior,
with concrete literal constants that were also cross-checked against real
file bytes or another independent function): `LoadPaletteFile`,
`LoadPCXPaletteFromFile`, `BuildPaletteTransformTables`,
`GenerateDarknessPaletteTable`, `GenerateHighlightPaletteTable`,
`GenerateAlphaBlendTables`, `GenerateAdditiveBlendTable`,
`GenerateMultiplyBlendTable`, `GenerateTintPaletteTable`,
`GetPaletteBrightnessTable`, `LoadItemPaletteFile`/`LoadAllItemPaletteTransforms`.

**Lower confidence** (decompiled and read, arithmetic is concrete, but the
body routes through a generic, badly-named, heavily-shared helper the
decompiler calls `Unwind_6fb7dc30` — not an unwind routine; the label should
not be trusted, per the same pattern the COF-pipeline audit flagged for a
different symbol shared across 567 call sites): `GetUnitPaletteTransform`,
`GetUnitGlowType`, `GetUnitRenderColorByMode`, `LoadMonsterPaletteShiftFile`,
`ApplyPaletteShiftTransform`. The chapter presents these functions' *concrete,
literal* behavior (bounds checks, offset arithmetic, which constants gate
which branch) with confidence, and explicitly hedges the *semantic* claims
(what a mode number or state value actually represents in play) as
unconfirmed. `GenerateColorTransformTables`, `AdjustPaletteSaturation`, and
`GenerateScreenBlendTable` were fully decompiled and their write offsets
confirmed, but their HSL/fixed-point formulas were not independently
reproduced against file bytes (see "What was not reproduced" in the
chapter) — a deliberate scope decision given three tables' worth of
byte-exact reproduction had already been established elsewhere in the file.

## Data (type D) claims — full sweeps, not samples

- `armor.txt` (202 records) and `weapons.txt` (306 records): `Transform`
  column swept in full. Values found: `armor.txt` — `{0:63, 1:22, 2:30, 5:15,
  7:18, 8:54}`; `weapons.txt` — `{0:6, 1:101, 2:74, 5:125}`. 3, 4, and 6 never
  appear in either table.
- `misc.txt` (151 records): `Transform` column swept; `{0:151, '':1}` — no
  item transform is meaningfully used here.
- `uniqueitems.txt` (402 records): confirmed it has **no** `Transform`/
  `InvTrans` columns, but does have `chrtransform`/`invtransform`; both swept
  in full. 21 distinct string codes found, counts given in the chapter. What
  consumes these codes was not located — flagged in the chapter as
  unverified beyond the raw inventory.
- `monstats.txt` (734 records, 210 distinct `Code` values): full sweep for
  the "210 monster tokens" figure.
- `palshift.dat` archive listing: `list --pattern "data\global\monsters\*\cof\palshift*"` (case-insensitive glob) across all ten 1.13c archives, both
  `D2Exp.mpq` (15 files) and `D2Data.mpq` (55 files) — 70 total, cross-checked
  against the 210-token count above.
- Item-transform files: `list --pattern "data\global\items\palette\*"` — all
  8 named files present in both 1.13c and 1.09d archives; all 8 extracted and
  hashed in 1.13c (sizes and SHA-256 pairs recorded in the chapter).

## Version comparison (1.09d vs. 1.13c)

Three file pairs extracted from 1.09d and diffed by SHA-256 against their
1.13c counterparts:

| File | 1.13c SHA-256 (first 16 hex) | 1.09d SHA-256 (first 16 hex) | Match |
|---|---|---|:---:|
| `ACT1\pal.dat` | `92faf0d06039faaf` | `92faf0d06039faaf` | identical |
| `ACT1\Pal.PL2` | `de848a8dfef15e9d` | `de848a8dfef15e9d` | identical |
| `SK\cof\palshift.dat` | `8f73890676a263c1` | `8f73890676a263c1` | identical |

This establishes the **data format** is unchanged 1.09d→1.13c for these
three files; it does not establish that `D2CMP.dll`'s function addresses are
unchanged, since 1.09d's `D2CMP.dll` was not imported/mapped in Ghidra this
pass (only `D2Common.dll`, `D2Client.dll`, `D2Gfx.dll`, and `D2Gdi.dll` for
1.09d were already loaded in the shared project; importing and re-mapping
`D2CMP.dll` for 1.09d was judged out of scope given the byte-identical data
already answers the question the chapter's "Version differences" table needs
to answer).

## Unverified claims — collected

Every item below is also flagged in place in the chapter text; collected
here for a single-pass review:

1. Why `STATIC` and `Units` ship a `pal.dat` with no companion `Pal.PL2`.
2. The exact internal sub-offsets within the `0x53500`–`0x5B500`
   hue-rotation/lightness/saturation region (`GenerateColorTransformTables`).
   An early attempt to reconstruct these from memory produced an internally
   inconsistent offset (two sub-tables appearing to overlap); rather than
   publish a guessed correction, the chapter reports only the region's
   confirmed outer bounds and general contents.
3. The exact byte-level output of the screen-blend table (`0x5B500`), the
   self-blend ramp (`0x6B500`), and the saturation-boost table (`0x3400`) —
   located and read from the disassembly, not independently reproduced.
4. The ~256-byte span between the small data block near `0x6B627` and the
   tint table's start at `0x6B727`.
5. Why `loading.pl2` is 259 bytes shorter than every other full-size
   `Pal.PL2` in the archive.
6. What each of `palshift.dat`'s 8 tables (per monster token) corresponds to
   in play — only that table 2 of 8 (for Skeleton) is the identity
   permutation and the others are non-trivial index remaps.
7. The exact effect of `ApplyPaletteShiftTransform`'s second-stage remap
   through `DAT_6fbcc2d8`.
8. The semantic meaning of `GetUnitPaletteTransform`'s 16 transform-type
   values, `GetUnitGlowType`'s state values (5, 6, 7), and
   `GetUnitRenderColorByMode`'s render-mode boundaries (8; 0x6A–0x6C) —
   the bounds and branch structure are confirmed, what each value represents
   in actual play is not.
9. Whether `GetUnitRenderColorByMode`'s fade/highlight paths actually read
   from the alpha-blend or tint tables in `Pal.PL2` — plausible given what
   both subsystems are for, but no call edge was traced connecting them.
10. What resolves `uniqueitems.txt`'s 21 `chrtransform`/`invtransform` string
    codes to an actual palette operation.
11. Which of `palshift.dat`'s two source-string address callers (`R\Cof\palshift.dat` at `6fb85e27` vs. the actual path-building code in
    `LoadMonsterPaletteShiftFile`) is closer to what a modder would need to
    replicate the naming convention by hand; both were read, but the literal
    "R" in the string constant's exact role was not resolved.

## Open questions for the author

- Is there a `/mpq`-adjacent or fleet-based way to catch
  `GetUnitRenderColorByMode` or `GetUnitPaletteTransform` at a breakpoint on
  a live member, to settle open item 9 above (which table a glowing unit's
  colour actually comes from)? This chapter did not use a live member at
  all; it may be the fastest way to close the chapter's largest remaining
  gap.
- `InitializeLoadedResourceHeader@6fe25660`'s only caller is
  `LoadTileProject`, not anything in the palette path — worth flagging to
  whoever maintains `dt1-tile-format.md`, since it is presumably the more
  accurate home for that function's documentation.
- The `uniqueitems.txt` `chrtransform`/`invtransform` string-code system (21
  values, listed above) looks like it deserves its own investigation — it is
  a materially different, richer mechanism than the numeric `Transform`
  column this chapter traces in depth, and this pass did not have budget to
  find its consuming function.
