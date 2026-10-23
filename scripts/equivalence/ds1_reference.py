"""Original-game DS1 loader reference: D2Common.dll 1.13c RVA 0xb560.

What the game makes of a ``.ds1`` is observed from the game's own loader,
``LoadAndParsePresetArchive`` (Ghidra diablo2:/Vanilla/1.13c/D2Common.dll,
0x6fd5b560..0x6fd5bbb2), at the stage where it returns: the 0x5c-byte preset
record its only caller (``GetOrCreatePresetFileCacheEntry``, 0x6fd5be50)
caches per file name. Two observation routes, both read-only for the game's
own data:

``natural``
    Walk the game's preset cache (head ``D2Common+0xa147c``; entries are
    ``name[0x104], refcount@0x104, record@0x108, next@0x10c``) after the game
    has loaded maps by itself, and decode each record. Nothing is called.

``direct``
    Call the loader on a buffer we supply. Its only file access is the
    ``push 0; push eax; mov eax,[esp+0x38]; call RESOURCE_AllocateAndOpen``
    sequence at RVA 0xb56b..0xb577 (12 bytes, a stdcall callee that pops its
    two arguments, so the sequence is stack-neutral). For one call that
    sequence is replaced by ``mov eax, <our buffer>; nop x7`` and put back
    immediately afterwards; the buffer holds the file plus 800 zero bytes,
    matching the loader's own ``filesize + 800`` allocation. Everything after
    the read -- every branch, remap table and allocation -- is the shipped
    code. The patch window is verified closed (original bytes read back)
    after every call; a failure to restore is fatal to the session.

The record (dword index: meaning, from the disassembly)::

    0 tagType   1 file buffer   3 width-1   4 height-1   5 wall layers
    6 floor layers   7..10 wall orientation[]   11..14 wall tiles[]
    15..16 floor tiles[]   17 shadow   18 tag (only tagType 1/2, or v<4)
    19 group count   20 group array (0x18 stride: x,y,w,h,-,unk)
    21 object list head (0x20 nodes: unit type, id, x, next, path, type, y,
       flags; prepended, so the list is in reverse file order)

Processing stage. Tile layers are pointers into the file buffer (orientation
remapped in place below version 7). Object IDs are REMAPPED by act/version
tables and objects whose mapped id is negative are dropped; type-1 objects of
version <= 4 are dropped; paths attach to the first list node (= last object
in file order) at the path's (x, y). Raw file object IDs are therefore not
observable here: ``compare_saved_map`` compares stage-invariant fields with the
intended map directly, and mapped IDs by loading an *independent* encoding of
the intended map (test/fixtures/make_ds1_fixtures.py, no editor code) through
the same loader -- both sides at the same stage, no model of the remap.

Mocked tests: test/python/test_ds1_reference.py. Live lane only with a
privileged, idle 1.13c d2-fleet member (``D2FLEET_TOKEN`` from the environment).

CLI::

    python -m scripts.equivalence.ds1_reference --contract C --baseline B \\
        --cases CASES.json --probe EXE --out-dir DIR \\
        [--fleet-root ../d2-fleet --host H --port P]

Without a fleet member every required case is ``unavailable`` (exit 2).
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import struct
import sys
from pathlib import Path

if __package__ in (None, ""):  # pragma: no cover
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from scripts.equivalence import contracts as C
    from scripts.equivalence import ds1_cases
else:
    from . import contracts as C
    from . import ds1_cases

REPO_ROOT = Path(__file__).resolve().parents[2]
GAME_SCHEMA = "ds1edit.game-preset/1"
ADAPTER_NAME = "ds1edit.ds1-loader-adapter"
ADAPTER_VERSION = "1"
RECIPE_KIND = "d2common-preset-loader/1"

MODULE = "D2Common.dll"
LOADER_RVA = 0xB560
CACHE_HEAD_RVA = 0xA147C
PATCH_RVA = 0xB56B
PATCH_ORIGINAL = bytes.fromhex("6a00508b442438e889e3ffff")
RECORD_BYTES = 0x5C
TAIL_SLACK = 800
READ_CHUNK = 4096
MAX_CELLS = 1 << 20
MAX_OBJECTS = 1 << 16
MAX_PATH_POINTS = 1 << 12


class ReferenceUnavailable(Exception):
    """A required capability (member, privilege, module, route) is missing."""


class ReferenceRefused(Exception):
    """The reference must not be used: wrong bytes, unsafe state, bad output."""


def patch_bytes(buffer_addr: int) -> bytes:
    """``mov eax, imm32`` + 7 NOPs: the 12-byte stack-neutral replacement."""
    out = b"\xb8" + struct.pack("<I", buffer_addr) + b"\x90" * 7
    assert len(out) == len(PATCH_ORIGINAL)
    return out


# ---------------------------------------------------------------------------
# Decoding a loader record from member memory
# ---------------------------------------------------------------------------

def _u32(b: bytes, off: int = 0) -> int:
    return struct.unpack_from("<I", b, off)[0]


def _i32(b: bytes, off: int = 0) -> int:
    return struct.unpack_from("<i", b, off)[0]


def read_block(mem, addr: int, n: int) -> bytes:
    """Read ``n`` bytes in route-sized chunks."""
    out = bytearray()
    off = 0
    while off < n:
        k = min(READ_CHUNK, n - off)
        out += mem.read(addr + off, k)
        off += k
    return bytes(out)


def decode_record(mem, record_addr: int) -> dict:
    """Decode one loader output record into ``ds1edit.game-preset/1``."""
    rec = mem.read(record_addr, RECORD_BYTES)
    d = [_u32(rec, 4 * i) for i in range(RECORD_BYTES // 4)]
    buf = d[1]
    if buf == 0:
        raise ReferenceRefused("record has no file buffer")
    version = _i32(mem.read(buf, 4))
    width, height = _i32(rec, 12) + 1, _i32(rec, 16) + 1
    cells = width * height
    if width <= 0 or height <= 0 or cells > MAX_CELLS:
        raise ReferenceRefused("record dimensions %dx%d are out of range" % (width, height))
    nwall, nfloor = _i32(rec, 20), _i32(rec, 24)
    if not (0 <= nwall <= 4 and 0 <= nfloor <= 2):
        raise ReferenceRefused("record layer counts %d/%d are out of range" % (nwall, nfloor))
    tag_type = _i32(rec, 0)

    def layer(ptr):
        raw = read_block(mem, ptr, 4 * cells)
        return list(struct.unpack("<%dI" % cells, raw))

    walls = [layer(d[11 + i]) for i in range(nwall)]
    orients = [layer(d[7 + i]) for i in range(nwall)]
    floors = [layer(d[15 + i]) for i in range(nfloor)]
    # below version 4 the loader points record[15] at the file's floor block
    # but never sets the floor-layer count (record[6] stays 0)
    unlisted = layer(d[15]) if (nfloor == 0 and d[15]) else None
    shadow = layer(d[17]) if d[17] else None
    tags = layer(d[18]) if d[18] else None
    out_cells = []
    for i in range(cells):
        out_cells.append({
            "walls": [[walls[l][i], orients[l][i]] for l in range(nwall)],
            "floors": [floors[l][i] for l in range(nfloor)],
            "floor0_unlisted": unlisted[i] if unlisted is not None else None,
            "shadow": shadow[i] if shadow is not None else None,
            "tag": tags[i] if tags is not None else None,
        })

    groups = None
    if d[20]:
        n = _i32(rec, 0x4C)
        if not 0 <= n <= MAX_OBJECTS:
            raise ReferenceRefused("group count %d is out of range" % n)
        raw = read_block(mem, d[20], 0x18 * n) if n else b""
        groups = [{"tile_x": _i32(raw, 0x18 * i), "tile_y": _i32(raw, 0x18 * i + 4),
                   "width": _i32(raw, 0x18 * i + 8), "height": _i32(raw, 0x18 * i + 12),
                   "unk": _u32(raw, 0x18 * i + 0x14) if version >= 13 else 0}
                  for i in range(n)]

    objs = []
    node = d[21]
    while node:
        if len(objs) >= MAX_OBJECTS:
            raise ReferenceRefused("object list does not terminate")
        b = mem.read(node, 0x20)
        o = {"unit_type": _i32(b, 0), "id": _i32(b, 4), "x": _i32(b, 8),
             "type_field": _i32(b, 0x14), "y": _i32(b, 0x18), "flags": _i32(b, 0x1C),
             "path": None}
        pp = _u32(b, 0x10)
        if pp:
            hdr = mem.read(pp, 8)
            n, arr = _i32(hdr, 0), _u32(hdr, 4)
            if not 0 <= n <= MAX_PATH_POINTS:
                raise ReferenceRefused("path point count %d is out of range" % n)
            raw = read_block(mem, arr, 12 * n) if n else b""
            o["path"] = [[_i32(raw, 12 * k + 4), _i32(raw, 12 * k + 8), _i32(raw, 12 * k)]
                         for k in range(n)]
        objs.append(o)
        node = _u32(b, 0x0C)
    objs.reverse()  # prepended by the loader: restore file order

    return {"schema": GAME_SCHEMA, "version": version, "width": width, "height": height,
            "tag_type": tag_type, "walls": nwall, "floors": nfloor,
            "cells": out_cells, "objects": objs, "groups": groups}


def walk_cache(mem, base: int, limit: int = 10000) -> list[dict]:
    """The game's own preset cache: ``[{name, refcount, record}]`` newest first."""
    head = _u32(mem.read(base + CACHE_HEAD_RVA, 4))
    out, e = [], head
    while e:
        if len(out) >= limit:
            raise ReferenceRefused("preset cache list does not terminate")
        blk = mem.read(e, 0x110)
        name = blk[:0x104].split(b"\0", 1)[0].decode("latin-1")
        out.append({"name": name, "refcount": _u32(blk, 0x104), "record": _u32(blk, 0x108)})
        e = _u32(blk, 0x10C)
    return out


