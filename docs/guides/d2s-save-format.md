# The .d2s Save File

A Diablo II character is 900-odd bytes on disk. Everything the game will
remember about her — which acts she has cleared, which waypoints she can reach,
which of thirty skills she has spent points on, and the exact strength value the
menu will show before she is even loaded — is packed into a file smaller than
this page of text.

In July 2001, working from nothing but a hex editor and the patience to compare
saves before and after doing one thing in-game, Paul Siramy wrote down where all
of it lives. His offset table was right. It is also, for any save written after
September 2001, wrong from byte 8 onward — because patch 1.09 inserted three
fields into the header and pushed everything after them down.

Both statements are true. This chapter documents the header 1.13c writes —
what every save made in the last fifteen-odd years actually looks like — and
treats Siramy's original layout as what it now is: the version before the
break, still correct on its own terms, marked wherever it differs.

---

> ## Origin
>
> This chapter is a modernization of **D2S Unofficial Documentation** by
> **Paul Siramy** (`paul.siramy@free.fr`), published at
> `http://paul.siramy.free.fr/d2ref/eng/`, last updated **28 July 2001**.
> The original pages are preserved verbatim in this repository under
> [`docs/preservation/siramy/paul.siramy.free.fr/d2ref/eng/`](../preservation/siramy/paul.siramy.free.fr/d2ref/eng/)
> and are the authoritative copy; where this chapter and the archive disagree,
> the disagreement is documented rather than smoothed over.
>
> Siramy did not claim sole discovery, and neither does this chapter. His own
> credits page names **the Diablo II Save Game Mapping Project** — Lee Hamel
> (Juicy), Glenn C. (Mephiston), Terje B. (Instant), Mike Harrison (Polaris),
> IceTeaMan, and Sephiroth — whose documentation and editor he built on, plus
> **Jamella** (editor and source) and **TheTelamon** (CV5). That chain is part
> of the record.
>
> **Source pages used** (from the English tree, which is ten days newer than the
> French one and the only tree carrying the Act V updates):
> `part_1`, `part_2`, `part_3`, `part_4`, `part_5`, `part_5b`, `signatur`,
> `hardcore`, `name`, `class`, `title`, `playconf`, `unknown1`, `levlcopy`,
> `menulook`, `shortcut`, `handskil`, `location`, `quests`, `quests_2`,
> `waypoint`, `unknownq`, `datatype`, `index`, `history`.
> Per-file SHA-256 and capture provenance are in
> [`docs/preservation/MANIFEST.tsv`](../preservation/MANIFEST.tsv).
>
> **What was changed:** converted to Markdown; every offset re-checked against
> real save files, the game's own data tables, and the game's own code;
> version-scoped into two layouts rather than one; the author's four
> self-flagged unknowns investigated and three of them resolved; blanks he asked
> for and never received (Act V waypoint names, Act V quest names) filled in
> from the game's string tables.
>
> ### Rights — the position taken
>
> **No page in the archive carries a licence, a copyright notice, or a
> republication grant.** The project proceeds on an explicit fair-use
> judgment rather than seeking Siramy's permission; the full reasoning and
> its limits are recorded in [BOOK-STATUS.md](../BOOK-STATUS.md). If Paul
> Siramy objects, the terms change.

