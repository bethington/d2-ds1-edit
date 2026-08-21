# Book material — status and evidence

An index of the documentation prepared for publication: what exists, what
standard of evidence stands behind each claim, and what is still open. Every
chapter has a companion `.verification.md` recording claim-by-claim verdicts
and the corrections applied. Produced 2026-08-21 by the `doc-verify-enrich` and
`preservation-modernize` skills.

**Read the verification report before citing a chapter.** The chapters are
written to read cleanly; the reports are where the uncertainty lives.

**Patch 1.13c is the book's baseline** — it is the most-modded version, so it is
what a reader is assumed to be running. Every chapter states 1.13c behaviour
unqualified and marks other versions as variations. The binding rules are in
[book-conventions.md](book-conventions.md); game data comes from vanilla
archives via `tools/d2mpq.py`, never from the repository's own tables
(see [vanilla-data.md](vanilla-data.md)).

## Chapters

| Chapter | Source | Evidence | Open |
| --- | --- | --- | --- |
| [COF pipeline](guides/cof-pipeline-1.13c.md) | original RE | ~291 rows / 300 addresses + 8 strings vs retail 1.13c D2Client, D2Common, D2CMP | full behavioural pass over all functions |
| [DS1 map format](guides/ds1-map-format.md) | original, w/ Siramy lineage | **2,276 vanilla `.ds1` from the 1.13c MPQs**; 5,174,713 cells, 885,185 decomposed bit by bit; loader from disassembly; version census across all 38 builds | runtime cell→tile handoff; several `prop1`/cell bits |
| [DT1 tile format](guides/dt1-tile-format.md) | **reconstructed** from a lost page | 360 files, 26,005 tile headers, 564,457 sub-tile headers, byte by byte; D2CMP + D2Common | flag-bit meanings, v4.1 record format |
| [Sprite formats: DCC + DC6](guides/sprite-formats-dcc-dc6.md) | original RE | **100% of both archives** — 21,692 DCC (3,304,078 frames), 1,654 DC6 (26,316 frames), zero parse failures; D2CMP decoders from disassembly | DC6 `flip`, DCC `variable0`, bit-width table on other patches |
| [D2S save format](guides/d2s-save-format.md) | Siramy `d2ref`, 2001 | 580 claims (462 confirmed, 26 corrected); all 525 corpus saves read, no sampling; D2Client cross-check | 92 unverified, incl. cow level, `unknownq` bits |
| [Excel tables & data loading](guides/excel-tables-and-data-loading.md) | original RE | 27 vanilla tables (1.13c) + 23 (1.09d); `DATATBLS_*` loader family, every stride from disassembly | **what sets the `.txt`/`.bin` flag** — see below |
| [Monsters and objects](guides/monsters-and-objects.md) | Siramy tutorial, 2010 | 61 claims, no sampling; D2Common 1.09d + 1.13c; re-verified against vanilla tables with zero discrepancies | 5 runtime/attribution items |
| [Mods and hooking](guides/mods-and-hooking-1.13c.md) | original research | SHA-256 diff of all 24 PD2 binaries vs retail; full load chain from shipped bytes; independent PE parser | which patches actually take at runtime |
| [AnimData.d2](guides/animdata-d2.md) | original RE | all **3,558 records** parsed, proven by exact byte-accounting (`256×4 + 3,558×160 = 570,304`, no remainder); hash verified against every record, zero mismatches; 1.09d byte-identical | event codes 1–3, `EAnimData.d2` |
| [TBL string tables](guides/tbl-string-tables.md) | original RE | all **9,378 active records** across three real tables, not a sample; format solved from `D2Lang.dll` disassembly; precedence demonstrated on shipped data | header `+0x08`, node dword `+0x03` |
| [Palettes and colour](guides/palettes-and-colour.md) | original RE | every generator function **reimplemented independently and byte-diffed** against the shipped `Pal.PL2`: 344,064 of 443,175 bytes (77.6%) reproduced with zero mismatches | the hue-rotation/screen-blend ~22%, `palshift` table semantics |
| [Network protocol](guides/network-protocol.md) | original research — the closing part | 341 live capture pairs; `Fog.dll` ordinal 10224 confirmed instruction-for-instruction at `0x6FF6EDA0`; both D2Net size tables read from memory | which PD2 site writes the overrides |
| [ds1edit manual](getting-started/manual.md) · [overview](getting-started/overview.md) · [tutorial 1](tutorials/01-basic-map-editing.md) | Siramy, 2004–2011 | 12 data claims re-verified against vanilla archives; format claims now sourced to the DS1 chapter; [one report](getting-started/siramy-conversions.verification.md) covers all three | ~35 keyboard sections (the 2003–2006 binary is absent) |

