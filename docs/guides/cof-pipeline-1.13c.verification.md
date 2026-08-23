# Verification record — COF Pipeline (1.13c)

Companion audit for [cof-pipeline-1.13c.md](cof-pipeline-1.13c.md).

**Binaries.** Retail 1.13c LoD, read-only in Ghidra: `D2Client.dll` (image base
`6fab0000`), `D2Common.dll` (`6fd50000`), `D2CMP.dll` (`6fe10000`).

> **A note on this file.** This record was assembled in two passes. The first
> pass's written record went missing from the repository between being written
> and the second pass running (several sessions work in this repo concurrently);
> the second pass honestly began the record at the open items rather than
> re-document evidence it had not seen. The first pass's findings have since
> been restored below from its own working notes. Where a figure is attributed
> to a particular pass, it says so.

---

## First pass — mechanical verification (2026-08-21)

**Binaries pinned.** `D2Client.dll` SHA-256
`dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906`, from
`F:\D2VersionChanger\VersionChanger\LoD\1.13c\`; `D2Common.dll` and `D2CMP.dll`
imported from that same stock 1.13c tree for this audit (neither was loaded
before it). A live `lod-113c` fleet member was available as a second oracle but
was not needed — every claim resolved statically.

**Method caveat that shaped everything.** The document's function names are
almost certainly the *source* of this Ghidra project's names, so a name match
proves self-consistency, not correctness. The load-bearing checks were
therefore (a) does a function start at exactly the claimed address, and (b)
does the decompiled body support the stated purpose.

| Class | Checked | Result |
| --- | --- | --- |
| Strings at address | 8 | 8 byte-exact |
| Function-address rows | ~293 (all rows, all three DLLs) | ~99% resolved to exact entry points |
| Call-graph edges (sampled) | 15 | 15 present in the binary |
| Behavior (decompiled) | 11 flagship functions | 10 confirmed, 1 plausible |

**Strings — all 8 confirmed byte-exact** at their claimed addresses:
`%s\%s\COF\%s%s%s.COF`, `cmncof_a6.d2`, `cmncof_a4.d2`, `cmncof_a3.d2`,
`cmncof_a2.d2`, `cmncof_a1.d2`, `COF Memory->%i of %i`, `R\Cof\palshift.dat`.

**Corrections applied to the document in that pass:**

1. **`ProcessMonsterAnimationFrame` — wrong address.** The document gave
   `6fafbef0`, which is 0x30 *inside* `CreateMonsterDoubleAttackMissiles`
   (`6fafbec0`–`6fafbf9b`), not a function start. The real entry point is
   **`6faff2c0`**. Confirmed twice, independently.
2. **`GetComponentListProperty` — three distinct functions in one "3 instances"
   row.** Only `6fb1d780` carries that name; `6fb1d700` is
   `GetComponentListMode` and `6fb1d740` is `GetComponentListType`. Split.
3. **Four Stage 4 blit "2 variants" rows — first address was a different
   function in each.** `6fe1eb50` is `BlitSpriteRLEWithDualTableClipped`
   (the documented `…PaletteBlendClipped` is `6fe1f620`); `6fe1edd0` is
   `…DualTableFull` (name belongs to `6fe1f8a0`); `6fe1e860` is
   `…PaletteClipped` (the `…ClippedAlt` name belongs to `6fe1efd0`);
   `6fe1ea20` is `…PaletteFull` (`…FullAlt` belongs to `6fe1f190`).
   `BlitSpriteDispatcher`'s decompile confirms distinct dispatch conditions.
   Split into eight single-address rows.
4. **`CleanupAllCellListItems` — "2 instances" mislabel.** `6fe11ad0` is
   `ProcessMemberDestructors`, a different function; only `6fe11f80` is
   `CleanupAllCellListItems`. Split.

**Non-aligned addresses, all checked and legitimate.** Three addresses sit on
odd byte boundaries and were individually confirmed as real function entry
points: `DecodeSpriteFrameData@6fe1d76e` (a genuine start, not mid-function),
and `GfxRenderCommand@6fabd1c8` + `GfxDrawAndUpdateFrameIndex@6fabd0f6`, which
are 5-byte inter-module JMP thunks — hence the alignment — in the same cluster
as `DrawGraphicsInterface@6fabd120`.

**Type-B sampling policy (first pass).** Decompiled every function whose purpose
line made a specific behavioral claim among the flagship entry points (COF load,
path build, unit render, dispatch, monster and overlay state machines, sprite
load, blit dispatch, DCC decode). Generic accessor rows ("Gets cel flags") were
confirmed at the address level only. A full behavioral pass over all ~300
functions was not done and remains the main residual verification work.

**Divergences where the DOCUMENT was right and the Ghidra label spurious** —
adjudicated by decompile in the first pass: `6fd93c30` `BuildCofPathString`
(labelled `BinkBufferGetError`; body calls `wsprintfA` with the COF format
string), `6fd93860` `GetWeaponClassToken` (labelled `SetMissileDataField0x14`;
returns weapon-class tokens such as `"hth"`), and `6fd71420`
`RebuildEquippedVisualComponents` (labelled `CalcSkillMaxDamage`, while
Ghidra's own auto-comment describes rebuilding equipped visual components).

---

## Closed: five name divergences

Each of these was a case where the document's chosen name for a function
disagreed with the name carried in the Ghidra database. The question in every
case is the same: which name describes what the code actually does? The binary
does not automatically win. Ghidra names in this project arrive from bulk
labelling passes, and several of them turn out to be inherited from a
neighbouring function or from a copy-pasted comment block.

### 1. `6fb18be0` — doc `CleanupOverlayBufferState` vs Ghidra `CleanupOverlayBuffer`

**Verdict: DOC_RIGHT.**

The document lists one row, `CleanupOverlayBufferState`, carrying two addresses
`6fb18be0` and `6fb18c30`, described as two instances. Decompiling both settles
it — they are the same routine over two different global buffers:

| | `6fb18be0` | `6fb18c30` |
|---|---|---|
| frees marked hash entries in | `0x6fbc44c8` | `0x6fbc4cd8` |
| decrements | `DAT_6fbc44d0` | `DAT_6fbc4ce0` |
| re-inits module if | `DAT_6fbc44cc != 0` | `DAT_6fbc4cdc != 0` |
| then zeroes | `0x203` dwords | `0x203` dwords |

Identical control flow, identical `0x203`-dword wipe, and the two buffers sit
`0x810` bytes apart — adjacent arrays of the same size. Grouping them as two
instances of one routine is correct, and Ghidra's `CleanupOverlayBuffer` on the
first is simply an inconsistent shorthand for the name already on the second.
No change to the document.

### 2. `6fb504d0` — doc `InitializeMemoryBudget` vs Ghidra `InitializeClientMemoryBuffers`

**Verdict: DOC_RIGHT.**

This is a call-graph node in Stage 1, not a table row. The body reads the
machine's physical memory, refuses to continue below a floor, and then computes
and clamps two budgets from what is left:

- aborts through `ValidateParameterOrShutdown` with the literal `"This machine
  doesn't have enough physical memory (> %.2f MB) for minimal performance!"`
  when physical memory is under `0x167d000`;
