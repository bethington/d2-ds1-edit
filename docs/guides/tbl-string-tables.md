# The `.tbl` String Tables

> **Provenance.** Verified against the vanilla 1.13c game tree
> (`F:\D2VersionChanger\VersionChanger\LoD\1.13c\`) via `tools/d2mpq.py`, and
> against `D2Lang.dll` 1.13c (`/Vanilla/1.13c/D2Lang.dll` in the project's
> Ghidra database, image base `6fc00000`, SHA-256
> `206386a81f4046c693f2aca60b9ee19fffd05b1157bbb60992c0d4caad9b67ec`), with
> `D2Client.dll` 1.13c (image base `6fab0000`, SHA-256
> `dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906`) checked
> for consumers. Every offset and constant below was confirmed against
> **disassembly**, not decompiled C, and then checked a second, independent
> way: a from-scratch Python re-implementation of the header parser, the hash
> function, and the linear probe was run against the complete, real
> `string.tbl`, `patchstring.tbl` and `expansionstring.tbl` — every active
> record in all three, 9,378 of them, not a sample. Verified 2026-08-21.
> Companion report:
> [tbl-string-tables.verification.md](tbl-string-tables.verification.md).

Open any Diablo II data file that touches the screen — `Levels.txt`,
`Skills.txt`, `SuperUniques.txt`, the item and quest tables — and you will
find that almost none of them contain the text a player actually reads.
`Levels.txt` calls Act V's second waypoint `Rigid Highlands`. The loading
screen and the waypoint menu both call it `Frigid Highlands`.
Neither is a typo. `Levels.txt` isn't holding display text at all — it's
holding a *key*, and every one of those keys resolves through one of three
small binary files under `data\local\lng\eng\`: `string.tbl`,
`patchstring.tbl`, and `expansionstring.tbl`. This chapter is about those
three files: the hash table inside them, the DLL that reads it, and what
happens when a key doesn't resolve to anything at all.

It's a deliberate layer, not an accident of the file format. Every piece of
text in the game — item names, skill descriptions, quest logs, NPC
dialogue, even the "K" / "M" / "B" abbreviations after a large gold number —
is looked up by a short internal key rather than being stored inline where
it's used. That indirection is what makes localization possible: ship a
Korean `string.tbl` next to the Korean client and every other file in the
game — `Levels.txt`, `Skills.txt`, the whole excel catalog described in
[Excel tables and data loading](excel-tables-and-data-loading.md) — is
untouched, because none of them hold displayable text to translate. It is
also, as the closing section of this chapter shows, the reason a handful of
Act V place names in the shipped game don't match what's sitting in
`Levels.txt` at all: the table can correct a key that predates the final
text, and the raw table alone will lie to you about what the game actually
displays.

This chapter follows one key end to end — `qstsa5q1`, the internal name for
Act V's first quest — from its raw bytes on disk, through the hash function
that locates it, to the string "Siege on Harrogath" a player sees in the
quest log. Along the way it covers the file's on-disk layout, the hashing
scheme, why there are three tables instead of one, and what happens on a
miss.

## Three files, one job

`data\local\lng\eng\` holds the English text for three separate tables, and
the game reads all three every time it starts:

| table | archive (1.13c) | bytes | records* |
|---|---|---|---|
| `string.tbl` | `D2Data.mpq` | 390,570 | 5,391 |
| `patchstring.tbl` | `Patch_D2.mpq` | 52,523 | 1,169 |
| `expansionstring.tbl` | `D2Exp.mpq` | 173,234 | 2,818 |

\* Active key→string entries — see [Reference tables](#reference-tables) for
how that number is derived from the file's own header.

`string.tbl` is the base Classic vocabulary. `expansionstring.tbl` is
everything Lord of Destruction added — Act V dialogue, the new waypoints,
the new quests. `patchstring.tbl` is smaller than both and is where balance
patches land new or corrected text without touching the other two files —
new unique items, new rune words, class-restriction labels, and, as the
[precedence section](#precedence-when-a-key-lives-in-more-than-one-table)
below shows, outright typo fixes to text `string.tbl` already shipped with.

Extracting them is its own small lesson in why this book insists on
`tools/d2mpq.py` over bare `mpyq` (see
[vanilla-data.md](../vanilla-data.md)): `patchstring.tbl` in 1.13c's
`Patch_D2.mpq` carries the `MPQ_FILE_IMPLODE` flag with **no `(listfile)`
at all** in that archive — the same failure mode `vanilla-data.md` documents
for `objects.txt`. A bare-`mpyq` read of it would return the raw compressed
sectors and look like it worked. Extraction by exact known name — the path
this chapter used — sidesteps the missing listfile entirely, because MPQ
lookup hashes the requested path directly against the archive's hash table
and never consults the listfile to do it.

The three tables are also not equally stable across versions. `string.tbl`
and `expansionstring.tbl` are **byte-identical** between 1.09d and 1.13c
(matching SHA-256 on both) — the base and expansion vocabularies haven't
changed since release. `patchstring.tbl` has grown from 1,006 active entries
in 1.09d to 1,169 in 1.13c: 163 patches' worth of new item names, rune
words, and corrected text, all landing in the one file designed to absorb
them. See [Version differences](#version-differences).

## On-disk layout

A `.tbl` file is a compact open-addressing hash table, and every one of the
three files uses the identical layout — same 21-byte header shape, same
field widths, same hashing scheme. `D2Lang.dll`'s loader
(`FUN_6fc0a130 @ D2Lang.dll 1.13c 6fc0a130`) builds the path
`data\local\lng\%s\string.tbl` (and the `patchstring`/`expansionstring`
variants — the three format strings sit at `6fc0d46c`, `6fc0d424`, and
`6fc0d3fc` respectively, confirmed by reading the bytes directly), loads the
raw file, and hands it to a validator
(`FUN_6fc094e0 @ 6fc094e0`) before touching a single string.

The file is four regions back to back:

```
0x00 ─ header ───────────────── 21 bytes (0x15)
0x15 ─ ID table ────────────── H × 2 bytes
0x15+2H ─ node array ────────── N × 17 bytes (0x11)
X ─ string data ──────────────── to end of file (S)
```

### The header

All seven fields were read directly out of the disassembly of the loader
and the lookup function — none of this came from the decompiler's
reconstructed C, which (as the [excel tables chapter](excel-tables-and-data-loading.md)
found for a different DLL) can silently fabricate an expression that looks
plausible and isn't.

| offset | size | field | confirmed by |
|---|---|---|---|
| `0x00` | word | **CRC** — checksum of the string-data region | `CMP CX,word ptr [ESI]` in `FUN_6fc094e0` @ `6fc0950c` |
| `0x02` | word | **H** — size of the ID table, in entries | `MOVZX EAX,word ptr [EBP+0x2]` in `FUN_6fc09360` @ `6fc0936b` |
| `0x04` | dword | **N** — node-array slot count; also the hash modulus | `MOV EAX,dword ptr [EBP+0x4]` @ `6fc09374`, passed straight into the hash call |
| `0x08` | byte | unverified — `1` in all six files sampled (three tables × two versions); no code path in `D2Lang.dll` reads it | — |
| `0x09` | dword | **X** — file offset where the string-data blob starts | `MOV ECX,dword ptr [ESI+0x11]` / `MOV EDX,dword ptr [ESI+0x9]` @ `6fc094f4`–`6fc094f7`, passed to the CRC routine |
| `0x0D` | dword | **P** — probe-count ceiling for a lookup | `CMP ESI,dword ptr [EBP+0xd]` in `FUN_6fc09360` @ `6fc093ee` |
| `0x11` | dword | **S** — total file size | `if (node_addr >= rawBase+S) → fatal` bounds check in `FUN_6fc094e0` |

`P` looks at first like it should just be `H` or `N` — it's neither. It's
covered on its own below, because what it actually holds is one of the more
interesting things this chapter found.

### The ID table

`H` little-endian words starting at `0x15`. Entry *i* holds the node-array
index of the record carrying legacy numeric string ID *i* — this is the
table a much older calling convention (`GetStringByID`, still very much
alive in 1.13c — see [Numeric IDs](#numeric-ids-a-second-front-door) below)
walks, and it's a completely separate access path from the key-string hash
lookup this chapter's worked example follows.

### The node array

`N` fixed 17-byte (`0x11`) records, starting right after the ID table. This
is the hash table proper — every key-string lookup lands somewhere in this
array.

| offset | size | field |
|---|---|---|
| `+0x00` | byte | **Active** — `1` if the slot holds a real entry, else the slot is empty and probing skips it |
| `+0x01` | word | **StringID** — the legacy numeric ID for this entry (what the ID table points back to) |
| `+0x03` | dword | unverified — see [the field this chapter couldn't verify](#the-one-field-this-chapter-couldnt-verify) |
| `+0x07` | dword | **KeyOffset** — file offset of the NUL-terminated key string |
| `+0x0B` | dword | **TextOffset** — file offset of the NUL-terminated display string |
| `+0x0F` | word | **TextLength** — byte length of the display string **including its terminating NUL** |

Every one of these six fields was confirmed against raw instructions, not
the decompiler: the active-flag compare (`CMP byte ptr [ECX],0x1` @
`6fc093a1`), the key-offset read (`MOV ECX,dword ptr [ECX+0x7]` @
`6fc093a9`), and the text offset/length pair (`MOVZX EDX,word ptr
[EBX+0x4]` / `MOV ECX,dword ptr [EBX]` @ `6fc0a310`–`6fc0a314`, where `EBX`
already points at `node+0xb`). The record stride itself is computed by a
multiply-by-17 trick rather than an `IMUL`: `SHL EAX,0x4` (×16) followed by
`LEA ECX,[EDI+EAX*1]` (+1×`EDI`) — `16x + x = 17x` — confirmed at
`6fc09396`–`6fc0939b` and reused identically in three other functions.

### The string data blob

Everything from `X` to the end of the file (`S`) is raw NUL-terminated
bytes — both key strings and display strings live in the same region,
referenced by absolute file offset from the node fields above. This is the
exact byte range the header's CRC covers, and it's also the reason the
region is called "string data" rather than "display text": key strings
share the space.

## The hash

The function that turns a key into a bucket index is a textbook
implementation of the ELF/PJW string hash — the same hash the ELF object
format itself uses for its symbol tables (`FUN_6fc08fe0 @ 6fc08fe0`):

```c
uint hash(char *key, uint modulus) {
    uint h = 0;
    for (; *key; key++) {
        h = h * 16 + (unsigned char)*key;
        if (h & 0xF0000000)
            h = ((h & 0xF0000000) >> 24 ^ h) & 0x0FFFFFFF;
    }
    return h % modulus;
}
```

`modulus` is always `N`, the node-array slot count from the header — never
`H`. The lookup function (`FUN_6fc09360 @ 6fc09360`) computes
`bucket = hash(key, N)`, and if that slot's `Active` byte is `0`, or its key
string doesn't match, it tries `(bucket + 1) % N`, then `(bucket + 2) % N`,
and so on — straight linear probing, capped at `P` attempts.

### Worked example: `qstsa5q1`

Quest names are keyed `qstsa<act>q<n>` — act number, quest slot within the
act. Act V, quest 1, is `qstsa5q1`, and it lives in `expansionstring.tbl`
(`N = 2819`). Running the hash by hand:

```
hash("qstsa5q1")  =  0x0AB94BC1  (before the modulus)
0x0AB94BC1 % 2819 =  1495
```

Bucket `1495` is occupied — but by a different key (`Hwanin's Refuge`, a
unique item name that happens to hash to the same slot). This is where
`expansionstring.tbl`'s real character shows: **2,818 of its 2,819 slots are
occupied.** With the table built to a load factor over 99.9%, a collision at
the very first probe isn't the exception, it's close to the norm — and
resolving one can mean walking a very long way. In this exact case, the
probe sequence runs forward from `1495`, wraps at `N` back to `0`, and
finally reaches an `Active` slot whose key string really is `qstsa5q1` at
bucket **`444`** — **1,769 probes later**, every single one of them landing
on another occupied, non-matching slot. The one empty slot in the entire
file sits at index 1297, nowhere on this particular path.