## The `.txt`/`.bin` global: nothing writes it, anywhere

The generic table loader forks on a global at `0x6fde9e20`, and in shipped
retail 1.13c that global holds **1** — confirmed at rest from Ghidra's image and
from a from-scratch `pefile` parse of the untouched DLL, and confirmed **live**
by reading `D2Common.dll + 0x99e20` out of a running vanilla 1.13c fleet member
(`01000000`).

The obvious hypothesis was a cross-module write — a command-line or launcher
path setting it. **That hypothesis is now excluded.** Every one of D2Common's
**123 xrefs is a READ, with zero writes**, and a byte-pattern search for the
literal address finds no reference at all in `D2Client.dll`, `D2Game.dll`,
`Fog.dll`, or `Game.exe`. Nothing in the shipped 1.13c binaries ever changes it.

So the value is a compile-time constant in practice, and if the fork selects
`.bin`, the `.txt` branch is unreachable in retail 1.13c without patching the
value — which is exactly the kind of thing a mod's runtime patcher does.

**One caveat worth chasing before this goes in the book.** The xref list is
dominated by `*_Free*` functions — `ANIMATE_FreeAllAnimateTables`,
`DATATBLS_FreeMonsterAndHireTables`, `STATS_FreePropertiesTable`,
`SETS_FreeSetItemsTable` and many more — alongside the loaders. A global read by
every table *freer* as well as every loader is at least as consistent with an
allocation-pool or memory-mode selector as with a file-format selector. The
"nothing writes it" finding holds either way, but the flag's **semantics**
deserve a second look before the chapter states what it selects.

## Coverage is complete

Every major Diablo II format now has a verified chapter — DS1, DT1, DCC/DC6,
D2S, COF, AnimData, TBL, the excel tables and the archives that hold them —
plus three parts of original research: mods and hooking, the network protocol,
and the palette pipeline.

What remains is not more chapters but **finishing**: closing each chapter's
residual unverified items (the reports list them), settling the rights question,
and an editorial pass for a consistent voice across eleven documents written in
parallel.

## What these passes contributed beyond verification

- **Answered a question the original author left open in 2010.** Siramy found
  preset entry 46 holding `652`, wrote "For some reasons this is not the Monster
  with hcIdx", and stopped. The preset unit index is a concatenated space; 1.09's
  superunique block starts at 612, proven by four entries agreeing on the constant.
- **Resolved `part_2`'s two "tricky" bytes.** All 16 status fields are 32-bit and
  the six vitals are 8.8 fixed-point, so the integer part sits at +1/+2 — his
  "skip one, read two, skip two" was a walk through six dwords. His own guess was
  right. Every vital is an exact multiple of 256 in 19/19 saves.
- **Settled the save-format version drift empirically.** File size (`0008h`) and
  checksum (`000Ch`) appear at v92 (1.09), moving the character name to `0014h`
  and shifting every later section by `0CDh`. Independently confirmed in code:
  D2Client reads a status word from `+0x18` below version `0x5C` and `+0x24` at
  or above — three inserted dwords. **Both layouts are presented side by side;
  no original value was overwritten.**
