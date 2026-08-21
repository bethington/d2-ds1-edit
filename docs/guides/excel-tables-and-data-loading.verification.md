# Verification report — Excel tables and data loading

This chapter is original research, not a conversion of prior documentation:
every claim in it was established directly against the vanilla game archives
and the `D2Common.dll` disassembly during drafting, rather than corrected
afterward against a pre-existing document. This report records what was
checked, how, and what remains open.

## Ground truth used

- **Archives:** `F:\D2VersionChanger\VersionChanger\LoD\1.13c\` and
  `...\1.09d\`, read via `tools/d2mpq.py` (the PKWARE-DCL-aware reader —
  see `docs/vanilla-data.md`). `selfcheck --version 1.13c` passed (8 tables
  at known-good counts, zero PD2 contamination markers) before any table
  claim was drafted.
- **Ghidra:** program `/Vanilla/1.13c/D2Common.dll`, image base `6fd50000`,
  2,764 functions. Confirmed as the intended vanilla binary, not a mod copy
  sharing the same preferred base, by hashing the file directly:
  `F:\D2VersionChanger\VersionChanger\LoD\1.13c\D2Common.dll`, SHA-256
  `59fa5928522f566f2bf99675571206ad70df889c89d3d07fa87edf5083e06e10`,
  679,936 bytes. `program=` was passed explicitly on every Ghidra call in
  this session (several other D2Common copies were open concurrently, at
  the same or different image bases: `/PD2Realm/D2Common.dll`,
  `/Mods/PD2-S12/D2Common.dll`, `/Vanilla/1.09d/D2Common.dll.0`, and a
  second `/Vanilla/1.13c/D2Common.dll.0` project copy).
- **Live fleet:** not used for this chapter. A vanilla-labelled member
  (`10.0.10.30:8790`, `lod-113c`) was located via `fleet_members` and its
  address space resolved via `fleet_resolve`, but the oracle memory-read
  path (`oracle_status`) reported no route available in this session, and
  reading a shared fleet member's live memory for a documentation task felt
  like the wrong tradeoff against fleet-slot discipline for a question the
  static evidence already answered convincingly. This is the one avenue
  from the skill's evidence ladder not exercised; see the open question
  below.
- **Date:** 2026-08-21.

## Delivery checklist

### 1. Table inventory (vanilla record counts, source archive)

Regenerated in full for this chapter — the checked-in `.vanilla-cache`
manifests were stale (only 6 of 27 tables tracked, left over from an earlier
`--table`-filtered run). Rebuilt with:

```
python tools/d2mpq.py tables 1.13c   # -> 27 tables
python tools/d2mpq.py tables 1.09d   # -> 23 tables
```

Both counts match the task brief's expectation exactly (27 and 23). The full
per-table source/records table in the chapter body is drawn directly from
this regenerated `manifest.json`; SHA-256 and byte counts are in the
manifest itself rather than duplicated in the chapter, per
`docs/vanilla-data.md`'s own framing of the manifest as the citation record.

Sanity figures cross-checked against the task brief and `docs/vanilla-data.md`:
Objects **573**, MonStats **734**, MonPreset **229** — all three confirmed
identical in the freshly regenerated manifest.

1.09d gap confirmed directly (not inferred): `monstats2.txt`, `monpreset.txt`,
`monplace.txt`, `monseq.txt` are reported "not in this version" by the
extraction tool itself, i.e. absent from every archive in the 1.09d chain,
not merely un-cached.

### 2. Loader family — addresses and disassembly confirmation

All of the following were confirmed by **address existence** (a function
genuinely starts there) and by reading raw **disassembly**, not only the
decompiled C, for every numeric constant cited in the chapter:

| address | claim | disassembly evidence |
|---|---|---|
| `6fd8f0c0` (`LoadObjectsTxtData`) | record size `0x1C0` (448) | decompiled source shows `HasItemType3InInventory(param_1,"objects",&local_c58,&g_dwData_0b98,0x1c0)` and the load-loop's own advance `local_c5c += 0x1c0`; both are literal immediates in the disassembly, not decompiler-synthesized |
| `6fdaef40` (generic loader) | dual-mode fork on `g_dwData_9e20` | decompiled + disassembled; suffix strings read directly from raw memory (`.txt` at `6fddda94`, `.bin` at `6fddda80`) rather than trusted from the decompiler's string rendering |
| `6fda9870` (`MONSTERS_LoadSuperUniquesTable`) | record size `0x34` (52) | disassembled: `PUSH 0x34` immediately precedes `CALL 0x6fdaef40`; loop increment `ADD EDX,0x34` |
| `6fd91e50` (`DATATBLS_LoadAnimDataTable`) | record stride `0xA0` (160) | disassembled: `LEA ECX,[ECX+ECX*4]` (×5) then `SHL ECX,5` (×32) = ×160 = `0xA0` — checked specifically because the task brief warned this database's decompiler has rendered at least one other stride wrong in a similar shape; this one held up |
| `6fdad800` (`OBJECTS_GetObjectName`) | stride `0x1C0`, base `g_dwData_0b94` @ `6fdf0b94`, count `g_dwData_0b98` @ `6fdf0b98` | disassembled directly: `CMP ECX,[0x6fdf0b98]` / `IMUL EAX,EAX,0x1c0` / `ADD EAX,[0x6fdf0b94]` |
| `6fd51800` (`DATATBLS_GetObjectsTxtRecord`) | stride `0x1B8` (440) | disassembled: `IMUL EAX,EAX,0x1b8` — real and correct as a number, but for a different table (see corrections below) |

### 3. `.txt` vs `.bin` verdict, with evidence

This is the chapter's central, and most heavily cross-checked, claim.
Evidence chain, all independently reachable:

1. **Mechanism** — the generic loader (`6fdaef40`) branches on a global
   flag at `6fde9e20`. Read `0`: opens `.txt`, parses via
   `ParseTabDelimitedText` + the `FormatDescriptor` array. Read nonzero:
   opens `.bin`, treats the first 4 bytes of the loaded file as the record
   count and the following bytes as an already-laid-out record array — no
   parsing at all.
2. **Default value in the shipped binary** — read **two independent ways**:
   - Ghidra's loaded image: `read_memory(6fde9e20, 4)` → `01 00 00 00`.
   - The untouched file on disk, parsed independently with `pefile` (RVA
     `0x99e20` off image base `0x6fd50000`, section `.data`) → `01 00 00 00`.

   Both return `1` (binary-selecting). This rules out the possibility that
   the Ghidra project's copy of the byte had been patched by unrelated
   prior work in this shared project — the second read never touched Ghidra
   at all.
3. **Live-code corroboration** — `LoadAllDataTables`'s own cleanup path
   contains `if (g_dwData_9e20 != 0) dwResourceBuffer = DAT_6fdf006c - 4;`
   before a `DeallocateResourceBuffer` call. A dead branch does not get a
   pointer-arithmetic special case; this one exists because a real
   allocation in binary mode is 4 bytes larger than the pointer callers are
   given.
4. **Archive corroboration** — `python tools/d2mpq.py list 1.13c --pattern
   "data\global\excel\*.bin"` enumerates 70 `.bin` files in `D2Exp.mpq`,
   essentially the full excel catalog, including `monstats.bin`,
   `missiles.bin` and `gems.bin` under `ENCRYPTED|FIX_KEY` — flags that
   only have a purpose if something opens these files through the normal
   decrypt-on-read path.
5. **Byte-level corroboration** — `objects.bin` extracted directly from
   `Patch_D2.mpq` by exact name (256,708 bytes): first 4 bytes (LE `u32`) =
   `573` (the known-good vanilla `Objects.txt` record count); bytes 4–9
   read `Dummy\0` (record 0's `Name`, matching `Objects.txt` row 1 exactly);
   `(256,708 − 4) / 573 = 448 = 0x1C0` exactly — the same record size
   `LoadObjectsTxtData` passes to the generic loader and the same stride
   `OBJECTS_GetObjectName` multiplies by.

**What was not resolved:** no write to `g_dwData_9e20` exists anywhere in
`D2Common.dll` (`get_xrefs_to` on `6fde9e20` returns 123 references, all
typed `READ`). Whatever causes an ordinary `.txt`-editing mod to work in
practice — and it plainly does, as a matter of two decades of community
practice — must happen outside this DLL. This chapter searched for a
`-txt`-style command-line string in `D2Common.dll` and in the only LoD-era
`Game.exe` available in this Ghidra project (1.14d) and found nothing
matching; it did not check `D2Client.dll`'s command-line handling, any
launcher, or a registry-based override, and does not claim to have found
the mechanism. This is marked `(unverified)` in the chapter rather than
either asserted or omitted. **This is the single highest-value follow-up**
if anyone continues this thread: tracing what, if anything, clears
`g_dwData_9e20` before `LoadAllDataTables` runs in a real launch would
settle it definitively, and a live memory read on a running vanilla 1.13c
process (before vs. after `LoadAllDataTables`) would be the fastest way to
check.

### 4. Load-order precedence, with evidence

- Precedence order taken from `tools/d2mpq.py`'s own `ARCHIVE_ORDER`
  constant, then **reproduced independently against the real trees** rather
  than trusted from the source comment: `python tools/d2mpq.py tables 1.13c`
  reports, per table, which archive it actually pulled from, and that
  matches the documented order in every one of the 27 tables (e.g.
  `Objects.txt`/`shrines.txt`/`monstats.txt` from `Patch_D2.mpq`;
  `lvlwarp.txt`/`objgroup.txt` from `D2Exp.mpq`, because `Patch_D2.mpq`
  doesn't carry those two).
- `Objects.txt` present in three archives with different precedence outcomes
  confirmed directly: extracting the same internal path from
  `Patch_D2.mpq`, `D2Exp.mpq`, and `D2Data.mpq` in isolation returns three
  different byte sequences; the version `tools/d2mpq.py tables` picks
  (matching what the game's own search order would pick) is the
  `Patch_D2.mpq` copy.
- `Patch_D2.mpq`'s listfile-less enumeration behavior confirmed directly:
  `list 1.13c --pattern "*objects*"` returns 0 named blocks out of 209 for
  `Patch_D2.mpq`, while `extract` of `objects.txt` and `objects.bin` by
  their exact names from that same archive both succeed — demonstrating the
  enumeration/lookup asymmetry the chapter describes, rather than asserting
  it from `docs/vanilla-data.md`'s prose alone.

### 5. Everything still unverified

Marked `(unverified)` in place, or called out explicitly, in the chapter:

- **What clears `g_dwData_9e20` before normal play**, discussed above — the
  chapter's largest open item.
- **The identity of `6fd51800` (`DATATBLS_GetObjectsTxtRecord`).** Confirmed
  it is *not* an `Objects.txt` accessor (wrong base globals, item/equipment
  callers), but its actual subject was not identified. Not needed for this
  chapter's claims, but left as a correction opportunity for whoever
  maintains this Ghidra project next.
- **`6fdae3c0` (`DATATBLS_OpenExcelFile`) has zero cross-references** found
  by either `get_function_callers` or `get_xrefs_to`, anywhere in the
  binary. It disassembles to real, coherent code (a single-mode file
  opener), so this chapter reports it as apparently unreferenced in this
  build rather than asserting it is dead code — a function reached only
  through an indirect function-pointer table would look the same to a
  static xref scan, and this chapter did not rule that out.
- **`InitializeCommonDataTables`'s debug-gated table batch.** A block of
  ~15 additional `HasItemType3InInventory` calls (`playerclass`,
  `bodylocs`, `storepage`, `elemtypes`, `hitclass`, `colors`, `hiredesc`,
  `monmode`, `plrmode`, `monai`, `monplace`, `skillcalc`, `misscalc`,
  `skills`, `events`) is gated behind `g_dwPrimaryTemplateDebugEnabled != 0`
  (confirmed `0` in retail) inside `InitializeCommonDataTables`. Since these
  tables obviously load and function in retail play, they must be loaded
  elsewhere in `LoadAllDataTables`'s ~50-function sequence; this chapter did
  not trace where. Not cited as a chapter claim, mentioned here only so the
  gap is on record.
- **The `.bin` mechanism was checked only on 1.13c.** The Version
  differences table marks 1.09d's equivalent flag "not checked" rather than
  guessing whether the same default holds; `D2Common.dll` 1.09d loads at a
  different image base (`6fd40000`) and re-deriving the equivalent global
  would need its own pass.
- **The `D2Common #10037` / `DATATBLS_CompileTxt` community-name attribution**
  for the generic loader came from a pre-existing comment already present
  in this Ghidra project (not authored during this session). The chapter
  attributes it as such and does not claim to have independently verified
  the ordinal number; what *was* independently verified is the export-table
  name (`HasItemType3InInventory`, confirmed via `list_exports` returning
  that literal string bound to `6fdaef40`) and the behavior (via decompile
  and disassembly).

