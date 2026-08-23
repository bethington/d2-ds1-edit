# Diablo II's Game Network Protocol, Decoded

> **Provenance.** Original reverse engineering, verified **21 August 2026**
> against vanilla Lord of Destruction **1.13c** binaries in
> `F:\D2VersionChanger\VersionChanger\LoD\1.13c\` and against a working
> decoder, `d2-fleet/net/d2codec.py`, validated in that project against **341
> (compressed → decompressed) pairs** recorded from a live Project Diablo 2
> member. Ghidra: `Fog.dll` (image base `6ff50000`, SHA-256
> `96c05770…2643bb863`) and `D2Net.dll` (image base `6fbf0000`, SHA-256
> `99af2f04…5018e9910c9` — hash-identical to the file shipped in vanilla
> 1.13c, confirmed in this pass despite living in a project folder named for
> the private-realm work) and `D2Client.dll` (`6fab0000`, SHA-256
> `dd8bc602…8836d906`). PE structure was read with `read_memory` and
> `disassemble_bytes` directly, so the addresses below are read off the byte
> image, not taken on trust from a decompiler.
>
> `net/d2codec.py` is original work from the `d2-fleet` project (read-only
> source for this chapter; nothing in it was launched, driven, or modified).
> A companion audit records every claim, its verdict, and what is still
> unverified: [network-protocol.verification.md](network-protocol.verification.md).

Diablo II shipped in 2001 with no published protocol specification, and its
game stream is not merely undocumented — for years it looked, to four
separate people doing careful static analysis, like four different and
mutually exclusive things. One read it as uncompressed. One found the
compression in the wrong DLL. One found a table that looked like the
Huffman code lengths and built the wrong tree from it. One tried the
standard trick for exactly that situation — reconstruct the codes from
canonical ordering — and got numbers a real client would reject. All four
readings were defensible. All four were wrong, and each cost real time
before someone stopped reasoning about the file and instead put a breakpoint
on the function actually doing the work, inside a running game.

This chapter is the settled account: how the bytes on the wire become the
packets a client and server exchange, why the compression resisted
inspection specifically, how a length is worked out for a packet with no
length field of its own, and what one modern client — Project Diablo 2 —
changes without telling anyone. One real captured chunk is followed the
whole way through, from the two bytes that arrive over TCP to the two
in-game events it describes. Every claim below says what settled it, and the
last section says plainly which parts rest on the decoder's own comments
versus what this pass independently re-derived from the binaries.

---

## 1. A stream of chunks, not a stream of packets

The first thing a client sees from the server is not a packet. It is two
cleartext bytes — `AF 01` — sent before any compression begins. Everything
after that is **chunked**: wrapped in a small header whose job is only to say
how many bytes follow, with no relationship to where one game packet ends
and the next begins. A reader that treats "one chunk" as "one packet" will be
right by accident some of the time and wrong the rest, because a single
compressed chunk routinely holds two or more packets end to end, and a
single packet can straddle two TCP segments.

The chunk header is one or two bytes, and which it is depends on a single
comparison against `0xF0`:

| First byte `b0` | Header size | Total chunk length (header included) |
|---|---|---|
| `b0 < 0xF0` | 1 byte | `b0` |
| `b0 >= 0xF0` | 2 bytes | `((b0 & 0x0F) << 8) \| b1` |

The length **includes the header itself** — a chunk reporting length 16 carries
15 bytes of payload after its own header byte. Getting this backwards is the
single costliest arithmetic slip on record for this work: `0xAF` is decimal
175, which is **not** `>= 0xF0` (240). Reading the two-byte handshake as a
2-byte chunk header mis-frames byte zero of the entire stream and makes
everything downstream look like an unrecognizable format — not a subtly
wrong parse, a completely opaque one. `tests/test_d2codec.py::test_af_01_is_not_a_two_byte_header`
exists specifically to pin this one comparison.

A **worked example**, carried through the rest of this chapter: a real
15-byte compressed chunk, captured mid-session (fixture pair 93 of the 341
described in §6), arrives as sixteen bytes on the wire:

```
10 58 83 a0 35 10 cd 96 ac 3f c4 93 54 06 cb 40
```

`b0 = 0x10` (16), which is less than `0xF0`, so this is a 1-byte header
reporting a total length of 16. The payload is the remaining 15 bytes:

```
58 83 a0 35 10 cd 96 ac 3f c4 93 54 06 cb 40
```

Those 15 bytes are **not** the packet. They are Huffman-compressed, and
decompressing them is §2's job. What matters here is only the framing: this
chunk's boundary is at byte 16 of the stream, full stop — nothing about that
boundary says anything about how many game packets live inside it. As it
turns out, this one holds exactly two.

### Reassembly is per-TCP-connection, not per-port

A server that hosts both sides of a game in one process — as the fleet's own
test host does — produces a loopback capture with **two** independent
server→client streams sharing port 4000. Extracting bytes by port number
glues the two together, and the corruption does not look like corruption: it
looks like a plausible chunk that mis-frames a few packets later and then
dies on an opcode that resembles a missing size-table entry. The fix is to
reassemble by connection (the 4-tuple), never by port; `tests/test_d2pipe.py`
pins both the clean parse and this specific mixed-stream failure so it
cannot silently return.

A related trap on the receiving end: a packet can arrive split across two
TCP segments. A dechunker that assumes "one `recv()` == one or more complete
chunks" will occasionally hand a caller a truncated tail. The decoder's own
`dechunk()` returns `(payloads, leftover)` for exactly this reason — the
leftover bytes are carried forward and prefixed onto the next read, rather
than discarded or misread as the start of a new chunk. In the reference
capture used to validate this decoder end to end, at least one of 210 chunks
straddles a segment boundary this way.

---

## 2. The codec that took four tries to name correctly

Diablo II compresses its server→client stream with a Huffman code — one
alphabet of 256 symbols (every possible output byte), variable-length codes
from 1 to 11 bits, MSB-first, decoded through a classic fast/slow two-stage
lookup. None of that is unusual for a 2001 game engine. What made it
resistant to inspection was not the algorithm; it was where its tables live.

### Four readings, in order, each wrong

The decoder's own docstring keeps the record because it is the best
illustration available of why measurement beats inference in this codebase:

> "no compression", "the codec is in D2Net", "the code-length table is a
> seed", "canonical ordering"

Each of those was a real conclusion someone reached from static analysis of
the shipped files, and each was refuted only by watching the real code run.
What finally settled it was a breakpoint on **Fog.dll ordinal 10224**, in a
live game, recording every `(compressed input → decompressed output)` pair
the function actually produced. That methodology — and what it produced — is
the subject of §6. This section is about the function itself, confirmed
independently in this pass by reading Fog.dll in Ghidra rather than trusting
the decoder's transcription of it.

### Finding the function: from an import name to a disassembly match

`D2Net.dll` frames the compressed stream and hands each chunk's payload to
Fog.dll to decompress. Following the call site at `D2Net.dll+0x73F5`
(`6FBF73F5`) through its thunk lands on an import-table slot whose symbol
name Ghidra already carries: `Ordinal_10224`. That is Fog.dll's ordinal
10224 — confirmed, not assumed, by the import record itself
(`EXTERNAL:00000058 → Ordinal_10224`), and it resolves to
`DecodeHuffmanBitStream` at `Fog.dll+0x1EDA0` (`6FF6EDA0`), whose Ghidra
signature reads:

```
int DecodeHuffmanBitStream(byte *pOutput, int nMaxOutputBytes,
                            byte *pInput, int nInputBytes)