- takes 10% of `physical - 0xb00000` and clamps it to `[0x300000, 0x500000]`;
- subtracts a further `0x600000` when its argument is set;
- divides the remainder by three and clamps that to `[0x1800000, 0x4000000]`;
- hands the resulting sizes down and calls `InitializeCofLayerDataArrays`.

Every branch in the function is arithmetic on a budget. It sizes buffers; it
does not initialize them. `InitializeMemoryBudget` is the better description and
the document keeps it. No change.

### 3. `6fd82820` and `6fd95d30` — the conflated pair

**Verdict: `6fd82820` DOC_RIGHT. `6fd95d30` DOC_WRONG — row removed.**

The document carried two rows with near-identical names and identical purpose
text: `UNITS_InitializeAnimationFromGfxMode` at `6fd82820` and
`InitializeAnimFromGraphicsMode` at `6fd95d30`. In Ghidra *both* addresses carry
the name `InitializeAnimFromGraphicsMode` and *both* carry the same
copy-pasted descriptive comment. They are not the same function, and the comment
describes only the first of them.

`6fd82820` (ordinal 10099) does what the comment says. It resolves the unit's
graphics-mode record and writes the animation fields:

```
pUnit+0x30 = record base pointer
pUnit+0x34 = record[+4] << 8      (frame count, 8.8 fixed)
pUnit+0x38 = 0                    (frame accumulator)
pUnit+0x3C = 0x100                (rate = 1.0)
pUnit+0x48 = record[+8] << 8
pUnit+0xC4 |= 0x4000              (update flag)
```

