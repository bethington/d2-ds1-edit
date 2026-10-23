"""Invoke an original 1.13c function on a d2-fleet member, for reference capture.

This module does not contain an HTTP client. It loads the installed d2-fleet
checkout's ``reimpl/prove.py`` by explicit path and uses its ``Member``
transport, ``Refused`` error, and ``bytes_agree`` relocation-aware byte check.
The transport file's SHA-256 is recorded in every adapter record.

Local/live lane only. Portable tests use a mocked member (``member_factory``).
The token is held in memory for the transport and is never placed in an
adapter record, capture, log line, or exception message.
"""

from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path

if __package__ in (None, ""):  # pragma: no cover
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from scripts.equivalence import contracts as C
else:
    from . import contracts as C

ADAPTER_NAME = "ds1edit.fleet-adapter"
ADAPTER_VERSION = "1"

#: ABI recipe kinds this adapter knows how to marshal.
SUPPORTED_RECIPES = ("stdcall-palette-nearest/1",)


class ReferenceUnavailable(Exception):
    """A required capability (member, privilege, module, transport) is missing."""


class ReferenceRefused(Exception):
    """The reference must not be used: wrong bytes/module/recipe, or bad output."""


class _MemberRefused(Exception):
    """Stand-in for prove.Refused when a factory supplies a mocked member."""


