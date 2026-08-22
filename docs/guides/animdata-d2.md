# AnimData.d2: Animation Timing

> **Provenance.** Verified against the retail 1.13c binaries in Ghidra on 2026-08-21:
> `D2Common.dll` (`/Vanilla/1.13c/D2Common.dll`, image base `6fd50000`), cross-checked
> against `/Vanilla/1.09d/D2Common.dll` (image base `6fd40000`). The data file itself —
> `data\global\AnimData.d2` — was read from the real 1.13c and 1.09d MPQ chains with
> `tools/d2mpq.py` (never bare `mpyq` — see [vanilla-data.md](../vanilla-data.md)) and
> parsed structurally in full: every one of its 3,558 records, not a sample. Every
> address and byte offset below was confirmed in the **disassembly**, not the
> decompiler's C — this project's decompiler has been shown to mislabel functions in
> this exact corner of D2Common (see the companion report). A full claim-by-claim audit,
> including two corrections to the COF-pipeline chapter's function table, is in
> [animdata-d2.verification.md](animdata-d2.verification.md).

## The Table Behind the Motion

The [COF pipeline](cof-pipeline-1.13c.md) chapter follows `AMTNHTH.cof` — the Amazon,
in town, empty-handed — from a filename to a picture. It stacks her layers, loads her
sprites, decodes her cels, and blits her to the screen. Nowhere in that chain does
anything say how *fast* she moves. A COF is a parts list and a stacking order; it has
no clock. The sprites underneath her are worse than silent on the question — a DCC is a
grid of pre-drawn cels, one image per direction per frame, and an image does not know
how long it should stay on screen.

That question — how many frames this animation has, how quickly they advance, and
which frame is the one where a sound plays or an arrow leaves the bow — is answered by
a single 570,304-byte file: `data\global\AnimData.d2`. It is loaded once, at startup,
into a hash table of 3,558 records, and from then on every animated thing in the game —
players, monsters, objects, missiles — reads its timing out of it on every tick. The
same Amazon who obtained her *parts* from `AMTNHTH.cof` obtains her *pace* from the
record named, not coincidentally, `AMTNHTH` — same eight characters, no path, no
extension, sitting in a different file, resolved by a different mechanism, but built
from the same three tokens. This chapter follows that record: where it sits in the
file, how a name turns into a bucket, what its bytes mean, and what happens after
`AnimSpeed` leaves the table and becomes a number of pixels a frame advances by,
tonight, on your screen.

The reason this is a *table* at all, rather than a number baked into the sprite or the
COF, is the same reason COF exists: separation buys reuse. `AnimSpeed` is what
"increased attack speed" gear multiplies. If timing lived inside 20,000 individual DCC
files, tuning combat feel would mean re-authoring art. Because it lives in one table
keyed by a name, a modder — or Blizzard, patch to patch — changes combat pacing by
editing rows of numbers, and every sprite that shares that token+mode+weaponclass moves
differently the next time the game starts.

## Anatomy of the File

Open `AnimData.d2` in a hex editor and there is no header to speak of — no magic
number, no version field, nothing that announces what you're looking at. The file
*is* the index. `DATATBLS_LoadAnimDataTable`, at `D2Common 6fd91e50`, builds the
in-memory path with `wsprintfA("%s\AnimData.d2", "DATA\GLOBAL")` — a literal
confirmed by reading the format string at `6fde34c4` byte-for-byte — and hands the raw
bytes it gets back straight to a walk that assumes a fixed shape: **256 hash buckets,
back to back, each a four-byte record count followed by that many 160-byte records.**
Nothing else. No bucket, no padding, no trailer.

That shape is not folklore — it falls straight out of the loader's own pointer
arithmetic, present in the disassembly independent of anything the decompiler asserts:

```
LEA ECX,[ECX + ECX*0x4]     ; ECX = count * 5
SHL ECX,0x5                 ; ECX = count * 5 * 32 = count * 0xA0
...
LEA EAX,[EAX + ECX*0x1 + 0x4]   ; next bucket = this bucket + count*0xA0 + 4
```