The document's name is an accurate synonym of Ghidra's. Row kept unchanged.

`6fd95d30` (ordinal 10110) has nothing to do with animation. Disassembly shows
`RET 0x8` — two stdcall arguments — with `XOR EAX,EAX` on every failure path and
`MOV EAX,0x1` on one success path. It is a boolean predicate over an item and a
txt record:

- argument 1 must be a unit of type 4 (item); asserts `0x1505`/`0x1506`/`0x1508`
  otherwise;
- reads `pItemData+0x30` against 100, and on the low branch fetches the
  Items.txt record from `dwTxtFileNo` and tests the byte at record `+0x132`,
  falling through to `GetItemTypeProperty`;
- gates on argument 2's word at `+0x0E` against 100;
- walks four shorts at argument 2 `+0x1E`, returning **0** on any match — an
  exclusion list;
- walks seven shorts at argument 2 `+0x10`, returning **1** on any match — an
  inclusion list;
- returns 0 if nothing matched.

Both loops call `6fd74430`, whose body indexes the ItemTypes equivalency bitmask
table (`sgptDataTables+0xC04`, stride at `+0xC00`) and falls back to the
Items.txt secondary type at record `+0x120` — that is, "is this item of this
item type, following the type tree". So `6fd95d30` answers "does this item
satisfy this record's item-type include/exclude lists and its level gate". It is
an item-eligibility predicate, and its Ghidra name is inherited wholesale from
`6fd82820`.

The row was removed from the Stage 2 animation table. Nothing in the document's
prose referenced it.

### 4. `6fd83110` — doc `UNITS_UpdateAnimationSpeedByMode` vs Ghidra `UNITS_SetGfxSelected`

**Verdict: DOC_RIGHT**, and Ghidra's own comment block agrees with the document
against Ghidra's own name.

The comment attached to the function in the database is titled "Set Unit GFX
Selected State" and then immediately describes something else: "Sets animation
speed and frame rate for a unit based on its type, current mode, equipped
weapons, and stat modifiers." The body is the second thing, not the first:

- skips null units, items (type 4), and types above 4;
- dispatches on mode category — base, walk/run, cast, attack, swing;
- reads stat `0x43` (walk/run speed), `0x44` (attack speed), `0x45` (cast
  speed), adds the diminishing-returns modifier, clamps to `[15, 175]`;
- averages the two hand slots for a dual-wielding unit;
- applies a `-30` penalty to player mode `0x12`;
- writes the result to `pUnit+0x4C` and `pUnit+0x3C`.

Nothing in it selects anything. The document's name is precise and stays.

### 5. `6fd80460` — doc `SKILLS_GetActiveSkillAnimData` vs Ghidra `GetActiveSkillField8`

**Verdict: neither name was right; the document's claim was the false one, so
the row was corrected.**

The function is four lines long:

```c
if (pUnit == 0)                    abort(0x641);
if (*(int *)(pUnit + 0xa8) == 0)   abort(0x3ea);
return *(int *)(*(int *)(pUnit + 0xa8) + 8);
```