## Claim-type tally

- **Type A (mechanical — address/count/stride):** ~15 distinct constants
  checked; all confirmed against disassembly, none found wrong. (Contrast
  with the task brief's warning about a decompiler-rendered stride error
  elsewhere in this database — the AnimData `0xA0` stride was checked
  specifically because it has the same "count × constant" shape, and held
  up under disassembly.)
- **Type B (interpretive — what a function does):** `LoadObjectsTxtData`,
  the generic loader, `MONSTERS_LoadSuperUniquesTable`,
  `DATATBLS_LoadAnimDataTable`, `OBJECTS_GetObjectName`,
  `DATATBLS_OpenExcelFile`, `DATATBLS_OpenExcelWithDebugSave`,
  `LoadAllDataTables` — all decompiled and read in full, not sampled,
  because this is an 8-function loader family, not a 250-function sweep.
- **Type C (contextual — "mods work by editing `.txt`"):** treated as
  well-established outside fact used to *stress-test* the binary-mode
  finding (if binary mode were unconditional and untouchable, `.txt`
  modding couldn't work at all) rather than as something independently
  re-derived; the tension is exactly what's flagged unverified above.
- **Type D (data/asset — archive contents, record bytes):** every count in
  the chapter's reference tables traced to a live `tools/d2mpq.py`
  extraction in this session, not carried over from `docs/vanilla-data.md`
  without re-running it.

## Corrections made during drafting

(Not corrections to a prior document — this chapter had none — but to my own
first-pass assumptions, caught before they reached the page.)

- Initially assumed, from the task's own evidence packet, that
  `LoadObjectsTxtData`'s record size was simply `0x1C0` with no further
  question. Re-examining `6fd51800` (`DATATBLS_GetObjectsTxtRecord`)
  surfaced a *second*, disassembly-confirmed stride (`0x1B8`) attached to a
  function whose Ghidra name also claims `Objects.txt`. Resolved by tracing
  which globals each accessor actually touches: `0x1C0`/`g_dwData_0b94` is
  the table `LoadObjectsTxtData` writes; `0x1B8`/`sgptDataTables+0xc18` is
  something else entirely, reached only from item/equipment code. The
  chapter cites `0x1C0` and explicitly warns readers off the `0x1B8`
  function by name.
- Initially took `6fd8e980`'s Ghidra label (`GetAnimSequenceRecord`) and
  header comment at face value while scanning for "the" `Objects.txt`
  accessor. Its own xref list (reads on `g_dwData_0b94`/`g_dwData_0b98`)
  contradicted the label directly; adjudicated in the label's favor of the
  *evidence*, not the name, per the skill's guidance to decompile before
  reflex-flipping — but also not blindly trusting the binary's label
  either. The chapter documents both the correct behavior and the
  incorrect label.

## Open questions for the author

1. What actually clears `g_dwData_9e20` (or forces the `.txt` path) during
   an ordinary launch — traced through `D2Client.dll`/`Game.exe`, or a live
   before/after memory read on a running vanilla member — would convert
   this chapter's largest caveat into a settled fact.
2. Whether `6fd51800`'s real subject (item/equipment, stride 440 bytes) is
   worth identifying and renaming in this Ghidra project, so the next
   analyst doesn't repeat the same near-miss this chapter caught.
3. Where in `LoadAllDataTables`'s ~50-call sequence `playerclass.txt` et al.
   actually load in retail, given `InitializeCommonDataTables`'s copy of
   that load is debug-gated and therefore inert.
