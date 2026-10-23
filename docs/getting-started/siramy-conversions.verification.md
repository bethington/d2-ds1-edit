# Verification report — three Siramy conversions

Companion report for three documents recovered from the preserved
`paul.siramy.free.fr` archive:

| Modernized document | Archived original |
|---|---|
| [`docs/getting-started/manual.md`](manual.md) | `_divers/ds1/doc/index.html` |
| [`docs/getting-started/overview.md`](overview.md) | `_divers/ds1/dl_ds1edit.html` |
| [`docs/tutorials/01-basic-map-editing.md`](../tutorials/01-basic-map-editing.md) | `_divers/ds1/doc/tut01/index.html` |

**Verification date:** 2026-08-21 (first pass); **updated 2026-08-21** (second
pass — 1.13c re-centring and vanilla re-verification); **updated 2026-08-21**
(third pass — `manual.md` only: vanilla `.ds1` corpus and Ghidra, §-1 below,
this update).

---

## -1. Third pass — `manual.md` re-sourced to vanilla data and the game's loader

**Scope: `manual.md` only.** `overview.md` and `01-basic-map-editing.md` are
untouched by this pass.

**Why this pass happened.** The first two passes settled `manual.md`'s *format*
claims against two sources: this repository's parser, and the 2,666-file tile
tree under `assets/tiles/`. Both passes flagged the weakness in writing —
§4.2 Caveat 1 records that the parser is Siramy-derived and so is not an
independent witness — but neither pass could do anything about it, because
neither used a disassembler. This pass did, and it also discovered that the
*data* half of the evidence had the same defect as `assets/excel/` did before
the second pass: `assets/tiles/` is a working set, not vanilla.

**Ground truth added this pass.**

| Source | What it settled | Detail |
|---|---|---|
| **[G]** Ghidra, vanilla 1.13c `D2Common.dll` | The version gates, the cell bit-fields, the special-orientation test, and the path-matching rule — read out of Blizzard's code, owing Siramy nothing | Program `/Vanilla/1.13c/D2Common.dll`, image base `6fd50000`, 2,764 functions. DS1 reader `LoadAndParsePresetArchive` @ `6fd5b560`–`6fd5bbb1`; cell decoder `ProcessPresetRoomObjectsAndTiles` @ `6fd5c060`–`6fd5c678`. Blizzard's own source path for this code survives at `6fde6eb0`: `..\Source\D2Common\DRLG\Preset.cpp` |
| **[D]** all **2,276 distinct vanilla 1.13c `.ds1` files** | Every empirical count in the chapter | Read with `tools/d2mpq.py` (`selfcheck` passed first) through the MPQ chain in search order — `Patch_D2.mpq` → `D2Exp.mpq` → … → `D2Data.mpq`. All 2,276 parsed with zero stream desync by a Python port of `src/core/ds1.c` written for this pass |

**Independence check on the port.** The Python parser is a port of
`src/core/ds1.c`, so it inherits that file's Siramy ancestry and is *not* an
independent witness to the layout. It is used here only to count occurrences of
bit patterns whose *meaning* is established separately by **[G]**. As a
cross-check, its output was compared against the independently-written census in
[`ds1-map-format.md`](../guides/ds1-map-format.md), which used the same vanilla
corpus: the two agree exactly on file count (2,276), Hidden per layer
(2 371 / 86 / 145 / 142 / 4 733 / 1 957 / 7), IsShadow (15 146), path totals
(17 files, 136 blocks, 511 points) and the action histogram
(328 / 105 / 43 / 34 / 1).

### -1.1 Corrections made to `manual.md`

Every empirical figure in the chapter changed, because every one of them had
been computed over `assets/tiles/` (2,666 files, 117 of them byte-different from
vanilla and 390 absent from it) rather than over vanilla's own 2,276.

| Claim | Before (from `assets/tiles/`) | After (vanilla **[D]**) |
|---|---|---|
| Corpus | 2,666 files | **2,276 files** |
| Hidden bit, wall cells | 10,562 | **2,744** (2 371 / 86 / 145 / 142) |
| Hidden bit, floor cells | 25,370 | **6,690** (4 733 / 1 957) |
| Hidden bit, shadow cells | *not counted* | **7** |
| Unwalkable, wall cells | 22,251 | **9,173** (9 062 / 26 / 65 / 20) |
| Unwalkable, floor cells | 940,661 | **152,700** (152 515 / 185) |
| Unwalkable, shadow cells | *not counted* | **150** |
| Wall-cell population | "over 17.5 M" | **1,493,661** slots, 173,437 occupied |
| Floor-cell population | "over 9.5 M" | **1,254,392** slots, 696,999 occupied |
| Orientation records | 17,530,587 | **1,493,661** (upper 3 bytes zero in all — claim holds) |
| Orientation 10 | 799× | **654×** |
| Orientation 11 | 301× | **285×** |
| Files using orientation 10/11 | 546 | **476** |
| Path data | 138 blocks, 544 points | **136 blocks, 511 points** (17 files, unchanged) |
| `action` histogram | 1 (373×), 2 (102×), 3 (40×), 4 (28×), 5 (1×) | **1 (328×), 2 (105×), 3 (43×), 4 (34×), 5 (1×)** |

**Two substantive claims were corrected, not just renumbered.**

1. **"Dominated by `*warp*.ds1`" — withdrawn.** The chapter said the 10/11
   orientation values were concentrated in files whose names are dominated by
   `*warp*.ds1`. Against vanilla: of the 476 files using those orientations,
   only **65** have `warp` in the name, and they hold **97 of the 939** cells —
   about 10%. The heaviest users are `act2/town/lutN.ds1` (15), `lutW.ds1` (14),
   `lut2.ds1` (12), `arena/tombs/dm2.ds1` (17), `dm3.ds1` (12) and the guild
   houses. The chapter now says warps are a conspicuous use, not the dominant
   one — which is also closer to what Siramy actually wrote, since he never
   ties the value to warps.

2. **The Type-2 path finding shrank from 4 blocks to 2.** This was the
   chapter's headline correction to Siramy ("only Type 1 objects can use
   paths"). The finding **survives** — but two of its four worked examples do
   not exist in vanilla. `ACT2/Town/lutN.ds1` (84,154) and `ACT2/Town/lutW.ds1`
   (83,154) have **no path block at those coordinates** in vanilla 1.13c; they
   came from the non-vanilla tree. The vanilla census of path-block anchors is
   `{type 1: 134, type 2: 2}`, the two being:

   | File | Anchor | Object | Points |
   |---|---|---|---|
   | `act3/docktown/docktown3.ds1` | (219, 38) | Type 2, Id 110 | 3 |
   | `expansion/town/townWest.ds1` | (143, 28) | Type 2, Id 126 | 2 |

