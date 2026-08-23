# Verification report — `d2s-save-format.md`

Companion audit for [`d2s-save-format.md`](d2s-save-format.md), the modernized
edition of Paul Siramy's *D2S Unofficial Documentation* (2001).

**Verified 21 August 2026.** Every claim in the archive that could be reduced to
a byte offset, a field width, a bit meaning, an enum value or a constant was
extracted and checked. This file records the inventory, the verdicts, every
correction with its evidence, and — at least as important — everything that
could not be settled.

---

## 1. Origin and rights

| | |
|---|---|
| **Author** | Paul Siramy (`paul.siramy@free.fr` / `siramy_paul@yahoo.com`) |
| **Original** | *D2S Unofficial Documentation*, `http://paul.siramy.free.fr/d2ref/eng/` |
| **Last updated by the author** | 28 July 2001 |
| **Preserved copy** | `docs/preservation/siramy/paul.siramy.free.fr/d2ref/eng/` — untouched |
| **Per-file SHA-256** | `docs/preservation/MANIFEST.tsv` |
| **Credits the author gives** | The Diablo II Save Game Mapping Project — Lee Hamel (Juicy), Glenn C. (Mephiston), Terje B. (Instant), Mike Harrison (Polaris), IceTeaMan, Sephiroth — plus Jamella and TheTelamon |

### Rights status — the position taken

No page in the `d2ref` tree carries a licence, a copyright notice, or a
republication grant. It is personal fan-site documentation.

**The project proceeds on an explicit fair-use judgment rather than seeking
the author's permission.** The reasoning and its limits are recorded in
[BOOK-STATUS.md](../BOOK-STATUS.md). If Paul Siramy objects, the terms
change.

### Which language tree was used, and why