Each bucket is `4 + count × 0xA0` bytes, and the loop that walks all 256 of them is the
entire parse. Parsing the real file the same way and summing the result proves the
shape is complete, not merely plausible: **256 buckets whose counts sum to 3,558
records, at 4 + 3,558 × 160 = 570,304 bytes — exactly the file's size, with zero bytes
left over.** That arithmetic identity, checked against the actual 1.13c archive rather
than assumed, is the strongest evidence in this chapter: if the record stride or the
bucket count were wrong by even one byte, the walk would not land exactly on the file's
last byte.

| Field | Size | Notes |
|---|---|---|
| Bucket count | 256 (`0x100`) | Fixed; `MOV EDI,0x100` in the loader |
| Per-bucket header | 4 bytes | Record count for this bucket (can be 0) |
| Per-bucket records | count × 0xA0 bytes | Contiguous, no padding |
| **Total, 1.13c** | 570,304 bytes | 256 empty-or-not buckets, 3,558 records total |

Of the 256 buckets, **120 are empty** and the busiest holds **67 records**; the average
occupied bucket carries about 26. That spread is not incidental — it is the direct
consequence of the hash function in the next section, and it is what keeps a lookup
fast: instead of a linear scan over 3,558 records, the game scans at most a few dozen.

### The record itself

Every record is exactly `0xA0` (160) bytes, and the loader's own arithmetic
(`count*5<<5`) is only one confirmation of that stride — a second, independent one
comes from the accessor that actually reads a record's fields (`GetAnimDataFrameInfo`,
covered below), whose field offsets land at `0x08`, `0x0C`, and `0x10`, and whose
per-frame scan is explicitly bounded to `0x90` (144) entries. `0x10 + 0x90 = 0xA0` —
the same 160 bytes, arrived at from the opposite direction.

| Offset | Size | Field | What it holds |
|---|---|---|---|
| `0x00` | 8 bytes | Name | Token+mode(+weaponclass), ASCII, NUL-padded, always uppercase on disk |
| `0x08` | 4 bytes | FramesPerDir | Frame count for one direction of this animation |
| `0x0C` | 4 bytes | AnimSpeed | Base playback rate, consumed as a fixed-point multiplicand |
| `0x10` | 144 bytes | Frame events | One byte per frame (indices 0–143); nonzero = an event fires on that frame |

Here is `AMTNHTH` in full, read straight out of the vanilla archive at file offset
`0x2d2d4`:

```
41 4d 54 4e 48 54 48 00  10 00 00 00  50 00 00 00  00 00 00 00 00 00 00 00 ... (144 zero bytes)
'A''M''T''N''H''T''H'\0  frames=16    speed=80     no frame carries an event
```

Sixteen frames, an `AnimSpeed` of 80, and not one of the 144 event bytes set — an
unarmed town idle has nothing to announce. Compare it to `AMA1BOW`, the Amazon's first
attack sequence with a bow drawn, at offset `0x6b424`: 14 frames, `AnimSpeed` 256, and
byte index 6 of the event array set to `2`. Frame 6 of that swing is where something is
supposed to happen — and what "2" means, concretely, is the subject of a later section.

## From a Name to a Bucket: the Hash

A lookup starts with a name — `AMTNHTH`, `AMA1BOW`, whatever token+mode(+weaponclass)
the caller wants timing for — and has to land on one of 256 buckets before it can
compare anything. The function that does this, at `D2Common 6fd91dd0`, is labeled
`LOG_FindLogEntryByTag` in this Ghidra project and decompiles to a comment about
"searching a log by tag." Neither is true. It is the AnimData hash-and-scan, and the
label is a casualty of the same project-wide issue documented in the COF-pipeline
chapter: a blanked source-filename pointer at `0x6fdda728`, shared across hundreds of
call sites, that has driven the auto-documentation tooling to paste one function's
boilerplate onto another's body. The disassembly does not lie the way the comment does.

Read straight from the instructions, the algorithm is:

