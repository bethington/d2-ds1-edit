# Verification report — *How Mods Attach to Diablo II*

Companion to [mods-and-hooking-1.13c.md](mods-and-hooking-1.13c.md).

This chapter is **original reverse engineering**, not a conversion of archived
third-party material, so there is no origin block or rights question in the
chapter itself. What follows is the audit: what was checked, how, what it
returned, what was corrected mid-draft, and what is still open.

**Date:** 2026-08-21.
**Method:** static analysis only. No Diablo II process was launched, driven, or
attached to at any point, by instruction. Every claim below is therefore either
(a) read out of a shipped file, (b) read out of Ghidra's static database, or
(c) attributed to someone else's live measurement and marked as such.

---

## 1. Ground truth

### Binaries

| Role | Path | SHA-256 (head…tail) | Bytes |
|---|---|---|---|
| vanilla `D2Win.dll` | `F:\D2VersionChanger\VersionChanger\LoD\1.13c\D2Win.dll` | `a9afb52d…334730fb` | 147,456 |
| PD2 S13 `D2Win.dll` | `F:\D2Fleet\versions\pd2-s13\game\D2Win.dll` | `ff5cbb95…ec453736` | 147,456 |
| vanilla `Storm.dll` | `…\LoD\1.13c\Storm.dll` | `4b5fcaf8…a2da2d5b` | 372,736 |
| PD2 S13 `Storm.dll` | `…\pd2-s13\game\Storm.dll` | `cc848eb2…50fb194d` | 372,736 |
| vanilla `D2Common.dll` | `…\LoD\1.13c\D2Common.dll` | `59fa5928…83e06e10` | 679,936 |
| vanilla `D2Client.dll` | `…\LoD\1.13c\D2Client.dll` | `dd8bc602…8836d906` | 1,093,632 |
| vanilla `Bnclient.dll` | `…\LoD\1.13c\Bnclient.dll` | `3631ca88…e76a56f5` | 139,264 |
| PD2 S13 `ProjectDiablo.dll` | `…\pd2-s13\game\ProjectDiablo.dll` | `538a77b7…cfbf9cb3` | 4,312,576 |
| PD2 S13 `PD2_EXT.dll` | `…\pd2-s13\game\PD2_EXT.dll` | `a47e6f02…58766f49` | 86,528 |

