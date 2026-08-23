# Verification report — sprite-formats-dcc-dc6.md

Companion to [sprite-formats-dcc-dc6.md](sprite-formats-dcc-dc6.md).
Verification date: 2026-08-21.

---

## 1. Origin and rights

**This chapter is not a modernisation of archived material.** The preservation
archive at `docs/preservation/siramy/` was searched for any page covering DCC
or DC6: `MANIFEST.tsv` and all 101 archived HTML pages contain no DCC or DC6
format documentation, and no file in the archive is named for either format.
The single grep hit for `.dcc`/`.dc6` is a binary GIF. The chapter was
therefore written from primary evidence rather than converted, and it carries
no origin block for archived prose and no attributed author's wording.

**The rights question that does apply** is narrower than the archive's. Two
files this chapter used as a cross-check — `src/core/dcc.c` and
`src/core/dc6.c` — are derived from Paul Siramy's `win_ds1edit`, and the
repository's `NOTICE` already states the project's position on that reuse:
Siramy published source alongside every binary from 2002 onward, no licence
text accompanies it, and DS1Edit uses the code on that basis with attribution.
That position covers **reusing his code in this repository**. It does not by
itself cover **republishing his prose in a book**, and no prose of his is
reproduced here — which is why this chapter raises no new rights question. The
project's position on republishing archive-derived prose and images — an
explicit fair-use judgment, not a licence — is recorded in
[BOOK-STATUS.md](../BOOK-STATUS.md) for the chapters that do convert his
pages; it is untouched by this one.

---

## 2. Ground truth used

