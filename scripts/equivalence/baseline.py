"""Build a ``ds1edit.baseline/1`` manifest from a vanilla 1.13c directory.

The manifest identifies the original binaries and archives by SHA-256 and, in
``corpus`` scope, the effective bytes of every enumerated editor resource after
MPQ priority resolution. It never reads the repository's mixed ``assets/tiles``
tree, loose ``data\\`` overrides, mod MPQs, preferences, or user overlays: only
the four explicitly ordered archives below and the named top-level binaries.

CLI (local lane only -- the default reader needs ``mpyq``)::

    python -m scripts.equivalence.baseline --game-dir PATH --out PATH \\
        [--extra-names PATH] [--scope corpus|identity] [--require-complete]

The game directory is printed, never written into the manifest, so moving the
installation does not change the manifest's identity.
"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import importlib.util
import json
import os
import struct
import sys
from pathlib import Path, PureWindowsPath

if __package__ in (None, ""):  # pragma: no cover
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from scripts.equivalence import contracts as C
else:
    from . import contracts as C

REPO_ROOT = Path(__file__).resolve().parents[2]

#: Editor resource resolution order, highest priority first. Explicit: nothing
#: else in the game directory is consulted.
ARCHIVES = ("Patch_D2.mpq", "d2exp.mpq", "d2data.mpq", "d2char.mpq")

#: Binaries that must exist for a directory to be a usable 1.13c reference.
REQUIRED_BINARIES = ("Game.exe", "D2CMP.dll", "D2Common.dll", "D2Client.dll",
                     "D2Gfx.dll", "D2Win.dll", "D2Lang.dll", "Fog.dll", "Storm.dll")

#: Editor-consumed resources hashed in corpus scope (normalised virtual paths).
DEFAULT_INCLUDE = (
    "data\\global\\tiles\\*.ds1",
    "data\\global\\tiles\\*.dt1",
    "data\\global\\excel\\*.txt",
    "data\\global\\palette\\*",
    "data\\global\\animdata.d2",
)

#: Tables whose ``File*`` columns name tile resources relative to tiles\.
DEPENDENCY_TABLES = ("data\\global\\excel\\lvltypes.txt",
                     "data\\global\\excel\\lvlprest.txt")
TILES_DIR = "data\\global\\tiles\\"

#: Fixed render/configuration for visual cases (no implicit discovery).
RENDER_CONFIG = {
    "palette": "data\\global\\palette\\act<N>\\pal.pl2 per act; no gamma table",
    "gamma": "none",
    "zoom": 1,
    "frame": 0,
    "seed": 0,
    "overlays": "disabled",
    "user_preferences": "ignored",
    "mod_directories": "none",
}


class BaselineError(Exception):
    """The directory cannot produce a trustworthy vanilla baseline."""


# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------

def sha256_file(path: Path, chunk=1 << 20) -> tuple[str, int]:
    h = hashlib.sha256()
    n = 0
    with open(path, "rb") as fh:
        while True:
            b = fh.read(chunk)
            if not b:
                break
            h.update(b)
            n += len(b)
    return h.hexdigest(), n


def _find_case_insensitive(game_dir: Path, name: str) -> Path | None:
    want = name.lower()
    for entry in os.listdir(game_dir):
        if entry.lower() == want and (game_dir / entry).is_file():
            return game_dir / entry
    return None


def pe_sections(path: Path) -> dict | None:
    """Section identities of a PE image: what a live module can be checked against.

    ``bytes`` is the length a loaded image shares with the file:
    ``min(VirtualSize, SizeOfRawData)``. Returns None for a non-PE file.
    """
    with open(path, "rb") as fh:
        data = fh.read()
    if data[:2] != b"MZ" or len(data) < 0x40:
        return None
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        return None
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    timestamp = struct.unpack_from("<I", data, pe + 8)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    size_of_image = struct.unpack_from("<I", data, opt + 56)[0]
    sections = {}
    off = opt + opt_size
    for _ in range(nsec):
        name = data[off:off + 8].rstrip(b"\0").decode("latin-1")
        vsize, va, rsize, rptr = struct.unpack_from("<IIII", data, off + 8)
        chars = struct.unpack_from("<I", data, off + 36)[0]
        n = min(vsize, rsize) if vsize else rsize
        sections[name] = {
            "rva": "0x%x" % va, "bytes": n,
            "sha256": hashlib.sha256(data[rptr:rptr + n]).hexdigest(),
            "characteristics": "0x%08x" % chars,
        }
        off += 40
    return {"timestamp": "0x%08x" % timestamp, "image_base": "0x%08x" % image_base,
            "size_of_image": size_of_image, "sections": sections}


def binary_identities(game_dir: Path) -> list[dict]:
    """Every top-level .exe/.dll, plus a hard failure for missing required ones."""
    found = {}
    for entry in sorted(os.listdir(game_dir), key=str.lower):
        p = game_dir / entry
        if p.is_file() and entry.lower().endswith((".exe", ".dll")):
            found[entry.lower()] = p
    missing = [b for b in REQUIRED_BINARIES if b.lower() not in found]
    if missing:
        raise BaselineError("missing required original binaries: %s" % ", ".join(missing))
    out = []
    for low, p in sorted(found.items()):
        sha, size = sha256_file(p)
        rec = {"name": p.name, "sha256": sha, "size": size}
        pe = pe_sections(p)
        if pe:
            rec["pe"] = pe
        out.append(rec)
    return out


def archive_identities(game_dir: Path) -> list[tuple[dict, Path]]:
    out = []
    missing = []
    for i, name in enumerate(ARCHIVES):
        p = _find_case_insensitive(game_dir, name)
        if p is None:
            missing.append(name)
            continue
        sha, size = sha256_file(p)
        out.append(({"name": p.name, "priority": i, "sha256": sha, "size": size}, p))
    if missing:
        raise BaselineError("missing required archives: %s" % ", ".join(missing))
    return out


# ---------------------------------------------------------------------------
# Virtual paths
# ---------------------------------------------------------------------------

def safe_scratch_path(scratch_root: Path, virtual_path: str) -> Path:
    """Map an archive member name under *scratch_root*, refusing escapes.

    Rejects absolute names, drive letters, ``..`` components, and anything that
    would resolve outside the owned scratch directory.
    """
    norm = virtual_path.replace("/", "\\")
    if not norm or norm.startswith("\\") or PureWindowsPath(norm).drive or ":" in norm:
        raise BaselineError("refusing absolute member path %r" % virtual_path)
    parts = [p for p in norm.split("\\") if p not in ("", ".")]
    if any(p == ".." for p in parts) or not parts:
        raise BaselineError("refusing traversing member path %r" % virtual_path)
    root = Path(scratch_root).resolve()
    target = root.joinpath(*parts).resolve()
    if os.path.commonpath([str(root), str(target)]) != str(root):
        raise BaselineError("member path %r escapes the scratch root" % virtual_path)
    return target


def _included(norm: str, patterns) -> bool:
    return any(fnmatch.fnmatchcase(norm, p.lower()) for p in patterns)


def _table_dependencies(text: str) -> list[str]:
    """Tile paths named by ``File*`` columns of a tab-separated excel table."""
    lines = text.replace("\r\n", "\n").split("\n")
    if not lines:
        return []
    head = lines[0].split("\t")
    cols = [i for i, h in enumerate(head)
            if h.lower().replace(" ", "").startswith("file")
            and h.lower().replace(" ", "")[4:].isdigit()]
    refs = []
    for line in lines[1:]:
        cells = line.split("\t")
        for i in cols:
            if i < len(cells):
                v = cells[i].strip()
                if v and v != "0" and "." in v:
                    refs.append(C.normalize_virtual_path(TILES_DIR + v))
    return refs


# ---------------------------------------------------------------------------
# Readers
# ---------------------------------------------------------------------------

def default_reader_factory(path: Path):
    """Lazy import of the repository's corrected MPQ reader (needs mpyq)."""
    spec = importlib.util.spec_from_file_location(
        "ds1edit_d2mpq", REPO_ROOT / "tools" / "d2mpq.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.D2Archive(str(path))


def _close(reader):
    close = getattr(reader, "close", None)
    if close:
        close()


# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

def build_baseline(game_dir: Path, extra_names: list[str], reader_factory=None, *,
                   scope: str = "corpus", include=DEFAULT_INCLUDE,
                   require_complete: bool = False) -> dict:
    """Return a sealed ``ds1edit.baseline/1`` for *game_dir*.

    ``scope="identity"`` records binaries and archives only (no assets); it is
    for contracts that consume no game assets. ``reader_factory(path)`` returns
    an object with ``namelist(extra_names=...)``, ``has(name)`` and
    ``read(name)``; the default is ``tools/d2mpq.py``'s ``D2Archive``.
    """
    game_dir = Path(game_dir)
    if not game_dir.is_dir():
        raise BaselineError("game directory not found: %s" % game_dir)
    if scope not in ("corpus", "identity"):
        raise BaselineError("scope must be 'corpus' or 'identity'")

    binaries = binary_identities(game_dir)
    arcs = archive_identities(game_dir)

    baseline = {
        "schema": C.BASELINE_SCHEMA,
        "build": C.BASELINE_BUILD,
        "binaries": binaries,
        "archives": [a for a, _ in arcs],
        "render": dict(RENDER_CONFIG),
    }

    if scope == "identity":
        baseline["enumeration"] = {
            "scope": "identity",
            "sources": [],
            "limits": ["identity-only baseline: binaries and archives are hashed, "
                       "no archive member is enumerated or hashed"],
            "complete": False,
        }
        baseline["assets"] = []
        return C.seal(baseline)

    factory = reader_factory or default_reader_factory
    extras = sorted({n for n in extra_names if n.strip()}, key=str.lower)
    sources, limits = [], []
    effective = {}   # norm -> asset record
    readers = []
    complete = True
    try:
        for rec, path in arcs:
            r = factory(path)
            readers.append((rec, r))
        # Pass 1: enumeration per archive.
        names_by_arc = []
        for rec, r in readers:
            src = {"archive": rec["name"], "listfile": False, "names": 0,
                   "supplementary_resolved": 0}
            listed = []
            try:
                has_list = bool(r.has("(listfile)"))
                listed = list(r.namelist())
                src["listfile"] = has_list
            except Exception as exc:  # unreadable listfile is a limit, not a crash
                has_list = False
                limits.append("%s: listfile unreadable (%s)" % (rec["name"], exc))
            if not has_list:
                complete = False
                if not any(l.startswith(rec["name"] + ":") for l in limits):
                    limits.append("%s: no readable (listfile); only supplementary "
                                  "names can be resolved" % rec["name"])
            known = {n.lower().replace("/", "\\") for n in listed}
            sup = 0
            for n in extras:
                if C.normalize_virtual_path(n) not in known and r.has(n):
                    listed.append(n)
                    known.add(C.normalize_virtual_path(n))
                    sup += 1
            src["names"] = len(listed)
            src["supplementary_resolved"] = sup
            sources.append(src)
            names_by_arc.append(listed)

        # Pass 2: effective bytes, highest priority first; case variants dedupe.
        for (rec, r), listed in zip(readers, names_by_arc):
            for original in sorted(listed, key=str.lower):
                norm = C.normalize_virtual_path(original)
                if norm in effective or not _included(norm, include):
                    continue
                data = r.read(original)
                if data is None:
                    limits.append("%s: listed %r is not readable" % (rec["name"], original))
                    complete = False
                    continue
                effective[norm] = {
                    "path": norm, "original_path": original, "archive": rec["name"],
                    "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                    "kind": norm.rsplit(".", 1)[-1] if "." in norm else "",
                }
                if norm in DEPENDENCY_TABLES:
                    effective[norm]["_text"] = data.decode("latin-1")
    finally:
        for _, r in readers:
            _close(r)

    deps = {}
    for table in DEPENDENCY_TABLES:
        a = effective.get(table)
        if a is None:
            limits.append("dependency table %s not found" % table)
            complete = False
            continue
        refs = sorted(set(_table_dependencies(a.pop("_text"))))
        unresolved = [p for p in refs if p not in effective]
        deps[table] = {"referenced": len(refs), "resolved": len(refs) - len(unresolved),
                       "unresolved": unresolved}
    for a in effective.values():
        a.pop("_text", None)

    if require_complete and not complete:
        raise BaselineError("required corpus enumeration is incomplete: %s"
                            % "; ".join(limits))

    assets = [effective[k] for k in sorted(effective)]
    counts = {}
    for a in assets:
        counts[a["kind"]] = counts.get(a["kind"], 0) + 1
    baseline["enumeration"] = {
        "scope": "corpus",
        "include": list(include),
        "supplementary_names": len(extras),
        "sources": sources,
        "limits": limits,
        "complete": complete,
        "counts": dict(sorted(counts.items())),
    }
    baseline["dependencies"] = deps
    baseline["assets"] = assets
    return C.seal(baseline)


def read_extra_names(path: Path | None) -> list[str]:
    if not path:
        return []
    text = Path(path).read_text(encoding="latin-1")
    return [l.strip() for l in text.replace("\r\n", "\n").split("\n") if l.strip()]


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="python -m scripts.equivalence.baseline")
    ap.add_argument("--game-dir", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--extra-names", type=Path)
    ap.add_argument("--scope", choices=("corpus", "identity"), default="corpus")
    ap.add_argument("--require-complete", action="store_true")
    a = ap.parse_args(argv)
    try:
        b = build_baseline(a.game_dir, read_extra_names(a.extra_names),
                           scope=a.scope, require_complete=a.require_complete)
    except BaselineError as exc:
        print("baseline: FAIL: %s" % exc, file=sys.stderr)
        return 2
    a.out.parent.mkdir(parents=True, exist_ok=True)
    with open(a.out, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(b, fh, indent=1, sort_keys=True)
        fh.write("\n")
    en = b["enumeration"]
    print("baseline %s from %s" % (b["digest"], a.game_dir))
    print("  binaries %d, archives %s" % (len(b["binaries"]),
                                          " > ".join(x["name"] for x in b["archives"])))
    print("  scope %s, assets %d, complete %s" % (en["scope"], len(b["assets"]),
                                                  en["complete"]))
    for k, v in en.get("counts", {}).items():
        print("    %-8s %d" % (k, v))
    for t, d in b.get("dependencies", {}).items():
        print("  %s: %d referenced, %d unresolved" % (t, d["referenced"], len(d["unresolved"])))
    for l in en["limits"]:
        print("  limit: %s" % l)
    return 0


if __name__ == "__main__":
    sys.exit(main())