That node's fields, read straight from the file:

| field | value |
|---|---|
| `Active` | `1` |
| `StringID` | `2618` (local) → `22618` global — see below |
| `KeyOffset` → key string | `qstsa5q1` |
| `TextOffset` → display string | `Siege on Harrogath` |

The ID table confirms the round trip: entry `2618` of `expansionstring.tbl`'s
ID table holds `444` — the exact bucket the hash walk just found. And the
CRC over the string-data region of this file (`0xCB2F` at header offset
`0x00`) matches a from-scratch CRC-16 (polynomial `0x1021`, initial value
`0xFFFF`, table-driven — the 256-entry table sits at `6fc0fbb0` and is the
standard CRC-CCITT table byte for byte) computed over exactly the `[X, S)`
range this chapter derived above. The file is internally consistent, and so
is the walk that reads it.

Repeating the same lookup for the rest of Act V's quests lands on exactly
the text this book's [save-format chapter](d2s-save-format.md#8-quests)
already cites independently, resolving the one error its own source
material flagged and never fixed:

| slot | key | display string |
|---|---|---|
| 1 | `qstsa5q1` | Siege on Harrogath |
| 2 | `qstsa5q2` | Rescue on Mount Arreat |
| 3 | `qstsa5q3` | Prison of Ice |
| 4 | `qstsa5q4` | Betrayal of Harrogath |
| 5 | `qstsa5q5` | Rite of Passage |
| 6 | `qstsa5q6` | Eve of Destruction |

