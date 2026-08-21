# Verification report: The `.tbl` String Tables

This chapter was written from scratch rather than corrected from an
existing draft — `.tbl` was, per the assignment, the last major
undocumented Diablo II data format in this book. This report records what
ground truth was used, what was checked, and what is still open.

## Ground truth

1. **Real archive data**, extracted with `tools/d2mpq.py` from the vanilla
   game trees at `F:\D2VersionChanger\VersionChanger\LoD\1.13c\` and
   `...\1.09d\`:
   - `data\local\lng\eng\string.tbl` (1.13c, from `D2Data.mpq`)
   - `data\local\lng\eng\expansionstring.tbl` (1.13c, from `D2Exp.mpq`)
   - `data\local\lng\eng\patchstring.tbl` (1.13c, from `Patch_D2.mpq`)
   - the same three files re-extracted for 1.09d, for the version-differences
     section
   - `data\global\excel\levels.txt` (1.13c, from `Patch_D2.mpq`, 137 records
     — matches the known-good count in `vanilla-data.md`)

   SHA-256 of every extracted file:

   | file | version | SHA-256 |
   |---|---|---|
   | `string.tbl` | 1.13c | `d6b177f4d78ef3d3797d308da6ba8e38db6ebf6ca544990bcb5f7d16968946a4` |
   | `expansionstring.tbl` | 1.13c | `eb532e9a52dc1c4c6b5d12e9ec8ba98be0066f995be7bb8207ca8ad49be9d428` |
   | `patchstring.tbl` | 1.13c | `79e729672af12834093c3598c8ee439b08f44edf9df6af777e85ef08b062b839` |
   | `string.tbl` | 1.09d | `d6b177f4d78ef3d3797d308da6ba8e38db6ebf6ca544990bcb5f7d16968946a4` (identical to 1.13c) |
   | `expansionstring.tbl` | 1.09d | `eb532e9a52dc1c4c6b5d12e9ec8ba98be0066f995be7bb8207ca8ad49be9d428` (identical to 1.13c) |
   | `patchstring.tbl` | 1.09d | `ce79242a4b1580ea1852ea4ff960efd312e74b62d7ce79264216b01f32ee5236` |

   `patchstring.tbl` in 1.13c's `Patch_D2.mpq` carries `MPQ_FILE_IMPLODE`
   with no `(listfile)` present in that archive at all (209 blocks total,
   confirmed via `python tools/d2mpq.py list 1.13c --pattern "*LNG\ENG*"`).
   Extraction succeeded because `d2mpq.py` requests the file by its exact,
   known path, which resolves directly against the archive's hash table.

2. **`D2Lang.dll` 1.13c**, imported fresh into the project's Ghidra database
   from `F:\D2VersionChanger\VersionChanger\LoD\1.13c\D2Lang.dll` (it was not
   already loaded) at `/Vanilla/1.13c/D2Lang.dll.0`, image base `6fc00000`,
   SHA-256 `206386a81f4046c693f2aca60b9ee19fffd05b1157bbb60992c0d4caad9b67ec`,
   335 functions after auto-analysis.

3. **`D2Client.dll` 1.13c**, already loaded at `/Vanilla/1.13c/D2Client.dll`,
   image base `6fab0000`, SHA-256
   `dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906` —
   checked for consumers of `D2Lang.dll`'s table-lookup functions.

4. **A from-scratch Python re-implementation** (`parse_tbl.py`, written for
   this chapter, not carried over from any existing tool) of the header
   parser, the ID table, the node array, the CRC-16, the ELF hash, and the
   linear-probe lookup — run against the complete real files, not samples.

Verified 2026-08-21.

## Method

Every structural claim in the chapter was derived from `D2Lang.dll`'s
**disassembly**, then cross-checked a second, independent way: implementing
the claim in Python and running it against every active record in all three
real tables (9,378 records total: 5,391 + 1,169 + 2,818). Where the
disassembly and the from-scratch parser agreed on every record with zero
exceptions, the claim is reported as confirmed. Two claims never reached
that bar and are marked unverified in the chapter — see below.

The decompiler was used only to form hypotheses, never as evidence on its
own. One place this mattered directly: the decompiled form of the loader
(`FUN_6fc0a130`) renders part of the allocation-sizing code as `count &
0x3fffffff`, which reads like a flags-in-the-top-bits scheme. The raw
disassembly at that address is a `SHL ECX,0x2` (multiply by 4, for a byte
allocation size) immediately followed, after the allocation call, by `SHR
ECX,0x2` (restoring the dword count for a `REP STOSD` zero-fill) — there is
no masking instruction anywhere in that code path. The "`& 0x3fffffff`" is
the decompiler's own reconstruction artifact from a shift-left/shift-right
pair, not a real operation on the file's data. No claim in the chapter rests
on that field carrying flag bits.

## Claim tally

| type | checked | confirmed | unverified |
|---|---|---|---|
| A — mechanical (offsets, sizes, addresses, counts) | 15 | 13 | 2 |
| B — interpretive (what a function does) | 9 | 9 | 0 |
| C — contextual (world/format claims) | 4 | 4 | 0 |
| D — data (real file contents) | 9,378 active records across 3 tables | 9,378 | 0 |

The two unverified type-A items are the header byte at `+0x08` and the node
dword at `+0x03` — both discussed below.

## What was confirmed, and how

**Header layout (all seven fields).** Read directly from disassembly of the
loader/validator (`FUN_6fc094e0`) and the lookup function (`FUN_6fc09360`),
then independently confirmed by parsing all three 1.13c tables and all
three 1.09d tables (six files) with the derived field offsets and getting
internally consistent results in every case (CRC field matches a from-
scratch CRC-16 computed over the derived `[X, S)` range in all six files;
`S` matches the file's own actual byte length in all six).

**Node layout (all six fields except `+0x03`).** `Active` (`+0x00`),
`StringID` (`+0x01`), `KeyOffset` (`+0x07`), `TextOffset` (`+0x0B`), and
`TextLength` (`+0x0F`) were each read from a specific disassembled
instruction (cited inline in the chapter) and then validated against every
active node in all three 1.13c tables: every `KeyOffset` points at a
NUL-terminated string inside `[X, S)`; every `TextOffset`/`TextLength` pair
stays inside `[X, S)`; every `Active` byte is exactly `0` or `1` (no third
value observed in 9,383 total node slots across the three tables).

**The 17-byte stride.** Confirmed twice independently in disassembly: once
in the lookup function via the `SHL EAX,0x4` / `LEA ECX,[EDI+EAX*1]`
16+1=17 trick (`6fc09396`–`6fc0939b`), and again in the loader's first pass
over the node array via a plain `ADD EBX,0x11` per-iteration increment
(`6fc0a325`).

**The hash function.** Read from `FUN_6fc08fe0`'s decompile (simple enough
that decompiler risk is low — no pointer arithmetic, no struct-field
inference) and confirmed empirically by re-implementing it in Python and
using it to look up every single active key in all three tables: every
lookup terminates at the correct node (see "self-lookup round-trip" below).

**The linear-probe lookup, including the probe-count field `P`.** This is
the strongest single result in the chapter. `P`'s purpose was not obvious
from the disassembly alone — it's read and compared against a running probe
counter, which only says "probe limit," not what determined the limit's
value. Computing, for every active key in each table, the actual distance
from `hash(key) % N` to that key's real node-array position, and taking the
maximum over the whole table, reproduces `P` **exactly** in all three
1.13c tables:

| table | computed max probe distance | header `P` | key needing the max |
|---|---|---|---|
| `string.tbl` | 5,192 | 5,192 | `x` |
| `patchstring.tbl` | 1,058 | 1,058 | `Hellfire Torch` |
| `expansionstring.tbl` | 2,453 | 2,453 | `MercX149` |

This was checked against every active key in every table, not a sample, and
the match is exact (not off by one in either direction) in all three files.

**Table precedence (`patchstring` > `expansionstring` > `string`).**
Confirmed in disassembly by the call order inside `FUN_6fc0a7b0`
(`6fc0a835`: patchstring; `6fc0a85d`: expansionstring; `6fc0a881`: string)
and the literal additive constants `ADD EAX,0x2710` (`+10000`, patch) and
`ADD EAX,0x4e20` (`+20000`, expansion) at `6fc0a841`/`6fc0a869`. Then
demonstrated empirically: `string.tbl` and `patchstring.tbl` share 87 active
keys; for the two cited in the chapter (`9bl`, `AmaOnly`) the text genuinely
differs between the two files, and by construction of the precedence order
`patchstring.tbl`'s copy is what the game shows.

**Numeric-ID routing (`GetStringByID`, `FUN_6fc09450`).** The three ranges
(`0`–`9999`, `10000`–`19999`, `20000`+) and the 16-bit-wraparound arithmetic
(`+0xD8F0`, `+0xB1E0`) were read from disassembly/decompile and confirmed
by looking up IDs `0`–`5`, `10000`–`10002`, and `20000`–`20002` through the
from-scratch Python `lookup_id()` and checking each one lands in the table
the formula predicts, with a real, sensible string attached (see the
transcript below).

**The K/M/B/T abbreviation IDs.** `FUN_6fc09590`'s literal ID constants
(`0x14d0`–`0x14d3` = `5328`–`5331`) and its hard-coded fallback characters
(`K`/`M`/`B`/`T`, read as raw bytes at `6fc10550`–`6fc1055f`: `4b 00 00 00
4d 00 00 00 42 00 00 00 54 00 00 00`) were both read from the binary
directly. Looking those four IDs up in `string.tbl` via `lookup_id()`
returns exactly `K`, `M`, `B`, `T` — the fallback and the real localized
value agree byte for byte in English.

**Self-lookup round-trip (the strongest confirmation of the hash + probe
mechanism together).** For every active node in all three tables, this
report extracted the node's own key string and ran it back through the
from-scratch hash-and-probe lookup, checking that the result lands on the
*same* node the key was taken from. It did not, on the first pass: 292 of
5,391 in `string.tbl`, 107 of 1,169 in `patchstring.tbl`, and 30 of 2,818 in
`expansionstring.tbl` "failed." Investigating why closed the loop
completely: every single one of those mismatches is explained by a
**duplicate key** elsewhere in the same table (most commonly the ID-only
placeholder key `x`/`X`) — the lookup correctly finds the *first* node with
that key text in probe order, which for a duplicated key is not necessarily
the node the check started from. Counting duplicate-key groups
independently (11/4/10 groups, accounting for exactly 292/107/30 extra
entries) matches the "failure" counts precisely. With duplicates accounted
for, the round-trip is 100% consistent across all 9,378 active records.

**`TextLength` includes the terminating NUL.** This was caught by the
verification process itself, not assumed: an early draft read `TextLength`
as "characters, not counting the NUL," which is the more common convention
and looked plausible from a handful of spot checks. Checking `TextLength`
against `strlen()` of the actual NUL-terminated bytes at `TextOffset` for
every one of the 9,378 active nodes across all three tables found `TextLength
== strlen + 1` in all 9,378 cases, zero exceptions — the field counts the
NUL. The chapter states it the corrected way.

**Bounds, on every entry.** For every active node in all three 1.13c
tables: `KeyOffset` is inside `[X, S)`; the NUL terminator following it is
inside `[X, S)`; `TextOffset` is inside `[X, S)`; `TextOffset + TextLength`
is `≤ S`. Zero violations across 9,378 records.

**Version stability (1.09d).** The same header/node/ID-table field offsets,
the same hash function, and the same CRC-16 were used unmodified to parse
1.09d's three tables, and every one of them validated (CRC match, `S`
matches file length, no out-of-range fields) — direct evidence the binary
format itself hasn't changed between the two versions, only content has
(and only in `patchstring.tbl`).

## What could not be confirmed

- **Header byte `+0x08`.** Observed as `1` in all six sampled files (three
  tables, two versions). No instruction in `D2Lang.dll` 1.13c reads this
  byte in any function this chapter examined (`FUN_6fc0a130`,
  `FUN_6fc094e0`, `FUN_6fc09360`, `FUN_6fc09450`, `FUN_6fc09050`,
  `FUN_6fc0a7b0`). Plausibly a compiler/format-version tag consumed only by
  the offline table-building tool, but that tool wasn't available to check
  against. Marked unverified in the chapter.

- **Node dword `+0x03`.** Checked against three hypotheses — cached hash
  value, node's own array index, and `StringID` — and matched none of them
  consistently. Across `string.tbl`'s 5,391 active nodes it equals the
  node's own array position (`node_idx`) in 2,323 cases (43%) and matches
  neither `node_idx` nor `StringID` in the remaining 3,068 (57%). No
  function in `D2Lang.dll` reads this field at runtime in any code path
  this chapter traced. Marked unverified in the chapter.

- **The exact byte-for-byte assembly of the "not xlated" fallback string.**
  The marker text itself — `" -not xlated call ken w"` at `6fc10568` — is
  confirmed byte-exact by direct memory read. The routine that builds the
  final fallback string around it (`FUN_6fc09fb0`) is dense, hand-tuned
  string-copy code with heavy register reuse that the decompiler renders
  only partially coherently; this report is confident the routine
  concatenates the missing key with that marker text and caches the result,
  but did not fully re-derive the precise final byte layout. The chapter's
  claim is scoped to what's certain (the marker text exists, byte-exact,
  and a miss does not crash the client) and flags the exact concatenation
  order as not fully traced.

- **The cross-module call path from `D2Client.dll` to `D2Lang.dll`'s
  lookup functions was not found.** `D2Lang.dll`'s PE export table (162
  entries, enumerated in full) contains only the `Unicode::` utility class
  and CRT-adjacent helpers — none of `FUN_6fc0a130`, `FUN_6fc0a7b0`,
  `FUN_6fc09360`, `FUN_6fc09450`, or any other function this chapter
  documents is exported by name or by bare ordinal. `D2Client.dll` does
  reference the literal string `"D2Lang.dll"` (at `6fb89f78`), but it has
  zero code cross-references in Ghidra's analysis, and a full enumeration
  of `D2Client.dll`'s 1,083 external-symbol imports contains no entry
  resolving to `D2Lang.dll` — meaning `D2Client.dll` does not statically
  import from it. The likeliest explanation is that `D2Win.dll` sits
  between the two (dynamically loading `D2Lang.dll` and re-exporting a
  wrapper by ordinal), but vanilla `D2Win.dll` 1.13c was not imported into
  this session's Ghidra project and this chapter did not chase that link
  further. Everything the chapter claims about the TBL subsystem itself is
  confirmed from inside `D2Lang.dll`; only the exact call site that invokes
  it from gameplay code is unconfirmed.

## Numeric-ID lookup transcript

For the record, the raw output of `lookup_id()` against real 1.13c data,
supporting the [Numeric IDs](tbl-string-tables.md#numeric-ids-a-second-front-door)
section:

```
id=0      (string.tbl)                    key='WarrivAct1IntroGossip1'      -> (Warriv's Act I gossip line)
id=1      (string.tbl)                    key='WarrivAct1IntroPalGossip1'   -> (Warriv's Paladin-specific variant)
id=10000  (patchstring.tbl, local id 0)    key='Cutthroat1'                  -> "Bartuc's Cut-Throat"
id=10001  (patchstring.tbl, local id 1)    key='ModStr5d'                    -> "Maximum Stamina"
id=10002  (patchstring.tbl, local id 2)    key='Mauler'                      -> "Mauler"
id=20000  (expansionstring.tbl, local id 0) key='A4Q2ExpansionSuccessTyrael' -> (Tyrael's post-Act-IV success line)
id=20001  (expansionstring.tbl, local id 1) key='A4Q2ExpansionSuccessCain'   -> (Cain's post-Act-IV success line)
id=20002  (expansionstring.tbl, local id 2) key='AncientsAct5IntroGossip1'   -> (the Ancients' Arreat Summit challenge line)
```

`id=10000` resolving to `Cutthroat1` → `"Bartuc's Cut-Throat"` (a unique
item name) is a good illustration of why `patchstring.tbl` exists: unique
items introduced or renamed by balance patches land exactly in the table
built to hold ID range `10000`–`19999`, without touching `string.tbl`'s
existing ID space at all.

## Sampling policy

Type A and type D claims were checked exhaustively — every header field in
every one of six files, and every active record (9,378 of them) in the
three 1.13c tables — because the from-scratch Python re-implementation made
100% coverage cheap. Nothing in this chapter's structural claims rests on a
sample.

Type B claims (what a function does) were each confirmed by reading the
specific function's disassembly directly rather than sampling; the function
count here is small enough (eleven functions cited in the chapter) that
full coverage was the natural approach, not an exception to a sampling
policy.

## Open questions for the author

- What actually calls into `D2Lang.dll`'s `GetStringByKey`/`GetStringByID`
  from `D2Client.dll` or `D2Game.dll`? Importing vanilla `D2Win.dll` 1.13c
  and searching its exports/imports for `D2Lang.dll` symbols is the
  concrete next step, but it wasn't in this chapter's assigned evidence
  list and wasn't chased.
- Is there a known community table-compiler (`.tbl` ⇄ plain-text) that could
  confirm the header byte at `+0x08` and the node field at `+0x03` from the
  *writing* side, rather than only the reading side this chapter had access
  to?
- Does the `" -not xlated call ken w"` marker appear, in this or a similar
  form, in any other Blizzard-era binary this project has already imported?
  It reads like a shared internal convention rather than a one-off.