The archive holds three trees. `d2ref/eng/` (English, 28 July 2001) was used
exclusively. It is the newest: its `history.html` carries 26 July and 28 July
revisions — the Act V waypoint and quest updates — that neither French tree has.
`d2ref/` (French, 25 July 2001) and `fra/d2ref/` (18 July 2001, carrying the
author's own abandonment banner) are superseded and were not converted.

### Source pages converted

25 pages: `part_1`, `part_2`, `part_3`, `part_4`, `part_5`, `part_5b`,
`signatur`, `hardcore`, `name`, `class`, `title`, `playconf`, `unknown1`,
`levlcopy`, `menulook`, `shortcut`, `handskil`, `location`, `quests`,
`quests_2`, `waypoint`, `unknownq`, `datatype`, `index`, `history`.

Not in scope for this chapter and left for later: `items_15.html` (the 15-byte
item record and its ~50 type codes) and `maps.html` (the `.MAP`/`.MA*` sidecar
format). Both are referenced from pages that were converted; neither was
verified here.

One load-bearing image is carried into the chapter by relative path:
`invisibl.png`, the author's screenshot of the "invisible man" appearance
effect, which is evidence for the `menulook` structure and cannot be replaced by
a table. The 22 class icons (`class00.png` … `class15.png`) are decorative and
were dropped.

---

## 2. Ground truth used

Four independent sources. Where they disagreed, the disagreement is recorded
rather than resolved by preference.

### 2.1 Real save files (primary)

`c:\tmp\d2-saves-corpus\` — **525 unique `.d2s` files**, deduplicated by
SHA-256, with per-file provenance (immutable GitHub commit permalinks or local
paths) in `manifest.jsonl`. Version coverage:

| Save version | Game family | Files | Files with a full body |
|---|---|---|---|
| 71 (`47h`) | 1.00 – 1.06 classic | 14 | 14 |
| 87 (`57h`) | 1.07 LoD | 1 | 1 |
| 89 (`59h`) | 1.08 | 1 | 1 |
| 92 (`5Ch`) | 1.09 | 3 | 3 |
| 96 (`60h`) | 1.10 – 1.14 | 344 | 337 (7 are 335-byte stubs) |
| 97 (`61h`) | D2R 1.0 – 2.4 | 36 | 36 |
| 98 (`62h`) | D2R 2.5 | 42 | 42 |
| 99 (`63h`) | D2R 2.6 | 72 | 72 |
| 105 (`69h`) | D2R 2.7 / 3.x | 12 | 12 |

**No corpus file was modified.** All access was read-only.

**Sampling policy:** no type-F claim was sampled. Where a claim could be checked
against a family, it was checked against *every* file in that family. Counts in
the chapter and below are always "N of M", never "spot-checked".

### 2.2 The game's own code (Ghidra)

| Program | Image base | Used for |
|---|---|---|
| `/Vanilla/1.13c/D2Game.dll` | `6fc20000` | the entire `.d2s` serializer |
| `/Vanilla/1.13c/D2Client.dll` | `6fab0000` | single-player write-back, the version-gated status-word read |
| `/Vanilla/1.13c/D2Common.dll` | `6fd50000` | item (`JM`) codec |
| `/Vanilla/1.14d/Game.exe` | `00400000` | the checksum routine, which in 1.13c is a Fog.dll import and Fog is not loaded |

Key functions located (D2Game unless noted): header writer `6fd0c470`, header
reader/validator around `6fd0d270`, status decoder around `6fd0bb00`, quest
section `6fd0b7b0`, waypoint section `6fd0b700`, NPC section `6fd0b610`, stat
section reader `6fd0b8e0`, skill section writer `6fd0bc68`, mercenary block
`6fd0c120`, save-to-file `6fc73f10`, load-from-file `6fc76250`; checksum
`Game.exe 1.14d : 0x00411130`.

**Not done:** binary hashes for the 1.13c DLLs were not re-derived in this run;
the programs were identified by project path and image base. The 1.13c Fog.dll
was not loaded, so the checksum routine is from the byte-identical statically
linked 1.14d copy and was **not diffed against 1.13c's Fog**. It is nonetheless
corroborated empirically over 506 files.

### 2.3 The game's own data

An extracted vanilla LoD data tree at `F:\D2L-Data\data\`:

- `global\excel\Levels.txt` (134 rows) — waypoint indices and level names
- `global\excel\Composit.txt` — the 16 character composite components
- `global\excel\PlayerClass.txt` — class order
- `global\excel\skills.txt` — per-class skill id blocks
- `local\lng\eng\string.tbl`, `patchstring.tbl`, `expansionstring.tbl`
  (the latter two dated 27 April 2001) — quest and level display names
- `local\lng\eng\otheractinput.txt` — the plain-text source of the Act II–IV
  quest strings, used to cross-check the `.tbl` parse

The `.tbl` files were parsed with a purpose-written reader (17-byte hash nodes
after a 21-byte header and a `uint16` index array); the parse was validated by
the fact that `21 + 2×numElements + 17×hashTableSize` lands exactly on the
declared data-start offset, and by cross-checking the Act II–IV quest strings
against the plain-text `otheractinput.txt`, where they agree exactly.

The `Levels.txt` waypoint mapping was cross-checked against a **second,
independent copy** in this repository (`assets/excel/Levels.txt`, 196 rows —
a modded table). The two disagree on rows the mod added and **agree on all 39
waypoint rows**.

### 2.4 A working parser (second opinion)

`d2-fleet/tools/d2s.py` — read, not run against anything that could write.
Agrees on every Layout B header offset (`0x08` filesize, `0x0C` checksum,
`0x14` name, `0x24` status, `0x25` progression, `0x28` class, `0x2B` level,
first marker at `0x14F`), on the checksum algorithm, on locating sections by
marker rather than by assumed offset, and on the 14-byte simple item.

---

## 3. Claim tally

A **claim** is one discrete checkable assertion: one row of an offset table, one
bit assignment, one enum value, one constant, one position in a skill-order
table.

| Source page | Claims | Confirmed | Corrected | Unverified |
|---|---|---|---|---|
| `part_1` (Layout A header) | 40 | 39 | 1 | 0 |
| Section markers / sizes, both layouts | 8 | 8 | 0 | 0 |
| `part_2` (status flags + read algorithm) | 24 | 21 | 1 | 2 |
| `part_3` (skill section structure) | 4 | 3 | 1 | 0 |
| `part_3` (skill order tables, 7 × 30) | 210 | 210 | 1 | 0 |
| `part_4` (item record sizes) | 3 | 2 | 1 | 0 |
| `part_5` (corpse, mercenary, tail) | 12 | 9 | 1 | 2 |
| `part_5b` (pre-1.08 tail) | 15 | 4 | 0 | 11 |
| `signatur` | 3 | 3 | 0 | 0 |
| `hardcore` | 12 | 8 | 1 | 3 |
| `name` | 7 | 2 | 0 | 5 |
| `class` (save-file class values) | 9 | 8 | 0 | 1 |
| `class` (Battle.net client table) | 12 | 0 | 0 | 12 |
| `title` | 6 | 2 | 0 | 4 |
| `location` | 16 | 1 | 1 | 14 |
| `playconf` | 3 | 2 | 1 | 0 |
| `unknown1` | 2 | 2 | 0 | 0 |
| `levlcopy` | 2 | 2 | 0 | 0 |
| `menulook` | 26 | 14 | 12 | 0 |
| `shortcut` | 3 | 3 | 0 | 0 |
| `handskil` | 1 | 1 | 0 | 0 |
| `quests` | 55 | 20 | 4 | 31 |
| `quests_2` (offset table) | 31 | 31 | 0 | 0 |
| `waypoint` | 54 | 54 | 0 | 0 |
| `unknownq` | 14 | 6 | 0 | 8 |
| `datatype` | 8 | 7 | 1 | 0 |
| **Total** | **580** | **462** | **26** | **92** |

By claim type (from `doc-verify-enrich`'s taxonomy): **546 type F**
(format/layout), **22 type D** (game data / asset), **12 type C**
(contextual — the Battle.net table). No type A or B claims: the archive contains
no function names or addresses.

Additionally, **15 blanks were filled** with material the archive lacked — see
§6.

---

## 4. The central finding: version drift in `part_1`

The survey (`docs/preservation/siramy/INVENTORY.md` §5) flagged `part_1`'s
offset table as pre-1.09 and warned it would be silently wrong on modern saves.
That is confirmed, and now pinned to an exact version.

### 4.1 The boundary is between save version 89 and save version 92

**Empirical.** Two fields appear in the header at version 92 and are absent
below it:

| Family | Files | `dword@0008h == file length` | Checksum at `000Ch` verifies | Name at `0014h` well-formed |
|---|---|---|---|---|
| 71 / 87 / 89 | 16 | **0 / 16** | **0 / 16** | 0 / 16 (bytes `08h`–`17h` are the name) |
| 92 | 3 | 3 / 3 | 3 / 3 | 3 / 3 |
| 96 | 344 | 344 / 344 | 342 / 344 | 344 / 344 |
| 97 | 36 | 35 / 36 | 35 / 36 | 36 / 36 |
| 98 | 42 | 42 / 42 | 42 / 42 | 0 / 42 (field zeroed) |
| 99 | 72 | 72 / 72 | 72 / 72 | 0 / 72 (field zeroed) |
| 105 | 12 | 12 / 12 | 12 / 12 | n/a (field removed) |

**From the code.** D2Client's `ReadCharacterSaveFile` reads the character status
word from `+0x18` when the file version is below `0x5C` and from `+0x24` when it
is `0x5C` or above — a delta of `0x0C`, exactly three inserted dwords (file
size, checksum, active weapon set). D2Game's header validator accepts only
`0x5C`–`0x60` (`CMP EAX,0x5b / JBE` at `6fd0d2c4`; `CMP EAX,0x60 / JA` at
`6fd0d2cd`) and routes anything below `0x5C` to a legacy reader that requires a
minimum size of `0x82` — which is precisely the archive's Layout A offset for
the quest header.

**Verdict: the archive's table is correct for versions ≤ 89 and wrong for
versions ≥ 92.** The chapter presents both layouts as separate tables. No
Layout A value was overwritten.

### 4.2 Every field the archive documents, by era

Layout A rows were checked against **16 saves** (14 × v71, 1 × v87, 1 × v89).
39 of 40 held with no qualification. Highlights that are stronger than they
look:

- `001Ch` "Unknown", claimed `DD 00` up to 1.06 and `3F 01` from 1.08 — observed
  `DD 00` in **all 14** v71 saves and `3F 01` in **both** the v87 and v89 saves.
  A two-value claim from 2001, exactly reproduced.
- `001Eh` constant `10 00 82 00` — 16/16.
- `0082h` quest header `57 6F 6F 21 06 00 00 00` — 16/16.
- `01AAh` waypoint header `00 00 57 53 01 00 00 00 50 00` — 16/16.
- `01FCh` NPC header `01 77 34 00` — 16/16.
- `0230h` status header `67 66` — 16/16.
- The three 17-byte waypoint gaps and the 36-byte gap at `005Ah`: all zero,
  16/16.

Layout B rows were derived from the corpus and confirmed against D2Game's header
writer at `6fd0c470`, which emits exactly `0x14F` bytes.

**Section offsets shift by a constant `0CDh` (205) between the layouts**,
measured in 16 Layout A and 339 Layout B saves and independently confirmed in
the writer:

| Section | Marker | Layout A | Layout B |
|---|---|---|---|
| Quests | `Woo!` | `0082h` | `014Fh` |
| Waypoints | `WS` | `01ACh` | `0279h` |
| NPC | `01 77 34 00` | `01FCh` | `02C9h` |
| Status | `gf` | `0230h` | `02FDh` |

### 4.3 What each family shows, in one line

| Version | Name at | Size/checksum | Status block | Skill count source |
|---|---|---|---|---|
| 71, 87, 89 | `0008h` | absent | flag-based, **with** a pad byte after the two flag bytes | fixed 30 |
| 92 | `0014h` | present | flag-based, **no** pad byte | header `002Ah` |
| 96 – 99 | `0014h` (zeroed from 98) | present | bit-packed, 9-bit stat ids | header `002Ah` |
| 105 | removed; everything after shifts −16 | present | bit-packed | header `001Ah` |

The stat-encoding change is pinned by the code, not by a file: D2Game branches
`if (0x5e < version)` to the bit-packed reader, so the switch is at save version
**95** (`5Fh`). The corpus has nothing between 92 and 96, so the boundary is
**code-derived, not file-derived** — stated as such in the chapter.

---

## 5. Corrections

Twenty-six, each with its evidence.

### C1 — `part_1` / `part_2`: the "Reserved flag" at `0234h` is not always zero

**Before:** *"That's the reason why I called the byte which follow the first 2
flags, the reserved flag. It's always set to zero, like if Blizzard projected to
use it as another flag."*

**After:** it is `00h` in 7 of 16 Layout A saves and **`CCh` in 9** — the fill
byte Microsoft's debug runtime writes into uninitialised stack memory. It is not
a flag and it is not read: setting it does not change how the block parses, and
a parse that skips it lands correctly in all 16 files.

**Evidence:** aggregate over all 16 Layout A saves.

### C2 — `part_2`: the two "tricky" bytes, resolved

**Before:** the author reads six 4-byte fields, then *"a byte that we must skip…
I don't figure out what it means"*, then five 2-byte values each followed by two
unexplained bytes, then a 2-byte Stamina-max followed by only **one**
unexplained byte — and asks whether the leading byte and the trailing byte are
two halves of the same thing.

**After:** they are. All sixteen status fields are **32-bit**. The six vitals
(life, mana, stamina — current and max) hold the value **multiplied by 256**,
i.e. 8 fixed-point fractional bits, so the integer part occupies bytes `+1` and
`+2` of each field. The author's "skip one, read two, skip two" is a walk
through six consecutive dwords pulling the integer part from each; the leading
byte is the fraction of the first vital and the lone trailing byte is the high
byte of the last.

**Evidence:** modelled as 16 dwords with vitals `× 256` and run over
**19 saves** (14 v71, 1 v87, 1 v89, 3 v92): every vital is an exact multiple of
256 and the parse lands on the `if` marker in **19 of 19**. Worked case: a
level-1 Barbarian's 55 life is `00 37 00 00` = 14 080 = 55 × 256; his 92 stamina
is `00 5C 00 00` = 23 552 = 92 × 256. Corroborated by the 1.10 bit-packed format
retaining the same 8 fractional bits (`ItemStatCost` `ValShift = 8`).

**This is the modernization's principal contribution over the original.**

### C3 — `part_3`: the skill array is not fixed at 30 bytes

**Before:** *"It's an array of 30 bytes."*

**After:** 30 is vanilla's value. The length is written in the header at
`002Ah`, read by the game from its own data tables.

**Evidence:** header `002Ah` equals the measured `if`→`JM` gap in **483 of 484**
saves at versions 92–99. Vanilla writes 30 (141 saves); Project Diablo 2 writes
33 and carries 33 skill bytes (194 saves); one save writes 95 and carries 95.
Ghidra: the writer sources it from `sgptDataTables->[0xBA8]`, the same field that
sizes the section.

### C4 — `part_3`: Amazon slot 18 is Charged Strike, not Charged Bolt

**Evidence:** `Skills.txt` id 24 (Amazon block, index 18) is `Charged Strike`;
the string is present in `string.tbl`. `Charged Bolt` is Sorceress id 38.

### C5 — `part_4`: the simple item is 14 bytes from 1.07, not 15

**Before:** *"Up to version 1.03, Items were 27 bytes. Since version 1.04, they
can be 15 or 31 bytes."*

**After:** correct for the pre-LoD family — 27, 31 and 15 all appear in v71
saves — but from version 87 the simple item is **14** bytes.

**Evidence:** consecutive-`JM` measurement across the corpus: v71 → 27 × 92,
31 × 44, 15 × 6; v87/89 → 14 × 12; v92 → 14 × 18; v96 → 14 × 521.
`d2-fleet/tools/d2s.py` derives 14 independently (92 fixed bits rounded to 96,
plus the 2 marker bytes).

### C6 — `part_5`: the copy distance is `4Ch`, not `4Ah`

**Before:** *"They're take exactly 4A bytes before (74 in decimal)."* Two
sentences later, the same page says *"a whole block of 48 bytes is take from 76
bytes before."*

**After:** the distance is **`4Ch` = 76**. The decimal was right; the hex was
wrong.

**Evidence:** in the v87 and v89 saves — the only corpus files carrying this
tail — all six testable `FF 00` markers have their four following bytes matching
the bytes exactly `4Ch` earlier and matching nothing at `4Ah`. Twelve of twelve
across two files with different content.

### C7 — `hardcore`: the status field is 16 bits, and two more bits exist

**Before:** a 1-byte bitfield at `0018h` with bits 2, 3 and 5.

**After:** on Layout B it is a **`uint16` at `0024h`** whose high byte is the
acts-passed counter documented separately on `title.html`. Two bits were added
after 2001: **bit 0** = newly created / never played, **bit 6** = ladder.

**Evidence:** D2Game's writer stores a word (`MOVZX EDX, word [EBX+0xa]` at
`6fd0c543`); the reader tests bit 0 and returns "build a fresh character", and
tests bit `0x40` against the game's ladder flag with a season-expiry check.
Corpus: bit 0 set in all 7 of the 335-byte stubs and in nothing else; bit 6 set
in 162 of 344 v96 saves; bits 1, 4 and 7 clear in all 344.

### C8 — `hardcore`: values `0Ch` and `2Ch` never occur in a game-written save

**Evidence:** 0 of 344 v96 saves have bits 2 and 3 both set. D2Game refuses that
combination on load (returns error 10). The author's constructions are valid as
*edits* — which is exactly what his resurrection recipe relies on.

### C9–C20 — `menulook`: the twelve "reserved" bytes are components

**Before:** *"026h Head … 02Fh Left shoulder, 030h Reserved (6 bytes),
036h Head tint … 03Fh Left shoulder tint, 040h Reserved tint (6 bytes)."*

**After:** the structure is **16 component bytes and 16 tint bytes**. The
game's own `Composit.txt` names sixteen components: Head, Torso, Legs, RightArm,
LeftArm, RightHand, LeftHand, Shield, then **Special1 … Special8**. The
author's "Right shoulder" and "Left shoulder" are Special1 and Special2 — good
labels for what those slots hold on player classes — and his two "Reserved
(6 bytes)" runs are Special3 … Special8 and their tints. Twelve bytes
relabelled.

**Evidence:** `Composit.txt`, plus the corpus: those bytes read
`FF FF FF FF FF FF` in **all 16** Layout A saves, not zeros — `FFh` being the
"nothing" marker the author himself documents for the hand slots. His offsets
for the first eight components are confirmed exactly: in all 16 saves the byte
at `002Bh` (Right hand) and the byte at `002Dh` (Shield) are the only two that
vary.

### C21 — `datatype`: "Reserved" is not zeros inside the appearance structure

Follows from C9–C20. Everywhere else in the file — `005Ah`, and the three
17-byte waypoint gaps — *Reserved* means zeros, confirmed 16/16.

### C22, C23 — `quests`: Act III slots 2 and 3 are swapped

**Before:** Act III listed as Lam Esen's Tomb, Blade of the Old Religion,
Khalim's Will, The Golden Bird, The Blackened Temple, The Guardian.

**After:** slot 2 is **Khalim's Will** and slot 3 is **Blade of the Old
Religion**.

**Evidence:** the game keys quest names `qstsa<act>q<n>` where `n` is the slot
index. From `string.tbl`: `qstsa3q1` = Lam Esen's Tome, `qstsa3q2` = Khalim's
Will, `qstsa3q3` = Blade of the Old Religion, `qstsa3q4` = The Golden Bird,
`qstsa3q5` = The Blackened Temple, `qstsa3q6` = The Guardian. Cross-checked
against the plain-text `otheractinput.txt`, which agrees.

That the `qstsa` index is the *slot* order and not the quest-log display order
is established by the acts where the archive is right: `qstsa1q1..q6` reproduce
the author's Act I file order exactly (Den of Evil, Sisters' Burial Grounds,
Tools of the Trade, The Search for Cain, The Forgotten Tower, Sisters to the
Slaughter) while he labels them 1, 2, **5**, 3, 4, 6 — the quest-log numbers.
`qstsa2q1..q6` and `qstsa4q1..q3` likewise match his file order, and his Act IV
labels are 1, **3**, **2**.

**This is the error the author flagged himself** and never fixed:

> *07-26-2001 … find an error in the doc : in act 3, number and name of quests
> don't correspond. Will be fixed later.*

It is fixed here, in exactly the place he suspected. His Act III labelling
(a sequential 1–6, unlike every other act) is the visible symptom.

### C24 — `quests`: "Lam Esen's Tomb" is "Lam Esen's Tome"

**Evidence:** `qstsa3q1` in `string.tbl`.

### C25 — `quests`: `7D 1C` / `7F 1C` are not values the game writes

**Before:** *"To indicate that a Quest is complete and the reward has been
collected, set the value for the Quest to 7D 1C."*

**After:** neither value occurs in **9 099 quest words sampled across 337
saves** — 0 occurrences of `7D 1C`, 0 of `7F 1C`, against 141 of the
neighbouring `79 1C`. They may work as inputs; they are not what the game
produces. Reframed in the chapter as 2001-era editing advice rather than format
documentation.

### C26 — `location` and `playconf`: both fields changed shape at 1.09

`location.html`'s 2-byte word (difficulty in the high nibble, act in the low)
became **three bytes at `00A8h`, one per difficulty**, with bit `80h` marking
the active one and the **low seven bits** holding the act — not a nibble.
D2Game writes `AL = act | 0x80` and the reader does `bVar1 = bVar2 & 0x7f;
if (4 < bVar1) bVar1 = 0;`. Exactly one byte has the high bit set in 336 of 344
v96 saves.

`playconf.html`'s 2-byte field at `001Ah` became the dword at `0010h`, of which
only bit 0 is used (`OR dword [ESP+0x30],0x1` at `6fd0c5d9`). Observed 0 in 299
and 1 in 45 of 344 v96 saves.

### Two offset corrections that are *not* against the archive

Recorded because they are against the common modern reading, and the chapter
states them:

- The NPC section marker is the four bytes `01 77 34 00`; the printable `w4`
  sits at **+1**, so the section starts at `02C9h`, not `02CDh`. The archive
  gets this right (`01FCh` in Layout A); modern write-ups often do not.
- The status tag `gf` begins at `02FDh`, not `02FCh`. Measured in 339 saves and
  confirmed by `MOV word [ECX],0x6667` at `6fd0beae`.

---

## 6. Blanks filled

Fifteen items the archive marks with dots or leaves absent.

**Act V waypoint names (bits 30–38)** — the author wrote nine dots and asked for
the names. From `Levels.txt` cross-referenced with `expansionstring.tbl`:
Harrogath, Frigid Highlands, Arreat Plateau, Crystalline Passage, Glacial Trail,
Halls of Pain, Frozen Tundra, The Ancients' Way, Worldstone Keep Level 2.

The internal `Levels.txt` names for five of these are **pre-release names that
never shipped** (`Rigid Highlands`, `Crystalized Cavern Level 1` / `Level 2`,
`Halls of Death's Calling`, `Tundra Wastelands`, `Glacial Caves Level 1`), so a
reader who uses the raw table rather than the string table gets a
plausible-looking set of wrong names. Both columns are printed in the chapter.

**Act V quest names** — six more dots, from `expansionstring.tbl`:
Siege on Harrogath, Rescue on Mount Arreat, Prison of Ice, Betrayal of
Harrogath, Rite of Passage, Eve of Destruction.

Also added, as version-scoped context the archive could not have had: the `jf`
(mercenary items) and `kf` (iron golem) tail sections; the ladder status bit;
the checksum algorithm; the bit-packed 1.10 stat format; the 335-byte
never-played stub; and the D2R-era header changes at versions 98 and 105.

---

## 7. The author's self-flagged errors — each one, and what was found

The archive is unusually honest about its own limits. Four flags, four verdicts.

### 7.1 The red warning on `part_5`

> *I just found that the folowing unknown datas are used for the additional
> shortkeys of expansion. If the player use more than the regular F1 to F8
> shortkey, the format change, so what I describe below is incorrect.*

**Preserved verbatim in the chapter, and partly resolved.** The structure he
describes is real and reproducible: eight six-byte records at the very end of an
expansion save, each beginning `FF 00`, followed by eight bytes that are zero in
a non-expansion save. His copy-distance is wrong (see C6). His own reading —
that these are the expansion's additional hotkey slots — is *supported* by
`00FFh` being the same "no skill assigned" sentinel the F1–F8 array uses, and by
the stale four bytes looking like an uninitialised buffer. **That reading is a
hypothesis and is labelled as one.** It was not confirmed in code: 1.13c does
not write this format.

### 7.2 The unfixed Act III quest mismatch

**Real, found, and fixed.** See C22–C24. Slots 2 and 3 were swapped, and the
title of slot 1 has a typo. It is the only place in the whole quest table where
the author's labelling breaks its own convention, which is precisely the
inconsistency he noticed and recorded on 26 July 2001, two days before his last
update.

### 7.3 The blank Act V waypoint names

**Filled.** See §6. Nine names, from two independent copies of `Levels.txt`
plus the shipped string table.

### 7.4 The cow-level table: *"Not verified, take this as is"*

**Still unverified, and left flagged.** The offsets he gives — `0094h`, `00F4h`,
`0154h` — are the *Search for Cain* quest slot in each difficulty, which is the
right slot for the Cairn Stones and Tristram. The byte values were not tested:
no corpus save carries a labelled cow-level state, and testing them would mean
driving a character to the Cow King in three difficulties.

### 7.5 `unknownq`: the admittedly partial NPC bit map

**Still unverified.** Three of eight bits are unknown in the original and remain
so. The section's position (`0200h` / `02CDh`) and size (48 bytes of payload
behind a 4-byte header declaring `0x34`) are confirmed in code and in 336 of 337
saves; the contents are not.

