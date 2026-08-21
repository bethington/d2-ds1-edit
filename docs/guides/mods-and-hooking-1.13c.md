# How Mods Attach to Diablo II

> **Provenance.** Original reverse engineering, performed 2026-08-21 against the
> retail Lord of Destruction 1.13c binaries in
> `F:\D2VersionChanger\VersionChanger\LoD\1.13c\` and against Project Diablo 2
> Season 13 as installed at `F:\D2Fleet\versions\pd2-s13\`. Static analysis in
> Ghidra: `D2Win.dll` (image base `6f8e0000`, SHA-256 `a9afb52d…334730fb`),
> `Storm.dll` (`6fbf0000`, `4b5fcaf8…a2da2d5b`), `D2Common.dll` (`6fd50000`,
> `59fa5928…83e06e10`), `D2Client.dll` (`6fab0000`, `dd8bc602…8836d906`),
> `Bnclient.dll` (`6ff20000`, `3631ca88…e76a56f5`), and
> `ProjectDiablo.dll` (`10000000`, `538a77b7…cfbf9cb3`). PE structure, byte
> diffs and import/export tables were parsed independently of Ghidra so that
> the two would have to agree. Where a claim rests on someone else's
> measurement rather than on the bytes, it says so inline.
>
> A companion audit records every claim, its verdict, and what remains
> unverified — see
> [mods-and-hooking-1.13c.verification.md](mods-and-hooking-1.13c.verification.md).

## The lie in the file

Project Diablo 2 ships a `D2Common.dll` that is byte-for-byte Blizzard's. Not
similar: identical. SHA-256 `59fa5928…83e06e10` on both the mod's copy and the
retail 1.13c copy, 679,936 bytes each, and the mod's own installer manifest
lists it at MD5 `ee1238806ef6d6d9801d12a09d128fe1` — the vanilla value —
as the file it expects to find.

That same `D2Common.dll` reads data tables a stock `D2Common.dll` cannot read.

Both statements are true, and the space between them is what this chapter is
about. You can copy PD2's `D2Common.dll` into a clean install, verify the hash,
open it in a disassembler, read the table loader instruction by instruction, and
write down what it does — and be wrong, because by the time that code runs,
something else has rewritten it in memory. The file on disk is an accurate record
of what shipped. It is not a record of what executes.

This is not a quirk of one mod. It is the normal condition of a modded Diablo II
process, and it is the reason a disassembler is a necessary but insufficient
tool here. What follows is an account of how code actually gets into a running
Diablo II — five distinct mechanisms, each measured on real binaries — and of
what a reader of those binaries has to do to avoid describing code that never
runs.

We will follow one concrete case the whole way: the arrival of
`ProjectDiablo.dll`, a four-megabyte module that no file in the game directory
imports. Somewhere in `D2Win.dll`, Project Diablo 2 changed five bytes. We will
start at those, and finish at the instruction that finally calls
`LoadLibraryA` — by which point it will be clear that the five bytes were the
*second* way in, and that the disk image has stopped being true several times
along the route.

---

## Why 1.13c

Every guide to modding Diablo II is a guide to 1.13c, and it is worth being
precise about why, because the reason is architectural rather than sentimental.

Retail Lord of Destruction 1.13c installs twenty-five PE files. Twenty of them
are game DLLs, each a separate module with its own export table, its own import
table, and its own preferred load address:

| Module | Preferred base | Size (bytes) | Exports |
|---|---|---|---|
| `D2Client.dll` | `6fab0000` | 1,093,632 | 4 (1 named) |
| `D2Game.dll` | `6fc20000` | 1,138,688 | 61, ordinal-only |
| `D2Common.dll` | `6fd50000` | 679,936 | 1,172 (2 named) |
| `D2CMP.dll` | `6fe10000` | 163,840 | 107, ordinal-only |
| `D2Win.dll` | `6f8e0000` | 147,456 | 207, ordinal-only |
| `D2Lang.dll` | `6fc00000` | 77,824 | 63 (49 named) |
| `D2Gfx.dll` | `6fa80000` | 77,824 | 88 (1 named) |
| `D2Net.dll` | `6fbf0000` | 49,152 | 38, ordinal-only |
| `D2Sound.dll` | `6f9b0000` | 98,304 | 71, ordinal-only |
| `D2Launch.dll` | `6fa40000` | 167,936 | 1 |
| `Fog.dll` | `6ff50000` | 212,992 | 268 (9 named) |
| `Storm.dll` | `6fbf0000` | 372,736 | 814, ordinal-only |
| `Bnclient.dll` | `6ff20000` | 139,264 | 23, all named |

(The full twenty-five, with the renderers and the media codecs, are in the
[reference tables](#module-map-lod-113c).)

Three properties follow from that layout, and all three are load-bearing.

**Every module boundary is a seam.** `D2Client` calls into `D2Common` through an
import table. `D2Common` calls `Storm` the same way. Those calls go through
indirection the operating system itself set up, which means they can be
redirected without touching either module's code — and, more to the point for
this chapter, a mod can replace or patch exactly one module and leave the other
nineteen untouched and verifiable.

**The bases are fixed and none of these modules opt into ASLR.** Every 1.13c
game DLL has `DllCharacteristics = 0x0000` — no `IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE`,
no `NX_COMPAT`. Absent a system-wide policy forcing relocation, `D2Client.dll`
loads at `6fab0000` on every machine, every launch. That is what makes an absolute
address like `6fd91e50` a durable, publishable fact rather than a session-local
artifact, and it is why two decades of community documentation is written in
absolute virtual addresses at all.

The exception is worth noting because it bites: **two 1.13c modules want the same
address.** `Storm.dll` and `D2Net.dll` both declare a preferred base of
`6fbf0000`, so one of them must relocate and its published addresses are *not*
stable in the way `D2Client`'s are. (Unverified: which of the two moves. `Storm`
is loaded first — everything imports it — which would make `D2Net` the loser, but
confirming that means reading a running game's module list, and this chapter's
research did not run the game.)

This is specific to 1.13c, and is the kind of thing worth checking per version
rather than assuming: on 1.09d the same two modules sit at `6ffb0000` and
`6fc00000` and do not collide at all.

**Almost nothing is exported by name.** `D2Common.dll` exports 1,172 functions and
names two of them. `D2Win.dll` names none of its 207. `Storm.dll` names none of
its 814. A mod that wants `D2Common`'s ordinal 10062 must ask for it by number,
and an ordinal is a promise that function *N* means a particular thing — a promise
that holds only for the exact build the number was read from. This single fact
generates a large fraction of the failure modes later in this chapter.

Then 1.14 happened.

| | 1.13c | 1.14d |
|---|---|---|
| PE files in the install | 25 | 7 |
| Game DLLs | 20 | 0 |
| `Game.exe` on disk | 61,440 bytes | 3,618,792 bytes |

Blizzard folded every game DLL into `Game.exe`. The twenty seams became zero. The
fixed bases became one base, and the exported ordinals became internal calls with
no external name of any kind. Every absolute address in every guide written before
2016 stopped meaning anything, and — because there is no longer a module you can
replace — the technique of "ship one patched DLL" stopped existing.

> **Version note (1.14 and later):** the install holds seven PE files and no game
> DLLs; everything is statically linked into a `Game.exe` of 3,618,792 bytes. A mod
> must patch that one image or nothing.

> **Version note (1.09d):** the same twenty-module architecture as 1.13c, and every
> base and every address differs — `D2Client.dll` sits at `6faa0000` and
> `D2Common.dll` at `6fd40000`. A patch written against 1.13c offsets is not merely
> wrong on 1.09d; it lands inside unrelated code.

So 1.13c is the last version with the architecture that makes modding tractable,
and it is the version the modding scene stayed on. Everything below is 1.13c
unless a callout says otherwise.

---

## Stage one: five bytes

Here is the whole of what Project Diablo 2 changes on disk.

The mod's game directory holds twenty-four files that also exist in a retail 1.13c
install. Twenty-two of them are byte-identical to Blizzard's — `D2Client.dll`,
`D2Common.dll`, `D2Game.dll`, `D2Net.dll`, `D2CMP.dll`, `Fog.dll`, `Game.exe`,
`Diablo II.exe`, every renderer, every codec. Two are not:

| File | Length | Verdict |
|---|---|---|
| `D2Win.dll` | 147,456 both | **247 bytes differ** |
| `Storm.dll` | 372,736 both | **20 bytes differ** |
| the other 22 | — | SHA-256 identical |

Both patched files are the *same length* as the originals. That is the first
tell: nobody recompiled anything. Somebody opened Blizzard's DLL in a hex editor —
or, more likely, ran a script — and changed bytes in place.

267 bytes. That is the mod's entire on-disk footprint in Blizzard's code. Everything
else Project Diablo 2 does, and it does a great deal, happens at runtime.

Of the 247 bytes in `D2Win.dll`, 234 are a bulk edit we will come back to. The
part that matters right now is three bytes at file offset `0xab47`:

```
vanilla   b8 01 00 00 00        MOV EAX, 1
PD2       e9 55 f7 00 00        JMP  +0xf755
```

Five bytes overwritten, three of them different. `D2Win.dll` has
`FileAlignment == SectionAlignment == 0x1000`, so file offset equals RVA
throughout, and this is `D2Win+0xab47` — virtual address `6f8eab47` once the
module is loaded at its preferred base.

Ask Ghidra what lives there.

```
6f8eaa20  FUN_6f8eaa20    (body ends 6f8eab4c)
...
6f8eab44  XOR EAX, EAX
6f8eab46  RET
6f8eab47  MOV EAX, 0x1        <-- the patch site
6f8eab4c  RET
```

The patched instruction is the *last* instruction of a function, and specifically
it is the success half of a two-exit epilogue. `XOR EAX,EAX; RET` is the failure
return. `MOV EAX,1; RET` is the success return. Project Diablo 2 did not patch the
function. It patched the moment the function succeeds.

Which function? The forty instructions before the epilogue answer that:

```
6f8eaa20  PUSH 0x0
6f8eaa22  PUSH 0x0
6f8eaa24  PUSH 0x6f8fd078            ; "D2EXPANSION"
6f8eaa29  PUSH 0x6f8fd06c            ; "d2data.mpq"
6f8eaa2e  PUSH 0x6f8fc9a8
6f8eaa33  MOV  EAX, 0x3e8
6f8eaa38  CALL 0x6f8e7e60            ; open archive
6f8eaa4b  MOV  [0x6f9a9ccc], EAX     ; keep the handle
...                                  ; and again, six more times
6f8eab0e  CALL 0x6f8e76ea
6f8eab13  TEST EAX, EAX
6f8eab15  JNZ  0x6f8eab47            ; -> success
6f8eab17  MOV  EAX, [0x6f9a9ccc]     ; d2data handle
6f8eab1c  TEST EAX, EAX
6f8eab1e  JZ   0x6f8eab44            ; -> failure
...
```

This is `D2Win`'s archive-mounting routine. It opens `d2data.mpq`, `d2sfx.mpq`,
`d2speech.mpq`, `d2delta.mpq`, `d2kfixup.mpq` and `d2exp.mpq` in sequence, stores
each handle in a global, checks that the essential ones came back non-null, and
returns 1 if the game has its data.

Project Diablo 2 hijacked the return.

The reasoning is visible in the choice of site. The mod needs to run before the
game does anything interesting, but *after* the game data is mounted — because
much of what it does depends on archives being open. "The instant the MPQ set
mounted successfully" is precisely that point, and it is expressible as five
bytes.

---

## Stage two: eighteen bytes of nowhere

`JMP +0xf755` from `6f8eab4c` lands at `6f8fa2a1`. In Blizzard's `D2Win.dll`,
that address holds this:

```
00 00 00 00 00 00 00 00 00 00 00 00 00
```

Thirteen zero bytes, in a run of them. `D2Win`'s `.text` section spans RVA
`0x1000`–`0x1afff` but its `VirtualSize` is only `0x192a1`; the rest is alignment
padding that the linker emitted and nothing ever reads. `6f8fa2a1` is 3,423 bytes
into that padding.

Padding inside `.text` is executable memory that the file already accounts for. A
patcher can write code into it without adding a section, without changing
`SizeOfImage`, and without changing the file's length — which is exactly why
`D2Win.dll` is still 147,456 bytes.

Here is what Project Diablo 2 wrote there:

```
6f8fa2a1  68 78 e3 8f 6f          PUSH 0x6f8fe378
6f8fa2a6  ff 15 08 b2 8f 6f       CALL dword ptr [0x6f8fb208]
6f8fa2ac  b8 01 00 00 00          MOV  EAX, 1
6f8fa2b1  c3                      RET
6f8fa2b2  90                      NOP
```

Eighteen bytes, and every one of them is doing something.

`0x6f8fe378` is RVA `0x1e378`, which in Blizzard's file is seventeen more zero
bytes — this time in the tail of `.rdata`, whose `VirtualSize` (`0x336e`) again
falls short of its allocated `0x4000`. Project Diablo 2 wrote a string there:

```
50 72 6f 6a 65 63 74 44 69 61 62 6c 6f 2e 64 6c 6c 00
 P  r  o  j  e  c  t  D  i  a  b  l  o  .  d  l  l