| Source | Identity |
|---|---|
| Binary | `D2CMP.dll`, vanilla LoD 1.13c, 163,840 bytes, SHA-256 `2ee205f484161c5ed854f30aed12565a7ac3e97fd1a838e697aaefe4dc349756` |
| Ghidra program | `/Vanilla/1.13c/D2CMP.dll`, image base `6fe10000`, 804 functions |
| Game data | `d2char.mpq`, `d2data.mpq`, `d2exp.mpq` from `F:\D2Catalog\Diablo2-L113c\` |
| Cross-version data | fifteen catalogued installs, 1.07a through 1.14d |
| Derived parsers (single source) | `src/core/dcc.c`, `src/core/dc6.c`, `src/core/dc6_header.c` |

The DLL is byte-identical between the version-changer tree
(`F:\D2VersionChanger\VersionChanger\LoD\1.13c\D2CMP.dll`) and the catalogue
install whose MPQs supplied the data, so binary evidence and data evidence come
from the same build. Both were hashed to confirm this.

**Program disambiguation.** Two Ghidra programs share the path prefix
`/Vanilla/1.13c/D2CMP.dll` — one suffixed `.0` with 709 functions, one without
a suffix with 804. Both have image base `6fe10000` and the same executable
path. All work in this chapter used `/Vanilla/1.13c/D2CMP.dll` (804 functions),
passed explicitly as `program=` on every call. The session's *active* program
was `/Vanilla/1.09d/D2Common.dll` — an unrelated module — so relying on the
default would have produced nonsense.

**Not used.** No live fleet member and no `/call` oracle was used; every claim
here is settled statically or from file data. No claim in the chapter needed a
running game.

---

## 3. Claim tally

| Type | Checked | Confirmed | Corrected during work | Left unverified |
|---|---|---|---|---|
| F — format/layout (byte offsets, widths, bit fields) | 41 | 41 | 3 | 2 |
| A — mechanical (function at address, string, constant) | 19 | 19 | 0 | 0 |
| B — interpretive (what a function does) | 9 | 8 | 1 | 1 |
| C — contextual (world/tooling claims) | 5 | 4 | 1 | 1 |
| D — data/asset (what real files contain) | 24 | 24 | 0 | 0 |

Every function whose behaviour the chapter asserts was decompiled in full, and
the six carrying published constants were additionally read as disassembly.
Type-D claims were not sampled: the sweep covers **100% of the DCC and DC6
files in the three archives**.

---

## 4. Functions verified

| Function | Address | Verified as | Method |
|---|---|---|---|
| `LoadSpriteDefinition` | `6fe1d350` | DC6 header parser + descriptor fill | decompile + full disassembly |
| `LoadTileResourceData` *(mislabelled)* | `6fe23010` | **DCC header parser** | decompile + full disassembly |
| `BuildSpritePath` | `6fe1c2d0` | chooses `.dcc`/`.dc6` and the directory root | decompile |
| `DecompressDCCDirection` | `6fe240a0` | the whole DCC direction decoder | decompile |
| `CalculateDCCCellCount` | `6fe24020` | 4×4 cell grid sizing | decompile |
| `DecompressDCCFramePixels` | `6fe23b70` | stage 1, pixel-buffer fill | decompile |
| `DecompressDCCPixelBlock` | `6fe23780` | stage 2, cell → pixels | decompile |
| `BlitSpriteRLECopyFull` | `6fe1fb50` | the three RLE opcodes | decompile |
| `ConvertRawCelToLoaded` | `6fe21d00` | raw cel (magic 5) → loaded cel (magic 6) | decompile |
| `DecodeSpriteFrameData` | `6fe1d76e` | sole caller of `DecompressDCCDirection` | decompile + xref |

Constants read directly from the DLL image rather than from decompiled C:

| Address | Content |
|---|---|
| `6fe2d3f0` | DCC bit-width table, 16 × u32: `0,1,2,4,6,8,10,12,14,16,20,24,26,28,30,32` |
| `6fe2d4b0` | mask table, 33 × u32 of `(1<<n)-1` |
| `6fe2d42c` / `6fe2d538` | sign-bit and sign-extension masks for the two signed frame fields |

Instruction-level confirmations of published caps and sizes:

| Claim | Instruction | Address |
|---|---|---|
| DCC signature must be `0x74` | `CMP byte ptr [ESP + 0xc],0x74` | `6fe2304e` |
| DCC header is 15 bytes | `PUSH 0xf` | `6fe23034` |
| DCC directions ≤ 32 | `CMP CL,0x20` / `JBE` | `6fe23065` |
| DC6 header is 24 bytes | `PUSH 0x18` | `6fe1d48e` |
| DC6 directions ≤ 32 | `CMP ECX,0x21` / `JC` | `6fe1d503` |
| DC6 dirs × frames < 16,384 | `CMP EAX,0x4000` / `JC` | `6fe1d4b8` |
| DCC file-size sentinel past last direction | `MOV [ESI + ECX*0x4 + 0x1c],EAX` | `6fe230e9` |

---

## 5. Corrections made during the work

These are errors in the working model that evidence overturned before
publication. They are listed because each is a trap a reader could fall into.

**5.1 — DC6 scanlines do not have to fill the row.**
*Before:* a scanline ends when `x == width`, and `0x80` confirms it.
*After:* `0x80` ends the row wherever the cursor is; the remainder stays
transparent.
*Evidence:* `BlitSpriteRLECopyFull` @ `6fe1fb50` — the `(bVar1 & 0x7f) == 0`
branch decrements the row counter and moves to the next row with no width
check. Confirmed by data: enforcing `x == width` rejects **24,797 of 26,316**
real frames; dropping it accepts all 26,316. Of 1,704,391 scanlines, 1,362,445
end short.

**5.2 — DC6 `nextBlock` is not reliably `offset + 32 + length + 3`.**
*Before:* the field always points at the next frame header.
*After:* it does in 21,480 of 26,316 frames; in the other 4,836 it holds values
that are not file offsets at all.
*Evidence:* `fkpskp.DC6` (2,642 bytes) carries 7,340,156 / 7,340,200 /
7,340,244 on consecutive frames — stride 44, consistent with stale in-memory
pointers. 4,553 mid-frame and 283 last-frame mismatches; in none of them does
the value equal the next frame's offset or the file size.

**5.3 — the 3 bytes after each DC6 frame are not a terminator.**
*Before:* a fixed terminator sequence.
*After:* uninitialised slack. Values across the corpus: `EEEEEE` ×19,242,
`000000` ×4,077, `CDCDCD` ×2,748, and a tail of ~250 other values. Nothing in
D2CMP reads them. The structural reason is that D2CMP's cel frame record has a
`0x23`-byte stride but writes only `0x20` before the data
(`ConvertRawCelToLoaded` @ `6fe21d00`).

**5.4 (type B) — `GetCelFramePixelData` and `ConvertRawCelToLoaded` are not
DCC/DC6 parsers.** Both were on the inherited list of "sprite engine" functions
to extend from. Decompiling them shows they operate on D2CMP's *internal cel*
structure (magic 5 / magic 6), not on either on-disk format. They are cited in
the chapter only for what they actually establish — the cel layout and the
`0x23` stride.

---

## 6. Tooling defect found and worked around

`tools/d2mpq.py` **does not exist in this repository** (`tools/` contains only
`tools/ai/`). The working copy named in the task brief was used instead.

That copy has a real defect worth fixing before it is productionised: **it
cannot read files stored uncompressed.** Its `read()` assumes every
non-single-unit file carries a sector-offset table, but files flagged only
`MPQ_FILE_EXISTS` (`0x80000000`) — no `MPQ_FILE_COMPRESS`, no
`MPQ_FILE_IMPLODE`, `size == archived_size` — are stored flat with no table.
For those it returns `None` or a truncated buffer, silently.

Scale of the problem: **11,821 of 21,717 DCC files** were unreadable — almost
all of `data\global\missiles\*.dcc` in `d2data.mpq` and most of
`data\global\CHARS\*.dcc` in `d2exp.mpq`. The first sweep of this chapter
produced garbage statistics because of it (spurious signature values, a
`framesPerDirection` range topping out at 3,936,616,448).

The fix is four lines — when neither compression flag is set, seek to
`block.offset + header.offset` and read `block.size` bytes directly — and was
applied in a subclass rather than by editing the shared file. After the fix,
21,692 of 21,717 files parse; the remaining 25 are `(listfile)` entries with no
block-table entry, i.e. genuinely absent from the archive.

**Recommendation:** fold the uncompressed-file case into `tools/d2mpq.py` when
it lands. The failure mode is silent, which is the dangerous kind — the brief
already warns that bare `mpyq` hands back compressed bytes without raising, and
this is the same class of bug one layer down.

---

## 7. Data verification — coverage and results

Decoders were written **from the binary**, not from the repository's parsers,
then run over the whole corpus.

### 7.1 Structural sweep (all files)

| | DCC | DC6 |
|---|---|---|
| Files in archives' listfiles | 21,717 | 1,654 |
| Files readable | 21,692 | 1,654 |
| Files parsed without error | **21,692 (100%)** | **1,654 (100%)** |
| Frame headers decoded | 3,304,078 | 26,316 |
| Directions decoded | 271,102 | — |

Invariants that held with **no exceptions**:

| Invariant | Count |
|---|---|
| DCC signature = `0x74` | 21,692 / 21,692 |
| DCC version = 6 | 21,692 / 21,692 |
| DCC `tag` = 1 | 21,692 / 21,692 |
| DCC first direction offset = `15 + 4 × directions` | 21,692 / 21,692 |
| DCC `variable0` width code = 0 | 271,102 / 271,102 directions |
| DCC `optionalBytes` width code = 0 | 271,102 / 271,102 directions |
| DCC `bottomUp` = 0 | 3,304,078 / 3,304,078 frames |
| DCC four sized streams fit within the direction | 271,102 / 271,102 |
| DC6 version/flags/encoding = 6/1/0 | 1,654 / 1,654 |
| DC6 first frame offset = `24 + 4 × dirs × frames` | 1,654 / 1,654 |
| DC6 frame `unknown` field (`+0x14`) = 0 | 26,316 / 26,316 |

Observed ranges: DCC directions ∈ {1, 4, 8, 16, 32}; DCC frames/direction
1–200; DCC frame width 1–345, height 1–324, offsetX −319…202, offsetY
−236…240; DCC palette colours per direction 1–205; DCC `outsizeCoded`
37–1,315,780; DCC `finalDc6Size` 65–2,974,804. DC6 directions ∈ {1, 4, 8, 16};
DC6 frames/direction 1–1,499; DC6 frame width 1–319, height 1–256, offsetX
−287…62, offsetY −228…169, data length 2–66,560; DC6 `flip` = 1 on 140 of
26,316 frames.

DCC compression-flag distribution across 271,102 directions: `0` ×91,224,
`3` ×81,643, `2` ×65,782, `1` ×32,453 — all four values occur, so all five
bitstream configurations are exercised by vanilla data.

### 7.2 DC6 scanline decode (all frames)

Every one of the **26,316 frames in all 1,654 files** decodes exactly under the
three opcodes taken from `BlitSpriteRLECopyFull`: the opcode stream consumes
precisely `length` bytes and emits precisely `height` scanlines, with `x` never
exceeding `width`. Maximum raw-copy run observed: 127. Zero occurrences of
opcode `0x00`. 1,704,391 end-of-scanline opcodes, exactly matching the summed
frame heights.

### 7.3 DC6 row orientation — settled visually

D2CMP's blitter cannot settle orientation: it advances rows by a
caller-supplied stride and is direction-agnostic. The claim was therefore
tested against data. `data\global\ui\PANEL\invchar.DC6` frame 0 (256×256) was
decoded both ways and rendered to PNG with the Act I palette
(`data\global\palette\ACT1\pal.dat`, 768 bytes). The bottom-up render produces
a correctly oriented Diablo II character panel; the top-down render is
vertically mirrored. **Bottom-up confirmed from data, independent of any
existing parser.**

### 7.4 Cross-version sampling

Up to 200 DCC and 200 DC6 files from each of `d2data`, `d2char`, `d2exp` in
fifteen installs (1.07a, 1.08a, 1.09a, 1.09b, 1.09d, 1.10a, 1.11a, 1.11b,
1.12a, 1.13c, 1.13d, 1.14a, 1.14b, 1.14c, 1.14d). DCC `(signature, version,
tag)` was `(0x74, 6, 1)` in every sample; DC6 `(version, flags, encoding)` was
`(6, 1, 0)` in every sample.

**This result is weaker than it looks and the chapter says so.** The base
archives are byte-identical across all fifteen installs — `d2data.mpq` is
267,642,202 bytes with the same leading-4KB hash everywhere, and `d2char.mpq`
likewise. The catalogue reuses one base MPQ set, so this establishes that every
patch reads the same sprite bytes, **not** that Blizzard never revised a sprite
between patches. Patch archives from 1.10a onward carry no `(listfile)` and
could not be enumerated at all.

---

## 8. Shared-ancestry determination

The brief flagged `dc6info.c -> src/core/dc6.c`. Reading `NOTICE` at the
repository root shows the DCC parser is in the same position:

```
    src/core/dc6.c, .h         <- dc6info.c, dc6info.h
    src/core/dcc.c, .h         <- dccinfo.c, dccinfo.h