### 7.6 `part_2`: the two unexplained bytes

**Resolved.** See C2. His guess was correct.

---

## 8. Unverified claims — the full list

92 claims. Silence is not confirmation, so all of them are named.

| # | Claim | Source | Why not settled |
|---|---|---|---|
| 1–11 | Quest bitfield bit meanings (completed SP, reward available, started, stages 1–6, completed MP) | `quests` | Requires driving one character through each quest stage and diffing saves between stages; no corpus save carries a labelled partial state |
| 12–14 | `7D 1C` / `7F 1C` as working *inputs*; the "vanishes in fire" behaviour | `quests` | Requires a running game; the values are absent from every save the game wrote |
| 15–23 | Cow-level enable / disable / killed values, 3 difficulties | `quests` | Author's own "not verified"; offsets are the right slot, values untested |
| 24–31 | Act V quest slot **layout** within the 26-byte block (enable A, 6 reserved, enable B, 4 reserved, 6 quests) | `quests` | Derived from `quests_2`'s offsets, which are internally consistent, but no save in the corpus has partial Act V quest state to discriminate the reserved runs |
| 32–39 | NPC dialogue bit map (Gheed, Akara, Kashya, Charsi introductions and 4 unknowns) | `unknownq` | Requires a live game and controlled dialogue |
| 40–44 | Character-name rules: 2–15 length, letters only, one `-` or `_`, never both, never at an edge | `name` | UI behaviour; would need character creation on nine game versions |
| 45 | `_` was added in 1.08 | `name` | Same |
| 46–49 | Honorific titles at each of the three tiers | `title` | Display concern; the difficulty-unlock thresholds at 4/5 and 8/10 *are* confirmed in code, the 12/15 tier is not |
| 50–63 | `location.html` difficulty × act table beyond Act I Normal | `location` | All 16 Layout A saves read `0000h` |
| 64 | "If you try an invalid class value, the game will freeze" | `class` | 1.13c refuses the file (`if (bVar2 < 8)`); the 1.08 behaviour is untestable here |
| 65–76 | Battle.net client values `07h`–`15h` | `class` | Sourced from `Diablo II\support\bnet\char.htm`, which is not in this tree. Not save-file values in any case |
| 77–87 | Eleven pre-1.08 mercenary descriptor byte-strings with names, levels and stats | `part_5b` | No pre-1.08 corpus save has a hired mercenary |
| 88–89 | Meaning of the trailing 48-byte block and the final 8 bytes | `part_5` | Structure and copy distance verified; semantics are a hypothesis |
| 90 | Exact version at which the status-block pad byte disappeared | `part_2` | Corpus has nothing between versions 89 and 92 |
| 91 | Meaning of header `002Ch` on 1.13c | new | Provably not `time()` and zero in 335 of 337 saves, but the source accessor was not identified |
| 92 | The name "active weapon set" for `0010h` | new | Only bit 0 is used and it maps to a player field also touched by inventory-slot code; consistent, not proven |