```

`0x6f8fb208` is the interesting one. It is RVA `0x1b208`, and parsing `D2Win`'s
import directory places it inside the `KERNEL32.dll` import address table, which
begins at RVA `0x1b1b0`: slot 22, which is **`KERNEL32.dll!LoadLibraryA`**.

Project Diablo 2 did not add an import. It did not touch the import directory at
all. It found a `LoadLibraryA` slot that Blizzard's `D2Win.dll` already had and
called through it.

Why did a windowing DLL already import `LoadLibraryA`? Ghidra answers that too.
The only other reference to `6f8fb208` in the whole module is at `6f8e679b`,
inside a function Ghidra names `___crtMessageBoxA`:

```
6f8e6796  PUSH 0x6f8fc228          ; "user32.dll"
6f8e679b  CALL dword ptr [0x6f8fb208]
```

Microsoft's C runtime library dynamically loads `user32.dll` when it needs to
display a fatal-error message box. That is the entire reason `LoadLibraryA`
appears in `D2Win`'s imports, and it is what Project Diablo 2 borrowed. A
compiler-emitted CRT helper from 1998, present in the binary purely to display an
error nobody ever sees, is load-bearing for a mod written twenty-five years later.

And then `MOV EAX, 1; RET` — the instruction that was overwritten, replayed
faithfully, so the archive-mounting function still returns exactly what
`D2Win`'s caller expects. The `NOP` is padding to a round eighteen.

That is the seam. Blizzard's loader mounts the game archives, reports success, and
in doing so loads a four-megabyte mod it has never heard of. From the operating
system's point of view nothing unusual happened: a DLL called `LoadLibraryA`.

> **Mod note (Project Diablo 2):** no file in the game directory statically
> imports `ProjectDiablo.dll` — parsing the import descriptors of all thirty-two
> PE files in the S13 game folder finds zero references. It is reached only by
> `LoadLibraryA`, and this stub is not the only caller: `PD2_EXT.dll` has one
> too. [Both paths are traced below](#stage-four-the-second-way-in).

---

## Stage three: the second patch in the same function

Return to that archive-mounting routine, because we skipped 234 bytes.

Every one of `D2Win`'s MPQ-open calls takes a filename. In Blizzard's binary those
filenames are ordinary `.rdata` strings — `d2data.mpq`, `d2char.mpq`, and so on —
and the `PUSH` instructions that pass them carry the string's absolute address as
a 32-bit immediate.

Project Diablo 2 wrote twelve replacement strings into the same `.rdata` padding
that holds `ProjectDiablo.dll`, and then retargeted seventeen `PUSH` operands to
point at them. Because the replacements sit at `6f8fe3xx` and the originals at
`6f8fcfxx`–`6f8fd0xx`, only the low half of each pointer changes — which is why
the byte diff shows seventeen tidy two-byte edits:

| Patch site | Original operand | New operand | Original string | Replacement |
|---|---|---|---|---|
| `6f8ea48b` | `6f8fcfc8` | `6f8fe3dc` | `d2char.mpq` | `..\\d2char.mpq` |
| `6f8ea4bb` | `6f8fcfc8` | `6f8fe3dc` | `d2char.mpq` | `..\\d2char.mpq` |
| `6f8ea846` | `6f8fcf80` | `6f8fe39a` | `d2Xvideo.mpq` | `..\\d2Xvideo.mpq` |
| `6f8ea868` | `6f8fcf6c` | `6f8fe38a` | `d2video.mpq` | `..\\d2video.mpq` |
| `6f8ea8d9` | `6f8fcfc8` | `6f8fe3dc` | `d2char.mpq` | `..\\d2char.mpq` |
| `6f8ea8f5` | `6f8fcfac` | `6f8fe3cc` | `d2music.mpq` | `..\\d2music.mpq` |
| `6f8ea919` | `6f8fcf9c` | `6f8fe3bb` | `d2Xmusic.mpq` | `..\\d2Xmusic.mpq` |
| `6f8ea939` | `6f8fcf90` | `6f8fe3ab` | `d2Xtalk.mpq` | `..\\d2Xtalk.mpq` |
| `6f8ea959` | `6f8fcf80` | `6f8fe39a` | `d2Xvideo.mpq` | `..\\d2Xvideo.mpq` |
| `6f8ea9ac` | `6f8fcfc8` | `6f8fe3dc` | `d2char.mpq` | `..\\d2char.mpq` |
| `6f8ea9c7` | `6f8fcfac` | `6f8fe3cc` | `d2music.mpq` | `..\\d2music.mpq` |
| `6f8eaa2a` | `6f8fd06c` | `6f8fe439` | `d2data.mpq` | `..\\d2data.mpq` |
| `6f8eaa47` | `6f8fd058` | `6f8fe42b` | `d2sfx.mpq` | `..\\d2sfx.mpq` |
| `6f8eaa69` | `6f8fd03c` | `6f8fe41a` | `d2speech.mpq` | `..\\d2speech.mpq` |
| `6f8eaa8b` | `6f8fd028` | `6f8fe40a` | `d2delta.mpq` | `..\\d2delta.mpq` |
| `6f8eaaad` | `6f8fd008` | `6f8fe3f9` | `d2kfixup.mpq` | `..\\d2kfixup.mpq` |
| `6f8eaaf1` | `6f8fcfd4` | `6f8fe3eb` | `d2exp.mpq` | `..\\d2exp.mpq` |

Seventeen sites, twelve distinct archives. (The literal bytes are `2e 2e 5c 5c` —
two dots and *two* backslashes. Windows collapses the doubled separator, so
`..\\d2data.mpq` and `..\d2data.mpq` resolve identically; the doubling is almost
certainly a C string literal that was never un-escaped.)

Notice what was *not* patched. Each open call passes two strings — the archive's
filename and an internal key such as `D2EXPANSION` or `PATCH_D2`. Only the
filename operand moves. The keys are untouched.

The effect is that Project Diablo 2's copy of `D2Win` looks for Blizzard's base
archives one directory *above* the game folder. The installed tree matches
exactly: `pd2-s13/game/` holds the executables and PD2's own archives, while
`pd2-s13/` — one level up — holds `d2data.mpq`, `d2char.mpq`, `d2exp.mpq` and the
rest of Blizzard's set. The mod is built to live inside a stock Diablo II
installation and reach up into it.

This finding reproduces an independent measurement. The d2-fleet project measured
the same patch on 2026-08-15 from the opposite direction — trying to run PD2 in a
container — and recorded "twelve hard-coded strings PD2 appended to D2Win's
`.rdata` plus seventeen `push` operands retargeted at them"
(`d2-fleet/docs/REMOTE.md:139-144`), and put the total delta at "PD2's 247" bytes
(`REMOTE.md:145-152`). Twelve, seventeen, and 247: three numbers arrived at by a
different route on a different day, all three matching the byte diff above.

---

## Attachment, mechanism by mechanism

The five bytes in `D2Win` are one of five ways code gets into a Diablo II process,
and they are worth laying out together before we follow `ProjectDiablo.dll` any
further. Two of the five happen on disk before the process exists. Three happen
while it runs.

### 1. Patch the shipped file

Edit Blizzard's DLL in place and let the loader do the rest. This is what Project
Diablo 2 does to `D2Win.dll` and `Storm.dll`, and the four techniques it uses
between them are the standard vocabulary:

**Tail-patch a function's epilogue.** Overwrite an instruction with a `JMP` to
slack space, do the work, replay the overwritten instruction, return.
`D2Win+0xab47`, above.

**Retarget a pointer operand.** Leave the code alone; change what it points at.
The seventeen `PUSH` edits, above. Cheapest possible patch — often two bytes —
and invisible to any analysis that only reads mnemonics.

**Neuter a branch.** `Storm.dll` at `6fc19f82`:

```
vanilla   0f 84 17 01 00 00        JZ  0x6fc1a09f
PD2       e9 18 01 00 00 90        JMP 0x6fc1a09f ; NOP
```

Same target, six bytes to six bytes, conditional to unconditional. The
surrounding function opens the file `(attributes)` inside an MPQ — the string is
at `6fc370d8` — and, if it is present, parses it: 4 bytes per file when flag bit
0 is set, 8 bytes per file when bit 1 is, written into per-file records of stride
`0x1c`. Those are the CRC32 and FILETIME arrays of StormLib's `(attributes)`
file. With the branch forced, the function returns 0 and neither array is ever
populated. (What PD2 gains by this is not established from the binaries alone —
only that per-file CRC32s and timestamps stop being loaded.)

**Flip a constant.** Eight single-byte edits in `Storm.dll`, all the same edit:

```
vanilla   6a 04    PUSH 4      ; PAGE_READWRITE
PD2       6a 40    PUSH 0x40   ; PAGE_EXECUTE_READWRITE
```

At `6fbf8607`, `6fbfcfc1`, `6fbfd044`, `6fc06f6a`, `6fc0a574`, `6fc0be13`,
`6fc0c308` and `6fc24808`. In every case the pushed value is the `flProtect`
argument of a `VirtualAlloc` call. Seven are followed within four instructions by
`CALL dword ptr [0x6fc33230]`, which Storm's import directory resolves to
`KERNEL32.dll!VirtualAlloc`; the eighth (`6fc0be13`) sits in `FUN_6fc0bdd0`,
which loads that same slot into a register on entry —
`6fc0bdd7  MOV EDI, dword ptr [0x6fc33230]` — and calls `EDI`. All eight are
`VirtualAlloc`. Project Diablo 2 makes Storm's allocations executable. (Measured;
the motive is not established from the binaries.)

Twenty bytes, eleven sites, and the memory allocator of a 1998 archive library now
returns executable pages.

### 2. Rename an import descriptor

The second on-disk technique is subtler, and `Storm.dll` carries the only example
in this install. Its sixth import descriptor names a DLL, and the name string sits
at file offset `0x4e63a`:

```
vanilla   56 45 52 53 49 4f 4e 2e 64 6c 6c        VERSION.dll
PD2       50 44 32 5f 45 58 54 2e 64 6c 6c        PD2_EXT.dll
```

Seven characters replaced with seven characters. The descriptor's thunk arrays,
its ordinals, its three imported names — `GetFileVersionInfoA`,
`GetFileVersionInfoSizeA`, `VerQueryValueA` — are all unchanged. Only the module
name moved, and it had to be exactly seven characters to fit, which explains the
otherwise inexplicable name `PD2_EXT`.

The Windows loader now resolves those three imports out of `PD2_EXT.dll`, which
sits in the game directory. And `PD2_EXT.dll` exports:

| Export | Target |
|---|---|
| `GetFileVersionInfoA` | forwards to `version.GetFileVersionInfoA` |
| `GetFileVersionInfoSizeA` | forwards to `version.GetFileVersionInfoSizeA` |
| `GetFileVersionInfoSizeW` | forwards to `version.GetFileVersionInfoSizeW` |
| `GetFileVersionInfoW` | forwards to `version.GetFileVersionInfoW` |
| `VerFindFileA` / `VerFindFileW` | forward to `version.*` |
| `VerInstallFileA` / `VerInstallFileW` | forward to `version.*` |
| `VerLanguageNameA` / `VerLanguageNameW` | forward to `version.*` |
| `VerQueryValueA` / `VerQueryValueW` | forward to `version.*` |

Twelve named exports, every one of them a PE *export forwarder* — a string in the
export directory naming another module, which the loader chases so the caller
never notices. `PD2_EXT.dll` reimplements nothing. Its entire visible API is a
redirection back to the real `version.dll`.

Which means its purpose is not the exports at all. It is a **proxy DLL**: a module
whose only job is to be loaded. The full VERSION.dll surface exists so that
nothing breaks; the payload is in `DllMain`, which the loader runs on the way in.

This is the same class of trick as the `D2Win` stub, arrived at from the other
direction. `D2Win` was taught to call `LoadLibrary`. `Storm` was taught to name a
different dependency. Both put a module in the process; neither required a new
import descriptor or a longer file.

The difference is *when*. `D2Win`'s stub fires when the game mounts its archives,
which is well into startup. `Storm.dll` is imported directly by `Game.exe`, so
`PD2_EXT.dll` is a second-order dependency of the executable itself — the Windows
loader maps and initialises it **before `Game.exe`'s entry point runs at all**.
Nothing of the game has executed yet, and a mod is already in the address space
with a `DllMain` to spend. What it spends it on is [the last stage of this
chapter's worked example](#stage-four-the-second-way-in).

### 3. Let the search order do it

The third disk-side technique requires no patching whatsoever: give a file the
name of a DLL the game already asks for, and put it where the loader looks first.

`D2DDraw.dll` and `D2Direct3D.dll` — both byte-identical to Blizzard's — import
`DDRAW.dll`. `D2Glide.dll`, also identical, imports `glide3x.dll`. Project Diablo 2
ships a `ddraw.dll` and a `glide3x.dll` in the game directory, and the standard
Windows search order checks the application directory before `System32`. The
game's own unmodified renderers load the wrappers instead of the system
components, and nothing anywhere had to be edited.

The one thing that can defeat this is `KnownDLLs`, the registry list of modules
the loader always takes from the system directory regardless of what sits beside
the executable. Checking
`HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs` on this machine
returns thirty-eight entries — `kernel32`, `user32`, `gdi32`, `advapi32`,
`shell32`, `ole32`, `ws2_32`, and so on. `ddraw` is not among them. The shadow
works.

This is also why `Diablo II.exe` and `Game.exe` matter as *locations*: the entire
technique depends on the process's image directory, so a mod that runs the game
from somewhere else silently loses its wrappers.

### 4. Rewrite the import table at process creation

Now the runtime mechanisms, and the first of them is what the d2-fleet project
uses to attach its own instrumentation to twenty-eight different Diablo II
builds.

Microsoft Detours exposes `DetourCreateProcessWithDllsW`, which creates the target
process **suspended**, rewrites its import table in memory to add an import of a
payload DLL, and resumes it. Because the edit lands in the import directory before
the process's first instruction executes, the payload's `DllMain` runs ahead of
the game's entry point:

```c
DetourCreateProcessWithDllsW(wexe.c_str(), cmdbuf.data(), nullptr, nullptr,
                             FALSE, CREATE_DEFAULT_ERROR_MODE, nullptr,
                             wdir.c_str(), &si, &pi, 1, dlls, nullptr)
