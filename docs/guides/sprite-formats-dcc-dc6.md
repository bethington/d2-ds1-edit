# Sprite formats: DCC and DC6

**Provenance.** Verified against `D2CMP.dll` from vanilla Lord of Destruction
1.13c (163,840 bytes, SHA-256 `2ee205f484161c5ed854f30aed12565a7ac3e97fd1a838e697aaefe4dc349756`,
Ghidra program `/Vanilla/1.13c/D2CMP.dll`, image base `6fe10000`), and against
every `.dcc` and `.dc6` file in the vanilla archives `d2char.mpq`, `d2data.mpq`
and `d2exp.mpq` — 21,692 DCC files and 1,654 DC6 files, read through a
PKWARE-DCL-capable MPQ reader. Verified 2026-08-21. Full claim ledger, sampling
policy and open questions: [sprite-formats-dcc-dc6.verification.md](sprite-formats-dcc-dc6.verification.md).

This chapter is not a conversion of an existing document. The Siramy archive
under `docs/preservation/` contains no page on either format, so nothing here
is recovered prose. It does, however, rest partly on code with a known
lineage — see [What is independent here](#what-is-independent-here) before
weighing any single claim.

---

## The two halves of a picture

The [COF pipeline chapter](cof-pipeline-1.13c.md) follows one Amazon standing
in an Act I town from the moment her animation mode changes to the moment her
torso is drawn. It loads her sprite in Stage 4 and blits it in Stage 6, and in
between it says something quietly unsatisfying: the file is "DCC for the
compact, delta-compressed character animations, DC6 for simpler frames." That
is true, and it is the whole of what that chapter needs. It is also the point
at which a reader who wants to *change* the Amazon has to stop.

This chapter is the layer underneath. It describes what is actually in those
two files, byte by byte and bit by bit, and it follows the same Amazon —
`AMTRLITTN1HT.dcc`, 35,479 bytes, the light-armour torso of a town-idle
Amazon holding a one-handed weapon — from her file header to a decoded frame.
When she opens her inventory, the javelin in her hand shows up as a second
file in the other format, `invjav.dc6`, 1,276 bytes; that one is small enough
to walk through in full, and it does the job of explaining DC6 completely.

Two formats exist because Diablo II has two very different sprite problems.
One is a wardrobe problem: sixteen directions times sixteen frames of a torso,
in three armour weights, per weapon class, per character class, and again for
every monster in the game — 3.3 million frame headers across 21,692 files in
the vanilla archives, all of which must fit on a CD and into 1999-era memory.
The other is a signage problem: a static inventory icon, a button, a panel
background, drawn once at a fixed size and never animated. DCC solves the
first with an aggressive, stateful, bit-packed delta encoding that only makes
sense across a run of near-identical frames. DC6 solves the second by barely
compressing at all, which is the correct answer when there is no redundancy to
exploit and you would rather have the pixels immediately.

The clean way to hold the difference: **DC6 is a container for pixels that are
already in the form the blitter wants. DCC is an archive that has to be
unpacked into that form first.** Everything else follows from that.

---

## Which format the game asks for

The choice is not made by looking at what is on disk. It is made in code,
before any file is opened, by `BuildSpritePath`
(D2CMP 1.13c @ `6fe1c2d0`), which formats a path and an extension together
from the entity's type. The binary carries five format strings for it:

| Format string | Used for |
|---|---|
| `%s\%s\%s\%s.dcc` | characters, monsters, objects — token/mode/class path |
| `%s\%s\%s\%s.dc6` | the same three, when the entity is flagged DC6 |
| `%s\%s.dcc` | missiles and overlays |
| `%s\%s.dc6` | overlays, for one specific overlay id |
| `%s\items\%s.dc6` | inventory and "floor" item graphics |

and five directory roots, all of them literal in the DLL: `DATA\GLOBAL\CHARS`,
`DATA\GLOBAL\MONSTERS`, `DATA\GLOBAL\OBJECTS`, `DATA\GLOBAL\MISSILES`,
`DATA\GLOBAL\OVERLAYS`, plus a bare `DATA\GLOBAL` for the items path.

The extension is selected by a global flag that certain entities clear. In
`BuildSpritePath` the DCC branch is taken by default for characters, monsters
and objects, but the function forces the DC6 branch for a hard-coded list of
ids — object ids `0x156` and `0x233`, monster ids `0xF2`, `0xFB`, `0x16F`,
`0x209`, `0x2C0` unconditionally, and `0xF3`, `0x11C`, `0x14D`, `0x220`,
`0x22F`, `0x23A`, `0x2C1`, `0x2C5` when a secondary field is zero — and for one
literal token string, `"OYTRlitTNhth"`, compared case-insensitively. Items are
always DC6; missiles are always DCC.

Two consequences matter to anyone editing game data. First, **the extension is
not negotiable from the data side**: if the code asks for `.dcc` and you supply
a `.dc6` under that name, the DCC header check rejects it and the sprite fails
to load. Second, the DCC path sets a flag bit that later decides which of two
entirely separate header parsers runs — `LoadSpriteDefinition`
(@ `6fe1d350`) tests bit 0 of the sprite descriptor's `+0x44` field and either
reads a 24-byte DC6 header inline or calls the 15-byte DCC header reader at
`6fe23010`. There is no sniffing and no fallback.

> **Note on Ghidra naming:** the DCC header parser at `6fe23010` is labelled
> `LoadTileResourceData` in this project's database. The label is wrong; the
> function reads a DCC sprite header and has nothing to do with tiles. It is
> cited by address throughout this chapter for that reason.

---

## One descriptor, two files

Before either format, it helps to see where they both land. Both parsers fill
the *same* in-memory sprite descriptor, and the fields line up slot for slot:

| Descriptor offset | From DCC | From DC6 |
|---|---|---|
| `+0x04` | `version` (1 byte) — always 6 | `version` (u32) — always 6 |
| `+0x08` | the u32 at file offset 7 — always 1 | `flags` (u32) — always 1 |
| `+0x0C` | forced to zero | `encoding` (u32) — always 0 |
| `+0x14` | `directions` (1 byte) | `directions` (u32) |
| `+0x18` | `framesPerDirection` (u32) | `framesPerDirection` (u32) |
| `+0x1C…` | direction offsets, one u32 each | first-frame offset of each direction |

Evidence: `6fe23010` disassembly for the DCC column, `6fe1d350` disassembly for
the DC6 column; the field values are from all 21,692 DCC and all 1,654 DC6
files. Both parsers reject a file with more than 32 directions — `CMP CL,0x20`
at `6fe23065` and `CMP ECX,0x21` at `6fe1d503` respectively — and the DC6
parser additionally rejects `directions × framesPerDirection ≥ 0x4000` (16,384)
at `6fe1d4b8`.

The symmetry is not a coincidence. Both formats are serialisations of D2CMP's
internal *cel* structure, whose header is also 24 bytes with directions and
frame count in the same slots (`ConvertRawCelToLoaded` @ `6fe21d00`). DC6 is
close to that structure written straight to disk; DCC is the same structure
compressed. That is why the DCC file header carries a `finalDc6Size` field:
the format's own name for "how big this would be as a DC6."

---

## DC6: the format that is already pixels

DC6 has three levels and no bitstream anywhere. A 24-byte file header, a flat
table of 32-bit frame offsets, and then one self-contained record per frame.
Nothing is relative, nothing is delta-coded against anything else, and any
frame can be read without touching any other frame.

### The file header

Read as 24 bytes in one call at `6fe1d48e` (`PUSH 0x18`), then distributed to
the descriptor at `6fe1d4a4`–`6fe1d4ce`:

| Offset | Size | Field | Observed in 1,654/1,654 files |
|---|---|---|---|
| `0x00` | 4 | version | `6` |
| `0x04` | 4 | flags | `1` |
| `0x08` | 4 | encoding | `0` |
| `0x0C` | 4 | termination | **not read by the game** |
| `0x10` | 4 | directions | 1, 4, 8 or 16 |
| `0x14` | 4 | framesPerDirection | 1 … 1,499 |

The names for the first four are the community's, not Blizzard's; the game
stores the first three and never touches the fourth. That fourth dword is worth
a moment, because it is the first sign of something this chapter returns to.
Across the archives it takes exactly three values: `0xEEEEEEEE` in 1,193 files,
`0xCDCDCDCD` in 403, and `0` in 58. Those are not data. `0xCDCDCDCD` is the
Microsoft C runtime's debug-heap fill for freshly allocated memory; the field
is uninitialised in the tool that wrote the files, and the value records which
build of that tool ran. **Do not validate against it, and do not read meaning
into it.**

Immediately after the header comes the frame-offset table: `directions ×
framesPerDirection` little-endian u32 file offsets, direction-major, so frame
*f* of direction *d* is at index `d × framesPerDirection + f`. In every one of
the 1,654 files the table is followed immediately by the first frame, with no
padding.

> **Version note (all patches 1.07a–1.14d):** these six header values are
> unchanged. In the fifteen catalogued installs checked, `version` is 6,
> `flags` is 1 and `encoding` is 0 in every DC6 sampled.

### The frame record

Each frame is a 32-byte header, then `length` bytes of scanline data, then
three bytes of slack:

| Offset | Size | Field | Observed across 26,316 frames |
|---|---|---|---|
| `+0x00` | 4 | flip | `0` in 26,176; `1` in 140 |
| `+0x04` | 4 | width | 1 … 319 |
| `+0x08` | 4 | height | 1 … 256 |
| `+0x0C` | 4 | offsetX (signed) | −287 … 62 |
| `+0x10` | 4 | offsetY (signed) | −228 … 169 |
| `+0x14` | 4 | unknown | `0` in all 26,316 |
| `+0x18` | 4 | nextBlock | see below |
| `+0x1C` | 4 | length, in bytes | 2 … 66,560 |
| `+0x20` | *length* | scanline data | |
| | 3 | slack | `EE EE EE` / `00 00 00` / `CD CD CD` / junk |

The three trailing bytes are the second sign. They are commonly described as a
frame terminator; they are not. Their most common values across the corpus are
`EEEEEE` (19,242 frames), `000000` (4,077) and `CDCDCD` (2,748) — the same
uninitialised-memory patterns as the header's fourth dword — with a long tail
of other values. Nothing in D2CMP reads them.

They are, however, structurally real, and D2CMP explains why. The engine's
in-memory cel gives each frame a record stride of `0x23` (35) bytes of header
before its data, while writing only the first `0x20` (32)
(`ConvertRawCelToLoaded` @ `6fe21d00`: data is copied to `frame + 0x20`, and
the next record starts at `frame + length + 0x23`). Three bytes per frame are
never written. The DC6 file has the same 32-used-of-35 shape, which is what a
straight dump of that structure would produce. (The causal link is an
inference from the two layouts matching, not something the code states.)

That also settles `nextBlock`. In 21,480 of 26,316 frames it equals
`frameOffset + 32 + length + 3` — the offset of the next frame's header. In
the other 4,836 it holds values that are not offsets into the file at all: in
`fkpskp.DC6`, a 2,642-byte file, consecutive frames carry 7,340,156, 7,340,200,
7,340,244 — a regular stride of 44, which is the signature of stale in-memory
pointers rather than file offsets. **Use the frame-offset table; never trust
`nextBlock`.**

### The scanline encoding

The frame data is a byte-oriented opcode stream. The authority for it is not
folklore but the blitter that consumes it — `BlitSpriteRLECopyFull`
(@ `6fe1fb50`), the simplest of the sixteen `BlitSpriteRLE*` variants, whose
inner loop reduces to three cases:

| Opcode | Meaning |
|---|---|
| `0x00`–`0x7F` | copy the next *n* bytes as raw palette indices |
| `0x80` | end of scanline — move to the next row |
| `0x81`–`0xFF` | skip `n & 0x7F` pixels, leaving them transparent |

Three details are easy to get wrong and all three are settled by that function.
There is **no repeat/run opcode** — the only compression is the transparent
skip. `0x00` is a legal zero-length copy, not a terminator (it occurs zero
times in the corpus, but the decoder would accept it). And critically, **a
scanline may end early**: the blitter's `0x80` case advances to the next row
unconditionally, without any requirement that the row was filled. Across the
corpus that is the common case, not the exception — of 1,704,391 scanlines,
1,362,445 end before reaching the right edge and only 341,946 fill it exactly.
A decoder that asserts `x == width` at end-of-line rejects 24,797 of the
26,316 real frames.

With those rules, **every one of the 26,316 frames in all 1,654 files decodes
exactly**: the opcode stream consumes precisely `length` bytes and emits
precisely `height` scanlines, never exceeding `width`.

Rows are stored **bottom-up** — the first scanline in the data is the *lowest*
row on screen. This one is worth stating carefully, because D2CMP's blitter
cannot settle it (it advances by a caller-supplied stride, so it is
orientation-agnostic). It was settled from the data instead: decoding
`data\global\ui\PANEL\invchar.DC6` both ways and rendering with the Act I
palette produces a correctly-oriented character panel bottom-up and a
vertically mirrored one top-down.

The `flip` field at `+0x00` is set on 140 of 26,316 frames. What the engine
does with it is **unverified** — this chapter did not locate a read of it, and
the repository's own decoder ignores it entirely.

### Worked example: `invjav.dc6`

The Amazon's javelin, as it appears in her inventory. 1,276 bytes total.

```
header    06 00 00 00  01 00 00 00  00 00 00 00  ee ee ee ee  01 00 00 00  01 00 00 00
          version=6    flags=1      encoding=0   termination  dirs=1       frames/dir=1
                                                 (uninitialised)
offsets   1c 00 00 00                     -> one frame, at byte 28 (= 24 + 4·1)
```

At byte 28, the frame record:

```
00 00 00 00  1c 00 00 00  54 00 00 00  00 00 00 00
flip=0       width=28     height=84    offsetX=0
00 00 00 00  00 00 00 00  fc 04 00 00  bd 04 00 00
offsetY=0    unknown=0    nextBlock    length=1213
                          =1276
```

`28 + 32 + 1213 + 3 = 1276`, exactly the file size — so here `nextBlock` points
one past the end, and the three slack bytes are the file's last three. Sure
enough the file ends `83 02 ae b4 80 ee ee ee`: a 3-byte copy, the final `0x80`
closing the last scanline, then `ee ee ee`.

The data begins:

```
80           end of scanline        (row 0 is entirely empty)
86           skip 6 transparent
01 ac        copy 1 pixel:   index AC
8e           skip 14 transparent
03 ac 0f ac  copy 3 pixels:  AC 0F AC
80           end of scanline        (this row stopped at x=25 of 28)
85           skip 5 transparent
03 21 2d ac  copy 3 pixels:  21 2D AC
```

Two rows, decoded, in eighteen bytes — and the second one is one of the
1.36 million that ends short. That is the entire format.

---

## DCC: the format that has to be unpacked

DCC discards the assumption that frames are independent. A DCC direction is a
single continuous bitstream in which one frame's pixels are described as
*differences* from the frame before it, over a shared 4×4 cell grid, with a
per-direction palette and field widths chosen to fit that particular direction.
Nothing in it is byte-aligned except by accident, and no frame can be read
without decoding every frame before it in the same direction.

The payoff is real. Our Amazon torso is 35,479 bytes on disk and declares a
`finalDc6Size` of 97,411 — 2.7× — and that is before considering that DC6 could
not have expressed the 256 frames as compactly in the first place.

All of the following is from `DecompressDCCDirection` (@ `6fe240a0`), which is
one enormous function containing the whole decoder, plus its three helpers.
Blizzard's own source filename for it is in the binary: `..\Source\D2CMP\SRC\Codec.cpp`.

### The file header

15 bytes, read in one call at `6fe23034` (`PUSH 0xf`), followed by the
direction offset table:

| Offset | Size | Field | Observed in 21,692/21,692 files |
|---|---|---|---|
| `0x00` | 1 | signature | `0x74` — **checked** (`CMP byte ptr [ESP+0xc],0x74` @ `6fe2304e`) |
| `0x01` | 1 | version | `6` |
| `0x02` | 1 | directions | 1, 4, 8, 16 or 32 — **capped at 32** |
| `0x03` | 4 | framesPerDirection | 1 … 200 |
| `0x07` | 4 | tag | `1` |
| `0x0B` | 4 | finalDc6Size | 65 … 2,974,804 — **not read by the game** |
| `0x0F` | 4×*d* | direction offsets, absolute from file start | |

The signature is the one constant the engine actually enforces; a file whose
first byte is not `0x74` is rejected outright. The `finalDc6Size` dword is read
into the buffer and then never referenced — the same shape of vestigial field
as DC6's `termination`, but this one holds a meaningful number.

After the offset table the game appends a sentinel: `6fe230e9` writes the total
file size into `descriptor[0x1C + directions × 4]`, one slot past the last
direction. That is how the last direction's length is known. In all 21,692
files the first direction offset equals `15 + 4 × directions` exactly, so the
direction data begins immediately after the table.

### The direction header

Each direction begins with a 32-bit `outsizeCoded` — read directly as a dword,
not from the bitstream — and the bitstream proper starts at the next dword
boundary. `DecompressDCCDirection` sums `outsizeCoded` across the requested
directions before decoding anything, to size the output buffer.

The bitstream then opens with 30 bits:

| Bits | Field |
|---|---|
| 2 | compressionFlag |
| 4 | width code for `variable0` |
| 4 | width code for `width` |
| 4 | width code for `height` |
| 4 | width code for `offsetX` |
| 4 | width code for `offsetY` |
| 4 | width code for `optionalBytes` |
| 4 | width code for `codedBytes` |

Every one of those seven is a 4-bit **index into a 16-entry table**, not a bit
count. The table is at `6fe2d3f0` in D2CMP 1.13c and reads, verbatim from the
DLL image:

```
0, 1, 2, 4, 6, 8, 10, 12, 14, 16, 20, 24, 26, 28, 30, 32
```

So code `0` means the field occupies **zero bits** and is implicitly 0; code
`4` means 6 bits; code `15` means 32. (Alongside it at `6fe2d4b0` sits the
decoder's mask table, 33 entries of `(1 << n) - 1`, and at `6fe2d42c` /
`6fe2d538` the sign-bit and sign-extension masks used for the two signed
fields.)

All bit reads in DCC are **LSB-first within each byte**.

### The frame headers

Immediately after those 30 bits come `framesPerDirection` frame headers, packed
back to back with no alignment, each field taking the width its code selected:

| Order | Field | Signed? |
|---|---|---|
| 1 | `variable0` | no |
| 2 | `width` | no |
| 3 | `height` | no |
| 4 | `offsetX` | **yes** |
| 5 | `offsetY` | **yes** |
| 6 | `optionalBytes` | no |
| 7 | `codedBytes` | no |
| 8 | `bottomUp` — always 1 bit | no |

Only `offsetX` and `offsetY` are sign-extended, and the binary does it
explicitly: after reading, it tests the value against the sign-bit mask at
`6fe2d42c[n]` and ORs in `6fe2d538[n]` if set.

Three of these fields are, in practice, dead weight in vanilla data — and this
is the kind of thing only a full-corpus sweep can tell you:

- **`variable0` uses width code 0 in all 271,102 directions.** It is always
  zero bits wide and therefore always 0. Nobody knows what it was for.
- **`optionalBytes` uses width code 0 in all 271,102 directions.** No vanilla
  DCC contains an optional-bytes block at all, so the whole optional-data
  mechanism below is unexercised by shipped data.
- **`bottomUp` is 0 in all 3,304,078 frames.** DCC frames are top-down,
  universally — the opposite of DC6, and one of the two formats' few outright
  contradictions.

If `optionalBytes` were non-zero for any frame, the stream would byte-align and
then carry each such frame's raw bytes contiguously in frame order before
continuing (`6fe240a0`, the block guarded on the summed total). Vanilla never
takes that path.

### The five bitstreams

Next come up to four 20-bit sizes, gated on the two-bit compression flag:

| Order | Size field | Bits | Present when |
|---|---|---|---|
| 1 | equalCell stream size | 20 | `compressionFlag & 0x02` |
| 2 | pixelMask stream size | 20 | **always** |
| 3 | encodingType stream size | 20 | `compressionFlag & 0x01` |
| 4 | rawPixel stream size | 20 | `compressionFlag & 0x01` |

Note the asymmetry: bit 1 gates one stream, bit 0 gates two. All four flag
values occur in vanilla data — `0` in 91,224 directions, `3` in 81,643, `2` in
65,782, `1` in 32,453.

Then a **256-bit palette key**: eight 32-bit words, one bit per palette index
0–255. Walking indices 0…255 in ascending order and appending each set index
produces the direction's palette — a compact list of just the colours this
direction uses. Pixel *codes* in the streams below index into that list, not
into the game palette. Across the corpus a direction uses between 1 and 205
distinct colours; our Amazon torso's direction 0 uses 61.

Finally the five bitstreams themselves, laid end to end **at bit granularity**
— each begins exactly where the previous ended, with no padding between them:

1. `equalCell` — 1 bit per cell: "this cell is unchanged from the last frame"
2. `pixelMask` — 4 bits per changed cell: which of its 4 colour slots changed
3. `encodingType` — 1 bit per cell: how that cell's new colours are coded
4. `rawPixel` — 8-bit literal colour codes
5. `pixelCodeAndDisplacement` — everything else; **no size field**, it runs to
   the end of the direction

In all 271,102 directions of all 21,692 files, the four declared sizes fit
within the direction's extent — the layout is self-consistent everywhere.

### Cells

DCC divides a direction into a grid of **4×4-pixel cells**, and this is the
unit everything else is expressed in. `CalculateDCCCellCount` (@ `6fe24020`)
computes it from the direction's bounding box:

```
cellsWide  = (width  + 3) >> 2      // ceil(width / 4)
cellsHigh  = (height + 3) >> 2      // ceil(height / 4)
totalCells = cellsWide × cellsHigh
```

Cells on the right and bottom edges are simply narrower or shorter. Frames
inside the direction get their own cell grid, phase-aligned to the direction's
grid so that a frame cell always lines up with the buffer cell it updates —
which is what makes "unchanged from last frame" a meaningful statement.

### Decoding, stage 1: the pixel buffer

`DecompressDCCFramePixels` (@ `6fe23b70`) walks every cell of every frame and
maintains, per cell, a running record of **four colour codes**. For each cell:

1. If the cell has never been written, the pixel mask is forced to `0xF` (all
   four slots new) and no bits are consumed.
2. Otherwise read **1 bit** from `equalCell`. If set, the cell is identical to
   last frame — skip it entirely, consuming nothing further.
3. Otherwise read **4 bits** from `pixelMask`. Each set bit marks one of the
   four slots as changed; clear bits inherit that slot from the previous entry.
4. If any slot changed, read **1 bit** from `encodingType`.
5. For each changed slot, read one colour code:
   - `encodingType = 1` → **8 raw bits** from `rawPixel`.
   - `encodingType = 0` → a **delta from the previous code**, accumulated in
     4-bit nibbles from `pixelCodeAndDisplacement`, where a nibble of `15`
     means "continue, add the next nibble too". A total displacement of zero —
     that is, a code equal to the previous one — means *stop*: the remaining
     slots for this cell are not coded.

At the end of the stage every stored code is translated through the 256-bit
palette key into a real palette index.

### Decoding, stage 2: pixels

`DecompressDCCPixelBlock` (@ `6fe23780`) turns each cell's four-colour record
into actual pixels. The clever part is that **the bit depth is never
transmitted** — it is inferred from how many of the four codes are distinct:

| Condition | Bits per pixel | Meaning |
|---|---|---|
| `val[0] == val[1]` | 0 | the whole cell is one colour; no bits read |
| `val[1] == val[2]` | 1 | two colours, one bit selects |
| otherwise | 2 | up to four colours, two bits select |

With that decided, the cell's pixels are read row-major, `bitsPerPixel` bits
each, from `pixelCodeAndDisplacement`, and each value indexes the cell's own
four-entry table. A flat 4×4 cell of one colour therefore costs **zero** bits
of pixel data — which, on a character sprite that is mostly flat shading over
a transparent background, is where the compression actually comes from.

### Worked example: `AMTRLITTN1HT.dcc`

The town Amazon's torso. Its 15-byte header:

```
74 06 10 | 10 00 00 00 | 01 00 00 00 | 83 7c 01 00
^  ^  ^    ^             ^             ^
|  |  |    frames=16     tag=1         finalDc6Size=97411
|  |  directions=16
|  version=6
signature=0x74
```

Sixteen directions × sixteen frames = 256 frames, matching `AMTNHTH.cof`
exactly — the COF chapter's own worked example declares 8 layers, 16 frames
and 16 directions, and its torso layer carries weapon class `1ht`, which is
where the `TN1HT` in this filename comes from. The two chapters are looking at
the same Amazon.

The direction table follows at byte 15 and the first direction begins at byte
79 = `15 + 4 × 16`, as it must. Direction 0 opens:

```
5e 18 00 00   outsizeCoded = 6238
00 11 15 d8 …  the bitstream
```

Reading the first 30 bits of that stream, LSB-first:

| Field | Code | Width |
|---|---|---|
| compressionFlag | — | `0b00` — no equalCell, no encodingType, no rawPixel |
| `variable0` | 0 | 0 bits |
| `width` | 4 | 6 bits |
| `height` | 4 | 6 bits |
| `offsetX` | 4 | 6 bits |
| `offsetY` | 5 | 8 bits |
| `optionalBytes` | 0 | 0 bits |
| `codedBytes` | 6 | 10 bits |

So each frame header in this direction costs `0+6+6+6+8+0+10+1 = 37` bits, and
sixteen of them occupy 592 bits with no padding. Decoding the first three:

| Frame | width | height | offsetX | offsetY | codedBytes | bottomUp |
|---|---|---|---|---|---|---|
| 0 | 15 | 26 | −10 | −32 | 357 | 0 |
| 1 | 16 | 26 | −11 | −32 | 370 | 0 |
| 2 | 15 | 25 | −11 | −32 | 363 | 0 |

A 15×26 torso, offset up and to the left of the unit's origin, sitting almost
still between frames — which is exactly the redundancy the cell-delta encoding
is built to eat. Because `compressionFlag` is 0 here, only the `pixelMask`
stream carries a declared size (1,772 bits), and the direction's palette holds
61 colours.

---

## From a decoded frame to pixels on screen

Both formats converge. What the blitters consume is neither a DCC bitstream nor
a DC6 file but D2CMP's internal *cel*: a 24-byte header, a frame-offset table,
and per-frame records of the `0x23`-byte shape described earlier
(`ConvertRawCelToLoaded` @ `6fe21d00`, which promotes a "raw" cel with magic 5
to a "loaded" cel with magic 6).

For DC6 that conversion is nearly free — the scanline data on disk is already
in the blitters' opcode format, which is why the same three opcodes appear in
`BlitSpriteRLECopyFull`. For DCC it is the last step of decompression: after
the cell decode finishes, `DecompressDCCDirection` calls `EncodeImageToRLE` on
each frame, and if that fails the engine emits the string that is in the DLL
verbatim — `Error decompressing sprite - Possible corruption in data file: %s`.
**The game never holds a DCC frame as a flat bitmap.** It unpacks the
bitstream, rebuilds the image, and immediately re-encodes it into the same
run-length form DC6 already ships in.

Colour is applied at blit time, not decode time. The bytes in a decoded frame
are palette *indices*; which palette, and which remapping table on top of it,
is chosen per component and per blit. That machinery — `LoadPaletteFile`,
`LoadItemPaletteFile`, `LoadAllItemPaletteTransforms`, and the
`WithPalette` / `WithChainedTable` / `WithDualTable` blitter variants — is
[Stage 11 of the COF pipeline chapter](cof-pipeline-1.13c.md#stage-11-palette--color-d2cmpdll)
and is not repeated here. The only thing the sprite formats contribute is the
index, and one wrinkle worth naming: a DCC direction's codes are indices into
*its own* 256-bit-key palette list, resolved back to real palette indices at
the end of stage 1, so by the time any palette transform sees them the two
formats are indistinguishable.

---

## Replacing a sprite

What all of the above means at the workbench:

**The extension is fixed by code, not by you.** `BuildSpritePath` decides
`.dcc` or `.dc6` from the entity type and a hard-coded id list. You cannot ship
a DC6 where the engine asks for a DCC; the signature check at `6fe2304e`
rejects it and the sprite silently fails to draw.

**DC6 is the format you can author by hand; DCC is not.** DC6's encoding is
three opcodes and no state — a working writer fits on a page. DCC requires
reproducing a stateful cell-delta encoder that must also *choose* the seven
per-direction field widths and the compression-flag configuration — and D2CMP
contains only the decoder. The DLL has exactly three DCC-specific functions,
`DecompressDCCDirection`, `DecompressDCCFramePixels` and
`DecompressDCCPixelBlock`, and no compressing counterpart to any of them; every
`Compress*`/`Encode*` function in the module targets the RLE cel format or the
tile subtiles instead. Whatever tool you use to produce a DCC is reimplementing
an encoder the game does not carry, so a DCC round-trip is only ever as good as
that tool.

**Respect the caps.** Both formats reject more than 32 directions. DC6
additionally rejects `directions × frames ≥ 16,384`. Vanilla DCC never exceeds
200 frames per direction, though nothing in the header parser enforces that.

**Frame counts must match the COF.** A sprite is only ever indexed through the
COF's direction and frame counts. `AMTNHTH.cof` declares 16 × 16; the torso
DCC provides 16 × 16. A replacement with a different frame count does not
gracefully degrade — the COF, not the sprite, decides which indices get asked
for.

**Watch the orientation flip.** DC6 rows are bottom-up; DCC frames are
top-down (`bottomUp = 0` in all 3.3 million vanilla frames). A converter that
gets this wrong produces a vertically mirrored sprite, which is the single most
common visible symptom of a bad round-trip.

**Ignore the junk fields, and do not validate against them.** DC6's
`termination` dword, its per-frame 3 slack bytes, and its `nextBlock` in 18% of
frames are uninitialised memory. A loader that requires `0xEEEEEEEE`, or that
follows `nextBlock` instead of the offset table, will reject or misread real
Blizzard files.

**Patches do not ship sprites.** In the catalogued installs, `Patch_D2.mpq`
carries 0 DCC files and at most 1 DC6 in the versions whose patch archives
could be enumerated (1.08a–1.09d). Sprite overrides come from a mod's own
archive or from loose files, not from the patch archive.

---

## Reference

### Field caps and constants enforced by D2CMP 1.13c

| Check | Value | Address |
|---|---|---|
| DCC signature | must be `0x74` | `6fe2304e` |
| DCC directions | ≤ 32 | `6fe23065` |
| DC6 directions | ≤ 32 | `6fe1d503` |
| DC6 directions × frames | < `0x4000` (16,384) | `6fe1d4b8` |
| DCC header read size | 15 bytes | `6fe23034` |
| DC6 header read size | 24 bytes | `6fe1d48e` |

### DCC bit-width table (D2CMP 1.13c @ `6fe2d3f0`)

| Code | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Bits | 0 | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 20 | 24 | 26 | 28 | 30 | 32 |

### Corpus summary (vanilla 1.13c `d2char` + `d2data` + `d2exp`)

| Measure | DCC | DC6 |
|---|---|---|
| Files parsed | 21,692 | 1,654 |
| Frames | 3,304,078 | 26,316 |
| Parse failures | 0 | 0 |
| Directions observed | 1, 4, 8, 16, 32 | 1, 4, 8, 16 |
| Frames per direction | 1 – 200 | 1 – 1,499 |
| Frame width range | 1 – 345 | 1 – 319 |
| Frame height range | 1 – 324 | 1 – 256 |
| Row orientation | top-down (all frames) | bottom-up |

25 further paths appear in the archives' `(listfile)` but have no block-table
entry and cannot be read; they are counted as absent, not as failures.

### D2CMP source files named in the binary

`Codec.cpp` (the DCC decoder), `SpriteCache.cpp`, `CelCmp.cpp`,
`CelDataHash.cpp`, `LRUCache.cpp`, `GfxHash.cpp`, `DrwCntxt.cpp`, plus the
tile-side `Raw.cpp`, `Tilecmp.cpp`, `SubTile.cpp`, `FindTiles.cpp`,
`FastCmp.cpp`, `TileProjects.cpp`.

---

## What is independent here

Two of the three sources used for this chapter are independent of each other;
one is not, and saying so is the difference between two confirmations and one
wearing two hats.

The repository's own parsers, `src/core/dcc.c` and `src/core/dc6.c`, are
**both derived from Paul Siramy's code** — the repository's `NOTICE` lists
`dccinfo.c -> src/core/dcc.c` and `dc6info.c -> src/core/dc6.c` among the files
taken from `win_ds1edit`. Their agreement with Siramy's documentation of these
formats is therefore *not* corroboration; it is one source restated. This
chapter treats them as a single source and never counts them twice.

The genuinely independent evidence is:

1. **D2CMP.dll's own code**, read as disassembly, not decompiled C. Every
   published constant in this chapter — the `0x74` signature, the 15- and
   24-byte header sizes, the field-to-slot assignments, the 32-direction and
   16,384-frame caps, the 16-entry bit-width table — was confirmed against the
   instruction stream or read directly out of the DLL image.
2. **21,692 DCC and 1,654 DC6 real files**, parsed with a decoder written from
   the binary rather than from any existing parser.

Where those two agree with the Siramy-derived parsers, the parsers are
correct — but the weight is carried by the binary and the corpus. Two places
where they *diverge* are worth recording: the repository's DCC parser validates
no header constant at all (neither signature nor version nor the direction cap
the game enforces), and its DC6 parser never reads the `flip` field.

---

## Version differences

| What | 1.13c | 1.09d | 1.07a–1.12a | 1.14a–1.14d |
|---|---|---|---|---|
| DCC signature / version | `0x74` / `6` | `0x74` / `6` | `0x74` / `6` | `0x74` / `6` |
| DC6 version / flags / encoding | `6` / `1` / `0` | `6` / `1` / `0` | `6` / `1` / `0` | `6` / `1` / `0` |
| Module holding the decoder | `D2CMP.dll` | `D2CMP.dll` | `D2CMP.dll` | `Game.exe` — no `D2CMP.dll` ships |
| DCC bit-width table | 16 entries as above | (unverified) | (unverified) | (unverified) |
| Sprites in `Patch_D2.mpq` | none enumerable | 0 DCC, 1 DC6 | 0 DCC, 0–1 DC6 | none enumerable |

The header-constant row was checked by sampling up to 200 DCC and 200 DC6 files
from `d2data`, `d2char` and `d2exp` in each of fifteen catalogued installs
(1.07a, 1.08a, 1.09a, 1.09b, 1.09d, 1.10a, 1.11a, 1.11b, 1.12a, 1.13c, 1.13d,
1.14a–d). **Important caveat:** those installs share byte-identical base
archives — `d2data.mpq` is 267,642,202 bytes with the same leading hash in all
fifteen — so this establishes that every patch reads the same sprite data, not
that Blizzard never revised a sprite. Patch archives from 1.10a onward carry no
`(listfile)` and could not be enumerated.

The bit-width table was read only from 1.13c's `D2CMP.dll`; the same table in
other versions is **unverified**, though the format's version byte being 6
everywhere makes a change unlikely.

---

*Companion report, with the full claim ledger, corrections and open questions:
[sprite-formats-dcc-dc6.verification.md](sprite-formats-dcc-dc6.verification.md).*