Whole-directory comparison covered all 32 PE files in
`F:\D2Fleet\versions\pd2-s13\game\` against all 25 in
`F:\D2VersionChanger\VersionChanger\LoD\1.13c\`.

### Ghidra programs used

Every call passed `program=` explicitly. The programs touched:

| Program path | Image base | Notes |
|---|---|---|
| `/PD2Realm/D2Win.dll` | `6f8e0000` | **contains VANILLA bytes** — see below |
| `/PD2Realm/Storm.dll` | `6fbf0000` | **contains VANILLA bytes** — see below |
| `/PD2Realm/ProjectDiablo.dll` | `10000000` | genuine PD2 module, 12,398 functions |
| `/PD2Realm/PD2_EXT.dll` | `10000000` | **imported during this run** (see §7) |
| `/Vanilla/1.13c/D2Common.dll` | `6fd50000` | retail; used for the `LoadInventoryTable` verification |

Note that **two** programs claim the path `/Vanilla/1.13c/D2Common.dll` — a second
one is suffixed `.0`, and both point at the same retail file. The unsuffixed one
was used throughout. Because the load-bearing D2Common claim (§3.9) was also
verified byte-for-byte against the file on disk in Python, the choice between them
cannot have changed the result.

### The base-collision hazard, and how it was handled

The skill warns that mod copies share image bases with vanilla, so passing the
wrong `program=` silently compares a binary to itself. In this project the hazard
is worse than advertised, and it bit in a way worth recording:

**`/PD2Realm/D2Win.dll` and `/PD2Realm/Storm.dll` are not PD2's binaries.** Their
`executable_path` is `F:/D2Fleet/d2gs-runtime/…`, and hashing those files shows
them byte-identical to retail 1.13c (`a9afb52d…` and `4b5fcaf8…`). The
`d2gs-runtime` tree is the game-*server* runtime, which uses stock DLLs. A folder
named `/PD2Realm/` therefore contains, for these two modules, vanilla code.

This turned out to be convenient rather than harmful — the chapter needed the
*vanilla* side of each diff, and that is exactly what those programs hold — but
only because it was checked. Any conclusion of the form "PD2's Storm does X",
drawn from `/PD2Realm/Storm.dll`, would have been a statement about Blizzard's
Storm.

**Every Ghidra-sourced address in the chapter is therefore a vanilla address**,
and every PD2-side byte was read from the file on disk, not from Ghidra. The two
were then reconciled by RVA. `D2Win.dll` and `Storm.dll` both have
`FileAlignment == SectionAlignment == 0x1000`, so file offset equals RVA in every
section that matters; this was verified from the section table rather than
assumed.

### Independence of sources

Where the chapter says two sources agree, they are genuinely independent:

- **Byte diffs and PE parsing were done in Python, not Ghidra.** The import
  tables, export tables, section tables, and every `rel32` resolution in the
  chapter were computed by a standalone parser written for this run. Ghidra was
  used for disassembly and cross-references. When the two agree (e.g. the IAT
  slot at `D2Win+0x1b208`), that is two implementations agreeing, not one.
- **d2-fleet's measurements are independent of this analysis.** They were made
  between 2026-08-09 and 2026-08-17 by a different process (running the game in
  containers) and recorded before this chapter existed.
- **PD2's own `local_metadata.json` is a third-party assertion.** It is the mod
  launcher's manifest, written by the PD2 project, not by anyone in this
  investigation.

---

## 2. Claim tally

| Type | Checked | Confirmed | Corrected mid-draft | Unverified / marked |
|---|---|---|---|---|
| A — mechanical (bytes, addresses, counts, imports/exports) | 104 | 104 | 4 | 0 |
| B — interpretive (what a function does) | 16 | 13 | 1 | 3 |
| C — contextual (claims about the world) | 6 | 4 | 1 | 1 |
| D — data/asset | 2 | 2 | 0 | 0 |

Type-A coverage is exhaustive, not sampled, for the on-disk diffs: they are small
enough (247 + 20 bytes) that every changed byte was resolved.

Type-B sampling policy: **every function whose behaviour the chapter asserts was
disassembled in full** — `D2Win!FUN_6f8eaa20`, `Storm!FUN_6fc19f60`,
`Storm!FUN_6fc0bdd0`, `PD2_EXT!FUN_100011c0`, `PD2_EXT!FUN_10001080`,
`PD2_EXT!10001000`, `PD2_EXT!10001050`, `D2Common!LoadInventoryTable` (tail),
`ProjectDiablo!FUN_102adad0`, `!FUN_102ad560`, `!FUN_102ad020`, `!FUN_102ad1f0`,
`!FUN_102ad400`, `!FUN_102f10f0`, `!FUN_10253f30`, `!FUN_102e79d0`. No behavioural
claim rests on decompiled C; per the skill's warning about this database, every
constant and address in the chapter came from `disassemble_function` /
`disassemble_bytes` / `read_memory` output.

**Division of labour.** The `ProjectDiablo.dll` survey (§3.9) was delegated to a
subagent working in Ghidra under instruction to use disassembly only. Its
load-bearing results were then re-verified independently here — see §3.9 and §4.6,
including one finding of its own that had to be overturned.

---

## 3. Verified claims

### 3.1 The disk-footprint claim

**Claim:** PD2 ships 22 of 24 Blizzard binaries byte-identical, and patches
exactly two, in place, at unchanged length.

**Evidence:** SHA-256 over every `.dll`/`.exe` in both trees. Result: 22
identical, 2 differing (`D2Win.dll` 247 bytes / 33 runs; `Storm.dll` 20 bytes /
11 runs), 8 files absent from vanilla, 1 vanilla file (`D2VidTst.exe`) absent from
PD2. **Confirmed.**

**Corroborating third source:** PD2's own `local_metadata.json` lists MD5s for the
files its launcher manages. Recomputing them:

| File | manifest MD5 | installed MD5 | vanilla MD5 | verdict |
|---|---|---|---|---|
| `D2Win.dll` | `01a5b0f4…` | `01a5b0f4…` | `32b35919…` | manifest matches PD2's patched copy |
| `Storm.dll` | `a4deb162…` | `a4deb162…` | `adb3aecd…` | manifest matches PD2's patched copy |
| `D2Client.dll` | `f5860c62…` | `f5860c62…` | `f5860c62…` | manifest asserts the vanilla value |
| `D2Common.dll` | `ee123880…` | `ee123880…` | `ee123880…` | manifest asserts the vanilla value |
| `D2Game.dll` | `978f6f74…` | `978f6f74…` | `978f6f74…` | manifest asserts the vanilla value |

This rules out the alternative hypothesis that d2-fleet, or this machine's
staging, produced the patched `D2Win`/`Storm`. PD2 distributes them.

### 3.2 The `D2Win` tail patch

| Claim | Evidence | Verdict |
|---|---|---|
| patch at `D2Win+0xab47`, `b8 01 00 00 00` → `e9 55 f7 00 00` | byte diff | confirmed |
| the site is the last instruction of `FUN_6f8eaa20` | Ghidra `get_function_by_address` (`body_end 6f8eab4c`) + `disassemble_function` | confirmed |
| the function mounts the MPQ set and returns 1/0 | full disassembly: 7 open calls, handles to 7 globals, validity checks, `XOR EAX,EAX;RET` vs `MOV EAX,1;RET` | confirmed |
| the `JMP` lands at `6f8fa2a1` | `0x6f8eab4c + 0xf755 = 0x6f8fa2a1` | confirmed |
| `6f8fa2a1` is `.text` padding, zero in vanilla | segment table (`.text` `0x1000`–`0x1afff`, `VirtualSize 0x192a1`); diff shows 13 zero bytes | confirmed |
| stub = `PUSH 0x6f8fe378` / `CALL [0x6f8fb208]` / `MOV EAX,1` / `RET` / `NOP` | byte diff, hand-decoded, operands resolved independently | confirmed |
| `0x6f8fe378` holds `ProjectDiablo.dll` in PD2, zeros in vanilla | byte diff | confirmed |
| `0x6f8fb208` is `KERNEL32!LoadLibraryA` | PE import parse: KERNEL32 FirstThunk RVA `0x1b1b0`, slot 22 at RVA `0x1b208` | confirmed |
| that slot exists in vanilla because of `___crtMessageBoxA` | Ghidra `get_xrefs_to 6f8fb208` → single ref at `6f8e679b` in `___crtMessageBoxA`, preceded by `PUSH 0x6f8fc228`; bytes at RVA `0x1c228` read `user32.dll` | confirmed |

### 3.3 The `D2Win` MPQ path retarget

17 two-byte operand patches, 12 new strings. Each old and new operand was
resolved to its string by reading both files at `operand − 0x6f8e0000`. Full
table is in the chapter. **Confirmed**, all 17.

**Independent corroboration:** d2-fleet measured "twelve hard-coded strings PD2
appended to D2Win's `.rdata` plus seventeen `push` operands retargeted at them"
(`d2-fleet/docs/REMOTE.md:139-144`) and "PD2's 247" total bytes
(`REMOTE.md:145-152`), on 2026-08-15, from a container bring-up. Three numbers,
independently derived, all matching.

**Also confirmed:** the installed tree matches the redirect —
`F:\D2Fleet\versions\pd2-s13\` holds `d2data.mpq`, `d2char.mpq`, `d2exp.mpq` and
the rest of Blizzard's archives, one level above `…\pd2-s13\game\`.

### 3.4 The `Storm` patches

| Claim | Evidence | Verdict |
|---|---|---|
| 8 sites flip `PUSH 4` → `PUSH 0x40` | byte diff; each changed byte preceded by `0x6a` | confirmed |
| all 8 are `VirtualAlloc` `flProtect` | Ghidra disassembly at all 8 instruction addresses; 7 followed by `CALL dword ptr [0x6fc33230]`, the 8th (`6fc0be13`) in `FUN_6fc0bdd0` which does `MOV EDI,[0x6fc33230]` at `6fc0bdd7` | confirmed |
| `0x6fc33230` is `KERNEL32!VirtualAlloc` | PE import parse of Storm's IAT | confirmed |
| `6fc19f82` `JZ`→`JMP`, same target | byte diff + both displacements computed: `0x6fc1a09f` either way | confirmed |
| that function loads MPQ `(attributes)` | full disassembly; `PUSH 0x6fc370d8` where `6fc370d8` = `"(attributes)"`; size check `EBP*4+8` / `+EBP*8`; stride-`0x1c` copies | confirmed |
| forcing the branch means the CRC32/FILETIME arrays are never populated | the `JMP` target is `MOV EAX,ESI;…;RET 8` with `ESI = 0` from `XOR ESI,ESI` at `6f c19f6e` | confirmed |
| Storm import descriptor 6 renamed `VERSION.dll` → `PD2_EXT.dll` | PE import parse of both files: same descriptor index, same FirstThunk RVA `0x43398`, same 3 imported names, name string at file offset `0x4e63a` | confirmed |

### 3.5 `PD2_EXT.dll`

| Claim | Evidence | Verdict |
|---|---|---|
| 12 exports, all PE forwarders to `version.*` | export directory parsed by hand; each `AddressOfFunctions` RVA falls inside the export directory, i.e. is a forwarder string | confirmed |
| 14 function slots but 12 names (ordinals 12, 13 empty) | same parse | confirmed |
| imports `VirtualProtect`, `GetModuleHandleA`, `LoadLibraryA`, `GetProcAddress`, `MessageBoxA` | PE import parse; `USER32.dll` contributes exactly one function | confirmed |
| `FUN_100011c0` is `DllMain` | `CMP [EBP+0xc], 1`; `MOV EAX,1`; `RET 0xc`; and `get_xrefs_to` shows two calls from Ghidra's `dllmain_dispatch` | confirmed |
| `DllMain` splices `Game.exe+0x82dd` to `10001080` and saves the original | full disassembly | confirmed |
| `Game.exe+0x82dd` vanilla is `e8 fe fb ff ff` → `0x00407ee0` | read from retail `Game.exe`, `rel32` resolved | confirmed |
| `10001080` NOPs 33 bytes at `Game.exe+0x83ef` | disassembly: `VirtualProtect(…, 0x21, 0x40, …)` then eight `dword` and one `byte` store of `0x90` = 33 | confirmed |
| those 33 bytes are a jump table + 4 flag arms | retail bytes: `ff 24 85 38 84 40 00` + 4 × (`c6 44 24 xx 01` [+ `eb xx`]) = exactly 33 | confirmed |
| `10001080` splices `Game.exe+0x763f` to `10001000` | disassembly | confirmed |
| `Game.exe+0x763f` vanilla is `e8 82 fe ff ff` → `0x004074c6` | read from retail `Game.exe` | confirmed |
| `10001080` guards `Fog.dll` before patching it | disassembly: `GetModuleHandleA("Fog.dll")`, then two `CMP dword ptr [EAX+0xff5f/0xff63]` against `0x5e0cc483` / `0xc314c483`, each `JNZ` to the exit | confirmed |
| the guard passes on retail 1.13c `Fog.dll` | retail `Fog+0xff5f` reads `83 c4 0c 5e 83 c4 14 c3` — the two constants, little-endian | confirmed |
| `10001080` splices `Fog+0x17ea7` to `10001050` | disassembly | confirmed |
| `Fog+0x17ea7` vanilla is `e8 a8 4f ff ff` → `0x6ff5ce54` | read from retail `Fog.dll` | confirmed |
| `10001080` chains to the displaced original via `[0x100162d8]` | `CALL dword ptr [0x100162d8]` at `100011a6`; written at `10001227` in `DllMain` | confirmed |
| `10001000` calls the original then `LoadLibraryA("ProjectDiablo.dll")` | full disassembly; operands resolved to the three strings and to the `LoadLibraryA`/`MessageBoxA` IAT slots | confirmed |
| build path `C:\projects\Project-Diablo-2\scripts\build-pd2ext-release\PD2_EXT.pdb` | string at file offset `0x12148` | confirmed |

### 3.6 The 1.13c architecture claims

| Claim | Evidence | Verdict |
|---|---|---|
| 1.13c installs 25 PE files, 1.14d installs 7 | directory enumeration of both version trees | confirmed |
| `Game.exe` 61,440 → 3,618,792 bytes | file sizes in the version trees | confirmed |
| every 1.13c module has `DllCharacteristics = 0x0000` | PE optional-header parse of all 25 | confirmed |
| `Storm.dll` and `D2Net.dll` share preferred base `6fbf0000` | PE optional-header parse | confirmed |
| export styles (ordinal-only vs named) per module | export-directory parse of all 22 DLLs | confirmed |
| `Game.exe`, `Diablo II.exe`, `D2VidTst.exe` have relocations stripped | `IMAGE_FILE_RELOCS_STRIPPED` in the COFF characteristics | confirmed |
| `ProjectDiablo.dll` preferred base collides with `binkw32.dll` and `SmackW32.dll` | all three declare `0x10000000` | confirmed |
| `ProjectDiablo.dll` exports exactly one ordinal | export-directory parse: 1 function, 1 name, ordinal base 1, RVA `0x233b50` | confirmed |
| `ProjectDiablo.dll` sections, incl. `.data` `10398000`–`104eb103` | Ghidra `list_segments` | confirmed |
| `103d1708` is zero-filled on disk | Ghidra `read_memory` at `103d16c8`, 192 bytes, all zero | confirmed |
| `ProjectDiablo.dll` imports the dynamic CRT | PE import parse: `MSVCP140.dll`, `VCRUNTIME140.dll`, 10 × `api-ms-win-crt-*` | confirmed |

### 3.7 The load-order shadow

| Claim | Evidence | Verdict |
|---|---|---|
| `D2DDraw.dll` and `D2Direct3D.dll` import `DDRAW.dll` | PE import parse; both are byte-identical to vanilla | confirmed |
| `D2Glide.dll` imports `glide3x.dll` | same | confirmed |
| `ddraw` is not in `KnownDLLs`, so the shadow works | `reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs"` — 38 entries, no `ddraw`, no `glide3x` | confirmed |
| no file statically imports `ProjectDiablo.dll` | import descriptors of all 32 PE files in the game dir enumerated; zero references | confirmed |
| `libcrypto-1_1.dll` is imported by `ProjectDiablo.dll` | PE import parse | confirmed |
| `PD2_EXT.dll` is imported only by the patched `Storm.dll` | PE import parse | confirmed |