```

That matches the calling convention the decoder's comments record —
`__fastcall(dst/ecx, dstLen/edx, src/stack, srcLen/stack)` — parameter for
parameter. More usefully, the call site itself carries a second, independent
confirmation: immediately before the `CALL`, `D2Net` loads the constant
`0x7B8` (1,976 decimal) into `EDX` as the maximum output size. That is
exactly the `max_out=0x7B8` default the Python decoder uses, read here
straight off the disassembly rather than out of a comment.

The disassembly of `DecodeHuffmanBitStream` reproduces the algorithm the
decoder claims, instruction for instruction:

```
6ff6edb9   MOV  ESI, 0x20                 ; free = 32
6ff6edbe   CMP  ESI, 0x8                  ; while free >= 8 and bytes remain:
6ff6edc9   TEST EDX, EDX
6ff6edcb   MOVZX EBX, byte ptr [EAX]      ;   bitbuf |= *src++ << (free -= 8)
...
6ff6ede4   SHR  ECX, 0x18                 ; top = bitbuf >> 24
6ff6ede7   MOV  EBX, [ECX*4 + 0x6ff806d0] ; entry = TBL_FAST[top]
6ff6edee   MOVZX EBP, byte ptr [EBX]      ; nbits = entry[0]
6ff6edfc   AND  EDX, [EBP*4 + 0x6ff7f310] ; idx  &= TBL_MASK[nbits]
6ff6ee03   MOV  BL, [EDX + EBX + 0x1]     ; sym  = entry[1 + idx]
6ff6ee0a   MOVZX ECX, byte ptr [ECX + 0x6ff7f210]  ; codelen = TBL_CLEN[sym]
6ff6ee11   ADD  ESI, ECX                  ; free += codelen
6ff6ee13   CMP  ESI, 0x20
6ff6ee16   JG   0x6ff6ee36                ; free > 32: clean end, stop
```

`0x6FF806D0`, `0x6FF7F310` and `0x6FF7F210` are exactly the three table
addresses (`TBL_FAST_VA`, `TBL_MASK_VA`, `TBL_CLEN_VA`) the decoder's own
Huffman-table reader names — independently confirmed here from the
instructions that reference them, not copied from the reader's source. The
loop's termination condition — accumulate bits until adding one more symbol's
code length would push the 32-bit window's unfilled-bit counter (`free`)
past 32, then stop cleanly — is also reproduced exactly, and it is the
mechanism that keeps padding bits from decoding into phantom trailing bytes
(§2's last topic, below).

### What this pass additionally found: the tables are not in the file

Reading those same three addresses directly — with Ghidra's static
`read_memory`, and independently with `pefile` against the plain, un-patched
`Fog.dll` on disk (SHA-256 identical across the vanilla 1.13c tree, PD2
Season 13's client tree, and the minimized fleet tree used to record the
original captures) — returns **all zeros**. Not corrupted data: a
zero-filled region, in three separate copies of the file that are
byte-identical to each other.

That is not a bug in the file. Fog.dll exports a function named (by earlier
work in this Ghidra project) `BuildPKWareHuffmanDecodeTables`, at
`Fog.dll+0x1EFD0`. Its decompiled body takes a 256-byte array of per-symbol
code lengths and writes the fast-lookup and mask tables into exactly those
three addresses — `&DAT_6ff806d0`, the region at `0x6ff7f210`, and their
neighbors — as part of a classic canonical-Huffman table build. In other
words: **the lookup tables the decoder needs do not exist in the shipped
file.** They are computed once, at runtime, from a seed array that the
disassembly shows being consumed but that is itself transient — read,
bucketed, and discarded within the same function, never sitting at a fixed,
easily-located address the way the finished tables briefly do once built.

This is a freshly independent confirmation, not a repetition of the
decoder's comments, and it explains — mechanistically, not just
historically — why two of the four wrong readings looked as plausible as
they did. A reader who spots `BuildPKWareHuffmanDecodeTables` and its
256-byte input naturally reaches for "there's a seed table, and the standard
canonical-Huffman assignment reconstructs the real codes from it." Both
pieces of that guess are real: the function is real, the seed argument is
real. What is not recoverable by inspection is the *contents* of that seed
at the moment this build runs, or the exact bucket-ordering the routine uses
when two symbols share a code length — and a table built from either wrong
guess round-trips against itself perfectly while decoding nothing a real
client sent. This is also why `d2huff.py` — the project's *second* Huffman
reader, which parses a `Fog.dll` file directly with `pefile` rather than
querying a live process — can only ever validate against a **runtime memory
image** of the DLL, never the shipped file: tested in this pass against the
genuine on-disk file, it fails cleanly with `"null fast-table entry for
0x58"`, for the reason above. The three-address match confirmed by
disassembly is real and independent; a static byte-for-byte table read from
the raw file is not possible, and no claim in this chapter rests on one.

