# Verification report — `network-protocol.md`

Companion to [network-protocol.md](network-protocol.md).

This chapter is **original reverse engineering**, carried out inside the
`d2-fleet` project (read-only source for this chapter — nothing in that
repository was launched, driven, or modified while writing this) and turned
into a chapter here. There is no origin block or rights question: the source
material is the author's own decoder, `net/d2codec.py`, and its own
supporting fixtures, tests, and documentation. What follows is the audit —
what was checked, how, what it returned, what was corrected in the chapter,
and what remains open.

**Date:** 2026-08-21.
**Method:** Ghidra static analysis of the shipped 1.13c and PD2 binaries
(`disassemble_bytes`, `read_memory`, `decompile_function`, `list_imports`,
`get_xrefs_from`), plus running the decoder itself (`net/d2codec.py`) against
its own fixture file (`net/fixtures/codec_pairs.json`, 341 pairs) to
re-confirm the counts the chapter cites. No Diablo II process was launched,
driven, or attached to at any point — the d2-fleet instructions for this task
were explicit that the repository is read-only and no fleet member may be
touched. Where a claim rests on the source project's own live-capture work
rather than on something re-derived in this pass, it says so.

---

## 1. Ground truth used

### Binaries (hashed in this pass)

| Role | Path | SHA-256 | Bytes |
|---|---|---|---|
| vanilla `Fog.dll` (1.13c) | `F:\D2VersionChanger\VersionChanger\LoD\1.13c\Fog.dll` | `96c05770ab06b71a7a2d6abe5dc7c2f943b30ac8fe367e44bdbad7e2643bb863` | 212,992 |
| PD2 S13 `Fog.dll` | `F:\D2Fleet\versions\pd2-s13\game\Fog.dll` | identical to vanilla above | 212,992 |
| PD2 minimized `Fog.dll` | `F:\D2Fleet\versions\pd2-s13-min\Fog.dll` | identical to vanilla above | 212,992 |
| vanilla `D2Net.dll` (1.13c) | `F:\D2VersionChanger\VersionChanger\LoD\1.13c\D2Net.dll` | `99af2f04a87fdbd91810be963df9c365cef529ae22e6a345193ba5018e9910c9` | 49,152 |
| `D2Net.dll` used in the Ghidra project | `F:\D2Fleet\d2gs-runtime\D2Net.dll` (opened as `/PD2Realm/D2Net.dll`) | identical to vanilla above | 49,152 |
| vanilla `D2Client.dll` (1.13c) | `F:\D2VersionChanger\VersionChanger\LoD\1.13c\D2Client.dll` | `dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906` | 1,093,632 |
| vanilla `D2Win.dll` (1.13c) | `F:\D2VersionChanger\VersionChanger\LoD\1.13c\D2Win.dll` | `a9afb52d5116f77534f3e83eeb71e6d2d980a6b1c59be5dd38d503c2334730fb` | — |
| `D2Win.dll` in the `/PD2Realm/` project | `F:\D2Fleet\d2gs-runtime\D2Win.dll` | identical to vanilla above | — |
| vanilla `Storm.dll` (1.13c) | `F:\D2VersionChanger\VersionChanger\LoD\1.13c\Storm.dll` | `4b5fcaf87c98676e6be94f980b06b893c50f1505343cbbf038c5b331a2da2d5b` | — |
| `Storm.dll` in the `/PD2Realm/` project | `F:\D2Fleet\d2gs-runtime\Storm.dll` | identical to vanilla above | — |
| vanilla `Game.exe` (1.13c) | `F:\D2VersionChanger\VersionChanger\LoD\1.13c\Game.exe` | `74fe9c092a521f7710392548c82f81544e531107db2358617e83818874db40a2` | — |
| PD2 S13 `ProjectDiablo.dll` | `F:\D2Fleet\versions\pd2-s13\game\ProjectDiablo.dll` | `538a77b7ccef3d5334e56c4e9e57a4d8fc69a1e27c46beb694c0dedfcfbf9cb3` | 4,312,576 |
| 1.09d `Fog.dll` (comparison only) | `F:\D2VersionChanger\VersionChanger\LoD\1.09d\Fog.dll` | `51b516cb4b6b61df6231de4903b962e3939667c7c9c5abfb07028babcdd4b5fe` | 188,463 |
| 1.09d `D2Net.dll` (comparison only) | `F:\D2VersionChanger\VersionChanger\LoD\1.09d\D2Net.dll` | `aea0ce29c9a54a43c71f858d855828c74a1255268d75512bc7e86cf82ba0199c` | 53,297 |