### What `P` is actually for

The header's fourth dword looked, at first pass, like a mystery — it's
close to `H` in each file but never equal to it, and nothing about "probe
limit" explained the specific number. Computing the *real* probe distance
for every one of `expansionstring.tbl`'s 2,818 active keys — the distance
from `hash(key) % N` to wherever that key actually landed — settled it
exactly: **the longest probe chain in the file is 2,453 steps, for the key
`MercX149`, and `P` is `2453`.** The same equality holds in the other two
tables: `string.tbl`'s longest chain is `5,192` steps (for the key `x`,
used by dozens of ID-only entries — see below) and its `P` is `5192`.
`patchstring.tbl`'s longest chain is `1,058` (`Hellfire Torch`) and its `P`
is `1058`. All three match to the probe.

`P` isn't a fixed formula — it's a number the offline table compiler
computed by actually building the hash table and measuring the worst
collision it produced, then baking that exact value into the header so the
runtime lookup can give up on a genuine miss in bounded time instead of
scanning every slot. It's a compile-time fact about *this specific file's*
key set, not a property of the format.

### The one field this chapter couldn't verify

The one node field this chapter could not pin down is the dword at offset
`+0x03`. It isn't a cached copy of the hash (recomputing the ELF hash for
every active key in `string.tbl` and comparing it against this field
disagrees for the overwhelming majority of entries), and it isn't
consistently a copy of the node's own array index or its `StringID` either
— across `string.tbl`'s 5,391 active nodes it matches the node's own array
position in 2,323 of them (43%) and matches neither obvious candidate in
the remaining 3,068. No function in `D2Lang.dll` reads this field at
runtime that this chapter could find. It's marked unverified rather than
guessed at — see the companion report for the exact numbers.

