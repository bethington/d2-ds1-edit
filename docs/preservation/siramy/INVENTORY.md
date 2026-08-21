# Siramy Archive — Survey & Modernization Triage

Inventory of `docs/preservation/siramy/paul.siramy.free.fr/` — 101 HTML pages,
643 images, 5 C source files, 3 PCX, 1 TXT.

**Original author: Paul Siramy** (`paul.siramy@free.fr` / `siramy_paul@yahoo.com`),
site `paul.siramy.free.fr`. Archive spans **2001–2011**.

This file is a read-only survey. No archived file was modified.

> **RIGHTS — unresolved, flag before publication.** No page in this archive
> carries a license, a copyright notice, or any republication grant. This is
> personal fan-site documentation. Permission from the author, or an explicit
> fair-use judgment, is required before any of this material appears in a book.
> The `d2ref` index does grant redistribution of *itself* as a ZIP ("You can
> find the ZIP version of this doc here"), which is weaker than a republication
> license. `docs/preservation/README.md` already commits to "these originals are
> the authoritative version" — a preservation stance, not a publication one.
> Decide this before converting, not after.

---

## 0. What this archive actually is

Three unrelated bodies of work were collected under one domain. The parent
task described `d2ref/` as "a Diablo II game reference"; it is not. It is a
**`.d2s` character save file format specification** — offsets, field widths,
bitfields, and the variable-length status block. That reframes its value
considerably.

| Body of work | Where | Dates | Subject |
|---|---|---|---|
| **A. D2S save format** | `d2ref/`, `d2ref/eng/`, `fra/d2ref/` | 2001 | Binary layout of `.d2s` and `.MAP`/`.MA*` files |
| **B. DS1 map editing** | `_divers/ds1/`, `_divers2/tut_any_units_ds1/` | 2003–2011 | Siramy's DS1 editor + map/warp/unit modding |
| **C. Item set tables** | `d2_sets/` | 2001 | French listings of the 32 item sets |

Body B is the strongest material and the reason this archive is in a DS1 repo.
Body A is the most *rigorous* material. Body C is fan-compiled stat lists the
author himself marks as incomplete.

### ⚠ Four pages are ALREADY CONVERTED — but none are verified

Before proposing any work, the existing `docs/` tree was checked. Four of the
five highest-value pages have already been turned into markdown:

| Archive page | Existing markdown | Lines |
|---|---|---|
| `_divers/ds1/doc/index.html` | `docs/getting-started/manual.md` | 1336 |
| `_divers/ds1/doc/tut01/index.html` | `docs/tutorials/01-basic-map-editing.md` | 557 |
| `_divers2/tut_any_units_ds1/index.html` | `docs/guides/monsters-and-objects.md` | 279 |
| `_divers/ds1/dl_ds1edit.html` | `docs/getting-started/overview.md` | 103 |

**Converted is not modernized.** These are raw HTML→markdown dumps that have had
only step 2 of the workflow applied. Every one of them:

- opens with **stray leftover title text** (`" DS1 Editor - Documentation"`, `" Ds1 Editor - Tutorial 1"`) that was the `<title>` tag, not a heading;
- has a **table of contents whose every link is a dead `(#)` anchor**;
- carries **no Origin block** — no author attribution header, no source URL, no statement of what was changed, no rights status (two of the four do carry a scoped note about the example ZIPs, which is the right instinct applied to one narrow question);
- is **not version-scoped** — `manual.md` is 1.09/1.10-era, `01-basic-map-editing.md` says "Patch 1.10 Beta" only in passing, and nothing states what holds on 1.13c;
- has **no claims verified** against Ghidra, game data, or the repo's own parsers;
- has **no companion `.verification.md`**.

So the cheap-looking items are the ones already half-done, and the honest
remaining work on them is *verification and framing*, not conversion. That is
reflected in the priority order below.

Also already in `docs/`, and **not** Siramy-derived: `docs/guides/cof-pipeline-1.13c.md`
(681 lines, reverse-engineered from the 1.13c DLLs directly). Do not confuse it
with archive material.

### Language trees — one is authoritative, two are superseded

`d2ref/` (root) is **French**, `d2ref/eng/` is **English**, `fra/d2ref/` is a
**stale French copy**. They are not equal:

| Tree | Pages | Last content update | Status |
|---|---|---|---|
| `d2ref/eng/` | 27 | **07-28-2001** | **AUTHORITATIVE** — most current |
| `d2ref/` (fr) | 27 | 25-07-2001 | Superseded — 10 days behind English |
| `fra/d2ref/` | 10 | 18-07-2001 | **Dead** — carries the author's own "this page is still here for an undetermined period" banner |

Verified by diffing `history.html` revision lists: the English history carries
`07-26-2001` and `07-28-2001` entries (Act V waypoints and quests) that neither
French tree has. **Convert from `d2ref/eng/` only.** The French trees are
translation artifacts, not content.

### Six orphaned asset directories — pages never archived

These directories contain images with **no HTML page anywhere in the archive
referencing them**. The pages they belonged to were not captured. (The
preservation README confirms this was known: the Wayback index revealed the DT1
material existed, but the pages themselves did not come back.)

| Directory | Contents | What was lost |
|---|---|---|
| `_divers/dt1_doc/dt1doc_data/` | 24 GIFs: `floor_flags.gif`, `floor_grid.gif`, `fence_grid1/2.gif`, `or_1`–`or_9.gif`, `system1/2/3.gif`, `box_big/small.gif`, `random_tiles.gif` | **A DT1 tile-format documentation page.** The filenames describe tile orientation values 1–9, floor flags, and sub-tile grids. The single most painful loss in the archive. |
| `_divers/dt1/` | 4 C sources + 14 GIFs | The DT1 tool page. **The C sources survive** — this is the mitigation. |
| `_divers2/tmptutcmap/` | 34 PNG/GIF + 20 palettes + PCX | A colormap/palette tutorial (Izual recolor, `.pl2`, act palettes) |
| `_divers2/view_pl2_cmaps/` | 9 files: `0049_0.25trans`, `0305_0.50trans`, `0561_0.75trans`, `0817_screen`, `1073_luminance`, `1457_darkscreen` | A PL2 colormap viewer page. Filenames encode **PL2 colormap block offsets** — recoverable data even without prose, and directly checkable against `src/misc.c:1300` (`misc_pl2_correct`). |
| `_divers/d2_anim/` | 35 GIFs (`ambw`, `amfw`, `asnu1`…) | Character animation token reference |
| `_divers/ds1/screenshots/` | 16 GIFs (`big_00`–`big_07`, `prev_00`–`prev_07`) | Referenced by 4 pages; already partly reused in `docs/assets/images/` |

**Recommendation:** treat `_divers/dt1_doc/` and `_divers2/view_pl2_cmaps/` as a
**reconstruction** task, not a conversion task. The image filenames + the DT1 C
sources + `src/core/dt1.c` + D2CMP in Ghidra are enough to rebuild the DT1
chapter from scratch, and it would be original work of real value.

### Non-HTML load-bearing sources (higher value than most of the HTML)

| File | Lines | Why it matters |
|---|---|---|
| `_divers/dt1/dt1make.c` | 990 | **Executable DT1 spec.** Declares `dt1_head_size 276`, `block_size 96`, `sub_tile_size 20`. The repo's own `src/core/dt1.c` independently uses header `+272` and a 96-byte block stride — **two independent implementations agreeing is strong evidence** |
| `_divers/dt1/dt1extr.c` | 726 | DT1 extraction; tile flag macros (`is_floor` `&2`, `is_wall` `&4`, `is_static` `&8`, `is_animated` `&16`, `is_wall_up` `&32`, `is_wall_down` `&64`) |
| `_divers/dt1/dt1debug.c` | 896 | DT1 inspector |
| `_divers/dt1/dt1info.c` | 301 | DT1 block/sub-block header walker |
| `d2ref/eng/v.c` | 1681 | **Executable D2S spec.** Enumerates 15-byte item IDs and the version enum `{VER_ERR, VER_100, VER_108, VER_109}`. Identical (md5 `a1181c39…`) in all three language trees — one file, three copies. **The only D2S "parser" available to this repo** |
| `_divers/ds1/obj.txt` | — | DS1 editor's object table (Act/Type/Id/Description). A tracked copy already lives at `assets/editor/obj.txt` |

These six files are worth more per byte than any HTML page here. **Convert the
C sources into documented struct tables, not prose.**

---

## 1. Per-page inventory

CT = content type · V = verifiability · BV = book value

### A. DS1 map editing — `_divers/`, `_divers2/`

| Path | Title | Covers | CT | V | BV |
|---|---|---|---|---|---|
| `_divers2/tut_any_units_ds1/index.html` | *(no `<title>`)* — "Adding ANY Monsters and ANY Objects to a DS1" | Hardcoded D2Common unit tables (Type 1 = 60/act = 300; Type 2 = 150/act = 750); index→`MonStats.txt` fallback in 1.10; the +150 Object rule; `MonPreset.txt` softcoding. Credits TeknoKyo and SVR. May 2010, tested on 1.13. *Converted → `docs/guides/monsters-and-objects.md`, unverified* | tutorial | **ghidra** | **high** |
| `_divers/ds1/doc/index.html` | DS1 Editor - Documentation | Full keyboard/mouse reference; layer model (Floor 1–2, Wall 1–4, Shadow, Special, Paths); the **Advanced Tile Editing bitfield window** (Hidden / Unwalkable bits per layer); `ds1edit.ini`; incremental-backup save scheme. Rev. **2006-03-05**. *Converted → `docs/getting-started/manual.md`, unverified* | tool-doc | **artifact** | **high** |
| `_divers/ds1/doc/tut01/index.html` | Ds1 Editor - Tutorial 1 | Warps end-to-end: Vis/Warp columns in `Levels.txt`, `LvlWarp.txt` geometry (SelectX/Y/DX/DY, ExitWalkX/Y, Direction l/r/b), `LvlTypes.txt` DT1 lists, the **`Dt1Mask` bitfield** (959→1023 worked example), `Pops`/`PopPad` roofs. Quotes three D2Common assertion strings verbatim. Rev. **2004-07-01**, written on 1.10 beta. Marked "To be continued". *Converted → `docs/tutorials/01-basic-map-editing.md`, unverified* | tutorial | gamedata | **high** |
| `_divers/ds1/dl_ds1edit.html` | Diablo II DS1 (map) Editor - Download Page | Download page; feature list; build provenance (MSVC 2010 + Allegro 4.4.2, 25 326 lines). Dated **2011-11-05** — the newest page in the archive. All links dead. *Converted → `docs/getting-started/overview.md`, unverified* | meta | unverifiable | low |

### B. D2S save format — `d2ref/eng/` (AUTHORITATIVE — convert from here)

**None of these 27 pages has been modernized.** They are also the *only* corpus
in the archive with **no in-repo parser to check against** — see §2.

| Path | Title | Covers | CT | V | BV |
|---|---|---|---|---|---|
| `d2ref/eng/part_1.html` | *(none)* | Master offset table, `0000h`–`0241h`: signature, name, hardcore flag, class, save location, map seed, quests ×3, waypoints ×3, NPC dialog, stat header, first four attributes | format-spec | gamedata | **high** |
| `d2ref/eng/part_2.html` | *(none)* | The variable-length status block: two flag bytes at `232h`/`233h`, 16 bits → 16 optional fields; mixed 4/2/1-byte widths; the two "tricky" unexplained bytes. The archive's best piece of reverse engineering | format-spec | ghidra | **high** |
| `d2ref/eng/part_3.html` | *(none)* | Skill header `6669h`; 30-byte skill array; ordering rule (primary key = min level, secondary = tree 3→2→1); full skill order tables for all 7 classes | format-spec | ghidra | **high** |
| `d2ref/eng/part_4.html` | *(none)* | Item record sizes: 27 bytes ≤1.03, 15 or 31 bytes from 1.04. Two paragraphs — a stub deferring to `items_15.html` | format-spec | gamedata | medium |
| `d2ref/eng/part_5.html` | *(none)* | Corpse marker `4A 4D 00 00`; mercenary block (alive/dead word, 8 descriptor bytes, XP, item count, items). Carries the author's own red warning that the trailing "unknown data" description is wrong | format-spec | gamedata | **high** |
| `d2ref/eng/part_5b.html` | *(none)* | Pre-1.08 corpse/mercenary: 12 bytes total, plus 11 observed mercenary descriptor byte-strings with name/level/stats | format-spec | gamedata | medium |
| `d2ref/eng/quests.html` | *(none)* | 96-byte quest structure per difficulty; per-act layout; **quest bitfield semantics** (completed, reward available, stages 1–6, SP vs MP completion); `7D 1C`/`7F 1C`; cow level bytes; imbue bit | format-spec | ghidra | **high** |
| `d2ref/eng/quests_2.html` | *(none)* | Absolute offset lookup for every quest × 3 difficulties (`8Ah`–`19Eh`) | format-spec | gamedata | medium |
| `d2ref/eng/waypoint.html` | *(none)* | 5-byte waypoint bitfield ×3 difficulties; bit-number → waypoint-name map, bits 0–38. Act V names **missing** (dots) | data-reference | gamedata | **high** |
| `d2ref/eng/menulook.html` | *(none)* | 32-byte character-appearance struct at `026h`: 10 body-part bytes + 6 reserved + 10 tint bytes + 6 reserved; example values; cross-refs `composit.txt` | format-spec | gamedata | **high** |
| `d2ref/eng/maps.html` | *(none)* | **`.MAP` file format** (24 bytes: signature `0000000Bh`, next-slot, 4 map IDs) and its relation to `.MA0`–`.MA3` and the `07Eh` seed; map-transplant procedure | format-spec | gamedata | **high** |
| `d2ref/eng/items_15.html` | *(none)* | 15-byte item record byte-by-byte; location value tables (backpack/stash/belt/cube); ~50 item type codes for potions, scrolls, gems, skulls. Tested on 1.05 | data-reference | gamedata | **high** |
| `d2ref/eng/hardcore.html` | *(none)* | Hardcore/expansion flag at `018h`: bit 2 = hardcore, bit 3 = has died, bit 5 = expansion; all 8 combined values | format-spec | ghidra | **high** |
| `d2ref/eng/signatur.html` | *(none)* | Version magic: `55 AA 55 AA` + `47` (≤1.06), `59` (1.08), `5C` (1.09) | format-spec | ghidra | **high** |
| `d2ref/eng/class.html` | *(none)* | Class byte values 00–15h: 7 playable, 3 special, Diablo 1 clients, other Blizzard game clients, Battle.net operator roles | data-reference | ghidra | medium |
| `d2ref/eng/title.html` | *(none)* | Acts-passed counter → difficulty unlock and honorific title at 4/8/12 (5/10/15 expansion) | data-reference | ghidra | medium |
| `d2ref/eng/location.html` | *(none)* | Save-location word: difficulty in the high nibble, act in the low (`0000h`–`0024h`) | data-reference | gamedata | medium |
| `d2ref/eng/unknownq.html` | *(none)* | NPC dialogue-state block at `200h`; 6 records of 8 bytes; partial bit map for Act 1 NPC introductions | format-spec | ghidra | medium |
| `d2ref/eng/name.html` | *(none)* | Character name rules: 2–15 chars, single `-` or `_` (never both, never at an edge); `_` added in 1.08; filename-must-match-name anti-cheat check | format-spec | gamedata | medium |
| `d2ref/eng/playconf.html` | *(none)* | Weapon-swap slot at `01Ah`; expansion only | format-spec | gamedata | medium |
| `d2ref/eng/levlcopy.html` | *(none)* | Why the level byte at `024h` is a **display-only copy**; the real level lives in the status block | format-spec | ghidra | low |
| `d2ref/eng/unknown1.html` | *(none)* | Unknown word at `01Ch`: `DD 00` ≤1.06, `3F 01` from 1.08 | format-spec | gamedata | low |
| `d2ref/eng/shortcut.html` | *(none)* | F1–F8 skill array (8 × 2 bytes, `00FFh` = empty). Author calls it useless to edit | format-spec | gamedata | low |
| `d2ref/eng/handskil.html` | *(none)* | Left/right-hand skill bytes. 3 sentences; defers to `shortcut.html` | format-spec | gamedata | low |
| `d2ref/eng/datatype.html` | *(none)* | The author's own notation glossary: Array, String, Bitfield, Value, Header, Reserved, Structure, `?` | format-spec | unverifiable | low |
| `d2ref/eng/index.html` | D2S Unofficial Documentation | TOC, download, credits (D2 Save Game Mapping Project, Jamella, TheTelamon/CV5), contact | meta | unverifiable | low |
| `d2ref/eng/history.html` | *(none)* | Revision log 07-15 → 07-28-2001. **Diagnostic value: this is how the three language trees were dated** | meta | unverifiable | low |

### B′. D2S save format — French duplicates (SUPERSEDED — do not convert)

Same page names, same structure, same CT/V ratings as their English
counterparts; **BV: low for all 37 pages** — value is translation reference only.

| Path set | Pages | Status |
|---|---|---|
| `d2ref/{class,datatype,handskil,hardcore,history,index,items_15,levlcopy,location,maps,menulook,name,part_1,part_2,part_3,part_4,part_5,part_5b,playconf,quests,quests_2,shortcut,signatur,title,unknown1,unknownq,waypoint}.html` | 27 | Superseded by `d2ref/eng/` — missing the 07-26 and 07-28 Act V updates |
| `fra/d2ref/{history,index,items_15,part_1,part_2,part_3,part_4,part_5,quests,quests_2}.html` | 10 | Dead. Author's own banner: *"Le site a complètement changé… cette page est encore ici [pour] une période indéterminée"* |

### C. Item sets — `d2_sets/`

| Path | Title | Covers | CT | V | BV |
|---|---|---|---|---|---|
| `d2_sets/set.html` | Les collections | Index of 32 sets; explains the blue/green/gold affix-tier display; notes 1.08 buffed sets and LoD added 16 (7 class-specific). Updated 08-12-2001 | meta | unverifiable | low |
| `d2_sets/d2/dico/set/<name>/<name>.html` × 32 | *(French set names)* | Per-set item listing: French item name, defense/damage, requirements, blue/green/gold affix lines. aldur, arcanna, arctique, berserker, bulkathos, cathan, celeste, civerb, cleglaw, disciple, divin, enfer, griswold, hsarus, hwanin, iratha, isenhart, mavina, milabrega, mort, naj, natalya, orphelin, roiimmortel, sander, sazabi, sigon, talrasha, tancred, taureau, trangoul, vidala | data-reference | gamedata | low |

**Self-flagged as incomplete by the author.** `set.html` states only sets marked
`(#)` were validated against post-1.08 items — **only 2 of 32** (`divin`,
`sigon`). The other 30 "may have some magic properties forgotten." Screenshots
were crowd-sourced (Nostenfer, clan GDI) — consistent with the preservation
README calling this material "co-authored". `SetItems.txt` from any MPQ
supersedes all 32 pages completely.

---

## 2. Summary tables

### By content type

| Content type | Count | Notes |
|---|---|---|
| format-spec | 47 | 20 English + 20 French dup + 7 stale French |
| data-reference | 43 | 32 of these are the item-set pages |
| meta | 8 | 3 index + 3 history + `set.html` + download page |
| tutorial | 2 | Both high value |
| tool-doc | 1 | The DS1 editor doc |
| **Total** | **101** | |

*After de-duplication (English tree only, sets collapsed): **~31 distinct
documents**, of which 4 are the DS1 material and 25 are the D2S spec.*

### By verifiability

| Verifiability | Count | Notes |
|---|---|---|
| gamedata | 68 | 32 set pages + 30 D2S leaf pages + `tut01` |
| ghidra | 22 | Bitfield/enum/dispatch claims + `tut_any_units` |
| unverifiable | 10 | Indexes, histories, the type glossary, the download page |
| artifact | 1 | The DS1 editor doc |
| **Total** | **101** | |

**The verification asymmetry is the most important triage fact in this survey.**
It is not visible in the counts above, so state it plainly:

| Corpus | In-repo parser | Local sample data | Verification cost |
|---|---|---|---|
| DS1 / DT1 / DCC / DC6 / COF / PL2 / Excel `.txt` | **Yes** — `src/core/{ds1,dt1,dcc,dc6,cof,palette,txtread}.c`, plus a CLI (`probe`, `dump-txt-row`, `audit-lvltypes`) built to do exactly this | **Yes** — 2 666 `.ds1` and 359 `.dt1` on disk | **Low** |
| D2S / `.MAP` | **None.** Zero matches for `d2s` anywhere in `src`, `test`, `scripts`, `tools` | **None.** Zero `.d2s` files in the tree | **High** — needs a save file sourced externally, plus Ghidra |

The D2S corpus is the archive's most rigorous writing and its most expensive to
verify. The DS1/DT1 corpus is the reverse. Plan accordingly: the DS1 material
can be verified almost mechanically; the D2S material needs a fixture obtained
first. (Note also: the repo's tests are entirely synthetic — they fabricate byte
arrays in memory and never load a real game file. A real-file check is new work
regardless of format.)

### By book value

| Book value | Count |
|---|---|
| high | 14 |
| medium | 9 |
| low | 78 |
| **Total** | **101** |

**69 of the 78 low-value pages are redundancy, not weak content** — 37 duplicate
French pages plus 32 superseded set pages.

---

## 3. Prioritized modernization order

Revised after discovering four pages are already converted. Verifying an
existing conversion is cheaper and worth more than starting a new one.

| # | Work | Why |
|---|---|---|
| 1 | **Verify + reframe `docs/guides/monsters-and-objects.md`** (`tut_any_units`) | Already converted; text needs no work. Every claim resolves in Ghidra against a loaded D2Common: the 300-entry Type 1 table, the 750-entry Type 2, the +150 rule, the 1.09→1.10 divergence. Highest verifiability in the archive, lowest cost to finish. Add Origin block, fix the dead `(#)` TOC, version-scope to 1.13, write the companion report. |
| 2 | **Verify + reframe `docs/getting-started/manual.md`** (DS1 editor doc) | The repo *is* a DS1 editor: `src/core/ds1.c` reads v1–v18 and `structs.h` pins every on-disk field width. The layer model and tile bitfield window check directly against it, with 2 666 local `.ds1` files to sample. Strip the stray title line, add attribution, mark tool-UI claims as historical past-tense per the skill. |
| 3 | **Verify + reframe `docs/tutorials/01-basic-map-editing.md`** (tut01) | `Dt1Mask` 959→1023 and the `LvlWarp.txt` geometry are checkable via the repo's own `dump-txt-row` / `audit-lvltypes` CLI against real MPQ tables. **Grep the three quoted D2Common assertion strings in Ghidra** — they date the claims and confirm the failure modes. Note that `monsters-and-objects.md` supersedes its trap-door method. |
| 4 | **Reconstruct the DT1 chapter** from `_divers/dt1/*.c` + `_divers/dt1_doc/dt1doc_data/*.gif` | Not a conversion — the page is gone. But `dt1make.c` (`dt1_head_size 276`, `block_size 96`, `sub_tile_size 20`) and the repo's independent `src/core/dt1.c` (header `+272`, 96-byte stride) **agree**, which is the strongest evidence available short of Ghidra. 359 local `.dt1` files to test against. Highest original-contribution value in the whole plan. |
| 5 | `d2ref/eng/part_2.html` | Start of the D2S work, and the right place to start: the 16-bit flag → optional-field dispatch is a real algorithm, not a table, and it is why naïve `.d2s` parsers fail. Resolving the two "tricky" bytes in Ghidra would be a genuine contribution over the original. |
| 6 | `d2ref/eng/part_1.html` | The spine — every leaf page hangs off this offset table. Convert with #5 so the chapter has both skeleton and hard part. **Must carry a prominent pre-1.09 version warning** (see §5). |
| 7 | `d2ref/eng/quests.html` + `quests_2.html` | Quest bitfield semantics are the most-wanted D2S knowledge and are ghidra-checkable. Convert as one unit — the split is an artifact of 2001 page sizes. |
| 8 | `d2ref/eng/maps.html` | The **only** documentation of the `.MAP`/`.MA*` sidecar format anywhere. Self-contained; nothing else covers it. |
| 9 | `d2ref/eng/items_15.html` + `part_4.html` | The 15-byte item record plus ~50 concrete type codes. `part_4` alone is a stub; merged they are a real section. |
| 10 | `d2ref/eng/{hardcore,signatur,class,menulook}.html` | Four short, sharply-defined, individually verifiable pages that complete the D2S header chapter cheaply. |

**Prerequisite for #5–10:** obtain a `.d2s` fixture. There is none in this repo.
Do this before scheduling any D2S work, or the whole block stalls.

**Everything else is deferred or dropped.** Do not convert `d2ref/` (French) or
`fra/d2ref/` at all. Convert the 32 set pages only if a data-tables appendix is
wanted, and regenerate it from `SetItems.txt` rather than transcribing French
prose the author already flagged as incomplete.

---

## 4. Proposed chapter groupings

Clustered by subject, not by the original site's navigation.

| Ch. | Title | Source pages | Through-line |
|---|---|---|---|
| 1 | **The DS1 Map Format and Its Editor** | `_divers/ds1/doc/index.html`, `dl_ds1edit.html` | What a DS1 *is* — layers, cells, sub-tiles, special tiles — told through the tool built to edit it. The bitfield dialog is the reveal: the editor exposes the on-disk flags directly. |
| 2 | **Warps: Connecting Levels by Hand** | `_divers/ds1/doc/tut01/index.html` | One worked example from Rogue Encampment to Graveyard, then a second warp invented from scratch. The three D2Common assertions are the narrative spine — each failure teaches one rule. |
| 3 | **Breaking the 60-Monster Limit** | `_divers2/tut_any_units_ds1/index.html` | A hardcoded DLL table, a community trick that abused out-of-bounds indices, and the 1.10 patch that softcoded the table and broke every map relying on the trick. Best story in the archive. |
| 4 | **DT1: Tiles on Disk** | `_divers/dt1/*.c`, `_divers/dt1_doc/dt1doc_data/*.gif`, `src/core/dt1.c` | **Reconstruction chapter.** Rebuild the lost page from two independent implementations plus D2CMP. Header 276/272, block 96, sub-tile 20; orientations 1–9; floor flags. |
| 5 | **The D2S File: Header and Fixed Fields** | `part_1`, `signatur`, `hardcore`, `class`, `name`, `playconf`, `location`, `title`, `levlcopy`, `unknown1`, `datatype` | The easy 578 bytes. Opens with the offset table and closes at the wall: `0232h`. |
| 6 | **The D2S File: The Variable-Length Block** | `part_2`, `part_3` | The hard part. Two flag bytes decide which of 16 fields exist; nothing after has a fixed offset. Then the skill array and its counter-intuitive ordering. The chapter that justifies the book. |
| 7 | **Quests, Waypoints, and World State** | `quests`, `quests_2`, `waypoint`, `unknownq`, `maps` | Progression as bitfields, plus the `.MAP` sidecar and the map-seed transplant trick as the payoff. |
| 8 | **Items in the Save File** | `part_4`, `items_15`, `part_5`, `part_5b` | Record sizes across versions, the 15-byte simple item decoded, then corpse and mercenary — including the author's own admission that the tail is wrong. |
| 9 | **Appearance and Cosmetic State** | `menulook`, `shortcut`, `handskil` | The 32-byte composite struct; why the menu look is a lie the game overwrites on save. Short chapter, strong hook. |
| 10 | *(optional)* **Data Tables Appendix** | `d2_sets/*` | Regenerate from `SetItems.txt`. Cite Siramy's 2001 transcription as provenance; do not reproduce it. |

Chapters 1–4 form the DS1/DT1 book; chapters 5–9 form the D2S book. They share
an author and a domain but not a subject, and they have opposite verification
costs (§2). Consider whether they are one volume or two.

---

## 5. Outdated / contradicted / superseded — flagged

### Superseded outright
- **`d2ref/` (27 French pages)** — superseded by `d2ref/eng/`, 10 days behind.
- **`fra/d2ref/` (10 pages)** — dead; carries its own abandonment banner.
- **All 32 `d2_sets/` pages** — superseded by `SetItems.txt`; author flags 30 of 32 as unvalidated.
- **`_divers/ds1/dl_ds1edit.html`** — every download link dead; historical only.

### Version drift (correct when written, wrong for 1.13c)
- **`signatur.html` stops at 1.09 (`5C`).** 1.10+ uses `60`; 1.13c uses `61`. Anything downstream that switches on version is incomplete.
- **`part_1.html`'s entire offset table is pre-1.09.** The 1.09+ header inserts file-size and checksum fields, moving the character name from `0008h` to `0014h` and shifting **every subsequent offset on the page**. A reader applying `part_1` to a modern save gets garbage from byte 8 onward. **Version-scope prominently; do not correct silently.**
- **`class.html`** explicitly says "correct if you have patch 1.08" and documents that 1.08 renumbered classes 05–07 → 07–09. Two numbering schemes exist; the page states which it uses. Keep both.
- **`items_15.html`** tested on 1.05 only. Item encoding changed substantially by 1.09+.
- **`part_5b.html`** documents pre-1.08 behaviour and is *correctly* framed as historical — a model for how the other pages should have been scoped.
- **`tut01`** written on the 1.10 **beta**; `tut_any_units` tested on 1.13. Where they overlap (adding a trap door), `tut_any_units` explicitly supersedes `tut01`'s method: *"check that tutorial… This is an easier alternative, if you're using the patch 1.10."* **Author-acknowledged supersession — preserve the pointer.** The existing markdown conversions do not.

### Author-flagged errors (do not propagate)
- **`part_5.html`** carries a red warning that the "unknown data" section is *incorrect* — expansion shortcut keys change the format. Convert the warning, not the wrong description.
- **`history.html`** (07-26 entry) records an unfixed error: *"in act 3, number and name of quests don't correspond. Will be fixed later."* Check `quests.html` Act 3 against the game before converting; the fix may never have landed.
- **`waypoint.html`** Act V waypoint names are **blank** (dots). The author asked for them and never got them. Fill from `Levels.txt`.
- **`quests.html`** cow-level section: *"Not verified, take this as is."*
- **`unknownq.html`** bit map is admittedly partial (3 of 8 bits unknown).
- **`part_2.html`** two "tricky" bytes unexplained; the author speculates they are one field split across the structure. **Resolvable in Ghidra — a genuine contribution the modernization can make.**

### Contradictions between the repo's own files and the archive
- **`docs/preservation/README.md` mis-dates two pages.** It lists the ds1edit documentation as **(2007)** and tutorial 1 as **(2011)**. The archived files' own `Date-Revision-yyyymmdd` meta tags say **2006-03-05** and **2004-07-01**, and the in-page revision histories agree. The README's dates appear to be capture/mtime-derived, not content dates. **Fix the README, and use the meta dates when scoping claims.**
- **`docs/preservation/README.md`** says the pages "carry dates from 2002 to 2011". The `d2ref/` and `d2_sets/` trees are **2001**. The range should read 2001–2011.
- **`fra/d2ref/index.html`** announces *"plus de version anglaise"* (no more English version) while `d2ref/eng/` not only exists but is the **most current** tree. Stale banner, not information.

---

## 6. Images — load-bearing vs decorative

643 images. Roughly a third carry information.

### Load-bearing — carry information markdown cannot replace

| Set | Count | Why |
|---|---|---|
| `_divers/ds1/doc/bitfield*_big.gif` | 4 | **Highest-value images in the archive.** The Advanced Tile Editing dialog showing per-layer tile flag bits — a format diagram wearing a GUI costume. Cross-check against `CELL_W_S`/`CELL_F_S`/`CELL_S_S` in `src/structs.h`. Transcribe to a table *and* keep the image. |
| `_divers/ds1/doc/f1*/f2*/f3_*/f4_*/f10*/f11*` layer-toggle pairs | ~30 | Before/after pairs showing what each layer contains. The comparison *is* the content. |
| `_divers/ds1/doc/area*_big.gif`, `copy*_big.gif`, `cut*_big.gif` | ~19 | Step sequences for selection and copy/paste; red/green paste-collision preview. |
| `_divers/dt1_doc/dt1doc_data/*.gif` | 24 | **Orphaned but load-bearing.** `floor_flags`, `floor_grid`, `fence_grid1/2`, `or_1`–`or_9`, `system1/2/3` — DT1 orientation and flag diagrams. The only surviving trace of the lost DT1 page. |
| `_divers2/tut_any_units_ds1/images/01–04, 08, 09, 13, 14, 17` | ~9 | `MonPreset.txt` / `Obj.txt` table screenshots and the index-resolution logic diagram (`08.png`). Transcribe `01.png` and `08.png` to tables. |
| `_divers2/view_pl2_cmaps/*.gif` | 6 | Filenames encode PL2 colormap block offsets (`0049`, `0305`, `0561`, `0817`, `1073`, `1457`) with their semantics. Data, despite the missing page — check against `misc_pl2_correct()` in `src/misc.c:1300`. |
| `_divers/ds1/doc/tut01/*` warp/roof figures | ~12 | Sub-tile coordinate grid and mouse-select box geometry for `LvlWarp.txt` — the numbers only make sense against the figure. |
| `_divers/d2_gif_palettes/*.gif` | 20 | Act palettes as images; extractable as actual palette data. |

### Decorative or redundant — drop

| Set | Count | Why |
|---|---|---|
| `_divers/ds1/doc/*_small.gif` | 76 | Thumbnails of the `_big` versions. Pure duplication — keep the `_big`. |
| `_divers/ds1/doc/night*`, `gamma*` | ~8 | Cosmetic previews of an editor toy the author calls "just a toy for now". |
| `d2_sets/**/*.gif` | ~300 | Item icons. Decorative, and superseded by the game's own DC6 assets. |
| `_divers/d2_anim/*.gif` | 35 | Orphaned animation strips with no page; the token names in the filenames may be worth a table, the images are not. |
| `_divers/ds1/screenshots/big_*` / `prev_*` | 16 | Gallery screenshots; `prev_*` already reused in `docs/assets/images/`. |
| Backgrounds, rules, navigation arrows (`right_arrow.gif`) | ~20 | Medium artifacts. Drop. |

---

## 7. Notes for the converter

1. **Check `docs/` before converting anything.** Four pages are already converted. The remaining work on them is verification and framing, not translation.
2. **Convert from `d2ref/eng/` only.** Confirmed via `history.html` diff, not assumed.
3. **Get a `.d2s` fixture before scheduling D2S work.** There is no save-file parser and no sample save anywhere in this repo.
4. **Version-scope every offset.** `part_1` in particular is pre-1.09 and silently wrong for 1.13c.
5. **The C sources outrank the HTML** for DT1, and `dt1make.c` independently corroborates `src/core/dt1.c`. That agreement is the DT1 chapter's foundation.
6. **`part_2`'s two unexplained bytes are a real open question** — resolving them in Ghidra is the clearest value the modernization can add over the original.
7. **Credit the chain.** `tut_any_units` credits TeknoKyo and SVR; `d2ref` credits the D2 Save Game Mapping Project (Hamel, Glenn C., Terje B., Harrison, IceTeaMan, Sephiroth), Jamella, and TheTelamon. Siramy did not claim sole discovery and neither should the modernized text.
8. **`MANIFEST.tsv`** (in `docs/preservation/`) carries sha256 and live-vs-Wayback provenance per file — cite it for source integrity rather than re-deriving.
9. **Rights are unresolved** — see the banner at the top of this file.