- **Reconstructed a chapter whose source page no longer exists**, decoding its
  orphaned diagrams by falsifiable prediction against real files.
- **Fixed the author's own flagged errors**: the Act III quest slots really were
  reversed; `part_5`'s copy distance is `4Ch`/76, as his own prose said two
  sentences later; blank Act V waypoint and quest names filled.

## Caveats that apply across the set

- **The Ghidra database is not ground truth.** Names *and* comment blocks
  sometimes describe an entirely different function, and decompiled pseudocode
  has rendered at least one constant wrongly. Published constants were confirmed
  against disassembly.
- **`assets/excel/` is PD2-derived, not vanilla** (`objects.txt`: 627 records vs
  573). Data claims were checked against real archives, not the checked-in copies.
- **Beware shared ancestry.** `src/core/dt1.c` derives from Siramy's own source,
  so their agreement is not independent corroboration; the DT1 chapter does not
  lean on it.
- **Version-scope everything.** Several claims are correct for the era they were
  written in and wrong today — the tutorial's "first empty row" warp at Id 83 is
  now occupied by `Act 4 Mesa to Hellcaves`, and following it overwrites a
  shipped warp.

## Rights — the position taken

No page in the Siramy archive carries a licence, copyright notice, or
republication grant. **The project's position, decided by Ben Ethington on
2026-08-21, is an explicit fair-use judgment** rather than a permission sought
from the author.

Fair use is a defense rather than a clearance, and it is only as good as the
record behind it, so this section is that record. Two things make the position
defensible and both are already true of the material:

**1. Attribution is universal and prominent.** Every chapter derived from the
archive opens with an Origin block naming Paul Siramy, the source page, and its
original date. Corrections credit rather than erase — "Siramy documented X; on
1.13c it is Y" — and his self-flagged uncertainties are preserved as his. The
repository's [NOTICE](../NOTICE) separately records the code provenance and
states that the MIT grant covers only this project's modifications, not a
relicensing of his work. It also invites him to change the terms, and that
invitation stands: if Paul objects, the terms change.

**2. The work is transformative, and measurably so.** These are not
transcriptions. Every factual claim was re-verified against the game binaries
and vanilla archives; claims were corrected, version-scoped to a 1.13c baseline
the originals predate, and extended with findings the originals do not contain —
including answers to questions their author left explicitly open.

**How much of each chapter is his, honestly stated:**

| Chapter | Derived from the archive | Original to this project |
| --- | --- | --- |
| D2S save format | structure and much factual content from `d2ref` (2001) | the format-96 re-centring, both layouts, the empirical version-drift proof, `part_2`'s "tricky bytes" resolved, his Act III quest error fixed |
| Monsters and objects | his 2010 tutorial's narrative and worked example | 1.13c re-centring, the concatenated-index-space answer to a question he left open, six ID/object hazards he never noted |
| ds1edit manual · tutorial 1 | his documentation, restored to what he actually wrote | verification against vanilla data, version-scoping, repair of a prior conversion that had silently modernised his words |
| DT1 tile format | his C sources and surviving diagrams; the original page is **lost** | the entire reconstruction, verified across 360 files and the game's own loader |
| DS1 map format | naming lineage only (his parser is this repo's ancestor) | essentially all of it — 2,276 vanilla files, the loader from disassembly |
| COF · sprites · excel · AnimData · TBL · palettes | nothing | entirely original reverse engineering |
| Mods and hooking · network protocol | nothing | entirely original research by this project |

Six of the eleven chapters contain no archive-derived content at all. Of the
five that do, one reconstructs a page that no longer exists anywhere.

**What this position does not cover**, and should not be stretched to: bulk
reproduction of the archive's pages as pages, or republishing his images beyond
the few the DT1 chapter needs to carry information a diagram encodes. Where an
image is reproduced, it is credited in place.

If the book moves toward commercial publication, revisit this with a publisher's
counsel — the analysis above is the project's own reasoning, not legal advice,
and a publisher will want its own view.