### -1.2 Claims newly settled by the binary (previously circular or unverified)

These were previously supported only by the Siramy-derived parser. They now rest
on Blizzard's code.

| Claim | Evidence **[G]** |
|---|---|
| Hidden is bit 31 of the cell dword | `6fd5c338 AND EAX,0x80000000` |
| `main_index` is bits 20–25 | `6fd5c32f SHR ESI,0x14` + `6fd5c335 AND ESI,0x3f` — i.e. Siramy's `(prop3>>4) + ((prop4&3)<<4)` is the same field, expressed byte-wise |
| `sub_index` is bits 8–15 (`prop2`) | `6fd5c332 MOVZX EBX,AH` |
| Orientation 10 and 11 are special-cased by the game | `6fd5c33d CMP ECX,0xb`, `6fd5c346 CMP ECX,0xa` |
| Path block gated at version ≥ 14 | `6fd5bac2 CMP EBX,0xe` / `JL` |
| Third point dword (`action`) gated at version ≥ 15 | `6fd5bb05 CMP EBX,0xf`; stride `6fd5bb08 LEA ESI,[ESI+EBP*0x8]` then `6fd5bb0d LEA ESI,[ESI+EBP*0x4]` |
| Paths keyed by coordinate, no object-type test anywhere | `6fd5baf4 CMP [EDI+0x8],EAX`, `6fd5baf9 CMP [EDI+0x18],ECX` — and nothing else is compared |
| Act field gated at v ≥ 8; `tagType` at v ≥ 10; filenames at v ≥ 3; two skipped dwords for 9 ≤ v ≤ 13; layer counts at v ≥ 4; floor count at v ≥ 16; orientation remap below v 7; groups at v ≥ 12 with `tagType` ∈ {1,2}; extra group dword at v ≥ 18 | `6fd5b58f`, `6fd5b5b9`, `6fd5b5cb`, `6fd5b603`/`6fd5b608`, `6fd5b610`, `6fd5b61a`, `6fd5b6a1`, `6fd5ba0c`/`6fd5ba1d`/`6fd5ba25`, `6fd5ba2e` — the whole version-gate chain of `src/core/ds1.c` reproduced in the game's own loader |

### -1.3 A new finding: the editor is stricter than the game

`src/core/ds1.c:1091-1152` discards a path block outright when two objects share
the anchor cell. The game does not — its match loop takes the **first** node
whose coordinates agree and assigns the path to it (`6fd5bafc JZ`, exiting the
list walk) **[G]**. No vanilla file exercises the difference: **zero** path
blocks in the corpus have two objects on the anchor cell **[D]**. It matters
only for newly authored maps, where the editor will drop paths the game would
have honoured. Added to the chapter.

### -1.4 Also added

- **The tag layer**, which the chapter never mentioned. The file has one when
  `tagType` is 1 or 2; the editor exposes it nowhere, so the F-key row is the
  file's layer model *minus one stream*. Noted where the chapter claims the
  F-keys are "a direct picture of the file format".
- **The version census**, which turned up something the old version table
  implied wrongly: vanilla 1.13c has **no version-14 `.ds1` at all**. Versions
  present are 3 (1), 8 (6), 12 (13), 13 (30), 15 (13), 16 (223), 17 (145), 18
  (1,845). The table's v14 column is a reading of the loader, not of any shipped
  file, and now says so.
- **Layer-count distributions**, replacing the bare assertion that maxima hold:
  walls 0 (51 files), 1 (1,151), 2 (759), 3 (145), 4 (170); floors 0 (3),
  1 (1,724), 2 (549).
- **`IsShadow` given non-circular support**: `prop4 & 0x08` is set on all 15,146
  shadow cells and on no wall or floor cell **[D]**. Previously the label rested
  only on this repository's dialog strings matching Siramy's screenshot.

### -1.5 One discrepancy found in a sibling chapter — since corrected

[`ds1-map-format.md`](../guides/ds1-map-format.md) gave floor-1 Unwalkable as
**152 115**; this pass counts **152 515** over the same 2,276-file vanilla
corpus, with the two censuses agreeing exactly on every other cell in that
table — including floor 1's *Hidden* count of 4 733, produced by the same layer
scan over the same cells, which is what rules out a parse-offset bug on either
side.

**Settled by hand-decode.** `act3\jungle\transl.ds1` was read from raw bytes
with offsets derived independently of both parsers (v18, 65 × 33 = 2 145 cells,
streams `wall1, orient1, floor1, shadow`, floor 1 at byte 17 603): **1 303**
unwalkable floor-1 cells, matching the census, with the computed end of the
layer block landing exactly on a plausible object count. **152 515 is correct**;
the chapter has been fixed and the change recorded as C5 in
[its own report](../guides/ds1-map-format.verification.md).

### -1.6 The Type-2 path question — resolved, statically and then empirically

This was the largest open item. It is now answered both ways, and they agree.

**What the two vanilla instances actually are — and a correction to this
report's own first answer.** An intermediate draft identified the two Type 2
anchors as a *Shrine* and a *hidden stash*, by reading their DS1 `id` as a row
number in `objects.txt`. **That lookup was wrong.** A DS1 object's `id` is not a
row index; it passes through a 5 × 150 table compiled into D2Common at
`6fdee2c8`, `objectsTxtId = table[act * 150 + id]` **[G]** — the table
[`ds1-map-format.verification.md`](../guides/ds1-map-format.verification.md) §7
had already established. The error surfaced when a live member reported the
object under test as `txt_id 385` = `TrappedSoul`, which is not a shrine; the
mismatch was the tell.

Resolved correctly, with the file's own `act` field:

| File | act | DS1 id | → `objects.txt` | `obj.txt` description |
|---|---|---|---|---|
| `act3\docktown\docktown3.ds1` | 3 | 110 | 379 | **"Hratli end"** |
| `expansion\town\townWest.ds1` | 5 | 126 | 543 | **"Larzuk Standard"** |

Both are `Name = Dummy` in `objects.txt` — Blizzard's label for a marker with no
behaviour — and both are **NPC position markers**, the spots where Hratli and
Larzuk stand. So the two stray path blocks are walking routes attached to an
NPC's *marker* rather than to the NPC. The conclusion the wrong names pointed at
survives, and is better supported by the right ones.

**What the game does with them.** The preset object list is walked with the type
field (`+0x14`) as the switch. On the type-2 branch of
`ProcessPresetRoomObjectsAndTiles` (`6fd5c060`) — entered at
`6fd5c590 CMP [ESI+0x14],EDI`, `EDI = 2` — the only things read from the node are
its id (`+0x04`), bounded against the `objects.txt` record count, and one flag
byte inside that record (`6fd5c5c1 IMUL EAX,EAX,0x1c0`,
`6fd5c5c7 TEST [EAX+EDX+0x167],BL`), before the walk steps to `+0x0c`. **The path
pointer at `+0x10` is never read on that branch** **[G]**.