1. **Uppercase the name in place** — for each byte, if it falls in `'a'`–`'z'`, subtract
   `0x20`.
2. **Sum every byte of the uppercased name**, keeping only the low 8 bits
   (`AND EAX,0xff` — an implicit mod 256, not a documented step but a visible
   instruction).
3. **Index the bucket-pointer table** at `[table_base + 4 + hash*4]` — the table built
   by the loader, one pointer per bucket, hash `0`–`255`.
4. **Walk that bucket linearly**, comparing the caller's name against each record's
   8-byte name field (via a second function, `6fd91d00`, misnamed `LOG_CompareLogTags`)
   until one matches or the bucket runs out.

For `AMTNHTH`: `A+M+T+N+H+T+H = 65+77+84+78+72+84+72 = 532`, and `532 mod 256 = 20`.
Bucket 20 is where the record lives — and it is not a light bucket. It holds 48
records, and `AMTNHTH` is the **46th** name the scan reaches, after 45 non-matches with
names as unrelated as `Z3NUHTH`, `OBOPHTH`, and `PASCSTF` sharing the same hash by pure
coincidence of letter sums. The lookup is not instant; it is real, bounded work, and
this chapter's worked example spends 45 failed comparisons before it succeeds — a fact
invisible from the file alone, visible only by actually running the bucket walk against
the real data.

This was checked across the whole file, not just this one name: **every one of the
3,558 records hashes, via this exact uppercase-and-sum rule, to the bucket it is
physically stored in.** Zero exceptions. The compare function additionally enforces
that both the stored name and the caller's query are 8 characters or fewer, aborting
with distinct internal error codes (`0xd6` for the query, `0xd7` for the stored record)
if either is longer — consistent with every one of the 3,558 stored names being 8
characters or fewer, confirmed by the same full-file parse.

## One Name-Builder, Two Destinations

Where does the query string come from? The COF-pipeline chapter's Stage 1 already
names the function: `BuildCofPathString`, `D2Common 6fd93c30`, credited there with
"formats the COF path using `%s\%s\COF\%s%s%s.COF`" — the same format string, cached
separately as a D2Client-side string literal at `6fb84ec0` for that chapter's own
purposes. `ANIM_LookupAnimDataByPath` (`D2Common 6fd91f70`, mislabeled
`BinkBufferGetError` by the decompiler — its own disassembly shows it calling
`6fd93c30` and then the hash walk above) reaches the very same function.

What the COF chapter's one-line description does not capture — because it only needed
`BuildCofPathString`'s path-building behavior — is that `6fd93c30` is not
single-purpose. It is a dispatcher, keyed on a caller-supplied flag, that assembles the
same three tokens (character/monster/object class, mode, weapon or sub-type) and then
chooses between **two** `wsprintfA` format strings read directly out of the binary:

| Address | Format string | Used for |
|---|---|---|
| `6fde3428` | `%s%s%s` | The bare AnimData.d2 lookup key — no separators, no extension |
| `6fde3430` | `%s\%s\COF\%s%s%s.COF` | The full COF file path |

Both branches build from the *same* three pieces before the format is applied — the
class-prefix string (`6fde33fc`, `"DATA\GLOBAL\OBJECTS"`, or `6fde3410`,
`"DATA\GLOBAL\MONSTERS"`, depending on unit type), the mode, and the weapon/sub-type
token — with trailing spaces stripped from each piece byte-by-byte before concatenation
(the block of `CMP ... ,0x20 / SETZ / DEC / AND` instructions repeated for every
character position). That guarantees something a modder should be able to rely on:
**the AnimData key for a given unit-in-mode is, by construction, the same three
characters-worth of tokens that make up its `.COF` filename**, just concatenated
without the slashes and the extension. There is no separate naming scheme to keep in
sync — one code path derives both strings from the same inputs.

## What the Record Drives

A resolved record is not read once and discarded — it is cached. `GetDataTableRecord0x70`,
`D2Common 6fd82670`, calls `ANIM_LookupAnimDataByPath` and stores the returned pointer
directly into the unit structure at offset `0x50` (`pUnit[0x14]`). Everything downstream
reads that cached pointer rather than re-hashing the name every frame.