### The table that *does* work: built by querying the live decoder

The code table the decoder actually ships, `HUFF_CODES` in `d2codec.py`, was
built a different way: by enumerating every one of the 2,048 possible 11-bit
prefixes and asking the real, running decoder what symbol each one yields.
That sidesteps the seed-and-reconstruct problem entirely — it treats the
live function as an oracle rather than trying to re-derive its internal
state — and it is why the table is explicitly documented as *not* guessed
from canonical ordering. All 256 symbols map with zero prefix conflicts,
which a genuinely broken reconstruction would not produce reliably across
the full alphabet.

### Watching the worked example decode, one symbol at a time

The compressed payload from §1 — `58 83 a0 35 10 cd 96 ac 3f c4 93 54 06 cb
40` — begins, as a bit string (MSB-first), `01011000 10000011 10100000...`.
The first code table lookup checks progressively longer prefixes against the
table until one matches. Symbol `0x6D` has the 7-bit code `0101100`, and the
first 7 bits of the stream are exactly `0101100` — a match, so the decoder
emits `0x6D` and consumes 7 bits, leaving one unconsumed bit (`0`) at the
tail of the first byte.

The next 7 bits are drawn from that leftover bit plus the first 6 bits of
byte two (`10000011`): `0` + `100000` = `0100000`. Symbol `0x04` has exactly
that 7-bit code. Second symbol: `0x04` — which, followed through to the end
of this chapter, turns out to be the low byte of a 4-byte unit id. The two
symbols already spell out the first two bytes of a `0x6D` packet
(`6d 04 ...`) before a fifth byte of input has even been touched.

Carried the rest of the way (confirmed by running the actual decoder, not
hand-traced further here), the full 15-byte payload decodes to 20 bytes:

```
6d 04 00 00 00 04 11 2e 16 80 6d 03 00 00 00 37 11 04 16 80
```

Two packets, back to back, out of one chunk — exactly the point §1 makes
about chunks and packets being different units. §3 explains how a reader
knows where the first one ends.

### The padding rule, and why it is not cosmetic

