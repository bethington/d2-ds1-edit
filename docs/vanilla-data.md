# Vanilla game data — how to check a claim against the real tables

Every data claim in this book is checked against Blizzard's own archives, read
straight out of a stock Diablo II installation. This page explains why that is
not optional, how to do it, and what the right answers look like.

- [Why `assets/excel/` is not ground truth](#why-assetsexcel-is-not-ground-truth)
- [Why `mpyq` alone gives you the wrong answer](#why-mpyq-alone-gives-you-the-wrong-answer)
- [Using the tool](#using-the-tool)
- [Where the archives live](#where-the-archives-live)
- [The local cache](#the-local-cache)
- [Known-good vanilla record counts](#known-good-vanilla-record-counts)
- [How a record is counted](#how-a-record-is-counted)
- [Limits worth knowing](#limits-worth-knowing)

## Why `assets/excel/` is not ground truth

There are tab-separated tables checked into `assets/excel/` in this repository.
They are **not vanilla**. They came from a Project Diablo 2 installation, and PD2
adds rows.

| `objects.txt` | lines | records | PD2-only rows present |
|---|---|---|---|
| vanilla 1.13c (`Patch_D2.mpq`) | 575 | 573 | none |
| `assets/excel/objects.txt` (PD2) | 628 | 626 | `UberAncientAltar`, `PVPTome1`, `LucionChest`, … |

Fifty-three extra objects. Every one of them shifts nothing — PD2 appends — but
their presence means any statement of the form "there are N objects", "the last
object is X", or "object ID 573 is …" is wrong if it was derived from the
checked-in copy. The same file is where the editor falls back when it cannot
reach an MPQ, so it is easy to read by accident.

The tables are gitignored (`assets/excel/*.txt`) precisely because they are
whatever the last developer happened to have installed. Treat them as a runtime
cache belonging to somebody's game, never as a citation.

> Note on the count. `docs/book-conventions.md` quotes PD2's `objects.txt` as
> 627 records. That figure counts the `Expansion` separator row as a record;
> under the counting rule used here and by `d2mpq.py` — which excludes both the
> header and the separator, giving vanilla its 573 — the PD2 file is 626. The
> two numbers describe the same 628-line file. Cite the vanilla 573 and the gap
> and you are safe either way.

**If you need a number for the book, extract it.** It takes about a second.

## Why `mpyq` alone gives you the wrong answer

The obvious Python approach is the `mpyq` package:

```python
import mpyq
data = mpyq.MPQArchive("Patch_D2.mpq").read_file(r"data\global\excel\objects.txt")
```

That call **silently returns garbage**, and it is the reason `tools/d2mpq.py`
exists.

Diablo II flags its data files `MPQ_FILE_IMPLODE` (`0x00000100`): each sector is
compressed with the PKWARE Data Compression Library, an older scheme than the
multi-method `MPQ_FILE_COMPRESS` (`0x00000200`) path. `mpyq` implements only the
latter. Its `read_file` gates decompression on the `COMPRESS` bit
(`mpyq.py:230`), so for an IMPLODE-flagged file it decompresses nothing, warns
nothing, raises nothing — it concatenates the raw compressed sectors and returns
them as if they were the file.

You get a `bytes` object of plausible length, and every read "succeeds":

| 1.13c `Patch_D2.mpq` → `objects.txt` | bytes | lines | "records" |
|---|---|---|---|
| `mpyq.read_file()` | 30,560 | 88 | 87 |
| `tools/d2mpq.py` | 211,970 | 575 | **573** |

The 30,560-byte answer even survives a line count, because compressed data
contains `0x0A` often enough to produce plausible-looking lines. Nothing about
the failure announces itself. A chapter that cites 87 objects is citing noise
that arrived through an API call that returned normally.

This is not confined to one table. On 1.13c every table shadowed by
`Patch_D2.mpq` is IMPLODE-flagged and every one of them reads wrong:

| table | `d2mpq.py` records | `mpyq` records |
|---|---|---|
| `objects.txt` | 573 | 87 |
| `monstats.txt` | 734 | 216 |
| `monpreset.txt` | 229 | 2 |
| `levels.txt` | 137 | 33 |
| `lvlprest.txt` | 1091 | 160 |
| `lvltypes.txt` | 36 | 6 |
| `superuniques.txt` | 66 | 5 |
| `monplace.txt` | 37 | 0 |

On 1.09d `mpyq` fails *loudly* instead — the same tables are additionally
`ENCRYPTED`, which raises `NotImplementedError`, or reach it through the
`COMPRESS` path with a DCL mask byte, which raises `RuntimeError`. That is the
harmless case. The dangerous case is 1.13c, where the read comes back clean.

`tools/d2mpq.py` reuses `mpyq` for what it gets right — header, hash table,
block table, key derivation, decryption — and reimplements `read_file` with a
pure-Python PKWARE DCL `explode` (transcribed from Mark Adler's public-domain
`blast.c`) applied per sector. It also handles the encrypted and
`ENCRYPTED|FIX_KEY` files `mpyq` refuses outright.

Every failure path raises `MpqReadError` naming the file, the block flags, and
the sector. Decompressed output is length-checked against the block table
before it is returned. The tool will not hand you bytes whose correctness it has
not established — a silent wrong answer is the exact defect it exists to
prevent.

That discipline earned its keep immediately. Sweeping every file in every
archive turned up two more bugs the excel tables alone could never have
revealed, both of which `mpyq` also has:

- a file with **no compression flag** has no sector offset table at all, so
  parsing one reads the file's own first bytes as offsets;
- the sector count is a **ceiling**, not `size // sector_size + 1` — a file
  whose size is an exact multiple of the sector size has no trailing partial
  sector, and counting one walks off the end of the offset table.

Both are fixed, and `selfcheck` now reads the smallest real example of each
structural shape in the archives — stored, imploded, compressed, single-unit,
sectored, exact-sector-multiple, encrypted — so neither can come back unnoticed.
Neither bug would have been caught by testing on excel tables, because no excel
table has either shape. They surfaced as *errors* rather than as wrong data
only because the reader checks its own output length.

## Using the tool

Requires `mpyq` (`pip install mpyq`). Nothing else.

```bash
# Prove the reader still works. Run this before trusting any number.
python tools/d2mpq.py selfcheck --version 1.13c

# One file to stdout, or to disk
python tools/d2mpq.py extract 1.13c "data\global\excel\objects.txt" --out objects.txt
python tools/d2mpq.py extract F:\path\to\D2Data.mpq "data\global\excel\levels.txt"

# What is in an archive
python tools/d2mpq.py list 1.13c --pattern "data\global\excel\lvl*"
python tools/d2mpq.py list F:\path\to\Patch_D2.mpq --pattern "*objects*"

# Bulk-extract the tables a chapter is likely to cite, into the local cache
python tools/d2mpq.py tables 1.13c
python tools/d2mpq.py tables 1.09d --out-dir /somewhere/else
python tools/d2mpq.py tables 1.13c --table monstats.txt --table monpreset.txt
python tools/d2mpq.py tables 1.13c --all-excel

# Line/record counts for files you already have (flags PD2 contamination)
python tools/d2mpq.py counts assets/excel/objects.txt
```

`extract` and `list` accept either a single `.mpq` path or a version, in which
case the whole MPQ chain is searched in the game's own priority order:

```
Patch_D2 > D2Exp > D2Xtalk > D2XMusic > D2Xvideo > D2Data > D2Char > D2Sfx > D2Music > D2Speech > D2Video
```

This ordering matters. On 1.13c, `objects.txt` exists in `Patch_D2.mpq`,
`D2Exp.mpq` **and** `D2Data.mpq`, with different contents; the game reads the
first. Extracting from `D2Data.mpq` directly gets you the pre-patch table.

Exit codes: `0` success, `1` the requested data was missing or a check failed,
`2` the tool could not read something it should have been able to.

The tool is importable as well as runnable:

```python
import sys; sys.path.insert(0, "tools")
import d2mpq

chain = d2mpq.archive_chain("1.13c")
src, data = d2mpq.read_excel(chain, "objects.txt", required=True)
lines, records = d2mpq.record_count(data)      # -> 575, 573
```

## Where the archives live

Nothing is hardcoded to one machine. The tool resolves a version name against a
root directory, in this order:

1. `--d2-root DIR` on the command line
2. `$D2_VERSIONS_ROOT`
3. the documented default, `F:\D2VersionChanger\VersionChanger\LoD`

On this project's development machine the trees are D2VersionChanger's, one
directory per version:

| version | path |
|---|---|
| 1.13c (the book's baseline) | `F:\D2VersionChanger\VersionChanger\LoD\1.13c\` |
| 1.09d | `F:\D2VersionChanger\VersionChanger\LoD\1.09d\` |

Elsewhere, either point `D2_VERSIONS_ROOT` at your own collection:

```bash
export D2_VERSIONS_ROOT=/mnt/games/d2versions   # tree per version underneath
python tools/d2mpq.py tables 1.13c
```

…or skip version names entirely and pass a directory or an archive wherever a
version is expected:

```bash
python tools/d2mpq.py tables "C:\Program Files (x86)\Diablo II"
python tools/d2mpq.py list "C:\Program Files (x86)\Diablo II\Patch_D2.mpq"
```

A plain single-version install works: the version argument is only a way of
naming a directory.

## The local cache

`tables` writes to `.vanilla-cache/<version>/excel/` by default. That directory
is **gitignored** and must stay that way.

The reader is ours. The data is Blizzard's. Extracted tables are verbatim
copyrighted game data and are never committed — the cache is a convenience so a
chapter's numbers can be re-checked without re-exploding the archives, and it is
regenerated on demand from *your* installation:

```bash
python tools/d2mpq.py tables 1.13c     # rebuild, ~1s
rm -rf .vanilla-cache                  # safe to delete at any time
```

Override the location with `--cache-dir` or `$D2_VANILLA_CACHE`.

Alongside the tables, `tables` writes `manifest.json` recording, per table, the
archive it came from, its size, its line and record counts, and its SHA-256.
That is the provenance record for a citation: it says which archive in which
tree produced the number, so a reader on a different install can tell whether
they are looking at the same bytes.

## Known-good vanilla record counts

`selfcheck` asserts these. If it fails, either the reader regressed or the tree
is not vanilla — do not adjust the expectations to make it pass.

### 1.13c — the book's baseline

| table | source archive | bytes | lines | **records** |
|---|---|---|---|---|
| `objects.txt` | `Patch_D2.mpq` | 211,970 | 575 | **573** |
| `monstats.txt` | `Patch_D2.mpq` | 432,634 | 736 | **734** |
| `monstats2.txt` | `Patch_D2.mpq` | 138,542 | 611 | 609 |
| `monpreset.txt` | `Patch_D2.mpq` | 3,594 | 230 | **229** |
| `monplace.txt` | `Patch_D2.mpq` | 658 | 38 | 37 |
| `monseq.txt` | `Patch_D2.mpq` | 27,761 | 1011 | 1010 |
| `superuniques.txt` | `Patch_D2.mpq` | 8,071 | 68 | 66 |
| `levels.txt` | `Patch_D2.mpq` | 66,615 | 139 | 137 |
| `lvlprest.txt` | `Patch_D2.mpq` | 134,937 | 1093 | 1091 |
| `lvltypes.txt` | `Patch_D2.mpq` | 10,719 | 38 | 36 |
| `lvlsub.txt` | `Patch_D2.mpq` | 3,466 | 36 | 34 |
| `lvlmaze.txt` | `Patch_D2.mpq` | 3,463 | 83 | 81 |
| `lvlwarp.txt` | `D2Exp.mpq` | 5,244 | 90 | 88 |
| `objgroup.txt` | `D2Exp.mpq` | 10,880 | 134 | 132 |
| `shrines.txt` | `Patch_D2.mpq` | 2,262 | 24 | 23 |
| `misc.txt` | `Patch_D2.mpq` | 55,821 | 153 | 151 |
| `armor.txt` | `Patch_D2.mpq` | 76,370 | 204 | 202 |
| `weapons.txt` | `Patch_D2.mpq` | 118,029 | 308 | 306 |
| `itemtypes.txt` | `Patch_D2.mpq` | 8,775 | 105 | 103 |
| `setitems.txt` | `Patch_D2.mpq` | 26,934 | 129 | 127 |
| `uniqueitems.txt` | `Patch_D2.mpq` | 73,610 | 404 | 402 |
| `missiles.txt` | `Patch_D2.mpq` | 192,979 | 685 | 684 |
| `skills.txt` | `Patch_D2.mpq` | 173,535 | 358 | 357 |
| `sounds.txt` | `Patch_D2.mpq` | 505,304 | 4700 | 4699 |
| `treasureclassex.txt` | `Patch_D2.mpq` | 88,253 | 854 | 853 |

The three counts most often cited, in one line: **Objects 573, MonStats 734,
MonPreset 229.**

### 1.09d

| table | source archive | bytes | lines | records |
|---|---|---|---|---|
| `objects.txt` | `D2Exp.mpq` | 211,974 | 575 | 573 |
| `monstats.txt` | `Patch_D2.mpq` | 394,191 | 577 | 575 |
| `superuniques.txt` | `Patch_D2.mpq` | 5,402 | 68 | 66 |

`objects.txt` is the same 573 records on 1.09d as on 1.13c — four bytes differ,
no rows do. `monstats.txt` is not: 1.09d has 575 monsters where 1.13c has 734.

## How a record is counted

D2's excel tables are CRLF-terminated, tab-separated, with a header row. The
expansion tables additionally carry a single `Expansion` separator row — line
412 in every version from 1.09d to 1.14d — marking where classic ends and the
expansion begins. The game skips it; it is not a monster or an object.

So:

```
records = non-empty lines − 1 header − Expansion separator rows
```

`record_count()` returns both numbers so a citation can say which it means, and
every command that prints a count prints lines and records side by side. When
you quote a figure in a chapter, quote **records** and say so — "573 objects",
not "575 lines".

Row IDs confirm the rule: the last data row of 1.13c `objects.txt` is
`Dummy / door blocker / 572`, and IDs are zero-based, so 573 records.

## Limits worth knowing

**Audio is not supported, loudly.** Blizzard's `.wav` files use the Storm WAVE
codec — Huffman entropy coding over IMA ADPCM (compression masks `0x41` mono
and `0x81` stereo). This tool does not implement it and does not pretend to: it
raises `MpqReadError` naming the mask. Use StormLib or Ladik's MPQ Editor for
audio. Every other file type in both trees reads.

That claim is measured. Sweeping every nameable file in all ten archives of
1.13c and 1.09d (large archives sampled, all `.txt`/`.bin`/`.tbl` read in full):

| archive | files read | ok | failed |
|---|---|---|---|
| `Patch_D2.mpq` (1.13c) | 117 | 117 | 0 |
| `Patch_D2.mpq` (1.09d) | 82 | 82 | 0 |
| `D2Data.mpq` | 500 | 500 | 0 |
| `D2Char.mpq` | 400 | 400 | 0 |
| `D2Sfx.mpq` | 31 | 31 | 0 |
| `D2Video.mpq` / `D2Xvideo.mpq` | 20 | 20 | 0 |
| `D2Exp.mpq` | 636 | 612 | 24 — all `.wav` |
| `D2Music.mpq` | 41 | 8 | 33 — all `.wav` |
| `D2Speech.mpq` | 400 | 0 | 400 — all `.wav` |
| `D2Xtalk.mpq` | 400 | 0 | 400 — all `.wav` |

Zero non-audio failures, in either version. `.dcc`, `.dt1`, `.ds1`, `.cof`,
`.bik`, `.tbl`, `.bin`, `.txt`, and the packed `.dll`/`.exe` files all read,
including the `ENCRYPTED|FIX_KEY` ones `mpyq` refuses outright.

**MPQ hash tables store hashes, not names.** An archive can only be enumerated
if it carries a `(listfile)`. 1.13c's `Patch_D2.mpq` does not — 209 blocks, no
listfile — so `list` falls back to probing the names its sibling archives
declare, and reports how many blocks it could not name (115 of 209). Extraction
by known name is unaffected; only enumeration is.

**D2's listfiles are themselves incomplete.** `monstats2.txt`, `monpreset.txt`,
`monplace.txt` and `monseq.txt` are real files in the archives that no listfile
mentions. They extract fine by name. This is why `tables` works from a curated
list rather than from enumeration, and why `--all-excel` is a floor rather than
a census.

**`.bin` beside `.txt`.** Most excel tables ship as both a `.txt` and a compiled
`.bin`. The game reads the `.bin` unless a text override is active. The `.txt`
is the human-readable source of the same data and is what this book cites; if
the two ever disagreed, the `.bin` would be what the game actually did.

**Speed.** `explode` is pure Python. A full excel extraction takes about a
second; sweeping a whole 10,000-file archive takes minutes. That is why there is
a cache.

## See also

- `tools/d2mpq.py` — the tool; its module docstring is the fuller technical note
- `docs/book-conventions.md` §5 — vanilla data as a binding rule for chapters
- `.gitignore` — the `/.vanilla-cache/` and `assets/excel/*.txt` entries