**Frame events.** `SetAnimEventFromFrameData`, `D2Common 6fd7ee10`, takes a unit and a
frame index, dereferences the cached record pointer at `unit+0x50`, and reads the byte
at `0x10 + frameIndex` within it — exactly the event array this chapter's record
layout table describes — and, if that byte is `1`, `2`, `3`, or `4`, writes the value
into `unit+0x4E`, the field the rest of the engine reads to know "something is due
this frame." That is the mechanism the COF chapter names
generically as "a footstep sound, the instant a missile should launch, the point in a
swing where damage lands" without enumerating the codes; this is where those codes
actually live.

The real file settles what those codes are used for empirically, if not by name. Across
all 3,558 records, 568 carry at least one nonzero event byte (585 nonzero bytes in
total — a handful of records carry more than one), and the values that appear are
**not evenly spread**:

| Code | Byte instances (across the whole file) |
|---|---|
| 1 | 433 |
| 2 | 148 |
| 3 | 4 |
| 4 | 0 |

Code `4` is checked for in the consuming code but never appears in vanilla 1.13c data —
whatever it was for, nothing in the retail table uses it. Code `3` is nearly as rare,
appearing on only four bytes total. The split between `1` and `2` lines up cleanly with
weapon class on the Amazon's own attack animations: her melee-class first attacks —
`AMA1HTH`, `AMA1STF`, `AMA11HS`, `AMA11HT`, `AMA12HS`, `AMA12HT` — all carry code `1`;
her ranged-class first attacks — `AMA1BOW`, `AMA1XBW` — both carry code `2`. That
pattern (confirmed by inspecting every `AM*` record in the file, not the two cited) is
strong circumstantial evidence that `1` is a generic strike/sound trigger and `2` is
specifically a missile-launch trigger, but no string table or symbol in either DLL
names the codes directly — this is inference from usage, not a recovered label, and it
is marked that way rather than asserted as fact.

**Playback rate.** `AnimSpeed` (record offset `0x0C`) is not the number of pixels a
frame advances by — it is a base rate that a separate function scales per-mode. That
function, `UNITS_SetGfxSelected`, `D2Common 6fd83110`, reads the cached record's
`AnimSpeed` field directly (`*(int*)(cachedRecord + 0xc)` in its own decompiled body,
matching the record layout established above) and multiplies it by a percentage derived
from one of three unit stats depending on the current mode — walk/run speed (stat id
`0x43`), cast speed (`0x45`), or attack/swing speed (`0x44`, averaged across both hands
for a dual-wielding class) — clamping the stat-derived percentage to a sane range before
dividing by 100. The result, a signed 16-bit rate, is written to `unit+0x4C`.

**Advancing the frame.** Every tick, `AdvanceAnimFrameWithWrap` (`D2Common 6fd7f060`)
and `AdvanceAnimSubAccumulator` (`6fd7f090`) each add that rate at `unit+0x4C` to a
fixed-point accumulator at `unit+0x44`, wrapping it back down when it reaches the
sequence's frame count times `0x100`. The `×256` boundary math is explicit in the
disassembly (`nEndFrame * 0x100`), which is the confirmation that this is 8.8
fixed-point arithmetic: 256 sub-units per whole frame. A rate of `256` at `unit+0x4C`
advances exactly one frame per tick; half that advances one frame every two ticks.

Put together, this is the whole path from the file to the screen:

```
AnimData.d2 record (AnimSpeed @ 0x0C)
        |
        v
UNITS_SetGfxSelected            -- scales AnimSpeed by a per-mode unit stat (0x43/0x44/0x45),
        |                          clamps, writes a 16-bit rate to unit+0x4C
        v
AdvanceAnimFrameWithWrap /       -- adds that rate to an 8.8 fixed-point
AdvanceAnimSubAccumulator           frame accumulator at unit+0x44, each tick
        |
        v
SetAnimEventFromFrameData        -- reads the resolved frame's event byte
                                     (cachedRecord + 0x10 + frame), fires it via unit+0x4E
```