> ## Provenance of the verification
>
> Verified **21 August 2026** against four independent sources:
>
> 1. **525 real save files** at `c:\tmp\d2-saves-corpus\` (per-file SHA-256 and
>    origin in its `manifest.jsonl`), spanning save-format versions 71, 87, 89,
>    92, 96, 97, 98, 99 and 105 — that is, 1.00 through Diablo II: Resurrected.
> 2. **The game's own code** in Ghidra: `D2Game.dll`, `D2Common.dll` and
>    `D2Client.dll` from vanilla LoD **1.13c** (image bases `6fc20000`,
>    `6fd50000`, `6fab0000`), plus `Game.exe` 1.14d for the checksum routine,
>    which is a Fog.dll import in 1.13c.
> 3. **The game's own data**: `Levels.txt`, `Composit.txt`, `PlayerClass.txt`,
>    and the `string.tbl` / `patchstring.tbl` / `expansionstring.tbl` string
>    tables from an extracted vanilla LoD tree.
> 4. **A working parser**, `d2-fleet/tools/d2s.py`, read as a second opinion.
>
> Every claim below carries its evidence. Claims that could not be settled are
> marked **unverified** and listed in full at the end. A companion audit records
> each claim, verdict and correction:
> [`d2s-save-format.verification.md`](d2s-save-format.verification.md).

---

## 1. The save-format version

Everything in this chapter depends on one number: the **save-format version**,
a 32-bit little-endian integer at offset `0004h`. It is not the game's patch
number. It is a format revision, and the game refuses files whose version it
does not recognise. **1.13c always writes `60h` — decimal 96 — and every
offset in this chapter is that version's layout unless a callout says
otherwise.**

| Version | Hex | Game |
|---|---|---|
| **96** | **`60`** | **1.10 – 1.14 — the layout this chapter documents** |
| 92 | `5C` | 1.09 |
| 89 | `59` | 1.08 |
| 87 | `57` | 1.07 (LoD) |
| 71 | `47` | 1.00 – 1.06 (classic) |
| 97 – 99, 105 | `61`, `62`, `63`, `69` | Diablo II: Resurrected — out of this book's and the fleet's scope, see [Version differences](#version-differences) |

Siramy documented the first three low values (`47`, `59`, `5C` — he saw 1.07
and 1.08 as one step). The corpus adds `57` for 1.07 and everything from 1.10
forward.

D2Game's header validator accepts only `0x5C`–`0x60` (`CMP EAX,0x5b / JBE`,
`CMP EAX,0x60 / JA`, at `6fd0d2c4` and `6fd0d2cd`), returning error 7 for
anything outside that range on 1.13c's own read path. Its own write path always
produces `0x60`.

The character name sits at `0014h`, because the header carries three 32-bit
fields — file size, checksum, and active weapon set — ahead of it.

> **Version note (1.08 and earlier, save format ≤ 89):** these three fields do
> not exist. The character name starts at `0008h` instead of `0014h`, and the
> header D2Game will accept for these versions is `0x82` bytes long, not
> `0x14F`. This is patch 1.09's break: D2Client's `ReadCharacterSaveFile` reads
> the character's status word from `+0x18` below file version `0x5C` and from
> `+0x24` at or above it — a delta of `0x0C`, exactly the three inserted
> dwords. Full field-by-field offsets are in §3 and the Version differences
> table at the end of this chapter.

Evidence citations below occasionally need to name the two layouts directly
rather than repeat "format ≥ 92" or "format ≤ 89" every time. Where they do,
this chapter calls the current header **Layout B** and the pre-1.09 one
**Layout A** — Siramy's own structure, not a renumbering of it.

And it shows up in the files without any code at all. In **344 saves of
version 96**, the dword at `08h` is the file's own length in bytes and the
dword at `0Ch` is a checksum over the whole file. In **16 saves of versions 71,
87 and 89** (14 + 1 + 1), those same eight bytes are instead the first eight
characters of the character's name:

| Family | Saves checked | `dword@08h == file length` | Checksum at `0Ch` verifies |
|---|---|---|---|
| **v96 (1.10–1.14)** | **344** | **344 / 344** | **342 / 344** |
| v92 (1.09) | 3 | 3 / 3 | 3 / 3 |
| v71 / v87 / v89 (1.00–1.08) | 16 | 0 / 16 | 0 / 16 |
| v97 (D2R 1.0–2.4) | 36 | 35 / 36 | 35 / 36 |
| v98 (D2R 2.5) | 42 | 42 / 42 | 42 / 42 |
| v99 (D2R 2.6) | 72 | 72 / 72 | 72 / 72 |
| v105 (D2R 2.7 / 3.x) | 12 | 12 / 12 | 12 / 12 |

Nothing in that table is a coincidence a handful of files could produce. The
three checksum exceptions among the 96/97 rows are informative rather than
contradictory and are accounted for in §12.

**So: the header in this chapter is what 1.09 introduced and 1.13c writes in
final form.** Siramy's page documents what came before that break. It is not
sloppy — it is dated, and a reader who wants the pre-1.09 numbers finds them in
version-note callouts rather than in the chapter's main tables.

---

## 2. The signature

```
55 AA 55 AA  <version dword>
```

Eight bytes. The magic number `AA55AA55` is written as a literal in D2Game
(`MOV dword [ESP+0x2c],0xaa55aa55` at `6fd0c522`) and checked on load; a file
that fails it is rejected with error 9. All 525 corpus saves carry it.

Siramy's `signatur.html` gives three signatures — `47`, `59`, `5C` — which is
exactly the set that existed when he wrote. The version byte is the low byte of
a dword; the upper three bytes are zero in every save examined.

---

## 3. The fixed header

Verified against **344 saves at v96** and **3 at v92** (the two versions that
share this layout), cross-checked against **16 saves at versions ≤ 89** where
a field's history matters. The offsets are also confirmed in D2Game's header
writer at `6fd0c470`, which builds this structure field by field and copies
out exactly `0x14F` bytes (`LEA EAX,[EDI + 0x14f]` at `6fd0c4eb`).

| Offset | Field | Size | Evidence |
|---|---|---|---|
| `0000h` | Signature `55 AA 55 AA` | 4 | literal at `6fd0c522`; 469/469 saves |
| `0004h` | Save-format version | 4 | 1.13c always writes `0x60` (`6fd0c52a`) |
| `0008h` | File size | 4 | present since 1.09; matches the real file length in 508 of 509 saves at v92 and above |
| `000Ch` | Checksum | 4 | present since 1.09; verifies in 506 of 509 — algorithm in §12 |
| `0010h` | Active weapon set | 4 | present since 1.09; only bit 0 is used (`OR dword [ESP+0x30],0x1`, `6fd0c5d9`). `0` in 299 and `1` in 45 of 344 v96 saves |
| `0014h` | Character name | 16 | 15 chars + a NUL the loader forces (`MOV byte [EBP+0x23],0x0`, `6fd0d2d8`); well-formed in 383 of 383 saves at v92–v97 |
| `0024h` | Status | **2** | the writer stores a **word** (`MOVZX EDX, word [EBX+0xa]`, `6fd0c543`) — see §5 |
| `0025h` | Progression / acts passed | (high byte of the word above) | `status >> 8 & 0x1f`, then difficulty-unlock thresholds |
| `0026h` | Unknown | 2 | always `00 00` — 344/344 |
| `0028h` | Class | 1 | matched the class named in the file's own path in 186 of 186 saves whose path names one |
| `0029h` | Constant `10h` | 1 | `MOV byte [ESP+0x4d],0x10` (`6fd0c5a5`); `0x10` in 336/337 |
| `002Ah` | **Skill count** | 1 | *not* a magic constant — see below |
| `002Bh` | Level (display copy) | 1 | equalled the level held in the stat section in 480 of 480 saves that carry one |
| `002Ch` | Unknown | 4 | **not a creation timestamp on 1.13c** — see below |
| `0030h` | Last played (Unix time) | 4 | written from `time(NULL)` (`6fd0c58f`); a plausible Unix timestamp in 336/337 |
| `0034h` | Constant `FF FF FF FF` | 4 | `MOV dword [ESP+0x58],0xffffffff` (`6fd0c59d`); 336/337 |
| `0038h` | Hotkey skills | 64 | 16 entries × 4 bytes; `0000FFFFh` = empty |
| `0078h` | Left-hand skill | 4 | |
| `007Ch` | Right-hand skill | 4 | |
| `0080h` | Left-hand skill, weapon set II | 4 | |
| `0084h` | Right-hand skill, weapon set II | 4 | |
| `0088h` | Appearance in menu | 32 | §7 |
| `00A8h` | Difficulty / act, one byte per difficulty | 3 | `0x80` = active, **low 7 bits** = act |
| `00ABh` | Map seed | 4 | |
| `00AFh` | Unknown | 2 | always `00 00` — 344/344 |
| `00B1h` | Mercenary dead / alive | 2 | |
| `00B3h` | Mercenary control seed | 4 | |
| `00B7h` | Mercenary name id | 2 | stored **biased** — see §14 |
| `00B9h` | Mercenary type | 2 | |
| `00BBh` | Mercenary experience | 4 | |
| `00BFh` | Unknown / realm data | 144 | mostly zero-filled; only `+0xCF` is written |
| `014Fh` | *end of fixed header* | — | quest section begins |

Two fields in that table are more interesting than "unknown" suggests.

**`002Ah` is the number of skills the character class has.** The game reads it
from its own data tables (`sgptDataTables->[0xBA8]`) and writes it here — the
same value that sizes the skill array later in the file. Community
documentation records it as the constant `1Eh`. It is not a constant: across
**484 saves at versions 92 through 99, the byte at `002Ah` equals the measured
length of the skill array in 483 of them.** Vanilla writes 30. Project Diablo 2
saves write 33, and their skill arrays are 33 bytes long. One save in the corpus
writes 95, and carries 95 skill bytes. The file describes its own skill count,
and a parser that hardcodes 30 will desynchronise on the first modded character
it meets.

**`002Ch` is not a creation timestamp on 1.13c.** It is written from a game
accessor, not from `time()`, and it is `00000000` in 335 of 337 v96 saves. The
two exceptions are Diablo II: Resurrected–adjacent files carrying a later
format; on 1.13c-written saves it is zero. *(Diablo II: Resurrected's own
header changes are out of scope for this book — one line in the Version
differences table at the end covers them.)*

> **Version note (1.08 and earlier, save format ≤ 89):** the header before
> patch 1.09 has none of the three fields above `0014h` and is `0x82` bytes
> long instead of `0x14F`. It also arranges the rest of the header
> differently — a different constant header at `001Eh`, class at `0022h`
> instead of `0028h`, and no skill-count field at all, since the skill array
> was fixed at 30 bytes for every class that existed. The full table is below,
> as it was measured against **16 saves** (14 × v71, 1 × v87, 1 × v89) — every
> row of Siramy's original `part_1` held.

### The header before 1.09, in full

This is Siramy's `part_1` table, transcribed and checked field by field
against those 16 saves without changing a value. It documents save formats 71,
87 and 89 — patches 1.00 through 1.08 — and it is superseded by the table
above from format 92 onward.

| Offset | Field | Size | Type | Observed |
|---|---|---|---|---|
| `0000h` | Signature header | 8 | Header | `55 AA 55 AA` + version dword — 16/16 |
| `0008h` | Player name | 16 | String | printable, NUL-terminated, NUL-padded — 15/16 (one save has a trailing byte set) |
| `0018h` | Hardcore / status flag | 1 | Bitfield | `00h` on 14 classic saves, `20h` on both LoD saves |
| `0019h` | Number of acts passed | 1 | Value | `0` in all 16 — all are fresh characters, so **weakly sampled** |
| `001Ah` | Weapon configuration I / II | 2 | Value | `00 00` — 16/16 |
| `001Ch` | *Unknown* | 2 | ? | `DD 00` on all 14 v71; `3F 01` on v87 and v89 — **exactly as `unknown1.html` claims** |
| `001Eh` | Unknown header | 4 | Header | `10 00 82 00` — 16/16 |
| `0022h` | Player class | 2 | Value | matches the character in all 16 |
| `0024h` | Player level (copy) | 2 | Value | `1` in all 16 (all level-1 fixtures) |
| `0026h` | Appearance in menu | 32 | Structure | see §7 |
| `0046h` | Shortcuts, F1–F8 | 16 | Array | `FF 00` × 8 — 16/16 |
| `0056h` | Skill, left hand | 1 | Value | `00` in 14, `00`/`46h`/`24h` across the set |
| `0057h` | Skill, right hand | 1 | Value | as above |
| `0058h` | Save location | 2 | Value | `00 00` — 16/16, i.e. Act I Normal only. **Weakly sampled** |
| `005Ah` | Reserved | 36 | Reserved | all zero — 16/16 |
| `007Eh` | Map seed | 4 | Value | varies per save |
| `0082h` | Quest header | 8 | Header | `57 6F 6F 21 06 00 00 00` (`Woo!`) — 16/16 |
| `008Ah` | Normal quests | 96 | Structure | §8 |
| `00EAh` | Nightmare quests | 96 | Structure | §8 |
| `014Ah` | Hell quests | 96 | Structure | §8 |
| `01AAh` | Waypoint header | 10 | Header | `00 00 57 53 01 00 00 00 50 00` — 16/16 |
| `01B4h` | Waypoint sub-header | 2 | Header | `02 01` — 16/16 |
| `01B6h` | Normal waypoints | 5 | Bitfield | §9 |
| `01BBh` | Reserved | 17 | Reserved | all zero — 16/16 |
| `01CCh` | Waypoint sub-header | 2 | Header | `02 01` — 16/16 |
| `01CEh` | Nightmare waypoints | 5 | Bitfield | §9 |
| `01D3h` | Reserved | 17 | Reserved | all zero — 16/16 |
| `01E4h` | Waypoint sub-header | 2 | Header | `02 01` — 16/16 |
| `01E6h` | Hell waypoints | 5 | Bitfield | §9 |
| `01EBh` | Reserved | 17 | Reserved | all zero — 16/16 |
| `01FCh` | NPC header | 4 | Header | `01 77 34 00` — 16/16 |
| `0200h` | NPC dialogue state | 48 | Structure | §10 |
| `0230h` | Status header | 2 | Header | `67 66` (`gf`) — 16/16 |
| `0232h` | Stat / skill flag | 1 | Bitfield | §11 |
| `0233h` | Xp / gold / stash flag | 1 | Bitfield | §11 |
| `0234h` | "Reserved flag" | 1 | — | **not always zero — see §11** |
| `0235h` | Strength | 4 | Value | correct class starting value in all 16 |
| `0239h` | Energy | 4 | Value | correct in all 16 |
| `023Dh` | Dexterity | 4 | Value | correct in all 16 |
| `0241h` | Vitality | 4 | Value | correct in all 16 |

Two notes on that last block. Siramy writes the two-byte headers as words —
`6667h` for the status header, `6669h` for the skill header — which are the
ASCII pairs `gf` and `if` stored little-endian. Both conventions appear in the
archive and they mean the same bytes.

And the row at `0234h` is his one plain error in a hundred-odd correct ones.
It is covered in §11, because it turns out to be the door into the best part of
his work.

---

## 4. Where everything is: the constant offset shift

1.13c's header is `014Fh` bytes — 335 — because LoD added Act V quests and
waypoints and a mercenary block. Every fixed section after the header sits at
an offset that reflects that size:

| Section | Marker | 1.13c (format ≥ 92) | 1.08 and earlier (format ≤ 89) | Δ |
|---|---|---|---|---|
| Quests | `Woo!` (`57 6F 6F 21`) | `014Fh` | `0082h` | `−0CDh` |
| Waypoints | `WS` (`57 53`) | `0279h` | `01ACh` | `−0CDh` |
| NPC dialogue | `01 77 34 00` | `02C9h` | `01FCh` | `−0CDh` |
| Status | `gf` (`67 66`) | `02FDh` | `0230h` | `−0CDh` |

> **Version note (1.08 and earlier, save format ≤ 89):** the header these
> versions write is only `0082h` bytes — no Act V quests, no Act V waypoints,
> no mercenary block — so every one of the four sections above sits `0CDh`
> (205 bytes) *earlier* than the 1.13c offset in the table.

`0CDh` is 205 bytes, and it is the difference between the two header sizes.
Measured in **339 saves at format ≥ 92** and **16 saves at format ≤ 89**;
confirmed independently in D2Game, which builds the quest section at `+0x14F`
with a declared length of `0x12A` (`6fd0b7c6`), the waypoint section at
`+0x279` with length `0x50` (`6fd0b70a`), and writes `MOV word [ECX],0x6667`
for the status tag at `6fd0beae`.

Two small corrections to how the markers are usually written down. The NPC
section's marker is the four bytes `01 77 34 00` and the printable pair `w4`
sits at **offset +1**, not at the start — so the section begins at `02C9h`, not
`02CDh`. And the status tag `gf` begins at `02FDh`, not `02FCh`. Both were
measured in the files first and then confirmed in the writer.

The 335-byte header has a visible consequence. **A character that has been
created but never played is saved as exactly 335 bytes** — the header and
nothing else. Seven such files are in the corpus, and all seven carry status
bit 0 set, which is the bit D2Game's loader tests to decide "this is a fresh
character, build a new one and ignore the rest of the file". No quests, no
waypoints, no stats, no skills. A tool that expects a `gf` section will find
none.

---

## 5. The status word

The status field is a **16-bit** value at `0024h`. Its high byte is the
acts-passed counter, documented separately on Siramy's `title.html`; its low
byte carries the flag bits below, two of which were added after 2001.

> **Version note (1.08 and earlier, save format ≤ 89):** `hardcore.html`
> describes this as a single byte at `0018h` with three meaningful bits
> (hardcore, has-died, expansion) — correct for that era, and a strict subset
> of the word below at the same bit positions.

| Bit | Mask | Meaning | Evidence |
|---|---|---|---|
| 0 | `01h` | Newly created, never played | D2Game clears it and returns 2 (build a fresh character). Set in all 7 of the 335-byte stubs |
| 2 | `04h` | **Hardcore** | mismatch against the game's own hardcore flag returns error `0xB` / `0xC`. Set in 14/344 v96 |
| 3 | `08h` | **Has died** | when bit 2 is also set, the loader returns error 10 — a dead hardcore character is refused. Set in 149/344 |
| 5 | `20h` | **Expansion** | mismatch against the game mode returns error 8 / 9. Set in 339/344 v96, and in both LoD saves but neither classic save in Layout A |
| 6 | `40h` | **Ladder** — *added after 2001* | tested against the game's ladder flag with a season-expiry check (errors `0x19`/`0x1A`). Set in 162/344 |
| 8–12 | — | Progression / acts passed | `status >> 8 & 0x1f` |

Bits 1, 4 and 7 are clear in all 344 v96 saves examined.

Siramy's eight combined values (`00h`, `04h`, `08h`, `0Ch`, `20h`, `24h`,
`28h`, `2Ch`) are correct as constructions, and his resurrection recipe —
edit `0Ch` to `04h`, or `2Ch` to `24h` — is sound. But **`0Ch` and `2Ch` never
occur in a save the game itself wrote**: zero of 344 v96 saves have both bit 2
and bit 3 set, because the loader refuses that combination outright. The dead
hardcore character exists as a file the game will not open, which is the point
of hardcore mode and the reason his edit works.

His last observation on that page is also confirmed by the format: *"when the
player is in hardcore mode AND is died, there is NO current life status data."*
The status block is a bitmask of optional fields (§11), and the current-life
field is simply not written.

### Acts passed, and titles

`title.html` documents the counter that unlocks difficulties and honorifics.
The thresholds are in the game's code, in the same routine that decodes the
status word: nightmare at **4** (5 in expansion), hell at **8** (10 in
expansion), and the final title at 12 (15 in expansion). The first two
thresholds are confirmed in the decompiled comparison; the third is Siramy's,
consistent with the pattern, and **unverified**.

The mapping from counter to honorific — Sir/Dame, Count/Countess, Slayer,
Destroyer at the first tier, and so on — is a display concern and was not
checked.

---

## 6. Class, name, and the small fields

**Class.** `class.html` warns that it is "correct if you have the patch 1.08",
because 1.08 inserted the Druid and the Assassin at 05 and 06 and pushed the
old special values up to 07–09. The post-1.08 numbering is what modern saves
use, and it is confirmed twice over. The game's own `PlayerClass.txt` lists
Amazon, Sorceress, Necromancer, Paladin, Barbarian, an `Expansion` separator row
that the loader skips, then Druid and Assassin. And in **186 of 186** corpus
saves whose own file path names a class — `chars/97/Assassin.d2s`,
`Blank Characters/Level 30s/Druid.d2s`, and the like — the byte at `0028h` is
that class's number. Not one disagreement.

| Value | Class |
|---|---|
| `00h` | Amazon |
| `01h` | Sorceress |
| `02h` | Necromancer |
| `03h` | Paladin |
| `04h` | Barbarian |
| `05h` | Druid *(expansion)* |
| `06h` | Assassin *(expansion)* |

D2Game rejects any value of 8 or above outright (`if (bVar2 < 8)`). Siramy's
warning that "if you try an invalid value, the game will freeze" is a 2001
observation about 1.08 and is **unverified** for later patches; 1.13c does not
freeze, it refuses the file.

The remainder of `class.html` — values `07h`–`15h` for Diablo 1 clients,
Starcraft, Brood War, Warcraft II BNE, and Battle.net operator roles — is not
the save-file class enum. It is the **Battle.net product/role byte**, which
Siramy sourced from `Diablo II\support\bnet\char.htm` and which he correctly
labels as belonging to other clients. It is reproduced in Appendix D as
historical context and is **unverified**: the file it came from is not in this
tree.

**Name.** `name.html`'s rules — 2 to 15 characters, letters only, at most one
`-` or `_` and never both, never at an edge, `_` added in 1.08 — are behaviour
of the character-creation UI and could not be tested without creating characters
in nine different game versions. **Unverified.** What *is* verified is the
storage: 16 bytes, NUL-terminated and NUL-padded, in 383 of 383 saves at
versions 92 through 97; and the game forces a NUL into the sixteenth byte before
comparing, so 15 usable characters is a hard limit in the loader, not a UI
convention.

His anti-cheat note — that the `.d2s` file name must match the name inside —
is confirmed in the code: the loader calls `__stricmp` against the name at
`+0x14`, and the comparison is case-insensitive, exactly as he says. It holds in
the files too: the header name equals the file's own base name in **374 of 383**
saves at versions 92 through 97. All nine exceptions are files renamed by
researchers after the game wrote them — five variants of one Barbarian all still
called `Barb` inside, a `.dat` extension, a mule named `Sorceress` in a file
called `Mule`.

**Weapon set.** The active weapon set is the dword at `0010h`, of which the
game uses **only bit 0** (`OR dword [ESP+0x30],0x1`, `6fd0c5d9`). Observed `0`
in 299 and `1` in 45 of 344 v96 saves.

> **Version note (1.08 and earlier, save format ≤ 89):** `playconf.html` gives
> a two-byte field at `001Ah` instead — `00 00` for configuration I, `01 00`
> for configuration II, expansion only. All 16 Layout A saves read `00 00`,
> consistent with his claim, but they are all fresh characters, so his "always
> `00 00` in a non-expansion savegame" is **weakly sampled**.

**Level copy.** `levlcopy.html` explains that the level in the header is a
display-only duplicate — the menu reads it, the game does not. Confirmed by
decoding the level out of the status section, a different section of the file
entirely, and comparing: the two agree in **480 of 480** saves that carry a
status section. It really is a copy, and his warning that editing it changes
nothing stands.

**Save location.** Three bytes at `00A8h`, one per difficulty. Bit `80h` marks
the active difficulty and the **low seven bits** hold the act (D2Game:
`AL = act | 0x80`, and the reader does `bVar1 = bVar2 & 0x7f;
if (4 < bVar1) bVar1 = 0;`). Exactly one of the three bytes has the high bit
set in 336 of 344 v96 saves — the eight exceptions are the never-played stubs
and a truncated file.

> **Version note (1.08 and earlier, save format ≤ 89):** `location.html` gives
> a two-byte word at `0058h` instead, difficulty in the high nibble and act in
> the low: `0000h`–`0004h` Normal, `0010h`–`0014h` Nightmare, `0020h`–`0024h`
> Hell. All 16 Layout A saves read `0000h`, so his table is **verified only at
> Act I Normal** — but "low nibble" is close enough for acts 0–4 in either
> layout; it is only 1.13c's *width* (a full byte, low seven bits) that his
> two-byte word does not anticipate.

**Hotkeys and hand skills.** Sixteen four-byte entries at `0038h`, with
`0000FFFFh` for empty, plus four more entries at `0078h`–`0087h` for the two
weapon sets. Each four-byte entry is two 16-bit halves rather than a flat
skill id.

> **Version note (1.08 and earlier, save format ≤ 89):** `shortcut.html`
> describes eight two-byte entries at `0046h` instead, `00FFh` meaning empty,
> and `handskil.html` shrugs at the separate left- and right-hand skill bytes
> at `0056h`–`0057h`. All 16 Layout A saves carry `FF 00` eight times at
> `0046h`, confirming both pages.

Siramy calls these fields useless to edit, on the grounds that you cannot assign
a skill your class does not have. That judgement is his and is left as written.

---

## 7. Appearance in the menu

Thirty-two bytes at `0088h`: 16 component bytes followed by 16 tint bytes.
Change them and your character in the file-select menu wears whatever you
like — a helm floating over an invisible body, a broadsword held by nothing:

![An invisible man, with only his weapon, shield, helm and left shoulder visible](../preservation/siramy/paul.siramy.free.fr/d2ref/eng/invisibl.png)

*From the original page. The effect and the component order are unchanged in
1.13c; only the block's offset moved. The effect survives only until you enter
the game; on save-and-quit the real look, derived from the four items you
wear, is written back over it.*

D2Game fills the block with `FFh` and then hands two 16-byte halves to a
routine that writes the real components; 279 of 344 v96 saves carry non-`FF`
values there.

**The two "reserved" runs Siramy documents are not reserved.** The game's own
`Composit.txt` names sixteen components, not ten:

| Index | Composit.txt | Siramy |
|---|---|---|
| 0–7 | Head, Torso, Legs, RightArm, LeftArm, RightHand, LeftHand, Shield | same |
| 8, 9 | Special1, Special2 | "Right shoulder", "Left shoulder" |
| 10–15 | Special3 … Special8 | "Reserved (6 bytes)" |

So the structure is **16 component bytes followed by 16 tint bytes**, and the
"reserved" runs are simply the six special slots most classes leave empty. That
also resolves a contradiction inside the archive: `datatype.html` defines
*Reserved* as "a series of 0 (zero)", but those two runs read `FF FF FF FF
FF FF`. They are not zeros because they are not reserved — `FFh` is the same
"nothing" marker documented for the hands below.

Siramy's names for Special1 and Special2 are what those slots hold for the
player classes, so they are useful labels rather than errors. The correction
is the tail.

> **Version note (1.08 and earlier, save format ≤ 89):** `menulook.html` — the
> most charming page in the archive and one of the most accurate — puts this
> block at `0026h` instead of `0088h`, unchanged in size and order. This is
> where the structure was checked byte-for-byte: in all 16 Layout A saves,
> bytes `0026h`–`002Fh` follow the pattern
> `FF FF FF FF FF <weapon> FF <shield> FF FF` — Right hand at `002Bh` and
> Shield at `002Dh`, precisely where `menulook.html` puts them, with `FFh`
> meaning "nothing" — and it matches Siramy's own worked example (`67h FFh
> 1Dh` for a broadsword and a triangular shield). The "reserved" bytes read
> `FF FF FF FF FF FF` here too, in all 16 saves, which is the evidence behind
> the correction above.

---

## 8. Quests

Three blocks of 96 bytes — Normal, Nightmare, Hell — laid out act by act:
Act I, II and III take 16 bytes each (two enable words, then six quests of two
bytes), Act IV takes 10 (two enable words, three quests), Act V takes 26 (an
enable word, six reserved bytes, a second enable word, four reserved bytes, then
six quests), and 10 reserved bytes close the block.

The enable words are exactly what he says. In the Act I Normal block, the first
word is `2A 01` in **336 of 337** v96 saves, and the second is `01 00` in 314 of
them. In every one of **222 corpus saves whose active difficulty is Nightmare or
Hell**, the corresponding difficulty's second enable word is `01 00`, confirming
his "other quest and/or other difficulty: Enable A = `01 00`, Enable B = `01 00`"
as the *value that enables a tab*, not a value present from the start.

### The Act III error the author flagged and never fixed

On 26 July 2001, Siramy added Act V to the quest page and wrote in his revision
log:

> *find an error in the doc : in act 3, number and name of quests don't
> correspond. Will be fixed later.*

It was not fixed later. The site's last content update is two days after that
entry.

The error is real, and it is now settled from the game's own string tables. D2
keys quest names as `qstsa<act>q<n>`, where `n` is the slot index within the
act. Reading them out of `string.tbl` and `expansionstring.tbl`:

| Slot | Act I | Act II | Act III | Act IV |
|---|---|---|---|---|
| 1 | Den of Evil | Radament's Lair | **Lam Esen's Tome** | The Fallen Angel |
| 2 | Sisters' Burial Grounds | The Horadric Staff | **Khalim's Will** | Terror's End |
| 3 | Tools of the Trade | Tainted Sun | **Blade of the Old Religion** | Hell's Forge |
| 4 | The Search for Cain | Arcane Sanctuary | The Golden Bird | — |
| 5 | The Forgotten Tower | The Summoner | The Blackened Temple | — |
| 6 | Sisters to the Slaughter | The Seven Tombs | The Guardian | — |

Compare that against the archive. His Act I order (Den of Evil, Sister's Burial
Ground, Tools of the Trade, Search for Cain, Forgotten Tower, Sister to the
Slaughter) matches the game's slot order exactly, and his labels — `Quest 1`,
`Quest 2`, `Quest 5`, `Quest 3`, `Quest 4`, `Quest 6` — are the numbers the
quest log shows, which differ from the slot order. Same for Act II, where both
orders agree, and for Act IV, where he correctly labels the second slot
`Quest 3` and the third `Quest 2`.

For Act III he labelled the slots `1` through `6` in sequence — and swapped two
names. **Slots 2 and 3 are the wrong way round: `Khalim's Will` is slot 2 and
`Blade of the Old Religion` is slot 3, not the reverse.** In Layout A that means
Khalim's Will lives at `00B0h`, not `00B2h`.