## Numeric IDs, a second front door

Every entry also carries a small integer identity, and a second, older
lookup path uses it instead of a key string. `GetStringByID`
(`FUN_6fc09450 @ 6fc09450`) takes a plain 16-bit ID and, depending on its
range, routes to one of the three tables:

- **`0`–`9999`** → `string.tbl`, ID used as-is.
- **`10000`–`19999`** → `patchstring.tbl`, ID minus `10000`.
- **`20000` and up** → `expansionstring.tbl`, ID minus `20000`.

The subtraction is done with 16-bit wraparound arithmetic rather than a
plain subtract — `id + 0xD8F0` for the patch range, `id + 0xB1E0` for the
expansion range (`65536 − 10000 = 0xD8F0`, `65536 − 20000 = 0xB1E0`) — which
is exactly what the disassembly shows and exactly equivalent to `id −
10000` / `id − 20000` once the 16-bit result wraps. `qstsa5q1`'s `StringID`
of `2618`, read earlier straight from its node, is a **local** ID — its
*global* ID, the one `GetStringByID` would need, is `20000 + 2618 = 22618`.

This path isn't a historical leftover nobody calls. Immediately after all
three tables finish loading, `D2Lang.dll`'s own startup code
(`FUN_6fc09590 @ 6fc09590`) uses it to fetch four single characters —
string IDs `5328`–`5331` — for the large-number abbreviation suffixes the
UI uses (a gold pile past a million reads `1.2M`, not `1200000`). Reading
those four IDs directly out of `string.tbl` gives exactly `K`, `M`, `B`,
`T`, matching the hard-coded fallback characters the same routine uses if a
localized table doesn't supply a single-character string at those IDs. Each
of those four nodes' key string is the literal placeholder `x` — they're
never meant to be found by key, only by number.