This is the direct answer to what a modder changes for attack speed: **raising a
record's `AnimSpeed` raises the per-tick rate proportionally**, for every unit that
resolves to that token+mode+weaponclass, independent of the "Increased Attack Speed"
stat — the stat scales the same base number, it does not replace it. A modder who wants
Amazon bow attacks to animate faster edits `AMA1BOW`'s `AnimSpeed` (`256` in retail);
one who wants a *longer* swing without changing its speed edits `FramesPerDir` instead,
which changes where the `×256` wrap boundary sits without touching the rate at all.

## When the Lookup Misses

Not every query succeeds. `ANIM_LookupAnimDataByPath` returns a pointer into the
zeroed, otherwise-unused tail of the loader's own in-memory index — offset `0x404` in
the `0x4A4`-byte table structure the loader allocates (`0x129` dwords: a base pointer,
256 bucket pointers, two constants at `0x40C`/`0x410`, and a handful of dwords past
that which are zeroed but never separately written). A miss there resolves to an
all-zero record: no name, no frames, no speed, no events — silent, not a crash.

`GetAnimDataFrameInfo`, `D2Common 6fd91ef0` — the accessor that actually surfaces
`FramesPerDir`, `AnimSpeed`, and the first event-frame index to callers — has its own,
separate fallback for the same failure: instead of the zeroed record, it substitutes
the two constants the loader wrote at table offsets `0x40C` and `0x410` (`0x800` and
`0x100`) directly into its `FramesPerDir`/`AnimSpeed` outputs. Those two values are
loader bookkeeping, not real animation data — `0x100` is the bucket count, reused here
opportunistically — so a caller that hits this path gets numbers that happen to be
non-zero rather than numbers that mean anything. Both fallback paths were confirmed in
the disassembly; **why the two consumers disagree about what "not found" should return
is not explained anywhere in the code and is left open** rather than guessed at.

## Quirks in the Real Table

Three thousand five hundred fifty-eight records carry only 3,529 distinct names — 29
names are stored twice. Twenty of those 29 pairs are byte-for-byte identical, harmless
redundancy: `AITNBOW` — a town-mode, bow-class stance for whichever unit owns the `AI`
token (not independently identified here; see the verification report) — sits in the
very same bucket 20 as `AMTNHTH`, one slot after it. It is stored twice, at `0x2d374`
and `0x2d414`, 160 bytes apart, and the two records match byte for byte. Because the
lookup takes the first match a bucket's linear scan reaches, the second copy is
permanently unreachable — dead weight the loader still allocates and zeroes, but
nothing ever reads.

Nine of the 29 duplicate names are not harmless. They hold **genuinely different**
timing under the same key. `MINUHTH` has one stored definition with `FramesPerDir=1`
and a second, unreachable definition later in the same bucket, with `FramesPerDir=8`
(the token's owning monster or object was not independently identified — see the
verification report). `VMS1HTH` has two entries differing only in `AnimSpeed` (200 vs.
160). Whichever the file's authors
intended, only the record the linear scan reaches first is the one the retail game has
ever actually used — the other has been sitting in every copy of this file since at
least 1.09d, doing nothing.

| Name | First entry (used) | Second entry (dead) |
|---|---|---|
| `MINUHTH` | 1 frame, speed 256 | 8 frames, speed 256 |
| `VMS1HTH` | 17 frames, speed 200, event @6=2 | 17 frames, speed 160, event @6=2 |
| `64A1HTH` | 27 frames, speed 208, event @16=2 | 27 frames, speed 200, event @13=2 |
| `3DNUHTH` | 1 frame, speed 256 | 16 frames, speed 176 |

A separate, smaller quirk: one record, `42DTHTH`, declares `FramesPerDir = 200` — more
frames than the 144-byte event array has slots for. `GetAnimDataFrameInfo`'s own
per-frame scan is hard-clamped to the first `0x90` (144) entries regardless of what
`FramesPerDir` says, so frames 144–199 of that sequence can never carry a table-driven
event no matter what a modder writes into bytes that do not exist.