He also renders the first Act III quest as "Lam Esen's Tomb". The game calls it
Lam Esen's **Tome**.

The archive's Act V quest names are blank dots — he asked for them and never got
them. From `expansionstring.tbl`:

| Slot | Act V |
|---|---|
| 1 | Siege on Harrogath |
| 2 | Rescue on Mount Arreat |
| 3 | Prison of Ice |
| 4 | Betrayal of Harrogath |
| 5 | Rite of Passage |
| 6 | Eve of Destruction |

### The quest bitfield

Each quest is a 16-bit field. Siramy's bit map:

| Byte | Bit | Meaning |
|---|---|---|
| 1st | 0 | Quest completed (single player) |
| 1st | 1 | Reward available |
| 1st | 2 | Quest started |
| 1st | 3 | Stage 4 complete |
| 1st | 4 | Stage 1 complete |
| 1st | 5 | Stage 2 complete |
| 1st | 6 | Stage 3 complete |
| 2nd | 2 | Stage 5 complete |
| 2nd | 3 | Stage 6 complete |
| 2nd | 4 | Quest completed 2 (single player) — already vanished from the quest window |
| 2nd | 7 | Quest completed (multiplayer) — will vanish from the quest window |

**Unverified.** The individual bit meanings could not be settled: they would
require driving a character through each quest stage and diffing saves, and no
corpus save carries a labelled partial state. What the corpus does show is that
these are dense, structured values rather than simple booleans — 9,099 quest
words sampled across 337 saves produce `FD 9F` 2,060 times and a long tail of
`01 10`, `05 10`, `19 10`, `81 11`, `79 1C`, each hundreds of times. The
patterns are consistent with a bitfield of stages plus completion flags.