So Siramy's rule of thumb is **right about the game and wrong only about the
reason**: the format permits a Type 2 path and the loader attaches it faithfully,
but nothing downstream ever asks a Type 2 object for one. The constraint lives in
the consumer.

**Scope.** This rests on reading the two functions that walk the preset object
list (`6fd5c060`, `6fd5b320`), not on an exhaustive enumeration of every reader of
`+0x10`. A heuristic sweep of 594 D2Common functions for that field surfaced no
other candidate that both walks the list and reads it, but a regex over
decompiled text is not a proof.

**The runtime experiment — first blocked, then run.** A dedicated live 1.13c
member was used throughout (fleet slot 5; the three pre-existing members were
never touched).

*The first attempt failed and is recorded because the failure is the useful
part.* The plan was to inject a modified `townE1.ds1` via D2's own `-direct`
loose-file switch. It had no effect: a control that blanked the floor-1 layer of
**all thirteen** Act 1 town presets (each `f_num = 1` with an empty floor 2, so
the town ground should have gone black) produced a screen identical to vanilla.
Storm resolves data from the loaded archive set only — which `patches.cdkey.cpp`
had independently measured years earlier. `-direct` is not a path in.

*So the seam was built.* `mpq_override` in d2-fleet hooks Storm's own open and
serves chosen files from `<rundir>\override\`, documented in
[`d2-fleet/docs/MPQ-OVERRIDE.md`](../../../d2-fleet/docs/MPQ-OVERRIDE.md). With it
the experiment ran: `act1\town\townE1.ds1` with one NPC's three-point path block
re-anchored onto the Type 2 object at subtile (164, 81) — act 1, id 110 →
`objects.txt` 385, *"cain start position"*, the same class of NPC position marker
as the two vanilla cases. The served bytes were verified byte-for-byte by reading
the file back out of the running client's own Storm.

**Result, over ninety seconds [L]:**

| unit | what it is | moved? | modes observed |
|---|---|---|---|
| monster #3 | an ordinary path-walking town NPC — positive control | **yes**, 5 distinct positions | `neutral`, **`walk`** |
| object #2 (`txt_id` 385) | the Type 2 marker given the NPC path | **no**, 1 position | `neutral` only |

The control establishes that path-walking was live in that session. The marker
never entered `walk`. Static reading and running game agree: **D2 does not walk a
Type 2 object along a path.**

The member was closed and the override files removed afterwards.

**One thing the live client did settle.** `townE1.ds1` read back through the
running game's own Storm (`/mpq/read`) is **byte-identical** to the copy in this
pass's corpus — same 57,691 bytes, same sha256 `0b6c775d3a01def7…`. The corpus
every count above is drawn from is exactly what a running 1.13c reads.

### -1.7 Still unverified after this pass

- **All tool-UI behaviour** — every key binding and dialog in the chapter. The
  2006 `win_ds1edit` binary is still not in this repository.
- **Runtime behaviour.** Still open: Siramy's claim that an NPC "will only use the
  points on the Pink lines (and randomly), and they'll never go back to their
  original starting position". The Type 2 question that sat here is **now
  settled** — see §-1.6.
- ~~**A way to inject loose game files into a fleet member.**~~ **Built.**
  `mpq_override` (`d2-fleet/patch/patches.mpqoverride.cpp`, docs at
  [`MPQ-OVERRIDE.md`](../../../d2-fleet/docs/MPQ-OVERRIDE.md)) serves files from
  `<rundir>\override\` through a hook on Storm's own open. The class of runtime
  verification this blocked is now open: any claim testable by handing the game
  a modified data file can be put to the game directly.
- **Why Blizzard's maps carry Hidden tiles along their top and bottom edges.**
  The bit is now understood down to the instruction that tests it; the intent
  still is not.

---

## 0. Second pass — 1.13c re-centring and vanilla re-verification

This update covers two of the three documents: `manual.md` and
`01-basic-map-editing.md`. `overview.md` is out of scope for this pass and is
unchanged; its rows below (§3.4, §4.9's download-page portion) still describe
the first pass only.

**What changed and why.** The first pass verified the tutorial's twelve
Excel-table data claims against this repository's checked-in `assets/excel/`
cache — flagged throughout as a caveat, because that cache is
**Project-Diablo-2-derived, not vanilla** (`objects.txt`: 626 records vs
vanilla's 573; see [`docs/vanilla-data.md`](../vanilla-data.md)). This pass
re-checks every one of those claims against genuine **vanilla 1.13c**
archives (`Patch_D2.mpq` / `D2Exp.mpq`), read with `tools/d2mpq.py`, and
additionally against vanilla **1.09d** where useful for a real version
comparison. Both documents are also re-centred on **1.13c as the unmarked
default**, per `docs/book-conventions.md` §1, with a `## Version differences`
table added to each per §3.

**Result: eleven of the twelve claims were confirmed unchanged; one was
corrected; one previously-unverified claim was newly confirmed.**