```

**Both** parsers derive from Paul Siramy's `win_ds1edit`. Neither is an
independent witness to his documentation of these formats, and the two of them
together are one source, not two. The chapter states this explicitly in its
*What is independent here* section and never counts them twice.

Independence therefore rests on exactly two legs: D2CMP's own code, and the
21,692 + 1,654 real files. Where the derived parsers agree with those, they are
correct — but they contribute no additional confidence.

### Where the binary independently confirms the derived parsers

Every one of these was established from D2CMP or from file data without
reference to the parsers, and matches them:

- the 16-entry bit-width table, value for value (read from `6fe2d3f0`)
- DCC header: 15 bytes, `15 + 4 × directions`, field order and widths
- DCC direction header: 32-bit `outsizeCoded`, 2-bit flag, seven 4-bit codes
- DCC frame header: eight fields in that order, `offsetX`/`offsetY` signed only
- optional-bytes handling: byte-align, then raw bytes in frame order
- the four 20-bit stream sizes and their gating (`0x02` → equalCell;
  unconditional → pixelMask; `0x01` → encodingType **and** rawPixel)
- the 256-bit palette key and its ascending-index expansion
- five streams laid end to end at bit granularity, the fifth unsized
- pixel buffer: 4 entries per cell, 4-bit mask, 1-bit encoding type, 4-bit
  nibble accumulation with `15` as the continuation escape, equal-to-previous
  as terminator
- stage 2: bits-per-pixel inferred from value equality, never transmitted
- cell grid: `ceil(w/4) × ceil(h/4)`. The binary computes `(w + 3) >> 2`; the
  parser computes `1 + (w - 1) / 4`. These are the same function for `w ≥ 1` —
  not a divergence.
- DC6: 24-byte header, 32-byte frame header, data at `+0x20`, the three opcodes

### Where they diverge

| Divergence | Adjudication |
|---|---|
| `src/core/dcc.c` validates **no** header constant — signature, version and the 32-direction cap are never checked | The **binary** is stricter and is right about the format. `CMP byte ptr [ESP+0xc],0x74` is enforced by the game; a file failing it will not load however tolerant an editor is. Parser robustness gap, not a format question. |
| `src/core/dc6.c` never reads the frame `flip` field | Cannot be adjudicated — see open question 9.1. |
| `src/core/dc6_header.c` allows up to 1,024 directions and 65,536 frames/dir; does not check `version == 6` | The **game** caps directions at 32 and `dirs × frames` at 16,384. The sniffer's limits are looser than the format's. |

---

## 9. Unverified, and open questions

**9.1 — What does DC6's `flip` field do?**
Set on 140 of 26,316 frames, so it is not always zero and is not obviously
junk. No read of it was located in D2CMP, and the repository's decoder ignores
it. Marked unverified in the chapter. Settling it would need either a targeted
search of D2Win/D2Client (which also draw DC6 UI art) or a runtime experiment.

**9.2 — What is DCC's `variable0`?**
Width code 0 in all 271,102 vanilla directions, so it is always zero bits wide
and always 0. The decoder reads it into the frame record and the chapter can
say nothing more. Its purpose is unknown and probably unknowable from vanilla
data alone.

**9.3 — Semantics of DC6 `flags` and `encoding`.**
Constant at 1 and 0 across all 1,654 files. The game stores them in the sprite
descriptor at `+0x08` and `+0x0C`; consumers of those slots were not traced.
The names are the community's. Presented in the chapter as observed constants
with the semantics unclaimed.

**9.4 — The `0x23`/`0x20` cel-stride explanation for DC6's 3 slack bytes.**
The chapter presents this as an inference, explicitly marked. The two layouts
match, but no code states that DC6 was produced by dumping the cel structure.

**9.5 — Cross-version bit-width table.**
Read only from 1.13c. Other versions unverified; noted as such in the version
table.

**9.6 — Whether patches ever ship sprites (1.10a+).**
Patch archives from 1.10a onward have no `(listfile)`, so the "0 DCC files"
result covers only 1.08a–1.09d. The chapter scopes the claim to what was
enumerable.

**9.7 — The DCC encoder. (Raised, then resolved.)**
An earlier draft asserted that Blizzard's DCC encoder "was never released" and
that modders use community converters — a type-C claim resting on general
knowledge rather than evidence. It was rewritten to a checkable form and then
checked: D2CMP 1.13c exports exactly three DCC-specific functions
(`DecompressDCCDirection` @ `6fe240a0`, `DecompressDCCFramePixels` @ `6fe23b70`,
`DecompressDCCPixelBlock` @ `6fe23780`) and **no** compressing counterpart. The
module's `Compress*` and `Encode*` functions —
`CompressImageToRLE` @ `6fe1fd70`, `CompressFramesToRLEWithDirections` @ `6fe208f0`,
`EncodeImageToRLE` @ `6fe23620`, `CompressSubtileToRLE` @ `6fe20e30`,
`CompressSubtileRowsToRLE` @ `6fe21000`, `ConvertAndCompressSubtile` @ `6fe264f0`,
`ProcessSubtileForCompression` @ `6fe21100`,
`EncodeTileDirectionsToPixelData` @ `6fe26a10` — all target the RLE cel format
or DT1 subtiles. The chapter now says only what that supports: the game carries
no DCC writer. What third-party tools exist is still outside this chapter's
evidence and is no longer claimed.

---

## 10. Conventions compliance

- **1.13c unmarked default** — the body is written for 1.13c with no
  qualifiers; every other version appears in a marked callout or the
  differences table.
- **Version notes** — one inline callout (DC6 header constants across
  1.07a–1.14d). One `> **Note on Ghidra naming:**` callout is used for the
  mislabelled function at `6fe23010`; this is not a version or mod note and is
  formatted distinctly from both.
- **Differences table** — present, 1.13c first data column.
- **Evidence cited inline** — every non-obvious claim carries an address, an
  instruction, or a corpus count.
- **Unverified marked in place** — six items, each carried in the text as well
  as listed above.
- **Vanilla data only** — nothing in `assets/` was used; all data came from
  `F:\D2Catalog\Diablo2-L113c\*.mpq`.
- **Structure** — provenance block, narrative introduction, prose-led body
  sections, one worked example threaded through (the town Amazon: her torso
  `AMTRLITTN1HT.dcc` for DCC, her javelin's inventory icon `invjav.dc6` for
  DC6, tied to `AMTNHTH.cof` from the COF chapter), reference tables, version
  differences, companion link.
- **No duplication of Stage 11** — palette transforms are referenced by link,
  and only the sprite formats' own contribution (the index, and DCC's
  per-direction palette indirection) is described.

**One deviation to record.** Convention §7 asks for a single worked example.
This chapter covers two formats that share no bytes, so a single *file* could
not serve both. The thread used instead is a single *scene* — one Amazon
standing in town with her inventory open — from which both files are drawn.
The chapter states this rather than implying the two files are related by
format.