```
— `d2-fleet/fleet/fleet_launch.cpp:648-650`

The ordering is the point, and d2-fleet's own comment says why:

> "creates the target SUSPENDED, writes the hook into it, and resumes -- so
> D2FleetHook.dll's DllMain runs before the game's own startup. That ordering is
> the whole point: the single-instance check and the registry isolation both have
> to be in force before the game asks its first question."
> — `fleet_launch.cpp:15-19`

Compare this with what Project Diablo 2 does. Both end with an extra module in the
process. But the Detours route requires control of process *creation*, and it
leaves the files on disk untouched; the `D2Win` route requires no control of
launch at all — double-clicking the game is enough — and it leaves a permanent
mark in Blizzard's DLL. A mod distributed to players cannot assume it launches the
game. An instrumentation harness can.

### 5. Patch from inside, once you are already there

The last mechanism is the one all the others exist to enable: a module that is
already in the address space rewriting the modules around it.

The primitive is four calls:

```c
VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
memcpy(at, bytes, n);
VirtualProtect(at, n, old, &ignore);
FlushInstructionCache(GetCurrentProcess(), at, n);
```
— `d2-fleet/patch/patchcore.cpp:629-640`

Mark the page writable, overwrite the instruction stream, restore the protection,
and invalidate the CPU's instruction cache so the processor does not execute a
stale copy of what used to be there. Every runtime patch in every Diablo II mod is
some version of this.

What varies is discipline — [below](#the-discipline-problem).

### The one that is not on this list

Classic DLL injection — `OpenProcess`, `VirtualAllocEx`, `WriteProcessMemory`,
`CreateRemoteThread` on `LoadLibraryA` — is the technique most people reach for
first, and it is absent from everything examined for this chapter. Neither
Project Diablo 2 nor d2-fleet uses it.

Diablo II is part of the reason. The game restricts access to its own process, and
d2-fleet measured the consequence from the outside: an unelevated console asking
for `PROCESS_TERMINATE` on a member it launched itself is refused with error 5,
"measured against explorer.exe and the shell itself in the same breath, both of
which open fine, so it is this process specifically and not the caller's rights in
general" (`d2-fleet/patch/patchcore.cpp:1174-1181`). The same self-imposed DACL is
what defeats attaching a debugger.

Injecting at process creation sidesteps the DACL entirely, because the import
table is rewritten before the game runs the code that installs it.

---

## Stage four: the second way in
<a id="stage-four-the-second-way-in"></a>

`PD2_EXT.dll` is 86,528 bytes, and its imports give away that it is not a version
shim. Alongside the CRT it pulls in `VirtualProtect`, `GetModuleHandleA`,
`LoadLibraryA`, `GetProcAddress`, and — from `USER32.dll` — exactly one function,
`MessageBoxA`. Its strings give away the rest:

```
0x11c38  ProjectDiablo.dll
0x11c4c  Error: Failed to load ProjectDiablo.dll
0x11c78  Failed to load ProjectDiablo.dll. Please check your Anti-Virus logs
         and restore the file.
0x11d20  Please reinstall Project Diablo 2 into a valid Diablo 2 LoD directory.
0x11d68  Fog.dll
0x12148  C:\projects\Project-Diablo-2\scripts\build-pd2ext-release\PD2_EXT.pdb
```

That last one is the developer's own build tree, shipped by accident in the
release binary, the way such things always are.

The module's `AddressOfEntryPoint` is RVA `0x1590`, the CRT's
`_DllMainCRTStartup`; the user `DllMain` is `FUN_100011c0`, reached from the
function Ghidra labels `dllmain_dispatch` at `100014c5` and `100014dd`. It
identifies itself unambiguously besides: it tests `dword ptr [EBP+0xc]` against
`1` — `fdwReason == DLL_PROCESS_ATTACH` — returns `MOV EAX, 1`, and ends
`RET 0xc`, three stdcall arguments.

On process attach it does exactly one thing:

```
100011e6  PUSH 0x4082dd
100011eb  CALL dword ptr [0x1000e000]        ; VirtualProtect(0x4082dd, 5,
                                             ;   PAGE_EXECUTE_READWRITE, &old)
100011f1  MOV  byte ptr [0x004082dd], 0xe8   ; force a CALL opcode
100011f8  MOV  EAX, 0x10001080               ; ...to our function
100011fd  MOV  ESI, dword ptr [0x004082de]   ; save the ORIGINAL rel32
10001203  SUB  EAX, 0x4082e2                 ; rel32 = target - (site + 5)
10001208  MOV  [0x004082de], EAX
1000121b  CALL dword ptr [0x1000e000]        ; restore protection
10001221  LEA  EAX, [ESI + 0x4082e2]         ; reconstruct the original target
10001227  MOV  [0x100162d8], EAX             ; keep it
```

`0x004082dd` is inside **`Game.exe`**. In Blizzard's 1.13c executable that address
holds `e8 fe fb ff ff` — `CALL 0x00407ee0`. `PD2_EXT.dll` overwrites the call
target with its own function and stores `0x00407ee0` in a global so it can chain
back to it later.

Five bytes, written into the executable's code, before the executable has run a
single instruction of its own.

And note what `DllMain` does *not* do. It does not call `LoadLibrary`. It does not
create a thread. It does not wait on anything. Every one of those is forbidden or
unreliable under the loader lock, and this `DllMain` is eleven instructions long
precisely because everything else has been deferred to a point where the game's
own code will call into the mod.

### The deferred stage

That point arrives when `Game.exe` reaches `0x004082dd` and calls `10001080`
instead of `0x00407ee0`. The loader lock is long released. Now `PD2_EXT` can work,
and it applies three more patches, all through the same `VirtualProtect` /
overwrite / restore sequence with `EDI` holding the `VirtualProtect` import:

**One.** Thirty-three bytes of `Game.exe` at `0x004083ef`, overwritten with `0x90`:

```
100010f4  PUSH 0x4083ef
100010f9  CALL EDI                              ; VirtualProtect(.., 0x21, 0x40, ..)
100010fb  MOV  EAX, 0x4083ef
10001100  MOV  dword ptr [EAX],      0x90909090
10001106  MOV  dword ptr [EAX + 4],  0x90909090
          ... eight dwords ...