`pUnit+0xA8` is `pInfo`; the confirmation is next door, where `6fd82820` sets
`*(uint *)(pUnit + 0xc4) |= 0x4000` — `+0xC4` is `dwFlags`, which fixes the
struct. `pInfo+0x08` and `pInfo+0x0C` are the two active-skill slots, and
`6fd80420` is the identical function for `+0x0C`.

What comes back is a `Skill*`, not animation data. Its three callers prove it by
dereferencing it as one:

| caller | reads | writes |
|---|---|---|
| `UpdatePlayerSkillAnimData8` `6fd80670` | `*(short *)*node` (skill id), `node[0xD]` | `pPlayerData+0x74`, `+0x7C` |
| `UpdatePlayerTargetFromSkill` `6fd80590` | same two fields | `pPlayerData+0x84`, `+0x8C` |
| `UpdatePlayerSkillAnimDataC` `6fd806d0` | via the `+0x0C` twin | `pPlayerData+0x70`, `+0x78` |

So the document was asserting something false ("animation data") where Ghidra
was merely being opaque ("Field8"). The row is now
`SKILLS_GetActiveSkillNode`, described as returning the active-skill node at
`pInfo+0x08` — a `Skill*`, not animation data.

---

## Closed: the function count summary

The old summary claimed D2CMP 95, D2Common 38, D2Client "120+ … Mostly named,
~12 still `FUN_*`", and "253+ total, ~95% named". Every figure was low, and the
`FUN_*` claim was false.

Counted mechanically from the document's own tables — a "row" is a table line, an
"address" a distinct function address named on one:

| DLL | rows | distinct addresses | distinct names |
|---|---|---|---|
| D2CMP.dll | 111 | 111 | 111 |
| D2Common.dll | 36 | 45 | 35 |
| D2Client.dll | 144 | 144 | 141 |
| **Total** | **291** | **300** | **287** |

Rows and addresses diverge in both directions: a row carries several addresses
when one routine was compiled into more than one instance (`GetAnimDataFrameInfo`
alone lists four), and a function counted as two rows is one address when it
appears in two stages (`RenderUnitComponentLayers` in Stages 3 and 6,
`RenderUnitOverlayEffects` and `DrawOverlaySpritesForLayer` in Stages 6 and 8,
`UnpackAnimComponentFields` in Stages 2 and 3). The eight COF string-table
entries are counted separately and are not in the table above.

The `FUN_*` question was answered by listing every such function in each DLL
rather than by sampling:

- **D2Client.dll** — 5,933 functions, **zero** named `FUN_*`. The document's
  "~12 still `FUN_*`" has no basis; there are none in the DLL at all, let alone
  among the 144 the document names.
- **D2CMP.dll** — zero named `FUN_*`.
- **D2Common.dll** — 21 named `FUN_*` (`6fd59d50`, `6fd5aa20`, `6fd5cd10`,
  `6fd65840`, `6fd66e10`, `6fd7c0c0`, `6fd94aa0`, `6fd95210`, `6fda2e70`,
  `6fda4130`, `6fda4200`, `6fda5490`, `6fdb7f30`, `6fdb8860`, `6fdb88b0`,
  `6fdb95a0`, `6fdcac50`, `6fdcc2c0`, `6fdcc760`, `6fdd1ea0`, `6fdd61a0`). The
  intersection with the document's 45 D2Common addresses is empty.

So every function this document names is named in Ghidra. The summary table in
the document now carries these figures and states plainly that they are counts
of its own rows, not an independent survey of the DLLs.

---

## Closed: `6fd8e980` — and it was not a rewording

This item was raised as a wording problem: the row read "Gets the AnimData
sequence record for a token+mode combination", while the function takes a single
integer and returns `id * 0x1C0 + base`, so the token+mode framing belonged to
the caller. That much is true. The larger problem is that the record is not an
AnimData record.