### 3.8 `Bnclient` disk side

`Bnclient.dll+0xf954` reads `e8 07 27 00 00` in retail 1.13c; the `rel32`
resolves to `0xf959 + 0x2707 = 0x12060`, i.e. VA `6ff32060` at Bnclient's
preferred base `6ff20000`. This exactly matches the "on disk" row of d2-fleet's
live measurement (`docs/PD2-ONLINE.md:80-92`), which is what makes that
measurement's "live" row credible. MD5 of both the vanilla and the PD2 copy is
`f0e1caaf1ee073802714e6f88f4700b2`, matching PD2's manifest. **Confirmed
(disk side only).**

### 3.9 `ProjectDiablo.dll`

Surveyed by a delegated Ghidra agent; the results below marked **†** were then
re-derived here from primary evidence.

| Claim | Evidence | Verdict |
|---|---|---|
| `DllMain` = `FUN_102adad0`; handles only `DLL_PROCESS_ATTACH`; all init synchronous inside it | disassembly: `SUB [EBP+0xc],1 / JNZ / CALL 102ad560` | confirmed |
| patch tables A/B/C at `0x103c7f50`, `0x10367ee0`, `0x103681b0` with strides `0x14`/`0x14`/`0x40c` | record walkers `FUN_102ad020`, `FUN_102ad1f0` disassembled; tables parsed | confirmed |
| 1,290 records, 5,357 bytes, distributed as tabulated | exhaustive table parse | confirmed (agent); not independently re-parsed here |
| primitive = `VirtualProtect(0x40)` → copy → `memcmp` → `VirtualProtect(restore)` | disassembly `102ad0b1`–`102ad149` | confirmed |
| **no** `FlushInstructionCache`, **no** `WriteProcessMemory` | all 325 external symbols enumerated | confirmed |
| zero hardcoded `0x6f8*`/`0x6fa*`/`0x6fb*`/`0x6fc*`/`0x6fd*` immediates | `search_instructions` over 612,009 instructions; single `0x6fd` hit is `PUSH 0x6fd` = 1789 | confirmed |
| module table at `0x103c68a0`, 27 × `{LPCWSTR, HMODULE}` | `FUN_102ad400` disassembly + `read_memory` of 216 bytes | **† confirmed here** |
| entry 0 = `D2Client.dll`, entry 21 = `Bnclient.dll`, UTF-16 | `read_memory` at `10376ebc` and `103770b0` | **† confirmed here** |
| resolver `FUN_102f10f0`: negative selector → `GetProcAddress` ordinal, positive → base+RVA | disassembly `102f1163`–`102f1197` | confirmed |
| 1,959 resolver call sites, all table-driven selectors | agent enumeration | confirmed (agent) |
| version gate accepts only `"1.0.13.60"` and `"1.0.13.64"`, else `exit(0)` | `FUN_10253f30` disassembly | confirmed |
| four MPQ mounts via `D2Win+0x7e60` at priority `0x1770` | `FUN_102ad560` disassembly + selector table read | confirmed |
| `0x6f8fc9a8` (D2Win's own 5th argument) is the string `D2Win.DLL` | **† read from the retail `D2Win.dll` file here** | **† confirmed here** |
| D2Win's own priorities are `0x3e8`, `0xbb8`, `0x1388` | **† from this chapter's own `FUN_6f8eaa20` disassembly** | **† confirmed here** |
| `D2Common+0x82cb5`, 30 × `0x90`, erases `JZ` + assert + `exit(-1)` | **† retail `D2Common.dll` read in Python**: `74 1c` at `0x82cb5`, `81 c4 b4 05 00 00` at `0x82cd3`, `0x6fdd2cd3 − 0x6fdd2cb5 = 0x1e = 30` | **† confirmed here** |
| the guarded value is `CMP dword ptr [0x6fdefa58], 0x20` | **†** bytes `83 3d 58 fa de 6f 20` at RVA `0x82cae` | **† confirmed here** |
| `0x6fdefa58` is the record count, written by this function | `get_xrefs_to`: DATA write at `6fdd2b19` (in `LoadInventoryTable`), reads at `6fdd2cae` and four accessors | **† confirmed here** |
| `D2Common+0x2e9c1` rewrites 2,500,000 → 5,000,000 | agent disassembly of stock `6fd7e9c0` = `MOV EAX,0x2625a0; RET 4` | confirmed (agent) |
| SHA-256 module manifest, PSAPI walk, base64 watch list of six | `FUN_102e79d0` / `FUN_102e7dd0` disassembly; six base64 literals decoded | confirmed (agent) |
| the scan runs on a background thread, not in `DllMain` | `FUN_102cf530` passed to `_beginthreadex` at `102e6f03` | confirmed |
| `ProjectDiablo.dll` imports nothing from any game DLL | full 20-DLL import list | confirmed |

**The `mod=0x15` cross-check.** d2-fleet read a live patch record as
`mod=0x15, offset=0xF955, value=0x102E4F20, isRel32=1`
(`docs/PD2-ONLINE.md:88-90`). Independently, the static module table at
`0x103c68a0` places `Bnclient.dll` at index 21 = `0x15`, and the static record
layout recovered from `FUN_102ad020` has exactly the fields
`{module index, RVA, value, rel32 flag, length}`. Two investigations, one live and
one static, agreeing on a record format and an index neither could have inferred
from the other. This is the strongest single piece of corroboration in the
chapter.

---

## 4. Corrections applied during drafting

These are errors this report exists to record. All three were caught by checking
rather than by rereading.

### 4.1 "The `D2Win` stub is the sole entry point" — WRONG, corrected

The first draft stated: *"`ProjectDiablo.dll` is loaded only this way… The stub
in `D2Win`'s `.text` padding is the sole entry point."*

The reasoning was that no PE in the game directory statically imports
`ProjectDiablo.dll` — which is true — and the leap from "nothing imports it" to
"only one thing loads it" was unjustified. Scanning `PD2_EXT.dll` for strings
found `ProjectDiablo.dll` at file offset `0x11c38`, and searching the `.text`
section for that address as a 32-bit immediate found a reference at `1000100d`,
five bytes before `CALL dword ptr [0x1000e008]` — `LoadLibraryA`.

There are **two** load paths. The claim was replaced with a forward reference, and
the discovery became the chapter's strongest section. This is also why
`PD2_EXT.dll` was imported into Ghidra mid-run (§7).

### 4.2 `Storm` patch-site addresses were off by one

The first draft's Storm table gave `6fbf8608` etc. — the address of the *changed
byte*, labelled as though it were the instruction address. The `PUSH 4`
instruction begins one byte earlier. Corrected throughout to give the instruction
address with the changed byte in its own column, and cross-checked against
Ghidra (`6fbfd044 PUSH 0x4`, `6fc0be13 PUSH 0x4`).

### 4.3 "7 of 8 `VirtualAlloc` sites" was asserted from a heuristic

The first pass identified the call target by scanning forward for the first
`ff 15` within 64 bytes — which is not a disassembly and gave a wrong answer for
one site (it found an unrelated `ExitProcess` call). All eight were then
disassembled in Ghidra. The conclusion held (all eight are `VirtualAlloc`) but
the route to it was replaced.

### 4.4 1.09d PE-file count

Draft said 24; enumeration returns 25. Corrected in the version table.

### 4.5 A 1.09d base-collision row invented during the differences table

While filling the `## Version differences` table I added a row asserting that
1.09d shares 1.13c's `Storm`/`D2Net` collision at `6fbf0000`. It was a guess, made
because the collision felt like an old artifact that would predate 1.13c. Parsing
the 1.09d headers gives `Storm.dll` at `6ffb0000` and `D2Net.dll` at `6fc00000` —
no collision. Corrected before publication, and the body now uses 1.09d as the
counter-example rather than as a parallel.

Recorded because it is the only claim in either file that was written before it
was checked, and it was wrong.

### 4.6 The delegated survey's DRLG attribution — WRONG, overturned

The `ProjectDiablo.dll` survey reported that the 30-byte NOP at `D2Common+0x82cb5`
sits in DRLG code, citing the assert block's filename argument as
`..\Source\D2Common\DRLG\DrlgLogic.cpp`, and concluded that the Ghidra label
`LoadInventoryTable` on the containing function was wrong.

It is the attribution that is wrong, and the way it fails is instructive. The
`PUSH` at `6fdd2cbf` pushes `0x6fdda728`. The `DrlgLogic.cpp` string begins at
`0x6fdda72c` — **four bytes later**. `0x6fdda728` points at four NUL bytes: it is
D2Common's *blanked* assert-filename pointer, shared by **567** call sites across
the module, which is what a release build looks like when most `__FILE__` strings
have been stripped. Only 9 sites in the whole DLL reference the real
`DrlgLogic.cpp` string, and all 9 lie in `0x80fea`–`0x81e36` — entirely *before*
`LoadInventoryTable` begins at `0x820a0`.

The label is correct. The proof is in the data immediately below the assert block:
`6fdda3e4`–`6fdda720` holds **72** consecutive column names — `glovesHeight`,
`glovesWidth`, … `feetLeft`, `beltWidth`, `torsoTop`, `gridX`, `gridY`,
`invBottom`, `invTop`, `invRight`, `invLeft` — preceded by the Excel column-list
terminator `end` at `6fdda3e0`. That is `inventory.txt`'s schema, not DRLG's.

(A first pass at this counted 48 names from `6fdda504`; extending the scan
downward found 24 more, beginning at `glovesHeight`. The chapter carries the
corrected figure.)

Two lessons, both recorded because the chapter turns on this function's identity:
a pointer four bytes short of a string is not a reference to that string, and
"the binary's label is wrong" is a claim that needs the same evidence as any
other. The chapter states the function as `LoadInventoryTable` and the table as
`inventory.txt`, on this evidence.

### 4.7 The framing claim itself was too loose

The chapter was commissioned around the statement that "a stock `D2Common` cannot
read PD2's tables because `ProjectDiablo.dll` patches the loader at runtime". The
first half is true. The second half is not: `D2Common`'s loader is not patched.
Its path-format strings (`DATA\GLOBAL\EXCEL` at `6fdddbf0`, `%s\%s.bin` at
`6fde154c`) are untouched, and none of the 84 `D2Common` patch records land in that
code.

What actually happens is two-part: PD2's archives are mounted at priority 6,000 —
above every archive `D2Win` mounts — so the *stock* reader finds PD2's file; and
the loader's hardcoded **validation** of what it found is erased. The chapter now
says this, and says it as a sharpening of the received version rather than a
contradiction of it, because the received version has the consequence right.

### 4.8 A claim that was *not* corrected, and why

The chapter says PD2's `D2Win.dll` looks for archives "one directory above the
game folder". The doubled backslash in `..\\d2data.mpq` invited a correction to
`..\`. It was left as measured — the file genuinely contains `2e 2e 5c 5c` — with
a parenthetical explaining that Windows collapses the separator. Reporting the
bytes and explaining the effect is better than silently normalising the bytes.

---

## 5. Unverified, and marked as such in the chapter

| Claim | Why it could not be settled | Where marked |
|---|---|---|
| **Which patches actually took, at run time** | PD2's *runtime* record at `ProjectDiablo.dll+0x3d1708` is zero-filled on disk. The static tables give what PD2 *intends* to apply (1,290 records); whether every one lands — the walker's own `memcmp` can fail, and a record whose module is absent is skipped at `102ad066` — is not observable statically. | "When you still cannot read the process, say so" |
| **Why PD2 makes Storm's allocations executable** | The eight `flProtect` flips are measured beyond doubt; the motive is not in the binaries. | "(Measured; the motive is not established from the binaries.)" |
| **What PD2 gains by disabling `(attributes)`** | Same: the patch is unambiguous, the intent is not. Whether Storm would otherwise *enforce* those CRC32s was not traced. | "(What PD2 gains by this is not established…)" |
| **What the four `Game.exe` flags at `[ESP+0x5c/0x5e/0x5f/0x61]` control** | Requires identifying the enclosing function in 1.13c `Game.exe`, which is not loaded in Ghidra. Circumstantially a command-line option parser, but that is a guess and is not stated. | "(What those four flags control is **unverified**…)" |
| **What `PD2_EXT!10001050` does to `Fog.dll`'s behaviour** | The replacement's shape is legible (two calls to a helper with the same two arguments, a NUL written through the returned pointer between them — the shape of find-substring-and-truncate) but the helper was not identified. The chapter states the splice, not the semantics. | not asserted |
| **Which of `Storm.dll` / `D2Net.dll` relocates** | Both declare base `6fbf0000`; deciding the winner requires a live module list. | "(Unverified: which of the two actually moves…)" |
| **`BH.dll`, `SGD2FreeRes.dll`, `SGD2FreeDisplayFix.dll` loaders** | Nothing statically imports them, so something `LoadLibrary`s them at runtime. `ProjectDiablo.dll` is the obvious candidate but was not confirmed. | *(loader unverified)* in the added-modules table |
| **The per-table record counts (1,198 / 35 / 57) and the 1,959 resolver call sites** | Parsed exhaustively by the delegated agent; not independently re-parsed here. The table *addresses*, *strides*, *record layout* and *walker code* were re-verified. | numbers stated as measured; this row is the caveat |
| **What `PD2_EXT!10001050` does to `Fog.dll`'s behaviour** | Its shape is legible (two calls to one helper with the same arguments, a NUL written through the returned pointer between them) but the helper was not identified. | the chapter states the splice, not the semantics |
| **Whether all 1,290 records target code that exists in 1.13c** | Table B is skipped when `-plugy` is passed and some records may target the other supported build (`1.0.13.64`). No per-record version filtering was attempted. | not asserted |

---

## 6. Claims attributed to d2-fleet rather than to this analysis

The chapter quotes the d2-fleet repository for things that can only be learned by
running the game, and for source comments recording measurements. These are
marked inline with a file and line. For the record, the load-bearing ones:

| Claim | Source | Independent check done here |
|---|---|---|
| `Bnclient+0xf955` reads `c7 55 7a 93` live, calling `ProjectDiablo+0x2e4f20` | `docs/MEMORY-IMAGES.md:13-19` | disk side confirmed (§3.8); live side **not** checked |
| Blizzard's auth builder never runs (token global still 0) | `docs/PD2-ONLINE.md:80-92` | not checked |
| PD2's patch record at `+0x3d1708` | `docs/PD2-ONLINE.md:88-90` | address confirmed zero-filled on disk; **record layout and `mod=0x15` → `Bnclient.dll` independently confirmed** from the static tables (§3.9) |
| `ProjectDiablo.dll` loads at `0x033f0000` live | `docs/PD2-ONLINE.md:98-99` | not checked; the *reason* it must relocate was confirmed (base collision) |
| Dynamic CRT → `LoadLibrary` error 126, silent death | `CMakeLists.txt:18-26` | PD2's dynamic-CRT imports confirmed; the redist is *present* on this machine today, so the failure could not be reproduced and is dated in the chapter |
| Empty export directory → `0xc000007b` | `fleet/hook_main.cpp:87-100` | not reproduced (would require building and injecting) |
| SGD2FreeRes ordinal 10991 → modal box → 20-hour loader-lock wedge | `fleet/hook_main.cpp:147-153` | not reproduced |
| SGD2FreeRes returns FALSE on Classic <1.09d (error 1114) | `patch/patches.resmod.cpp:110-121`, `docs/RESOLUTION-LADDER.md:31-37` | not reproduced |
| Two failed `..\` runtime patches, both reporting `active` | `docs/PD2-ONLINE.md:820-830` | not reproduced |
| D2's process DACL refuses `PROCESS_TERMINATE` | `patch/patchcore.cpp:1174-1181` | not reproduced |
| twelve strings / seventeen operands / 247 bytes | `docs/REMOTE.md:139-152` | **independently reproduced here** — see §3.3 |

The d2-fleet quotations of its own source code (the patch descriptor, the staging
model, the guard-bytes message, `DetourCreateProcessWithDllsW`) are verbatim from
the files at the cited lines and are claims about that project's design, not about
Diablo II.

---

## 7. Changes made to shared state

**Ghidra:** one program was imported —
`F:\D2Fleet\versions\pd2-s13\game\PD2_EXT.dll` into `/PD2Realm/PD2_EXT.dll`, with
auto-analysis. This was necessary to decode the second load path (§4.1) and is
the import the skill's method calls for when a needed module is absent. No
function was renamed, no comment added, no existing program modified.

**d2-fleet repository:** nothing. It was read only.

**No fleet member** was launched, driven, or closed. Nothing was built.

---

## 8. Open questions for the author

1. **Is the `(attributes)` patch load-bearing for modified archives?** The patch
   is certain; the consequence is not. Tracing whether 1.13c's Storm ever
   *compares* against the CRC32 table it now never loads would turn a measured
   edit into an explained one.

2. **What are the four `Game.exe` flags?** Loading retail 1.13c `Game.exe` into
   Ghidra and naming the function containing `0x4083ef` would settle it in
   minutes, and it is the one remaining "PD2 disables *something*" with no
   identified object.

3. ~~**Does `ProjectDiablo.dll` patch `D2Common`'s table loader specifically?**~~
   **Answered, and the answer is no** — see §3.9 and §4.7. The loader is stock;
   the archive priority substitutes the content and a 30-byte NOP removes the
   validation. The remaining sub-question is narrow: **how many records does
   `inventory.txt` have in `pd2data.mpq`?** Confirming it is more than 32 would
   close the loop from the mod's side rather than the game's. That needs MPQ
   tooling, which was not used in this run.

4. **What are the other 83 `D2Common` records for?** Three were characterised
   (the stat-accessor redirect, the manufactured call, the 2.5M→5M immediate).
   The rest are unexamined, and a per-record classification would say a great deal
   about what PD2 actually changes.

5. **Is `PD2_EXT.dll`'s `Fog` splice about the install path?** `Fog.dll` is where
   `GetInstallPath` lives (d2-fleet cites Fog ordinal 10116), `PD2_EXT` carries
   the string *"Please reinstall Project Diablo 2 into a valid Diablo 2 LoD
   directory"*, and the replacement function looks like string truncation. Three
   circumstantial facts pointing the same way is not a finding, and the chapter
   does not claim it.

5. **Chapter-scope question:** should the version-differences table cover the
   Classic (non-expansion) trees? The `d2exp.mpq`/expansion detection interacts
   with the `..\` redirect in ways this chapter does not touch.

---

## 9. Conventions compliance

- **1.13c as unmarked default** — the body is written in the present tense about
  1.13c with no version qualifier. Three inline `> **Version note (…):**`
  callouts (1.14+, 1.09d, Classic <1.09d) and one `## Version differences` table
  with 1.13c as the first data column.
- **Mod behaviour uses `> **Mod note (Project Diablo 2):** `** — one such callout.
  Elsewhere PD2 is the chapter's subject rather than an aside, so the mod material
  is body text by design; this is the one place the convention's letter and the
  chapter's shape diverge, and it is deliberate.
- **Evidence cited inline** — function + address, or file + line, throughout.
  No bare "verified" or "confirmed".
- **Unverified marked in place** — see §5.
- **Structure** — provenance block, narrative introduction, staged body with prose
  before every table, one worked example threaded start to finish (the arrival of
  `ProjectDiablo.dll`), reference tables, `## Version differences`, link to this
  report.
- **Vanilla data only** — every "vanilla" byte came from
  `F:\D2VersionChanger\VersionChanger\LoD\1.13c\`, never from a repo's checked-in
  copy. No game-data tables are claimed, so the `assets/excel/` hazard does not
  arise.
