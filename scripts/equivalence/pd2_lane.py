"""Local-only Project Diablo 2 (PD2) compatibility lane.

Project Diablo 2 ships its own archives ahead of the vanilla ones, and the
editor can load them (``use_pd2`` / ``pd2_dir``, src/core/d2install.c). This
lane answers one question for one PD2 installation on this machine: does the
editor read PD2's maps, tables and art without rejecting, crashing or
silently falling back to vanilla data?

It is **local only** (CTest label ``pd2``, registered by test/cmake/pd2.cmake
only when ``DS1EDIT_PD2_DIR`` is set) because PD2's art is not redistributable:
nothing it reads or writes may be committed or packaged. Everything lives under
``tmp/pd2/`` (``run-<ts>/`` per run, ``baseline/`` for the local render
baseline). qualify.py treats its cases as optional (``required: false``,
``OPTIONAL_LANES``); they can never satisfy or block a release.

What it does, in order
----------------------
1. **Identity** -- SHA-256 of every PD2 archive (plus the vanilla archives the
   chain also reads) and the PD2 version from ``ProjectDiablo.json``. Every
   result is bound to that identity. A baseline recorded for another
   identity is ``incomparable``: no pass, no fail.
2. **Inventory** -- every archive's names (``(listfile)``; where it is missing
   the enumeration limit is recorded, and names are recovered from sibling
   listfiles), every DS1/DT1/TXT/DCC/DC6/COF classified ``pd2_unique``,
   ``overrides_vanilla`` or ``identical`` against the vanilla archives.
3. **Maps** -- every PD2-unique and PD2-overridden DS1 through the production
   reader and checked-writer resave (``ds1_sweep.sweep_one``), with each
   DT1/LvlPrest/LvlTypes dependency resolved through the PD2 chain.
4. **Tables** -- Levels, LvlPrest, LvlTypes, Objects, MonStats through the
   editor (``dump-txt-row``, ``list-areas``, ``list-files``, ``list-tokens``);
   listed versus unlisted maps in the area browser.
5. **Graphics** -- every PD2 DCC/DC6/DT1 through ``test_visual_reference_probe``,
   every COF through ``test_adversarial_probe``. A nonzero exit other than the
   probe's declared rejection is a defect; each one keeps a repro under
   ``failures/``.
6. **Render** -- a fixed set (PD2-unique maps plus one PD2-overridden town)
   rendered headless twice from an owned windowed runtime, byte-identical,
   against a local baseline (first run creates it, ``reviewed: false``). A
   render that does not say "Project Diablo 2 data from" fails.

Everything is judged against the editor itself; none of it is an original-game
comparison. CLI::

    python -m scripts.equivalence.pd2_lane all --pd2-dir C:\\Diablo2\\ProjectD2 \\
        --d2-dir C:\\Diablo2 --exe bin\\ds1edit.exe \\
        --probe-ds1 tmp\\build-pd2\\test\\Debug\\test_ds1_reference_probe.exe \\
        --probe-visual ... --probe-robust ... --out-root tmp\\pd2

(``inventory``, ``sweep`` and ``render`` run one part.) Paths are always
explicit: nothing here auto-detects an installation. Exit 0 every case passed
(or only created its baseline), 1 any mismatch, 2 anything unavailable.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if __package__ in (None, ""):  # pragma: no cover
    sys.path.insert(0, str(REPO_ROOT))
    from scripts.equivalence import contracts as C
    from scripts.equivalence import ds1_cases
    from scripts.equivalence import ds1_sweep as S
    from scripts.equivalence import render_cases as R
else:
    from . import contracts as C
    from . import ds1_cases
    from . import ds1_sweep as S
    from . import render_cases as R

LANE = "pd2"
CASE_PREFIX = "pd2/"
IDENTITY_SCHEMA = "ds1edit.pd2-identity/1"
BASELINE_SCHEMA = "ds1edit.pd2-baseline/1"
SUMMARY_SCHEMA = "ds1edit.pd2-summary/1"

#: PD2's own archives in chain order (src/core/d2install.c slots 0-4). The
#: first four are required; the ProjectD2 copy of patch_d2 is optional.
PD2_ARCHIVES = ("pd2data.mpq", "pd2maps.mpq", "pd2assets.mpq", "pd2monchars.mpq",
                "patch_d2.mpq")
PD2_SLOTS = ("pd2data", "pd2maps", "pd2assets", "pd2monchars", "pd2patch")
#: The vanilla archives the editor appends behind PD2's, highest priority first.
BASE_ARCHIVES = ("patch_d2.mpq", "d2exp.mpq", "d2data.mpq", "d2char.mpq")
BASE_SLOTS = ("patch_d2", "d2exp", "d2data", "d2char")

KIND_BY_EXT = {".ds1": "ds1", ".dt1": "dt1", ".txt": "txt", ".dcc": "dcc",
               ".dc6": "dc6", ".cof": "cof"}
CLASSES = ("pd2_unique", "overrides_vanilla", "identical")
UNIQUE, OVERRIDE, IDENTICAL = CLASSES

EXCEL = "data\\global\\excel\\"
TABLES = (
    ("levels", EXCEL + "levels.txt"),
    ("lvlprest", EXCEL + "lvlprest.txt"),
    ("lvltypes", EXCEL + "lvltypes.txt"),
    ("objects", EXCEL + "objects.txt"),
    ("monstats", EXCEL + "monstats.txt"),
)
ANIMDATA = "data\\global\\animdata.d2"

LIMITATIONS = [
    "local-only lane against one Project Diablo 2 installation on this machine; "
    "the editor is compared with itself, not an original-game comparison",
    "PD2 art and data are not redistributable: nothing here is committed or packaged",
]

PROBE_TIMEOUT = 120.0
STATUS_DLL_NOT_FOUND = 0xC0000135


class Pd2Error(Exception):
    """The PD2 installation or a lane input cannot be used."""


def norm(path: str) -> str:
    return C.normalize_virtual_path(path)


def kind_of(path: str):
    return KIND_BY_EXT.get(os.path.splitext(path)[1].lower())


def sha256_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def slug(text: str) -> str:
    return re.sub(r"[^a-z0-9._-]+", "_", text.lower()).strip("_")[:150]


def utc_stamp() -> str:
    return time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())


# ---------------------------------------------------------------------------
# identity
# ---------------------------------------------------------------------------

_VERSION_KEYS = ("version", "pd2_version", "game_version", "client_version",
                 "build", "build_version", "build_number")


def extract_version(obj, _prefix=""):
    """First scalar under a version-like key, as ``(value, key path)``; or
    ``(None, None)``. Booleans and empty strings are not versions, and nothing
    is guessed from other keys."""
    if not isinstance(obj, dict):
        return None, None
    for k, v in obj.items():
        if k.lower() in _VERSION_KEYS and not isinstance(v, (bool, dict, list)) \
                and v is not None and str(v).strip():
            return str(v), _prefix + k
    for k, v in obj.items():
        if isinstance(v, dict):
            found = extract_version(v, _prefix + k + "/")
            if found[0] is not None:
                return found
    return None, None


def _find_file(directory: Path, name: str):
    want = name.lower()
    try:
        for entry in os.listdir(directory):
            if entry.lower() == want and (Path(directory) / entry).is_file():
                return Path(directory) / entry
    except OSError:
        pass
    return None


def hash_file(path: Path, chunk: int = 1 << 20):
    """``(sha256 hex, md5 hex, size)`` in one pass."""
    s, m, n = hashlib.sha256(), hashlib.md5(), 0
    with open(path, "rb") as fh:
        while True:
            b = fh.read(chunk)
            if not b:
                break
            s.update(b)
            m.update(b)
            n += len(b)
    return s.hexdigest(), m.hexdigest(), n


def _read_version(pd2_dir: Path):
    for fname in ("ProjectDiablo.json", "local_metadata.json"):
        p = _find_file(pd2_dir, fname)
        if p is None:
            continue
        try:
            obj = json.loads(p.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError):
            continue
        v, where = extract_version(obj)
        if v is not None:
            return v, "%s:%s" % (fname, where)
    p = _find_file(pd2_dir, "ProjectDiablo.json")
    return None, ("no version field in ProjectDiablo.json" if p is not None
                  else "ProjectDiablo.json not found (no version recorded)")


def _metadata_md5(pd2_dir: Path):
    p = _find_file(pd2_dir, "local_metadata.json")
    if p is None:
        return {}
    try:
        obj = json.loads(p.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError):
        return {}
    out = {}
    for line in obj.get("checksum", []) if isinstance(obj, dict) else []:
        m = re.match(r"^([0-9a-fA-F]{32})\s+(.+?)\s*$", str(line))
        if m:
            out[m.group(2).lower()] = m.group(1).lower()
    return out


def compute_identity(pd2_dir, base_dir, hasher=hash_file) -> dict:
    """Identity of the PD2 installation and the vanilla chain behind it.

    Hashes every archive; the digest covers archive names, sizes and SHA-256
    plus the PD2 version, never a directory path, so moving the install keeps
    the identity."""
    pd2_dir, base_dir = Path(pd2_dir), Path(base_dir)
    archives, md5s = [], {}
    for role, folder, names in (("pd2", pd2_dir, PD2_ARCHIVES),
                                ("base", base_dir, BASE_ARCHIVES)):
        for n in names:
            p = _find_file(folder, n)
            if p is None:
                if role == "pd2" and n != "patch_d2.mpq":
                    raise Pd2Error("PD2 archive %s not found in %s" % (n, folder))
                archives.append({"role": role, "name": n, "present": False,
                                 "size": None, "sha256": None})
                continue
            sha, md5, size = hasher(p)
            archives.append({"role": role, "name": n, "present": True,
                             "size": size, "sha256": sha})
            if role == "pd2":
                md5s[n] = md5
    version, source = _read_version(pd2_dir)
    meta = _metadata_md5(pd2_dir)
    check = None
    if meta:
        matched = [n for n in PD2_ARCHIVES if n in md5s and meta.get(n) == md5s[n]]
        differ = [n for n in PD2_ARCHIVES if n in md5s and n in meta and meta[n] != md5s[n]]
        check = {"source": "local_metadata.json", "matched": matched, "differ": differ}
    return C.seal({"schema": IDENTITY_SCHEMA, "version": version,
                   "version_source": source, "archives": archives,
                   "metadata_check": check})


def compare_identity(baseline, current) -> dict:
    """``comparable`` only for the same PD2 version *and* the same bytes."""
    if not baseline or not baseline.get("digest"):
        return {"verdict": "incomparable", "reasons": ["no baseline identity recorded"]}
    reasons = []
    bv, cv = baseline.get("version"), current.get("version")
    if bv is not None and cv is not None and bv != cv:
        reasons.append("pd2 version differs: baseline %s, current %s" % (bv, cv))
    if baseline["digest"] != current.get("digest"):
        reasons.append("archive identity differs: baseline %s..., current %s..."
                       % (baseline["digest"][:12], str(current.get("digest"))[:12]))
    return {"verdict": "incomparable" if reasons else "comparable", "reasons": reasons}


# ---------------------------------------------------------------------------
# archive chain and inventory
# ---------------------------------------------------------------------------

class _PathSet:
    def __init__(self, chain):
        self.chain = chain

    def __contains__(self, path):
        return self.chain.has(path)


class Chain:
    """Ordered archives, first hit wins (the editor's lookup order)."""

    def __init__(self, archives):
        self.archives = list(archives)  # [(slot, reader)]

    def read(self, name):
        for slot, rd in self.archives:
            data = rd.read(name)
            if data is not None:
                return data, slot
        return None

    def has(self, name):
        n = name.replace("/", "\\")
        return any(rd.has(n) for _, rd in self.archives)

    def pathset(self):
        return _PathSet(self)

    def close(self):
        for _, rd in self.archives:
            c = getattr(rd, "close", None)
            if c:
                c()


def indexed_reader_factory(path):
    """The repository's corrected MPQ reader with an O(1) hash lookup.

    mpyq scans the whole hash table for every name; enumerating ~40k
    candidate names against 15k-entry tables that way takes hours."""
    from . import baseline as B
    rd = B.default_reader_factory(path)
    a = rd.a
    index = {}
    for e in a.hash_table:
        index.setdefault((e.hash_a, e.hash_b), e)

    def lookup(filename, a=a, index=index):
        return index.get((a._hash(filename, "HASH_A"), a._hash(filename, "HASH_B")))
    a.get_hash_table_entry = lookup
    return rd


def open_archives(pd2_dir, base_dir, reader_factory=None):
    """``(pd2 [(slot, reader)], base [(slot, reader)])`` from explicit dirs."""
    reader_factory = reader_factory or indexed_reader_factory
    pd2, base = [], []
    for slots, names, folder, out in ((PD2_SLOTS, PD2_ARCHIVES, Path(pd2_dir), pd2),
                                      (BASE_SLOTS, BASE_ARCHIVES, Path(base_dir), base)):
        for slot, n in zip(slots, names):
            p = _find_file(folder, n)
            if p is not None:
                out.append((slot, reader_factory(p)))
    return pd2, base


def classify_file(pd2_data: bytes, vanilla_read, vanilla_size=None) -> str:
    """Class of one PD2 file against the vanilla one (``vanilla_read`` is None
    when vanilla has no such path). A different size is a different file, so
    vanilla's bytes are read only when the sizes agree."""
    if vanilla_read is None:
        return UNIQUE
    if vanilla_size is not None and vanilla_size != len(pd2_data):
        return OVERRIDE
    return IDENTICAL if vanilla_read() == pd2_data else OVERRIDE


def _enumerate(archives, extra_names):
    """Per-archive names, with the listfile and what could not be named."""
    listed = {slot: rd.namelist() for slot, rd in archives}
    supp = {}
    for names in listed.values():
        for n in names:
            supp.setdefault(n.lower(), n)
    for n in extra_names:
        supp.setdefault(n.lower(), n)
    out = {}
    for slot, rd in archives:
        names = rd.namelist(extra_names=list(supp.values()))
        has_list = bool(rd.has_listfile)
        blocks = rd.block_count
        unnamed = max(0, blocks - len(names) - (1 if has_list else 0))
        out[slot] = {"names": names, "listed": len(listed[slot]),
                     "record": {"slot": slot, "listfile": has_list, "blocks": blocks,
                                "names": len(names), "unlisted_found":
                                len(names) - len(listed[slot]), "unnamed": unnamed}}
    return out


def build_inventory(pd2_archives, base_archives, extra_names=()) -> dict:
    """Enumerate and classify every PD2 DS1/DT1/TXT/DCC/DC6/COF.

    ``pd2_archives`` / ``base_archives``: ordered ``[(slot, reader)]``. The
    effective PD2 file is the first PD2 archive holding the path; later copies
    are listed under ``shadowed``."""
    enum = _enumerate(list(pd2_archives) + list(base_archives), extra_names)
    limits, archives = [], []
    for slot, rd in list(pd2_archives) + list(base_archives):
        rec = dict(enum[slot]["record"])
        rec["name"] = getattr(rd, "name", slot)
        rec["role"] = "pd2" if (slot, rd) in list(pd2_archives) else "base"
        archives.append(rec)
        if rec["role"] != "pd2":
            continue
        if not rec["listfile"]:
            limits.append("%s (%s): no (listfile); %d of %d blocks named via sibling "
                          "listfiles, %d unnamed and cannot be enumerated"
                          % (slot, rec["name"], rec["names"], rec["blocks"], rec["unnamed"]))
        elif rec["unnamed"]:
            limits.append("%s (%s): %d block(s) not named by its (listfile)"
                          % (slot, rec["name"], rec["unnamed"]))

    seen, shadow = {}, {}
    for slot, rd in pd2_archives:
        for name in enum[slot]["names"]:
            k = kind_of(name)
            if k is None:
                continue
            n = norm(name)
            if n in seen:
                shadow.setdefault(n, []).append(slot)
                continue
            seen[n] = (slot, rd, name, k)

    files = {}
    counts = {k: {c: 0 for c in CLASSES} for k in KIND_BY_EXT.values()}
    for n, (slot, rd, name, k) in sorted(seen.items()):
        data = rd.read(name)
        if data is None:
            limits.append("%s: %s is named but could not be read" % (slot, name))
            continue
        vread = vsize = vslot = None
        vcache = {}
        for bslot, brd in base_archives:
            if brd.has(name):
                ent = brd.entries([name])
                vsize = ent[0][2] if ent else None

                def vread(brd=brd, name=name):
                    vcache["b"] = brd.read(name)
                    return vcache["b"]
                vslot = bslot
                break
        cls = classify_file(data, vread, vsize)
        files[n] = {"path": n, "name": name, "kind": k, "archive": slot, "size": len(data),
                    "sha256": sha256_bytes(data), "class": cls,
                    "vanilla_archive": vslot,
                    "vanilla_sha256": sha256_bytes(vcache["b"]) if vcache.get("b") is not None
                    else None}
        counts[k][cls] += 1
    shadowed = [{"path": n, "effective": seen[n][0], "shadowed_in": s}
                for n, s in sorted(shadow.items())]
    return {"archives": archives, "files": files, "counts": counts,
            "shadowed": shadowed, "limits": limits}


# ---------------------------------------------------------------------------
# process outcomes
# ---------------------------------------------------------------------------

def classify_exit(rc, timed_out: bool = False) -> str:
    """``ok`` / ``rejected`` (the probes' declared rejection, exit 1) /
    ``environment`` (DLL not found: not the editor's fault) / ``hang`` /
    ``crash`` (any other nonzero exit, including bad usage and signals)."""
    if timed_out:
        return "hang"
    if rc is None:
        return "crash"
    if (rc & 0xFFFFFFFF) == STATUS_DLL_NOT_FOUND:
        return "environment"
    if rc == 0:
        return "ok"
    if rc == 1:
        return "rejected"
    return "crash"


def run_graphics(files, runner, jobs: int = 4) -> dict:
    """Run ``runner(item) -> (rc, stderr, timed_out[, command])`` over ``files``
    (ordered) and keep the outcome counts plus every non-ok result."""
    def one(item):
        res = runner(item)
        rc, err, to = res[0], res[1], res[2]
        rec = {"path": item["path"], "kind": item["kind"],
               "outcome": classify_exit(rc, to), "returncode": rc,
               "stderr": (err or "")[-600:]}
        if len(res) > 3:
            rec["command"] = res[3]
        return rec

    with ThreadPoolExecutor(max_workers=max(1, jobs)) as ex:
        results = list(ex.map(one, files))
    counts, defects, rejections, env = {}, [], [], []
    for r in results:
        c = counts.setdefault(r["kind"], {})
        c[r["outcome"]] = c.get(r["outcome"], 0) + 1
        if r["outcome"] in ("crash", "hang"):
            defects.append(r)
        elif r["outcome"] == "rejected":
            rejections.append(r)
        elif r["outcome"] == "environment":
            env.append(r)
    return {"total": len(results), "counts": counts, "defects": defects,
            "rejections": rejections, "environment": env}


def graphics_status(res: dict) -> str:
    """Crashes, hangs and undeclared exits fail. A declared rejection (the
    probe's exit 1) is the decoder refusing a file, which is its contract; it
    is reported as a finding on the case (never hidden) but does not fail it."""
    if res["defects"]:
        return "mismatch"
    if res["environment"]:
        return "unavailable"
    return "pass"


def write_repro(failures_dir: Path, item: dict, src: Path) -> Path:
    """Keep the input and the command that crashed a probe."""
    d = Path(failures_dir) / slug(item["path"])
    d.mkdir(parents=True, exist_ok=True)
    ext = os.path.splitext(item["path"])[1].lower() or ".bin"
    data = Path(src).read_bytes()
    (d / ("input" + ext)).write_bytes(data)
    meta = {k: item.get(k) for k in ("path", "kind", "outcome", "returncode",
                                     "command", "stderr")}
    meta["input_sha256"] = sha256_bytes(data)
    (d / "repro.json").write_text(json.dumps(meta, indent=1, sort_keys=True),
                                  encoding="utf-8")
    return d


# ---------------------------------------------------------------------------
# render checks
# ---------------------------------------------------------------------------

_PD2_LINE = re.compile(r"Project Diablo 2 data from <([^>]*)>")


def _dir_key(p: str) -> str:
    return p.replace("/", "\\").rstrip("\\").lower()


def render_uses_pd2(text: str, pd2_dir) -> tuple:
    """Did the run say it read PD2 data from the requested folder? ``(ok, why)``.
    The vanilla fallback line, the use_pd2=yes error, a missing line or another
    folder all fail."""
    if re.search(r"ERROR use_pd2=yes but no Project Diablo 2 folder", text):
        return False, "use_pd2=yes found no Project Diablo 2 folder (vanilla fallback)"
    m = _PD2_LINE.search(text)
    if m is None:
        return False, ("no 'Project Diablo 2 data from' line in the output: the run "
                       "used vanilla data")
    if _dir_key(m.group(1)) != _dir_key(str(pd2_dir)):
        return False, "PD2 data came from %s, not the requested %s" % (m.group(1), pd2_dir)
    return True, "Project Diablo 2 data from <%s>" % m.group(1)


def read_source(text: str, name: str):
    """The archive the editor read ``name`` from, per its 'want to read' log
    (the last read of that file name)."""
    base = name.replace("/", "\\").rsplit("\\", 1)[-1].lower()
    found = None
    for r in R.parse_resource_reads(text):
        if r["name"].replace("/", "\\").rsplit("\\", 1)[-1].lower() == base:
            found = r["source"]
    return found


RENDER_INI = """d2_install = {d2_install}
pd2_dir = {pd2_dir}
use_pd2 = yes
mod_dir =
upscale_enabled = NO
full_screen = NO
gamma_correction = {gamma}
default_zoom = {zoom}
only_normal_type2 = YES
always_max_layers = YES
"""


def render_ini(d2_dir, pd2_dir, zoom="1:4", gamma="1.00") -> str:
    """The owned ds1edit.ini of a render runtime: windowed, PD2 on, both
    directories explicit, no mod directory, no upscaler."""
    return RENDER_INI.format(d2_install=d2_dir, pd2_dir=pd2_dir, zoom=zoom, gamma=gamma)


def select_render_set(files: dict, usable, unique_count: int = 4) -> list:
    """The fixed render set: ``unique_count`` PD2-unique maps (one smallest
    usable map per area directory, areas spread evenly over the sorted list)
    plus the first sorted PD2-overridden town map. Deterministic."""
    ds1 = {p: f for p, f in files.items() if f.get("kind") == "ds1" and usable(p)}
    groups = {}
    for p in sorted(ds1):
        if ds1[p]["class"] == UNIQUE:
            groups.setdefault(p.rsplit("\\", 1)[0], []).append(p)
    names = sorted(groups)
    if len(names) > unique_count > 1:
        idx = sorted({round(i * (len(names) - 1) / (unique_count - 1))
                      for i in range(unique_count)})
        names = [names[i] for i in idx]
    else:
        names = names[:unique_count]
    sel = []
    for g in names:
        best = min(groups[g], key=lambda p: (ds1[p]["size"], p))
        sel.append({"path": best, "role": "unique", "size": ds1[best]["size"]})
    towns = [p for p in sorted(ds1) if ds1[p]["class"] == OVERRIDE
             and "\\town\\" in p.rsplit("\\", 1)[0] + "\\"]
    if towns:
        sel.append({"path": towns[0], "role": "override_town",
                    "size": ds1[towns[0]]["size"]})
    return sel


# ---------------------------------------------------------------------------
# local baseline (hashes + images under tmp/pd2/baseline, never committed)
# ---------------------------------------------------------------------------

def files_identical(a: Path, b: Path) -> bool:
    return Path(a).read_bytes() == Path(b).read_bytes()


def baseline_check(baseline_dir, identity: dict, shots: dict) -> dict:
    """Compare renders with the local baseline, or create it.

    ``shots``: ``{case id: image path}``. Per case ``outcome`` is ``created``
    (first sighting; the baseline is then marked ``reviewed: false``),
    ``match``, ``mismatch`` or ``incomparable`` (a baseline for another PD2
    identity, or an unreadable one: nothing is compared and nothing is
    overwritten)."""
    bdir = Path(baseline_dir)
    meta_path = bdir / "baseline.json"
    meta = None
    unreadable = False
    if meta_path.is_file():
        try:
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
            if not isinstance(meta, dict) or "cases" not in meta:
                raise ValueError("not a baseline")
        except (OSError, ValueError):
            unreadable = True
    if unreadable:
        return {k: {"outcome": "incomparable", "reasons": ["baseline.json is unreadable"]}
                for k in shots}
    if meta is not None:
        cmp_ = compare_identity(meta.get("identity"), identity)
        if cmp_["verdict"] != "comparable":
            return {k: {"outcome": "incomparable", "reasons": cmp_["reasons"],
                        "reviewed": meta.get("reviewed", False)} for k in shots}
    else:
        meta = {"schema": BASELINE_SCHEMA, "identity": identity, "reviewed": False,
                "created": utc_stamp(), "cases": {}}
    out, changed = {}, False
    (bdir / "images").mkdir(parents=True, exist_ok=True)
    for cid, shot in shots.items():
        shot = Path(shot)
        data = shot.read_bytes()
        sha = sha256_bytes(data)
        have = meta["cases"].get(cid)
        if have is None:
            img = "images/%s%s" % (slug(cid), shot.suffix.lower())
            (bdir / img).write_bytes(data)
            meta["cases"][cid] = {"sha256": sha, "image": img, "size": len(data)}
            meta["reviewed"] = False
            changed = True
            out[cid] = {"outcome": "created", "sha256": sha}
        else:
            out[cid] = {"outcome": "match" if have["sha256"] == sha else "mismatch",
                        "sha256": sha, "baseline_sha256": have["sha256"],
                        "baseline_image": str(bdir / have["image"])}
    if changed or not meta_path.is_file():
        meta_path.write_text(json.dumps(meta, indent=1, sort_keys=True), encoding="utf-8")
    for v in out.values():
        v["reviewed"] = bool(meta.get("reviewed", False))
    return out


# ---------------------------------------------------------------------------
# cases, summary, exit code
# ---------------------------------------------------------------------------

def _platform_id() -> str:
    try:
        from . import replay
        return replay.platform_id()
    except Exception:  # pragma: no cover
        return "windows-x86" if os.name == "nt" else sys.platform


def make_case(case_id: str, status: str, identity: dict, failures=None, *,
              coverage=None, extra=None, limitations=None, platform=None,
              baseline_created: bool = False, candidate=None) -> dict:
    """A ``ds1edit.case/1`` for one lane result. Always ``required: false`` and
    in lane ``pd2``; ``incomparable`` is not a case status, so it is stored as
    ``unavailable`` with ``comparison: incomparable``."""
    incomparable = status == "incomparable"
    if incomparable:
        status = "unavailable"
    if status not in C.STATUSES:
        raise ValueError("unknown status %r" % status)
    case = {
        "schema": C.CASE_SCHEMA, "id": case_id, "lane": LANE,
        "platform": platform or _platform_id(), "required": False,
        "status": status, "success_outcome": "pass",
        "contract_digest": None, "baseline_digest": identity.get("digest"),
        "input_sha256": None, "expected_sha256": None, "actual_sha256": None,
        "coverage": dict(coverage or {}), "failures": list(failures or []),
        "provenance": {"candidate": candidate or {},
                       "reference": {"kind": "pd2-local-self-comparison",
                                     "pd2_version": identity.get("version"),
                                     "pd2_version_source": identity.get("version_source"),
                                     "pd2_digest": identity.get("digest")}},
        "limitations": list(LIMITATIONS) + list(limitations or []),
    }
    if incomparable:
        case["comparison"] = "incomparable"
    if baseline_created:
        case["baseline_created"] = True
    if extra:
        case.update(extra)
    return case


def exit_code(cases) -> int:
    if not cases:
        return 2
    st = [c["status"] for c in cases]
    if "mismatch" in st:
        return 1
    for c in cases:
        if c["status"] in ("unavailable", "not_run"):
            return 2
        if c["status"] == "partial" and not c.get("baseline_created"):
            return 2
    return 0


def summary_table(cases) -> str:
    rows = [(c["id"], c["status"], str(len(c.get("failures", [])))) for c in cases]
    w = max([len(r[0]) for r in rows] + [4])
    lines = ["%-*s  %-11s  %s" % (w, "case", "status", "failures")]
    lines += ["%-*s  %-11s  %s" % (w, a, b, c) for a, b, c in rows]
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# real run: context
# ---------------------------------------------------------------------------

class Ctx:
    def __init__(self, **kw):
        self.pd2_dir = Path(kw["pd2_dir"])
        self.d2_dir = Path(kw["d2_dir"])
        self.exe = Path(kw["exe"]) if kw.get("exe") else None
        self.probe_ds1 = Path(kw["probe_ds1"]) if kw.get("probe_ds1") else None
        self.probe_visual = Path(kw["probe_visual"]) if kw.get("probe_visual") else None
        self.probe_robust = Path(kw["probe_robust"]) if kw.get("probe_robust") else None
        self.out_root = Path(kw["out_root"]).resolve()
        self.run_dir = Path(kw["run_dir"]).resolve()
        self.jobs = kw.get("jobs") or os.cpu_count() or 4
        self.limit = kw.get("limit")
        self.manifest = Path(kw["manifest"]) if kw.get("manifest") else None
        self.identity = None
        self.inventory = None
        self.cases = []

    def add(self, case):
        self.cases.append(case)
        d = self.run_dir / "cases"
        d.mkdir(parents=True, exist_ok=True)
        (d / (slug(case["id"]) + ".json")).write_text(
            json.dumps(case, indent=1, sort_keys=True), encoding="utf-8")
        return case

    def candidate(self, probe=None):
        try:
            from . import replay
            return replay.default_provenance(probe or self.exe or Path(sys.executable))
        except Exception:  # pragma: no cover
            return {}

    def case(self, cid, status, failures=None, **kw):
        kw.setdefault("candidate", self.candidate())
        return self.add(make_case(CASE_PREFIX + cid, status, self.identity, failures, **kw))


def cli_flags(ctx) -> list:
    """Explicit installation flags for every ds1edit CLI call: no ini, no
    auto-detection."""
    return ["--no-ini", "--d2-install=" + str(ctx.d2_dir), "--pd2-dir=" + str(ctx.pd2_dir),
            "--use-pd2=yes"]


def _child_env(exe: Path) -> dict:
    return ds1_cases._probe_env(exe)


def run_proc(cmd, cwd, env, timeout=PROBE_TIMEOUT):
    """``(rc, stdout, stderr, timed_out)``; never raises on a crash or hang."""
    try:
        p = subprocess.run([str(c) for c in cmd], cwd=str(cwd), env=env, capture_output=True,
                           timeout=timeout, shell=False)
        return (p.returncode, p.stdout.decode("utf-8", "replace"),
                p.stderr.decode("utf-8", "replace"), False)
    except subprocess.TimeoutExpired as e:
        return (None, (e.stdout or b"").decode("utf-8", "replace"),
                (e.stderr or b"").decode("utf-8", "replace"), True)
    except OSError as e:
        return (127, "", "could not start %s: %s" % (cmd[0], e), False)


class Extractor:
    """Thread-safe extraction of effective PD2 bytes (readers share a file
    handle, so reads are serialised; the probes run outside the lock)."""

    def __init__(self, chain, root: Path):
        self.chain, self.root, self.lock = chain, Path(root), threading.Lock()

    def extract(self, entry: dict) -> Path:
        dest = self.root / slug(entry["path"])[:120]
        dest = dest.with_suffix("") if False else dest
        dest = self.root / (slug(entry["path"]) + os.path.splitext(entry["path"])[1])
        with self.lock:
            hit = self.chain.read(entry["name"])
        if hit is None:
            raise Pd2Error("%s vanished from the chain" % entry["path"])
        data = hit[0]
        if sha256_bytes(data) != entry["sha256"]:
            raise Pd2Error("%s changed since the inventory" % entry["path"])
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
        return dest


# ---------------------------------------------------------------------------
# real run: identity + inventory
# ---------------------------------------------------------------------------

def run_identity(ctx):
    ctx.identity = compute_identity(ctx.pd2_dir, ctx.d2_dir)
    (ctx.run_dir / "identity.json").write_text(
        json.dumps(ctx.identity, indent=1, sort_keys=True), encoding="utf-8")
    fails = []
    mc = ctx.identity.get("metadata_check")
    if mc and mc["differ"]:
        fails.append({"kind": "metadata_md5", "detail":
                      "local_metadata.json lists a different MD5 for %s" % ", ".join(mc["differ"])})
    ctx.case("identity", "pass" if not fails else "mismatch", fails,
             coverage={"archives_hashed": sum(1 for a in ctx.identity["archives"]
                                              if a["present"]),
                       "pd2_version": ctx.identity["version"]},
             extra={"identity": {k: ctx.identity[k] for k in
                                 ("version", "version_source", "digest", "metadata_check")}})


def _manifest_crosscheck(ctx, inv):
    """How the vanilla corpus manifest relates to the vanilla archives in use."""
    if not ctx.manifest or not ctx.manifest.is_file():
        return None
    m = C.load_json_object(ctx.manifest)
    ours = {a["name"].lower(): a["sha256"] for a in ctx.identity["archives"]
            if a["role"] == "base" and a["present"]}
    arch = {a["name"].lower(): ("match" if ours.get(a["name"].lower()) == a["sha256"]
                                else "differs" if a["name"].lower() in ours else "absent")
            for a in m.get("archives", [])}
    names = {a["path"] for a in m.get("assets", [])}
    ov = [p for p, f in inv["files"].items() if f["class"] == OVERRIDE and p in names]
    return {"manifest_digest": m.get("digest"), "base_archives_vs_manifest": arch,
            "pd2_overrides_also_in_manifest": len(ov),
            "note": "classification uses the vanilla archives actually in the chain; "
                    "the manifest is a cross-reference only"}


def _supplementary_names(ctx):
    if not ctx.manifest or not ctx.manifest.is_file():
        return []
    m = C.load_json_object(ctx.manifest)
    return [a.get("original_path") or a["path"] for a in m.get("assets", [])]


def load_or_build_inventory(ctx, force=False):
    cache = ctx.out_root / "cache" / ("inventory-%s.json" % ctx.identity["digest"][:16])
    if cache.is_file() and not force:
        ctx.inventory = json.loads(cache.read_text(encoding="utf-8"))
        return ctx.inventory
    pd2, base = open_archives(ctx.pd2_dir, ctx.d2_dir)
    try:
        inv = build_inventory(pd2, base, _supplementary_names(ctx))
    finally:
        for _, rd in pd2 + base:
            c = getattr(rd, "close", None)
            if c:
                c()
    inv["identity_digest"] = ctx.identity["digest"]
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text(json.dumps(inv, indent=1, sort_keys=True), encoding="utf-8")
    ctx.inventory = inv
    return inv


def run_inventory(ctx):
    inv = load_or_build_inventory(ctx)
    (ctx.run_dir / "inventory.json").write_text(
        json.dumps(inv, indent=1, sort_keys=True), encoding="utf-8")
    fails = [{"kind": "enumeration_limit", "detail": l} for l in inv["limits"]]
    by_class = {}
    for f in inv["files"].values():
        by_class.setdefault(f["class"], {}).setdefault(f["kind"], 0)
        by_class[f["class"]][f["kind"]] += 1
    # enumeration limits are findings, not failures: the lane says what it
    # could not see, but a missing listfile is PD2's packaging, not an editor defect
    ctx.case("inventory", "pass" if inv["files"] else "mismatch",
             [] if inv["files"] else [{"kind": "empty", "detail": "no PD2 files found"}],
             coverage={"counts": inv["counts"], "classes": by_class,
                       "files": len(inv["files"]), "shadowed": len(inv["shadowed"]),
                       "enumeration_limits": inv["limits"]},
             extra={"archives": inv["archives"],
                    "manifest": _manifest_crosscheck(ctx, inv)},
             limitations=inv["limits"])
    return inv


# ---------------------------------------------------------------------------
# real run: maps
# ---------------------------------------------------------------------------

def _text(chain, path):
    hit = chain.read(path)
    if hit is None:
        raise Pd2Error("%s is not in the PD2 chain" % path)
    return hit[0].decode("latin-1")


def resolve_ids(tiles, path):
    """``(LvlTypes id, LvlPrest Def)`` of a DS1 from an authoritative LvlPrest row."""
    for d, lid, _mask, _row in tiles.prest.get(path, []):
        if lid > 0:
            lt = tiles.level_type.get(lid)
            if lt is not None and lt in tiles.types:
                return lt, d
    return None


def run_maps(ctx, chain, inv):
    tiles = S.TileInputs(_text(chain, EXCEL + "lvlprest.txt"), _text(chain, EXCEL + "levels.txt"),
                         _text(chain, EXCEL + "lvltypes.txt"), chain.pathset())
    ds1 = sorted((f for f in inv["files"].values()
                  if f["kind"] == "ds1" and f["class"] in (UNIQUE, OVERRIDE)),
                 key=lambda f: f["path"])
    if ctx.limit:
        ds1 = ds1[:ctx.limit]
    maps_dir = ctx.run_dir / "maps"
    ext = Extractor(chain, maps_dir / "corpus")
    prov = {"platform": _platform_id(), "candidate": ctx.candidate(ctx.probe_ds1)}
    digest = ctx.identity["digest"]
    S.quiet_windows_errors()

    def one(f):
        asset = {"path": f["path"], "sha256": f["sha256"], "size": f["size"],
                 "archive": f["archive"]}
        try:
            local, err = ext.extract(f), None
        except Exception as e:
            local, err = None, str(e)
        case = S.sweep_one(asset, local, err, ctx.probe_ds1, maps_dir / "work", tiles,
                           digest, prov)
        case["pd2_class"] = f["class"]
        return case

    # PD2 maps were saved by the editor, so they carry its DS1EDIT_WRKSPC_*
    # marker entries in the file list, which every save rewrites: declared
    # normalization, applied for this sweep only (real file entries still compare)
    saved_policy = list(S.BASE_POLICY)
    S.BASE_POLICY[:] = saved_policy + ["workspace_files_ignored"]
    try:
        with ThreadPoolExecutor(max_workers=max(1, ctx.jobs)) as ex:
            cases = list(ex.map(one, ds1))
    finally:
        S.BASE_POLICY[:] = saved_policy
    manifest = {"digest": digest, "enumeration": {"limits": inv["limits"]},
                "assets": [{"kind": "ds1", "path": f["path"]} for f in ds1]}
    agg = S.aggregate(cases, manifest)
    agg["by_class"] = {c: sum(1 for f in ds1 if f["class"] == c) for c in (UNIQUE, OVERRIDE)}
    (ctx.run_dir / "maps-aggregate.json").write_text(
        json.dumps(agg, indent=1, sort_keys=True), encoding="utf-8")

    for c in cases:
        failing = c["status"] != "pass"
        cc = dict(c)
        cc.update({"id": CASE_PREFIX + "maps/" + c["ds1"]["path"].replace("\\", "/"),
                   "lane": LANE, "required": False, "baseline_digest": digest,
                   "limitations": list(c.get("limitations", [])) + LIMITATIONS})
        cc["provenance"] = dict(cc.get("provenance", {}))
        cc["provenance"]["reference"] = dict(cc["provenance"].get("reference", {}),
                                             pd2_version=ctx.identity["version"],
                                             pd2_digest=digest)
        # one file per map, kept even when it passes: the sweep is the evidence
        ctx.add(cc) if failing or True else None

    unresolved = agg["unresolved_keys_total"]
    bad_map = [c["id"] for c in cases if c["status"] != "pass"]
    ctx.case("maps/summary", "pass" if not bad_map else "mismatch",
             [{"kind": "map", "detail": b} for b in bad_map[:50]],
             coverage={"maps": len(cases), "status_counts": agg["status_counts"],
                       "read_ok": agg["read_ok"], "rejected": len(agg["rejected"]),
                       "round_trip_mismatches": len(agg["round_trip_mismatches"]),
                       "header_mismatches": len(agg["header_mismatches"]),
                       "by_class": agg["by_class"], "versions": agg["versions"]})
    dep_fail = [u for u in agg["unresolved_keys"]]
    ctx.case("maps/dependencies", "pass" if not unresolved else "mismatch",
             [{"kind": "unresolved_dependency", **u} for u in dep_fail[:100]],
             coverage={"key_classes": agg["required_input_key_classes"],
                       "unresolved_total": unresolved})
    return cases, tiles, agg


# ---------------------------------------------------------------------------
# real run: tables and area browser
# ---------------------------------------------------------------------------

def check_table_text(text: str) -> dict:
    """Independent shape check of one tab-separated table."""
    lines = [l for l in text.replace("\r\n", "\n").split("\n") if l != ""]
    if not lines:
        return {"rows": 0, "columns": 0, "ragged": [], "duplicate_headers": []}
    head = lines[0].split("\t")
    seen, dup = {}, []
    for h in head:
        if h and h in seen:
            dup.append(h)
        seen[h] = 1
    ragged = [i + 2 for i, l in enumerate(lines[1:]) if len(l.split("\t")) != len(head)]
    return {"rows": len(lines) - 1, "columns": len(head), "ragged": ragged[:20],
            "ragged_total": len(ragged), "duplicate_headers": dup,
            "first_id": lines[1].split("\t")[0] if len(lines) > 1 else None}


_BROWSER_LINE = re.compile(r"^\s*(\S.*?)\s+LvlType=(-?\d+)\s+Def=(-?\d+)\s+\((.+)\)\s*$")


def parse_list_files(stdout: str) -> dict:
    """``list-files`` output: ``{normalized tiles-relative path: area}`` plus the total."""
    out, total = {}, None
    for line in stdout.splitlines():
        m = _BROWSER_LINE.match(line)
        if m:
            out[norm(m.group(1))] = m.group(4)
            continue
        t = re.match(r"^Total:\s*(\d+)\s+DS1 files", line)
        if t:
            total = int(t.group(1))
    return {"files": out, "total": total}


def parse_list_areas(stdout: str) -> dict:
    groups = {}
    for line in stdout.splitlines():
        m = re.match(r"^\s*(Act \d+ - .+?)\s+\((\d+) maps, LvlType (-?\d+)\)\s*$", line)
        if m:
            groups[m.group(1)] = int(m.group(2))
    return groups


def parse_count(stdout: str) -> int:
    """Rows printed by list-tokens (indented, non-empty, not log noise)."""
    n = 0
    for line in stdout.splitlines():
        if re.match(r"^  \S", line) and not line.startswith(("  [", "  ok ")) \
                and "d2install" not in line:
            n += 1
    return n


def run_tables(ctx, chain, inv):
    env = _child_env(ctx.exe)
    cwd = ctx.run_dir / "tables"
    cwd.mkdir(parents=True, exist_ok=True)
    flags = cli_flags(ctx)
    info = {}
    for name, path in TABLES:
        hit = chain.read(path)
        fails, cov = [], {}
        if hit is None:
            fails.append({"kind": "missing", "detail": "%s is not in the PD2 chain" % path})
            ctx.case("tables/" + name, "mismatch", fails)
            continue
        text = hit[0].decode("latin-1")
        shape = check_table_text(text)
        cov.update(source=hit[1], rows=shape["rows"], columns=shape["columns"],
                   ragged_rows=shape.get("ragged_total", 0))
        for h in shape["duplicate_headers"]:
            cov.setdefault("duplicate_headers", []).append(h)
        if shape["rows"] == 0:
            fails.append({"kind": "empty", "detail": "%s has no data rows" % path})
        rc, out, err, to = run_proc([ctx.exe, "dump-txt-row", path, shape["first_id"] or "x"] + flags,
                                    cwd, env)
        cols = len(re.findall(r"^\s+\[\s*\d+\]", out, re.M))
        cov["editor_columns"] = cols
        if classify_exit(rc, to) != "ok":
            fails.append({"kind": "editor", "detail": "dump-txt-row exit %s%s" % (
                rc, " (timeout)" if to else ""), "stderr": err[-300:]})
        elif cols < shape["columns"]:
            fails.append({"kind": "editor", "detail":
                          "editor printed %d columns, the table has %d" % (cols, shape["columns"])})
        if not re.search(r"PD2|pd2", out + err) or "Project Diablo 2 data from" not in out + err:
            fails.append({"kind": "vanilla_data", "detail":
                          "no 'Project Diablo 2 data from' line in the editor's output"})
        info[name] = shape
        ctx.case("tables/" + name, "pass" if not fails else "mismatch", fails, coverage=cov)

    # area browser: Levels + LvlPrest + LvlTypes joined by the editor
    la = run_proc([ctx.exe, "list-areas"] + flags, cwd, env)
    lf = run_proc([ctx.exe, "list-files"] + flags, cwd, env)
    fails = []
    for label, r in (("list-areas", la), ("list-files", lf)):
        if classify_exit(r[0], r[3]) != "ok":
            fails.append({"kind": "editor", "detail": "%s exit %s" % (label, r[0]),
                          "stderr": r[2][-300:]})
    groups = parse_list_areas(la[2] + la[1])
    browser = parse_list_files(lf[1])
    pd2_ds1 = {p[len("data\\global\\tiles\\"):]: f for p, f in inv["files"].items()
               if f["kind"] == "ds1" and f["class"] in (UNIQUE, OVERRIDE)
               and p.startswith("data\\global\\tiles\\")}
    stat = {"listed": 0, "unlisted": 0, "absent": 0}
    absent = []
    for rel, f in sorted(pd2_ds1.items()):
        area = browser["files"].get(rel)
        if area is None:
            stat["absent"] += 1
            absent.append(f["path"])
        elif area.lower().endswith("unlisted"):
            stat["unlisted"] += 1
        else:
            stat["listed"] += 1
    if browser["total"] is None or not browser["files"]:
        fails.append({"kind": "editor", "detail": "list-files printed no map list"})
    if stat["absent"]:
        fails.append({"kind": "absent_from_browser", "detail":
                      "%d PD2 map(s) are not in the area browser index" % stat["absent"],
                      "examples": absent[:20]})
    ctx.case("tables/area-browser", "pass" if not fails else "mismatch", fails,
             coverage={"groups": len(groups), "browser_total": browser["total"],
                       "pd2_maps": len(pd2_ds1), **{"pd2_" + k: v for k, v in stat.items()},
                       "group_map_counts": groups})
    # object and monster index (compose index: Objects.txt / MonStats.txt)
    for cat in ("monsters", "objects"):
        r = run_proc([ctx.exe, "list-tokens", cat] + flags, cwd, env)
        n = parse_count(r[1])
        bad = []
        if classify_exit(r[0], r[3]) != "ok":
            bad.append({"kind": "editor", "detail": "list-tokens %s exit %s" % (cat, r[0]),
                        "stderr": r[2][-300:]})
        elif n == 0:
            bad.append({"kind": "editor", "detail": "list-tokens %s printed no tokens" % cat})
        ctx.case("tables/tokens-" + cat, "pass" if not bad else "mismatch", bad,
                 coverage={"tokens": n})
    return info


# ---------------------------------------------------------------------------
# real run: graphics
# ---------------------------------------------------------------------------

def run_graphics_real(ctx, chain, inv):
    items = sorted((f for f in inv["files"].values() if f["kind"] in ("dcc", "dc6", "dt1", "cof")),
                   key=lambda f: f["path"])
    if ctx.limit:
        items = items[:ctx.limit]
    gdir = ctx.run_dir / "graphics"
    ext = Extractor(chain, gdir / "in")
    env = _child_env(ctx.probe_visual or ctx.probe_robust)
    S.quiet_windows_errors()
    failures_dir = ctx.run_dir / "failures"
    keep = []
    lock = threading.Lock()

    def runner(item):
        f = inv["files"][item["path"]]
        src = ext.extract(f)
        work = Path(tempfile.mkdtemp(prefix="g", dir=str(gdir)))
        try:
            if f["kind"] == "cof":
                cmd = [ctx.probe_robust, "cof", "--input", src]
            else:
                cmd = [ctx.probe_visual, "decode", "--kind", f["kind"], "--input", src,
                       "--direction", "0" if f["kind"] == "dt1" else "-1", "--out-dir", work]
            rc, out, err, to = run_proc(cmd, work, env)
            cmd_s = [str(c) for c in cmd]
            if classify_exit(rc, to) in ("crash", "hang", "rejected"):
                rec = {"path": f["path"], "kind": f["kind"], "returncode": rc,
                       "outcome": classify_exit(rc, to), "command": cmd_s, "stderr": err[-600:]}
                with lock:
                    write_repro(failures_dir, rec, src)
            return rc, (err or out), to, cmd_s
        finally:
            shutil.rmtree(work, ignore_errors=True)
            try:
                src.unlink()
            except OSError:
                pass

    gdir.mkdir(parents=True, exist_ok=True)
    res = run_graphics([{"path": f["path"], "kind": f["kind"]} for f in items], runner, ctx.jobs)
    (ctx.run_dir / "graphics-results.json").write_text(
        json.dumps({k: res[k] for k in ("total", "counts", "defects", "rejections",
                                        "environment")}, indent=1, sort_keys=True),
        encoding="utf-8")
    for kind in ("dcc", "dc6", "dt1", "cof"):
        sub = {k: [r for r in res[k] if r["kind"] == kind]
               for k in ("defects", "rejections", "environment")}
        st = graphics_status(sub)
        fails = [{"kind": r["outcome"], "path": r["path"], "returncode": r["returncode"],
                  "stderr": r["stderr"][-200:]}
                 for r in sub["defects"] + sub["environment"]]
        findings = [{"kind": "declared_rejection", "path": r["path"],
                     "stderr": r["stderr"][-200:]} for r in sub["rejections"]]
        ctx.case("graphics/" + kind, st, fails[:200], extra={"findings": findings},
                 coverage={"files": sum(res["counts"].get(kind, {}).values()),
                           "outcomes": res["counts"].get(kind, {}),
                           "failures_total": len(fails),
                           "declared_rejections": len(findings)})
    # AnimData.d2 through the robustness probe
    hit = chain.read(ANIMDATA)
    if hit is not None and ctx.probe_robust:
        p = gdir / "animdata.d2"
        p.write_bytes(hit[0])
        rc, out, err, to = run_proc([ctx.probe_robust, "animdata", "--input", p], gdir, env)
        o = classify_exit(rc, to)
        ctx.case("graphics/animdata", "pass" if o == "ok" else "mismatch",
                 [] if o == "ok" else [{"kind": o, "returncode": rc, "stderr": err[-300:]}],
                 coverage={"source": hit[1], "size": len(hit[0])})
    return res


# ---------------------------------------------------------------------------
# real run: render
# ---------------------------------------------------------------------------

def build_runtime(ctx, runtime: Path) -> list:
    """A private windowed runtime: the candidate, its DLLs, the editor assets
    the start-up insists on, and an owned ds1edit.ini. Never touches the
    repository's ini files."""
    problems = []
    if runtime.exists():
        shutil.rmtree(runtime)
    (runtime / "assets" / "editor").mkdir(parents=True)
    (runtime / "appdata").mkdir()
    shutil.copy2(ctx.exe, runtime / ctx.exe.name)
    for f in ctx.exe.parent.iterdir():
        if f.is_file() and f.suffix.lower() in (".dll", ".so", ".dylib", ".cfg"):
            shutil.copy2(f, runtime / f.name)
    ui = REPO_ROOT / "assets" / "ui"
    if ui.is_dir():
        shutil.copytree(ui, runtime / "assets" / "ui")
    for n in ("ds1edit.dt1", "obj.txt"):
        src = REPO_ROOT / "assets" / "editor" / n
        if src.is_file():
            shutil.copy2(src, runtime / "assets" / "editor" / n)
        else:
            problems.append("editor asset missing: assets/editor/" + n)
    (runtime / "ds1edit.ini").write_text(render_ini(ctx.d2_dir, ctx.pd2_dir), encoding="ascii")
    return problems


def _render_once(ctx, runtime, entry, ids, out_path):
    env = dict(os.environ)
    for k in ("APPDATA", "XDG_CONFIG_HOME", "HOME"):
        env[k] = str(runtime / "appdata")
    env = ds1_cases._probe_env(runtime / ctx.exe.name) | {k: env[k] for k in
                                                         ("APPDATA", "XDG_CONFIG_HOME", "HOME")}
    cmd = [runtime / ctx.exe.name, entry["extracted"], str(ids[0]), str(ids[1]),
           "--zoom=1:4", "--headless", out_path]
    return run_proc(cmd, runtime, env, timeout=300)


def run_render(ctx, chain, inv, tiles=None, sweep_ok=None):
    if tiles is None:
        tiles = S.TileInputs(_text(chain, EXCEL + "lvlprest.txt"), _text(chain, EXCEL + "levels.txt"),
                             _text(chain, EXCEL + "lvltypes.txt"), chain.pathset())
    rdir = ctx.run_dir / "render"
    runtime = rdir / "rt"
    problems = build_runtime(ctx, runtime)
    if problems:
        ctx.case("render/runtime", "unavailable", [{"kind": "runtime", "detail": p}
                                                  for p in problems])
        return
    ids = {}
    for p in inv["files"]:
        if inv["files"][p]["kind"] == "ds1":
            r = resolve_ids(tiles, p)
            if r:
                ids[p] = r
    usable = lambda p: p in ids and (sweep_ok is None or p in sweep_ok)  # noqa: E731
    sel = select_render_set(inv["files"], usable)
    if not sel:
        ctx.case("render/selection", "unavailable",
                 [{"kind": "selection", "detail": "no PD2 map has a resolvable preset"}])
        return
    ext = Extractor(chain, rdir / "ds1")
    shots, results = {}, {}
    for s in sel:
        f = inv["files"][s["path"]]
        cid = "render/" + s["role"] + "/" + f["path"].replace("\\", "/")
        fails = []
        local = ext.extract(f)
        runs = []
        for n in (1, 2):
            out = rdir / ("%s.run%d.png" % (slug(f["path"]), n))
            if out.exists():
                out.unlink()
            rc, so, se, to = _render_once(ctx, runtime, {"extracted": str(local)},
                                          ids[s["path"]], str(out))
            runs.append((rc, so, se, to, out))
        rc, so, se, to, out1 = runs[0]
        text = so + se
        outcome = classify_exit(rc, to)
        if outcome != "ok" or not out1.is_file() or out1.stat().st_size == 0:
            fails.append({"kind": "render", "detail": "renderer %s (exit %s)" % (outcome, rc),
                          "stderr": se[-400:]})
            ctx.case(cid, "unavailable" if outcome == "environment" else "mismatch", fails,
                     coverage={"role": s["role"]})
            continue
        ok, why = render_uses_pd2(text, ctx.pd2_dir)
        if not ok:
            fails.append({"kind": "vanilla_data", "detail": why})
        # the map itself is read from the extracted copy (sha-checked against the
        # archive); the tables and tiles come from the chain, so LvlPrest must
        # have been found inside the PD2 folder
        src = read_source(text, "lvlprest.txt")
        if src is None or not _dir_key(src).startswith(_dir_key(str(ctx.pd2_dir)) + "\\"):
            fails.append({"kind": "vanilla_data", "detail":
                          "LvlPrest.txt was read from %s, not from the PD2 folder" % src})
        out2 = runs[1][4]
        if not out2.is_file() or not files_identical(out1, out2):
            fails.append({"kind": "determinism", "detail": "two renders differ"})
        results[cid] = {"role": s["role"], "path": f["path"], "ids": ids[s["path"]],
                        "fails": fails, "source": src, "out": out1}
        if not fails:
            shots[cid] = out1
    verdicts = baseline_check(ctx.out_root / "baseline", ctx.identity, shots) if shots else {}
    for cid, r in results.items():
        fails = list(r["fails"])
        v = verdicts.get(cid)
        status, created = "pass", False
        cov = {"role": r["role"], "lvltype": r["ids"][0], "lvlprest_def": r["ids"][1],
               "lvlprest_read_from": r["source"], "pd2_data": not any(
                   f["kind"] == "vanilla_data" for f in fails),
               "deterministic": not any(f["kind"] == "determinism" for f in fails)}
        if fails:
            status = "mismatch"
        elif v is None:
            status = "unavailable"
        elif v["outcome"] == "created":
            status, created = "partial", True
            cov["baseline"] = "created (reviewed: false)"
        elif v["outcome"] == "match":
            cov["baseline"] = "match (reviewed: %s)" % v["reviewed"]
        elif v["outcome"] == "incomparable":
            status = "incomparable"
            cov["baseline"] = "incomparable"
            fails.append({"kind": "identity", "detail": "; ".join(v.get("reasons", []))})
        else:
            status = "mismatch"
            d = {}
            try:
                d = R.compare_rgb(Path(v["baseline_image"]), r["out"])
            except Exception as e:  # PIL absent or unreadable image
                d = {"error": str(e)}
            fails.append({"kind": "baseline_mismatch", "sha256": v["sha256"],
                          "baseline_sha256": v["baseline_sha256"], "diff": d})
        ctx.case(cid, status, fails, coverage=cov, baseline_created=created,
                 extra={"actual_sha256": v.get("sha256") if v else None,
                        "expected_sha256": v.get("baseline_sha256") if v else None})


# ---------------------------------------------------------------------------
# driver
# ---------------------------------------------------------------------------

def finish(ctx):
    cases = ctx.cases
    summary = {"schema": SUMMARY_SCHEMA, "identity": {k: ctx.identity[k] for k in
                                                       ("version", "version_source", "digest")},
               "run_dir": str(ctx.run_dir), "cases": len(cases),
               "status_counts": {}, "exit_code": exit_code(cases),
               "results": [{"id": c["id"], "status": c["status"], "failures": len(c["failures"])}
                           for c in cases]}
    for c in cases:
        summary["status_counts"][c["status"]] = summary["status_counts"].get(c["status"], 0) + 1
    (ctx.run_dir / "summary.json").write_text(json.dumps(summary, indent=1, sort_keys=True),
                                              encoding="utf-8")
    # per-map and per-render cases are many; the table shows everything that
    # did not simply pass, plus the aggregate cases
    is_map = lambda c: c["id"].startswith(CASE_PREFIX + "maps/data/")  # noqa: E731
    table_rows = [c for c in cases if not is_map(c) or c["status"] != "pass"]
    txt = summary_table(table_rows)
    passed_maps = sum(1 for c in cases if is_map(c) and c["status"] == "pass")
    if passed_maps:
        txt += "\n(+ %d per-map cases passed; see cases/)" % passed_maps
    (ctx.run_dir / "summary.txt").write_text(txt + "\n", encoding="utf-8")
    print(txt)
    print("\nPD2 identity: version=%s (%s)\n              digest=%s\nrun dir: %s"
          % (ctx.identity["version"], ctx.identity["version_source"],
             ctx.identity["digest"], ctx.run_dir))
    return summary["exit_code"]


def _need(ctx, what, attr):
    p = getattr(ctx, attr)
    if p is None or not Path(p).is_file():
        raise Pd2Error("%s not found: %s" % (what, p))


def run(part: str, ctx) -> int:
    ctx.run_dir.mkdir(parents=True, exist_ok=True)
    S.quiet_windows_errors()
    run_identity(ctx)
    inv = run_inventory(ctx)
    if part == "inventory":
        return finish(ctx)
    pd2, base = open_archives(ctx.pd2_dir, ctx.d2_dir)
    chain = Chain(pd2 + base)
    try:
        tiles, sweep_ok = None, None
        if part in ("sweep", "all"):
            for what, attr in (("ds1edit executable", "exe"),
                               ("test_ds1_reference_probe", "probe_ds1"),
                               ("test_visual_reference_probe", "probe_visual"),
                               ("test_adversarial_probe", "probe_robust")):
                _need(ctx, what, attr)
            cases, tiles, _agg = run_maps(ctx, chain, inv)
            sweep_ok = {c["ds1"]["path"] for c in cases if c["status"] == "pass"}
            run_tables(ctx, chain, inv)
            run_graphics_real(ctx, chain, inv)
        if part in ("render", "all"):
            _need(ctx, "ds1edit executable", "exe")
            run_render(ctx, chain, inv, tiles, sweep_ok)
    finally:
        chain.close()
    return finish(ctx)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("part", choices=("inventory", "sweep", "render", "all"))
    ap.add_argument("--pd2-dir", required=True, type=Path, help="the ProjectD2 folder")
    ap.add_argument("--d2-dir", required=True, type=Path,
                    help="the Diablo II folder holding the vanilla archives")
    ap.add_argument("--exe", type=Path, help="ds1edit executable")
    ap.add_argument("--probe-ds1", type=Path)
    ap.add_argument("--probe-visual", type=Path)
    ap.add_argument("--probe-robust", type=Path)
    ap.add_argument("--manifest", type=Path,
                    help="vanilla ds1edit.baseline/1 manifest (cross-reference only)")
    ap.add_argument("--out-root", type=Path, default=REPO_ROOT / "tmp" / "pd2")
    ap.add_argument("--run-dir", type=Path)
    ap.add_argument("--jobs", type=int, default=0)
    ap.add_argument("--limit", type=int, help="development aid: first N maps/files only")
    a = ap.parse_args(argv)
    run_dir = a.run_dir or (a.out_root / ("run-" + utc_stamp()))
    try:
        ctx = Ctx(pd2_dir=a.pd2_dir, d2_dir=a.d2_dir, exe=a.exe, probe_ds1=a.probe_ds1,
                  probe_visual=a.probe_visual, probe_robust=a.probe_robust,
                  out_root=a.out_root, run_dir=run_dir, jobs=a.jobs, limit=a.limit,
                  manifest=a.manifest)
        return run(a.part, ctx)
    except (Pd2Error, C.ValidationError) as e:
        print("pd2_lane: %s" % e, file=sys.stderr)
        return 2


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