**AnimData.d2 records are `0xA0` bytes.** `DATATBLS_LoadAnimDataTable`
(`6fd91e50`) is unambiguous about it: it allocates against
`"..\Source\D2Common\DATATBLS\AnimTbls.cpp"`, builds `"%s\AnimData.d2"` from
`"DATA\GLOBAL"`, and walks 256 hash buckets advancing by `field_0 * 0xa0 + 4`.
That is the familiar 160-byte record — `szName[8]`, `dwFrames`, `dwSpeed`,
`pFrameData[144]`. The 144-byte frame array shows up again in `6fd91ef0`, which
returns the record's `+0x08` and `+0x0C` and then scans bytes from `+0x10` with
the bound `0x8f` for the first nonzero frame event.

`6fd8e980` indexes with stride `0x1C0`, which is not that table. Following the
globals settles which table it is. `LoadObjectsTxtData` (`6fd8f0c0`) parses the
file named by the string `"objects"` with **record size `0x1c0`**, storing the
base in the same global `6fd8e980` reads and the count in the same global it
bounds-checks against. Its field descriptors give the layout directly:

| Objects.txt column | offset |
|---|---|
| `FrameCnt0..7` | `0xD8`, `0xDC`, `0xE0`, `0xE4`, `0xE8`, `0xEC`, `0xF0`, `0xF4` (dwords) |
| `FrameDelta0..7` | `0xF8` … `0x106` (words) |
| `Start0..7` | `0x129` … `0x130` (bytes) |

The loader then shifts `FrameCnt0..7` left by 8, making them 8.8 fixed-point.

The sole internal caller, `6fd835f0`, reads exactly those three arrays. It is a
switch on unit type; in the `case 2` arm — objects — it bounds-checks the mode
against 8 (objects have eight modes), then:

```
pUnit+0x48 = record[0xD8 + mode*4]     // frame count, already <<8 by the loader
short rate = record[0xF8 + mode*2]     // frame delta
pUnit+0x44 = record[0x129 + mode] << 8 // start frame
if (record[0x175] == 0) rate += random(unit seed)   // the Sync column
```

Three per-mode arrays, three matching offsets, one matching stride, two matching
globals. `6fd8e980` is the Objects.txt record accessor.

The row is now `GetObjectsTxtRecord`, described as indexing Objects.txt by
object type id and yielding the per-mode `FrameCnt`, `FrameDelta`, and `Start`
columns. The Stage 2 paragraph that sent the reader to this function for the
Amazon's `AMTNHTH` timing now sends them to `DATATBLS_LoadAnimDataTable` and
`ANIM_LookupAnimDataByPath`, which are the AnimData functions, and notes that
objects are timed from Objects.txt instead. The address, the stage, and the
surrounding prose are otherwise untouched.

---

## What changed in the document

| # | Change | Why |
|---|---|---|
| 1 | Provenance block: "293 function-address rows" → "Every function-address row in the tables below" | The specific figure could not be reproduced by any counting method used here, and it now contradicted the recounted summary. The coverage claim is preserved; the unverifiable number is not restated. |
| 2 | Stage 2 prose: the `GetAnimSequenceRecord` sentence rewritten | It credited an Objects.txt accessor with the AnimData.d2 lookup. |
| 3 | Stage 2 row `GetAnimSequenceRecord` `6fd8e980` → `GetObjectsTxtRecord`, purpose rewritten | Wrong table, and the token+mode framing was caller-level. |
| 4 | Stage 2 row `InitializeAnimFromGraphicsMode` `6fd95d30` removed | Not an animation function; the label was inherited from `6fd82820`. |
| 5 | Stage 2 row `SKILLS_GetActiveSkillAnimData` `6fd80460` → `SKILLS_GetActiveSkillNode`, purpose rewritten | Returns a `Skill*`, not animation data. |
| 6 | Function Count Summary replaced | Every figure was low and the `FUN_*` claim was false. |

## What was left alone

- `CleanupOverlayBufferState` at `6fb18be0`/`6fb18c30` — the document's name and
  its two-instance grouping are both correct.
- `InitializeMemoryBudget` at `6fb504d0` — correct.
- `UNITS_InitializeAnimationFromGfxMode` at `6fd82820` — correct.
- `UNITS_UpdateAnimationSpeedByMode` at `6fd83110` — correct, and better than
  the name in the database.