That placeholder key turns out to be common: `string.tbl` alone has 277
active nodes whose key string is exactly `x`. Text that a UI element only
ever requests by numeric ID doesn't need a real key, and the table compiler
evidently didn't bother inventing one.

## Precedence: when a key lives in more than one table

`GetStringByKey` (`FUN_6fc0a7b0 @ 6fc0a7b0`) is what a key string actually
goes through, and its structure is the whole precedence rule in one
function: it calls the per-table lookup three times, in a fixed order, and
returns on the first hit.

```
FUN_6fc09360(patchstring.tbl, key, ...)       ; tried first
  hit  → return result + 10000
  miss ↓
FUN_6fc09360(expansionstring.tbl, key, ...)   ; tried second
  hit  → return result + 20000
  miss ↓
FUN_6fc09360(string.tbl, key, ...)            ; tried last
  hit  → return result
  miss → FUN_6fc09fb0(key)                    ; see "On a miss", below
```

The `+ 10000` / `+ 20000` additions are literal `ADD EAX,0x2710` and
`ADD EAX,0x4e20` at `6fc0a841` and `6fc0a869` — the exact same offsets
`GetStringByID` subtracts back out, confirming the two lookup paths (by key,
by number) land on the same global ID space.

**`patchstring.tbl` before `expansionstring.tbl` before `string.tbl`** isn't
a theoretical ordering — it's directly observable. 87 keys exist as active
entries in *both* `string.tbl` and `patchstring.tbl`, and for 56 of them the
text genuinely differs between the two files — the precedence rule is what
decides which one a player ever sees. Two examples, read straight out of
the files:

| key | `string.tbl` | `patchstring.tbl` | what wins |
|---|---|---|---|
| `9bl` | `Stilleto` | `Stiletto` | `Stiletto` — a shipped typo, silently fixed by a later patch |
| `AmaOnly` | *(eleven spaces)* | `(Amazon Only)` | `(Amazon Only)` — the class-restriction line every Amazon-only item shows |

`AmaOnly` is the sharper example: `string.tbl`'s copy isn't wrong, it's
*blank* — eleven literal space characters, presumably a layout
placeholder from before the restriction text existed. Without
`patchstring.tbl` taking precedence, every class-restricted item in the
game would show an empty line instead of `(Amazon Only)`. The precedence
rule isn't cosmetic; it's load-bearing for text that shipped broken and was
only ever fixed by a file the base table doesn't know exists.

## On a miss

A key that isn't in any of the three tables doesn't crash the client — and
that's a real design choice, not a given. The
[excel tables chapter](excel-tables-and-data-loading.md#runtime-lookup-and-bounds-checking)
found that an out-of-range index into `Objects.txt` calls
`CleanupAndAbort` and ends the process outright. A missing *string* key
gets the opposite treatment.

`FUN_6fc09fb0 @ 6fc09fb0` is what `GetStringByKey` falls through to after
all three tables miss. It keeps a small linked list of every key that's
ever missed (rooted at a static pointer, `DAT_6fc10a60`), so the same
missing key is never rebuilt twice, and it constructs a fallback display
string that incorporates the key itself together with a literal, hard-coded
marker string sitting in `D2Lang.dll`'s data section at `0x6fc10568`:

```
" -not xlated call ken w"
```

That's the exact byte content at that address, confirmed by reading it
directly — a debug-era note to a developer (almost certainly a "Ken W.")
that a string is missing its translation, left in the shipped retail
binary. The precise final concatenation the routine assembles around it —
the decompiled form of this function is dense, register-reused string-copy
code that this chapter could not cleanly re-derive step by step — is the
one loose end left in the miss path; what's certain, from the string's raw
bytes and from the code path that reaches it, is that a missing key
produces *this* text embedded in it rather than a crash. See the companion
report.

## Why the indirection matters: a Levels.txt example

`Levels.txt`'s `LevelName` column (not the `Name` column, which is an
internal level identifier the string tables never see) is exactly this
kind of key — and Act V is where the indirection stops being a formality
and starts actively correcting the shipped game. Five of `Levels.txt`'s
Act V `LevelName` keys are pre-release working titles that never made it to
the final localized text, and reading the raw table instead of resolving
the key through `expansionstring.tbl` gives a plausible-looking, wrong
answer every time:

| `Levels.txt` `LevelName` key | resolves to |
|---|---|
| `Rigid Highlands` | Frigid Highlands |
| `Crystalized Cavern Level 1` | Crystalline Passage |
| `Crystalized Cavern Level 2` | Glacial Trail |
| `Halls of Death's Calling` | Halls of Pain |
| `Glacial Caves Level 1` | The Ancients' Way |

This book's [save-format chapter](d2s-save-format.md#9-waypoints) needed
exactly these five corrections to fill in the Act V waypoint names its
source material had marked as unresolved dots, and every one of them
matches what this chapter's own from-scratch lookup returns independently.
The two chapters didn't share code to get there — one reads a `.d2s` byte
layout and cross-references `Levels.txt`, the other reads `D2Lang.dll`'s
hash table directly — and they land on the same five strings. `Harrogath`
and `Arreat Plateau`, by contrast, are keys that already match their own
display text — not every Act V key needed correcting, which is exactly why
skimming `Levels.txt` for "reasonable-looking" names is a trap: some of the
plausible ones are already right, and that's what makes the five that
aren't easy to miss.

## What a modder changes

Because the file *is* a compiled hash table — header fields, ID table, and
node array all derived from the exact key set the compiler was given —
there's no such thing as editing one string in place. Changing a display
string, adding a new key, or overriding an existing one all require
regenerating the whole file: a new `N`, a new hash table, a new `P` sized to
whatever the new worst-case collision turns out to be, and a new CRC over
the new string-data region. A hand-patched byte inside an existing record
works only if the new text is no longer than the old (the `TextLength`
field and the trailing NUL still have to agree with reality); anything else
needs a real table compiler.

