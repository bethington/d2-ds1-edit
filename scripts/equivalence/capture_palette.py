"""Nearest-colour pilot: authored inputs, contract, and original-game capture.

Subcommands::

    python -m scripts.equivalence.capture_palette inputs --out PATH
        Write the authored input fixture (deterministic; no game needed).

    python -m scripts.equivalence.capture_palette contract --baseline PATH \\
        --inputs PATH --out PATH
        Seal the contract for the given baseline and input fixture.

    python -m scripts.equivalence.capture_palette capture --contract PATH \\
        --baseline PATH --inputs PATH --fleet-root PATH --host HOST \\
        --port PORT --out PATH
        Call D2CMP.dll+0x9d30 on a privileged 1.13c member for every vector.
        The token is read from D2FLEET_TOKEN and never printed or stored.

The reference answers come only from the original routine; nothing here calls
the editor.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
from pathlib import Path

if __package__ in (None, ""):  # pragma: no cover
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from scripts.equivalence import contracts as C
else:
    from . import contracts as C

INPUTS_SCHEMA = "ds1edit.palette-nearest-inputs/1"
CONTRACT_ID = "palette-nearest-113c"
GENERATOR_VERSION = "1"
INPUTS_REL = "test/equivalence/fixtures/palette-nearest-inputs.json"
TOKEN_ENV = "D2FLEET_TOKEN"

#: Original D2CMP.dll 1.13c bytes at RVA 0x9d30 (112 bytes, no relocations),
#: read from the identified file and checked live before any call.
FUNCTION_BYTES = (
    "518b4c240c535533c083cbff33ed85c989442408765456578b7c241883c702900fb657fe"
    "8b7424208b4c24248b4424282bd60fb677ff2bf10fb60f2bc80fafc98bc60fafc603c88b"
    "c20fafc203c83bcb7306896c24108bd98b44241c4583c7043be872bc8a4424105f5e5d5b"
    "59c21400")

#: Observed ABI of D2CMP.dll 1.13c RVA 0x9d30 (export ordinal 10049), from the
#: disassembly of the identified binary -- not from decompiler parameter names.
ABI_RECIPE = {
    "kind": "stdcall-palette-nearest/1",
    "module": "D2CMP.dll",
    "rva": "0x9d30",
    "export_ordinal": 10049,
    "function_length": 112,
    "function_bytes": FUNCTION_BYTES,
    "relocations": [],
    "convention": "stdcall: 5 dword stack arguments, callee pops 0x14 (RET 0x14)",
    "stack_args": ["entries", "count", "c0", "c1", "c2"],
    "register_inputs": "none: ECX is pushed as the local best-index slot and reloaded "
                       "from [esp+0xc] before use; EAX/EDX/ESI/EDI/EBP are not read",
    "entry_layout": "stride 4; byte0 is compared with c0, byte1 with c1, byte2 with c2; "
                    "byte3 is never read",
    "distance": "32-bit sum of (byte_k - c_k)^2 over k=0..2; compared UNSIGNED (JNC)",
    "search": "relative indices 0..count-1 from `entries`; count==0 returns 0 without "
              "reading memory",
    "tie_policy": "strict less-than against the running minimum (initially 0xFFFFFFFF): "
                  "the first (lowest) relative index at the minimum wins",
    "return": "AL = relative index of the minimum; EAX bits 8..31 hold bits of `count` "
              "(or 0 when count==0) and are not part of the result",
    "hidden_state": "none: no global reads or writes, no calls, no relocations",
    "writes": "none outside its own stack frame",
    "marshalling": {
        "palette_memory": "1024 scratch bytes in the editor's RGBA_PALETTE order "
                          "(r,g,b,a per entry)",
        "entries": "scratch + 4*entry_start", "count": "entry_count",
        "c0": "query r", "c1": "query g", "c2": "query b",
    },
    "entry_start": 1,
    "entry_count": 255,
    "scratch_bytes": 1024,
    "timeout_ms": 5000,
    "evidence": [
        "Ghidra project diablo2:/Vanilla/1.13c/D2CMP.dll, disassembly 0x6fe19d30..0x6fe19da0",
        "PE .reloc of the identified D2CMP.dll: no fix-up inside RVA 0x9d30..0x9da0",
    ],
}


# ---------------------------------------------------------------------------
# Authored inputs
# ---------------------------------------------------------------------------

def _main_palette() -> bytes:
    out = bytearray()
    for i in range(256):
        out += bytes((i, (73 * i) % 256, (151 * i) % 256, 0 if i == 0 else 255))
    return bytes(out)


def _ties_palette() -> bytes:
    """Duplicates and equidistant entries; index 0 is an exact-match decoy."""
    cols = [(40, 40, 40)]                                    # 0: transparent decoy
    cols += [(100, 0, 0), (0, 100, 0), (100, 0, 0),          # 1..3: 3 duplicates 1
             (0, 0, 100), (0, 100, 0),                       # 4..5: 5 duplicates 2
             (200, 200, 200), (200, 200, 200), (60, 60, 60)]  # 6..8
    while len(cols) < 256:
        cols.append((255, 255, 255))                         # 9..255 duplicate white
    out = bytearray()
    for i, (r, g, b) in enumerate(cols):
        out += bytes((r, g, b, 0 if i == 0 else 255))
    return bytes(out)


def nearest(pal: bytes, rgb, start=1):
    """Reference-free helper for authoring: all indices at the minimum distance."""
    best, at = None, []
    for i in range(start, 256):
        d = sum((pal[4 * i + k] - rgb[k]) ** 2 for k in range(3))
        if best is None or d < best:
            best, at = d, [i]
        elif d == best:
            at.append(i)
    return at


def generate_inputs() -> dict:
    pals = {"main": _main_palette(), "ties": _ties_palette()}
    sha = {k: hashlib.sha256(v).hexdigest() for k, v in pals.items()}
    vectors, seen = [], set()

    def add(vid, pal, rgb, group):
        key = (pal, tuple(rgb))
        if key in seen:      # identical queries are not separate coverage
            return
        seen.add(key)
        vectors.append({"id": vid, "group": group,
                        "input": {"palette": pal, "palette_sha256": sha[pal],
                                  "rgb": list(rgb)}})

    m = pals["main"]
    for i in range(1, 256):
        add("main-exact-%03d" % i, "main", m[4 * i:4 * i + 3], "exact")
    levels = (0, 127, 255)
    for r in levels:
        for g in levels:
            for b in levels:
                add("main-grid-%03d-%03d-%03d" % (r, g, b), "main", (r, g, b), "grid")
    add("main-index0-colour", "main", m[0:3], "transparent-index-0")

    t = pals["ties"]
    add("ties-index0-colour", "ties", t[0:3], "transparent-index-0")
    for i in range(1, 9):
        add("ties-exact-%03d" % i, "ties", t[4 * i:4 * i + 3], "duplicate")
    add("ties-exact-white", "ties", (255, 255, 255), "duplicate")
    n = 0
    for r in range(0, 256, 25):
        for g in range(0, 256, 25):
            for b in range(0, 256, 50):
                if len(nearest(t, (r, g, b))) > 1 and n < 48:
                    add("ties-equidistant-%03d-%03d-%03d" % (r, g, b), "ties", (r, g, b),
                        "equidistant")
                    n += 1

    return C.seal({
        "schema": INPUTS_SCHEMA,
        "generator": {
            "version": GENERATOR_VERSION,
            "module": "scripts.equivalence.capture_palette",
            "main_palette": "entry i = (i, 73*i mod 256, 151*i mod 256), alpha 0 at i=0 "
                            "else 255; all RGB distinct",
            "ties_palette": "duplicates (1=3, 2=5, 6=7, 9..255 white), decoy at index 0",
            "queries": "exact colours 1..255; 0/127/255 RGB grid; index-0 colours; "
                       "ties exact; grid points (step 25/25/50) with >1 nearest index, "
                       "capped at 48; identical queries deduplicated",
        },
        "palettes": {k: v.hex() for k, v in pals.items()},
        "vectors": vectors,
    })


def load_inputs(path: Path) -> dict:
    inp = C.load_json_object(path)
    if inp.get("schema") != INPUTS_SCHEMA:
        raise C.ValidationError("%s is not a %s fixture" % (path, INPUTS_SCHEMA))
    if C.canonical_digest(inp) != inp.get("digest"):
        raise C.ValidationError("input fixture digest mismatch")
    for k, h in inp["palettes"].items():
        if len(bytes.fromhex(h)) != 1024:
            raise C.ValidationError("palette %s is not 1024 bytes" % k)
    return inp


def palette_bytes(inputs: dict) -> dict:
    return {k: bytes.fromhex(v) for k, v in inputs["palettes"].items()}


# ---------------------------------------------------------------------------
# Contract
# ---------------------------------------------------------------------------

def build_contract(baseline: dict, inputs: dict) -> dict:
    C.validate_baseline(baseline)
    return C.seal({
        "schema": C.CONTRACT_SCHEMA,
        "id": CONTRACT_ID,
        "kind": "palette-nearest",
        "behavior": "Nearest palette colour for an RGB query, transparent index 0 "
                    "excluded: editor palette_find_closest vs D2CMP.dll 1.13c 0x9d30",
        "baseline_digest": baseline["digest"],
        "reference": {
            "module": "D2CMP.dll",
            "rva": "0x9d30",
            "function_bytes_sha256": hashlib.sha256(bytes.fromhex(FUNCTION_BYTES)).hexdigest(),
            "abi_recipe_digest": C.payload_sha256(ABI_RECIPE),
            "abi_recipe": ABI_RECIPE,
        },
        "inputs": {"path": INPUTS_REL, "digest": inputs["digest"]},
        "domain": {
            "description": "%d distinct authored (palette, rgb) queries over 2 authored "
                           "palettes; not all 2^24 colours or all palettes"
                           % len(inputs["vectors"]),
            "axes": {
                "palette": {"coverage": "sampled", "required": True,
                            "note": "2 authored palettes (distinct; duplicates/ties)"},
                "query_rgb": {"coverage": "sampled", "required": True,
                              "note": "exact colours, 3-level grid, tie points"},
                "exact_colour_per_index": {"coverage": "exhaustive", "required": True,
                                           "note": "every index 1..255 of the main palette"},
                "tie_policy": {"coverage": "sampled", "required": True,
                               "note": "duplicates and equidistant queries"},
                "transparent_index_0": {"coverage": "pinned", "required": True,
                                        "note": "excluded by both: the editor skips it, the "
                                                "reference is given entries 1..255"},
                "count": {"coverage": "pinned", "required": True,
                          "note": "255 entries; other counts are outside this contract"},
            },
        },
        "outputs": [{"name": "index", "type": "int"},
                    {"name": "writes", "type": "json"}],
        "normalizations": [{"name": "index_offset",
                            "params": {"output": "index", "offset": 1,
                                       "applies_to": "expected"}}],
        "limitations": [
            "Compares the search algorithm under the editor's RGBA memory order. It does "
            "not establish which channel order the game's own palettes or callers use.",
            "Only AL is compared; EAX bits 8..31 are not a result of the routine.",
            "Finite authored domain; palettes with alpha/gamma handling are out of scope.",
        ],
        "required": True,
        "success_outcome": "pass",
    })


# ---------------------------------------------------------------------------
# Capture
# ---------------------------------------------------------------------------

def capture(contract: dict, baseline: dict, inputs: dict, session, evidence=()) -> dict:
    """Run every vector through *session*; return a sealed capture.

    Any failure aborts: a capture with missing vectors is never written.
    """
    C.validate_contract(contract)
    C.validate_baseline(baseline)
    if contract.get("inputs", {}).get("digest") != inputs.get("digest"):
        raise C.ValidationError("input fixture is not the one the contract names")
    if not inputs["vectors"]:
        raise C.ValidationError("input fixture has no vectors")
    opened = session.open(contract)
    pals = palette_bytes(inputs)
    vectors = []
    for v in inputs["vectors"]:
        out = session.invoke(contract, v, pals)
        vectors.append({"id": v["id"], "input": v["input"], "output": out})
    if len(vectors) != len(inputs["vectors"]):
        raise C.ValidationError("capture is short of the declared vectors")
    binary = C.baseline_binary(baseline, contract["reference"]["module"])
    adapter = session.adapter_record()
    cap = {
        "schema": C.CAPTURE_SCHEMA,
        "contract_id": contract["id"],
        "contract_digest": contract["digest"],
        "baseline_digest": baseline["digest"],
        "adapter": adapter,
        "adapter_digest": C.payload_sha256(adapter),
        "original": {
            "module": contract["reference"]["module"],
            "module_sha256": binary["sha256"],
            "rva": contract["reference"]["rva"],
            "function_bytes_sha256": contract["reference"]["function_bytes_sha256"],
            "live": {"module_base": opened.get("module_base"),
                     "relocated": opened.get("relocated"),
                     "member_version": opened.get("member", {}).get("version"),
                     "member_exe_sha256": opened.get("member", {}).get("exeSha256"),
                     "text_section_verified": True},
        },
        "captured_at": C.utc_now(),
        "evidence": [{"kind": "ghidra", "ref": e} for e in ABI_RECIPE["evidence"]]
                    + list(evidence),
        "vectors": vectors,
        "input_sha256": C.payload_sha256(C.vector_inputs_payload(vectors)),
        "output_sha256": C.payload_sha256(C.vector_outputs_payload(vectors)),
        "calls": session.calls,
        "limitations": [],
    }
    return C.seal(cap)


def _write(obj, out: Path):
    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(obj, fh, indent=1, sort_keys=True)
        fh.write("\n")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="python -m scripts.equivalence.capture_palette")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("inputs")
    p.add_argument("--out", required=True, type=Path)
    p = sub.add_parser("contract")
    p.add_argument("--baseline", required=True, type=Path)
    p.add_argument("--inputs", required=True, type=Path)
    p.add_argument("--out", required=True, type=Path)
    p = sub.add_parser("capture")
    for k in ("contract", "baseline", "inputs", "fleet-root", "out"):
        p.add_argument("--" + k, required=True, type=Path)
    p.add_argument("--host", required=True)
    p.add_argument("--port", required=True, type=int)
    a = ap.parse_args(argv)

    if a.cmd == "inputs":
        inp = generate_inputs()
        _write(inp, a.out)
        print("%d vectors, digest %s" % (len(inp["vectors"]), inp["digest"]))
        return 0
    if a.cmd == "contract":
        c = build_contract(C.load_baseline(a.baseline), load_inputs(a.inputs))
        _write(c, a.out)
        print("contract %s digest %s" % (c["id"], c["digest"]))
        return 0

    from .fleet_adapter import (FleetReferenceSession, ReferenceRefused,
                                ReferenceUnavailable)
    token = os.environ.get(TOKEN_ENV, "")
    try:
        contract = C.load_contract(a.contract)
        baseline = C.load_baseline(a.baseline)
        inputs = load_inputs(a.inputs)
        session = FleetReferenceSession(a.fleet_root, a.host, a.port, token, baseline)
        cap = capture(contract, baseline, inputs, session)
        C.validate_capture(cap, contract, baseline)
    except (C.ValidationError, ReferenceRefused) as exc:
        print("capture: REFUSED: %s" % exc, file=sys.stderr)
        return 2
    except ReferenceUnavailable as exc:
        print("capture: UNAVAILABLE: %s" % exc, file=sys.stderr)
        return 2
    _write(cap, a.out)
    print("captured %d vectors (%d calls), digest %s"
          % (len(cap["vectors"]), cap["calls"], cap["digest"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