**Weakly sampled but not wrong** — the offset holds, the value range does not
exercise the claim. Called out inline in the chapter rather than counted as
unverified: Layout A `0019h` (acts passed, `0` in all 16), `001Ah` (weapon
config, `00 00` in all 16), `0024h` (level copy, `1` in all 16), `0056h`/`0057h`
(hand skills), `0058h` (location). Every Layout A corpus file is a freshly
created level-1 fixture.

---

## 9. Reconciled contradictions

**Inside the archive.** `datatype.html` defines *Reserved* as "a series of 0",
while `menulook.html` labels twelve non-zero bytes as reserved. Resolved by
`Composit.txt`: they are not reserved, they are Special3–Special8 and their
tints, and `FFh` means "nothing" exactly as the same page says for the hands.

**Inside one paragraph.** `part_5` gives the copy distance as both `4Ah` (74)
and 76. Resolved empirically at 76 = `4Ch`.

**Archive vs. survey.** `docs/preservation/README.md` describes the archive as
carrying "dates from 2002 to 2011"; the `d2ref` tree is **2001**. Noted by the
inventory and repeated here — that README is owned elsewhere and was not
touched.

**Archive vs. modern convention.** The `w4` and `gf` marker offsets (§5). The
archive is right and the common modern reading is off by one.