# ---------------------------------------------------------------------------
# Projections compared at the same processing stage
# ---------------------------------------------------------------------------

def _dw(b4) -> int:
    return (b4[0] | (b4[1] << 8) | (b4[2] << 16) | (b4[3] << 24)) & 0xFFFFFFFF


def _paths_by_position(objects_xy: list[tuple[int, int]], paths: list[tuple]) -> list:
    return [[x, y, pts] for (x, y, pts) in paths]


def game_invariant(g: dict) -> dict:
    """Stage-invariant view of a loader record (no remapped IDs)."""
    paths = [[o["x"], o["y"], o["path"]] for o in g["objects"] if o["path"] is not None]
    return {
        "version": g["version"], "width": g["width"], "height": g["height"],
        "tag_type": g["tag_type"], "walls": g["walls"], "floors": g["floors"],
        "cells": [{k: c[k] for k in ("walls", "floors", "shadow", "tag", "floor0_unlisted")}
                  for c in g["cells"]],
        "objects": [[o["x"], o["y"], o["flags"]] for o in g["objects"]],
        "groups": g["groups"],
        "paths": sorted(paths),
    }


def game_has_tag_layer(version: int, tag_type: int) -> bool:
    return version < 4 or tag_type in (1, 2)


def game_reads_groups(version: int, tag_type: int) -> bool:
    return version >= 12 and tag_type in (1, 2)