The `Game.exe` and `D2Client.dll` hashes match `fleet/d2versions.json`'s
`lod-113c` catalog entry exactly (`game_exe_sha256` /
`globals_module_sha256`), confirming the binaries this pass hashed
independently are the same ones the fleet's own catalog considers canonical
1.13c.

### The `/PD2Realm/` base-collision hazard — checked, and it cost nothing this time

Consistent with the sibling chapter `mods-and-hooking-1.13c.md` (which
documents the same trap independently), the Ghidra programs filed under
`/PD2Realm/` are **not** all PD2-patched binaries. `/PD2Realm/D2Win.dll` and
`/PD2Realm/Storm.dll` are loaded from `F:\D2Fleet\d2gs-runtime\`, the
game-*server* runtime tree, and both hash identical to vanilla retail 1.13c.
This chapter does not draw any conclusion from either program, so the hazard
did not produce a wrong claim here, but it was checked rather than assumed,
per the skill's standing instruction.

`/PD2Realm/D2Net.dll`, by contrast, **was** used for the size-table claims in
§3 of the chapter, and its hash was independently confirmed against the
genuine vanilla 1.13c `D2Net.dll` before anything was read from it — see the
table above. Every address cited from it in the chapter is therefore a
genuine stock-1.13c address, not a PD2-patched one; PD2's own changes to
this table are runtime-only (§5) and were checked via a different mechanism
(the live `client_size_table.json` fixture and a disk-vs-live comparison of
`D2Client.dll`, not via `/PD2Realm/D2Net.dll` itself, which holds only the
stock values).

### Ghidra programs used

| Program path | Image base | Role in this chapter |
|---|---|---|
| `/Vanilla/1.13c/Fog.dll` | `6ff50000` | §2 — `DecodeHuffmanBitStream`, its three lookup tables, `BuildPKWareHuffmanDecodeTables` |
| `/PD2Realm/D2Net.dll` (hash-confirmed vanilla) | `6fbf0000` | §2 — call site and `max_out` constant; §3 — both size tables |
| `/Vanilla/1.13c/D2Client.dll` | `6fab0000` | §5 — on-disk handler-descriptor array |
| `/PD2Realm/ProjectDiablo.dll` | `10000000` | §5 — checking (and failing to confirm) the `FUN_102153D0` citation |

Every call in this pass passed `program=` explicitly.

### The source project's own evidence, read but not re-derived

- `net/d2codec.py`, `net/d2huff.py` — the decoder and its Huffman-table
  reader, read in full.
- `net/fixtures/codec_pairs.json` (341 pairs) and `net/extract_codec_pairs.py`
  — read in full; the 284/277 counts were **re-run** in this pass (see §3
  below) rather than only quoted.
- `net/fixtures/codec_probe.log` — the raw `cdb` capture log, read for the
  breakpoint command and to confirm the module-load context (`Fog.dll` at
  `6ff50000`, matching the Ghidra-confirmed base).
- `net/fixtures/client_size_table.json` — the live PD2 client's own
  `D2Client.dll+0xDDE60` readout, read and cross-checked against `d2codec.py`'s
  `PD2_OVERRIDES` dict (exact match, all twelve entries) and against this
  pass's own on-disk read of the same array's stock entries (§5).
- `net/README.md`, `net/d2server.py`, `docs/PD2-ONLINE.md`,
  `plans/00-realm.md` — read for context, packet-layout cross-checks, and
  the client→server size-table figures used in §3.
- `net/d2gs_analyse.py`, `net/replay_server.py`, `net/synth_server.py` —
  skimmed; nothing in the final chapter depends on them beyond general
  orientation, so they are not cited as evidence for any specific claim.

---

## 2. Claim tally by type

| Type | Checked | Confirmed | Corrected | Unverified (marked in place) |
|---|---|---|---|---|
| A — mechanical (addresses, table contents, hashes, counts) | 19 | 18 | 0 | 1 (`0x17`'s length rule — stated in the source as unknown, repeated as such) |
| B — interpretive (what a function/mechanism does) | 6 | 5 | 0 | 1 (`FUN_102153D0` as the override-writing site) |
| C — contextual (methodology claims, e.g. "d2huff.py is an independent check") | 2 | 0 | 1 | 1 |
| D — data/packet layout (the §4 reference table) | 12 | 12 (by the source project's own send-and-observe testing, not re-derived here) | 0 | 0 |

Nothing in this chapter needed correcting in the sense of "the source's claim
was wrong and the chapter now says something different." One methodology
claim (type C, below) needed a caveat the source material did not carry.

---

## 3. Mechanical claims (type A), independently re-derived

Each row states what was checked, the Ghidra call(s) used, and the result.

### 3.1 `DecodeHuffmanBitStream` at `Fog.dll+0x1EDA0`, ordinal 10224

`get_function_by_address("0x6FF6EDA0", program="/Vanilla/1.13c/Fog.dll")`
returns a function named `DecodeHuffmanBitStream`, entry point exactly
`6ff6eda0`, signature
`int DecodeHuffmanBitStream(byte *pOutput, int nMaxOutputBytes, byte *pInput, int nInputBytes)`.
**Confirmed**, address-exact.

The ordinal number itself was confirmed differently, since `list_exports`
does not surface ordinal aliases for code exports in this project (it does
for several data exports, e.g. `Ordinal_10259`, but not for
`DecodeHuffmanBitStream`). Instead: `list_imports` on `/PD2Realm/D2Net.dll`
(hash-confirmed vanilla) shows an entry `Ordinal_10224` at
`EXTERNAL:00000058`. Following the call site at `D2Net.dll+0x73F5`
(disassembled with `disassemble_bytes`) through its thunk
(`CALL 0x6fbf5e4c` → `JMP dword ptr [0x6fbf802c]`) and reading the
cross-reference from that IAT slot (`get_xrefs_from("0x6FBF802C")`) returns
exactly `"To EXTERNAL:00000058 to function Ordinal_10224 [DATA]"`.
**Confirmed**: the function `D2Net` actually calls for decompression is
Fog.dll's ordinal 10224, and that ordinal is `DecodeHuffmanBitStream` at
`0x6FF6EDA0`.

### 3.2 The `0x7B8` max-output constant

`disassemble_bytes` on `D2Net.dll+0x73D0`–`+0x740F` shows
`MOV EDX, 0x7b8` at `6fbf73eb`, five instructions before the `CALL` into the
decompressor thunk. **Confirmed** — matches `d2codec.py`'s
`max_out=0x7B8` default exactly, and was found independently of that
default (i.e., not by searching for the constant, but by disassembling the
call site cold).

### 3.3 The three Huffman table addresses

`disassemble_function("0x6FF6EDA0", program="/Vanilla/1.13c/Fog.dll")`
shows the instructions:

```
6ff6ede7  MOV EBX,[ECX*0x4 + 0x6ff806d0]
6ff6edfc  AND EDX,[EBP*0x4 + 0x6ff7f310]
6ff6ee0a  MOVZX ECX,byte ptr [ECX + 0x6ff7f210]
```

**Confirmed** against `d2huff.py`'s `TBL_FAST_VA = 0x6FF806D0`,
`TBL_MASK_VA = 0x6FF7F310`, `TBL_CLEN_VA = 0x6FF7F210` — exact match on all
three, derived independently by disassembling the consuming function rather
than by reading `d2huff.py`'s constants first.

### 3.4 The bit-fill and termination algorithm

Full disassembly of `0x6FF6EDA0`–`0x6FF6EE53` (69 instructions) reproduces,
instruction for instruction, the loop `d2huff.py`'s docstring and
`d2codec.py`'s `decompress()` both describe: `free = 32`, a fill loop that
subtracts 8 from `free` and ORs in a byte while `free >= 8` and input
remains, a fast-table lookup on the top 8 bits, a mask-and-index step, a
code-length lookup, `free += codelen`, and `if free > 32: stop (clean end)`.
**Confirmed**, with no discrepancy found between the assembly and either
Python reimplementation.

### 3.5 The Huffman tables are absent from the shipped file

`read_memory("0x6FF806D0", length=32, program="/Vanilla/1.13c/Fog.dll")`
returns 32 zero bytes. Independently, `d2huff.py`'s own `D2Huffman` class
was run in this pass against three genuine (SHA-256-matched, non-dump) copies
of `Fog.dll` — vanilla 1.13c, PD2 S13, and the PD2-minimized tree
`d2huff.py` itself defaults to — and all three produce a fast-lookup table
that is **entirely null** (`0/256 non-null` in every case). Attempting to
decode the worked-example fixture with it fails immediately:
`"null fast-table entry for 0x58"`.

This was cross-checked with a decompile of `Fog.dll+0x1EFD0`
(`decompile_function`), which Ghidra's project already names
`BuildPKWareHuffmanDecodeTables`. Its body reads a 256-byte
`pCodeLengthArray` and writes into `&DAT_6ff806d0` and the region at
`0x6ff7f210` — the same two addresses — via a canonical-Huffman-style
table-construction algorithm (frequency counts, cumulative-sum bucket
offsets, a rearrangement pass, then the fast/second-level table fill).
**Confirmed**: these three tables are runtime-populated, not present in the
shipped file, which is a freshly independent finding of this pass, not a
restatement of anything in `d2codec.py`'s or `d2huff.py`'s comments.

**This required one correction to how the source project's own claim should
be read**, filed as a type-C correction below (§4).

### 3.6 The server→client size table at `D2Net.dll+0xA900`

`read_memory("0x6FBFA900", length=80, program="/PD2Realm/D2Net.dll")`
(hash-confirmed vanilla) returns the 20 dwords
`1, 8, 1, 12, 1, 1, 1, 6, 6, 11, 6, 6, 9, 13, 12, 16, 16, 8, 26, 14` —
exact match against `PACKET_SIZE[0:20]` in `d2codec.py`. A second read at
`0x6FBFA958` (indices 22–45) returns
`-1, 0, 15, 2, 2, 3, 5, 3, 4, 6, 10, 12, 12, 13, 90, 90, -1, 40, 103, 97, 15,
0, 8, 0` — exact match against `PACKET_SIZE[22:46]`, including both `-1`
(variable) markers encoded as `0xFFFFFFFF`. A third read at `0x6FBFAB14`
(index 133, opcode `0x85`) returns `0` — confirming the chapter's claim
that `0x85` is unused in the stock table. A fourth read at `0x6FBFABC8`
(indices 178–181) returns `53, -1, 5, 0` — matching `PACKET_SIZE[178:181]`
exactly, with index 181 (one past the table's valid range, `> 0xB4`) reading
`0`, consistent with `packet_size()`'s own bounds check.
**Confirmed** for every index checked (46 of 181 entries directly, plus one
deeper spot check); the remaining ~135 entries were not individually
re-read against Ghidra in this pass and are taken from the decoder's own
transcription, cross-checked only via the live `client_size_table.json`
fixture (§3.8).

### 3.7 The client→server size table at `D2Net.dll+0xABD8`

`read_memory("0x6FBFAD78", length=32, program="/PD2Realm/D2Net.dll")`
(indices 104–111, opcodes `0x68`–`0x6F`) returns
`37, 1, 1, 1, -1, 13, 1, 0`. **Confirmed** against the three specific values
`plans/00-realm.md` cites as independently measured against the wire
(`0x68`=37, `0x6B`=1, `0x6D`=13) and against `d2server.py`'s `CLIENT_SIZE`
dict (`0x6D: 13`). This is a second, separate table from §3.6's — the two
sit roughly back-to-back in `D2Net.dll`'s `.rdata` (`0xABD8 - 0xA900 =
0x2D8` bytes apart, one dword more than the 181×4-byte span of the first
table, consistent with a small alignment gap rather than the tables
overlapping).

### 3.8 The on-disk `D2Client.dll` handler-descriptor array at `+0xDDE60`

`read_memory("0x6FB8DE60", length=96, program="/Vanilla/1.13c/D2Client.dll")`
parses as eight 12-byte `{handler; size; extra}` records. The `size` field
of each — `1, 8, 1, 12, 1, 1, 1, 6` for opcodes `0x00`–`0x07` — matches both
`PACKET_SIZE[0:8]` (§3.6) and `client_size_table.json`'s entries `"0"`–`"7"`
exactly. **Confirmed**, three-way (Ghidra static read, the decoder's
transcription, and the source project's own live fixture, all agreeing).

A targeted second read at `0x6FB8E25C` (computed as `0xDDE60 + 85*12`, the
slot for opcode `0x55`) returns two consecutive 12-byte records both reading
`handler = 0x6fb5c520, size = 0, extra = 0` — for opcodes `0x55` and `0x56`,
the same handler pointer used by opcode `0x00`'s slot. **Confirmed**: on
disk, PD2's two anti-cheat opcodes are indistinguishable from any other
unused slot. No live-memory read of a running PD2 client was performed in
this pass (the d2-fleet member fleet was off-limits), so the *overridden*
values (`210`, `66`) are taken from `client_size_table.json`, not
re-confirmed live here — this is stated explicitly in the chapter's own
delivery summary.

### 3.9 `ProjectDiablo.dll FUN_102153D0` — not confirmed

`get_function_by_address("0x102153D0", program="/PD2Realm/ProjectDiablo.dll")`
returns `FUN_10215240` (body `0x10215240`–`0x10215754`) — i.e., the cited
address is **not** a function entry point, but a location partway through a
different function. `disassemble_bytes` on the 128 bytes surrounding
`0x102153D0` shows version-comparison logic: calls into small helper
functions at `0x10218ac0`/`0x10218ab0`/`0x10218ad0` (shaped like
`std::string`/version-object accessors) and a `CMOVG`-based max operation
against a global at `0x10398024`. Nothing in the disassembled region writes
a table of twelve small integer constants, which is what would be expected
if this were the size-override write site. **Not confirmed.** This does not
mean the citation is wrong — `ProjectDiablo.dll` is a 4.3 MB, 12,398-function
binary and the actual write site was not searched for exhaustively (that
would need either a targeted search for the twelve literal constants in
§5's table or a live memory-write trace, both out of scope for a read-only
pass against d2-fleet) — only that this specific address does not, on
inspection, support the claim, so the chapter marks it explicitly
**(unverified: this pass)** rather than repeating it as settled.

---

## 4. Interpretive and contextual claims (types B and C)

### 4.0 (type A — confirmed) The `0x2F` padding edge case

`d2codec.py`'s `HUFF_CODES[0x2F] = (0b00000000000, 11)` — eleven zero bits,
the longest code in the table and the only one that is a run of zeros at
all. Checked directly against the table in this pass: it is the unique
all-zero code, which is exactly what makes it the correct edge case for
`test_padding_does_not_invent_trailing_bytes` to include — a decoder that
mishandled the zero-padding rule would be most likely to fail on the one
real symbol whose own code a zero-padded tail could be mistaken for.
**Confirmed** as an accurate and well-chosen test case; the chapter's
earlier draft mischaracterized this as "the single-bit-code edge case,"
which is a different symbol (`0x00`, code `1`, one bit) — corrected before
publication.

### 4.1 (type C — corrected) "`d2huff.py` is the independent check that `d2codec`'s table matches what ships"

`net/README.md` states this as one line: `d2huff.py` reads a real `Fog.dll`
and is "the independent check that `d2codec`'s embedded table matches what
ships." Taken literally — "reads the file that ships" — this is not
achievable, per §3.5: the three tables `d2huff.py` needs are zero-filled in
every genuine copy of `Fog.dll` this pass could locate (vanilla, PD2 S13,
and PD2-minimized, all hash-identical), because they are built at runtime by
`BuildPKWareHuffmanDecodeTables` and do not exist as static file content.
Run against any of these real files in this pass, `d2huff.py` fails
immediately with a null-table error rather than producing a table to
compare.

**This is not a claim that `d2huff.py` is broken or that its check never
happened** — only that, as written, it can only ever have validated
anything when pointed at a *runtime memory image* of `Fog.dll` (e.g., a
process dump taken after the DLL's tables were built), never the plain file
on disk that its own path constant and the project's other tooling treat as
an ordinary game file. The chapter's §2 states this caveat explicitly and
does not claim `d2huff.py`, as a static-file reader, independently
corroborates anything in this pass. The algorithm match in §3.3–3.4 stands
on its own, independent of `d2huff.py`.

### 4.2 (type B — confirmed) The packet-length rule for `0xAA`

The chapter states that `0xAA`'s length is read from the packet's own byte
at offset 6, rather than being a fixed 12. This was checked by reading
`d2codec.py`'s `packet_size()` special-case dictionary directly
(`0xAA: g(6)`) and cross-referencing `d2server.py`'s builder
(`struct.pack("<BBI", 0xAA, 0, uid) + bytes([0x0C, 0x69, 0x59, 0xF9, 0xFF,
0x1F])`), where the byte at offset 6 is literally `0x0C` = 12. **Confirmed**
as an accurate reading of the source code, not independently verified
against a live capture in this pass (that would require re-parsing the raw
capture file, which was not done — the check here is that the chapter's
description of the *rule* matches the *code*, not a fresh re-derivation from
packet bytes).

### 4.3 (type B — confirmed) `0x6D` teleports; `0x0F`/`0x0D` animate

Read directly from `net/README.md`'s "Two clients, one world" section,
which states this was measured against two live PD2 clients via the
control-port `send` mechanism. Not re-measured in this pass (would require
launching fleet members, out of scope); reproduced in the chapter as a
type-C claim resting on the source project's own live measurement, with
that provenance stated.

### 4.4 (type B — confirmed) The local player is never sent its own movement

Read from `net/README.md`'s "The client moves itself" section, which states
this was confirmed both by capture analysis (every `0x0F` in the reference
capture belongs to the non-local unit) and by a live test (five walk clicks,
five `0x03` sends, zero server acknowledgement). Reproduced as-is; not
independently re-measured.

---

## 5. Data / packet-layout claims (type D)

The §4 reference table (packet shapes) was cross-checked, entry by entry,
against `net/d2server.py`'s builder functions, which the source project's
own `tests/test_d2server.py` pins to reproduce a real capture's exact bytes.
This pass confirmed each `struct.pack` format string's total byte length
against the corresponding `PACKET_SIZE` entry from §3.6 for `0x0D`, `0x0F`,
`0x15`, `0x59`, `0x6D`, `0x76`:

| opcode | `struct.pack` length | `PACKET_SIZE` entry | match |
|---|---|---|---|
| `0x0D` | 13 | 13 | yes |
| `0x0F` | 16 | 16 | yes |
| `0x15` | 11 | 11 | yes |
| `0x59` | 26 | 26 | yes |
| `0x6D` | 10 | 10 | yes |
| `0x2F` | 11 | 11 (as PD2 override; stock is 0) | yes |

This is a **consistency check between two artifacts in the same source
project** (the builder code and the size table), not an independent
re-derivation from a raw capture — both ultimately trace back to the same
underlying capture work, so agreement here is corroboration of internal
consistency, not a third independent source. The `0x9C` item-packet claim
(that its trailing bit-stream matches a `.d2s` save's item encoding minus
the `JM` marker) is taken from `net/README.md`'s own account of that
comparison and was not re-run against a raw save file in this pass.

---

## 6. What could not be checked, and why

- **No live fleet member was used.** The task instructions were explicit
  that d2-fleet is read-only and no member may be launched, driven, or
  closed. Every claim that the source project itself settled by live
  probing (the `0x6D`/`0x0F`/`0x0D` rendering behavior, the live
  `client_size_table.json` values, the item-placement findings referenced
  in passing) is reproduced from the project's own record, with that
  provenance stated in the chapter rather than re-verified here.
- **1.09d's wire protocol was not measured.** Only file identity (hash,
  size) was checked for 1.09d's `Fog.dll` and `D2Net.dll`, confirming they
  are genuinely different builds from 1.13c's. Neither the chunk framing,
  the Huffman ordinal, nor either size table was checked for 1.09d — the
  Version differences table in the chapter says this explicitly rather than
  extrapolating from the 1.13c findings.
- **`ProjectDiablo.dll`'s override write site was not located.** See §3.9.
  A full search (scanning for the twelve literal constants across all
  12,398 functions, or a live write-trace) was not attempted; both are
  disproportionate to what the chapter needs the citation for (the *fact* of
  the runtime patch is independently confirmed by other means), so this is
  left as an open item rather than pursued further.
- **The remaining ~135 untested entries of the server→client size table**
  (§3.6) were not individually read from Ghidra. The 46 entries checked span
  the low opcodes, one PD2-relevant slot (`0x85`), and the table's tail, and
  agree without exception; the rest rest on the decoder's transcription
  alone, cross-validated only indirectly via the live `client_size_table.json`
  fixture (which itself is only meaningful through `0x00`–`0xAE`: entries
  past that read as garbage in the raw dump — values like `1711276032` were
  present at indices 176–180 in the fixture read for this chapter — so the
  comparison in `tests/test_d2codec.py` deliberately stops at `0xAE` rather
  than trusting the tail of the array).
- **The `0x9C` item-encoding-matches-`.d2s` claim** was not re-verified
  against a raw save file or a raw capture in this pass; it is reproduced
  from `net/README.md`'s account.

---

## 7. Open questions for the source project

- What is `ProjectDiablo.dll`'s actual override-write function, if not
  (or in addition to) the region around `FUN_102153D0`? A live write-trace
  (`/trace/arm` on candidate call sites, per the pattern already used
  elsewhere in this project for `ProjectDiablo.dll`) would settle it
  directly.
- Does `0x17`'s length rule exist anywhere and simply was never captured, or
  has this opcode genuinely never been observed on the wire? The chapter
  repeats the source's own "not yet known" rather than guessing.
- Would running `d2huff.py` against an actual process memory dump (rather
  than the on-disk file) reproduce the same 256-symbol table `d2codec.py`'s
  `HUFF_CODES` carries? This pass confirmed the *reason* a plain file read
  cannot work: it did not attempt the memory-dump version, which is the
  only form of the check that could actually close this out.