10001137  MOV  byte  ptr [EAX + 0x20], 0x90
```

Blizzard's bytes there are:

```
ff 24 85 38 84 40 00     JMP dword ptr [EAX*4 + 0x408438]
c6 44 24 61 01  eb 13    MOV byte [ESP+0x61], 1 ; JMP short
c6 44 24 5f 01  eb 0c    MOV byte [ESP+0x5f], 1 ; JMP short
c6 44 24 5e 01  eb 05    MOV byte [ESP+0x5e], 1 ; JMP short
c6 44 24 5c 01           MOV byte [ESP+0x5c], 1
```

Exactly thirty-three bytes: a jump-table dispatch and four arms, each setting a
different boolean on the stack. All of it becomes `NOP`, so none of the four flags
can ever be set. (What those four flags control is **unverified** — establishing it
means identifying the enclosing function, which this research did not do.)

**Two.** A second call-site splice in `Game.exe`, at `0x0040763f`, which vanilla
holds as `e8 82 fe ff ff` — `CALL 0x004074c6`, followed by `TEST EAX,EAX` and a
conditional jump. The new target is `10001000`, and the original `0x004074c6` is
saved to `[0x100162d4]`.

**Three.** A splice into `Fog.dll` — and this one checks first:

```
1000114b  PUSH 0x10012f68                    ; "Fog.dll"
10001150  CALL dword ptr [0x1000e004]        ; GetModuleHandleA
10001156  TEST EAX, EAX
10001158  JZ   0x100011a6                    ; not loaded -> give up
1000115a  CMP  dword ptr [EAX + 0xff5f], 0x5e0cc483
10001164  JNZ  0x100011a6                    ; wrong bytes -> give up
10001166  CMP  dword ptr [EAX + 0xff63], 0xc314c483
10001170  JNZ  0x100011a6                    ; wrong bytes -> give up
10001172  LEA  ESI, [EAX + 0x17ea7]          ; only now, the patch site
```

Two four-byte signatures at `Fog+0xff5f` and `Fog+0xff63`. Read Blizzard's
`Fog.dll` at `0xff5f` and it contains `83 c4 0c 5e 83 c4 14 c3` — which is
`0x5e0cc483` followed by `0xc314c483`, little-endian, exactly. The signature is
`ADD ESP,0xC; POP ESI` / `ADD ESP,0x14; RET`: two function epilogues, chosen
because they are stable, distinctive, and cheap to compare.

This is a **version guard**, hand-written into a mod, doing precisely what
d2-fleet's `D2Patch_GuardBytes` was later built to formalise: verify the bytes at
the address before writing, and on a mismatch do nothing rather than corrupt an
instruction stream. Project Diablo 2 got there first and independently.

Guard passed, it splices `Fog+0x17ea7` — vanilla `e8 a8 4f ff ff`,
`CALL 0x6ff5ce54` — to call `10001050` instead.

Then, at `100011a6`, `CALL dword ptr [0x100162d8]`: the original
`Game.exe` function the whole chain displaced, called at last, so the game's own
control flow continues as if nothing had happened.

### And finally the module loads

`10001000` — the function now wired into `Game.exe` at `0x0040763f` — is
seventeen instructions:

```
10001000  PUSH ESI
10001001  CALL dword ptr [0x100162d4]        ; the ORIGINAL 0x004074c6
10001007  MOV  ESI, EAX
10001009  TEST ESI, ESI
1000100b  JZ   0x10001032                    ; original failed: do nothing
1000100d  PUSH 0x10012e38                    ; "ProjectDiablo.dll"
10001012  CALL dword ptr [0x1000e008]        ; LoadLibraryA
10001018  TEST EAX, EAX
1000101a  JNZ  0x10001046                    ; loaded: carry on
1000101c  PUSH EAX                           ; uType = 0
1000101d  PUSH 0x10012e4c                    ; "Error: Failed to load ProjectDiablo.dll"
10001022  PUSH 0x10012e78                    ; "...check your Anti-Virus logs..."
10001027  PUSH EAX                           ; hWnd = NULL
10001028  CALL dword ptr [0x1000e128]        ; MessageBoxA
1000102e  XOR  EAX, EAX
10001030  POP  ESI
10001031  RET
```

Call the original. If it succeeded, load the mod. If the mod will not load, say so
and blame the antivirus — which, given that the mechanism is a renamed import
descriptor and a hand-written code splice, is a reasonable guess.

So the full chain, from a file on disk to a mod in control:

| # | Where | What |
|---|---|---|
| 0 | `Storm.dll` file offset `0x4e63a` | `VERSION` → `PD2_EXT`, 7 bytes |
| 1 | Windows loader | `Game.exe` imports `Storm`; `Storm` imports `PD2_EXT` |
| 2 | `PD2_EXT!DllMain` | splice `Game.exe+0x82dd` → `10001080`, 5 bytes |
| 3 | `Game.exe` runs, reaches `0x4082dd` | `10001080` gets control, loader lock released |
| 4 | `10001080` | NOP 33 bytes at `Game.exe+0x83ef` |
| 5 | `10001080` | splice `Game.exe+0x763f` → `10001000` |
| 6 | `10001080` | guard `Fog.dll`, splice `Fog+0x17ea7` → `10001050` |
| 7 | `10001080` | chain to the displaced original |
| 8 | `Game.exe` reaches `0x40763f` | `10001000`: `LoadLibraryA("ProjectDiablo.dll")` |

Seven bytes on disk, and by step eight a four-megabyte mod owns the process. Every
address in that table was read out of the shipped binaries; every "vanilla"
instruction quoted was read out of Blizzard's.

And the second path — `D2Win`'s archive-mount tail-patch from Stages one through
three — does the same `LoadLibraryA` from a different place at a different time.
Whichever fires first maps the module; the other gets a reference-count increment
and the same handle back. Two independent ways in, for a mod that would be dead if
either failed.

---

## What is now true of the process, and false of the files

`ProjectDiablo.dll` is mapped. It is a 4,312,576-byte module with a 3.2 MB `.text`
section, 12,398 functions, and a `.reloc` section of 296,960 bytes — fully
relocatable, which it needs to be, because its preferred base of `0x10000000`
collides with both `binkw32.dll` and `SmackW32.dll`. Three modules in this process
want `0x10000000` and only one can have it.

That relocation is the first thing that distinguishes analysing `ProjectDiablo.dll`
from analysing `D2Client.dll`. Blizzard's modules load where the file says.
`ProjectDiablo.dll` does not: the d2-fleet project measured it landing at
`0x033f0000` on a live member (`d2-fleet/docs/PD2-ONLINE.md:98-99`), a delta of
`0x0F3F0000` applied to every absolute address in the module.

Meanwhile `Game.exe` and `Fog.dll` — both byte-identical to Blizzard's, both
verifiable against a retail install, both perfectly legible in a disassembler —
are running code that is not in either file. Three sites in `Game.exe` have been
rewritten, forty-three bytes in all; one site in `Fog.dll`, five bytes.

And `Bnclient.dll` is a third case, which is worth stating because it is the same
thesis at a place that matters commercially. `Bnclient.dll` is byte-identical to
Blizzard's (MD5 `f0e1caaf1ee073802714e6f88f4700b2`, the same value in PD2's own
manifest). At file offset `0xf954` it contains:

```
e8 07 27 00 00        CALL +0x2707      -> Bnclient+0x12060
85 c0                 TEST EAX, EAX
0f 84 6b ff ff ff     JZ   <bail>
```

`Bnclient+0x12060` is Blizzard's own builder for the `SID_AUTH_CHECK` packet — the
message that tells Battle.net which game version you are running. Read the disk
image and the story is complete and coherent.

In the live process the `rel32` at `0xf955` does not read `07 27 00 00`. It reads
`c7 55 7a 93`, and the call goes to `ProjectDiablo+0x2e4f20`
(`d2-fleet/docs/MEMORY-IMAGES.md:13-19`). Blizzard's builder never executes at all,
and the evidence for *never* is elegant: the real builder's prologue
unconditionally writes a client-token accumulator at `Bnclient+0x1f930`, and in a
live PD2 process that global is still zero (`PD2-ONLINE.md:80-92`).

Project Diablo 2 even keeps its own record of the edit, in a structure at
`ProjectDiablo.dll+0x3d1708`: `mod=0x15` (Bnclient), `offset=0xf955`,
`value=0x102e4f20`, `isRel32=1` (`PD2-ONLINE.md:88-90`).

Read that address in the file. It lies in `.data`, which spans `10398000`–
`104eb103`, and in the shipped image it holds nothing but zeros — 192 bytes of them
around `103d1708`. The record of the patch, like the patch, exists only while the
process is running.

That record can still be checked, sideways. `mod=0x15` is 21, and 21 is an index
into a table of module names in `ProjectDiablo.dll`'s `.data` at `0x103c68a0`.
Reading entry 21 of that table gives a pointer to `103770b0`, and the bytes there
are `42 00 6e 00 63 00 6c 00 69 00 65 00 6e 00 74 00 2e 00 64 00 6c 00 6c 00` —
`Bnclient.dll`, UTF-16. (Entry 0 is `D2Client.dll`, at `10376ebc`.) A number read
out of a live process in one investigation and a table read out of a shipped file
in another agree on a detail neither could have guessed.

So: `D2Common.dll` is byte-identical to Blizzard's, and a stock `D2Common.dll`
cannot read PD2's tables. Both true. The bytes on disk are Blizzard's; the
instructions that execute are not; and nothing in any file records the difference.

> **Note (verification status).** The `Bnclient` runtime detour and the patch
> record are the d2-fleet project's measurements against a live PD2 process, not
> this chapter's — this research deliberately did not run the game. What is
> verified here is the target side: the disk bytes at `Bnclient+0xf954` are
> `e8 07 27 00 00`, whose `rel32` resolves to `Bnclient+0x12060` = VA `6ff32060`,
> exactly the "on disk" row of that measurement; and `103d1708` is zero-filled in
> the shipped `ProjectDiablo.dll`. The `PD2_EXT` chain in the previous section, by
> contrast, is verified end to end from the shipped binaries alone — as is
> everything in the section that follows.

---

## Inside `ProjectDiablo.dll`

`PD2_EXT.dll` patches four sites. `ProjectDiablo.dll` patches **1,290**.

They are not written as code. They are three tables in `.data`, walked by two
functions — and the tables are the interesting part, because a mod that expresses
its patches as data rather than as instructions can be counted, and this one can
be counted exactly.

| Table | Address | Record stride | Records | Bytes written |
|---|---|---|---|---|
| A | `0x103c7f50` | `0x14` | 1,198 | 5,061 |
| B | `0x10367ee0` | `0x14` | 35 | 128 |
| C | `0x103681b0` | `0x40c` | 57 | 168 |

**1,290 records, 5,357 bytes**, into modules Project Diablo 2 did not compile:

| Target | Records | Bytes |
|---|---|---|
| `D2Game.dll` | 704 | 2,989 |
| `D2Client.dll` | 485 | 1,940 |
| `D2Common.dll` | 84 | 364 |
| `D2Win.dll` | 7 | 25 |
| `Bnclient.dll` | 4 | 27 |
| `D2Lang.dll` | 3 | 3 |
| `D2Net.dll` | 2 | 2 |
| `D2Launch.dll` | 1 | 7 |

Nothing patches `Fog`, `Storm`, `D2CMP`, `D2Gfx`, `D2Sound`, `BH.dll`, or the
resolution mods, though all are in the module table.

The record layout, read out of the walker `FUN_102ad020`, is five fields in twenty
bytes: module index, RVA within that module, value, a rel32 flag, and a length.
Module index `0x1b` terminates the walk (`102ad03a: CMP EAX,0x1b`); so does a zero
RVA. When the rel32 flag is set, the value is converted from an absolute target to
a displacement:

```
102ad07f  MOV EAX, 0xfffffffc
102ad084  SUB EAX, EDI
102ad086  ADD ECX, EAX          ; disp = target - (writeAddress + 4)
```

`+4`, not `+5` — because the record's RVA points at the *operand* of an `E8`, one
byte past the opcode. All 800 rel32 records in table A are call-site splices of
exactly the shape `PD2_EXT` used four times.

### The primitive, and the thing it is missing

```
102ad0b1  PUSH 0x40                        ; PAGE_EXECUTE_READWRITE
102ad0b5  CALL dword ptr [0x10323040]      ; VirtualProtect
102ad0c8  CALL 10272d50                    ; copy the bytes in
102ad0e0..102ad121                         ; inlined memcmp: did they land?
102ad130  CALL dword ptr [0x10323040]      ; VirtualProtect, restore
```

Protect, write, **verify**, restore. The `memcmp` is a nice touch — the function
returns 0 if the bytes did not take, which is more than most hand-rolled patchers
bother with.

What is absent is `FlushInstructionCache`. It is not called, and it is not
imported: enumerating all 325 external symbols of `ProjectDiablo.dll` finds
neither it nor `WriteProcessMemory`. On x86 this is survivable — the architecture
keeps instruction and data caches coherent for self-modifying code on the same
processor — but it is the one point where PD2's patcher is less careful than
d2-fleet's, which flushes (`patchcore.cpp:639`).

### No absolute addresses, anywhere

The obvious way to write 1,290 cross-module patches is to hardcode
`0x6fab____`, `0x6fc2____`, `0x6fd5____`. Searching all **612,009 instructions**
of `ProjectDiablo.dll` for immediates in those ranges returns **zero matches**.
(The one hit for `0x6fd` is `102eac68: PUSH 0x6fd` — the integer 1,789.)

Instead there is a table at `0x103c68a0`: twenty-seven `{ LPCWSTR name; HMODULE
handle; }` pairs, filled at startup by `FUN_102ad400`:

```
102ad401  MOV  EBX, dword ptr [0x10323054]   ; LoadLibraryW
102ad409  MOV  EDI, dword ptr [0x1032305c]   ; GetModuleHandleW
102ad40f  MOV  ESI, 0x103c68a0
102ad414  PUSH dword ptr [ESI]               ; the name
102ad416  CALL EDI                           ; already loaded?
102ad41a  JNZ  102ad420
102ad41c  PUSH dword ptr [ESI] / CALL EBX    ; no: load it
102ad420  MOV  dword ptr [ESI+0x4], EAX      ; keep the handle
102ad423  ADD  ESI, 0x8 / CMP ESI, 0x103c6978 / JL
```

The patch records store the *index* into that table, and the walker computes
`base + RVA` at `102ad05f`. Relocation, a different Windows build, a forced-ASLR
policy — none of it matters.

Function references work the same way, through a resolver at `FUN_102f10f0`:

```
102f1168  CALL dword ptr [0x1032305c]   ; GetModuleHandleW
102f1173  CALL dword ptr [0x10323054]   ; LoadLibraryW fallback
102f1183  TEST ESI, ESI / JNS 102f1197  ; ESI = the selector
102f1187  NEG  ESI / PUSH ESI / PUSH EAX
102f118b  CALL dword ptr [0x10323028]   ; GetProcAddress(hMod, ordinal)
102f1197  ADD  EAX, ESI                 ; ...or just base + RVA
```

A negative selector means "export ordinal *−n*"; a positive one means "RVA". There
are **1,959 call sites** into this resolver, and every one loads its selector from
an indexed table rather than an immediate — `MOV EDX, dword ptr [EAX*4 + <table>]`.
Of them, 677 target `D2Client`, 594 `D2Game`, 507 `D2Common`.

The index is a **game version**. `FUN_10253f30` reads `Game.exe`'s file version
through the `VERSION.dll` imports, formats it with `"%d.%d.%d.%d"`, and compares
it against exactly two strings:

```
10253f4d  MOV EDX, 0x1037c434    ; "1.0.13.60"  -> index 0
10253f63  MOV EDX, 0x1037c440    ; "1.0.13.64"  -> index 1
10253f97  ... MessageBoxW(L"Game version not detected or is unsupported!")
10253fb7  CALL dword ptr [0x10323494]   ; exit(0)
```

Project Diablo 2 supports two builds of Diablo II and refuses to run on anything
else. Every selector table in the mod is two entries wide, and 1.13c is entry
zero.

This is the ordinal problem, solved from the opposite end. d2-fleet answered "an
ordinal only means the right thing on one build" by importing no game function at
all. PD2 answered it by tabulating every ordinal per supported build and narrowing
"supported" to two.

### Everything, under the loader lock

`ProjectDiablo.dll`'s `DllMain` is `FUN_102adad0`, and it handles one reason code:

```
102adad3  SUB  dword ptr [EBP+0xc], 0x1   ; reason - 1
102adad7  JNZ  102adaec                    ; anything but PROCESS_ATTACH: return
102adad9  CALL 102ad560                    ; <-- all of it
102adade  TEST EAX, EAX
102adae2  MOV  ECX, 0x1037df74             ; "Couldn't attach to Diablo II"
102adae7  CALL 102acf90                    ; MessageBoxA + TerminateProcess
```

There is no `DLL_PROCESS_DETACH` handling at all. And `FUN_102ad560` — called
synchronously, on the loader's thread, with the loader lock held — resolves
twenty-seven modules, mounts four MPQ archives, applies all 1,290 patches, runs
seventeen subsystem initialisers, and spawns three threads through
`_beginthreadex`.

Set that beside `PD2_EXT.dll`, whose `DllMain` is eleven instructions and defers
everything to a call site in `Game.exe`. Two modules in the same mod, by the same
project, taking opposite positions on the most-warned-about rule in Windows
programming. What separates them is *when* they run: `PD2_EXT` initialises as a
dependency of `Storm`, while other modules are still loading and the lock is
genuinely contended; `ProjectDiablo` arrives via an explicit `LoadLibraryA` from
code that is already executing.

### The archives, and a function both halves of this chapter found

The four MPQ mounts sit at `102ad5f0`, `102ad60d`, `102ad62a` and `102ad647`, all
through one thunk:

```
102ad5d8  PUSH 0 / PUSH 0
          PUSH 0x1037dee4        ; "PD2DATA"
          PUSH 0x1037ded8        ; "pd2data.mpq"
          PUSH 0x1037decc        ; "D2Win.DLL"
          PUSH 0x1770            ; 6000