A compressed chunk is not always a whole number of bytes long in bits, so
the encoder pads the final byte. It pads with **zero** bits, and that choice
is load-bearing: symbol `0x00` has the single shortest code in the whole
table, one bit (`1`). Padding with `1` bits would complete that one-bit code
over and over, and the decoder would emit a run of phantom `0x00` bytes
after every chunk whose bit length does not land on a byte boundary — a
silent, self-consistent-looking corruption that a round-trip test would
never catch, because encoding those phantom bytes right back would
reproduce the same padded tail. Padding with zero bits cannot trigger any
code, because no valid code in the table is a run of fewer than eleven
zeros; a short zero tail simply runs the bit accumulator past 32 and the
loop's own termination check (`free > 32`, shown above) ends the chunk
cleanly. Measured against the fixture set: 269 of 277 fully-captured chunks
need no padding at all, and the other 8 end already byte-aligned — none of
the 341 pairs contradicts the rule.

---

## 3. How long is this packet?

A decompressed byte stream is just bytes; nothing in it says "packet ends
here" except a table, external to the stream, that maps each opcode to a
length. Diablo II keeps one such table for **each direction** — server→client
and client→server are sized independently, and the same opcode number means
different things, and has a different length, depending on which direction
it travels in. A reader that applies one table to both directions produces a
confident-looking parse of pure garbage; this is exactly how the client-side
sizes were first noticed to differ.

### The server→client table

`D2Net.dll`'s stock packet-size table sits at `D2Net.dll+0xA900`
(`6FBFA900`) — read directly from the file in this pass and matching the
decoder's transcription exactly for every index checked (0–45, 133, and
178–180, spanning the low opcodes, the PD2-only `0x85` slot discussed in §5,
and the table's final entries). It is an array of 181 signed 32-bit
integers, one per opcode `0x00`–`0xB4`; an opcode above `0xB4` is not a
recognized packet at all. A handful of representative entries, verified by
direct memory read against `D2Net.dll`:

| opcode | size (bytes) | meaning |
|---|---|---|
| `0x00` | 1 | (fixed) |
| `0x0D` | 13 | unit stopped |
| `0x0F` | 16 | unit walking |
| `0x15` | 11 | player reassign (server direction) |
| `0x59` | 26 | player record |
| `0x6D` | 10 | unit position |
| `0x85` | 0 | **unused in stock 1.13c** — see §5 |
| `0xB4` | 5 | (fixed, the table's final valid entry) |

A value of `0` means "not a recognized packet in this build"; a value of
`-1` means "variable — consult the special-case rule below." Fourteen
opcodes carry a `-1` and need one:

| opcode | length rule |
|---|---|
| `0x16` | `u16` at offset 1 |
| `0x26` | two NUL-terminated strings (admin/chat text) |
| `0x3E` | `u8` at offset 1 |
| `0x5B` | `u16` at offset 1 |
| `0x94` | `(u8@1 + 2) * 3` |
| `0x9C` | `u8` at offset 2 — see the item packet below |
| `0x9D` | `u8` at offset 2 |
| `0xA6` | `u16` at offset 2 |
| `0xA8` | `u8` at offset 6 |
| `0xAA` | `u8` at offset 6 — its own length byte, inside the "6 bytes" tail |
| `0xAC` | `u8` at offset 0xC |
| `0xAE` | `u16` at offset 1, capped, `+3` |
| `0xAF` | `u8` at offset 1, `+1` (or `2` if that byte is zero) |
| `0xB3` | `u8` at offset 1, `+7` |

`0xAA` is worth pausing on, because it looks at first like an ordinary
fixed-length record — README-level documentation of it reads `aa <u8 kind>
<u32 unit> <6 bytes>`, twelve bytes total — but the size table marks it
variable, and the rule (`u8` at offset 6) is not an external lookup at all:
it reads the packet's own seventh byte, the first byte of that trailing
"6 bytes," as an explicit self-declared length. In every observed capture
that byte reads `0x0C` (12), matching the fixed twelve-byte shape — so the
packet behaves as fixed-length in practice while formally carrying its own
length inline, a distinction that matters only if a future capture ever
shows a different value there.

### The client→server table is a second, separate array

`D2Net.dll` keeps a second 256-entry table for the opposite direction, at
`D2Net.dll+0xABD8` (`6FBFABD8`) — read in this pass directly beside the
first one, twelve bytes past where the server→client table's last valid
entry ends. Spot-checked against three of the plan's own measured wire
lengths, all matching exactly:

| opcode | server→client size | client→server size |
|---|---|---|
| `0x15` | 11 (player reassign) | variable (chat text) |
| `0x5B` | variable | 5 (roster ack) |
| `0x68` | — | 37 (client logon) |
| `0x6B` | — | 1 (enter game) |
| `0x6D` | 10 (unit position) | 13 (ping: `u32` tick, `u32` rtt, `u32` reserved) |

`0x6D` is the case worth remembering: the same opcode number is a 10-byte
unit-position update going one way and a 13-byte keepalive ping going the
other. Nothing about the byte itself distinguishes the two — only which
table a reader consults for it does.

### Resolving the worked example

The two-packet decompressed output from §2 —
`6d 04 00 00 00 04 11 2e 16 80 6d 03 00 00 00 37 11 04 16 80` — is a
server→client stream, so the relevant table is the first one.
`PACKET_SIZE[0x6D] = 10`, a fixed length. The first ten bytes,
`6d 04 00 00 00 04 11 2e 16 80`, are one complete packet; the next byte,
at offset 10, is `0x6D` again — the start of a second, identical-shaped
packet occupying the remaining ten bytes. Twenty decompressed bytes, two
ten-byte packets, zero leftover: exactly what the codec's `outlen` field
for this fixture records.

---

## 4. Reading the stream: the shapes a client sends

With framing (§1), decompression (§2) and length resolution (§3) in hand,
the bytes finally resolve into meaning. This section finishes the worked
example and gives a reference table for the packet shapes recovered so far —
recovered by a mix of capturing a real session and, for the ones whose
effect on rendering was not obvious from the capture alone, sending
hand-built variants at a live client through a control channel and watching
what actually moved. `tests/test_d2server.py` pins each builder to
reproduce the exact bytes a real capture contains, which is the sharper
check: a field in the wrong place still passes the length table, still
compresses, still frames — and then silently fails to render anything, with
no error to notice.

### Finishing the worked example

The first packet, `6d 04 00 00 00 04 11 2e 16 80`, decodes field by field
under the `0x6D` layout (opcode, `u32` unit id, `u16` x, `u16` y, one
trailing byte):

| bytes | field | value |
|---|---|---|
| `6d` | opcode | unit position |
| `04 00 00 00` | unit id (`u32` LE) | 4 |
| `04 11` | x (`u16` LE) | `0x1104` = 4356 |
| `2e 16` | y (`u16` LE) | `0x162e` = 5678 |
| `80` | trailing byte | `0x80` |

The second, `6d 03 00 00 00 37 11 04 16 80`:

| bytes | field | value |
|---|---|---|
| `6d` | opcode | unit position |
| `03 00 00 00` | unit id | 3 |
| `37 11` | x | `0x1137` = 4407 |
| `04 16` | y | `0x1604` = 5636 |
| `80` | trailing byte | `0x80` |

One compressed chunk, fifteen bytes on the wire, decoding to two ordinary
position updates for two nearby units — the workhorse packet of a live
capture: over a hundred of them appear in the reference session, for
players and monsters alike. Note what this packet does **not** do: measured
against a real client by sending hand-built variants and watching the
result, `0x6D` updates a unit's rendered position but produces **no walking
animation** — it teleports. A unit that should visibly walk needs `0x0F`
(started walking, carrying both a destination and the unit's current
position) followed later by `0x0D` (arrived, stopped at a position); `0x0D`
alone also teleports, with no interpolation, and `0x6D` is not a substitute
for either. This matters for anyone building a server: sending `0x6D` every
tick to "smoothly" move a remote unit does not produce smooth motion at all.

A second detail from the same capture work, useful when reading a stream
end to end: the **local player never receives position updates for its own
movement**. A player walks by sending the server a single client→server
`0x03 RUN_TO_XY <u16 x> <u16 y>` and animating the walk locally; the server
sends nothing back for it. Only *other* units' movement arrives as `0x0F` /
`0x0D` / `0x6D`. A capture of a session shows this cleanly: every server-sent
`0x0F` in the reference capture belongs to the other player in the session,
never to the connection's own unit.

### Packet shapes recovered so far

| opcode | direction | layout | notes |
|---|---|---|---|
| `0x03` | C→S | `03 <u16 x> <u16 y>` | RUN_TO_XY; the client moves itself, unacknowledged |
| `0x0D` | S→C | `0d 00 <u32 unit> 07 <u16 x> <u16 y> 00 64` | unit stopped here; no animation |
| `0x0F` | S→C | `0f 00 <u32 unit> 17 <u16 dstX> <u16 dstY> 00 <u16 curX> <u16 curY>` | unit started walking, with animation |
| `0x15` | S→C | `15 00 <u32 unit> <u16 x> <u16 y> 01` | "this is you" — sent for the local player |
| `0x20` | S→C | `20 <u32 unit> <u8 stat> <u32 value>` | stat update: `0x43`/`0x44` life/mana %, `0x0C` level, `0x00` str, `0x02` dex |
| `0x2F` | S→C | `2f <u32 item unit> <u8 container> <u8 page> <u8 col> <u8 row> <u8> <u8>` | item placement within a container it is already assigned to (see §5's cross-reference on what actually assigns a container) |
| `0x59` | S→C | `59 <u32 unit> <u8 class> <char name[16]> <u16 x> <u16 y>` | player record; the local player's own copy carries `(0, 0)` |
| `0x6D` | S→C | `6d <u32 unit> <u16 x> <u16 y> 80` | unit position, no animation |
| `0x6D` | C→S | `<u32 tick> <u32 rtt> <u32 0>` (13 bytes) | ping — same opcode number, unrelated shape |
| `0x75` | S→C | `75 <u32 unit> ff ff 01 00 00 00 00 00` | life/mana |
| `0x76` | S→C | `76 00 <u32 unit>` | unit complete |
| `0x9C` | S→C | `9c <u8 action> <u8 total_len> <u8 category> <u32 unit id> <item bit-stream>` | item data; the tail is the same bit-stream a `.d2s` save stores, without the `JM` marker |
| `0xAA` | S→C | `aa <u8 kind> <u32 unit> <6 bytes>` | kind `0` player, `1` monster/NPC — the unit id alone never says which |

`0xAA`'s kind byte is worth flagging on its own: it describes players and
NPCs through the same opcode, and the unit-id field carries no signal about
which. Filtering by id alone — for instance, to remove one specific player
from a scene — silently removes any town NPC whose id happens to collide,
with no symptom beyond a slightly emptier town.

The item packet, `0x9C`, is the densest entry here and deserves its
provenance stated plainly: its trailing bit-stream is **the same encoding a
`.d2s` save file uses** for one item record — see [The .d2s Save
File](d2s-save-format.md), whose §14 documents that record's own field
widths and version history — minus the `JM` two-byte marker
that only ever appears as a file-format delimiter — confirmed by comparing
a captured wire body against the corresponding item's bytes in the
character's own save file, where the two agree everywhere except the
mid-record bytes that carry the wire-only unit id and grid placement. This
means a server can serve real items by **slicing** a save file's item
records directly into `0x9C` bodies rather than re-encoding them from
scratch — and that a reader decoding `0x9C` off the wire is, byte for byte,
reading the same format a `.d2s` parser already knows.

---

## 5. What Project Diablo 2 changes at runtime

Everything in §§1–4 describes stock 1.13c. A modern client run against a
modern private server does not stay inside that description, and the gap is
not cosmetic: apply the stock size table alone to a Project Diablo 2 stream
and the packet walk **stops dead** at the first opcode PD2 has repurposed,
because stock 1.13c's own table marks it unused.

> **Mod note (Project Diablo 2):** PD2 patches twelve entries of the
> server→client size table at runtime, after the process is already
> running. None of the twelve exist as non-zero values in the shipped
> `D2Net.dll` — every one of them is `0` (unused) in the file on disk.

### The override table

| opcode | stock (file) | PD2 (live) |
|---|---|---|
| `0x17` | 0 | -1 (variable — **length rule not yet known**) |
| `0x2D` | 0 | 5 |
| `0x2E` | 0 | 18 |
| `0x2F` | 0 | 11 |
| `0x55` | 0 | 210 |
| `0x56` | 0 | 66 |
| `0x66` | 0 | 5 |
| `0x80` | 0 | 4 |
| `0x83` | 0 | 16 |
| `0x84` | 0 | 4 |
| `0x85` | 0 | 221 |
| `0x86` | 0 | 7 |

This is exactly twelve entries, not more: an earlier working list also
carried `0x0A`, `0x22`, `0x97`, `0xA5`, `0xA9`, and all five turned out to
repeat the stock value exactly — noise that made the real override set look
bigger than it is. `0x17` is recorded with no known length rule specifically
so that a reader meeting one recognizes a named gap rather than mistaking it
for a decoder bug.

`0x85` is a special case worth naming precisely, because an earlier
hypothesis about it did not survive contact with a stock D2GS: it is
**PD2's own client-side state blob**, not a gate any server must send to let
a client into a game. A stock 1.13c server that never emits a single `0x85`
packet has been measured holding a PD2 client in an active game for
5+ minutes with no symptom at all — so a reader implementing a server should
not treat `0x85`'s presence in this table as something the server side must
produce.

### Where the numbers come from, and what this pass could and could not confirm

The override values above are read out of a **live** PD2 client's own memory
— `D2Client.dll+0xDDE60`, an array of `{void* handler; int size; int
extra}` records at a 12-byte stride, one per opcode, which is the client's
own copy of what it will accept on the wire. That is a stronger source than
a network capture: a capture only shows opcodes that happened to be sent
during that session, so an opcode nobody used in the recorded traffic is
indistinguishable from one that does not exist at all — precisely how `0x17`
and `0x80` were missing from earlier attempts at this table through three
separate captures.

This pass independently confirmed the **mechanism**, not merely the
numbers. Reading the same array's location in the vanilla, on-disk
`D2Client.dll` (SHA-256 `dd8bc602…8836d906`) at `D2Client.dll+0xDDE60`
returns the stock table byte-for-byte for the entries checked (opcodes
`0x00`–`0x07` match the size table of §3 exactly, entry for entry). More to
the point: the two anti-cheat opcodes, `0x55` and `0x56`, read `size = 0` on
disk, pointed at the **same generic placeholder handler** the array uses for
opcode `0x00`'s slot — there is nothing distinguished about those two entries
in the shipped file at all. Only a live PD2 process shows the overridden
sizes (`210` and `66`) with real handlers behind them. That is a directly
observable version of the same lesson §2 draws about the Huffman tables: the
file on disk is an accurate record of what shipped, and an inaccurate record
of what the running process actually does with these two specific bytes of
opcode space.

`d2codec.py`'s own comment attributes the write to
`ProjectDiablo.dll FUN_102153D0`. That specific citation is **(unverified:
this pass)** — the address in question falls inside a larger function,
`FUN_10215240` (body `0x10215240`–`0x10215754`), whose disassembly around
that point is version-string comparison logic (calls into small
`std::string`-shaped helpers), not an obvious loop writing twelve constants
into a table. The fact of the runtime patch is independently confirmed by
the disk-vs-live comparison above; the exact function responsible is not,
and is left as an open item in the companion report rather than repeated
here as settled.

A candidate worth checking before that citation is chased further: [How Mods
Attach to Diablo II](mods-and-hooking-1.13c.md) counts `ProjectDiablo.dll`'s
own static patch tables independently of this chapter and finds exactly
**2 records targeting `D2Net.dll`** among the 1,290 total. That chapter does
not identify what those two records write, and this chapter does not claim
they are the size-table overrides above — the two findings were reached
separately and neither confirms the other — but a reader chasing the write
site has a concrete, small number of records to start from rather than all
612,009 instructions.

---

## 6. Proving a decoder is right, not merely self-consistent

Every claim in §§1–5 ultimately traces back to one discipline: measure the
real, running game, and check a reimplementation against what it actually
did — never against what a disassembly merely suggests it might do. This
section is about that discipline itself, because a decoder that only
round-trips against its own output proves nothing. A self-consistent but
wrong Huffman table decodes its own encoded output perfectly and renders
nothing on an actual client; §2's four wrong readings are the concrete
record of how easy that trap is to fall into.

### How the ground truth was captured

The method: breakpoint `Fog.dll` ordinal 10224 — the exact
`DecodeHuffmanBitStream` function confirmed by disassembly in §2 — inside a
live game, and record every `(input bytes, output bytes)` pair the function
actually produces, via `cdb`:

```
bp Fog+0x1EDA0 ".printf \"IN  src=%p srclen=%d dst=%p dstlen=%d\n\",
    poi(@esp+4), poi(@esp+8), @ecx, @edx; db poi(@esp+4) L28;
    r $t0 = @ecx; gu; .printf \"OUT len=%d\n\", @eax; db @$t0 L28;
    .echo ===PAIR===; gc"
```

`db ... L28` dumps `0x28` = 40 bytes of each buffer — enough to capture most
chunks whole, not enough for the largest ones, which is why the fixture set
below distinguishes "usable in this direction" from "captured but
truncated." Extracting these pairs from the raw debugger log was itself not
trivial: `cdb`'s `db` output does not start on its own line — the first
sixteen bytes of a dump get appended directly onto the end of the preceding
`.printf` line. A first attempt at parsing this log line-by-line silently
dropped bytes 0–15 of *both* buffers and produced pairs that were internally
consistent, sixteen bytes out of phase, and looked exactly like a decoder
bug rather than a log-parsing one. The fix — matching on the dump-line shape
anywhere in a block and slicing by where `"OUT len="` falls, rather than by
line boundaries — is recorded in `net/extract_codec_pairs.py`, itself a
small, worthwhile lesson: the instrument used to gather ground truth can
introduce its own artifacts, and those need the same skepticism as a static
read of the target.

### What the 341 pairs show

| check | result | condition |
|---|---|---|
| decode: real input → decoder's output matches real output | **284 / 284** | usable when the full compressed input fits the 40-byte capture window |
| encode: compressing the real output reproduces the real input | **277 / 277** | usable when *both* buffers fit the window |
| round-trip on synthetic data (all 256 byte values, four times over) | passes | sanity check independent of any capture |
| padding does not invent trailing bytes | passes | four specific inputs, including `0x2F` — the one symbol whose own code is eleven zero bits, the exact pattern zero-padding produces |

284 and 277 are both **below** 341 because the capture window (40 bytes)
does not fit every real chunk whole; a pair unusable in one direction is
excluded rather than partially graded, so these two figures are exact, not
lower bounds on a wider pool. The remaining pairs are still real captured
data — they are simply not decisive in the direction where they were
truncated.

Encoding is checked separately from decoding, and deliberately the harder
way: not "does compress(decompress(x)) == x" (a tautology any internally
consistent, wrong table would also satisfy) but "does compressing the
*game's own decompressed output* reproduce the *game's own compressed
input*, byte for byte." That is a claim about matching a specific external
implementation's behavior, not just about the reimplementation being
internally coherent with itself.

### End-to-end, beyond individual chunks

Beyond the paired fixtures, the full pipeline — dechunk, decompress every
chunk, resolve packet boundaries with the size table — was run over two
independent full-session captures:

| capture | chunks | bytes | packets | result |
|---|---|---|---|---|
| reference capture | 209/210 (one straddles a segment) | 35,814 | 1,794 | **100.0%**, zero leftover bytes |
| second, later capture (host side) | — | — | 1,790 | **100.0%** |
| second, later capture (joiner side) | — | — | 1,497 | **100.0%** |

The second capture is the stronger evidence of the two: it was recorded
*after* every table in this chapter was already fixed, and parsed with
nothing adjusted for it — a genuine held-out test, not a fit to the data
that produced the tables.

### What is verified against measurement, and what rests on the source's own comments

Per this book's citation rule, stated plainly rather than left implicit:

- **Independently reproduced in this pass, from the binaries:** the exact
  address and signature of `DecodeHuffmanBitStream` / ordinal 10224; the
  full bit-accumulation and termination algorithm, matched instruction for
  instruction against the Python reimplementation; the three Huffman table
  addresses; the `0x7B8` max-output constant at the call site; the
  server→client size table for the indices checked (0–45, 133, 178–180);
  the existence and location of the separate client→server size table and
  three of its entries; the on-disk (stock, unpatched) state of the
  `D2Client.dll` handler array for opcodes `0x00`–`0x07` and for the two
  anti-cheat slots `0x55`/`0x56`; and the fact that the Huffman fast/mask/
  code-length tables are absent (zero-filled) in the shipped file and built
  at runtime by `BuildPKWareHuffmanDecodeTables`.
- **Resting on the decoder's own fixtures and comments, not independently
  re-derived here:** the 341 capture pairs themselves and their 284/277
  usable counts (this pass re-ran the checks against the existing fixture
  file rather than re-capturing from a live game); the full contents of the
  packet-layout table in §4 (measured by the source project via a
  send-and-observe methodology against a live client, not by decompiling
  the client's rendering code); and the twelve numeric override values in
  §5's table beyond the two spot-checked directly.
- **Marked unverified in place:** the `ProjectDiablo.dll FUN_102153D0`
  citation for where PD2 writes its overrides (§5); the length rule for
  opcode `0x17`, which is not known to exist anywhere in the project.

---

## Version differences

This chapter describes the wire format as **1.13c**, the version the
decoder itself targets (`d2codec.py`'s own scope, stated in its docstring,
is "Diablo II 1.13c / PD2"). Two genuinely different `Fog.dll` and
`D2Net.dll` builds exist for earlier versions, confirmed by hash in this
pass, but their internal wire behavior was not measured for this chapter and
is not claimed here.

| What | 1.13c | 1.09d |
|---|---|---|
| Chunk framing (`0xAF 01` handshake, 1-/2-byte header) | as described in §1 | not measured |
| `Fog.dll` | 212,992 bytes, SHA-256 `96c05770…` | 188,463 bytes, SHA-256 `51b516cb…` — a different build |
| `D2Net.dll` | 49,152 bytes, SHA-256 `99af2f04…` | 53,297 bytes, SHA-256 `aea0ce29…` — a different build |
| Huffman ordinal / table addresses | ordinal 10224 at `Fog+0x1EDA0`, verified | not measured — no 1.09d `Fog.dll` was opened for this chapter |
| Server→client size table contents | as tabulated in §3 | not measured |
| PD2 runtime overrides | as tabulated in §5 | — (PD2 is a 1.13c mod only) |

Because the underlying binaries genuinely differ, nothing in §§1–5 should be
assumed to hold for 1.09d or earlier without separate verification; this
chapter neither confirms nor denies those versions' wire behavior beyond
what the table above states.

---

## Closing

Five readings of this stream were attempted before one was right, and the
four wrong ones share a shape worth naming on the way out of this chapter,
and this book: each was a plausible story that a disassembly *could* support,
told without checking it against what the code actually did while running.
"No compression" is a claim about bytes that stops being checkable the
moment nobody actually renders the decoded output and looks. "The codec is
in D2Net" and "the code-length table is a seed" are both claims about which
function does what, refuted the same way — by finding the one function that
is actually called, at the actual moment the actual bytes change shape, and
reading what it does. "Canonical ordering" is the subtlest of the four,
because the underlying construction algorithm genuinely exists in the
binary — this chapter found it, `BuildPKWareHuffmanDecodeTables`, sitting
one export past the decoder itself — and building the wrong table from it
was not a failure of imagination so much as an untested assumption about a
detail (which seed, which bucket order) that only the running program
actually settles.

What finally worked was not more careful reading. It was fewer inferences:
a breakpoint on the one function that matters, 341 real answers recorded
from a real game, and a Python reimplementation held to the standard of
reproducing them exactly rather than merely being internally consistent.
Every table in this chapter can be traced back to that discipline, and the
verification report below is the record of exactly how far that tracing
goes and where it stops.

See the companion audit,
[network-protocol.verification.md](network-protocol.verification.md), for
the full claim-by-claim inventory: what this pass checked, what it
corrected, and what is still open.