The practical lever is the same one [Excel tables and data
loading](excel-tables-and-data-loading.md#mpq-load-order) describes for
`.txt` tables: MPQ load order. A mod's patch archive that sits ahead of
`Patch_D2.mpq`, `D2Exp.mpq`, and `D2Data.mpq` in the search chain and
carries its own `data\local\lng\eng\patchstring.tbl` overrides the shipped
one exactly the way `AmaOnly` is overridden above — same precedence
mechanism, same three-table order, just with the modder's file standing in
`patchstring.tbl`'s slot instead of Blizzard's. Nothing about `string.tbl`
or `expansionstring.tbl` needs to change for that override to take effect,
because the lookup checks `patchstring.tbl` first regardless of what put it
there.

## Reference tables

### Header fields (all three tables, offset `0x00`–`0x14`)

| offset | size | name | meaning |
|---|---|---|---|
| `0x00` | word | CRC | CRC-16 (poly `0x1021`, init `0xFFFF`) over `[X, S)` |
| `0x02` | word | H | ID-table entry count |
| `0x04` | dword | N | node-array slot count; hash modulus |
| `0x08` | byte | *(unverified)* | observed `1` in every sample |
| `0x09` | dword | X | string-data start offset |
| `0x0D` | dword | P | probe-count ceiling (compiler-measured worst case) |
| `0x11` | dword | S | total file size |

### Node fields (17 bytes / `0x11`, repeated N times from `0x15+2H`)

| offset | size | name | meaning |
|---|---|---|---|
| `+0x00` | byte | Active | `1` = occupied, else empty |
| `+0x01` | word | StringID | local numeric ID (paired with the ID table) |
| `+0x03` | dword | *(unverified)* | see [above](#the-one-field-this-chapter-couldnt-verify) |
| `+0x07` | dword | KeyOffset | file offset of key string |
| `+0x0B` | dword | TextOffset | file offset of display string |
| `+0x0F` | word | TextLength | display-string byte length, NUL included |

### Table statistics — vanilla 1.13c

Every count below is exhaustive — every active node in every file, not a
sample. `H`, `N`, and `P` are read straight from each header; active/total
and duplicate-key counts come from walking every one of the N node records.

| table | bytes | H | N | active | duplicate-key groups | P |
|---|---|---|---|---|---|---|
| `string.tbl` | 390,570 | 5,391 | 5,393 | 5,391 | 11 (277 share the key `x`) | 5,192 |
| `patchstring.tbl` | 52,523 | 1,169 | 1,171 | 1,169 | 4 (104 share the key `x`) | 1,058 |
| `expansionstring.tbl` | 173,234 | 2,818 | 2,819 | 2,818 | 10 (16 share the key `x`) | 2,453 |

A duplicate key is reachable by numeric ID for every entry that shares it,
but by key-string lookup only for whichever one the hash-and-probe walk
reaches first — every other owner of that key is permanently shadowed for
key-based lookup. This chapter found no case where that shadowing hid a
key a player would ever type or a table would ever reference; every
inspected duplicate was either the ID-only placeholder `x`/`X`, or a
genuine second entry never reached by key in practice.

### `D2Lang.dll` 1.13c function map

| address | function | role |
|---|---|---|
| `6fc0a130` | `FUN_6fc0a130` | top-level loader — reads all three files at startup |
| `6fc094e0` | `FUN_6fc094e0` | single-file loader/validator — CRC and node bounds checks |
| `6fc092f0` | `FUN_6fc092f0` | CRC-16 checksum routine |
| `6fc08fe0` | `FUN_6fc08fe0` | ELF/PJW string hash |
| `6fc09360` | `FUN_6fc09360` | per-table key lookup — hash + linear probe |
| `6fc0a7b0` | `FUN_6fc0a7b0` | `GetStringByKey` — three-table precedence dispatcher |
| `6fc09450` | `FUN_6fc09450` | `GetStringByID` — three-table numeric dispatcher |
| `6fc09050` | `FUN_6fc09050` | per-table numeric-ID lookup via the ID table |
| `6fc09fb0` | `FUN_6fc09fb0` | untranslated-key fallback / miss cache |
| `6fc09590` | `FUN_6fc09590` | post-load step — fetches K/M/B/T number-abbreviation characters |
| `6fc09e20` | `FUN_6fc09e20` | table unload / refcount release |
| `6fc0fbb0` | `DAT_6fc0fbb0` | CRC-16 lookup table (256 × word, standard poly-`0x1021` table) |
| `6fc10568` | `DAT_6fc10568` | fallback marker string, `" -not xlated call ken w"` |

## Version differences

| what | 1.13c | 1.09d |
|---|---|---|
| header/node/ID-table layout | (this chapter) | identical — same header shape, hash function, CRC |
| `string.tbl` | 390,570 bytes, 5,391 active keys | byte-identical (matching SHA-256) |
| `expansionstring.tbl` | 173,234 bytes, 2,818 active keys | byte-identical (matching SHA-256) |
| `patchstring.tbl` | 52,523 bytes, 1,169 active keys | 41,090 bytes, 1,006 active keys |
| `patchstring.tbl` archive/flags | `Patch_D2.mpq`, `IMPLODE` | `Patch_D2.mpq`, `IMPLODE\|ENCRYPTED` |

> **Version note (1.09d):** the `.tbl` binary format itself has not changed
> at all — a from-scratch parse of 1.09d's three tables round-trips its CRC
> exactly the same way 1.13c's do. Only content changed, and only in
> `patchstring.tbl`: 163 fewer active entries than 1.13c, consistent with
> several patches' worth of new unique items, rune words, and text fixes
> landing in the one table built to absorb them. `string.tbl` and
> `expansionstring.tbl` did not change a single byte between the two
> versions.

---

Companion report: [tbl-string-tables.verification.md](tbl-string-tables.verification.md).