102ad5f0  CALL 102eeba0
```

The thunk moves the first argument into `EAX` and calls through a pointer that
resolves — via the version table, selector `0x7e60`, positive, so base + RVA — to
**`D2Win+0x7e60`**.

That address should look familiar. It is `6f8e7e60`: the function called seven
times by `FUN_6f8eaa20` back in [Stage one](#stage-one-five-bytes). D2Win's own
archive-open routine, which is not exported and has no name.

The two sides match on every argument. Blizzard's calls pass
`PUSH 0; PUSH 0; PUSH <key>; PUSH <filename>; PUSH 0x6f8fc9a8`, priority in `EAX`
— and reading `D2Win` at `0x6f8fc9a8` gives the string **`D2Win.DLL`**, the same
value PD2 passes in the same position. The priorities line up too:

| Caller | Archive | Priority in `EAX` |
|---|---|---|
| `D2Win` `6f8eaa38` | `d2data.mpq` | `0x3e8` = 1,000 |
| `D2Win` `6f8eab04` | last of the base set | `0xbb8` = 3,000 |
| `D2Win` `6f8eaae2` | `d2exp.mpq` | `0x1388` = 5,000 |
| **`ProjectDiablo` `102ad5f0`** | `pd2data.mpq` | **`0x1770` = 6,000** |

One notch above everything Blizzard mounts. Project Diablo 2 puts its own four
archives — `pd2data`, `pd2maps`, `pd2assets`, `pd2monchars` — at the top of the
search order by calling a private function inside a DLL it located by RVA, with a
number chosen to win.

That is how the data gets replaced. It is not a hook on the table reader.

### The check that had to die

Which returns us to the question this chapter opened with: why can a stock
`D2Common.dll`, byte-identical to Blizzard's, not read PD2's tables?

Of the 84 `D2Common` records, one is thirty bytes of `0x90` at
**`D2Common+0x82cb5`** — virtual address `6fdd2cb5`. Here is what Blizzard put
there, read out of the retail file:

```
6fdd2ca4  e8 97 c2 fd ff        CALL 6fdaef40                     ; load the table
6fdd2ca9  a3 5c fa de 6f        MOV  [0x6fdefa5c], EAX            ; keep the pointer
6fdd2cae  83 3d 58 fa de 6f 20  CMP  dword ptr [0x6fdefa58], 0x20 ; count must be 32
6fdd2cb5  74 1c                 JZ   6fdd2cd3                     ; <-- NOPs START
6fdd2cb7  6a 70                 PUSH 0x70                         ; line 112
6fdd2cb9  e8 5e 65 f8 ff        CALL 6fd5921c                     ; assert reporter
6fdd2cbe  50                    PUSH EAX
6fdd2cbf  68 28 a7 dd 6f        PUSH 0x6fdda728                   ; source filename
6fdd2cc4  e8 4d 65 f8 ff        CALL 6fd59216
6fdd2cc9  83 c4 0c              ADD  ESP, 0xc
6fdd2ccc  6a ff                 PUSH -1
6fdd2cce  e8 3a ee f7 ff        CALL 6fd51b0d                     ; terminate
6fdd2cd3  81 c4 b4 05 00 00     ADD  ESP, 0x5b4                   ; <-- NOPs END
6fdd2cd9  c2 04 00              RET  4
```

`0x6fdd2cd3 − 0x6fdd2cb5 = 0x1e` = 30. The patch covers the conditional jump and
the entire abort block, to the byte.

The function is `LoadInventoryTable` at `6fdd20a0`, and it is genuinely that: the
`.rdata` immediately below the assert data is the column list of `inventory.txt` —
**seventy-two names** running from `glovesHeight` at `6fdda3e4` to `invLeft` at
`6fdda720`, through `feetLeft`, `beltWidth`, `torsoTop`, `gridX`, `gridY`,
`invRight` and the rest, with the terminator `end` sitting just above them at
`6fdda3e0`. The global at `6fdefa58` is the record count, written by this function
at `6fdd2b19` and read by four accessors elsewhere in the module.

So: stock `D2Common` loads `inventory.txt`, and if the table does not hold exactly
thirty-two records it reports an assertion at line 112 and terminates the process.
Project Diablo 2 ships an `inventory.txt` that does not hold thirty-two records.
Thirty bytes of `0x90`, written into memory at load, are the difference between a
game and an `exit(-1)`.

**And this sharpens the claim the chapter started from.** The usual formulation —
"PD2 patches D2Common's table loader" — is not what happens. `D2Common`'s loader
is untouched: its path strings (`DATA\GLOBAL\EXCEL` at `6fdddbf0`, `%s\%s.bin` at
`6fde154c`) are unpatched, and none of the 84 records land in that code. The reader
stays stock, reads through Storm, and gets PD2's file because PD2's archive is
mounted at priority 6,000. What had to be patched was not the reader but the
**hardcoded expectation about what it would find** — a number, `0x20`, compiled
into Blizzard's binary and true right up until somebody added an inventory row.

The other 83 `D2Common` records are variations on the theme. Some retarget an
existing `CALL`: nineteen of them redirect `6fd88a80`, a stat accessor, to a PD2
trampoline at `102edd50`. Some manufacture a call where none existed, using two
records — one writes the `E8` opcode, the next writes the displacement
(`D2Common+0x1a2e2` and `+0x1a2e3`). And one, at `D2Common+0x2e9c1`, writes three
bytes into the middle of an immediate: stock `6fd7e9c0` is
`MOV EAX, 0x2625a0; RET 4` — 2,500,000 — and the patch rewrites bytes one through
three to make it `0x004c4b40`, **5,000,000**. A gameplay cap doubled, expressed as
three bytes at a known offset.

### Why an injected DLL is detectable

One more thing lives in `ProjectDiablo.dll`, and it is why these techniques are
adversarial as well as technical.

The mod imports exactly three functions from `libcrypto-1_1.dll`: `SHA256_Init`,
`SHA256_Update`, `SHA256_Final`. `FUN_102e7710` hashes a file from disk in
`0x8000` chunks; `FUN_102e7660` hashes a string. Two walkers use them:

```
102e7aed  CALL dword ptr [0x10323070]   ; K32EnumProcessModules
102e7b24  CALL dword ptr [0x103230b0]   ; K32GetModuleFileNameExW
102e7bfc  CALL 102e7830                 ; is this module on the list?
102e7c15  CALL 102e7710                 ; if so, SHA-256 its file on disk
102e7ca1  CALL 102e81f0                 ; sort
102e7d49  CALL 102e7660                 ; hash the joined result
```

The list is six base64 strings, decoded at runtime:

| Stored at | Literal | Decodes to |
|---|---|---|
| `1037f264` | `cHJvamVjdGRpYWJsby5kbGw=` | `projectdiablo.dll` |
| `1037f280` | `ZDJoYWNrbWFwLmRsbA==` | `d2hackmap.dll` |
| `1037f298` | `ZDJtZS5kbGw=` | `d2me.dll` |
| `1037f2a8` | `ZDJjbXAuZGxs` | `d2cmp.dll` |
| `1037f22c` | `ZDJtYXBoYWNrLmRsbA==` | `d2maphack.dll` |
| `1037f244` | `ZDJjbGllbnQuZGxs` | `d2client.dll` |

Two well-known map hacks, by name, hidden from a strings dump by base64 and
nothing else.

The enumeration is PSAPI, not toolhelp: `CreateToolhelp32Snapshot` and
`Module32First` are not imported, and there is no PEB walk. Both walkers are
reached only from `FUN_102cf530`, which is never called directly — it is passed to
`_beginthreadex` at `102e6f03` as a thread body. **The integrity scan runs on a
background thread**, unlike everything else the mod does.

The consequence for anyone injecting into a PD2 client is simple. A module in the
process is in `K32EnumProcessModules`; a module in `K32EnumProcessModules` has a
path on disk; a path on disk has a SHA-256. This is why d2-fleet's hook goes
through Windows API seams rather than patching the game — and why that project is
careful to note that its own detours appear in any memory image taken from one of
its members (`docs/MEMORY-IMAGES.md:105-111`).

---

## Two rules a hook DLL cannot break

If you write a DLL to be injected into Diablo II, two build settings are not
preferences. Both were established by measurement in the d2-fleet project, both
fail silently, and both failures name something other than the actual cause.

### It must link the CRT statically

> "Measured 2026-08-09: the first build linked the DYNAMIC runtime, and
> LoadLibrary on the hook returned error 126 (ERROR_MOD_NOT_FOUND) because the
> x86 MSVCP140/VCRUNTIME140 redistributable is not installed on this machine. The
> symptom in the fleet was far worse than the cause: the launch reported success
> and a pid, the game exited seconds later, and NOTHING was logged -- because
> DllMain never ran to start the logger."
> — `d2-fleet/CMakeLists.txt:18-26`

The failure has no diagnostic surface at all. `DetourCreateProcessWithDllsW`
succeeds. A process ID comes back. The process dies. The hook's own logging never
starts, because the code that starts it lives in the `DllMain` that never ran.

Project Diablo 2 makes the opposite choice and gets away with it: parsing
`ProjectDiablo.dll`'s imports shows `MSVCP140.dll`, `VCRUNTIME140.dll`, and ten
`api-ms-win-crt-*` stubs. The mod requires the Visual C++ redistributable and
tells its users so. A DLL that is injected into other people's processes on
machines it has never seen cannot make that assumption.

(On this machine, as of 2026-08-21, `msvcp140.dll` and `vcruntime140.dll` are both
present in `C:\Windows\SysWOW64`, so PD2's dynamic imports resolve. The d2-fleet
measurement dates from when they were not.)

### It must export something

This is the stranger of the two, and the error message is actively misleading.

> "DetourCreateProcessWithDlls injects by rewriting the target's IMPORT TABLE to
> import from this DLL. An import descriptor has to name something to import, so
> a payload DLL with an empty export directory produces a malformed table and the
> loader rejects the whole image.
>
> Measured 2026-08-09, and the symptom named nothing that was wrong: Game.exe put
> up "The application was unable to start correctly (0xc000007b)" --
> STATUS_INVALID_IMAGE_FORMAT, which every search result attributes to mixing
> 32- and 64-bit binaries. Every file involved was verified x86, and the same
> staged game launched perfectly with no hook. The real fault was this DLL's
> export directory being rva=0 size=0, versus 344 bytes in D2Debugger.dll, which
> injects fine."
> — `d2-fleet/fleet/hook_main.cpp:87-100`

`0xc000007b` is `STATUS_INVALID_IMAGE_FORMAT`, and the internet has decided
unanimously that it means a 32/64-bit mismatch. Here it means the payload has no
export directory, so the import descriptor Detours synthesised has nothing to
name.

The fix is one function whose only requirement is to exist:

```c
extern "C" __declspec(dllexport) const char* D2FleetHook_Version(void)
{
	return "d2-fleet hook 1.0";
}
```
— `hook_main.cpp:104-107`

There is no `.def` file anywhere in that repository; a single `__declspec(dllexport)`
is the whole mechanism. And note the symmetry with `PD2_EXT.dll`, whose twelve
exports are all forwarders — and with `ProjectDiablo.dll`, whose export directory
contains exactly one ordinal. Three different projects, three different reasons, and
all three ship a module whose exports exist mainly so that the export directory is
not empty.

### And a third rule, about ordinals

Not a build setting, but the same class of trap.

`D2Common.dll` names two of its 1,172 exports. Everything else is a number, and
a number means different things in different builds. d2-fleet's hook deliberately
imports nothing from any game DLL, and the comment explains what the alternative
costs:

> "CONTRAST WITH D2MOO's D2Debugger.dll, which cannot be used here: it imports
> D2COMMON.dll ordinals 10562 and 10332 (verified against the built binary). An
> ordinal is an address-free promise that function #N means a specific thing, and
> that promise only holds for the ONE build the ordinals were taken from"
> — `hook_main.cpp:15-19`

And the failure is worse than a wrong function. On 2026-08-11 a fleet member
served nothing on its control port for over twenty hours. The cause:

> "SGD2FreeRes fails an ordinal lookup at startup (`Could not locate exported
> ordinal 10991 from D2Common.dll`, GetProcAddress error 0x7F -- it is built
> against a different D2Common than the one staged) and puts up a MODAL
> MessageBox from loader context. That dialog never closes in a container, so the
> loader lock is held forever."
> — `hook_main.cpp:147-153`

A missing ordinal became a message box, the message box was raised from inside
`DllMain`, and the loader lock it held stopped every subsequently created thread
in the process from ever entering its start routine. `CreateThread` returned valid
handles. The threads never ran. Nothing logged an error, because the code that
would have logged it was one of the threads.

> **Version note (Classic below 1.09d, and several builds above):** on these,
> `SGD2FreeRes` returns `FALSE` from its own `DllMain` rather than failing an
> ordinal lookup, which surfaces to the caller as `LoadLibrary` error 1114
> (`ERROR_DLL_INIT_FAILED`) — the mod declining the build, not a fault in the
> install. Measured across a 44-version sweep by the d2-fleet project
> (`patch/patches.resmod.cpp:110-121`).

This is the single most important thing to understand about `DllMain`: it runs
under the loader lock, and almost everything interesting is forbidden there.
`LoadLibrary` deadlocks. Waiting on another thread deadlocks. Creating a thread
*appears* to work and produces a thread that cannot start until the lock is
released. d2-fleet resolves this by splitting installation into declared stages —
`EARLY` inside `DllMain` for things that must beat the game's first question,
`MODULES` on a worker thread once the game's DLLs are mapped, `RUNTIME` for
anything togglable (`d2-fleet/patch/d2patch.h:38-54`).

---

## The discipline problem
<a id="the-discipline-problem"></a>

Writing four bytes into another module's instruction stream takes one line. Doing
it in a way that fails loudly when the assumptions are wrong takes considerably
more, and the difference is the whole distance between a mod that works and a mod
that produces crashes indistinguishable from game bugs.

The d2-fleet project's `patch/` directory is the most explicit treatment of this
available in the Diablo II ecosystem, because it turns the informal practice into
a declared one. Every behaviour change is a descriptor:

```c
typedef struct D2Patch
{
	const char*     id;        // stable identifier: config key, HTTP id, log key
	const char*     category;  // grouping for the panel ("Dev", "Debugging", ...)
	const char*     summary;   // one line; shown in the panel and in /patch/list
	int             defaultOn; // used only when the config has no opinion
	D2PatchStage    stage;
	D2PatchApplyFn  Apply;
	D2PatchRevertFn Revert;    // null => irreversible
} D2Patch;
```
— `d2-fleet/patch/d2patch.h:83-92`

Four properties of that shape are worth extracting, because each answers a
specific way runtime patching goes wrong.

**"When" is part of a patch's identity.** The `stage` field has three values, and
they map onto the loader's constraints exactly: `EARLY` runs inside `DllMain`,
where nothing may load a library, create a usable thread, or wait; `MODULES` runs
on a worker thread once the game's DLLs are mapped; `RUNTIME` runs any time
(`d2patch.h:38-54`). A patch that must beat the game's first question has to be
`EARLY` and a patch that needs a game module to exist cannot be, and a system
that does not model this produces patches that report success and do nothing.

**Refusal is a state, not an error.** The apply contract is that a patch returns 0
and writes a human-readable reason, and that reason is surfaced rather than
swallowed:

> "Apply -- return 1 on success. Return 0 to REFUSE, and write a one-line human
> reason into `why` (never left empty on a refusal; the reason is what the
> operator reads in the panel when a patch does nothing)."
> — `d2patch.h:73-77`

A patch that returns 0 without a reason is itself flagged, as
`"refused without a reason (patch bug)"` (`patchcore.cpp:821`).

**Prefer the API seam to the address.** This is the rule that most directly
concerns a mod author:

> "TARGETING ORDER: prefer an API seam, fall back to module+rva. An API seam is
> version-independent -- the same hook works on the stock Game.exe and on a
> reimplementation whose RVAs differ entirely -- so a patch that can be written
> against one MUST be."
> — `d2patch.h:13-17`

The concrete case is instructive. To make a PD2 client connect to a private realm,
the obvious move is to edit the hard-coded address literal inside
`ProjectDiablo.dll`. d2-fleet chose a detour on `ws2_32!connect` instead, and
listed the costs of the obvious move: "it edits a catalog entry that other members
hardlink, PD2's launcher re-downloads that file, and the fix would be per-season"
(`patch/patches.realm.cpp:28-34`). A byte patch against a mod is a patch against a
*version* of that mod.

**Verify the bytes before writing them, and report both sides.** When an address
patch is unavoidable, the guard reads what is actually there first, and its error
message is designed to distinguish two different problems that look identical:

> "Both sides, always. 'bytes did not match' sends you to Ghidra; 'found
> E9 xx xx xx xx' tells you somebody else already detoured this site, which is a
> completely different problem from a wrong game build."
> — `patchcore.cpp:588-591`

The one shipped patch that does target game code — a detour on Bnclient's CD-key
loader at `Bnclient+0x162B0` — refuses on a prologue mismatch with a message that
names what it found: *"Bnclient+0x%X is not the 1.13c key loader (prologue %02x
%02x %02x); refusing to detour an unknown function"*
(`patch/patches.cdkey.cpp:154-163`).

### Two patches that reported success and did nothing

The most useful record in that repository is of a technique that failed, twice,
and looked healthy both times. The goal was to rewrite PD2's twelve `..\` archive
paths in memory so the mod could run from a flat directory.

At the `MODULES` stage it was too late. That stage runs on a deferred thread after
a 1.5-second sleep, and `D2Win` has opened every archive long before. The patch
"reported `state: active`, rewrote all twelve paths, and the member still died on
'Failed to load game data files' — an applied patch that changed nothing"
(`d2-fleet/docs/PD2-ONLINE.md:820-830`).

Moved to `EARLY` as a Detours interception of `GetFileAttributesW`/`CreateFileW`,
it "committed cleanly, logged, reported `state: active`, and **never fired
once**: the paths reaching Wine still contained `..` and the redirect count stayed
zero" (same passage). The detour was installed on the address the hook's own
import resolved to, which under Wine was not the function the game actually
called.

Two failures, two different causes — a correct patch applied after the fact it was
meant to change, and a correct patch installed on the wrong copy of a function —
and both reported `active`. Neither is detectable by asking the patch system
whether it worked. Both are detectable by counting whether the hook ever fired,
which is why the second attempt logged a redirect count at all.

The lesson generalises past this repository: **a patch's own report of success is
not evidence that it changed anything.** Instrument the effect, not the
application.

---

## Telling a patched runtime from a stock one

You have a Diablo II install and a question: is this thing stock?

**Hashing the files is necessary and not sufficient.** It works, and it is the
first thing to do — it is how this chapter established that twenty-two of PD2's
twenty-four Blizzard files are untouched and two are not. But it answers only the
disk question. A process into which something was injected at launch has an
entirely clean install behind it.

**Compare the mapped image against the file.** This is the real test, and it is
the technique the d2-fleet project settled on after, in its own words, believing
the disk image "cost this project a day and produced three confident wrong
diagnoses" (`docs/MEMORY-IMAGES.md:3-8`). Read each module's pages out of the
running process, fix two header fields, and load the result into a disassembler
beside the disk copy:

> 1. **`PointerToRawData = VirtualAddress`** for every section, and
>    `FileAlignment = SectionAlignment`. In a mapped image section data sits at
>    its RVA, not at the file offset the on-disk header advertises.
> 2. **`OptionalHeader.ImageBase` = the actual load address.** Relocations have
>    already been applied in memory, so a relocated module only makes sense at the
>    address it was relocated to.
> — `MEMORY-IMAGES.md:46-57`

Same address, two answers. That is the whole diagnostic.

Three traps that project recorded, each of which will otherwise produce a
confidently wrong result:

- **Do not derive module ranges from `/proc/<pid>/maps`.** "PD2 patches itself, so
  its pages become private/anonymous and **lose the filename** — `ProjectDiablo.dll`
  shows exactly one 4 KB file-backed region out of 5.4 MB" (`MEMORY-IMAGES.md:74-78`).
  A self-modifying module stops looking like a file to the kernel.
- **Re-read the module list after every relaunch.** Relocated modules move, and a
  dump taken against stale bases is silently wrong (`MEMORY-IMAGES.md:81`).
- **Verify the dump contains a patch you already know about**, or you may have
  imaged the file rather than the process. For PD2 the cheapest check is the
  `Bnclient` detour: at file offset `0xf955` the memory image must read
  `c7 55 7a 93` where the disk copy reads `07 27 00 00` (`MEMORY-IMAGES.md:84-88`).

**Fingerprint the module cheaply.** If all you need is "is this the build I think
it is", d2-fleet folds two header fields together:

```c
return nt->FileHeader.TimeDateStamp ^ nt->OptionalHeader.SizeOfImage;
```
— `patchcore.cpp:534`

with the reasoning that "Timestamp alone collides across incremental relinks of
the same second; SizeOfImage alone collides across unrelated builds. Together they
are a cheap, allocation-free build identity" (`patchcore.cpp:529-532`).

**Read the bytes before you write them, and say what you found.** The most useful
diagnostic in that project's patch system is not a hash at all — it is the
requirement that a patch which targets an address verify the instruction bytes
first and report both sides on a mismatch:

> "Both sides, always. 'bytes did not match' sends you to Ghidra; 'found
> E9 xx xx xx xx' tells you somebody else already detoured this site, which is a
> completely different problem from a wrong game build."
> — `patchcore.cpp:588-591`

`E9` at the start of a function you expected to begin with `PUSH EBP` is the
signature of a detour. It is also, at `D2Win+0xab47`, the signature of PD2's own
on-disk patch — the same five-byte shape, applied by a hex editor instead of by
`VirtualProtect`.

---

## What this means for reading a D2 binary

The practical lesson is narrow and severe: **static analysis of a shipped DLL
describes the code that shipped, and in a modded Diablo II that is frequently not
the code that runs.**

Five consequences follow, in rough order of how often they bite.

**A `CALL` target is a claim about the file, not about the process.** Of the four
runtime patches `PD2_EXT.dll` applies, three are call-site splices: the opcode
`E8` stays exactly where it was and only the four-byte displacement changes. A
disassembler shows `CALL 0x00407ee0` because that is what the file says, and it
will keep saying so no matter how carefully you read it. Nothing about the
instruction looks tampered with, because nothing about it *is* — the tampering is
in the operand, and the operand is data. This is why the `Bnclient` case is
detectable at all only by comparing the mapped image: `e8 07 27 00 00` and
`e8 c7 55 7a 93` are both perfectly ordinary calls.

**An address is only as stable as its module's base.** Blizzard's game DLLs
declare fixed preferred bases and no ASLR, so `6fd91e50` is a durable fact about
`D2Common`. `ProjectDiablo.dll` has a `.reloc` section of 296,960 bytes and
collides with two other modules at `0x10000000`, so an address read from its file
is wrong in the process by whatever delta the loader chose. Publish absolute
addresses for the first kind of module and RVAs for the second.

**A byte-identical file is not an unmodified execution.** This is the trap the
chapter opened on. PD2's own installer manifest asserts vanilla MD5s for
`D2Client.dll`, `D2Common.dll`, and `D2Game.dll` — and those assertions are true,
and they tell you nothing about what those modules do at runtime. If your analysis
of a modded game rests on a hash check, it rests on the wrong thing.

**Where you have found the patch, look for its siblings.** The techniques cluster.
Having found a tail-patch at `D2Win+0xab47`, the seventeen retargeted operands in
the same function were the next thing to look for, and the `.rdata` padding that
held the mod's name also held its twelve replacement paths. Slack space at the end
of `.text` and `.rdata` is where in-place patchers live, because it is the only
place that does not change the file's length; a run of non-zero bytes past a
section's `VirtualSize` is worth a second look in any binary.

**Patches expressed as data can be read; patches expressed as code must be run.**
The reason this chapter can say "1,290 patches, 5,357 bytes" without ever starting
the game is that Project Diablo 2 stores its patches in three tables and walks
them with twenty lines of loop. Had the same 1,290 edits been written as 1,290
inline `VirtualProtect`/`memcpy` pairs, counting them would have meant reading a
live process. When you find a patcher, look for its table before you start
tracing call sites.

**When you still cannot read the process, say so.** PD2 keeps a *runtime* record
of applied patches at `ProjectDiablo.dll+0x3d1708`, and that region is zeros in
the shipped file. Anything that record would tell you — which patches actually
took, in what order, on this machine — is outside a static analysis, and this
chapter does not claim it.

The 267 bytes are, in the end, an argument about where truth lives. Every one of
them is legible: a `JMP` into padding, a `PUSH` of a string, a call through a
CRT's leftover import, seven characters of a dependency's name. Nothing is
obfuscated, nothing is packed, and a hex editor and a PE parser recover the whole
of it. Those 267 bytes are also the last thing about this game that a file can
tell you, because everything after them is written into memory by code that runs
once and leaves no record — into `Game.exe`, into `Fog.dll`, into `Bnclient.dll`,
into a `D2Common.dll` whose hash will match Blizzard's for as long as anyone cares
to check.

The file told you the truth about itself. It has no way to tell you that this was
not the question.

---

## Reference tables

### PD2 Season 13 vs retail 1.13c: every shipped binary

SHA-256 comparison of `F:\D2Fleet\versions\pd2-s13\game\` against
`F:\D2VersionChanger\VersionChanger\LoD\1.13c\`, 2026-08-21.

| File | Bytes | Status |
|---|---|---|
| `Bnclient.dll` | 139,264 | identical |
| `D2CMP.dll` | 163,840 | identical |
| `D2Client.dll` | 1,093,632 | identical |
| `D2Common.dll` | 679,936 | identical |
| `D2DDraw.dll` | 69,632 | identical |
| `D2Direct3D.dll` | 110,592 | identical |
| `D2Game.dll` | 1,138,688 | identical |
| `D2Gdi.dll` | 53,248 | identical |
| `D2Glide.dll` | 98,304 | identical |
| `D2Lang.dll` | 77,824 | identical |
| `D2Launch.dll` | 167,936 | identical |
| `D2MCPClient.dll` | 49,152 | identical |
| `D2Multi.dll` | 126,976 | identical |
| `D2Net.dll` | 49,152 | identical |
| `D2gfx.dll` | 77,824 | identical |
| `D2sound.dll` | 98,304 | identical |
| `Diablo II.exe` | 36,864 | identical |
| `Fog.dll` | 212,992 | identical |
| `Game.exe` | 61,440 | identical |
| `SmackW32.dll` | 95,232 | identical |
| `binkw32.dll` | 200,704 | identical |
| `ijl11.dll` | 180,224 | identical |
| **`D2Win.dll`** | 147,456 | **patched in place — 247 bytes, 33 runs** |
| **`Storm.dll`** | 372,736 | **patched in place — 20 bytes, 11 runs** |
| `D2VidTst.exe` | — | present in 1.13c, absent from PD2 |

### PD2's added modules, and how each enters the process

| Module | Bytes | Entry mechanism |
|---|---|---|
| `ProjectDiablo.dll` | 4,312,576 | `LoadLibraryA`, from two independent call sites: the patched `D2Win` stub at `6f8fa2a1`, and `PD2_EXT+0x1012` |
| `PD2_EXT.dll` | 86,528 | static import of the renamed `Storm.dll` descriptor |
| `libcrypto-1_1.dll` | 2,522,624 | static import of `ProjectDiablo.dll` |
| `ddraw.dll` | 279,552 | search-order shadow; imported as `DDRAW.dll` by `D2DDraw`/`D2Direct3D` |
| `glide3x.dll` | 2,934,272 | name replacement; imported by `D2Glide.dll` |
| `BH.dll` | 1,423,360 | no static importer — runtime load *(loader unverified)* |
| `SGD2FreeRes.dll` | 307,200 | no static importer — runtime load *(loader unverified)* |
| `SGD2FreeDisplayFix.dll` | 220,672 | no static importer — runtime load *(loader unverified)* |

Static import graph parsed from all thirty-two PE files in the game directory;
"no static importer" means zero references across all of them.

### `Storm.dll` patch sites

Addresses are the *instruction*; the changed byte is one higher in the
`PUSH` rows.

| Instruction VA | Changed byte | Vanilla | PD2 | Effect |
|---|---|---|---|---|
| `6fbf8607` | `0x08608` | `6a 04` | `6a 40` | `VirtualAlloc` `flProtect` → `PAGE_EXECUTE_READWRITE` |
| `6fbfcfc1` | `0x0cfc2` | `6a 04` | `6a 40` | same (`MEM_RESERVE` path) |
| `6fbfd044` | `0x0d045` | `6a 04` | `6a 40` | same |
| `6fc06f6a` | `0x16f6b` | `6a 04` | `6a 40` | same |
| `6fc0a574` | `0x1a575` | `6a 04` | `6a 40` | same |
| `6fc0be13` | `0x1be14` | `6a 04` | `6a 40` | same (call via `EDI`) |
| `6fc0c308` | `0x1c309` | `6a 04` | `6a 40` | same |
| `6fc24808` | `0x34809` | `6a 04` | `6a 40` | same |
| `6fc19f82` | `0x29f82` | `0f 84 17 01 00 00` | `e9 18 01 00 00 90` | `JZ`→`JMP`, same target: `(attributes)` never parsed |
| `6fc3e63a` | `0x4e63a` | `VERSION` | `PD2_EXT` | import descriptor renamed |

The `PUSH` rows are the fourth argument of a `VirtualAlloc` call. Ghidra at
`6fbfd044`, for example, reads
`PUSH 0x4` / `PUSH 0x1000` / `PUSH 0x8000` / `PUSH EDI` /
`CALL dword ptr [0x6fc33230]`, and parsing Storm's import directory resolves
`0x6fc33230` to `KERNEL32.dll!VirtualAlloc`.

### Attachment mechanisms compared

| Mechanism | When | Needs launch control | Marks the disk | Survives a file verify |
|---|---|---|---|---|
| In-place file patch | load | no | yes | no |
| Import-descriptor rename + proxy | load | no | yes | no |
| Search-order shadow | load | no | adds a file | the shipped files verify clean |
| Detours import injection | process creation | **yes** | no | yes |
| `VirtualProtect` + `memcpy` | runtime | — (already inside) | no | yes |

### `PD2_EXT.dll`'s runtime patches

All applied through `KERNEL32!VirtualProtect` at IAT slot `0x1000e000`.

| Stage | Target | Vanilla bytes | Written | Original preserved at |
|---|---|---|---|---|
| `DllMain` (`100011c0`) | `Game.exe+0x82dd` | `e8 fe fb ff ff` → `0x407ee0` | `CALL 0x10001080` | `[0x100162d8]` |
| deferred (`10001080`) | `Game.exe+0x83ef` | jump table + 4 flag arms, 33 bytes | 33 × `0x90` | — (irreversible) |
| deferred (`10001080`) | `Game.exe+0x763f` | `e8 82 fe ff ff` → `0x4074c6` | `CALL 0x10001000` | `[0x100162d4]` |
| deferred (`10001080`) | `Fog.dll+0x17ea7` | `e8 a8 4f ff ff` → `0x6ff5ce54` | `CALL 0x10001050` | — |

Guard before the `Fog.dll` write: `GetModuleHandleA("Fog.dll")` must succeed, and
`Fog+0xff5f` and `Fog+0xff63` must equal `0x5e0cc483` and `0xc314c483`
(`ADD ESP,0xC; POP ESI` / `ADD ESP,0x14; RET`). Retail 1.13c `Fog.dll` holds
`83 c4 0c 5e 83 c4 14 c3` at `0xff5f`, so it passes.

### `ProjectDiablo.dll`'s runtime patch tables

| Table | Address | Stride | Records | Bytes | Notes |
|---|---|---|---|---|---|
| A | `0x103c7f50` | `0x14` | 1,198 | 5,061 | 800 rel32 splices, 169 dword writes, 229 byte-fills |
| B | `0x10367ee0` | `0x14` | 35 | 128 | skipped entirely when `-plugy` is on the command line |
| C | `0x103681b0` | `0x40c` | 57 | 168 | blob records, ≤256 payload bytes each |

Applied by `FUN_102ad020` (tables A and B) and `FUN_102ad1f0` (table C), both
called once from `FUN_102ad560`, itself called from `DllMain`.

### Selected `ProjectDiablo.dll` patch sites

| Target | Bytes | Stock | Effect |
|---|---|---|---|
| `D2Common+0x82cb5` | 30 × `0x90` | `JZ` + assert(line 112) + `exit(-1)` | removes the `inventory.txt` "exactly 32 records" check |
| `D2Common+0x2e9c1` | 3 | `MOV EAX, 0x2625a0` (2,500,000) | rewrites the immediate to `0x004c4b40` (5,000,000) |
| `D2Common+0x6ac20`…`+0x6b123` | 19 × rel32 | `CALL 6fd88a80` | redirects a stat accessor to `102edd50` |
| `D2Common+0x1a2e2` / `+0x1a2e3` | `0xE8` + rel32 | `CMP EAX, imm32` | manufactures a `CALL` where none existed |
| `Bnclient+0xf955` | rel32 | `CALL 6ff32060` | replaces Blizzard's `SID_AUTH_CHECK` builder |

### Module map, LoD 1.13c
<a id="module-map-lod-113c"></a>

| Module | Preferred base | Bytes | Export style |
|---|---|---|---|
| `D2Client.dll` | `6fab0000` | 1,093,632 | 4 exports, 1 named |
| `D2Game.dll` | `6fc20000` | 1,138,688 | 61, ordinal-only |
| `D2Common.dll` | `6fd50000` | 679,936 | 1,172, 2 named |
| `D2CMP.dll` | `6fe10000` | 163,840 | 107, ordinal-only |
| `D2Win.dll` | `6f8e0000` | 147,456 | 207, ordinal-only |
| `D2Lang.dll` | `6fc00000` | 77,824 | 63, 49 named |
| `D2Gfx.dll` | `6fa80000` | 77,824 | 88, 1 named |
| `D2Net.dll` | `6fbf0000` | 49,152 | 38, ordinal-only |
| `D2Sound.dll` | `6f9b0000` | 98,304 | 71, ordinal-only |
| `D2Launch.dll` | `6fa40000` | 167,936 | 1, named |
| `D2Multi.dll` | `6f9d0000` | 126,976 | 1, named |
| `D2MCPClient.dll` | `6fa20000` | 49,152 | 63, ordinal-only |
| `D2Gdi.dll` | `6f870000` | 53,248 | 4, ordinal-only |
| `D2DDraw.dll` | `6f8c0000` | 69,632 | 1, ordinal-only |
| `D2Direct3D.dll` | `6f880000` | 110,592 | 1, ordinal-only |
| `D2Glide.dll` | `6f850000` | 98,304 | 1, ordinal-only |
| `Storm.dll` | `6fbf0000` | 372,736 | 814, ordinal-only |
| `Fog.dll` | `6ff50000` | 212,992 | 268, 9 named |
| `Bnclient.dll` | `6ff20000` | 139,264 | 23, all named |
| `Ijl11.dll` | `60000000` | 180,224 | 6, all named |
| `SmackW32.dll` | `10000000` | 95,232 | 61, all named |
| `Binkw32.dll` | `10000000` | 200,704 | 68, all named |
| `Game.exe` | `00400000` | 61,440 | relocations stripped |
| `Diablo II.exe` | `00400000` | 36,864 | relocations stripped |
| `D2VidTst.exe` | `00400000` | 184,320 | relocations stripped |

Every entry has `DllCharacteristics = 0x0000` — no ASLR, no `NX_COMPAT`.

---

## Version differences

| What | 1.13c | 1.14d | 1.09d |
|---|---|---|---|
| Game DLLs in the install | 20 | 0 | 20 |
| PE files in the install | 25 | 7 | 25 |
| `Game.exe` on disk | 61,440 bytes | 3,618,792 bytes | 61,440 bytes |
| Fixed module bases | yes, all `0x0000` DllCharacteristics | one image at `0x400000` | yes |
| `D2Client.dll` base | `6fab0000` | absent | `6faa0000` |
| `D2Common.dll` base | `6fd50000` | absent | `6fd40000` |
| Per-module patching possible | yes | no — one image | yes |
| Absolute addresses portable across builds | no | no | no |
| Project Diablo 2 runs on it | yes (`Game.exe` `1.0.13.60`) | no | no |
| SGD2FreeRes support | loads | — | declines at `DllMain` (error 1114) |
| `Storm.dll` / `D2Net.dll` base collision | **yes**, both `6fbf0000` | — | no (`6ffb0000` / `6fc00000`) |

`—` marks "not applicable": 1.14d has no separate modules for the question to be
about.

---

## Companion report

Every claim in this chapter, its verdict, the evidence that settled it, and the
list of what remains unverified:
[mods-and-hooking-1.13c.verification.md](mods-and-hooking-1.13c.verification.md).
