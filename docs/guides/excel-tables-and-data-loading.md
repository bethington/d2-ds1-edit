# Excel tables and data loading

> **Provenance.** Verified against the vanilla 1.13c and 1.09d game trees
> (`F:\D2VersionChanger\VersionChanger\LoD\`) via `tools/d2mpq.py`, and against
> `D2Common.dll` 1.13c (`/Vanilla/1.13c/D2Common.dll` in the project's Ghidra
> database, image base `6fd50000`, SHA-256
> `59fa5928522f566f2bf99675571206ad70df889c89d3d07fa87edf5083e06e10`). Every
> numeric constant attributed to a function below was checked against its
> **disassembly**, not just its decompiled C — the two disagree at least once
> in this DLL, and the difference matters. Verified 2026-08-21. Companion
> report: [excel-tables-and-data-loading.verification.md](excel-tables-and-data-loading.verification.md).

Every number in Diablo II that isn't computed from a formula came out of a
spreadsheet. How much damage a Bone Spear does, whether a Cap can roll an
upgrade to Sacred Feather in Hell, which of the 573 things you can find
standing in a level counts as an object instead of a monster — all of it
lives in tab-separated text files under `data\global\excel\`, which the D2
modding community calls **excel tables**, or just **the .txt files**, from the
extension almost all of them carry. This is the layer nearly every mod
touches, because it's the layer that requires no compiler: drop a new
`weapons.txt` into the right archive and the numbers change.

It is also where the most common modding mistake in this book's scope lives.
Diablo II does not simply read the `.txt` file you edited. Most tables ship
with a second, precompiled sibling — a `.bin` — and the game's own loader
chooses between them at a single global switch. Get the choice wrong, or not
know it exists, and an edited `.txt` sits in the archive doing nothing while
the game keeps reading the stale binary beside it. This chapter follows one
table, `Objects.txt`, from its bytes in `Patch_D2.mpq` through the loader that
parses it, to the runtime function that turns an object ID into the name a
player sees on screen — and along the way, into the loader code that decides
whether your edit was ever read at all.

## The catalog

`data\global\excel\` holds on the order of ninety tables in 1.13c, covering
everything from `armor.txt` and `weapons.txt` (equipment) through
`treasureclassex.txt` (drop tables), `skills.txt`, `levels.txt` and
`monstats.txt`, down to small lookup tables like `elemtypes.txt` (eleven
element-name-to-token pairs) that almost nothing modifies. Not every table is
equally load-bearing for a modder: a working total conversion usually touches
`monstats.txt`/`monstats2.txt` (monster stats), `treasureclassex.txt` (drops),
`weapons.txt`/`armor.txt`/`misc.txt` (item bases), `skills.txt`/`skilldesc.txt`,
and `levels.txt`/`objects.txt` (world layout) long before it touches
`hitclass.txt`.

Twenty-seven of these tables were pulled fresh from the vanilla 1.13c archive
chain for this chapter (`python tools/d2mpq.py tables 1.13c`); twenty-three
came back for 1.09d, four short — 1.09d predates the monster-data split that
added `monstats2.txt`, `monpreset.txt`, `monplace.txt` and `monseq.txt` (see
[Version differences](#version-differences)). Every extraction records its
source archive, byte count, line count, record count, and a SHA-256 in a
`manifest.json` beside the tables — that manifest, not this chapter's prose,
is the citation record for every count below.

## The `.txt` format

Open `Objects.txt` and the format is immediately familiar to anyone who has
used a spreadsheet's "save as tab-separated" option, because that is
literally what it is: Blizzard's own tools exported these from Excel.

```
Name	description - not loaded	Id	Token	SpawnMax	Selectable0	...
Dummy	test data	0	NU0	0	0	...
Casket	Casket #5	1	CS1	0	0	...
```

A header row names the columns; one data row per record; fields separated by
tabs; lines terminated CRLF. `Objects.txt` has 160 columns in its header and,
in vanilla 1.13c, 575 non-empty lines under it. Two of those lines are not
object records:

- **Row 0 is the header** — always excluded from any record count.
- **One row partway down is a literal separator**, `Expansion` followed by
  nothing but empty tabs, marking where the Classic-only rows end and the
  Lord of Destruction rows begin. It sits at line 413 in `Objects.txt` and
  line 412 in `MonStats.txt` — the exact line number moves with the table's
  own Classic-row count, but the mechanism is the same in every LoD-era table
  from 1.09d through 1.14d. The game does not treat it as a record either.

So `575 lines − 1 header − 1 Expansion row = 573 records`, and that is the
number every tool in this book's toolchain — and, as the rest of this
chapter shows, the game's own loader — agrees on. The last data row is
`Dummy / door blocker / 572`; object IDs are zero-based, so 573 records is
exactly right.

Within a row, a cell is one of three things: a real value, **blank**, or the
literal string **`xxx`**. Blank is by far the most common — the first data
row of `Armor.txt` (`Cap/hat`) leaves 72 of its 164 columns empty, because
most of `Armor.txt`'s columns are optional per-item stat bonuses that only a
handful of rows use. `xxx` means something more specific: "no value, and
don't try to resolve one" — reserved for columns that would otherwise hold a
code looked up in another table. `Armor.txt`'s `Full Helm` row has
`HellUpgrade = xxx`, meaning that item has no Hell-difficulty upgrade at all,
as opposed to a blank cell that a naive parser might mistake for "not yet
decided." The convention isn't universal — `Objects.txt` and `MonStats.txt`
use zero `xxx` cells between them, while `Armor.txt` (344), `Weapons.txt`
(508), `Misc.txt` (428) and `MonStats2.txt` (1218) lean on it heavily for
their code-chain columns.

## From bytes to memory: the loader family

Parsing 160 tab-separated columns by hand, once per table, would be a lot of
near-identical code, and Blizzard didn't write it that way. `D2Common.dll`
carries a **generic table loader** that every `DATATBLS_Load*` function feeds
a small description of its own table, and lets the generic loader do the
actual file I/O and column parsing.

`LoadObjectsTxtData` (`D2Common 1.13c @ 6fd8f0c0`, body running to `6fd90c10`)
is `Objects.txt`'s half of that contract. It doesn't open a file itself.
Instead it builds, on its own stack, an array of thirty-odd
`FormatDescriptor` entries — one per column it cares about, each a
`{ name, byte-offset-in-the-record, size, type }` tuple. The last few, read
straight from the decompiled initializer, are representative:

| column name | record offset |
|---|---|
| `DrawUnder` | `0x1b6` |
| `OpenWarp` | `0x1b7` |
| `AutoMap` | `0x1bc` |

With that array built, it makes one call:

```
g_dwData_0b94 = <generic loader>(param_1, "objects", &descriptorArray,
                                  &g_dwData_0b98, 0x1c0);
```

— table name `"objects"` (no extension: the generic loader appends one),
the descriptor array, an out-pointer for the record count, and `0x1C0`
(448 decimal) as the size of one compiled record. `g_dwData_0b94` and
`g_dwData_0b98` (static globals at `6fdf0b94` / `6fdf0b98`) end up holding the
base pointer and record count for the whole table — every later read of
`Objects.txt` in the game goes through this pair.

The generic loader itself sits at `D2Common 1.13c @ 6fdaef40`. Its name in
this Ghidra database is `HasItemType3InInventory` — the DLL's own PE export
table genuinely does bind that string to this address (confirmed with
`list_exports`), which makes it Blizzard's label, not an analyst's guess, and
makes it wrong regardless of who wrote it: the function's decompiled body
never touches an inventory. It builds a path under `DATA\GLOBAL\EXCEL\`,
loads the named file through the archive layer, and hands the bytes to a
tab-delimited parser (`ParseTabDelimitedText`) that walks the
`FormatDescriptor` array and writes each column into its byte offset in a
freshly zeroed, `recordCount × dwRecordSize`-byte buffer. A pre-existing
comment in this project's Ghidra database attributes it a community name,
`DATATBLS_CompileTxt` — consistent with what it does, though this chapter
did not independently verify the ordinal number attached to that name.

`MONSTERS_LoadSuperUniquesTable` (`@ 6fda9870`) is built the same way: a
descriptor array for `SuperUniques.txt`'s ~30 columns, one call into the same
generic loader with table name `"superuniques"` and record size `0x34` (52
bytes — confirmed in the disassembly: `PUSH 0x34` immediately precedes
`CALL 0x6fdaef40`, and the record-walk loop that follows increments by the
same `0x34`). It goes further than `Objects.txt`'s loader: after parsing, it
builds a 66-entry lookup array keyed by each superunique's `hcIdx` field, and
if even one of the 66 hardcoded unique-boss slots comes back unfilled, it
aborts the process outright (`CleanupAndAbort`, error code `0x782`) rather
than starting the game with a hole in the roster. A mod that trims
`SuperUniques.txt` below 66 usable hcIdx values won't misbehave — it won't
launch.

Not every table goes through this path. `AnimData.d2` — animation frame-rate
and sequence data, one of the few tables with no `.txt` form at all — has its
own loader, `DATATBLS_LoadAnimDataTable` (`@ 6fd91e50`). It builds a 256-slot
hash bucket table where each bucket holds a variable-length run of records;
the record stride is `0xA0` (160 bytes), confirmed in the disassembly as
`count*5, shl 5` (`(count×5)×32 = count×160`) rather than trusted from the
decompiled C, because this project's decompiler has been caught rendering at
least one other table's stride wrong in exactly this kind of expression. The
[AnimData.d2 chapter](animdata-d2.md) follows this loader, its hash function,
and every one of the file's 3,558 records in full.

## The `.txt`/`.bin` fork — and the trap

Here is the fact that the rest of this chapter has been building toward:
**the generic loader has two completely different code paths, chosen by a
single global flag, and the shipped retail 1.13c binary defaults to the one
that ignores your `.txt` edit.**

The flag lives at `D2Common 1.13c @ 6fde9e20` (this chapter refers to it by
address; Ghidra records no meaningful name for it). Inside the generic
loader:

- **Flag `== 0`:** build `DATA\GLOBAL\EXCEL\<table>.txt`, load it, and run it
  through `ParseTabDelimitedText` + the `FormatDescriptor` array exactly as
  described above. This is the path this chapter has been describing.
- **Flag `!= 0`:** build `DATA\GLOBAL\EXCEL\<table>.bin` instead, load *that*
  file, and skip parsing entirely — the loaded buffer's first 4 bytes are
  read directly as the record count, and the record array is taken to start
  immediately after, already laid out at the exact stride the generic loader
  would have used to build one.

Both suffix strings are real, readable bytes in the binary, not decompiler
inference: `.txt` at `6fddda94`, `.bin` at `6fddda80`.

**Verified: the shipped retail 1.13c `D2Common.dll` has this flag set to `1`
— the binary-reading branch — at rest.** This was checked two ways that
agree exactly: reading the byte through Ghidra's loaded image, and
independently parsing the untouched file at
`F:\D2VersionChanger\VersionChanger\LoD\1.13c\D2Common.dll` with `pefile`,
resolving the RVA from the image base by hand. Both reads return
`01 00 00 00`.

Three more things corroborate that this branch is real and exercised, not
dead code left over from a build tool:

1. **`LoadAllDataTables`'s own cleanup path special-cases it.** Near the end
   of that function: `if (g_dwData_9e20 != 0) dwResourceBuffer =
   DAT_6fdf006c - 4;` before freeing a buffer — because in binary mode the
   "record array" pointer handed out to callers is 4 bytes past the real
   allocation start (the count header sits in those 4 bytes), so freeing it
   correctly requires walking back over the header first. Code doesn't grow
   a special case for a path that never runs.
2. **`D2Exp.mpq` ships a matching `.bin` for essentially the entire excel
   catalog** — seventy files enumerated directly
   (`data\global\excel\*.bin`), covering everything from `armor.bin` through
   `weapons.bin`, including `objects.bin`. Several — `monstats.bin`,
   `missiles.bin`, `gems.bin` — carry the `ENCRYPTED|FIX_KEY` archive flags,
   which only matter to a reader that actually opens them through the normal
   decrypt-on-open path; there is no reason to encrypt a file nothing reads.
3. **The bytes match, exactly, three independent ways.** Extracting
   `objects.bin` from `Patch_D2.mpq` by its known name (pattern-based listing
   can't find it — more on that below) gives a 256,708-byte file. Its first
   4 bytes, read as a little-endian `u32`, are `573` — the known-good vanilla
   `Objects.txt` record count. The bytes immediately after spell
   `Dummy\0` — the `Name` field of record 0, matching `Objects.txt`'s own
   first data row exactly. And `(256,708 − 4) ÷ 573 = 448 = 0x1C0`, the
   *exact* record size `LoadObjectsTxtData` passes to the generic loader.
   Every one of those three numbers was reachable independently — from the
   file's own header, from the `.txt`, and from the loader's call site — and
   all three land on the same figure.

What this chapter could **not** settle: nothing inside `D2Common.dll` itself
ever writes to this flag. All 123 cross-references to it are reads. Whatever
makes an ordinary `.txt`-editing mod work in practice — and it self-evidently
does, `.txt`-only mods are the overwhelming majority of Diablo II mods ever
made — has to happen outside this DLL: in `D2Client.dll`, in `Game.exe`, or
in a loader that isn't part of the stock chain this chapter traced. This is
marked `(unverified)` rather than guessed at, and it is the single most
important open question in the companion report.

The practical consequence for a modder is symmetric either way. If binary
mode is active for a table (as it demonstrably is by default for anything
routed through the generic loader), editing only the `.txt` changes nothing
until the accompanying `.bin` is also replaced or removed — the game reads
back the record count and the raw bytes without ever glancing at the newer
text file sitting right beside it. Conversely, a small subset of tables
never take this branch at all: `DATATBLS_OpenExcelWithDebugSave`
(`@ 6fdae710`), used by `Levels`/`LevelDefs`, item-property initialization,
and `Missiles.txt`, always opens `.txt` and only *additionally* writes a
fresh `.bin` when a separate debug flag (`g_dwPrimaryTemplateDebugEnabled @
6fdf145c`, also confirmed `0` in the shipped retail binary) is set — for
those specific tables, a `.txt` edit is never shadowed by a stale `.bin` in
the first place.

> **Ghidra caveat.** Two other functions in this address range carry names
> that claim to be `Objects.txt`'s runtime accessor and are not. Address
> `6fd8e980` is labelled `GetAnimSequenceRecord` with a header comment about
> animation sequences; its own code reads `g_dwData_0b94`/`g_dwData_0b98` —
> the exact globals `LoadObjectsTxtData` populates — with the same
> `index × 0x1C0 + base` shape as the correctly-named function below. It
> *is* an `Objects.txt` accessor; the label is simply wrong. In the other
> direction, `DATATBLS_GetObjectsTxtRecord` (`@ 6fd51800`) disassembles
> cleanly to a bounds-checked accessor with stride `0x1B8` (440 bytes,
> confirmed via `IMUL EAX,EAX,0x1b8`) — but it reads a *different* pair of
> globals (`[sgptDataTables]+0xc18`/`+0xc1c`), and every one of its eight
> callers is an item or equipment function, not a world-object one. Despite
> its name, this chapter found no evidence it has anything to do with
> `Objects.txt`, and does not cite it as such. A function name in this
> database is a hint, never evidence — the whole reason this chapter
> confirms constants against disassembly rather than decompiled C.

## Runtime lookup and bounds checking

Once `LoadObjectsTxtData` has run, every later read of an object's row is
the same three-instruction shape, and `OBJECTS_GetObjectName`
(`D2Common 1.13c @ 6fdad800`) is the cleanest place to see it — it is a
correctly-named function end to end, which makes it a better citation than
the mislabeled accessor next to it. Given an object index in `ECX`, its
disassembly is:

```
CMP  ECX, [0x6fdf0b98]      ; index < record count?
JL   ok
...                          ; out of range -> fatal abort
ok:
TEST ECX, ECX                ; index >= 0?
JGE  compute
...                          ; negative -> fatal abort
compute:
IMUL EAX, EAX, 0x1c0         ; EAX = index * 448
ADD  EAX, [0x6fdf0b94]       ; EAX = base + index*448
```

— then it reads the string at that address (the `Name` field sits at offset 0
of the record) and copies it out, falling back to a generated
`"object %d"` string only if the `Name` field is empty. This is how a level
places a casket and knows to call it "Casket": the object's numeric ID,
stored in the level's own data, becomes a direct pointer arithmetic
expression into the array `LoadObjectsTxtData` built at startup — no
re-parsing, no file I/O, just an index bounds-checked against a count that
hasn't changed since the load screen.

That bounds check is not a clamp. An index of `-1` or `≥ g_dwData_0b98` does
not return a null pointer or a default record — it calls `CleanupAndAbort`
and terminates. This is the same fatal-rather-than-defensive posture as
`SuperUniques.txt`'s hcIdx check above, and it generalizes: Diablo II's excel
tables are trusted absolutely once loaded. If another table (a level's
preset-room list, a monster's treasure class, a skill's missile ID) holds an
ID one past the end of a table a mod has shrunk, the game does not degrade —
it crashes at the exact line that tried the read, with the source file
string it was compiled against (`..\Source\D2Common\DRLG\DrlgLogic.cpp`, a
shared assert helper, not diagnostic of which table failed) still burned
into the binary twenty years later.

## MPQ load order

None of the above matters if the loader can't find the file, and *which*
file it finds depends entirely on archive order — this is the mechanism that
lets a mod replace a table without touching a single byte of Blizzard's own
archives. Diablo II searches its MPQs in a fixed priority order, patch
archives first:

```
Patch_D2.mpq > D2Exp.mpq > D2Xtalk.mpq > D2XMusic.mpq > D2Xvideo.mpq >
D2Data.mpq > D2Char.mpq > D2Sfx.mpq > D2Music.mpq > D2Speech.mpq > D2Video.mpq
```

The first archive in that list that contains the requested internal path
wins, full stop — later archives are never consulted. This is exactly the
mechanism a mod's own patch MPQ relies on: ship a `Patch_D2.mpq` (or a
loader that inserts an equivalent archive ahead of it) containing a modified
`Objects.txt`, and every read of `data\global\excel\objects.txt` resolves to
the mod's copy without `D2Data.mpq` or `D2Exp.mpq` changing at all. In
vanilla 1.13c, `Objects.txt` itself demonstrates the chain in action: it
exists in `Patch_D2.mpq`, `D2Exp.mpq`, *and* `D2Data.mpq`, and the version
that ships to players is specifically the `Patch_D2.mpq` copy — extracting
from `D2Data.mpq` directly gets the pre-patch table instead.

Confirming *which* archive wins is not the same as being able to list what's
inside it, and that gap is worth knowing about before it produces a wrong
conclusion. `Patch_D2.mpq` in 1.13c carries no `(listfile)` at all — 209
blocks, zero of them recoverable by name through pattern-based enumeration,
even when probing every sibling archive's listfile for candidates. Asking
"what tables does `Patch_D2.mpq` override" by listing its contents returns
nothing. Asking for `objects.txt` and `objects.bin` **by their exact known
names** returns both files correctly, because extraction hashes the
requested name directly against the archive's hash table — it never needs
the listfile at all. MPQ hash tables store hashes, not names; a listfile is
only a convenience for enumeration, and its absence has no effect on lookup.
The same asymmetry shows up in `D2Exp.mpq`'s listfile, which *is* present
but still incomplete: `monstats2.txt`, `monpreset.txt`, `monplace.txt` and
`monseq.txt` are real, readable files that no listfile in any 1.13c archive
names. Enumeration is not, and cannot be treated as, an inventory.

## The practical traps

Three consequences fall directly out of the mechanisms above, and each one
has bitten a real project:

**A checked-in table may not be vanilla's.** `assets/excel/` in the sibling
`d2-ds1-edit` tooling is gitignored and populated from whatever installation
a developer last pointed it at — historically a Project Diablo 2 tree, which
appends rows rather than editing them in place. PD2's `objects.txt` carries
626 records against vanilla's 573; every one of the fifty-three extra rows
is additive, so a statement like "there are 573 objects" or "object ID 572
is the last one" is silently wrong if it was checked against that cached
copy instead of a real archive. The fix is mechanical: the vanilla record
counts in this chapter's [reference table](#reference-tables) are the check,
and `python tools/d2mpq.py counts <file.txt>` flags contamination directly.

**An edited `.txt` can be invisible.** As established above, the generic
loader's default in shipped retail 1.13c reads the precompiled `.bin`, not
the `.txt`, for every table that goes through it. A mod that patches
`Objects.txt` without also replacing or removing `Objects.bin` in the same
archive is patching a file the loader never opens, for reasons that will not
surface as an error — the game starts, loads normally, and simply keeps the
old data.

**Don't trust an archive's listing to say what it contains.** `Patch_D2.mpq`
has no listfile at all in 1.13c, and the listfiles that do exist elsewhere
are demonstrably incomplete. A tool, or a person, that concludes "this table
isn't overridden" from a directory listing rather than a direct
by-name lookup can be wrong in either direction — missing an override that's
really there, or missing a base file that a listfile simply never named.

## Reference tables

### Table inventory — vanilla 1.13c

Extracted via `python tools/d2mpq.py tables 1.13c`; full byte counts, line
counts and SHA-256 per table are in the generated `manifest.json`, which is
the citation record this table summarizes.

| table | source archive | records |
|---|---|---|
| `objects.txt` | `Patch_D2.mpq` | **573** |
| `monstats.txt` | `Patch_D2.mpq` | **734** |
| `monstats2.txt` | `Patch_D2.mpq` | 609 |
| `monpreset.txt` | `Patch_D2.mpq` | **229** |
| `monplace.txt` | `Patch_D2.mpq` | 37 |
| `monseq.txt` | `Patch_D2.mpq` | 1010 |
| `superuniques.txt` | `Patch_D2.mpq` | 66 |
| `levels.txt` | `Patch_D2.mpq` | 137 |
| `lvlprest.txt` | `Patch_D2.mpq` | 1091 |
| `lvltypes.txt` | `Patch_D2.mpq` | 36 |
| `lvlsub.txt` | `Patch_D2.mpq` | 34 |
| `lvlmaze.txt` | `Patch_D2.mpq` | 81 |
| `lvlwarp.txt` | `D2Exp.mpq` | 88 |
| `objgroup.txt` | `D2Exp.mpq` | 132 |
| `objtype.txt` | `D2Exp.mpq` | 573 |
| `objmode.txt` | `D2Exp.mpq` | 8 |
| `shrines.txt` | `Patch_D2.mpq` | 23 |
| `misc.txt` | `Patch_D2.mpq` | 151 |
| `armor.txt` | `Patch_D2.mpq` | 202 |
| `weapons.txt` | `Patch_D2.mpq` | 306 |
| `itemtypes.txt` | `Patch_D2.mpq` | 103 |
| `setitems.txt` | `Patch_D2.mpq` | 127 |
| `uniqueitems.txt` | `Patch_D2.mpq` | 402 |
| `missiles.txt` | `Patch_D2.mpq` | 684 |
| `skills.txt` | `Patch_D2.mpq` | 357 |
| `sounds.txt` | `Patch_D2.mpq` | 4699 |
| `treasureclassex.txt` | `Patch_D2.mpq` | 853 |

The three figures most often cited elsewhere in this book, in one line:
**Objects 573, MonStats 734, MonPreset 229.**

### Loader family — `D2Common.dll` 1.13c (base `6fd50000`)

Every address below was located by address and cross-reference, not by
trusting its Ghidra label; the "confirmed" column notes what was checked
against raw disassembly rather than decompiled C.

| address | Ghidra name | what it actually does | confirmed via disassembly |
|---|---|---|---|
| `6fd8f0c0` | `LoadObjectsTxtData` | Builds `Objects.txt`'s `FormatDescriptor` array (30 columns), calls the generic loader with record size `0x1C0` | record size and load-loop stride (`+= 0x1c0`) |
| `6fdaef40` | `HasItemType3InInventory` (export name; behavior is unrelated) | Generic table loader/compiler — the `.txt`/`.bin` fork lives here | mode flag `g_dwData_9e20`, suffix strings `.txt`/`.bin` |
| `6fda9870` | `MONSTERS_LoadSuperUniquesTable` | Loads `SuperUniques.txt` (record size `0x34`), builds the 66-slot hcIdx table, aborts if any slot is unfilled | `PUSH 0x34` at the call site; loop increment `+= 0x34` |
| `6fd91e50` | `DATATBLS_LoadAnimDataTable` | Loads `AnimData.d2` (no `.txt` form) into a 256-bucket hash table, record stride `0xA0` | `LEA ECX,[ECX+ECX*4]; SHL ECX,5` = count×160 |
| `6fdae3c0` | `DATATBLS_OpenExcelFile` | Single-mode file opener (path build + `RESOURCE_AllocateAndOpen`, no `.txt`/`.bin` branch) | no cross-references found anywhere in the binary — unreferenced in this build |
| `6fdae710` | `DATATBLS_OpenExcelWithDebugSave` | Always opens `.txt`; additionally dumps a `.bin` when `g_dwPrimaryTemplateDebugEnabled != 0` (confirmed `0` in retail) | suffix string `.bin`, `"wb"` open mode |
| `6fdad800` | `OBJECTS_GetObjectName` | Correctly-named runtime accessor: bounds-checks an object index, returns its `Name` field | `IMUL EAX,EAX,0x1c0` / `ADD EAX,[6fdf0b94]` |
| `6fd8e980` | `GetAnimSequenceRecord` (**mislabeled**) | A second `Objects.txt` accessor — reads the same globals `OBJECTS_GetObjectName` does | same `× 0x1C0` shape, confirmed via xref to `g_dwData_0b94`/`0b98` |
| `6fd51800` | `DATATBLS_GetObjectsTxtRecord` (**name unproven**) | Bounds-checked accessor, stride `0x1B8`, but reads unrelated globals; callers are all item/equipment functions | `IMUL EAX,EAX,0x1b8` — real, but not `Objects.txt` |
| `6fdb6160` | `LoadAllDataTables` | Top-level orchestrator; ~50 table loaders in sequence; its own cleanup path special-cases `g_dwData_9e20 != 0` | `if (g_dwData_9e20 != 0) ptr -= 4` before free |

## Version differences

| what | 1.13c | 1.09d |
|---|---|---|
| `Objects.txt` source archive | `Patch_D2.mpq` | `D2Exp.mpq` |
| `Objects.txt` records | 573 | 573 |
| `MonStats.txt` records | 734 | 575 |
| `MonStats2.txt` / `MonPreset.txt` / `MonPlace.txt` / `MonSeq.txt` | present | absent — not shipped before the 1.10 monster-data split |
| excel tables in this chapter's extraction | 27 | 23 |
| `.txt`/`.bin` fork default (`g_dwData_9e20`) | binary (`1`), verified | not checked |

> **Version note (1.09d):** `Objects.txt` is byte-for-byte the same 573
> records as 1.13c (four bytes differ across the whole file, no rows do),
> but it is read from `D2Exp.mpq` rather than `Patch_D2.mpq` — 1.09d's own
> patch archive does not carry a copy. `MonStats.txt` is a genuinely
> different table: 575 monster records against 1.13c's 734, since the
> intervening patches added the missing 159 rather than only rebalancing
> existing ones.

---

Companion report: [excel-tables-and-data-loading.verification.md](excel-tables-and-data-loading.verification.md).