---

## 10. Open questions for the author

Things the evidence could not supply and a human might:

1. ~~**Rights.**~~ Settled — see §1: the project proceeds on a fair-use
   judgment, not a licence from the author.
2. What did the eight `FF 00` records at the end of a 1.07/1.08 expansion save
   actually hold? The "additional expansion hotkeys" reading is the author's own
   and is unconfirmed.
3. Did the status-block pad byte disappear at version 90, 91, or 92? A single
   1.09-beta-era save would settle it.
4. The quest bitfield's stage bits: 2001-era editors clearly knew them. Is
   there a surviving source (Jamella's released source, or the Save Game Mapping
   Project's documentation) that records them, rather than re-deriving them by
   playing?
5. What is header `002Ch` on LoD? It is written from a game accessor, not a
   clock, and it is zero in every LoD save examined.

---

## 11. Method notes

- **Nothing was sampled** among type-F claims. Where a family exists, every file
  in it was read.
- **Every parse was self-checking.** Both status-block formats and the item
  stream terminate by landing on the next section's two-byte marker. A layout
  that is one byte or one bit wrong lands somewhere else. That property, not
  plausibility, is what makes "19 of 19" and "337 of 337" meaningful.
- **The code and the corpus were kept independent** until both were done. The
  Ghidra investigation was run without the corpus results and returned the same
  section offsets (`014Fh`, `0279h`, `02C9h`, `02FDh`), the same header length
  (`0x14F`), the same checksum algorithm and the same 1.09 delta (`0x0C`) that
  the files had already shown. Neither was tuned to the other.
- **Circularity was actively avoided.** The corpus manifest's `class`, `level`
  and `char_name` fields are themselves the output of a parser reading the very
  offsets under test, so they were **not** used as evidence. Three independent
  substitutes were used instead:

  | Check | Independent because | Result |
  |---|---|---|
  | Class byte `0028h` vs the class named in the file's own path | the path is metadata a human wrote, not a parse | **186 / 186** |
  | Header level `002Bh` vs the level decoded from the stat section | a different section, decoded by a purpose-written bit reader | **480 / 480** |
  | Header name `0014h` vs the file's own base name | the file name is outside the file | **374 / 383**; all 9 exceptions are files renamed after the game wrote them |

  Aggregate totals used in the chapter, recomputed for this note: magic
  `55 AA 55 AA` present in **525 / 525**; file size correct in **508 / 509**
  saves at version 92 and above; checksum correct in **506 / 509**.
- **Read-only throughout.** No corpus save, no archive file, and no Ghidra
  program was modified.

---

## 12. Re-centering pass (21 August 2026)

Same day, after the verification above. Per `docs/book-conventions.md` §1,
every chapter reads with **1.13c as the unmarked default**. The verification
pass had already established every fact needed; this pass restructured how the
chapter *presents* them — no claim's verdict changed, and nothing already
marked correct or corrected was re-derived.

### What moved

The chapter's spine was Siramy's 2001 header (`Layout A`, save format ≤ 89),
with the 1.09-and-later layout (`Layout B`) introduced as the variant. That is
inverted throughout: §3's fixed-header table, §4's section-offset table, §5's
status word, §6's weapon-set/save-location/hotkey fields, §7's appearance
block, §10's NPC-dialogue offset, §11's status-block encoding, §14's
corpse/mercenary layout, and Appendix A's quest-offset table now all lead with
what save format 96 — what 1.13c writes — does, in the present tense and
without a version qualifier. The pre-1.09 layout is not deleted anywhere; it
now appears as `> **Version note (1.08 and earlier, save format ≤ 89):**`
callouts placed immediately after the claim each one modifies, plus one
full historical subsection each in §3 ("The header before 1.09, in full") and
§11 ("The flag-based status block (format ≤ 94)") where the alternate
structure is large enough that a short callout could not carry it without
losing content. A new `## Version differences` table at the end of the
chapter gathers every offset delta in one place, 1.13c first, per
`book-conventions.md` §3.

**Section 11 was the largest inversion.** The chapter previously led with
`part_2`'s flag-based scheme — including the resolution of Siramy's two
"tricky bytes" (all sixteen status fields are 32-bit; the six vitals are 8.8
fixed-point, so the integer part sits at bytes +1/+2; every vital in the
corpus is an exact multiple of 256, 19/19) — and reached the bit-packed
`CSvBits` stream 1.13c actually uses only in a closing subsection. That is now
reversed: the bit-packed stream leads, present tense, and the flag-based
scheme — with the tricky-bytes resolution intact and unchanged — is the
version-scoped historical subsection. No evidence changed; only which one is
the chapter's default claim did.

**Diablo II: Resurrected (save formats 97–105) was cut from the body.** The
task that requested this pass scoped D2R out of both the fleet and this book;
a "What Resurrected did to Layout B" subsection in §3 that documented the v97
unchanged-name behavior, v98/99's zeroed name field, and v105's shifted fields
was removed and replaced with one line in the `## Version differences` table.
The underlying facts are not lost — they are still in this file's §4.1 and
§4.3 tables above — but the chapter itself, per the task's explicit
instruction, no longer documents D2R's format beyond noting it exists and is
out of scope.

### What did not change

Every verdict in §5 above is unchanged: 462 confirmed, 26 corrected, 92
unverified, 580 total. No new claim was extracted or checked in this pass, and
no corpus file, archive file, or Ghidra program was touched — this was a
same-material restructuring, not a re-verification. Section counts and cross-
references were checked for consistency after the reorder; one pre-existing
cross-reference bug was found and fixed in the same pass (§1's "accounted for
in §11" pointed at the status-block section but the checksum exceptions it
describes are explained in §12 — corrected to §11 → §12).

### Corpus counts the re-centred chapter leans on

Per family, as stated throughout the chapter and unchanged by this pass:
v71 **14**, v87 **1**, v89 **1** (16 total at format ≤ 89), v92 **3**, v96
**344**. A 1.13c reader who skips every `> **Version note...**` callout and
both marked historical subsections reads a complete, self-consistent chapter
built entirely on the v92/v96 evidence — nothing in the unmarked body depends
on a pre-1.09 fact.