def editor_invariant(snap: dict, version: int | None = None) -> dict:
    """The same view of an editor snapshot (``ds1edit.map/1``), as the loader
    would decode the file the editor describes. ``version`` overrides the
    snapshot's (a saved map is version 18 whatever it was read from)."""
    v = snap["version"] if version is None else version
    tt = snap.get("tag_type", 0) if v >= 10 else 0
    has_tag = game_has_tag_layer(v, tt)
    legacy = v < 4  # native quirk: one floor block, floor-layer count 0
    cells = []
    for c in snap["cells"]:
        cells.append({
            "walls": [[_dw(w[:4]), w[4]] for w in c["walls"]],
            "floors": [] if legacy else [_dw(f) for f in c["floors"]],
            "floor0_unlisted": _dw(c["floors"][0]) if legacy and c["floors"] else None,
            "shadow": _dw(c["shadows"][0]) if c["shadows"] else None,
            "tag": (c["tags"][0] if c["tags"] else 0) if has_tag else None,
        })
    objects = [o for o in snap["objects"] if not (o["type"] == 1 and v <= 4)]
    paths = []
    if v >= 14:
        for p in snap.get("paths", []):
            paths.append([p["x"], p["y"], [list(q) if v >= 15 else [q[0], q[1], 1]
                                           for q in p["points"]]])
    return {
        "version": v, "width": snap["width"], "height": snap["height"], "tag_type": tt,
        "walls": snap["layers"]["walls"], "floors": 0 if legacy else snap["layers"]["floors"],
        "cells": cells,
        "objects": [[o["x"], o["y"], o["flags"] if v >= 6 else 0] for o in objects],
        "groups": ([{k: g[k] for k in ("tile_x", "tile_y", "width", "height", "unk")}
                    for g in snap.get("groups", [])] if game_reads_groups(v, tt) else None),
        "paths": sorted(paths),
    }


def diff(expected, actual, path="", limit=64) -> list[dict]:
    """First differences as ``[{path, expected, actual}]``."""
    out: list[dict] = []

    def walk(e, a, p):
        if len(out) >= limit:
            return
        if isinstance(e, dict) and isinstance(a, dict):
            for k in sorted(set(e) | set(a)):
                walk(e.get(k, "<absent>"), a.get(k, "<absent>"), "%s.%s" % (p, k) if p else k)
        elif isinstance(e, list) and isinstance(a, list):
            if len(e) != len(a):
                out.append({"path": p + ".length", "expected": len(e), "actual": len(a)})
            for i, (x, y) in enumerate(zip(e, a)):
                walk(x, y, "%s[%d]" % (p, i))
        elif e != a:
            out.append({"path": p, "expected": e, "actual": a})
    walk(expected, actual, path)
    return out


# ---------------------------------------------------------------------------
# Independent reference encoding of an intended map
# ---------------------------------------------------------------------------