- Every address, call-graph edge, and string-table entry already verified.
- All narrative prose outside the one Stage 2 sentence.

---

## Recommended Ghidra renames

Read-only analysis; **nothing was renamed in the database.** These are for a
human to apply. The first four are cases where the document's name is better
than the one in Ghidra; the rest are spurious labels found while adjudicating,
where the existing name describes a different function entirely.

| Address | DLL | Current Ghidra name | Suggested | Why |
|---|---|---|---|---|
| `6fb18be0` | D2Client | `CleanupOverlayBuffer` | `CleanupOverlayBufferState` | Same routine as `6fb18c30`; match its name. |
| `6fb504d0` | D2Client | `InitializeClientMemoryBuffers` | `InitializeMemoryBudget` | Computes and clamps budgets; allocates nothing. |
| `6fd83110` | D2Common | `UNITS_SetGfxSelected` | `UNITS_UpdateAnimationSpeedByMode` | Sets animation speed from mode, weapons, and stats `0x43`/`0x44`/`0x45`. Its own comment body already says so. |
| `6fd80460` | D2Common | `GetActiveSkillField8` | `SKILLS_GetActiveSkillNode` | Returns `pInfo->[+0x08]`, a `Skill*`. (Twin: `6fd80420` → `SKILLS_GetActiveSkillNodeAlt` for `+0x0C`.) |
| `6fd8e980` | D2Common | `GetAnimSequenceRecord` | `DATATBLS_GetObjectsTxtRecord` | Indexes Objects.txt (stride `0x1C0`), not AnimData.d2 (stride `0xA0`). |
| `6fd95d30` | D2Common | `InitializeAnimFromGraphicsMode` | `ITEMS_MatchesRecordItemTypes` | Boolean item-vs-record item-type test; name inherited from `6fd82820`. **The copy-pasted doc comment on this function is wrong and should be cleared.** |
| `6fd835f0` | D2Common | `CalculateEquippedItemsServiceCost` | `UNITS_InitAnimRateByUnitType` | Switch on unit type that sets frame count, start frame, and rate. Nothing to do with repair cost; the attached comment is wholly spurious. |
| `6fd74430` | D2Common | `GetUnitDirection` | `ITEMS_IsItemOfType` | Tests an item against an item type via the ItemTypes equivalency bitmask. |
| `6fd721c0` | D2Common | `DumpProfilingStats` | *(needs analysis)* | Validates an item unit and returns a word in `AX` that the caller compares against 100. Current name and comment describe DRLG room timers and are unrelated. |
| `6fd91ef0` | D2Common | `GetAnimDataFrameInfo` | *(name is fine; comment is not)* | Returns an AnimData record's `dwFrames`/`dwSpeed` and scans its 144-byte frame array. The attached comment describes sorting a room's unit list — spurious. |

Two callee labels seen inside these functions are also clearly misattributed and
worth checking before anyone trusts them: `LOG_FindLogEntryByTag`, which is
reached from both AnimData accessors and behaves like the AnimData-by-name
lookup, and `BinkBufferGetError`, called from `ANIM_LookupAnimDataByPath`.

---

## Still open

- The txt file behind `6fd95d30`'s second argument was not identified. The
  record has a word at `+0x0E` gating on 100, seven item-type shorts at `+0x10`,
  and four exclusion shorts at `+0x1E`. The shape fits an affix- or
  runeword-style table, but the loader was not traced, so the document says only
  what the code does and names no file.
- The name/address bindings for the 300 addresses were verified by the earlier
  pass, not re-verified here. This pass re-derived only the seven functions it
  adjudicated, plus `6fd835f0`, `6fd8f0c0`, `6fd91e50`, `6fd91ef0`, `6fd74430`,
  `6fd74c30`, `6fd721c0`, `6fd80420`, `6fd80590`, `6fd80670`, and `6fd806d0`.