def load_transport(fleet_root: Path):
    """Load ``<fleet_root>/reimpl/prove.py``; return (module, file sha256)."""
    path = Path(fleet_root) / "reimpl" / "prove.py"
    if not path.is_file():
        raise ReferenceUnavailable("d2-fleet transport not found at %s" % path)
    data = path.read_bytes()
    spec = importlib.util.spec_from_file_location("d2fleet_prove", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    for need in ("Member", "Refused", "bytes_agree"):
        if not hasattr(mod, need):
            raise ReferenceUnavailable("%s lacks %s" % (path, need))
    return mod, hashlib.sha256(data).hexdigest()


class FleetReferenceSession:
    """One member, one contract's original function, scratch memory owned here."""

    def __init__(self, fleet_root: Path, host: str, port: int, token: str,
                 baseline: dict, member_factory=None, transport=None):
        self.baseline = C.validate_baseline(baseline)
        self.host, self.port = host, int(port)
        if transport is not None:
            mod, self.transport_sha256 = transport
        elif member_factory is not None:
            mod, self.transport_sha256 = None, "0" * 64
        else:
            mod, self.transport_sha256 = load_transport(fleet_root)
        self._refused = getattr(mod, "Refused", _MemberRefused) if mod else _MemberRefused
        self._bytes_agree = getattr(mod, "bytes_agree", None) if mod else None
        if member_factory is not None:
            self.member = member_factory(host, self.port, token)
        else:
            self.member = mod.Member(host, self.port, token, timeout=30)
        self._token_present = bool(token)
        self.contract = None
        self.recipe = None
        self.base = None
        self.scratch = None
        self._palette_loaded = None
        self.member_info = {}
        self.calls = 0

    # -- adapter identity -------------------------------------------------

    def adapter_record(self) -> dict:
        if self.recipe is None:
            raise ReferenceRefused("adapter_record before open()")
        return {
            "name": ADAPTER_NAME,
            "version": ADAPTER_VERSION,
            "abi_recipe_digest": C.payload_sha256(self.recipe),
            "recipe_kind": self.recipe["kind"],
            "transport": "d2-fleet reimpl/prove.py Member",
            "transport_sha256": self.transport_sha256,
        }

    # -- guards -------------------------------------------------------------

    def _req(self, fn, *a, **kw):
        try:
            return fn(*a, **kw)
        except self._refused as exc:
            raise ReferenceUnavailable("member refused: %s" % str(exc)[:160])

    def open(self, contract: dict) -> dict:
        """Verify member privilege, module identity, function bytes; allocate scratch."""
        C.validate_contract(contract)
        if contract["baseline_digest"] != self.baseline["digest"]:
            raise ReferenceRefused("contract baseline is not this session's baseline")
        ref = contract["reference"]
        recipe = ref.get("abi_recipe")
        if not isinstance(recipe, dict):
            raise ReferenceRefused("contract carries no ABI recipe; refusing to guess one")
        if recipe.get("kind") not in SUPPORTED_RECIPES:
            raise ReferenceRefused("unsupported ABI recipe kind %r" % recipe.get("kind"))
        if C.payload_sha256(recipe) != ref["abi_recipe_digest"]:
            raise ReferenceRefused("ABI recipe does not match the contract's digest")

        inst = self.member.instance()
        if not inst.get("ok"):
            raise ReferenceUnavailable("member did not answer /instance: %s"
                                       % str(inst.get("error"))[:160])
        if inst.get("tier") != "privileged":
            raise ReferenceUnavailable(
                "member tier is %r; /call needs the privileged tier (token %s)"
                % (inst.get("tier"), "supplied" if self._token_present else "absent"))
        self.member_info = {k: inst.get(k) for k in
                            ("slot", "version", "exeSha256", "tier", "pid")}

        mods = self.member.modules().get("modules", [])
        hit = [m for m in mods if m.get("name", "").lower() == ref["module"].lower()]
        if not hit:
            raise ReferenceUnavailable("%s is not mapped in the member" % ref["module"])
        base = int(hit[0]["base"], 16)
        preferred = int(hit[0].get("preferred_base") or "0", 16) or base

        binary = C.baseline_binary(self.baseline, ref["module"])
        if binary is None:
            raise ReferenceRefused("baseline does not identify %s" % ref["module"])
        self._verify_module(ref["module"], binary, base, preferred)

        rva = int(ref["rva"], 16)
        n = int(recipe["function_length"])
        live = self._req(self.member.read_rva, ref["module"], rva, n)
        relocs = list(recipe.get("relocations", []))
        if self._bytes_agree is not None and relocs:
            ok, why = self._bytes_agree(bytes.fromhex(recipe["function_bytes"]), live,
                                        relocs, base - preferred)
            if not ok:
                raise ReferenceRefused("live function bytes differ: %s" % why)
        elif hashlib.sha256(live).hexdigest() != ref["function_bytes_sha256"]:
            raise ReferenceRefused(
                "live %s+%s bytes hash to %s, contract requires %s"
                % (ref["module"], ref["rva"], hashlib.sha256(live).hexdigest(),
                   ref["function_bytes_sha256"]))

        self.scratch = self._req(self.member.alloc, int(recipe["scratch_bytes"]))
        self.contract, self.recipe, self.base = contract, recipe, base
        return {"module_base": "0x%08x" % base, "relocated": base != preferred,
                "member": dict(self.member_info)}

    def _verify_module(self, module, binary, base, preferred):
        """Live code section must equal the baseline file's code section."""
        pe = binary.get("pe")
        if not pe or ".text" not in pe.get("sections", {}):
            raise ReferenceRefused("baseline has no section identity for %s" % module)
        if base != preferred:
            raise ReferenceRefused("%s is relocated; whole-section identity cannot be "
                                   "checked without relocation data" % module)
        text = pe["sections"][".text"]
        rva, n = int(text["rva"], 16), int(text["bytes"])
        h = hashlib.sha256()
        off = 0
        while off < n:
            k = min(0x1000, n - off)  # the route caps reads at 4096 bytes
            h.update(self._req(self.member.read_rva, module, rva + off, k))
            off += k
        if h.hexdigest() != text["sha256"]:
            raise ReferenceRefused("live %s .text does not match the baseline binary" % module)

    # -- invocation ---------------------------------------------------------

    def invoke(self, contract: dict, vector: dict, palettes: dict) -> dict:
        """Run one vector; return its named outputs.

        ``palettes`` maps palette id -> 1024 RGBA bytes. Faults, timeouts and a
        dead member raise; they are never turned into an output.
        """
        if contract is not self.contract or self.recipe is None:
            raise ReferenceRefused("invoke() for a contract this session did not open")
        inp = vector["input"]
        pal = palettes.get(inp["palette"])
        if pal is None or len(pal) != 1024:
            raise ReferenceRefused("vector %s names an unknown palette" % vector["id"])
        if hashlib.sha256(pal).hexdigest() != inp["palette_sha256"]:
            raise ReferenceRefused("vector %s palette bytes do not match their hash"
                                   % vector["id"])
        r = self.recipe
        if self._palette_loaded != inp["palette_sha256"]:
            self._req(self.member.write, self.scratch, pal)
            self._palette_loaded = inp["palette_sha256"]
        start, count = int(r["entry_start"]), int(r["entry_count"])
        c = [int(x) for x in inp["rgb"]]
        if len(c) != 3 or not all(0 <= x <= 255 for x in c):
            raise ReferenceRefused("vector %s rgb is not three bytes" % vector["id"])
        stack = [self.scratch + 4 * start, count, c[0], c[1], c[2]]
        try:
            d = self.member.call(self.contract["reference"]["module"],
                                 int(self.contract["reference"]["rva"], 16),
                                 ecx=0, edx=0, stack=stack, cdecl=False,
                                 watch=[(self.scratch, 1024)], restore=True,
                                 timeout_ms=int(r.get("timeout_ms", 5000)))
        except self._refused as exc:
            msg = str(exc)[:160]
            alive = self.member.instance()
            if not alive.get("ok"):
                raise ReferenceUnavailable("member died during vector %s: %s"
                                           % (vector["id"], msg))
            raise ReferenceUnavailable("call refused for vector %s: %s" % (vector["id"], msg))
        self.calls += 1
        if d.get("seh") or d.get("faulted") or d.get("timedOut") or d.get("timed_out"):
            raise ReferenceUnavailable("vector %s faulted/timed out: %s"
                                       % (vector["id"], {k: d.get(k) for k in
                                                         ("seh", "faulted", "faultCode",
                                                          "faultAddr", "timedOut")}))
        if "eax" not in d:
            raise ReferenceUnavailable("vector %s: call reply carries no eax" % vector["id"])
        want_fn = self.base + int(self.contract["reference"]["rva"], 16)
        fn = d.get("fn")
        if fn is not None and (int(fn, 16) if isinstance(fn, str) else int(fn)) != want_fn:
            raise ReferenceRefused("route resolved the function to %s, expected 0x%08x"
                                   % (fn, want_fn))
        eax = int(d["eax"], 16) if isinstance(d["eax"], str) else int(d["eax"])
        rel = eax & 0xFF
        if rel >= count:
            raise ReferenceRefused("vector %s returned relative index %d outside 0..%d"
                                   % (vector["id"], rel, count - 1))
        writes = d.get("memdiff") or []
        return {"index": rel, "writes": _normalize_writes(writes, self.scratch)}

    def close(self):
        """Nothing to restore: palette scratch belongs to this session."""


def _normalize_writes(writes, scratch):
    """Changed watched ranges as [{offset, hex}] relative to scratch (sorted)."""
    out = []
    for w in writes:
        addr = w.get("addr")
        addr = int(addr, 16) if isinstance(addr, str) else int(addr or 0)
        out.append({"offset": addr - scratch, "hex": str(w.get("hex", w.get("after", "")))})
    return sorted(out, key=lambda x: x["offset"])
