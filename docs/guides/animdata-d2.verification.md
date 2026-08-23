# Verification report: AnimData.d2

Companion to [animdata-d2.md](animdata-d2.md). This chapter was authored fresh (no
prior draft existed), so this report documents the evidence gathered to support it
rather than a set of before/after corrections to existing prose — except where noted,
where it also records discrepancies found in the read-only, sibling
[cof-pipeline-1.13c.md](cof-pipeline-1.13c.md) chapter that could not be corrected
there because that file is off-limits to this task.

## Ground truth used

- **Ghidra**: `D2Common.dll` at `/Vanilla/1.13c/D2Common.dll` (image base `6fd50000`,
  the exact path this task specified; a second, differently-analyzed open program
  shares the same nominal name at `/Vanilla/1.13c/D2Common.dll.0` and was not used).
  Cross-check target: `/Vanilla/1.09d/D2Common.dll.0` (image base `6fd40000`).
- **Vanilla game data**: `data\global\AnimData.d2`, extracted with `tools/d2mpq.py`
  (from `docs/vanilla-data.md`'s own toolchain) from the real 1.13c and 1.09d MPQ
  chains at `F:\D2VersionChanger\VersionChanger\LoD\{1.13c,1.09d}\`. Both extracted
  through `D2Exp.mpq` (the first archive in the priority chain that carries the file).
  `tools/d2mpq.py selfcheck --version 1.13c` was run first and passed (8 tables at
  known-good record counts) before trusting any extraction.
- **No live fleet member was used.** Everything below is static: Ghidra plus the raw
  file bytes, parsed independently in Python. No `/mpq/read`, `/state`, or `/call`
  reach-in was available or needed for this chapter's claims.
- Date: 2026-08-21.

| File | Source | Bytes | SHA-256 |
|---|---|---|---|
| `AnimData.d2` (1.13c) | `D2Exp.mpq`, 1.13c chain | 570,304 | `36cb704b85a7a4788533f12aa3e7c61c619dc650c8efa0cbd1d562446e1dab68` |
| `AnimData.d2` (1.09d) | `D2Exp.mpq`, 1.09d chain | 570,304 | `36cb704b85a7a4788533f12aa3e7c61c619dc650c8efa0cbd1d562446e1dab68` (identical) |

## Method

1. Read `book-conventions.md` and `vanilla-data.md` first, per the task's binding
   instructions.
2. Located the three entry points the task named already-found
   (`DATATBLS_LoadAnimDataTable @ 6fd91e50`, `ANIM_LookupAnimDataByPath @ 6fd91f70`,
   `ANIMATE_FreeAllAnimateTables @ 6fd68f90`) and confirmed each by **disassembly**,
   not decompiled C, per the task's explicit warning that this database's decompiler
   mislabels functions sharing a blanked-filename pointer at `0x6fdda728`.
3. Followed call graphs (`get_function_callers`/`get_function_callees`/`get_xrefs_to`)
   outward from those three functions until the whole load → hash → lookup → cache →
   consume chain was traced.
4. Extracted the real file and **parsed all 3,558 records** in Python, independently
   re-implementing the on-disk structure exactly as read from the disassembly (not
   copied from any prior doc), then checked every structural claim — bucket layout,
   record stride, hash function, name-length limit — against the whole file, not a
   sample.
5. Cross-checked the loader against 1.09d's `D2Common.dll` to settle the "Version
   differences" section with evidence rather than by assumption.

## Claim tally by type

| Type | Checked | Confirmed | Corrected/flagged | Unverified |
|---|---|---|---|---|
| A — Mechanical (addresses, byte offsets, strings, stride) | 14 | 14 | 0 | 0 |
| B — Interpretive (function purpose, data flow) | 11 | 9 | 2 (see below) | 0 |
| C — Contextual (event-code semantics, class-token identity) | 4 | 0 | 0 | 4 (see below) |
| D — Data/asset (file structure, record counts, hash, duplicates) | 8 | 8 | 0 | 0 |

Type-B sampling policy: every function named in the chapter's body or reference table
was individually decompiled **and** disassembled — none were taken on the strength of
a Ghidra label alone, per the task's explicit warning about this database's
decompiler. Functions surfaced by call-graph traversal but not used in the chapter
(e.g. `UNITS_CalcAnimationFrameOffset`, `InitializeAnimFromGraphicsMode` — see "Left
out" below) were decompiled to *decide whether to include them*, then excluded once
they turned out not to touch AnimData.d2's structures; that decompile still counts
against the type-B tally above as "checked."

## Structural verification (type D), full-file

All of the following were checked against **every one of the 3,558 records** in the
real 1.13c file, using a from-scratch Python parser written against the disassembly
(`C:\Users\benam\AppData\Local\Temp\...\scratchpad` working files, not committed):

| Claim | Result |
|---|---|
| 256 buckets, each `count:u32` + `count × 0xA0` bytes, contiguous | Parser consumes exactly 570,304 of 570,304 bytes with zero remainder |
| Record stride is `0xA0` (160 bytes) | Confirmed by loader arithmetic (`count*5<<5`) **and independently** by `GetAnimDataFrameInfo`'s field offsets (`0x08`, `0x0C`, `0x10`+`0x90`=`0xA0`) |
| Hash = uppercase(name), sum bytes, `mod 256` | **0 mismatches** across all 3,558 records — every record hashes to the bucket it is physically stored in |
| All stored names ≤ 8 characters | **0 exceptions**, consistent with the compare function's abort-on-`>8` guard |
| All stored names already uppercase on disk | **0 exceptions** — no record's name contains a lowercase byte |
| Bucket occupancy | 120 of 256 buckets empty; busiest bucket (10) holds 67 records; mean occupied-bucket size ≈ 26.16 |
| Distinct vs. total records | 3,558 total, 3,529 distinct names, 29 names duplicated |
| Duplicate-pair content | 20 of 29 pairs byte-identical; 9 pairs differ in `FramesPerDir` and/or `AnimSpeed` (table in the chapter body) |
| `FramesPerDir` range | min 1, max 200 (`42DTHTH`) — one record exceeds the 144-slot event array |
| `AnimSpeed` range | min 0, max 512 |
| Event-byte value distribution | `1`: 433, `2`: 148, `3`: 4, `4`: 0 (585 total nonzero bytes across 568 records) |

## Mechanical verification (type A), the load/lookup/consume chain

Every address below was confirmed present at exactly the stated address in the
disassembly (function start, not "somewhere nearby") and the field offsets were read
directly off the instruction operands, not inferred from decompiled variable names.

| Function | Address | Confirmed by (disassembly evidence) |
|---|---|---|
| `DATATBLS_LoadAnimDataTable` | `6fd91e50` | `MOV ECX,0x129` (alloc size), `LEA ECX,[ECX+ECX*4]; SHL ECX,5` (stride=count×0xA0), `MOV EDI,0x100` (256 buckets), `MOV [ESI+0x40c],0x800` / `MOV [ESI+0x410],0x100` (fallback constants) |
| `ANIM_LookupAnimDataByPath` | `6fd91f70` | `CALL 0x6fd93c30` (token builder) immediately followed by `CALL 0x6fd91dd0` (hash walk); fallback `MOV EAX,[0x6fdf0b4c]; ADD EAX,0x404` |
| Hash-and-scan | `6fd91dd0` | Uppercase loop (`CMP AL,0x61`/`0x7a`; `SUB AL,0x20`), byte-sum loop into a `byte`-width accumulator, `AND EAX,0xff`, `MOV EAX,[ESI+EAX*4+4]` (bucket-pointer index), scan loop `ADD EDI,0xa0` per non-match |
| Name compare | `6fd91d00` | Length walk to `CMP EAX,8; JBE`, abort codes `PUSH 0xd6` / `PUSH 0xd7`, two-dword equality compare (bytes 0–3, then 4–7) |
| Token builder (dual-mode) | `6fd93c30` | Two distinct `wsprintfA` call sites reading format strings at `6fde3428` (`"%s%s%s"`, read via `read_memory`, bytes confirmed) and `6fde3430` (`"%s\%s\COF\%s%s%s.COF"`, bytes confirmed); class-prefix strings at `6fde33fc` (`"DATA\GLOBAL\OBJECTS"`) and `6fde3410` (`"DATA\GLOBAL\MONSTERS"`), all read as raw bytes, not decompiled strings |
| `GetAnimDataFrameInfo` (the real one) | `6fd91ef0` | `CALL 0x6fd91dd0` then `MOV ECX,[EAX+8]` / `MOV ECX,[EAX+0xc]` (frames/speed out-params) and a scan `CMP ECX,0x90; JNC` over bytes at `[EAX+ECX+0x10]` |
| `GetDataTableRecord0x70` | `6fd82670` | `CALL ANIM_LookupAnimDataByPath` then `MOV [pUnit+0x14*4],piVar1` i.e. `unit+0x50` |
| `SetAnimEventFromFrameData` | `6fd7ee10` | `MOV EAX,[EDX+0x50]` (cached record), four `CMP byte ptr [EAX+ECX+0x10],{1,2,3,4}` checks each followed by `MOV [EDX+0x4e],BL` |
| `UNITS_SetGfxSelected` | `6fd83110` | Its own decompiled body (not just its comment) contains literal `GetUnitBaseStat(pUnit,0x43)`, `(...,0x44)`, `(...,0x45)` calls and clamp constants `0xaf` (175), `0xf` (15), `0x19` (25), `0x1e` (30) matching the per-mode branches described in the chapter; final stores are `*(short*)(pUnit+0x13*4)`=`unit+0x4C` |
| `AdvanceAnimFrameWithWrap` | `6fd7f060` | `*(int*)(p+0x44) += *(short*)(p+0x4c)`, then `if (nEndFrame*0x100 <= accum) accum -= (nEndFrame-nStartFrame)*0x100` — `×0x100` confirmed in the disassembly |
| `AdvanceAnimSubAccumulator` | `6fd7f090` | Same accumulator-at-`+0x44`, rate-at-`+0x4c` pattern, wrap against a boundary at `+0x48` |
| Global table pointer | `0x6fdf0b4c` | `LoadAllDataTables @ 6fdb6160`: `CALL 0x6fd91e50` at `6fdb623a` immediately followed by `MOV [0x6fdf0b4c],EAX` at `6fdb623f` — the loader's return value is stored there directly, and it is the same address both `6fd91dd0` and `6fd91f70` dereference |
| 1.09d loader | `6fd45120` | Instruction-for-instruction identical to `6fd91e50` (same `0x129`/`0xA0`-stride/`0x100`-bucket/`0x800`/`0x100` sequence); `AnimData.d2` string at `6fdc5340` in that build |

## Corrections / discrepancies relative to the COF-pipeline chapter

The task's off-limits list excludes `docs/guides/cof-pipeline-1.13c.md` from editing.
The following were found while tracing its cited functions and are recorded here
rather than fixed in place.

1. **`ExtractAnimFrameData` (COF chapter: `6fd7e6e0`, `6fd9fa30`, described as
   "Extracts frame data from AnimData records") does not touch AnimData.d2 at all.**
   Both addresses decompile and disassemble to an unrelated function operating on a
   **6-byte-stride** array reached through `sgptDataTables`-relative offsets
   (`iVar2 = index*6 + *(int*)(sgptDataTables+0xb98)`), used for what appears to be a
   stat/requirement lookup (`GetUnitBaseStat` calls at the end of the second instance,
   `6fd9fa30`), not the `0xA0`-stride AnimData record. This chapter does not cite
   either address for anything AnimData-related.
2. **Three of the four addresses the COF chapter lists under `GetAnimDataFrameInfo`
   are unrelated functions**, sharing the name only because Ghidra's decompiler pasted
   the same boilerplate comment ("Sorts the room's unit list by position...") onto all
   four. Only `6fd91ef0` genuinely reads AnimData records (confirmed above).
   `6fd71fb0` is a bit-flag setter/clearer unrelated to animation.
   `6fd8c640` sorts a room's unit list (the boilerplate comment is, for this one
   address only, apparently accurate) and aborts with error `0x249` on a null room —
   nothing to do with AnimData. `6fd9fe80` is a stat-flag-gated light-radius/overlay
   calculation reading from `sgptDataTables`, also unrelated. This chapter's function
   reference table cites only `6fd91ef0`.
3. `ANIM_LookupAnimDataByPath @ 6fd91f70`'s decompiler label, `BinkBufferGetError`, is
   spurious — confirmed by its disassembly (`CALL 0x6fd93c30` — the same
   `BuildCofPathString` the COF chapter already names at that address — immediately
   followed by `CALL` into the hash walk), not by anything resembling Bink video buffer
   error handling. The COF chapter does not cite `6fd91f70` itself, so there is no
   correction needed there; noted here because this chapter relies on the function and
   had to establish its real behavior independently. Separately, this pass found that
   `BuildCofPathString` (`6fd93c30`) is dual-mode — it also emits the bare `%s%s%s`
   AnimData lookup key via a second `wsprintfA` format string at `6fde3428`, a behavior
   the COF chapter's one-line description does not cover because it only needed the
   path-building branch.
4. `ANIMATE_FreeAllAnimateTables @ 6fd68f90`'s own decompiled comment claims it frees
   five tables including "AnimData" (source path `DATATBLS/AnimTbls.cpp`, matching
   `DATATBLS_LoadAnimDataTable`'s own abort-string path). **The five global addresses
   it actually frees** (`0x6fdf13c8`, `0x6fdf13a8`, `0x6fdf13b8`, `0x6fdf13d8`,
   `0x6fdf13d4`) **do not include `0x6fdf0b4c`**, the global this report independently
   confirmed (via the `LoadAllDataTables` call site) holds the AnimData table pointer.
   Either the AnimData table is freed under a different mechanism, or the comment's
   claim is wrong about which of the five pointers is AnimData. Not resolved — flagged
   in the chapter's function table rather than asserted either way.

## Unverified (type C) — marked in the chapter, not silently accepted

- **Event codes 1/2/3's exact Blizzard-internal meaning.** No string table or symbol in
  `D2Common.dll` names them. The chapter reports the empirical melee-vs-ranged
  correlation across all 95 `AM*` records as a pattern, explicitly not as a recovered
  label.
- **Class/monster identity of the `AI`, `MI`, `VM`, `DZ`, `64`, `3D` tokens** seen in
  the duplicate-record and bucket-20 examples. `data\global\excel\charstats.txt` (the
  only vanilla table checked for this) lists class names (`Amazon`, `Sorceress`,
  `Necromancer`, `Paladin`, `Barbarian`, `Druid`, `Assassin`) but **no 2-letter token
  column**, so player-class tokens beyond `AM` (already established by the COF chapter,
  and reused here without independent re-verification) were not cross-checked against
  a data table. `DZ` and `AI` prefixing several of the duplicate-name clusters is
  suggestive of Druid/Assassin by pattern only and was deliberately **not** asserted in
  the chapter body after an initial draft wrongly stated it as fact — see "Corrections
  made during drafting" below.
- **`EAnimData.d2`'s purpose.** Present in both 1.13c's and 1.09d's `D2Exp.mpq`/
  `D2Data.mpq` at the same size class, but no reference to that literal filename was
  found anywhere in either version's `D2Common.dll` via `search_strings`. Not traced
  further (would require checking `D2Game.dll` or `D2Client.dll`, out of scope for a
  chapter about `D2Common`'s table).
- **Why `ANIM_LookupAnimDataByPath`'s miss fallback (a zeroed record) and
  `GetAnimDataFrameInfo`'s miss fallback (the `0x800`/`0x100` loader constants)
  disagree.** Both are confirmed in the disassembly; neither is explained by any
  comment or nearby code. Left as an open question in the chapter.
- **No static caller found for `SetAnimEventFromFrameData` (`6fd7ee10`).**
  `get_function_callers` returned zero results. Either it is reached through a computed
  call (function pointer table) this pass did not resolve, or it is dead code in this
  build. Not asserted either way in the chapter — the chapter describes what the
  function does, not who calls it.
- **`UNITS_CalcAnimationFrameOffset` (`6fd7e840`) and `InitializeAnimFromGraphicsMode`
  (`6fd82820`, the function the task's evidence pointer names
  `UNITS_InitializeAnimationFromGfxMode`)** were decompiled and found to initialize a
  *different* per-unit accumulator (fields at `+0x30`/`+0x34`/`+0x38`/`+0x3C`/`+0x48`,
  not the `+0x44`/`+0x48`/`+0x4C` fields `AdvanceAnimFrameWithWrap` consumes) from a
  "graphics mode" record reached through `unit[0x2a]+0x10`, indexed by
  `DAT_6fdecf40`/`DATATBLS_GetMonStats2TxtRecord` rather than through the
  `0x6fdf0b4c` AnimData global. This looks like a parallel, monster-graphics-mode-table
  path, **not confirmed to touch AnimData.d2**, and was deliberately left out of the
  chapter's core narrative rather than folded in speculatively.

## Corrections made during drafting (self-caught, not from any prior document)

Two drafting errors were caught and fixed before publication, recorded here for
transparency since they involved unverified claims that briefly entered the draft:

1. An early draft of the "Quirks" section asserted `MINUHTH` was "the Druid's neutral
   hand-to-hand stance." No data source used in this pass confirms `MI` = Druid (see
   "Unverified" above); the claim was removed and replaced with an explicit note that
   the token's owner was not identified.
2. The same section originally called `AITNBOW` "the Assassin's town-mode bow stance,"
   on the same unverified basis (`AI` pattern-matched against the Assassin's
   presumed token by analogy with `AM`=Amazon, never checked against a table). Revised
   to describe the record by its confirmed structure (town mode, bow weapon class)
   without asserting the owning class.

## What was not attempted

- No live fleet member was launched; every claim rests on Ghidra plus offline file
  parsing, both of which were sufficient for this chapter's scope (a data table and its
  loader, not runtime behavior that only a running process would reveal).
- `D2Client.dll` and `D2Game.dll` were not searched for additional AnimData.d2 or
  `EAnimData.d2` consumers; this chapter is scoped to `D2Common.dll`'s table and its
  immediate load/lookup/consume chain, consistent with the COF-pipeline chapter's own
  division of labor across DLLs.
- PD2 or any other mod's `AnimData.d2` was not examined. The task specifies vanilla
  data only; no mod-note callouts appear in the chapter because no mod claim was
  checked.