Siramy's two recipes — write `7D 1C` for "complete, reward taken" and `7F 1C`
for "complete, reward waiting" — **do not appear anywhere in the corpus**
(0 occurrences of either in 9,099 sampled quest words, against 141 occurrences
of the neighbouring `79 1C`). They may still work as *inputs*; they are not what
the game writes. Treat them as 2001-era editing advice, not as format
documentation.

His imbue note is at least self-consistent: Act I Quest 5 in quest-log
numbering is *Tools of the Trade*, which is slot 3 at `0092h` in Layout A, and
bit 1 of its first byte is "reward available" — Charsi's imbue is exactly an
uncollected reward.

His cow-level table carries his own disclaimer — *"Not verified, take this as
is"* — and it stays that way. The offsets he gives (`0094h`, `00F4h`, `0154h`)
are the *Search for Cain* slot in each difficulty, which is the right place for
it, since that quest covers the Cairn Stones and Tristram. The specific byte
values are **unverified**, and he already said so.

---

## 9. Waypoints

Five bytes per difficulty — 40 bits, of which 39 are used — with one bit per
waypoint, numbered 0 to 38 straight through all five acts.

Three checks, all clean. **No corpus save has any bit at 39 or above set**
(0 of 337). **Bit 0 is set in 334 of 337** — the Rogue Encampment waypoint is
granted immediately. And the bit numbering matches the game's own `Levels.txt`
`Waypoint` column exactly, index for index, in two independently sourced copies
of that table.

The archive's Act V column is nine dots. He asked for the names and never
received them. Here they are, from `Levels.txt` cross-referenced against
`expansionstring.tbl`:

| Bit | Act I | Bit | Act II | Bit | Act III |
|---|---|---|---|---|---|
| 0 | Rogue Encampment | 9 | Lut Gholein | 18 | Kurast Docks |
| 1 | Cold Plains | 10 | Sewers Level 2 | 19 | Spider Forest |
| 2 | Stony Field | 11 | Dry Hills | 20 | Great Marsh |
| 3 | Dark Wood | 12 | Halls of the Dead Level 2 | 21 | Flayer Jungle |
| 4 | Black Marsh | 13 | Far Oasis | 22 | Lower Kurast |
| 5 | Outer Cloister | 14 | Lost City | 23 | Kurast Bazaar |
| 6 | Jail Level 1 | 15 | Palace Cellar Level 1 | 24 | Upper Kurast |
| 7 | Inner Cloister | 16 | Arcane Sanctuary | 25 | Travincal |
| 8 | Catacombs Level 2 | 17 | Canyon of the Magi | 26 | Durance of Hate Level 2 |

