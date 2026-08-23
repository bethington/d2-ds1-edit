# Verification report — *Adding ANY Monsters and ANY Objects to a DS1*

Companion to [`monsters-and-objects.md`](monsters-and-objects.md).
Verification date: **2026-08-21**. Two passes are recorded here: the original
claim-by-claim check (below, under [Ground truth
used](#ground-truth-used) onward), and this same-day follow-up pass that
re-centred the chapter on 1.13c and re-checked its data claims against a
second, independent vanilla extraction.

---

## This pass: re-centering and re-verification against vanilla

### Re-centering

The chapter previously led with 1.09d/1.10 material and treated 1.13c as a
correction layered on top — 1.09d appeared 31 times, 1.10 28 times, 1.13c 25
times. Per this book's binding conventions (`docs/book-conventions.md` §1),
that is inverted: **1.13c is now the body**, written present-tense with no
qualifier, and the pre-1.10 material survives as `> **Version note (1.09d and
earlier):**` callouts placed immediately after the claim they modify, plus a
closing [`## Version differences`](monsters-and-objects.md#version-differences)
table with 1.13c as the first data column. After the rewrite the counts are
1.13c 32, 1.09d 29, 1.10 31 — 1.13c now leads, and every remaining "1.10"
reference is either inside a `Version note` (marking when the *current*
mechanism began) or is incidental historical color in Siramy's own
walkthrough text, not a competing default.

No fact changed as part of re-centering. The chapter's old evidentiary
blockquotes (`**Verified**`, `**Correction**`, `**Refinement**`, `**Added**`,
`**Unverified**`) were folded into flowing prose — book-conventions §4 asks
that evidence be cited "inline and briefly," not boxed off in a parallel
annotation scheme, and reserves blockquotes for the two conventions define:
`Version note` and `Mod note`. The two prior corrections (the Type 2 table
surviving into 1.13c; the Type 1 trick being only half-dead) are now stated as
the 1.13c-first body fact, with the pre-1.10 contrast in a `Version note`
rather than a `**Correction**` box. Genuinely unverifiable claims are now
marked inline as `(unverified: …)` per §4, rather than boxed as
`**Unverified**`. Section headers changed twice for the same reason: "Hardcoded
tables in DLL" is now "Resolving a DS1 unit's ID" (the 1.13c body describes a
table that is *partly* still hardcoded, partly moved to `MonPreset.txt` — the
old title asserted the pre-1.10 architecture as the default), and "Softcoded
Type 1 table in the patch 1.10" is now "MonPreset.txt: the Type 1 lookup
table" (naming what it *is*, not when it arrived). No image, ZIP reference,
or worked-example value changed; the same 21 screenshots sit at the same
narrative points.

### Re-verification against vanilla

The original pass (below) already sourced its game-data tables from
`Patch_D2.mpq` / `D2Exp.mpq` / `D2Data.mpq` via a bespoke session-scratchpad
reader (see [Tooling built for this run](#tooling-built-for-this-run)), and
explicitly rejected `assets/excel/objects.txt` as contaminated — so there was
reason to expect no drift, but the brief for this pass was to check anyway,
using the book's now-canonical `tools/d2mpq.py` rather than trust that the
scratchpad tool and the canonical one agree.

Every table the chapter cites was re-extracted through `tools/d2mpq.py tables`
into `.vanilla-cache/`, for both 1.13c and 1.09d, and every record-level claim
in the chapter was re-derived from the extracted text with an independent
parsing script (not reused from the first pass) and diffed against what the
chapter already said:

| Table | 1.13c records | 1.09d records | Source archive (1.13c / 1.09d) |
|---|---:|---:|---|
| `objects.txt` | 573 | 573 | `Patch_D2.mpq` / `D2Exp.mpq` |
| `monstats.txt` | 734 | 575 | `Patch_D2.mpq` / `Patch_D2.mpq` |
| `monpreset.txt` | 229 | absent | `Patch_D2.mpq` / — |
| `monplace.txt` | 37 | absent | `Patch_D2.mpq` / — |
| `superuniques.txt` | 66 | 66 | `Patch_D2.mpq` / `Patch_D2.mpq` |
| `lvlprest.txt` | 1091 | not re-extracted (no version drift claimed) | `Patch_D2.mpq` |
| `lvltypes.txt` | 36 | not re-extracted | `Patch_D2.mpq` |
| `levels.txt` | 137 | not re-extracted | `Patch_D2.mpq` |

All eight match `docs/vanilla-data.md`'s known-good counts and `d2mpq.py
selfcheck --version 1.13c`, which was run first and passed (8/8 tables at
known-good counts, zero PD2 mod markers found).

Record-level claims re-checked against these extractions, with the table that
settled each:

- **`monstats.txt` (1.13c)** — every `hcIdx` → `Id`/`NameStr` pair the chapter
  cites: 47→`corruptrogue5`/FleshHunter, 48→`baboon1`/DuneBeast,
  59→`fallenshaman2`/CarverShaman, 146→`cain1`/DeckardCain, 147→`gheed`/Gheed,
  148→`akara`/Akara, 149→`chicken`/dummy, 177→`drognan`/Drognan,
  243→`diablo`/Diablo, 652→`bloodlord6`/Blood Lord1 — all ten reproduced
  exactly.
- **`monstats.txt` (1.09d)** — header confirmed to carry no `hcIdx` column
  (`Class, namco, Type, Descriptor, BaseId, PopulateId, Spawned, Beta, …`),
  575 records, confirming the `hcIdx`-anachronism claim about the 1.09
  example without needing to resolve individual 1.09 rows by name.
- **`objects.txt` (1.13c)** — `Id` 0 = `Dummy`, 74 = `TrappDoor`,
  193 = `LamTome`, 392 = `Seal`, 546 = `ancientsaltar` — all five reproduced
  exactly, including the `Expansion`-divider off-by-one that makes 546 the
  correct ID for a file line numbered 547.
- **`superuniques.txt` (1.13c)** — record 9 = Boneash (`hcIdx` 9, class
  `skmage_pois3`), 20 = The Smith (`hcIdx` 20, `smith`), 39 = The Cow King
  (`hcIdx` 39, `hellbovine`), 40 = Corpsefire (`hcIdx` 40, `zombie1`),
  59 = Frozenstein (`hcIdx` 59, `snowyeti4`) — all five reproduced exactly,
  confirming both the 612-offset pattern behind "652" and the Frozenstein
  worked example.
- **`monpreset.txt` (1.13c)** — per-act row counts recomputed directly from
  the extracted table (not read from a prior claim): Act 1 = 47, Act 2 = 59,
  Act 3 = 39, Act 4 = 28, Act 5 = 56 — matching the 1.09d hardcoded table's
  populated-slot counts exactly, as already claimed. Act 1's first four rows
  (`gheed`, `cain1`, `akara`, `chicken`), Act 1 row 46 (`Corpsefire`), Act 2's
  last row (`skeleton5`, row 58) — all reproduced exactly. This also confirms
  a fact the chapter states but the first pass did not tabulate this way:
  Act 2's row 2 is `drognan`, the same value the pre-1.10 table's entry 62
  held — direct evidence that `MonPreset.txt`'s row order is the old table's
  row order, not a coincidence of the first four rows alone.
- **`lvlprest.txt` / `lvltypes.txt` / `levels.txt` (1.13c)** — `Def` 1
  (*Act 1 - Town 1*, files `TownN1`/`TownE1`/`TownS1`/`TownW1`), `Def` 52
  (*Act 1 - DOE Entrance*, files `DenEnt`/`DenEnt2`), `Def` 529 (*Act 3 -
  Town*, `LevelId` 75, file `DockTown3`); `LvlTypes.txt` 1/2/20 (*Act 1 -
  Town*/*Act 1 - Wilderness*/*Act 3 - Town*); `Levels.txt` 75 (*Act 3 - Town*,
  `LevelType` 20) and 109 (*Act 5 - Town*, i.e. Harrogath) — all reproduced
  exactly.

**Result: zero discrepancies, zero corrections.** Every value the chapter
already cited is the vanilla value under a second, independently-written
extraction and parse. This is a meaningful negative result, not a formality —
`docs/vanilla-data.md` exists precisely because a table checked into this
repository (`assets/excel/objects.txt`) is PD2-derived and silently wrong by
53 records, and this chapter's numbers do not come from that file or its
consequences. Where a claim rests on something other than a table row —
`hcIdx`-anachronism reasoning, the six-monster-to-object fixup, the negative-ID
wraparound arithmetic, the crash-at-object-0 claim — vanilla re-extraction
does not apply and those claims retain whatever status the first pass gave
them, unchanged below.

---

## Origin

| | |
|---|---|
| Author | Paul Siramy |
| Original page | <http://paul.siramy.free.fr/_divers2/tut_any_units_ds1/> |
| Date of original | May 2010 |
| Archived copy | `docs/preservation/siramy/paul.siramy.free.fr/_divers2/tut_any_units_ds1/index.html` (untouched) |
| Manifest entry | `docs/preservation/MANIFEST.tsv`, SHA-256 `74f39b4393b521f45396d25da80ace536600d66dfb93e9244c1edd234b14c43c`, retrieved live |
| Images | `docs/assets/images/01.png` … `21.png`, all 21 present |
| Third-party credit inside the document | **TeknoKyo** (Type 1 behaviour), **SVR** (Type 2 behaviour) |

**Rights status — the position taken.** The archived page carries no licence,
copyright notice, or permission statement of any kind. It is personal
fan-site documentation. The project proceeds on an explicit fair-use judgment
rather than seeking Siramy's permission; the reasoning and its limits are
recorded in [BOOK-STATUS.md](../BOOK-STATUS.md). If Paul Siramy objects, the
terms change.

The conversion to markdown preserved the text verbatim. Changes of form only:
bold pseudo-headings promoted to real markdown headings, the dead `#` anchors in
the table of contents pointed at real section anchors, and the object-ID table
(which the HTML-to-markdown conversion had flattened into a blockquote list)
restored to a markdown table. No sentence of Siramy's was deleted or reworded.

---

## Ground truth used

### Binaries (Ghidra)

| Program | Ghidra path | Image base | Provenance |
|---|---|---|---|
| `D2Common.dll` 1.09d | `/Vanilla/1.09d/D2Common.dll.0` | `6fd50000` | imported for this run from `F:\D2VersionChanger\VersionChanger\LoD\1.09d\D2Common.dll` |
| `D2Common.dll` 1.13c | `/Vanilla/1.13c/D2Common.dll` | `6fd50000` | `F:\D2VersionChanger\VersionChanger\LoD\1.13c\D2Common.dll` |

The 1.09d module was **not loaded at the start of this run** and had to be
imported. Its identity was pinned by hash: the version-changer copy and the
fleet's own 1.09d tree (`F:\D2Fleet\versions\lod-109d\D2Common.dll`) are the
same file, SHA-256
`CF1D735A2D6F6747F31158967466E4F049C7B00562B5DCEDE1565421F4940CD8`, and that
tree's `Game.exe` reports file version `1, 0, 9, 22`.

Three other `D2Common.dll` programs are open at the same image base
(`/PD2Realm/`, `/Mods/PD2-S12/`, and a second 1.13c import with fewer analysed
functions). Every Ghidra call in this run passed an explicit `program=`; no
call relied on the active program, which was neither of the two targets.

### Game data (MPQ)

Read directly out of the archives with a purpose-built reader (see
[Tooling](#tooling-built-for-this-run)):

| Table | 1.09d source | 1.13c source |
|---|---|---|
| `monstats.txt` | `lod-109d\Patch_D2.mpq` | `lod-113c\Patch_D2.mpq` |
| `superuniques.txt` | `lod-109d\Patch_D2.mpq` | `lod-113c\Patch_D2.mpq` |
| `objects.txt` | `lod-109d\D2Exp.mpq` | `lod-113c\Patch_D2.mpq` |
| `monpreset.txt` | **absent from all three archives** | `lod-113c\Patch_D2.mpq` |
| `monplace.txt` | **absent from all three archives** | `lod-113c\Patch_D2.mpq` |
| `lvlprest.txt`, `lvltypes.txt`, `levels.txt` | — | `lod-113c\Patch_D2.mpq` |
| DS1 files | — | `lod-113c\D2Data.mpq`, `lod-113c\D2Exp.mpq` |

The 1.13c tables were cross-checked against the same files pulled from the
1.14b install at `C:\Diablo2` (Game.exe `1.14.1.68`): all seven are
byte-identical between the two, so nothing here depends on which of the two
trees was read. `monpreset.txt` is additionally byte-identical (SHA-1
`d3612b85849b…`, 3,594 bytes) across the `lod-110`, `lod-111`, `lod-112a`,
`lod-113c` and 1.14b trees, which is the same span the document's "un-modded
1.10 patch" claims cover.

**Not used: a live fleet member.** No member was launched and no `/memory`,
`/state`, `/mpq` or `/call` endpoint was used. Every claim in this document is
static — binary code, binary tables, and shipped data files — with two
exceptions, both listed as unverified below. Launching a member was also
outside the brief for this run.

**Also rejected as ground truth:** `assets/excel/objects.txt` in this
repository. It contains `UberAncientAltar`, `ForceShardAltar`, `PVPTome1`,
`Sacrifice Altar` and `LucionChest` — 627 records against vanilla's 573. It is
a modded table and would have produced false "corrections" to three of the four
object IDs. It was discarded in favour of the MPQ.

---

## Claim tally

| Type | Checked | Confirmed as written | Corrected / refined | Contested, doc left standing | Unverified |
|---|---:|---:|---:|---:|---:|
| **A** — mechanical (binary) | 18 | 16 | 2 | 0 | 0 |
| **B** — interpretive | 7 | 4 | 0 | 1 | 2 |
| **C** — contextual | 3 | 0 | 0 | 0 | 3 |
| **D** — data / asset | 33 | 33 | 0 | 0 | 0 |
| **Total** | **61** | **53** | **2** | **1** | **5** |

Nothing was sampled in types A and D; every claim of both was resolved
individually. Type B sampling policy is stated [below](#type-b-sampling-policy).

---

## Corrections applied to the document

*This section records the original pass, unchanged. The chapter's callout
scheme has since changed — see [This pass: re-centering and re-verification
against vanilla](#this-pass-re-centering-and-re-verification-against-vanilla).
Each `**Correction**` / `**Refinement**` / `**Verified**` block named below now
reads as 1.13c-first body prose with the pre-1.10 contrast in a `> **Version
note (1.09d and earlier):**` callout; the facts below are what both forms say.*

### 1. The Type 2 table did not leave D2Common

**Before** — "In D2Common.DLL (before patch 1.10) we can find 2 tables, one for
the Type 1 units, the other for the Type 2 units."

**After** — sentence kept; a **Correction** block added noting that only the
Type 1 table was externalised, and that the Type 2 table is still in 1.13c's
D2Common and still resolves object IDs.

**Evidence** — 1.13c `LoadAndParsePresetArchive` at `6fd5b560` reads the object
table at `0x6fdee2c8`:

```asm
6fd5b80f  CMP  EDI, 0x96
6fd5b815  JGE  0x6fd5b82f
6fd5b817  MOV  EDX, dword ptr [ESP + 0x1c]     ; act
6fd5b81b  IMUL EDX, EDX, 0x96                  ; act*150
6fd5b821  ADD  EDX, EDI                        ; + id
6fd5b823  MOV  EDI, dword ptr [EDX*0x4 + 0x6fdee2c8]
6fd5b82f  SUB  EDI, 0x96
```

All 3,000 bytes at `6fdee2c8` (1.13c) are byte-identical to the 3,000 bytes at
`6fdd35c0` (1.09d), read in two overlapping passes and compared as hex.

Siramy is not contradicted elsewhere — he later writes "there is no equivalent
of MonPreset.txt for the Objects" — but the opening sentence reads as though
both tables were retired in 1.10, and only one was.

### 2. "This trick don't work anymore" is half true

**Before** — "the 1.10 patch changed that behavior, and this trick don't work
anymore".

**After** — sentence kept; a **Refinement** block added distinguishing the two
directions.

**Evidence** — 1.09d has no bounds test on the Type 1 index at all; the
addressing goes straight from `id + act*60` to the load at `6fdd4178`. 1.13c
tests only the upper bound:

```c
if (base != NULL && (int)id < g_dwMonPresetActCount[act]) { ... }
/* no else */
```

A negative `id` satisfies `(int)id < count` and still reads backwards out of the
act's block. So the downward half of the trick still functions in 1.13c — it
just reads MonPreset's rows instead of the hardcoded table's, which is why old
DS1s spawn wrong monsters rather than nothing.

### 3. `hcIdx` in the 1.09 example is an anachronism

**Before** — "…which has the value 652. For some reasons this is not the Monster
with hcIdx from MonStats.txt (bloodlord6)…"

**After** — sentence kept; a **Verified** block added confirming every number in
it and noting that `hcIdx` is a 1.10 column which does not exist in 1.09's
`MonStats.txt`, and that 1.09's `MonStats.txt` has only 575 records, so there is
no record 652 in the version being discussed.

**Evidence** — 1.09d `monstats.txt` header is
`Class, namco, Type, Descriptor, BaseId, PopulateId, Spawned, Beta, …` with no
`hcIdx` column; 575 records after excluding the `Expansion` divider. 1.13c's has
`hcIdx` and 734 records, where `hcIdx` 652 is `bloodlord6` / *Blood Lord1* —
so the parenthetical is a correct statement about the modern table, applied to
an older one.

---

## Contested, document left standing

### "For SuperUnique monsters you have no choice, you \*have\* to use MonPreset.txt"

The decompile suggests otherwise, but the counter-claim was not run in the game,
so Siramy's instruction was left in place and an **Unverified** block was added
beside it.

When a Type 1 ID falls past the end of its act's MonPreset block, 1.13c's parser
leaves the ID untouched and hands it downstream as a unit index. That index
space does not end at MonStats: `MonPreset.txt` rows tagged as superuniques are
themselves turned into `index + nMonStats`, so the downstream consumer must
already accept values in the superunique range. A direct DS1 ID of
`734 + 40 = 774` should therefore place Corpsefire with no MonPreset edit.

This is a prediction from static analysis. It is listed as an open question.

---

## Unverified claims

| # | Claim | Why it could not be settled |
|---|---|---|
| 1 | "in Objects.txt the object with ID 0 make the game crash" | Runtime behaviour. No member was launched. Object record 0 is `Dummy` / *"test data"*, which is consistent with the warning but is not evidence for it. |
| 2 | "you can't use a Type 2 Object ID of exactly 150" | Follows from #1 and inherits its status. The arithmetic (150 − 150 = 0) is confirmed; the consequence is not. |
| 3 | "TeknoKyo discovered how the Type 1 units were now working, and SVR did the same with the Type 2 units" | No local source. Settling it needs 1.10-era Phrozen Keep forum archives, which are not in `preservation/`. |
| 4 | "in fact it was there from the beta release, 1.10s" | No 1.10 beta tree exists in `F:\D2Fleet\versions` (which holds `lod-110` onward). The 1.10 *release* claim is confirmed; the beta claim rests on Siramy's word. |
| 5 | "It also introduced Data\Global\Excel\MonPlace.txt, but this one is better left out" | The introduction is confirmed; the advice is a judgement. It gains indirect support: the `Place` resolver at `6fda2e70` searches the SuperUniques index first, then MonStats, then MonPlace, and 34 of MonPreset's 183 distinct `Place` values are MonPlace codes — so MonPlace names are live in the same namespace and can shadow. Whether that makes them "better left out" is not a checkable statement. |

---

## Type B sampling policy

**No sampling. Full coverage.** The document's entire behavioural surface is one
function per game version, and both were decompiled in full and then confirmed
instruction-by-instruction at every load-bearing site.

Functions decompiled in full:

| Function | Program | Address | Why |
|---|---|---|---|
| DS1 preset parser (unnamed) | 1.09d | `6fd76530` | every pre-1.10 behavioural claim |
| `LoadAndParsePresetArchive` | 1.13c | `6fd5b560` | every 1.10+ behavioural claim |
| `monpreset` table loader | 1.13c | `6fda5490` | per-act base/count globals |
| `Place` column resolver | 1.13c | `6fda2e70` | the three-table index space |
| `MONSTERS_LoadSuperUniquesTable` | 1.13c | `6fda9870` | identity of `nSuperUniques` |

Instruction-level confirmation was taken for the six sites where the decompiler's
constant-folding or symbol naming could not be trusted: the object-table index
(`6fd767fb`, `6fd5b80f`), the monster-table index (`6fd76834`), the 150-subtract
(`6fd5b82f`), and the two global writes in `6fda5490` (`6fda554e`, `6fda5540`).

---

## Why instruction-level confirmation was necessary

Two of the decompiler's own symbol names embed an address that is not the
address the code touches. Anyone reading only the pseudocode would have copied
wrong constants into the document.

| Decompiler renders | Actual address | What it is |
|---|---|---|
| `&g_adwData_6fdef62c_1_` | `0x6fdee2c8` | the Type 2 object table |
| `&g_dwPad_6fdf6304` | `0x6fdf0984` | the per-act MonPreset base pointers |

The decompiler also names by the folded multiplier rather than the real stride:
the Type 1 lookup renders as `(id + act * 0xf) * 4`, which reads as 15 entries
per act. It is 60. The `LEA` chain settles it, and so does the table's extent
(1,200 bytes from `6fdd4178` to the next defined symbol).

Three further Ghidra labels in these functions are spurious and are recorded
here as things to fix in the project, not in the document:

| Ghidra label | What the code actually does |
|---|---|
| `HasItemType3InInventory` | the generic `.txt`/`.bin` table parser — called with `"monpreset"` and with `"superuniques"` |
| `g_dwLastError` | the SuperUniques **record count**; it is the count out-parameter of the `superuniques` parse |
| `GetExperienceTableEntry` | reads the MonPreset per-act base/count globals |

**The circularity trap does not apply to this document.** It contains no
addresses and no function names, so it cannot have been the source of any Ghidra
label here. Every binary claim in the enriched chapter was derived from the
binary and then matched against Siramy's prose, not the other way round.

---

## What verification added

New material in the chapter, all of it derived here rather than taken from the
original:

1. **The 1.13c unit index space.** MonStats `[0, 734)`, SuperUniques
   `[734, 800)`, MonPlace `[800, 837)`, composed explicitly by the `Place`
   resolver's three tags. Tag 2 is pinned to SuperUniques by the pointer
   `MONSTERS_LoadSuperUniquesTable` writes; tag 1 to MonStats by the index the
   same loader uses for the `Class` field; tag 0 to MonPlace deductively — all
   183 distinct `Place` values in `MonPreset.txt` resolve in exactly one of the
   three files with none left over, and `place_nothing` appears only in
   `MonPlace.txt`.

2. **The answer to "for some reasons".** The 1.09 superunique block begins at
   index 612. Four independent entries of the Act 1 block agree on the constant:

   | Table value | Superunique | Record | Difference |
   |---:|---|---:|---:|
   | 621 | Boneash | 9 | 612 |
   | 632 | The Smith | 20 | 612 |
   | 651 | The Cow King | 39 | 612 |
   | 652 | Corpsefire | 40 | 612 |

3. **MonPreset really is the old table.** The 1.09d hardcoded table's populated
   run per act — 47, 59, 39, 28, 56 — equals `MonPreset.txt`'s per-act row
   counts exactly, in all five acts, with no interior gaps; and Act 1's first
   four values (147, 146, 148, 149) name the same four monsters as MonPreset's
   first four Act 1 rows. Siramy inferred this from four rows; it holds for all
   229.

4. **Six monster IDs that are silently objects.** A fixup applied after the
   MonPreset lookup converts monster indexes 297 and 366 (act field 2) and 514,
   537, 538, 539 (act field 4) into `Objects.txt` records 382, 404, 461, 476,
   475, 474. Present in 1.09d and 1.13c with identical constants. This is a real
   hazard for the document's own direct-index method and it was not mentioned.

5. **The `Expansion` divider is not a record.** `Objects.txt`, `MonStats.txt`,
   `SuperUniques.txt`, `Levels.txt`, `LvlTypes.txt` and `LvlPrest.txt` all carry
   a divider line whose ID column is blank, after which the ID column runs one
   behind the raw file line. The game uses the ID column. Proved independently of
   the document: `Levels.txt` data line 110 is *Act 5 - Town* with `Id` 109, and
   Harrogath is level 109. Without this, the document's Ancients Altar row (546)
   would have looked like an off-by-one error; it is correct.

   This is also where this run's own near-miss happened, and it is worth
   recording. Two different parsing helpers were used during the session; one
   excluded divider rows and one did not. The second produced "Frozenstein is
   `SuperUniques.txt` record 60", which went into the first draft of the chapter.
   It is record **59** — `SuperUniques.txt` has its own divider, at data line 42,
   and Frozenstein sits after it. The correct count of 66 superunique records is
   confirmed against the binary: `MONSTERS_LoadSuperUniquesTable` initialises
   `0x42` `hcIdx` slots and aborts if any is unfilled. The error was caught on the
   final read-through, not by the tooling.

6. **The Type 2 table is frozen.** 3,000 bytes, byte-identical 1.09d → 1.13c.

7. **The DS1 act field is zero-based on disk** and clamped with
   `if (act > 4) act = 4`. Confirmed against three real files: `denent.ds1` and
   `townE1.ds1` carry 0, `docktown3.ds1` carries 2.

---

## Full claim log

Only the outcome column matters for skimming; every row was individually
resolved.

### Type A — mechanical

| # | Claim | Outcome |
|---|---|---|
| A1 | Type 1 hardcoded table exists in pre-1.10 D2Common | confirmed — `6fdd4178`, 1.09d |
| A2 | Type 2 hardcoded table exists in pre-1.10 D2Common | confirmed — `6fdd35c0`; **scope corrected**, still present in 1.13c |
| A3 | Both tables split into 5 per-act parts | confirmed — `act*60` and `act*150` addressing |
| A4 | Type 1: 60 entries per act | confirmed — `LEA` chain at `6fd7682e`/`6fd76831` |
| A5 | Type 1: 300 total | confirmed — 1,200 bytes, ends at the next defined symbol |
| A6 | Type 2: 150 entries per act | confirmed — `LEA` chain at `6fd76806`–`6fd7680f` |
| A7 | Type 2: 750 total | confirmed — 3,000 bytes, ends exactly at the Type 1 base |
| A8 | The DS1's Act value selects the block | confirmed — header field, version ≥ 8 |
| A9 | Act 1 = 0–59 … Act 5 = 240–299 | confirmed |
| A10 | 1.09: out-of-range Type 1 IDs work in both directions | confirmed — no bounds check exists |
| A11 | 1.09 Act 1 entry 46 = 652 | confirmed — `0x0000028C` at `6fdd4230` |
| A12 | 1.09 entry 62 = 177 | confirmed — Act 2 slot 2 |
| A13 | 1.09: Type 2 ID ≥ 150 rejected, negative accepted | confirmed — one signed `JGE` at `6fd76800` |
| A14 | 1.10+: an ID that is not a MonPreset row is used as `hcIdx` | confirmed — fall-through with no `else` |
| A15 | 1.10+: object ID ≥ 150 has 150 subtracted | confirmed — `SUB EDI, 0x96` at `6fd5b82f` |
| A16 | 1.10+: negative object IDs still work | confirmed — signed compare |
| A17 | 1.10 killed the Type 1 out-of-bounds trick | **refined** — upper bound added, lower bound never was |
| A18 | Obj.txt Act 1 rows 47–59 are dead 1.09 entries | confirmed — 13 zero dwords at `6fdd4234`–`6fdd4267` |

### Type B — interpretive

| # | Claim | Outcome |
|---|---|---|
| B1 | MonPreset.txt is the old hardcoded table, moved to a TXT | confirmed — 5/5 per-act counts, identical leading values |
| B2 | 1.09 DS1s using the trick break under 1.10 | confirmed, with mechanism (and silently) |
| B3 | SuperUniques *must* go through MonPreset.txt | **contested**, doc left standing — see above |
| B4 | Object ID 0 crashes the game | unverified (runtime) |
| B5 | Type 2 ID of exactly 150 is unusable | unverified (depends on B4) |
| B6 | Subtracting 150 reaches *all* of Objects.txt | confirmed — no upper clamp on the subtract path |
| B7 | Type 2 IDs below 150 are table entries, not Objects.txt IDs | confirmed |

### Type C — contextual

| # | Claim | Outcome |
|---|---|---|
| C1 | TeknoKyo / SVR discovered the 1.10 behaviours | unverified — no local source |
| C2 | The feature shipped in the 1.10s beta | unverified — no beta tree |
| C3 | MonPlace.txt is "better left out" | unverified as advice; mechanism noted |

### Type D — data / asset

| # | Claim | Outcome |
|---|---|---|
| D1 | MonPreset.txt introduced in 1.10 | confirmed — absent from every 1.09d archive; absent as a string in 1.09d D2Common; present and identical 1.10 → 1.14b |
| D2 | MonPlace.txt introduced in 1.10 | confirmed — same two-source check |
| D3 | MonPreset Act 1 has 47 entries | confirmed |
| D4 | MonPreset Act 1 rows 0–3 are gheed, cain1, akara, chicken | confirmed |
| D5 | hcIdx 147/146/148/149 = gheed/cain1/akara/chicken | confirmed |
| D6 | hcIdx 177 = Drognan | confirmed |
| D7 | hcIdx 652 = bloodlord6 | confirmed for 1.13c; anachronistic for 1.09 (see correction 3) |
| D8 | Corpsefire is a SuperUnique | confirmed — `SuperUniques.txt` record 40 |
| D9 | MonPreset Act 1 row 46 = Corpsefire | confirmed |
| D10 | hcIdx 47 = FleshHunter | confirmed — `corruptrogue5` |
| D11 | hcIdx 48 = DuneBeast | confirmed — `baboon1` |
| D12 | MonPreset Act 2 has 59 rows; row 58 = skeleton5 | confirmed |
| D13 | hcIdx 59 = CarverShaman | confirmed — `fallenshaman2` |
| D14 | Diablo is hcIdx 243, "greater than 38" | confirmed |
| D15 | MonPreset Act 3 has 39 rows, so a new row is #39 | confirmed — last existing row 38 is `Maffer Dragonhand` |
| D16 | Frozenstein is a SuperUnique normally found in Act 5 | confirmed — record 59 (`hcIdx` 59, data line 60), class `snowyeti4`, already MonPreset Act 5 row 46 |
| D17 | denent.ds1 + denent2.ds1 in d2data.mpq, ACT1\CAVES | confirmed — 1,555 bytes each |
| D18 | denent uses LvlPrest 52 | confirmed — *Act 1 - DOE Entrance*, File1/File2 exactly those two |
| D19 | denent uses LvlType 2 | confirmed — `LvlTypes.txt` 2 = *Act 1 - Wilderness* |
| D20 | docktown3.ds1 in d2data.mpq, ACT3\Docktown | confirmed — 118,210 bytes, act field 2 |
| D21 | docktown3 uses LvlPrest 529 | confirmed |
| D22 | docktown3 uses LvlType 20 | confirmed — via `Levels.txt` 75 → `LevelType` 20 → *Act 3 - Town* |
| D23 | townE1.ds1 in d2exp.mpq, ACT1\Town | confirmed — 57,691 bytes (also present in d2data.mpq at the same size) |
| D24 | townE1 uses LvlPrest 1 | confirmed |
| D25 | townE1 uses LvlType 1 | confirmed |
| D26 | The Act 1 Town DS1 has 4 variations | confirmed — TownN1/E1/S1/W1, and no fifth file column |
| D27 | Trap Door, act 2, Objects.txt 74 → DS1 224 | confirmed twice — `TrappDoor`, and it is entry 0 of the Type 2 table's Act 2 block |
| D28 | Lam Esen's Tome, act 3, 193 → 343 | confirmed — `LamTome` |
| D29 | Diablo seal, act 4, 392 → 542 | confirmed — `Seal` |
| D30 | Ancients Altar, act 5, 546 → 696 | confirmed — `ancientsaltar`, object **546** (file line 547) |
| D31 | Object 0 exists in Objects.txt | confirmed — `Dummy`, *"test data"* |
| D32 | Type 1 units come from MonStats / MonPlace / SuperUniques | confirmed — exactly those three, in that index space |
| D33 | Type 2 units come from Objects.txt | confirmed |

---

## Tooling built for this run

`mpyq`, the only MPQ library installed, does not decompress D2's archives: D2
flags its files `MPQ_FILE_IMPLODE` (0x100), and `mpyq` handles only
`MPQ_FILE_COMPRESS` with zlib or bzip2. It does not fail on a D2 file — it
silently returns the concatenated *compressed* sectors, which look like a
successfully-read binary blob. The first four extractions came back as plausible
garbage, and were only caught because a `.txt` file's header row was not text.

Two pieces were written to get past it, and live in the session scratchpad only
— nothing was added to any repository:

- **`explode()`** — the PKWARE Data Compression Library decompressor, transcribed
  from Mark Adler's `blast.c`, including the inverted Huffman code order.
- **`D2Archive`** — a `read()` that applies `explode` per sector, and adds file
  decryption (`MPQ_FILE_ENCRYPTED`, needed for 1.09d's `monstats.txt`) with the
  sector-offset table keyed at `key-1`, each sector at `key+i`, and the non-dword
  tail left in clear, which `mpyq`'s own `_decrypt` discards.

`MPQEditor.exe`'s command-line extraction was tried first and returned nothing,
silently, for both wildcard and exact-path forms — D2's archives carry no
`(listfile)`.

---

## Open questions for the author

1. ~~**Rights.**~~ Settled — see the Origin section above: the project
   proceeds on a fair-use judgment. Is there any correspondence with Paul
   Siramy on record?
2. **Attribution.** Can the TeknoKyo and SVR discoveries be cited to a specific
   forum thread? A dated citation would be worth more than the bare names, and
   these are the only two people the document credits.
3. **The 1.10s beta.** Is a 1.10 beta build obtainable? It is the one version
   claim in the document that no local artefact can reach.
4. **Direct-indexing a SuperUnique.** Does a Type 1 unit with ID 774 place
   Corpsefire on 1.13c? One DS1 edit and one `-direct -txt` launch settles it,
   and it decides whether the "you have no choice" sentence needs correcting.
5. **Object 0.** Does it actually crash? Same test, and it decides two claims.
6. **The 37-index band.** In 1.09 the superunique block starts at 612 and
   `MonStats.txt` ends at 575, leaving 37 indexes; `MonPlace.txt` shipped in 1.10
   with exactly 37 rows. The correspondence is arithmetic, not decompiled — the
   1.09 consumer of the index lives outside `D2Common.dll` and was not traced.
   `D2Game.dll` 1.09d is not currently loaded in Ghidra.
7. **The Ancient Statue permutation.** The act-5 fixup maps monster
   `ancientstatue1` → object *Ancient Statue 2* and `ancientstatue2` → *Ancient
   Statue 1*, via `1013 - id`. Is that a Blizzard off-by-one that nobody ever
   noticed, or do those object names simply not correspond to the monster names?
   Deciding it needs the art, not the tables.
8. **The example ZIPs.** Three downloads are referenced by name and none is
   archived. Should they be recovered into `preservation/`, or should the
   references become explicit dead links?