def _fixtures_module():
    spec = importlib.util.spec_from_file_location(
        "make_ds1_fixtures", REPO_ROOT / "test" / "fixtures" / "make_ds1_fixtures.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def encode_reference(snap: dict) -> bytes:
    """Encode an intended map as version 18 with the independent fixture
    encoder (shares no code with src/core/ds1*.c)."""
    fx = _fixtures_module()
    spec = spec_from_snapshot(snap)
    data, layout = fx.encode(spec)
    data = bytearray(data)
    # the fixture encoder pads orientation dwords with 0xDD; the editor's
    # writer pads with zero -- use zero so only interpreted bytes differ
    cells = spec["width"] * spec["height"]
    for l in range(spec["wall_num"]):
        o = layout["layers_at"] + (2 * l + 1) * cells * 4
        for i in range(cells):
            data[o + 4 * i + 1:o + 4 * i + 4] = b"\0\0\0"
    fx.check_layout(spec, bytes(data), layout)
    return bytes(data)


def v18_layout(snap: dict) -> dict:
    """Section offsets of the v18 file that describes ``snap`` (closed-form
    formula of the independent fixture module)."""
    return _fixtures_module().expected_layout(spec_from_snapshot(snap))


def spec_from_snapshot(snap: dict) -> dict:
    """A make_ds1_fixtures spec (version 18) for a ``ds1edit.map/1`` map."""
    w, h = snap["width"], snap["height"]

    def grid(fn):
        rows = []
        for y in range(h):
            rows.append([fn(snap["cells"][y * w + x]) for x in range(w)])
        return rows
    nw, nf = snap["layers"]["walls"], snap["layers"]["floors"]
    tt = snap.get("tag_type", 0)
    spec = {
        "version": 18, "width": w, "height": h, "act": snap.get("act", 1), "tag_type": tt,
        "files": list(snap.get("files", [])), "skip": [0, 0],
        "wall_num": nw, "floor_num": nf,
        "walls": [grid(lambda c, l=l: list(c["walls"][l][:4])) for l in range(nw)],
        "orient": [grid(lambda c, l=l: c["walls"][l][4]) for l in range(nw)],
        "floors": [grid(lambda c, l=l: list(c["floors"][l])) for l in range(nf)],
        "shadow": grid(lambda c: list(c["shadows"][0]) if c["shadows"] else [0, 0, 0, 0]),
        "tags": grid(lambda c: c["tags"][0] if c["tags"] else 0),
        "objects": [{k: o[k] for k in ("type", "id", "x", "y", "flags")} for o in snap["objects"]],
        "pre_group": 0,
        "groups": [{k: g[k] for k in ("tile_x", "tile_y", "width", "height", "unk")}
                   for g in snap.get("groups", [])],
        "npcs": [{"x": p["x"], "y": p["y"], "points": [list(q) for q in p["points"]]}
                 for p in snap.get("paths", [])],
    }
    return spec


# ---------------------------------------------------------------------------
# Live session
# ---------------------------------------------------------------------------

def load_transport(fleet_root: Path):
    path = Path(fleet_root) / "reimpl" / "prove.py"
    if not path.is_file():
        raise ReferenceUnavailable("d2-fleet transport not found at %s" % path)
    data = path.read_bytes()
    spec = importlib.util.spec_from_file_location("d2fleet_prove", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    if not hasattr(mod, "Member") or not hasattr(mod, "Refused"):
        raise ReferenceUnavailable("%s lacks Member/Refused" % path)
    return mod, hashlib.sha256(data).hexdigest()


class LoaderSession:
    """One member; the shipped loader, verified before every use."""

    def __init__(self, member, baseline: dict, transport_sha256: str = "0" * 64,
                 refused=Exception):
        self.m = member
        self.baseline = C.validate_baseline(baseline)
        self.transport_sha256 = transport_sha256
        self._refused = refused
        self.contract = None
        self.recipe = None
        self.base = None
        self.record = self.buffer = None
        self.capacity = 0
        self.member_info = {}
        self.calls = 0
        self.patched = False

    @classmethod
    def connect(cls, fleet_root: Path, host: str, port: int, token: str, baseline: dict):
        mod, sha = load_transport(fleet_root)
        return cls(mod.Member(host, int(port), token, timeout=60), baseline, sha, mod.Refused)

    def _req(self, fn, *a, **kw):
        try:
            return fn(*a, **kw)
        except self._refused as exc:  # transport refusal -> unavailable
            raise ReferenceUnavailable("member refused: %s" % str(exc)[:160])

    def read(self, addr, n):
        return self._req(self.m.read, addr, n)

    def adapter_record(self) -> dict:
        return {"name": ADAPTER_NAME, "version": ADAPTER_VERSION,
                "abi_recipe_digest": C.payload_sha256(self.recipe) if self.recipe else None,
                "recipe_kind": RECIPE_KIND,
                "transport": "d2-fleet reimpl/prove.py Member",
                "transport_sha256": self.transport_sha256}

    def open(self, contract: dict, capacity: int) -> dict:
        C.validate_contract(contract)
        if contract["baseline_digest"] != self.baseline["digest"]:
            raise ReferenceRefused("contract baseline is not this session's baseline")
        ref = contract["reference"]
        recipe = ref.get("abi_recipe")
        if not isinstance(recipe, dict) or recipe.get("kind") != RECIPE_KIND:
            raise ReferenceRefused("contract carries no %s recipe; refusing to guess" % RECIPE_KIND)
        if C.payload_sha256(recipe) != ref["abi_recipe_digest"]:
            raise ReferenceRefused("ABI recipe does not match the contract's digest")
        inst = self.m.instance()
        if not inst.get("ok"):
            raise ReferenceUnavailable("member did not answer /instance: %s"
                                       % str(inst.get("error"))[:160])
        if inst.get("tier") != "privileged":
            raise ReferenceUnavailable("member tier is %r; the direct route needs privileged"
                                       % inst.get("tier"))
        self.member_info = {k: inst.get(k) for k in ("slot", "version", "exeSha256", "tier", "pid")}
        mods = self.m.modules().get("modules", [])
        hit = [x for x in mods if x.get("name", "").lower() == MODULE.lower()]
        if not hit:
            raise ReferenceUnavailable("%s is not mapped (the member must be in a game)" % MODULE)
        base = int(hit[0]["base"], 16)
        preferred = int(hit[0].get("preferred_base") or "0", 16) or base
        if base != preferred:
            raise ReferenceRefused("%s is relocated; recipe addresses assume the preferred base"
                                   % MODULE)
        binary = C.baseline_binary(self.baseline, MODULE)
        pe = (binary or {}).get("pe") or {}
        text = pe.get("sections", {}).get(".text")
        if not text:
            raise ReferenceRefused("baseline has no .text identity for %s" % MODULE)
        h = hashlib.sha256()
        rva0, n = int(text["rva"], 16), int(text["bytes"])
        for off in range(0, n, READ_CHUNK):
            h.update(self._req(self.m.read_rva, MODULE, rva0 + off, min(READ_CHUNK, n - off)))
        if h.hexdigest() != text["sha256"]:
            raise ReferenceRefused("live %s .text does not match the baseline binary "
                                   "(or a patch is already in place)" % MODULE)
        fn = self._req(self.m.read_rva, MODULE, LOADER_RVA, int(recipe["function_length"]))
        if hashlib.sha256(fn).hexdigest() != ref["function_bytes_sha256"]:
            raise ReferenceRefused("live loader bytes do not match the contract")
        self.base = base
        self.contract, self.recipe = contract, recipe
        self.capacity = int(capacity) + TAIL_SLACK
        self.record = self._req(self.m.alloc, RECORD_BYTES)
        self.buffer = self._req(self.m.alloc, self.capacity)
        return {"module_base": "0x%08x" % base, "member": dict(self.member_info),
                "record": "0x%08x" % self.record, "buffer": "0x%08x" % self.buffer}

    # -- natural route ---------------------------------------------------

    def natural(self) -> list[dict]:
        if self.base is None:
            raise ReferenceRefused("natural() before open()")
        return walk_cache(_Reader(self), self.base)

    def decode(self, record_addr: int) -> dict:
        return decode_record(_Reader(self), record_addr)

    # -- direct route ----------------------------------------------------

    def _write(self, addr, data: bytes):
        for off in range(0, len(data), 2048):  # query-string sized
            self._req(self.m.write, addr + off, data[off:off + 2048])

    def load(self, data: bytes) -> dict:
        """Run the shipped loader on ``data``; return its decoded record."""
        if self.recipe is None:
            raise ReferenceRefused("load() before open()")
        if len(data) + TAIL_SLACK > self.capacity:
            raise ReferenceRefused("input of %d bytes exceeds the session buffer" % len(data))
        self._write(self.buffer, data + b"\0" * TAIL_SLACK)
        self._write(self.record, b"\0" * RECORD_BYTES)
        site = self.base + PATCH_RVA
        live = self.read(site, len(PATCH_ORIGINAL))
        if live != PATCH_ORIGINAL:
            raise ReferenceRefused("patch site holds %s, not the shipped bytes" % live.hex())
        reply = None
        try:
            self._req(self.m.write, site, patch_bytes(self.buffer))
            self.patched = True
            try:
                reply = self.m.call(MODULE, LOADER_RVA, ecx=0, edx=0,
                                    stack=[self.record, 0], cdecl=False,
                                    timeout_ms=int(self.recipe.get("timeout_ms", 10000)))
            except self._refused as exc:
                raise ReferenceUnavailable("call refused: %s" % str(exc)[:160])
        finally:
            self._req(self.m.write, site, PATCH_ORIGINAL)
            back = self.read(site, len(PATCH_ORIGINAL))
            if back != PATCH_ORIGINAL:
                raise ReferenceRefused("patch site was not restored (%s); stop using this "
                                       "member" % back.hex())
            self.patched = False
        self.calls += 1
        if reply.get("seh") or reply.get("faulted") or reply.get("timedOut") \
                or reply.get("timed_out"):
            raise ReferenceUnavailable("loader faulted/timed out: %s" % {
                k: reply.get(k) for k in ("faulted", "faultCode", "faultAddr", "timedOut")})
        rec = self.read(self.record, RECORD_BYTES)
        if _u32(rec, 4) != self.buffer:
            raise ReferenceRefused("record does not point at the supplied buffer")
        return self.decode(self.record)

    def close(self):
        """Scratch is the session's own; the patch site is already restored."""


class _Reader:
    def __init__(self, s):
        self.s = s

    def read(self, addr, n):
        return self.s.read(addr, n)


# ---------------------------------------------------------------------------
# Cases
# ---------------------------------------------------------------------------

def _sha_file(p: Path) -> str:
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def _case(case_id, contract, baseline_digest, status, failures, provenance, extra=None,
          input_sha=None, expected_sha=None, actual_sha=None, coverage=None):
    lim = list(contract["limitations"]) if contract else []
    c = {
        "schema": C.CASE_SCHEMA, "id": case_id, "lane": "live-reference",
        "platform": provenance.get("platform", ""),
        "required": bool(contract.get("required", True)) if contract else True,
        "status": status,
        "success_outcome": contract.get("success_outcome", "pass") if contract else None,
        "contract_digest": contract.get("digest") if contract else None,
        "baseline_digest": baseline_digest,
        "input_sha256": input_sha, "expected_sha256": expected_sha, "actual_sha256": actual_sha,
        "coverage": coverage or {}, "failures": failures[:64],
        "provenance": {"candidate": provenance.get("candidate", {}),
                       "reference": provenance.get("reference", {})},
        "limitations": lim,
    }
    if extra:
        c.update(extra)
    return c


def compare_saved_map(input_path: Path, expected_map: dict, contract: dict, session,
                      probe_exe: Path, out_dir: Path, case_id: str | None = None,
                      provenance: dict | None = None, editor_reader=None) -> dict:
    """Compare a map the editor saved with the intended map, through the
    editor's reader AND the original loader.

    Checks, all at declared stages:
      1. editor reads ``input_path`` as ``expected_map`` (contract's editor policy);
      2. original loader on ``input_path``: stage-invariant fields equal the
         intended map's (``editor_invariant`` at version 18);
      3. original loader on ``input_path`` equals the original loader on an
         independent v18 encoding of ``expected_map`` -- every decoded field,
         including remapped object IDs and path attachment.
    ``session`` None (or one that raises ReferenceUnavailable) -> unavailable.
    ``editor_reader(path) -> snapshot`` replaces the probe (tests only).
    """
    input_path, out_dir = Path(input_path), Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    provenance = dict(provenance or {})
    cid = case_id or "ds1-loader/" + input_path.name
    bd = contract.get("baseline_digest")
    data = input_path.read_bytes()
    in_sha = hashlib.sha256(data).hexdigest()
    exp_sha = C.payload_sha256(expected_map)
    policy = {"normalizations": list(contract.get("editor_policy", {}).get(
        "normalizations", ["writer_v18_upgrade", "source_ignored"]))}
    failures: list[dict] = []
    if session is None:
        return _case(cid, contract, bd, "unavailable",
                     [{"kind": "unavailable", "detail": "no original-loader session"}],
                     provenance, input_sha=in_sha, expected_sha=exp_sha)
    if session is not None and getattr(session, "adapter_record", None):
        provenance.setdefault("reference", {})["adapter"] = session.adapter_record()

    # 1. the editor's own reading of the saved file
    if editor_reader is not None:
        rc, snap = 0, editor_reader(input_path)
    else:
        rc, snap = ds1_cases.run_probe(Path(probe_exe), input_path.resolve(),
                                       (out_dir / (input_path.stem + ".editor.json")).resolve())
    if snap is None or (rc is not None and (rc & 0xFFFFFFFF) == 0xC0000135):
        return _case(cid, contract, bd, "unavailable",
                     [{"kind": "probe", "detail": "editor probe exit %s" % rc}], provenance,
                     input_sha=in_sha, expected_sha=exp_sha)
    res = ds1_cases.compare_maps(expected_map, snap, policy)
    for d in res["differences"][:16]:
        failures.append({"kind": "editor_read", "path": d.split(":")[0], "detail": d})

    try:
        g_saved = session.load(data)
        ref_bytes = encode_reference(expected_map)
        g_ref = session.load(ref_bytes)
    except ReferenceUnavailable as e:
        return _case(cid, contract, bd, "unavailable",
                     [{"kind": "unavailable", "detail": str(e)}], provenance,
                     input_sha=in_sha, expected_sha=exp_sha)
    with open(out_dir / (input_path.stem + ".game.json"), "w", encoding="utf-8") as fh:
        json.dump({"saved": g_saved, "reference_encoding": g_ref}, fh, indent=1)

    # 2. stage-invariant fields vs the intended map
    for f in diff(editor_invariant(expected_map, version=18), game_invariant(g_saved)):
        failures.append({"kind": "game_invariant", **f})
    # 3. same-stage full record vs the independent encoding
    for f in diff(g_ref, g_saved):
        failures.append({"kind": "game_same_stage", **f})

    status = "mismatch" if failures else "pass"
    uncovered = sorted(n for n, ax in contract["domain"]["axes"].items()
                       if ax["required"] and ax["coverage"] in C.UNCOVERED_AXIS)
    if status == "pass" and uncovered:
        status = "partial"
    return _case(cid, contract, bd, status, failures, provenance,
                 input_sha=in_sha, expected_sha=exp_sha,
                 actual_sha=C.payload_sha256(game_invariant(g_saved)),
                 coverage={"editor_read": res["status"], "uncovered_required_axes": uncovered,
                           "objects": len(g_saved["objects"]),
                           "reference_encoding_sha256": hashlib.sha256(ref_bytes).hexdigest()})


def compare_vanilla(editor_snap: dict, g: dict) -> list[dict]:
    """Original loader vs editor reader on one unedited vanilla file."""
    return diff(editor_invariant(editor_snap), game_invariant(g))


def classify_vanilla_mismatch(editor_snap: dict, g: dict) -> str | None:
    """Name a known discrepancy class when it explains EVERY difference.

    ``editor_drops_out_of_bounds_objects``: the editor's reader discards
    objects whose sub-tile position lies outside width*5 x height*5
    (src/core/ds1.c, "integrity check (not done by the game I believe)"); the
    loader keeps them (and any path attached to them). Nothing else differs.
    """
    w5, h5 = 5 * g["width"], 5 * g["height"]
    inside = dict(g)
    inside["objects"] = [o for o in g["objects"]
                         if 0 <= o["x"] < w5 and 0 <= o["y"] < h5]
    if len(inside["objects"]) != len(g["objects"]) and not compare_vanilla(editor_snap, inside):
        return "editor_drops_out_of_bounds_objects"
    return None


def run_corpus(session, sweep_dir: Path, out_dir: Path, contract: dict,
               provenance: dict | None = None, limit: int | None = None) -> dict:
    """Every DS1 the sweep (ds1_sweep.py) read: its source bytes through the
    original loader vs the editor's reading of the same bytes. One case per
    sweep case; a case the sweep could not read stays not_run."""
    sweep_dir, out_dir = Path(sweep_dir), Path(out_dir)
    (out_dir / "cases").mkdir(parents=True, exist_ok=True)
    provenance = dict(provenance or {})
    provenance.setdefault("reference", {})["adapter"] = session.adapter_record()
    provenance["reference"]["member"] = session.member_info
    files = sorted((sweep_dir / "cases").glob("*.json"))[:limit]
    counts: dict[str, int] = {}
    classes: dict[str, int] = {}
    mism = []
    for f in files:
        sc = json.loads(f.read_text(encoding="utf-8"))
        vp = sc["ds1"]["path"]
        slug = f.stem
        cid = "ds1-loader-vanilla/" + vp.replace("\\", "/")
        src = sweep_dir / "corpus" / Path(*vp.split("\\"))
        snap_p = sweep_dir / "work" / slug / "read.json"
        if sc.get("read", {}).get("status") != "ok" or not src.is_file() or not snap_p.is_file():
            case = _case(cid, contract, contract["baseline_digest"], "not_run",
                         [{"kind": "not_run", "detail": "editor did not read the file"}],
                         provenance)
        else:
            data = src.read_bytes()
            snap = json.loads(snap_p.read_text(encoding="utf-8"))
            try:
                g = session.load(data)
                fails = [{"kind": "game_invariant", **x} for x in compare_vanilla(snap, g)]
                cls = classify_vanilla_mismatch(snap, g) if fails else None
                if cls:
                    oob = [o for o in g["objects"] if not (0 <= o["x"] < 5 * g["width"]
                                                           and 0 <= o["y"] < 5 * g["height"])]
                    fails.insert(0, {"kind": "classified", "class": cls,
                                     "objects_dropped_by_editor": oob})
                case = _case(cid, contract, contract["baseline_digest"],
                             "mismatch" if fails else "pass", fails, provenance,
                             input_sha=hashlib.sha256(data).hexdigest(),
                             expected_sha=C.payload_sha256(editor_invariant(snap)),
                             actual_sha=C.payload_sha256(game_invariant(g)),
                             coverage={"version": g["version"], "objects": len(g["objects"]),
                                       "groups_read": g["groups"] is not None,
                                       "paths": sum(1 for o in g["objects"] if o["path"])},
                             extra={"comparison": "vanilla bytes: original loader vs editor "
                                                  "reader (no edit, no writer)"})
            except ReferenceUnavailable as e:
                case = _case(cid, contract, contract["baseline_digest"], "unavailable",
                             [{"kind": "unavailable", "detail": str(e)}], provenance)
        counts[case["status"]] = counts.get(case["status"], 0) + 1
        if case["status"] == "mismatch":
            cls = case["failures"][0].get("class") if case["failures"] else None
            classes[cls or "unclassified"] = classes.get(cls or "unclassified", 0) + 1
            mism.append({"id": cid, "class": cls, "failures": case["failures"][:4]})
        with open(out_dir / "cases" / (slug + ".json"), "w", encoding="utf-8",
                  newline="\n") as fh:
            json.dump(case, fh, indent=1, sort_keys=True)
    agg = {"schema": "ds1edit.ds1-loader-corpus/1", "contract_digest": contract["digest"],
           "cases": len(files), "status_counts": counts, "mismatch_classes": classes,
           "mismatches": mism,
           "loader_calls": session.calls, "member": session.member_info,
           "adapter": session.adapter_record()}
    with open(out_dir / "aggregate.json", "w", encoding="utf-8", newline="\n") as fh:
        json.dump(agg, fh, indent=2, sort_keys=True)
    return agg


# ---------------------------------------------------------------------------
# Feature matrix (design section 10: every distinct writer feature)
# ---------------------------------------------------------------------------

def _matrix_specs(fx) -> list[tuple[str, str, dict]]:
    """(id, feature, source spec). Edits are authored into the source; the
    editor's checked writer carries them into the saved v18 file."""
    out = []
    # format upgrade from every version family vanilla ships
    for v in (3, 8, 12, 13, 15, 16, 17, 18):
        out.append(("upgrade_v%02d" % v, "format_upgrade_to_v18", fx.default_spec(v)))
    s = fx.default_spec(18)
    s["wall_num"], s["floor_num"] = 4, 2
    out.append(("layers_4w_2f", "wall_layers_1_to_4,floor_layers_1_to_2", s))
    s = fx.default_spec(16)
    s["wall_num"], s["floor_num"] = 1, 1
    out.append(("layers_1w_1f_v16", "wall_layers_1_to_4,floor_layers_1_to_2", s))
    s = fx.default_spec(18)
    s["width"], s["height"], s["wall_num"] = 5, 5, 4
    for layer in range(4):
        s["walls"][layer] = [[[0x10 + layer, y, x, 0x40] for x in range(5)] for y in range(5)]
        s["orient"][layer] = [[(y * 5 + x + layer) % 25 for x in range(5)] for y in range(5)]
    s["floors"] = [[[[0x90, y, x, 1] for x in range(5)] for y in range(5)] for _ in range(2)]
    s["shadow"] = [[[0xD0, y, x, 2] for x in range(5)] for y in range(5)]
    s["tags"] = [[(y * 5 + x) | 0x100 for x in range(5)] for y in range(5)]
    s["objects"], s["npcs"], s["groups"] = [], [], []
    out.append(("orientations_0_24", "orientations", s))
    s = fx.default_spec(6)
    s["width"], s["height"], s["objects"] = 5, 5, []
    for layer in range(4):
        s["walls"][layer] = [[[0x10 + layer, y, x, 0x40] for x in range(5)] for y in range(5)]
        s["orient"][layer] = [[y * 5 + x for x in range(5)] for y in range(5)]
    s["floors"] = [[[[0x90, y, x, 1] for x in range(5)] for y in range(5)] for _ in range(2)]
    s["shadow"] = [[[0xD0, y, x, 2] for x in range(5)] for y in range(5)]
    out.append(("orientations_legacy_v06", "orientations,format_upgrade_to_v18", s))
    s = fx.default_spec(18)
    s["objects"] = [
        {"type": 2, "id": 0, "x": 1, "y": 1, "flags": 0},
        {"type": 2, "id": 11, "x": 13, "y": 8, "flags": 0x7FFF0001},
        {"type": 2, "id": 149, "x": 2, "y": 5, "flags": 0x20},
        {"type": 2, "id": 151, "x": 3, "y": 3, "flags": -1},
        {"type": 1, "id": 4, "x": 6, "y": 2, "flags": 0x40},
        {"type": 1, "id": 37, "x": 7, "y": 4, "flags": 0},
        {"type": 2, "id": 11, "x": 13, "y": 8, "flags": 2},
    ]
    out.append(("objects_flags", "objects_and_flags,object_id_remap", s))
    for tt in (1, 2):
        s = fx.default_spec(18)
        s["tag_type"] = tt
        s["groups"].append({"tile_x": 2, "tile_y": 1, "width": 1, "height": 1,
                            "unk": 0xFFFFFFFF})
        out.append(("groups_tags_tt%d" % tt, "groups_and_tags", s))
    s = fx.default_spec(18)
    s["tag_type"] = 0
    s["groups"] = []
    out.append(("tag_type_0", "groups_and_tags", s))
    s = fx.default_spec(18)
    s["objects"] = [{"type": 1, "id": 4, "x": 6, "y": 2, "flags": 0},
                    {"type": 2, "id": 11, "x": 13, "y": 8, "flags": 0},
                    {"type": 1, "id": 9, "x": 10, "y": 3, "flags": 0}]
    s["npcs"] = [{"x": 6, "y": 2, "points": [[7, 3, 1], [9, 4, 2], [11, 1, 13]]},
                 {"x": 10, "y": 3, "points": [[19, 2, 0], [-1, 40, 255]]}]
    out.append(("paths_actions", "paths_and_actions", s))
    s = fx.default_spec(18)
    s["walls"][0][1][2] = [0xEE, 0x01, 0x7F, 0x80]
    s["floors"][1][0][0] = [0x00, 0x00, 0x00, 0x00]
    s["objects"][0]["x"] += 5
    s["npcs"][0]["points"][1] = [99, 98, 7]
    out.append(("edited_values", "edited_values", s))
    return out


def _expected_after_writer(fx, spec) -> dict:
    exp = fx.expected_snapshot(spec)
    if spec["version"] < 4:  # declared: the writer emits no tag layer for tagType 0
        exp = ds1_cases.canonical_map(exp, {"normalizations": ["legacy_tag_layer_dropped"]})
    return exp


def _swap_wall_layers(data: bytes, snap: dict) -> bytes | None:
    lay = v18_layout(snap)
    if lay["end"] != len(data) or snap["layers"]["walls"] < 2:
        return None
    cells = snap["width"] * snap["height"]
    blk = 2 * cells * 4
    a = lay["layers_at"]
    return data[:a] + data[a + blk:a + 2 * blk] + data[a:a + blk] + data[a + 2 * blk:]


def _omit_first_object_flags(data: bytes, snap: dict) -> bytes | None:
    lay = v18_layout(snap)
    if lay["end"] != len(data) or not snap["objects"]:
        return None
    at = lay["objects_at"] + 4 + 16
    return data[:at] + data[at + 4:]


def build_matrix(out_dir: Path, probe_exe: Path, corpus_files: list[Path] = ()) -> list[dict]:
    """Write source maps, have the editor save each (probe ``resave``), and
    return the case list ``[{id, feature, input, expected, control}]``."""
    fx = _fixtures_module()
    out_dir = Path(out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    cases = []

    def saved_case(cid, feature, src_bytes, expected):
        src = out_dir / (cid + ".src.ds1")
        saved = out_dir / (cid + ".saved.ds1")
        src.write_bytes(src_bytes)
        for p in (saved, saved.with_suffix(".ds1.bak")):
            if p.exists():
                p.unlink()
        rc = ds1_cases.run_resave(Path(probe_exe).resolve(), src, saved)
        if rc != 0 or not saved.is_file():
            raise RuntimeError("editor could not save %s (exit %s)" % (cid, rc))
        exp_p = out_dir / (cid + ".expected.json")
        exp_p.write_text(json.dumps(expected, indent=1), encoding="utf-8")
        cases.append({"id": "ds1-loader/" + cid, "feature": feature, "input": str(saved),
                      "expected": str(exp_p), "control": False})
        return saved

    for cid, feature, spec in _matrix_specs(fx):
        data = fx.case_bytes({"spec": spec, "truncate": None, "append": 0})
        saved_case(cid, feature, data, _expected_after_writer(fx, spec))

    # real vanilla files of each family, re-saved by the editor
    for p in corpus_files:
        p = Path(p)
        tmp = out_dir / (p.stem + ".vanilla.json")
        rc, snap = ds1_cases.run_probe(Path(probe_exe).resolve(), p.resolve(), tmp)
        if snap is None or snap.get("status") != "ok":
            raise RuntimeError("editor could not read %s" % p)
        if snap["version"] < 4:
            snap = ds1_cases.canonical_map(snap, {"normalizations": ["legacy_tag_layer_dropped"]})
        saved_case("vanilla_%s_v%02d" % (p.stem.lower(), snap["version"]),
                   "format_upgrade_to_v18",
                   p.read_bytes(), snap)

    # controls: corrupt a correctly saved file; the comparison must fail
    base = next(c for c in cases if c["id"].endswith("layers_4w_2f"))
    snap = json.loads(Path(base["expected"]).read_text(encoding="utf-8"))
    data = Path(base["input"]).read_bytes()
    for cid, mut in (("control_wrong_layer_order", _swap_wall_layers),
                     ("control_omitted_object_flags", _omit_first_object_flags)):
        bad = mut(data, snap)
        if bad is None:
            raise RuntimeError("could not build %s" % cid)
        p = out_dir / (cid + ".ds1")
        p.write_bytes(bad)
        cases.append({"id": "ds1-loader/" + cid, "feature": "control", "input": str(p),
                      "expected": base["expected"], "control": True})
    (out_dir / "cases.json").write_text(json.dumps(cases, indent=1), encoding="utf-8")
    return cases


def control_verdict(case: dict) -> str:
    """A control passes only by failing ON THE GAME SIDE (the original loader's
    reading disagrees with the intended map); an editor-only failure is not
    enough."""
    if case["status"] != "mismatch":
        return "CONTROL_DID_NOT_FAIL" if case["status"] == "pass" else "control_" + case["status"]
    if not any(f.get("kind") in ("game_invariant", "game_same_stage") for f in case["failures"]):
        return "CONTROL_FAILED_ONLY_IN_EDITOR"
    return "failed_as_required"


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="DS1 original-loader comparison")
    ap.add_argument("--contract", type=Path,
                    default=REPO_ROOT / "test/equivalence/contracts/ds1-113c.json")
    ap.add_argument("--baseline", required=True, type=Path)
    ap.add_argument("--cases", type=Path,
                    help="JSON list of {id, input, expected[, control]} (paths)")
    ap.add_argument("--build-matrix", action="store_true",
                    help="author the section-10 feature matrix + controls under "
                         "OUT_DIR/matrix (editor-saved via the probe) and use it")
    ap.add_argument("--vanilla", type=Path, nargs="*", default=[],
                    help="vanilla .ds1 files (local corpus) to re-save in the matrix")
    ap.add_argument("--probe", required=True, type=Path)
    ap.add_argument("--out-dir", required=True, type=Path)
    ap.add_argument("--fleet-root", type=Path)
    ap.add_argument("--host")
    ap.add_argument("--port", type=int)
    args = ap.parse_args(argv)
    if os.name == "nt":  # probe loader errors -> exit codes, not modal dialogs
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    contract = C.load_contract(args.contract)
    baseline = C.load_baseline(args.baseline)
    if args.build_matrix:
        cases = build_matrix(args.out_dir / "matrix", args.probe, args.vanilla)
    elif args.cases:
        cases = json.loads(args.cases.read_text(encoding="utf-8"))
    else:
        ap.error("--cases or --build-matrix is required")
    session, why = None, "no fleet member selected (--fleet-root/--host/--port)"
    if args.fleet_root and args.host and args.port:
        token = os.environ.get("D2FLEET_TOKEN", "")
        try:
            session = LoaderSession.connect(args.fleet_root, args.host, args.port, token, baseline)
            biggest = max(Path(c["input"]).stat().st_size for c in cases) if cases else 0
            session.open(contract, max(biggest, 1 << 16) * 2)
        except (ReferenceUnavailable, ReferenceRefused) as e:
            session, why = None, str(e)
    results = []
    (args.out_dir / "cases").mkdir(parents=True, exist_ok=True)
    for c in cases:
        exp = json.loads(Path(c["expected"]).read_text(encoding="utf-8"))
        r = compare_saved_map(Path(c["input"]), exp, contract, session, args.probe,
                              args.out_dir / "work", case_id=c["id"])
        if session is None:
            r["failures"] = [{"kind": "unavailable", "detail": why}]
        r["feature"] = c.get("feature")
        if c.get("control"):
            r["control"] = True
            r["control_verdict"] = control_verdict(r)
        results.append(r)
        with open(args.out_dir / "cases" / (c["id"].replace("/", "_") + ".json"), "w",
                  encoding="utf-8", newline="\n") as fh:
            json.dump(r, fh, indent=1, sort_keys=True)
    counts: dict[str, int] = {}
    bad = 0
    for r in results:
        key = r["control_verdict"] if r.get("control") else r["status"]
        counts[key] = counts.get(key, 0) + 1
        if r.get("control"):
            bad += r["control_verdict"] != "failed_as_required"
        else:
            bad += r["status"] != "pass"
    summary = {"contract_digest": contract["digest"], "counts": counts,
               "member": session.member_info if session else None,
               "loader_calls": session.calls if session else 0,
               "unavailable_reason": None if session else why,
               "cases": [{"id": r["id"], "feature": r.get("feature"), "status": r["status"],
                          "control_verdict": r.get("control_verdict"),
                          "first_failures": r["failures"][:3]} for r in results]}
    with open(args.out_dir / "summary.json", "w", encoding="utf-8", newline="\n") as fh:
        json.dump(summary, fh, indent=1, sort_keys=True)
    print(json.dumps(counts))
    if session is None:
        return 2
    return 1 if bad else 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