| Bit | Act IV | Bit | **Act V** *(filled in)* | Internal name in `Levels.txt` |
|---|---|---|---|---|
| 27 | The Pandemonium Fortress | 30 | **Harrogath** | Act 5 - Town |
| 28 | City of the Damned | 31 | **Frigid Highlands** | Rigid Highlands |
| 29 | River of Flame | 32 | **Arreat Plateau** | Act 5 - Barricade 2 |
| | | 33 | **Crystalline Passage** | Crystalized Cavern Level 1 |
| | | 34 | **Glacial Trail** | Crystalized Cavern Level 2 |
| | | 35 | **Halls of Pain** | Halls of Death's Calling |
| | | 36 | **Frozen Tundra** | Tundra Wastelands |
| | | 37 | **The Ancients' Way** | Glacial Caves Level 1 |
| | | 38 | **Worldstone Keep Level 2** | Act 5 - Baal Temple 2 |

The third column is worth keeping. `Levels.txt` stores a *key* into the string
table, and for Act V several of those keys are pre-release names that never
shipped: the level the game calls **Crystalline Passage** is `Crystalized
Cavern Level 1` in the table, and **The Ancients' Way** is `Glacial Caves
Level 1`. Anyone reading the raw table instead of the string table gets a
plausible-looking set of wrong names.

One correction to Siramy's Act I column: he writes "Jail Lv 1" and "Catacombs
Lv 2", which is his abbreviation, not the game's.

---

## 10. NPC dialogue state

48 bytes at `02CDh`: six records of eight bytes, two per difficulty, of which
only the first four bytes of each are used — one byte per act. The section
header is `01 77 34 00` and its declared length is `0x34` (52 bytes) including
the header, which is 48 bytes of payload — confirmed in D2Game
(`LEA EBP,[EBX+0x34]`, `6fd0b61f`).

> **Version note (1.08 and earlier, save format ≤ 89):** `unknownq.html`
> covers the same 48 bytes at `0200h` instead of `02CDh` — the `0CDh` header
> shift from §4, nothing more.

His bit map for the first byte is explicitly partial:

| Bit | Meaning |
|---|---|
| 0 | ? |
| 1 | Gheed introduction |
| 2 | Akara introduction |
| 3 | Kashya introduction |
| 4 | ? |
| 5 | Charsi introduction |
| 6 | ? |
| 7 | Something, but not Cain |

**Unverified**, and he says so. The mechanism he describes — clear the bit and
the NPC greets you again — is exactly the kind of claim that needs a running
game and a controlled diff, and it was not tested here. The section's size and
position are confirmed; its contents are not.

---

## 11. The status block, and the two bytes that bothered him

The stat/skill block opens with the tag `gf` (`67 66`) at `02FDh`, then an
LSB-first bitstream:

```
tag "gf" (67 66), then an LSB-first bitstream:
    id     = 9 bits          ; id > 0x1FE terminates, then byte-align
    param  = CSvParam bits   ; only if ItemStatCost[id].CSvParam != 0
    value  = CSvBits bits    ; signed if the stat is flagged signed
```

Widths come from `ItemStatCost.txt`, which means **a mod that edits that table
changes the file format**. Decoding this way against **337 v96 saves**
produces correct stats and lands exactly on the `if` skill header — the
decisive self-check: a parse that is one bit wrong lands anywhere else.
D2Game selects this reader whenever `0x5e < version` (`if (0x5e < version)`) —
save formats 95 and 96, which is every format 1.13c itself ever writes. 1.13c
can still *read* an older format 92–94 file, but routes it to the flag-based
reader below instead. The corpus has nothing at format 93, 94 or 95, so that
boundary is supplied by the code, not by a file: v92 saves in hand use the
flag block and v96 saves use the bitstream, with nothing in between to sample.

The six vitals — life, mana and stamina, current and max — keep **8 fractional
bits** within their `CSvBits` width: the value stored is the real number ×256,
now packed into 21 bits instead of a 32-bit dword. That fixed-point
convention did not start here — see the version note below for where it was
first solved.

Siramy's parenthetical about the level field predates the bitstream but is
still true and still useful, since the header's level copy at `002Bh` is a
plain byte that the stat section's true level (bit 4 of flag 2, in the scheme
below) can outgrow:

> *since the copy of this value at the offset 024h is only 2 bytes size, it's
> not wise to put a value greater than 65535. A value greater than 99 is not
> wise at all btw* **:)**

> **Version note (save format ≤ 94, i.e. 1.09 and earlier):** these versions
> use a completely different, flag-based encoding instead of the bitstream
> above — two bytes of present/absent flags followed only by the fields those
> flags name, each a fixed width. It is the best reverse engineering in the
> archive, including a puzzle `part_2` solves one inference short of the
> answer. The full scheme, Siramy's own words, and the resolution are below.

### The flag-based status block (format ≤ 94)

This is `part_2`. The problem it solves: **the status data is optional, field
by field.** Two flag bytes follow the `gf` header, sixteen bits, one per
field. A field whose bit is clear is not zero — it is *absent*, and every
following field slides up. There are no fixed offsets after the flags. His
examples are still the clearest explanation of why: most of the time you have
no unspent stat points, so the stat-points field simply isn't there; if all
your gold is banked, the gold field is missing and the stash field is present.

| Flag 1 (`0232h` / `02FFh`) | | Flag 2 (`0233h` / `0300h`) | |
|---|---|---|---|
| Bit 0 | Strength | Bit 0 | Mana, current |
| Bit 1 | Energy | Bit 1 | Mana, max |
| Bit 2 | Dexterity | Bit 2 | Stamina, current |
| Bit 3 | Vitality | Bit 3 | Stamina, max |
| Bit 4 | Stat points | Bit 4 | True player level |
| Bit 5 | Skill points | Bit 5 | Experience |
| Bit 6 | Life, current | Bit 6 | Gold |
| Bit 7 | Life, max | Bit 7 | Gold in stash |

(`0232h`/`0233h` for save format ≤ 89; `02FFh`/`0300h` for format 92–94 — the
`0CDh` header shift from §4.)

Verified in **19 saves** (14 × v71, 1 × v87, 1 × v89, 3 × v92): decoding by
these flags produces the correct starting Strength, Energy, Dexterity and
Vitality for every class present, the correct life, mana and stamina totals,
and — the decisive test — lands **exactly** on the `if` skill header in all 19.
A parse that is one byte wrong lands anywhere.

Then he hits the part he could not explain. In his words:

> *Now here comes a trick. There is a byte that we must skip. Most of the time
> it's equal to zero, but not always. I don't figure out what it means.*

and, after the life/mana/stamina fields, each of which he reads as two bytes
followed by two unexplained ones:

> *Another trick. The Stamina max data have a size of 2 bytes (it's OK), but it
> is not followed by 2 bytes, only by 1 byte. Could it be that the previous
> tricky byte (between Skill points and Life current datas) is, in fact, one
> part of the unknown data which follow the Stamina max data? I don't known.*

**He was right, and this is the answer.**

Every one of the sixteen status fields is a **32-bit value**. The six vitals —
life, mana and stamina, current and max — are stored in **8.8 fixed point**:
the number multiplied by 256, so the low byte is a fraction and the integer part
occupies bytes +1 and +2 of the field. Siramy's "skip one byte, read two, skip
two" is exactly a walk through six consecutive 32-bit fields reading the integer
part out of each. The leading tricky byte is the *fractional* byte of the first
vital. The lone trailing byte is the *high* byte of the last one. They are two
ends of the same six-field run — which is what he guessed.

The arithmetic is unambiguous. A level-1 Barbarian's 55 life is stored as
`00 37 00 00`: `0x3700` = 14,080 = 55 × 256. His 92 stamina is `00 5C 00 00` =
23,552 = 92 × 256. Tested across all 19 Layout A and v92 saves: **every vital
field is an exact multiple of 256, and the parse lands on `if` in 19 of 19.**

Two consequences worth stating plainly:

- The block is not "mixed 4/2/1-byte widths". It is sixteen dwords, of which
  eleven are typically present, and the arithmetic he could not close is fixed
  point.
- The encoding outlived the scheme it was found in. When save format 95
  replaced this block with the bit-packed stream documented above, life, mana
  and stamina kept their 8 fractional bits — the same 12,800 for 50 life, now
  in 21 bits instead of 32.

#### The "reserved flag" at `0234h`

Siramy names the third byte the *reserved flag* and writes: *"It's always set to
zero, like if Blizzard projected to use it as another flag."*

It is not always zero. In the 16 Layout A saves (format ≤ 89) it is `00h` in
seven and **`CCh` in nine** — and `0xCC` is the fill byte Microsoft's debug
runtime writes into uninitialised stack memory. Whatever the byte is, it is
not a flag; it is padding the game does not initialise and does not read,
which is why his parser worked anyway.

It is also the one field that changes across the 1.09 boundary *inside* the
status block. At v92 the values begin immediately after the two flag bytes,
with no padding byte at all: the flags sit at `02FFh`–`0300h` and Strength
starts at `0301h`. In the format ≤ 89 layout there is one byte between them.
Verified in 3 v92 saves and 16 saves at format ≤ 89. The exact version at
which the pad byte disappeared is **unverified** — the corpus has nothing
between 89 and 92.

---

## 12. The checksum

The dword at `000Ch`, present since save format 92 (1.09). The routine lives
in Fog.dll and is reachable in 1.13c only as an import; recovered from the
statically-linked copy in `Game.exe` 1.14d at `0x00411130`:

```c
uint32_t d2s_checksum(const uint8_t *buf, int len) {
    uint32_t sum = 0;
    for (int i = 0; i < len; i++)
        sum = ((sum << 1) | (sum >> 31)) + buf[i];   /* rotate left 1, then add */
    return sum;
}
```

The field must be **zeroed before computing**, over the whole file including the
header. The loader does exactly that — writes zeros into `buf[0x0C..0x0F]`, then
recomputes and returns error 6 on mismatch (error 5 for a bad size, 7 for a bad
version, 9 for a bad magic, 4 for a short file).

Implemented independently and run over the corpus, it reproduces the stored
value in **506 of 509** saves at version 92 and above. `d2-fleet/tools/d2s.py`
implements the same routine and agrees.

The three failures are each a story:

- Two are 335-byte never-played stubs whose checksum field is `00000000`. The
  game writes them that way and reads them by the newbie bit instead.
- One is a v97 file from a public test repository whose stored checksum does not
  match its bytes — a save someone edited without fixing the checksum. It is
  the format's own tamper indicator working exactly as designed, preserved in a
  test fixture for twenty years.

---

## 13. Skills

`part_3`. After the status block comes the two-byte header `if` (`69 66`), then
one byte per skill, each from 0 to 20.

Verified: the byte is ≤ 20 in 336 of 337 v96 saves, and the array length equals
the header's skill-count byte at `002Ah` in **483 of 484** saves. Vanilla LoD
writes 30 skills. D2Game reads that count from its data tables rather than
hardcoding it, so **"30 bytes" is a vanilla fact, not a format fact** — see §3.

The ordering is the interesting part, and Siramy's rule is the thing worth
carrying forward:

> *Their order is not what you could think : the 10 first skills are NOT the 10
> skills you find on Skill Tree #1. The primary key of the order is the minimum
> level required, and the secondary key is the Skill Tree : Skill Tree #3, then
> #2, and finaly #1.*

The full 30-row order for each of the seven classes is in Appendix B, with his
tree/level codes intact. **The tables are correct.** Each class's thirty skills
occupy a contiguous block of skill ids in the game's `Skills.txt` — Amazon 6–35,
Sorceress 36–65, Necromancer 66–95, Paladin 96–125, Barbarian 126–155, Druid
221–250, Assassin 251–280 — and that id order *is* the save array order.
Comparing all **210 positions** against `Skills.txt`:

- **176 names match exactly**, which pins the alignment for every class beyond
  argument;
- **23 differ only in spelling** (`Batlle Cry`, `Whirl Wind`, `Artic Blast`,
  `Cloack of Shadows`, `Increase Stamina`, `IronGolem` vs `Iron Golem`, …);
- **11 differ because `Skills.txt` stores an internal name and Siramy wrote the
  one the game displays** — `Dopplezon` is Decoy, `Plague Poppy` is Poison
  Creeper, `Mammoth Bear` is Werebear, `Summon Fenris` is Dire Wolf, `Fire
  Trauma` is Fire Blast, `Royal Strike` is Phoenix Strike, and five more, nearly
  all of them Druid or Assassin;
- **one is a genuine error.** Amazon slot 18 is `Charged Strike`, a javelin
  skill. Siramy wrote `Charged Bolt`, which is a Sorceress skill. Corrected in
  Appendix B.

His ordering rule survives the check intact: within each class the codes run
`C01 … A01`, then the level-6 group, and so on to `A30`.

---

## 14. Items, the corpse, and the mercenary

### Record sizes

`part_4` is two paragraphs: 27 bytes up to 1.03, then 15 or 31 from 1.04.
Measured across the corpus by walking consecutive `JM` markers in each item
list — valid only for runs of simple items, since item records are bit-packed
and variable-length:

| Version | Dominant simple-item lengths observed |
|---|---|
| 71 (1.00–1.06) | **27** bytes (92 records), 31 (44), 15 (6) |
| 87 / 89 (1.07–1.08) | **14** bytes (12 records) |
| 92 (1.09) | **14** bytes (18 records) |
| 96 (1.10–1.14) | **14** bytes (521 records), then a long tail of extended items |

So his 27/15/31 sizes are all present in the pre-LoD family, exactly as he
describes — but by 1.07 the simple item had settled at **14** bytes, not 15.
`d2-fleet/tools/d2s.py` reaches the same number from the other direction: a
simple item's fixed header is 92 bits, rounded up to 96, plus the two marker
bytes.

### Corpse and mercenary

The mercenary lives in the fixed header (`00B1h`–`00BEh`), and the item tail
carries explicit section tags:

```
… player items …
4A 4D <count>     JM  — corpse item list
6A 66             jf  — mercenary item section
  4A 4D <count>       mercenary's items