| Claim | Outcome |
|---|---|
| Level Ids 1, 17, 38, 40 | Confirmed, identical to the PD2-derived check. Also identical on vanilla 1.09d. |
| Act 2 Town `Warp2 = 19`, Id 19 = "Act 2 Town to Sewer Trap" | Confirmed, identical. |
| `Dt1Mask = 959` = File1–10 except File7, → 1023 | Confirmed, identical arithmetic. One nuance: vanilla's File 7 cell holds the literal string `0`, not a blank cell as the PD2-derived cache showed — same meaning (unused slot), different on-disk representation. |
| `LvlTypes.txt` LvlType 2 File 25 = `Cottages.dt1` | Confirmed, identical. |
| `LvlPrest.txt` Def 47 `Pops = 2, PopPad = -4` | Confirmed, identical. |
| `objects.txt` lines 76 (Id 74, TrappDoor) and 24 (Id 22, StoneTheta) | Confirmed, identical. |
| `ObjType.txt` lines 76 ("Trap Door") and 24 ("Stone 6") | **Newly confirmed.** Previously unverified — `ObjType.txt` is not checked into this repository. Read directly from vanilla `D2Exp.mpq` this pass, and it matches Siramy's "line 76 onto line 24" exactly. |
| `assets/editor/obj.txt` lines 272 and 73, Act/Type/Id 1/2/11 | Re-confirmed (cheap re-check; this file is the **DS1 editor's own** bundled object table, not a game table, so it is correctly absent from every MPQ — not "unverifiable," just out of scope for MPQ verification). |
| `LvlWarp.txt` `l`/`r` Direction pairing, "exactly six Ids" | **Corrected.** Vanilla has **five** Ids (71, 73, 74, 81, 82), not six. The sixth pair the first pass counted, at Id 85, is a Project Diablo 2 addition absent from vanilla. |
| 5×5 sub-tiles | Confirmed — unaffected by the PD2/vanilla distinction, since it is a DT1 structural constant (`src/structs.h:1175`, `src/core/ds1.c:960`) rather than an Excel-table value. |

**The Id-83 "version drift" is reclassified.** The first pass's highest-value
finding — that following Siramy's "first empty row → Id 83" instruction
literally would overwrite an existing warp — turns out to rest on the same
PD2 contamination, not a Blizzard version difference. Vanilla `LvlWarp.txt` is
**byte-identical between 1.09d and 1.13c**: 88 records, Ids 0–82, and Id 83
genuinely free on both. `Act 4 Mesa to Hellcaves`, the row the first pass
found sitting at Id 83, cannot be a vanilla Diablo II area — vanilla's Act 4
has only the Outer Steppes, Plains of Despair, River of Flame, Chaos
Sanctuary and Pandemonium Fortress — so it is a Project Diablo 2 addition,
consistent with PD2 also adding the sixth `l`/`r` warp pair at Id 85 and 53
extra rows to `objects.txt`. **The warning is kept — it is real for anyone
modding on top of PD2 — but it is now a Mod note, not a Version note**, and
on stock 1.13c (as on 1.09d, as on the 1.10 beta Siramy used) his instruction
is correct exactly as written. Both documents' inline callouts and the
tutorial's `## Version differences` table now say so.

**A genuine version difference found in passing.** `objects.txt`'s `Name`
column for Id 74 reads `Trap Door` (with a space) on vanilla 1.09d, and
`TrappDoor` on vanilla 1.13c. Siramy reports seeing `TrappDoor` ("so no
error, that's the good one"), which matches 1.13c and not 1.09d — weak
evidence that the field had already changed by the 1.10 beta he used. Added
to the tutorial's `## Version differences` table.

**Ground truth for this pass:** `tools/d2mpq.py`, reading vanilla 1.13c and
1.09d installations located via `$D2_VERSIONS_ROOT` (the
`F:\D2VersionChanger\VersionChanger\LoD\` trees); `selfcheck` passed for
1.13c before use. Extracted tables cached at `.vanilla-cache/` (gitignored,
per `docs/book-conventions.md` §5). Six tables were pulled for 1.13c
(`objects.txt`, `levels.txt`, `lvlprest.txt`, `lvltypes.txt`, `lvlwarp.txt`,
`objtype.txt`) and three for 1.09d (`lvlwarp.txt`, `levels.txt`,
`objects.txt`, the ones with a real comparison to make). No Ghidra session,
live fleet member, or running game was used in this pass either — the D2Common
runtime-behaviour claims flagged unverified in the first pass (§4.8) remain
unverified.

---

## 1. Origin and rights

**Original author: Paul Siramy** (`paul.siramy@free.fr` / `siramy_paul@yahoo.com`),
site `paul.siramy.free.fr`. All three pages carry `<meta name="Author"
value="Paul SIRAMY">`. He is the author of `win_ds1edit`, the DS1 map editor
these pages document, and of the tutorial.

**Source integrity.** All three pages were retrieved **live** (not from the
Wayback Machine), per `docs/preservation/MANIFEST.tsv`:

| Page | bytes | sha256 |
|---|---|---|
| `_divers/ds1/doc/index.html` | 147,572 | `687a9e41c6c2538ae9b12b7c33320a852f04d64b288a71eaeab2669b441bc78d` |
| `_divers/ds1/doc/tut01/index.html` | 78,355 | `5981681b94de2832c6a79ad804d12e4bb8cd09f7c5195753763f18c9ce182d5d` |
| `_divers/ds1/dl_ds1edit.html` | 10,306 | `a3c1fe1944308f4eaa0bfba65a16f5c251f337f32d33a55beb61ef8250a807f6` |

No archived file was modified. Each modernized document is a separate file under
`docs/`; the archive under `docs/preservation/` is untouched.

### Rights status — the position taken

**No page in this archive carries a licence, a copyright notice, or any
republication grant.** This includes all three pages covered here. It is
personal fan-site documentation published between 2002 and 2011.

- The download page grants nothing beyond distributing the editor's own ZIPs,
  all of which are now dead links.
- The nearest thing to a grant anywhere in the archive is on the unrelated
  `d2ref` index, which offers a ZIP of *itself* — weaker than a republication
  licence, and not applicable to these pages.
- `docs/preservation/README.md` commits to treating the originals as
  authoritative. That is a **preservation** stance, not a **publication** one.

**The project proceeds on an explicit fair-use judgment rather than seeking
Paul Siramy's permission.** The reasoning and its limits are recorded in
[BOOK-STATUS.md](../BOOK-STATUS.md). If Paul Siramy objects, the terms
change. Each of the three documents carries the same position inline.

**Credit chain.** Siramy credits others within these pages and the credits are
preserved: **NewbieModder** (for the `mod_dir` examples, revision 22 June 2003)
and **Kingpin** (for the tent-warp idea, via a Phrozen Keep forum thread). The
tutorial also points readers at Siramy's own later Phrozen Keep tutorial, which
is a separate document owned elsewhere in this repo
(`docs/guides/monsters-and-objects.md`) and which he says supersedes his trap-door
method on 1.10 — that pointer was already present in the conversion and has been
kept.

---

## 2. The date contradiction

`docs/preservation/README.md` dates two of these pages from their capture or
mtime, not from their content. The files' own meta tags and their in-page
revision histories disagree, and agree with each other:

| Page | README says | `Date-Creation` meta | `Date-Revision` meta | In-page evidence |
|---|---|---|---|---|
| `_divers/ds1/doc/index.html` | **2007** | `20030220` | **`20060305`** | Revision History's newest entry is "05 March 2006" |
| `_divers/ds1/doc/tut01/index.html` | **2011** | `20030729` | **`20040701`** | The parent page's history records "29 Jully 2003 : Started Tutorial 1" |
| `_divers/ds1/dl_ds1edit.html` | 2011 | `20020301` | `20111105` | Newest ZIP on the page is dated 05 November 2011 — consistent |

**Verdict: the meta tags are authoritative. The README is wrong on two of three.**

The likely explanation for "2007" on the documentation page is that
`win_ds1edit_20070423.zip` — a build named *on the download page* — is dated
23 April 2007. That is a build date, not the date of the documentation page.
The "2011" on the tutorial has no plausible content basis at all: the page's
newest internal evidence is from 2004, and its own text says it was written on
the **1.10 beta**, which predates 1.10's release.

`docs/preservation/README.md` also states the archive's pages "carry dates from
2002 to 2011"; the `d2ref/` and `d2_sets/` trees are 2001, so the true range is
**2001–2011**. That file is owned elsewhere and was **not edited from here** —
this is a finding to hand back, not a change made.

All three modernized documents now carry the meta-tag date in their Origin block
and flag the contradiction where it applies.

---

## 3. What the conversions had dropped or garbled

These three markdown files already existed as raw HTML→markdown dumps. Diffing
each against its archived original (a five-word shingle diff over the stripped
HTML text, then a manual read of every mismatch) found the following.

### 3.1 Common to all three

| Defect | Detail |
|---|---|
| **Stray `<title>` as body prose** | Each file opened with its HTML `<title>` run together with the page's real banner text: `" DS1 Editor - Documentation         **Diablo II MAP Editor - Documentation**"`, `" Ds1 Editor - Tutorial 1         **Ds1 Editor \- Tutorial 1**"`, `" Diablo II DS1 (map) Editor - Download Page       **Diablo II MAP Editor** … By **P aul** **SIRAMY**"`. Removed; replaced by a real `#` heading. |
| **No heading structure at all** | Every section heading had been flattened to a bold paragraph, so the documents had **zero** anchors. Restored as real `##`/`###` headings. |
| **Dead `(#)` links** | See 3.2–3.4 — different cause in each file. |
| **No attribution, no rights status, no version scope, no verification** | All four added. |

### 3.2 `manual.md` (`_divers/ds1/doc/index.html`)

**Two substantive content losses — both silent, both now restored:**

1. **A row of the "What's inside the win_ds1edit.zip" table was rewritten.**
   Siramy's row reads `Pcx\` — *"Contains some graphical ressources for the
   editor"* (archive HTML lines 105–109). The conversion had replaced it with
   `assets/ui/` — *"Contains the editor's own interface images (PNG)"*. That is
   **this repository's** layout, presented as Siramy's 2006 text. The archive
   wording is restored, with the substitution documented in place.

2. **The `P` / `Shift + P` screenshot section was altered and truncated.**
   The original (archive HTML lines 1391–1401) says screenshots are **`.pcx`**,
   named `screenshot-00037.pcx`, and closes with a sentence the conversion
   **dropped entirely**:
   > "In addition, the format of this screenshot is in .BMP instead of .PCX,
   > because of the .PCX image dimensions limitations."

   The conversion had changed both `.pcx` mentions to `.png` and deleted the
   BMP sentence — which is the only explanation anywhere in the archive of why
   the two screenshot commands produce different formats. Restored verbatim.

**Garbled structure — restored:**

| What | How it was garbled | Fix |
|---|---|---|
| "What's inside the win_ds1edit.zip" table | Two-column table flattened into 24 alternating single-line paragraphs | Rebuilt as a markdown table |
| Advanced Tile Editing colour code | The original distinguished three states **by colour alone** (`#FFFFFF` white, `#0099FF` blue, `#009933` green). Stripping colour left two consecutive bullets reading **identically** — "**0** or **1** indicates…" twice — and left the later phrase "the bit will no longer be **blue** but **white**" referring to nothing | Colour names stated in words |
| 78 "click to enlarge" links | Every one pointed at `(#)`. Each originally pointed at a `*_big.gif` that **is present in the archive** | All 78 repointed at `../preservation/.../_divers/ds1/doc/<name>_big.gif`; all verified to resolve |
| 9 mode-legend figure grids (TAB, F1/F2, F3, F4, F5–F8, F9, F10, F11) | HTML tables pairing a toolbar icon, a screenshot and a caption were flattened into a run of images followed by a run of captions. F4 was the worst: **8 images then 4 captions**, with no way to tell which icon meant which mode | Re-paired by hand as markdown tables |
| 29 further figure/caption runs | Same flattening, in the worked exercises (area selection, `I`, throne room, copy, cut, night, walkable info, tile grid, path editing, bitfield selection) | Re-paired mechanically, under a rule that only fires when N images are followed by exactly N short captions; everything else left untouched |
| The `Left Click` modifier list | A definition list (`no key` / `Shift` / `Ctrl` / `H` → action) flattened into eight alternating paragraphs with dangling `:` prefixes | Rebuilt as a table |
| "Back to the main **Download Page**" | Retargeted to `../README.md`, losing the reference | Repointed at `overview.md`, which *is* the converted download page |

### 3.3 `01-basic-map-editing.md` (`_divers/ds1/doc/tut01/index.html`)

| What | How it was garbled | Fix |
|---|---|---|
| **The `LvlWarp.txt` new-warp column table** | The worst garbling in any of the three. Fourteen column/value pairs laid out in a two-up HTML grid (archive HTML lines 753–938) had been flattened into **~113 lines of one-fragment-per-line text** — `Name`, blank, `=`, blank, `Act 1 Small Tent R`, blank, … — destroying the pairing between every column name and its value | Rebuilt as a table, values verbatim from the archive |
| `txt_and_ds1.zip` | Filename replaced with the invented label "txt_and_ds1 example files" | Filename restored |
| `log.tx t` | An HTML `<font>` split mid-filename (`log.tx</font><font>t`) was carried through literally; the original page *renders* `log.txt` | Fixed |
| `Si nce` | Same cause (`Si</font><font>nce`) | Fixed |
| Three dead `(#)` ZIP links | `trap_door.zip`, `tent.zip`, `house.zip` — none archived, and all contain Diablo II game data | Marked unavailable rather than left as dead links |
| Dangling caption | "Here are 2 screenshots of this new Warp, in-game." had lost its figure association | Note added tying it to `tut01_08.gif` |
| 4 figure/caption runs | Same image-run-then-caption-run flattening as `manual.md` | Re-paired as tables |

No text was *lost* from this page — all 26 figures survive and every paragraph is
present.

### 3.4 `overview.md` (`_divers/ds1/dl_ds1edit.html`)

| What | How it was garbled | Fix |
|---|---|---|
| Five section headings | `Main ZIPs`, `Stand-alone DEMO version`, `Source Code`, `Links`, `Contact` had all become plain paragraphs | Restored as `##` headings |
| Screenshot gallery | Eight captions were dead `(#)` links; each originally pointed at `screenshots/big_0N.gif` — **all eight are in the archive** | Rebuilt as a preview/caption/full-size table with working links |
| Four `(enlarge picture)` links | Dead `(#)`; the targets `demo1.gif`–`demo4.gif` **are in the archive** | Repointed |
| Link to the documentation page | Dead `(#)` | Repointed at `manual.md` |
| Four dead ZIP downloads | Not archived, host gone | Marked unavailable |

No text was lost from this page.

---

## 4. Verification

### 4.1 Ground truth used

| Source | What it settles | Where |
|---|---|---|
| This repository's DS1 parser | On-disk layout, field widths, flag masks, version branches | `src/core/ds1.c`, `src/structs.h`, `src/misc.c`, `src/render/preview.c`, `src/ui/bits_window.c` |
| **2,666 real `.ds1` files** | Whether claimed values actually occur in shipped Blizzard maps | `assets/tiles/ACT1..ACT5/`, `assets/tiles/expansion/` |
| The game's Excel tables | Every `Levels.txt` / `LvlPrest.txt` / `LvlTypes.txt` / `LvlWarp.txt` / `objects.txt` claim in the tutorial | `assets/excel/` |
| The editor's object table | The `obj.txt` line numbers in the tutorial | `assets/editor/obj.txt` |
| The archived HTML | What the original actually said | `docs/preservation/siramy/paul.siramy.free.fr/` |

The DS1 parser was re-implemented in Python directly from `src/core/ds1.c` and
run over all 2,666 files: **all 2,666 parsed with zero stream desync**, which is
itself evidence the documented layout is right.

**Sources NOT used, and why.** No Ghidra session, no running game, no live
fleet member, no MPQ reach-in. Every claim that would have needed one is marked
unverified below rather than assumed. In particular, no D2Common function was
disassembled, so none of the tutorial's three quoted assertion strings, and none
of its statements about D2's runtime behaviour, were checked.

### 4.2 Two caveats on the evidence, stated plainly

**Caveat 1 — partial circularity.** `d2-ds1-edit` is a descendant of Siramy's
own `win_ds1edit`: it shares his struct names (`CELL_W_S`, `CELL_F_S`,
`CELL_S_S`), and its bitfield dialog carries the *identical* label strings to
his 2004 screenshot. Agreement on **naming** between this repository and his
documentation therefore proves very little. Agreement on **bit masks exercised
against real Blizzard map files** — which is where the load-bearing evidence sits
— does not have that problem, and is what the verdicts below rest on.

**Caveat 2 — the Excel tables are PD2-derived — RESOLVED for the tutorial's
claims by the second pass (§0).** `assets/excel/LvlTypes.txt` carries two
`PD2assets/` DT1 rows that vanilla does not have, so this table set is a
**Project Diablo 2** copy, not certified vanilla 1.13c. The first pass found
every row cited below matched exactly, and none of the matched rows looked
modded — a correct read, as it turned out: the second pass re-checked all
twelve tutorial data claims against genuine vanilla 1.13c and 1.09d archives
and found the same values, with one exception (the `LvlWarp.txt` `l`/`r`
pairing — see §0). The caveat below is left as the first pass wrote it, for
the historical record of what was and was not known at the time.

### 4.3 Claim tally

| | manual.md | 01-basic-map-editing.md | overview.md | Total |
|---|---|---|---|---|
| Format/data claims **confirmed** | 9 | 12 | 0 | **21** |
| Claims **corrected / contradicted** | 1 | 0 | 0 | **1** |
| Version-drift warnings added | 0 | 1 | 0 | **1** |
| Conversion defects **repaired** | 11 | 7 | 5 | **23** |
| Claims marked **unverifiable** (tool UI, runtime behaviour, dead downloads) | ~35 sections | ~10 | whole page | — |

**Second pass (§0), vanilla 1.13c/1.09d re-verification — `manual.md` and
`01-basic-map-editing.md` only:**

| | Count |
|---|---|
| Tutorial data claims re-checked against vanilla | 12 |
| Confirmed unchanged | 10 |
| **Corrected** (`LvlWarp.txt` `l`/`r` pairing: five Ids, not six) | 1 |
| **Newly confirmed**, previously unverified (`ObjType.txt` lines 76/24) | 1 |
| Reclassified version-drift → **Mod note** (Id 83) | 1 |
| Genuine version difference found in passing (`objects.txt` Id 74 `Name`) | 1 |
| `## Version differences` tables added | 2 (one per document) |

### 4.4 Confirmed — `manual.md`

| Claim | Verdict | Evidence |
|---|---|---|
| Floor layers 1–2, Wall layers 1–4, one Shadow layer | **CONFIRMED** | `src/structs.h:30-33`: `FLOOR_MAX_LAYER 2`, `WALL_MAX_LAYER 4`, `SHADOW_MAX_LAYER 1`. Across 2,666 files, on-disk wall count ∈ {0..4}, floor count ∈ {0..2}, never more. `src/core/ds1.c:528` hardcodes `s_num = 1 // # of shadow layer, always here` |
| A tile cell has a **Hidden** bit | **CONFIRMED** | `prop4 & 0x80` — `src/render/preview.c:1093`, comment `// binary : 1000-0000`; also `:675`, `:858`, `:1316`, `:3390`, `src/ui/edit_window.c:232-234`. Set on 10,562 wall cells and 25,370 floor cells across the corpus |
| A tile cell has an **Unwalkable** bit that makes the whole tile unwalkable | **CONFIRMED** | `prop3 & 0x02` — `src/misc.c:1102-1107` (floors) and `:1143-1148` (walls); both set the unwalkable flag on **all 25** sub-tiles, exactly matching "the entire Tile becomes unwalkable". Set on 22,251 wall and 940,661 floor cells |
| The dialog's column labels (Hidden / ? / IsShadow / Main-index / Unwalkable / Sub-index / "Layers priority, Type of layers, and unknown" / Orientation) | **CONFIRMED** (weakly — see Caveat 1) | Transcribed from `bitfield_big.gif` and `bitfieldv2b_big.gif`; identical strings at `src/ui/bits_window.c:69-92` |
| Orientation exists only on wall layers | **CONFIRMED** | Each wall layer is stored as *two* interleaved on-disk streams, cells and orientations (`src/core/ds1.c:653-657`, `:823-837`). Floors and shadow have no orientation stream. Matches the dialog exactly, where `F1`/`F2`/`SH` have one fewer column than `W1`–`W4` |
| Orientation field is one byte | **CONFIRMED** | The record occupies 4 bytes, only the low byte used (`ds1.c:823-837`); the upper 3 bytes are zero in **all 17,530,587** records in the corpus |
| Orientation 10/11 = special / Vis tiles | **CONFIRMED** | `src/misc.c:312-317` sets `BT_SPECIAL` for exactly those two values; `src/render/preview.c:1085-1091`, `:1413-1442` route them to the special draw pass. Empirically orientation 10 occurs 799× and 11 301×, concentrated in 546 files dominated by `*warp*.ds1` |
| The bitfield screenshot's own values are self-consistent | **CONFIRMED** | In `bitfield_big.gif` the `W1` row reads Orientation `00001010` = 10 and Main-index `000001` = 1, matching the green in-map label "Vis 1" |
| "Weird" invisible tiles are a flag, not corruption | **CONFIRMED** as to mechanism | It is the Hidden bit above. **Why** Blizzard's maps carry them along top and bottom edges remains unexplained — Siramy's "mostly unknown" still stands |
| NPC path block: per-point `x`, `y`, and an **Action** value | **CONFIRMED** | `src/core/ds1.c:1071-1207`; `PATH_S { x; y; action; }` at `src/structs.h:825-831`. Path block appears from version 14; the third `action` dword from version 15. Empirically 17 files, 138 blocks, 544 points; action values 1 (373×), 2 (102×), 3 (40×), 4 (28×), 5 (1×) |

### 4.5 Corrected — `manual.md`

**"Only Type 1 (Monster / NPC) objects can use paths in the game."**
→ **Not a format rule.** The DS1 path record encodes no object type at all;
paths are keyed by the owning object's (x, y) coordinate (`src/core/ds1.c:1091-1152`,
which discards a block outright when two objects share a cell). Four of the 138
path blocks in the corpus resolve to coordinates where the only object present is
**Type 2**:

| File | Coordinate | Object | Points |
|---|---|---|---|
| `ACT2/Town/lutN.ds1` | (84, 154) | Type 2 / Id 17 | 8 |
| `ACT2/Town/lutW.ds1` | (83, 154) | Type 2 / Id 17 | 9 |
| `ACT3/Docktown/docktown3.ds1` | (219, 38) | Type 2 / Id 110 | 3 |
| `expansion/Town/townWest.ds1` | (143, 28) | Type 2 / Id 126 | 2 |

The other 134 are Type 1, so Siramy's statement is a sound rule of thumb and may
well be correct about what the *engine* does. It is not a constraint the *file*
imposes. **Whether D2 actually walks a Type 2 object along a path was not
tested** — that needs the game, not the parser. The document keeps his sentence
and carries the correction beneath it.

One further clarification added, which is a correction to a natural
misreading rather than to Siramy: **there is no "special" layer on disk.**
Special tiles are ordinary wall-layer cells at orientation 10 or 11. The
editor's separate `spl` toggle invites the assumption; Siramy never makes it.

### 4.6 Confirmed — `01-basic-map-editing.md`

Every checkable data claim in the tutorial matched, most of them exactly.

| Claim | Verdict | Evidence (`assets/excel/`, `assets/editor/`) |
|---|---|---|
| `LvlPrest.txt` row "Act 1 - Town 1" has File1–File4 = `TownN1/E1/S1/W1.ds1` | **CONFIRMED** | Def 1, LevelId 1, exactly those four in that order |
| `Levels.txt`: Act 1 Town = Id 1, Graveyard = Id 17, Tristram = Id 38, Act 2 Town = Id 40 | **CONFIRMED** | LevelNames `Rogue Encampment`, `Burial Grounds`, `Tristram`, `Lut Gholein` |
| Act 2 Town's `Warp2` column holds **19** | **CONFIRMED** | Id 40: Vis2 = 47, Warp2 = 19; and `LvlWarp.txt` Id 19 is named **"Act 2 Town to Sewer Trap"** — the very trap door in the screenshot |
| An unset Warp column reads **-1** | **CONFIRMED** | Both Act 1 rows start with Vis6/Vis7 empty and Warp6/Warp7 = -1 |
| `Objects.txt` Id 74 = TrappDoor, **Excel line 76** | **CONFIRMED** exactly | |
| `Objects.txt` Id 22 = StoneTheta, **Excel line 24** | **CONFIRMED** exactly | |
| TrappDoor has `OpenWarp = 1` | **CONFIRMED** | The value his closing note depends on |
| `obj.txt` **line 272** = `Trap Door (74)` | **CONFIRMED** exactly | `assets/editor/obj.txt` — the DS1 editor's own bundled object table, not a game table; legitimately absent from every MPQ |
| `obj.txt` **line 73** = `Cairn Stone, Theta (inactive) (22)`, and its Act/Type/Id are **1 / 2 / 11** | **CONFIRMED** exactly | Precisely the three values he tells you to restore after the paste |
| `ObjType.txt` **line 76** onto **line 24** | **CONFIRMED exactly (second pass)** | Read from vanilla `D2Exp.mpq`: line 76 = `Trap Door` (Token `TD`), line 24 = `Stone 6` (Token `S6`). Reported **unverified** in the first pass, because this table is not checked into the repository |
| `LvlTypes.txt` LvlType 1 (Town) lists `Floor.dt1`, `Fence.dt1`, `trees.dt1` and no cottage — and **File7 is empty** | **CONFIRMED** | Which is why File7 was the free slot |
| `LvlTypes.txt` LvlType 2 (Wilderness) **File25 = `Act1/Outdoors/Cottages.dt1`** | **CONFIRMED** exactly | |
| `Dt1Mask` for Act 1 Town is **959**; 959 = `0b1110111111` = File1–File10 except File7; +64 = **1023** | **CONFIRMED** | Dt1Mask is *still* 959 in this table. Bits set: File 1,2,3,4,5,6,8,9,10 |
| `LvlPrest.txt` Def **47** = "Act 1 - Cottages 1", **Pops = 2, PopPad = -4** | **CONFIRMED** exactly | Act 1 Town's row has Pops = 0, PopPad = 0, as he says |
| `LvlPrest.txt` Def **160** = the Cairn Stones preset (`Act1/Outdoors/cairn2.ds1`) | **CONFIRMED** | |
| `LvlWarp.txt`: all warps use Direction `b` except some Expansion ones, which come in **same-Id `l`/`r` pairs** | **CONFIRMED, pairing count CORRECTED (second pass)** | First pass against `assets/excel/` (PD2-derived): 80 `b`, 6 `l`, 6 `r`, six Ids (71, 73, 74, 81, 82, 85). Second pass against **vanilla** `D2Exp.mpq` (1.13c and 1.09d, byte-identical): 78 `b`, 5 `l`, 5 `r` — exactly **five** Ids (71, 73, 74, 81, 82). Id 85 is a Project Diablo 2 addition, absent from vanilla; the pairing pattern Siramy describes is exactly right, just for one fewer Id than the PD2-derived cache showed |
| A tile is a group of **5 × 5 sub-tiles** | **CONFIRMED** | `src/core/dt1.c:168-200` reads a 25-entry sub-tile flag block through a 5×5 row-reversal index table; `sub_tiles_flags[25]` at `src/structs.h:1175`; and independently `src/core/ds1.c:960-961`: `max_subtile_width = new_width * 5` |

### 4.7 Version drift — `01-basic-map-editing.md` — RECLASSIFIED as a Mod note (second pass, §0)

**First pass finding.** Siramy says to add the new tent warp at "the bottom of
the file, in the first empty row" and uses **Id 83**. On the 1.10-beta
`LvlWarp.txt` he was working from, 83 was unused. In this repository's
`assets/excel/` cache it was **already taken — `Act 4 Mesa to Hellcaves`** —
and the highest Id present was 85. A reader following the instruction
literally against that table would **overwrite a shipped warp**. A warning
was added to the document at that step, framed at the time as a version
difference (1.10 beta vs. "a later, PD2-derived set").

Whether 83 was free in the *released* 1.10 (as opposed to the beta) was **not**
checked in the first pass; only this repository's table was consulted.

**Second pass — re-checked against vanilla, and it is not a version
difference.** Vanilla `LvlWarp.txt` is **byte-identical between 1.09d and
1.13c**: 88 records, Ids run 0–82, and **Id 83 is genuinely free on both**.
`Act 4 Mesa to Hellcaves` cannot be vanilla content — vanilla Diablo II's
Act 4 has only the Outer Steppes, Plains of Despair, River of Flame, Chaos
Sanctuary and Pandemonium Fortress, no Mesa or Hellcaves — so that row, and
the extra `l`/`r` pair at Id 85 (§4.6), are **Project Diablo 2 additions**
appended past vanilla's own Id 82, the same mechanism documented for
`objects.txt`'s 53 extra rows in `docs/vanilla-data.md`.

**Verdict: on stock 1.13c (as on 1.09d, as on the 1.10 beta Siramy used),
Siramy's instruction is correct exactly as written.** The warning is kept —
it is real for anyone modding on top of PD2, or any table where warps have
already been added past Id 82 — but the document now presents it as a
**Mod note (Project Diablo 2)**, not a version note, and says plainly that
vanilla 1.13c needs no such caution.

### 4.8 Unverifiable — stated, not silently assumed

**All tool-UI behaviour.** Every key binding, dialog, button and mode in
`manual.md`, and every editor step in the tutorial, describes a 2003–2006 build
of `win_ds1edit`. **The binary is not in this repository and cannot be run.**
Both documents now say so in their Origin block and frame these sections as
historical description. That is roughly 35 keyboard sections in `manual.md`
plus every editor step in the tutorial. Specific examples:

- the incremental-backup save scheme (`duriel.ds1` → `duriel-000.ds1`);
- the undo buffer and its `data\tmp` files;
- the SET/slot model for editing up to 100 DS1 at once;
- the night preview, gamma correction, and refresh-rate behaviour;
- that Ctrl + Shift + Right-click opened the bitfield dialog with no undo;
- the `25326` lines-of-code figure on the download page.

**All runtime/engine behaviour.** No disassembler and no running game were used,
so these remain unverified:

- The three D2Common assertion strings quoted in the tutorial
  (`LvlTbls.cpp` line #1047, `DrlgRoom.cpp` line #604, `DrlgVer.cpp` line #109)
  — text, line numbers, and trigger conditions all unchecked.
- That the game selects a `Direction l` vs `r` row in `LvlWarp.txt` by the Vis's
  orientation 10 vs 11. The `l`/`r` pairing in the table is confirmed; the
  selection rule is not.
- That "the last column and the last row of tiles in a ds1 are not used by the
  game". **Actively tested and inconclusive**: across 2,658 files the last floor
  row is completely empty in only 158, and last-row fill *exactly equals* the
  previous row's fill in 1,275 of them. The outer ring is mildly emptier on
  average — part of a smooth taper from row-2 → row-1 → row — but it is
  emphatically **not** systematically blank. That neither confirms nor refutes
  the runtime claim; there is simply no evidence either way in this repository.
  Siramy's advice not to place a Vis at the border is left standing.
- That an NPC "will only use the points on the Pink lines (and randomly), and
  they'll never go back to their original starting position".
- The meaning of `Pops` / `PopPad`. Siramy says it "is not well know"; **nothing
  in this repository reads or interprets those columns**, so his admission still
  stands unimproved. The *values* (2 and -4) are confirmed.
- That roof-disappearance depends on specific special tiles whose workings are
  "still unknown" — likewise unimproved.

**Dead downloads.** Every ZIP named across the three pages
(`win_ds1edit_20111030.zip`, `win_ds1edit_20070423.zip`, `win_ds1edit_demo.zip`,
`win_ds1edit_20111030_src.zip`, `txt_and_ds1.zip`, `trap_door.zip`, `tent.zip`,
`house.zip`) is gone and none were archived. Several contained Diablo II game
data and would not be redistributable regardless. All are marked unavailable
rather than linked.

**`ObjType.txt`.** ~~The tutorial's "copy line 76 onto line 24 in `ObjType.txt`"
step is **unverified** — that table is not in this repository.~~ **CONFIRMED
in the second pass (§0).** That table is not checked into this repository, but
it is a real vanilla MPQ table (`D2Exp.mpq`), and reading it directly with
`tools/d2mpq.py` confirms line 76 = `Trap Door`, line 24 = `Stone 6` — exactly
Siramy's pairing.

### 4.9 Images

`_big.gif` full-size screenshots for `manual.md` are all present in the archive
(76 files) and all 78 enlarge links now resolve to them. The tutorial's 26
figures and the download page's 8 gallery screenshots and 4 demo images are
likewise all present and linked.

The single highest-value image — `bitfield_big.gif`, the Advanced Tile Editing
dialog — has been **transcribed into a table** in `manual.md` (row order, column
order, and which columns exist only on wall rows), so the information survives
independently of the image. The image reference is kept alongside.

---

## 5. Open questions for the author or the repository owner

1. ~~**Rights.**~~ Settled — see §1: the project proceeds on a fair-use
   judgment, not a licence from the author. Is there any correspondence with
   Paul Siramy granting republication? Nothing in the archive constitutes
   one.
2. **`docs/preservation/README.md` needs two fixes** — the 2007 and 2011 dates
   above, and the "2002 to 2011" range, which should read 2001–2011. That file is
   owned elsewhere; it was not edited from here.
3. ~~**Was `assets/excel/` intended to be a vanilla table set?**~~ **RESOLVED
   by the second pass (§0).** No — it is Project Diablo 2-derived, and a
   vanilla set was available via `tools/d2mpq.py` all along. §4.6 was re-run
   against genuine vanilla 1.13c and 1.09d archives; Caveat 2 is resolved for
   every claim in this report, with one correction found (the `LvlWarp.txt`
   `l`/`r` pairing) that a PD2-derived table alone could not have caught.
4. **The three D2Common assertion strings** are the tutorial's narrative spine
   and are cheap to settle in Ghidra — grepping for them would both date the
   claims and confirm the failure modes. Not done here.
5. **Why do Blizzard's maps carry Hidden tiles along their top and bottom
   edges?** Siramy flags this as unknown and recommends leaving them alone. The
   *bit* is now understood; the *intent* is not, and nothing in this repository
   explains it.
6. **The `Pops` / `PopPad` columns** remain undecoded, twenty-two years after
   Siramy said so.