## Version differences

Every claim above was checked against 1.13c. It also holds, unchanged, for 1.09d:

| What | 1.13c | 1.09d |
|---|---|---|
| `AnimData.d2` file size | 570,304 bytes | 570,304 bytes |
| `AnimData.d2` SHA-256 | `36cb704b85a7a478…` | `36cb704b85a7a478…` (identical) |
| Loader address | `D2Common 6fd91e50` | `D2Common 6fd45120` |
| Loader algorithm | `0x129`-dword index, `0xA0` stride, 256 buckets, `0x800`/`0x100` constants | Instruction-for-instruction identical |

The file is byte-identical between the two versions — same archive contents, same
record data, same quirks — and the loading code in `D2Common.dll` disassembles to the
same instructions at both addresses, differing only in link-time placement. No
functional difference was found. `EAnimData.d2` — a same-named-pattern file present
alongside `AnimData.d2` in both versions' `D2Exp.mpq`/`D2Data.mpq` — was checked for:
no reference to that literal filename exists anywhere in either version's
`D2Common.dll`, so its purpose is unverified and out of scope here rather than guessed
at.

## Reference: Functions

| Function | Address (1.13c) | Role |
|---|---|---|
| `DATATBLS_LoadAnimDataTable` | `6fd91e50` | Loads `AnimData.d2`, builds the 256-bucket pointer index |
| `ANIM_LookupAnimDataByPath` | `6fd91f70` | Builds the query token, invokes the hash walk (Ghidra: `BinkBufferGetError` — mislabeled) |
| Hash-and-scan (unnamed in this chapter's prose) | `6fd91dd0` | Uppercases, sums, indexes the bucket, linear-scans it (Ghidra: `LOG_FindLogEntryByTag` — mislabeled) |
| Name compare | `6fd91d00` | 8-byte name compare with length-abort guards (Ghidra: `LOG_CompareLogTags` — mislabeled) |
| `BuildCofPathString` | `6fd93c30` | Shared 3-token formatter (named by the COF chapter for its path-building branch only); flag-selects bare-token vs. full-`.COF`-path output |
| `GetAnimDataFrameInfo` | `6fd91ef0` | Real accessor: frames, speed, first event-frame index (3 of the 4 addresses the COF chapter lists under this name are unrelated functions — see the verification report) |
| `GetDataTableRecord0x70` | `6fd82670` | Resolves and caches a unit's AnimData record pointer at `unit+0x50` |
| `SetAnimEventFromFrameData` | `6fd7ee10` | Reads the current frame's event byte, writes it to `unit+0x4E` |
| `UNITS_SetGfxSelected` | `6fd83110` | Scales `AnimSpeed` by a per-mode unit stat, writes the tick rate to `unit+0x4C` |
| `AdvanceAnimFrameWithWrap` | `6fd7f060` | Adds the tick rate to the 8.8 fixed-point frame accumulator, wraps at sequence end |
| `AdvanceAnimSubAccumulator` | `6fd7f090` | Same accumulator step for a secondary (sub-)counter |
| `ANIMATE_FreeAllAnimateTables` | `6fd68f90` | Frees animation-table allocations at shutdown (see verification report — its claimed table list does not match the confirmed AnimData global) |

## Reference: Record Layout

| Offset | Size | Field |
|---|---|---|
| `0x00` | 8 | Name (token+mode[+weaponclass], uppercase ASCII, NUL-padded) |
| `0x08` | 4 | FramesPerDir |
| `0x0C` | 4 | AnimSpeed |
| `0x10` | 144 | Per-frame event byte array (index = frame number, 0–143) |

Stride: `0xA0` (160) bytes, confirmed independently from the loader's pointer
arithmetic and from the accessor's field offsets.

See [animdata-d2.verification.md](animdata-d2.verification.md) for the full claim
ledger: what was checked, how many records, and everything that remains unverified.