6B 66             kf  — iron golem section
  <0 or 1>            whether a golem item follows
```

`jf` and `kf` are written by D2Game at `6fd0c1f7` and `6fd0c044` and are
present at the end of every geared v96 save examined. One detail from the code
worth recording because no file reveals it: the mercenary's name id at `00B7h`
is stored **biased** — the game subtracts a base value from the merc's table
entry before writing it.

> **Version note (1.08 and earlier, save format ≤ 89):** `part_5b` preserves
> the pre-1.08 form, and its framing — history, explicitly labelled — is the
> model the rest of the archive should have followed. There is no fixed
> mercenary block in the header and no `jf`/`kf` tags. After the last item
> there are 12 bytes instead: a four-byte inventory footer `4A 4D 00 00` (no
> corpse) or `4A 4D 01 00` (corpse present), then `4A 4D` and six bytes of
> mercenary descriptor, all zero for "no mercenary". Confirmed byte-for-byte
> in the classic v71 saves, which end
> `4A 4D 00 00 4A 4D 00 00 00 00 00 00`. His eleven observed mercenary
> byte-strings, each annotated with a name, level and stat line, are
> reproduced in Appendix C and are **unverified** — the corpus has no
> pre-1.08 save with a hired mercenary. `part_5` describes 1.08 and the
> expansion, and is where the tail below picks up.

### The tail the author flagged as wrong

`part_5` carries his own red warning:

> *I just found that the folowing unknown datas are used for the additional
> shortkeys of expansion. If the player use more than the regular F1 to F8
> shortkey, the format change, so what I describe below is incorrect. I'll
> analyse it later.*

He never did, and the warning is preserved here rather than tidied away. But
what he described *is* reproducible, and one number in it is wrong.

He describes eight six-byte sequences at the very end of the file, each starting
`FF 00`, whose remaining four bytes are copied from earlier in the file — and
he gives two different distances in the same paragraph: **"exactly 4A bytes
before (74 in decimal)"**, and then, two sentences later, *"a whole block of 48
bytes is take from 76 bytes before"*.

`4Ah` is 74. The correct distance is **`4Ch`, which is 76** — his decimal was
right and his hex was wrong. Tested on both LoD saves in the corpus that carry
this tail (the v87 and the v89 file), every one of the six testable `FF 00`
markers has its four following bytes matching the bytes exactly `4Ch` earlier,
and matching nothing at `4Ah`. Twelve out of twelve.

The structure is otherwise exactly as he describes: eight records of
`FF 00` + four bytes = 48 bytes, followed by eight bytes that are zero in a
non-expansion save. `00FFh` is the same "no skill assigned" sentinel used by the
F1–F8 array, which supports his own reading that these are the expansion's
additional hotkey slots, written from a buffer that was never fully
initialised — the stale four bytes being whatever the item serialiser left
there. **That interpretation is a hypothesis, not a finding.** The pattern and
the distance are verified; the meaning is not.

---

## 15. What is still unverified

Silence is not confirmation. Ninety-two of the 580 claims extracted from these
pages could not be settled with the evidence available; they are itemised one by
one in [the companion report](d2s-save-format.verification.md), and grouped here:

| Claim | Why it could not be settled |
|---|---|
| Quest bitfield bit meanings (`quests.html`) | Needs a character driven through each quest stage with saves diffed between stages |
| `7D 1C` / `7F 1C` as completion values | Zero occurrences in 9,099 sampled quest words; may work as inputs, not what the game writes |
| Cow-level bytes | The author's own "not verified"; the offsets are the right quest slot, the values are untested |
| NPC dialogue bit map (`unknownq.html`) | Needs a live game and controlled dialogue |
| Character-name character rules (`name.html`) | UI behaviour across nine game versions |
| `location.html` full difficulty × act table | All 16 Layout A saves read `0000h` |
| Layout A "acts passed", weapon config, hand skills | All 16 Layout A saves are fresh level-1 fixtures |
| The 12-title honorific mapping (`title.html`) | Display concern; the 4/8 thresholds are confirmed, the 12 is not |
| Battle.net client values `07h`–`15h` (`class.html`) | Sourced from `support\bnet\char.htm`, which is not in this tree |
| Pre-1.08 mercenary descriptor strings (`part_5b`) | No pre-1.08 save with a hired mercenary |
| Meaning of the trailing 48-byte block (`part_5`) | Structure and distance verified; semantics are a hypothesis |
| Exact version where the status-block pad byte vanished | No corpus save between versions 89 and 92 |
| Meaning of `002Ch` on 1.13c | Provably not `time()`; the source accessor was not identified |
| Name of `0010h` ("active weapon set") | Only bit 0 is used and it maps to a player field consistent with weapon swap; not proven |

---

## Appendix A — Quest offsets, 1.13c and the pre-1.09 layout

Siramy's `quests_2.html` table, with the Act III names corrected and Act V
filled in. The `Quest N` column is the **quest-log display number**; the rows
are in **file order**. The pre-1.09 offsets are the 1.13c column minus `0CDh`.

| | 1.13c: N / NM / Hell | 1.08 and earlier: N / NM / Hell |
|---|---|---|
| **Act I** — enable | `157h` / `1B7h` / `217h` | `8Ah` / `EAh` / `14Ah` |
| Quest 1 — Den of Evil | `15Bh` / `1BBh` / `21Bh` | `8Eh` / `EEh` / `14Eh` |
| Quest 2 — Sisters' Burial Grounds | `15Dh` / `1BDh` / `21Dh` | `90h` / `F0h` / `150h` |
| Quest 5 — Tools of the Trade | `15Fh` / `1BFh` / `21Fh` | `92h` / `F2h` / `152h` |
| Quest 3 — The Search for Cain | `161h` / `1C1h` / `221h` | `94h` / `F4h` / `154h` |
| Quest 4 — The Forgotten Tower | `163h` / `1C3h` / `223h` | `96h` / `F6h` / `156h` |
| Quest 6 — Sisters to the Slaughter | `165h` / `1C5h` / `225h` | `98h` / `F8h` / `158h` |
| **Act II** — enable | `167h` / `1C7h` / `227h` | `9Ah` / `FAh` / `15Ah` |
| Quest 1 — Radament's Lair | `16Bh` / `1CBh` / `22Bh` | `9Eh` / `FEh` / `15Eh` |
| Quest 2 — The Horadric Staff | `16Dh` / `1CDh` / `22Dh` | `A0h` / `100h` / `160h` |
| Quest 3 — Tainted Sun | `16Fh` / `1CFh` / `22Fh` | `A2h` / `102h` / `162h` |
| Quest 4 — Arcane Sanctuary | `171h` / `1D1h` / `231h` | `A4h` / `104h` / `164h` |
| Quest 5 — The Summoner | `173h` / `1D3h` / `233h` | `A6h` / `106h` / `166h` |
| Quest 6 — The Seven Tombs | `175h` / `1D5h` / `235h` | `A8h` / `108h` / `168h` |
| **Act III** — enable | `177h` / `1D7h` / `237h` | `AAh` / `10Ah` / `16Ah` |
| Lam Esen's Tome | `17Bh` / `1DBh` / `23Bh` | `AEh` / `10Eh` / `16Eh` |
| **Khalim's Will** *(corrected)* | `17Dh` / `1DDh` / `23Dh` | `B0h` / `110h` / `170h` |
| **Blade of the Old Religion** *(corrected)* | `17Fh` / `1DFh` / `23Fh` | `B2h` / `112h` / `172h` |
| The Golden Bird | `181h` / `1E1h` / `241h` | `B4h` / `114h` / `174h` |
| The Blackened Temple | `183h` / `1E3h` / `243h` | `B6h` / `116h` / `176h` |
| The Guardian | `185h` / `1E5h` / `245h` | `B8h` / `118h` / `178h` |
| **Act IV** — enable | `187h` / `1E7h` / `247h` | `BAh` / `11Ah` / `17Ah` |
| Quest 1 — The Fallen Angel | `18Bh` / `1EBh` / `24Bh` | `BEh` / `11Eh` / `17Eh` |
| Quest 3 — Terror's End | `18Dh` / `1EDh` / `24Dh` | `C0h` / `120h` / `180h` |
| Quest 2 — Hell's Forge | `18Fh` / `1EFh` / `24Fh` | `C2h` / `122h` / `182h` |
| **Act V** — enable quests | `191h` / `1F1h` / `251h` | `C4h` / `124h` / `184h` |
| *reserved (6 bytes)* | `193h` / `1F3h` / `253h` | `C6h` / `126h` / `186h` |
| **Act V** — enable waypoints | `199h` / `1F9h` / `259h` | `CCh` / `12Ch` / `18Ch` |
| *reserved (4 bytes)* | `19Bh` / `1FBh` / `25Bh` | `CEh` / `12Eh` / `18Eh` |
| Siege on Harrogath *(filled in)* | `19Fh` / `1FFh` / `25Fh` | `D2h` / `132h` / `192h` |
| Rescue on Mount Arreat | `1A1h` / `201h` / `261h` | `D4h` / `134h` / `194h` |
| Prison of Ice | `1A3h` / `203h` / `263h` | `D6h` / `136h` / `196h` |
| Betrayal of Harrogath | `1A5h` / `205h` / `265h` | `D8h` / `138h` / `198h` |
| Rite of Passage | `1A7h` / `207h` / `267h` | `DAh` / `13Ah` / `19Ah` |
| Eve of Destruction | `1A9h` / `209h` / `269h` | `DCh` / `13Ch` / `19Ch` |
| Reserved quest data (10 bytes) | `1ABh` / `20Bh` / `26Bh` | `DEh` / `13Eh` / `19Eh` |

The 1.13c section start (`157h`), the Act I enable words, and the block
stride were measured directly in 337 saves. The pre-1.09 column is Siramy's
own page, with the Act III names corrected and Act V filled in per §8; like
the 1.13c column, its individual quest rows are **arithmetic**, from the
per-act block sizes, not independently re-measured row by row.

---

## Appendix B — Skill array order, by class

Reproduced from `part_3.html` unchanged. Offsets are **relative** to the first
byte after the `if` header. The code beside each skill is the author's: a letter
for the skill tree and a number for the minimum character level.

| Tree | Amazon | Sorceress | Necromancer | Paladin | Barbarian | Druid | Assassin |
|---|---|---|---|---|---|---|---|
| **A** | Javelin and Spear | Cold Spells | Summoning | Defensive Auras | Warcries | Elemental | Martial Arts |
| **B** | Passive and Magic | Lightning Spells | Poison and Bone | Offensive Auras | Combat Masteries | Shape Shifting | Shadow Disciplines |
| **C** | Bow and Crossbow | Fire Spells | Curses | Combat Skills | Combat Skills | Summoning | Traps |

| # | Amazon | | Sorceress | | Necromancer | |
|---|---|---|---|---|---|---|
| 0 | Magic Arrow | C01 | Fire Bolt | C01 | Amplify Damage | C01 |
| 1 | Fire Arrow | C01 | Warmth | C01 | Teeth | B01 |
| 2 | Inner Sight | B01 | Charged Bolt | B01 | Bone Armor | B01 |
| 3 | Critical Strike | B01 | Ice Bolt | A01 | Skeleton Mastery | A01 |
| 4 | Jab | A01 | Frozen Armor | A01 | Raise Skeleton | A01 |
| 5 | Cold Arrow | C06 | Inferno | C06 | Dim Vision | C06 |
| 6 | Multiple Shot | C06 | Static Field | B06 | Weaken | C06 |
| 7 | Dodge | B06 | Telekinesis | B06 | Poison Dagger | B06 |
| 8 | Power Strike | A06 | Frost Nova | A06 | Corpse Explosion | B06 |
| 9 | Poison Javelin | A06 | Ice Blast | A06 | Clay Golem | A06 |
| 10 | Exploding Arrow | C12 | Blaze | C12 | Iron Maiden | C12 |
| 11 | Slow Missiles | B12 | Fire Ball | C12 | Terror | C12 |
| 12 | Avoid | B12 | Nova | B12 | Bone Wall | B12 |
| 13 | Impale | A12 | Lightning | B12 | Golem Mastery | A12 |
| 14 | Lightning Bolt | A12 | Shiver Armor | A12 | Raise Skeletal Mage | A12 |
| 15 | Ice Arrow | C18 | Fire Wall | C18 | Confuse | C18 |
| 16 | Guided Arrow | C18 | Enchant | C18 | Life Tap | C18 |
| 17 | Penetrate | B18 | Chain Lightning | B18 | Poison Explosion | B18 |
| 18 | **Charged Strike** | A18 | Teleport | B18 | Bone Spear | B18 |
| 19 | Plague Javelin | A18 | Glacial Spike | A18 | Blood Golem | A18 |
| 20 | Strafe | C24 | Meteor | C24 | Attract | C24 |
| 21 | Immolation Arrow | C24 | Thunder Storm | B24 | Decrepify | C24 |
| 22 | Decoy | B24 | Energy Shield | B24 | Bone Prison | B24 |
| 23 | Evade | B24 | Blizzard | A24 | Summon Resist | A24 |
| 24 | Fend | A24 | Chilling Armor | A24 | Iron Golem | A24 |
| 25 | Freezing Arrow | C30 | Fire Mastery | C30 | Lower Resist | C30 |
| 26 | Valkyrie | B30 | Hydra | C30 | Poison Nova | B30 |
| 27 | Pierce | B30 | Lightning Mastery | B30 | Bone Spirit | B30 |
| 28 | Lightning Strike | A30 | Frozen Orb | A30 | Fire Golem | A30 |
| 29 | Lightning Fury | A30 | Cold Mastery | A30 | Revive | A30 |

| # | Paladin | | Barbarian | | Druid | | Assassin | |
|---|---|---|---|---|---|---|---|---|
| 0 | Sacrifice | C01 | Bash | C01 | Raven | C01 | Fire Blast | C01 |
| 1 | Smite | C01 | Sword Mastery | B01 | Poison Creeper | C01 | Claw Mastery | B01 |
| 2 | Might | B01 | Axe Mastery | B01 | Werewolf | B01 | Psychic Hammer | B01 |
| 3 | Prayer | A01 | Mace Mastery | B01 | Lycanthropy | B01 | Tiger Strike | A01 |
| 4 | Resist Fire | A01 | Howl | A01 | Fire Storm | A01 | Dragon Talon | A01 |
| 5 | Holy Bolt | C06 | Find Potions | A01 | Oak Sage | C06 | Shock Web | C06 |
| 6 | Holy Fire | B06 | Leap | C06 | Spirit Wolf | C06 | Blade Sentinel | C06 |
| 7 | Thorns | B06 | Double Swing | C06 | Werebear | B06 | Burst of Speed | B06 |
| 8 | Defiance | A06 | Pole Mastery | B06 | Molten Boulder | A06 | Fists of Fire | A06 |
| 9 | Resist Cold | A06 | Throwing Mastery | B06 | Arctic Blast | A06 | Dragon Claw | A06 |
| 10 | Zeal | C12 | Spear Mastery | B06 | Carrion Vine | C12 | Charged Bolt Sentry | C12 |
| 11 | Charge | C12 | Taunt | A06 | Feral Rage | B12 | Wake of Fire | C12 |
| 12 | Blessed Aim | B12 | Shout | A06 | Maul | B12 | Weapon Block | B12 |
| 13 | Cleansing | A12 | Stun | C06 | Fissure | A12 | Cloak of Shadows | B12 |
| 14 | Resist Lightning | A12 | Double Throw | C12 | Cyclone Armor | A12 | Cobra Strike | A12 |
| 15 | Vengeance | C18 | Increase Stamina | B12 | Heart of Wolverine | C18 | Blade Fury | C18 |
| 16 | Blessed Hammer | C18 | Find Item | A12 | Dire Wolf | C18 | Fade | B18 |
| 17 | Concentration | B18 | Leap Attack | C18 | Rabies | B18 | Shadow Warrior | B18 |
| 18 | Holy Freeze | B18 | Concentrate | C18 | Fire Claws | B18 | Claws of Thunder | A18 |
| 19 | Vigor | A18 | Iron Skin | B18 | Twister | A18 | Dragon Tail | A18 |
| 20 | Conversion | C24 | Battle Cry | A18 | Solar Creeper | C24 | Lightning Sentry | C24 |
| 21 | Holy Shield | C24 | Frenzy | C24 | Hunger | B24 | Wake of Inferno | C24 |
| 22 | Holy Shock | B24 | Increase Speed | B24 | Shock Wave | B24 | Mind Blast | B24 |
| 23 | Sanctuary | B24 | Battle Orders | A24 | Volcano | A24 | Blades of Ice | A24 |
| 24 | Meditation | A24 | Grim Ward | A24 | Tornado | A24 | Dragon Flight | A24 |
| 25 | Fist of the Heavens | C30 | Whirlwind | C30 | Spirit of Barbs | C30 | Death Sentry | C30 |
| 26 | Fanaticism | B30 | Berserk | C30 | Summon Grizzly | C30 | Blade Shield | C30 |
| 27 | Conviction | B30 | Natural Resistance | B30 | Fury | B30 | Venom | B30 |
| 28 | Redemption | A30 | War Cry | A30 | Armageddon | A30 | Shadow Master | B30 |
| 29 | Salvation | A30 | Battle Command | A30 | Hurricane | A30 | Phoenix Strike | A30 |

Spellings corrected to the game's own where the archive differs (`Lycantropy`,
`Artic Blast`, `Batlle Cry`, `Cloack of Shadows`, `Shadow master`,
`Whirl Wind`, `Natural Resistances`, `Cyclone armor`). Skill *names* are the
ones the game displays, which is what Siramy used; `Skills.txt` stores internal
names for eleven of these rows and those are listed in §13. One row is a
substantive correction: **Amazon slot 18 is Charged Strike, not Charged Bolt.**
All 210 positions were checked against `Skills.txt`.

---

## Appendix C — Pre-1.08 mercenary descriptors

From `part_5b.html`, unverified. Six bytes following the `4A 4D` mercenary
marker; `00 00 00 00 00 00` means no mercenary.

| Bytes | Act | Name | Level | Life | Def | Cost | Damage | Skill |
|---|---|---|---|---|---|---|---|---|
| `CB 67 DF 87 5A 0D` | I | Blaise | 10 | 60 | 45 | 490 | 2–3 | Cold Arrow |
| `2B 72 E4 8E 5A 0D` | I | Blaise | 8 | 60 | 45 | 415 | 1–3 | Cold Arrow |
| `9B DA 65 86 71 0D` | I | Meghan | 9 | 60 | 15 | 415 | 2–4 | Cold Arrow |
| `20 FA 19 45 69 0D` | I | Isolde | 3 | 30 | 15 | 150 | 1–3 | — |
| `BE 3E 4A 15 5D 0D` | I | Kundri | 6 | 30 | 45 | 315 | 1–3 | — |
| `FF 10 1B 1A 09 04` | II | Leharas | 17 | 180 | 60 | 690 | 3–8 | Jab |
| `F7 7E 7C 75 00 04` | II | Haseen | 15 | 180 | 60 | 565 | 2–6 | Jab |
| `93 B7 74 E2 0E 04` | II | Waheed | 17 | 180 | 60 | 690 | 3–8 | Jab |
| `83 3B 56 46 1F 04` | III | Rhadge | 26 | 400 | 150 | 1285 | 4–10 | Cold, fast cast |
| `05 A0 02 76 23 04` | III | Scorch | 20 | 400 | 150 | 895 | 3–8 | Lightning |
| `FF 9B 57 EA 11 04` | III | Barani | 22 | 400 | 80 | 970 | 4–10 | Lightning |

---

## Appendix D — Battle.net client values

From `class.html`, sourced by the author from
`Diablo II\support\bnet\char.htm`. **These are not save-file class values** —
they belong to the Battle.net chat protocol, and Siramy reproduces them beside
the character classes because the same page in Blizzard's own help file lists
both. Unverified.

| Value | Meaning |
|---|---|
| `07h` | Open character |
| `08h`, `09h` | Dead hardcore character |
| `0Ah`, `0Bh`, `0Ch` | Diablo 1 Warrior, Wizard, Amazon (Rogue) |
| `0Dh` | Starcraft |
| `0Eh` | Brood War |
| `0Fh` | Warcraft II Battle.net Edition |
| `10h` | Blizzard employee |
| `11h` | Channel operator |
| `12h` | System administrator |
| `13h` | Blizzard arbiter |
| `14h` | Chat-only client (cannot play) |
| `15h` | Orator |

---

## Appendix E — The author's notation

From `datatype.html`, preserved because the offset tables above use it.

| Type | Meaning |
|---|---|
| **Array** | X elements of Y bytes each; X × Y is the total size |
| **String** | An array of at most (total size − 1) characters, followed by a zero byte |
| **Bitfield** | Each bit has its own meaning |
| **Value** | A direct value, low byte first (Intel order) |
| **Header** | A series of known constant values |
| **Reserved** | A series of zero bytes |
| **Structure** | A group of related fields, described separately |
| **?** | Still unknown |

One caveat, established in §7: the *Reserved* runs inside the appearance
structure are not zeros and are not reserved. They are component slots the game
fills with `FFh` for "nothing". Elsewhere in the file — the 36 bytes at `005Ah`,
the three 17-byte waypoint gaps — *Reserved* means exactly what he says, and all
of those were zero in all 16 Layout A saves.

---

## Version differences

Every field this chapter documents, gathered in one place. "1.09" is save
format 92; "1.08 and earlier" is format ≤ 89 — Siramy's original scope.

| What | 1.13c (format 96) | 1.09 (format 92) | 1.08 and earlier (format ≤ 89) |
|---|---|---|---|
| Header length | `14Fh` (335 bytes) | `14Fh` | `82h` (130 bytes) |
| Character name offset | `0014h` | `0014h` | `0008h` |
| File size field | `0008h` | `0008h` | absent |
| Checksum field | `000Ch` | `000Ch` | absent |
| Active weapon set | `0010h`, dword (bit 0 only) | `0010h` | `001Ah`, word (`00 00` / `01 00`) |
| Status word | `0024h`, 16-bit (2 bits added after 2001) | `0024h`, 16-bit | `0018h`, 8-bit (3 bits used) |
| Class offset | `0028h` | `0028h` | `0022h` |
| Level (display copy) offset | `002Bh` | `002Bh` | `0024h` |
| Skill-count source | header byte `002Ah` | header byte `002Ah` | fixed at 30 — no header field |
| Save location | 3 bytes at `00A8h`; bit 7 active, low 7 bits act | `00A8h` | word at `0058h`; high nibble difficulty, low nibble act |
| Hotkeys (F1–F8 and beyond) | 16 × 4 bytes at `0038h` | `0038h` | 8 × 2 bytes at `0046h` |
| Appearance / `menulook` block | 32 bytes at `0088h` | `0088h` | 32 bytes at `0026h` |
| Quest section (`Woo!`) | `014Fh` | `014Fh` | `0082h` |
| Waypoint section (`WS`) | `0279h` | `0279h` | `01ACh` |
| NPC dialogue section | `02C9h` | `02C9h` | `01FCh` |
| Stat/skill section (`gf`) | `02FDh` | `02FDh` | `0230h` |
| Status block encoding | bit-packed `CSvBits` stream (format ≥ 95) | flag-based, no pad byte (format 92–94) | flag-based, 1 pad byte after the two flags |
| Simple item record size | 14 bytes | 14 bytes | 14 bytes from 1.07; 27 / 31 / 15 bytes before |
| Mercenary storage | fixed header `00B1h`–`00BEh` + `jf`/`kf` item tail | same | 6-byte descriptor after a `4A 4D` tail marker; no `jf`/`kf` |

Diablo II: Resurrected (save formats 97–105) exists and is out of this book's
and the fleet's scope; it is not documented here beyond this line.

---

## Closing

Five hundred and eighty checkable claims came out of these twenty-five pages.
Four hundred and sixty-two of them are still true. Twenty-six needed correcting.
The rest could not be settled with the evidence available, and are listed above
rather than left to look confirmed.

That is what a hex editor and enough patience to change one thing at a time got
right in 2001. Where it drifted, it drifted because the game moved underneath
it — and the archive says so itself, in its own hand: a red warning on the
corpse page, a revision-log entry admitting an unfixed Act III error, a row of
dots where the Act V waypoint names should be and a note that he had asked for
them and never got an answer.

That is a better failure mode than confidence. The Act III error is fixed above,
in the two slots he suspected. The Act V dots are filled in. The two bytes he
could not explain in the status block turn out to be the ends of a run of
fixed-point numbers, and his own guess — that they were one field split across
the structure — was the right one, a single inference short.

And the `4Ah` on the corpse page was `4Ch` the whole time — written out in
decimal, correctly, two sentences further down the same paragraph.
