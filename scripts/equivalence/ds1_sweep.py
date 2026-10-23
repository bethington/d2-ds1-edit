"""Catalogued vanilla DS1 sweep: every DS1 in a ``ds1edit.baseline/1`` corpus
manifest through the editor's real reader and checked writer.

For each manifest DS1, exactly once:

1. extract its effective bytes (highest-priority archive, as the manifest
   says) into an owned scratch tree, lazily, and check the manifest SHA-256;
2. ``test_ds1_reference_probe dump`` -- the production reader
   (``ds1_read_checked``) -- and record version, dimensions, act, layer
   counts, objects/groups/paths and any rejection;
3. cross-check the header dwords independently (version, width+1, height+1,
   act+1 from version 8) against the reader's result;
4. ``resave`` through the checked writer, read the result back and compare
   canonical snapshots under the writer's *declared* transformations only
   (``writer_v18_upgrade``, ``source_ignored``, and ``legacy_tag_layer_dropped``
   for pre-v4 sources whose tag layer the writer does not emit);
5. resolve the DT1 inputs the editor would need (LvlPrest ``File1..6`` ->
   ``Def``/``Dt1Mask``/``LevelId`` -> Levels ``LevelType`` -> LvlTypes
   ``File 1..32``) against the manifest and classify every key.

Results are ``ds1edit.case/1`` objects, one per DS1, plus an aggregate. This
lane compares the editor with itself and with the file's own header -- it is
*not* an original-game comparison and never claims to be one (see
``ds1_reference.py`` for that).

CLI::

    python -m scripts.equivalence.ds1_sweep --manifest M --game-dir G \\
        --probe EXE --out-dir tmp/equivalence/ds1-sweep [--jobs N] [--limit N]

Exit 0 when every DS1 was read and round-tripped, 1 when any mismatch or
rejection was found, 2 when something could not be run.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

if __package__ in (None, ""):  # pragma: no cover
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from scripts.equivalence import contracts as C
    from scripts.equivalence import ds1_cases
else:
    from . import contracts as C
    from . import ds1_cases

LANE = "local-corpus"
CASE_PREFIX = "ds1-sweep/"
TILES_DIR = "data\\global\\tiles\\"
EXCEL_DIR = "data\\global\\excel\\"
SWEEP_LIMITATIONS = [
    "editor-only lane: reader, header cross-check and writer round trip; no "
    "original-game loader output is compared here",
    "a round trip shows the writer preserves what the reader produced; it "
    "cannot show the reader interpreted the file as the game does",
]

#: Windows NTSTATUS for a missing DLL: an environment fault, never a rejection.
STATUS_DLL_NOT_FOUND = 0xC0000135


def quiet_windows_errors() -> None:
    """Make loader errors/crashes of child probes come back as exit codes
    instead of blocking modal dialogs (inherited by children)."""
    if os.name == "nt":
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)


def is_environment_failure(rc) -> bool:
    return rc is not None and (rc & 0xFFFFFFFF) == STATUS_DLL_NOT_FOUND


# Normalizations the writer declares (scripts/equivalence/ds1_cases.py).
BASE_POLICY = ["writer_v18_upgrade", "source_ignored"]


# ---------------------------------------------------------------------------
# Header cross-check (independent of the C reader)
# ---------------------------------------------------------------------------

def header_fields(data: bytes) -> dict | None:
    """The fixed header dwords, decoded without the editor's reader."""
    if len(data) < 12:
        return None
    version, w1, h1 = struct.unpack_from("<iii", data, 0)
    out = {"version": version, "width": w1 + 1, "height": h1 + 1}
    if version >= 8:
        if len(data) < 16:
            return None
        out["act"] = struct.unpack_from("<i", data, 12)[0] + 1
    else:
        out["act"] = 1
    return out


def header_mismatches(header: dict | None, snap: dict) -> list[str]:
    if header is None:
        return ["header: file shorter than the fixed header"]
    out = []
    for k in ("version", "width", "height"):
        if header[k] != snap.get(k):
            out.append("%s: header %r != reader %r" % (k, header[k], snap.get(k)))
    # the reader clamps act to 1..5; report a raw value outside that only
    if header["act"] != snap.get("act") and 1 <= header["act"] <= 5:
        out.append("act: header %r != reader %r" % (header["act"], snap.get("act")))
    return out


def features(snap: dict) -> dict:
    """Coarse feature flags of a parsed map (for coverage accounting)."""
    cells = snap.get("cells", [])
    layers = snap.get("layers", {})
    return {
        "version": snap.get("version"),
        "width": snap.get("width"),
        "height": snap.get("height"),
        "act": snap.get("act"),
        "tag_type": snap.get("tag_type"),
        "walls": layers.get("walls"),
        "floors": layers.get("floors"),
        "shadows": layers.get("shadows"),
        "tags": layers.get("tags"),
        "files": len(snap.get("files", [])),
        "objects": len(snap.get("objects", [])),
        "groups": len(snap.get("groups", [])),
        "paths": len(snap.get("paths", [])),
        "path_points": sum(len(p.get("points", [])) for p in snap.get("paths", [])),
        "nonzero_tags": sum(1 for c in cells for t in c.get("tags", []) if t),
        "objects_with_flags": sum(1 for o in snap.get("objects", []) if o.get("flags")),
    }


def version_family(v) -> str:
    """Format families by the thresholds that change the layout."""
    if not isinstance(v, int):
        return "unknown"
    if v < 3:
        return "v1-2"
    if v < 4:
        return "v3"
    if v < 8:
        return "v4-7"
    if v < 9:
        return "v8"
    if v < 10:
        return "v9"
    if v < 13:
        return "v10-12"
    if v < 15:
        return "v13-14"
    if v < 16:
        return "v15"
    return "v16+" if v < 18 else "v18"


def resave_policy(source_snap: dict) -> list[str]:
    pol = list(BASE_POLICY)
    if isinstance(source_snap.get("version"), int) and source_snap["version"] < 4:
        pol.append("legacy_tag_layer_dropped")
    return pol


# ---------------------------------------------------------------------------
# Required tile inputs
# ---------------------------------------------------------------------------

def _rows(text: str) -> tuple[list[str], list[list[str]]]:
    lines = [l for l in text.replace("\r\n", "\n").split("\n") if l]
    head = lines[0].split("\t")
    return head, [l.split("\t") for l in lines[1:]]


def _int(s: str) -> int:
    try:
        return int(s.strip())
    except (ValueError, AttributeError):
        return 0


class TileInputs:
    """Resolve a DS1's required DT1 inputs from the corpus's own tables."""

    def __init__(self, lvlprest: str, levels: str, lvltypes: str, manifest_paths: set[str]):
        self.paths = manifest_paths
        head, rows = _rows(lvlprest)
        ix = {h: i for i, h in enumerate(head)}
        self.prest = {}  # ds1 virtual path -> [(def, levelid, mask, row)]
        for r_i, r in enumerate(rows):
            g = lambda k: r[ix[k]] if ix[k] < len(r) else ""  # noqa: E731
            d = _int(g("Def"))
            for f in range(1, 7):
                v = g("File%d" % f).strip()
                if v and v != "0":
                    key = C.normalize_virtual_path(TILES_DIR + v)
                    self.prest.setdefault(key, []).append(
                        (d, _int(g("LevelId")), _int(g("Dt1Mask")), r_i + 2))
        head, rows = _rows(levels)
        ix = {h: i for i, h in enumerate(head)}
        self.level_type = {_int(r[ix["Id"]]): _int(r[ix["LevelType"]])
                           for r in rows if len(r) > ix["LevelType"] and r[ix["Id"]].strip()}
        head, rows = _rows(lvltypes)
        ix = {h: i for i, h in enumerate(head)}
        self.types = {}
        for r in rows:
            if len(r) <= ix["Id"]:
                continue
            files = []
            for s in range(1, 33):
                col = ix.get("File %d" % s)
                v = r[col].strip() if col is not None and col < len(r) else ""
                files.append(None if (not v or v == "0") else
                             C.normalize_virtual_path(TILES_DIR + v))
            act = _int(r[ix["Act"]]) if "Act" in ix and ix["Act"] < len(r) else 0
            self.types[_int(r[ix["Id"]])] = {"act": act, "files": files}

    def resolve(self, ds1_path: str, act: int | None) -> dict:
        """Required inputs and the classification of every key."""
        refs = self.prest.get(ds1_path, [])
        out = {"lvlprest_rows": [{"def": d, "level_id": l, "dt1_mask": m, "row": row}
                                 for d, l, m, row in refs],
               "keys": []}
        if not refs:
            out["keys"].append({"key": ds1_path, "class": "no_lvlprest_reference"})
            return out
        seen = set()
        for d, lid, mask, _row in refs:
            if lid > 0:
                lt = self.level_type.get(lid)
                if lt is None or lt not in self.types:
                    out["keys"].append({"key": "levels:%d" % lid, "class": "level_type_unresolved"})
                    continue
                types = [(lt, "authoritative")]
            else:
                types = [(t, "context_candidate") for t, v in sorted(self.types.items())
                         if t > 0 and act is not None and v["act"] == act]
                out["keys"].append({"key": "lvlprest:def=%d" % d,
                                    "class": "level_type_context_dependent",
                                    "candidates": [t for t, _ in types]})
            for lt, how in types:
                for slot, f in enumerate(self.types[lt]["files"]):
                    if f is None or not (mask & (1 << slot)):
                        continue
                    if (f, how) in seen:
                        continue
                    seen.add((f, how))
                    out["keys"].append({"key": f, "class": ("resolved" if f in self.paths
                                                            else "dt1_missing_from_corpus"),
                                        "via": how, "lvltype": lt})
        return out


# ---------------------------------------------------------------------------
# Extraction
# ---------------------------------------------------------------------------

def sha256_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def extract_assets(manifest: dict, game_dir: Path, corpus_root: Path, wanted,
                   reader_factory=None) -> dict:
    """Extract ``wanted`` manifest assets under ``corpus_root`` if missing or
    stale. Returns ``{virtual path: (Path or None, error or None)}``."""
    from . import baseline as B
    reader_factory = reader_factory or B.default_reader_factory
    by_archive: dict[str, list[dict]] = {}
    for a in wanted:
        by_archive.setdefault(a["archive"], []).append(a)
    result = {}
    for archive, assets in by_archive.items():
        reader = None
        try:
            for a in assets:
                dest = B.safe_scratch_path(corpus_root, a["path"])
                if dest.is_file() and sha256_bytes(dest.read_bytes()) == a["sha256"]:
                    result[a["path"]] = (dest, None)
                    continue
                if reader is None:
                    apath = B._find_case_insensitive(Path(game_dir), archive)
                    if apath is None:
                        raise B.BaselineError("archive %s not found" % archive)
                    reader = reader_factory(apath)
                data = reader.read(a.get("original_path") or a["path"])
                if data is None:
                    result[a["path"]] = (None, "absent from %s" % archive)
                    continue
                if sha256_bytes(data) != a["sha256"]:
                    result[a["path"]] = (None, "sha256 differs from the manifest")
                    continue
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(data)
                result[a["path"]] = (dest, None)
        except Exception as e:  # an unreadable archive leaves its assets unavailable
            for a in assets:
                result.setdefault(a["path"], (None, "extraction failed: %s" % e))
        finally:
            if reader is not None:
                B._close(reader)
    return result


# ---------------------------------------------------------------------------
# One case
# ---------------------------------------------------------------------------

def case_id(virtual_path: str) -> str:
    return CASE_PREFIX + virtual_path.replace("\\", "/")


def _slug(virtual_path: str) -> str:
    return re.sub(r"[^a-z0-9._-]+", "_", virtual_path.lower())


def sweep_one(asset: dict, local: Path | None, extract_error: str | None, probe_exe: Path,
              work_dir: Path, tiles: TileInputs | None, baseline_digest: str,
              provenance: dict) -> dict:
    """Build the ``ds1edit.case/1`` result of one DS1."""
    vp = asset["path"]
    case = {
        "schema": C.CASE_SCHEMA, "id": case_id(vp), "lane": LANE,
        "platform": provenance.get("platform", ""), "required": True,
        "status": "not_run", "success_outcome": "pass",
        "contract_digest": None, "baseline_digest": baseline_digest,
        "input_sha256": asset["sha256"], "expected_sha256": None, "actual_sha256": None,
        "coverage": {}, "failures": [],
        "provenance": {"candidate": provenance.get("candidate", {}),
                       "reference": {"kind": "editor-self-round-trip+header",
                                     "archive": asset["archive"]}},
        "limitations": list(SWEEP_LIMITATIONS),
        "ds1": {"path": vp, "archive": asset["archive"], "size": asset["size"]},
    }
    if local is None:
        case["status"] = "unavailable"
        case["failures"].append({"kind": "extraction", "detail": extract_error})
        return case

    data = local.read_bytes()
    header = header_fields(data)
    wd = work_dir / _slug(vp)
    wd.mkdir(parents=True, exist_ok=True)
    try:
        rc, snap = ds1_cases.run_probe(probe_exe, local, wd / "read.json")
    except Exception as e:  # timeout / crash of the probe itself
        case["status"] = "unavailable"
        case["failures"].append({"kind": "probe", "detail": "dump: %s" % e})
        return case
    if is_environment_failure(rc):
        case["status"] = "unavailable"
        case["failures"].append({"kind": "probe", "detail":
                                 "probe could not start (STATUS_DLL_NOT_FOUND)"})
        return case
    if snap is None or rc not in (0, 2):
        case["status"] = "unavailable"
        case["failures"].append({"kind": "probe", "detail": "dump exit %s, no snapshot" % rc})
        return case
    if snap.get("status") == "error":
        case["status"] = "mismatch"
        case["read"] = {"status": "rejected", "error": snap.get("error"), "header": header}
        case["failures"].append({"kind": "rejected", "path": "read",
                                 "expected": "accepted (shipped vanilla map)",
                                 "actual": snap.get("error")})
        return case

    feats = features(snap)
    case["read"] = {"status": "ok", "features": feats, "family": version_family(snap["version"]),
                    "header": header}
    case["expected_sha256"] = C.payload_sha256(
        ds1_cases.canonical_map(snap, {"normalizations": ["source_ignored"]}))
    for m in header_mismatches(header, snap):
        case["failures"].append({"kind": "header", "path": m.split(":")[0], "detail": m})

    if tiles is not None:
        req = tiles.resolve(vp, snap.get("act"))
        case["required_inputs"] = req

    # writer round trip
    saved = wd / "resaved.ds1"
    for p in (saved, saved.with_suffix(".ds1.bak"), wd / "resaved.json"):
        if p.exists():
            p.unlink()
    try:
        rrc = ds1_cases.run_resave(probe_exe, local, saved)
        if is_environment_failure(rrc):
            raise RuntimeError("probe could not start (STATUS_DLL_NOT_FOUND)")
        if rrc != 0 or not saved.is_file():
            case["failures"].append({"kind": "resave", "path": "resave",
                                     "expected": 0, "actual": rrc})
        else:
            rc2, snap2 = ds1_cases.run_probe(probe_exe, saved, wd / "resaved.json")
            if snap2 is None or snap2.get("status") != "ok":
                case["failures"].append({"kind": "reread", "path": "reread",
                                         "expected": "ok",
                                         "actual": (snap2 or {}).get("error", rc2)})
            else:
                pol = {"normalizations": resave_policy(snap)}
                res = ds1_cases.compare_maps(snap, snap2, pol)
                case["round_trip"] = {"policy": pol["normalizations"],
                                      "status": res["status"],
                                      "saved_version": snap2.get("version"),
                                      "differences": res["differences"][:32],
                                      "differences_total": len(res["differences"])}
                case["actual_sha256"] = C.payload_sha256(ds1_cases.canonical_map(snap2, pol))
                for d in res["differences"][:16]:
                    case["failures"].append({"kind": "round_trip", "path": d.split(":")[0],
                                             "detail": d})
    except Exception as e:
        case["failures"].append({"kind": "probe", "detail": "resave: %s" % e})

    case["coverage"] = {"reader": "exercised", "writer_round_trip":
                        case.get("round_trip", {}).get("status", "not_run"),
                        "original_loader": "not_compared"}
    if any(f["kind"] == "probe" for f in case["failures"]):
        case["status"] = "unavailable"
    elif case["failures"]:
        case["status"] = "mismatch"
    else:
        case["status"] = "pass"
    return case


# ---------------------------------------------------------------------------
# Aggregate
# ---------------------------------------------------------------------------

def aggregate(cases: list[dict], manifest: dict) -> dict:
    ds1_assets = [a for a in manifest["assets"] if a.get("kind") == "ds1"]
    ids = [c["id"] for c in cases]
    expected_ids = sorted(case_id(a["path"]) for a in ds1_assets)
    by_status: dict[str, int] = {}
    fam: dict[str, int] = {}
    versions: dict[str, int] = {}
    key_classes: dict[str, int] = {}
    unresolved: list[dict] = []
    rejected, rt_mismatch, header_mm = [], [], []
    feat_tot = {"objects": 0, "groups": 0, "paths": 0, "with_objects": 0, "with_groups": 0,
                "with_paths": 0, "with_shadows": 0, "with_tags": 0, "with_object_flags": 0,
                "max_walls": 0, "max_floors": 0}
    for c in cases:
        by_status[c["status"]] = by_status.get(c["status"], 0) + 1
        r = c.get("read", {})
        if r.get("status") == "rejected":
            rejected.append({"id": c["id"], "error": r.get("error")})
        f = r.get("features")
        if f:
            fam[r["family"]] = fam.get(r["family"], 0) + 1
            versions[str(f["version"])] = versions.get(str(f["version"]), 0) + 1
            for k in ("objects", "groups", "paths"):
                feat_tot[k] += f[k]
                feat_tot["with_" + k] += 1 if f[k] else 0
            feat_tot["with_shadows"] += 1 if f["shadows"] else 0
            feat_tot["with_tags"] += 1 if f["nonzero_tags"] else 0
            feat_tot["with_object_flags"] += 1 if f["objects_with_flags"] else 0
            feat_tot["max_walls"] = max(feat_tot["max_walls"], f["walls"] or 0)
            feat_tot["max_floors"] = max(feat_tot["max_floors"], f["floors"] or 0)
        rt = c.get("round_trip")
        if rt and rt["status"] != "pass":
            rt_mismatch.append({"id": c["id"], "differences": rt["differences"][:4]})
        if any(x["kind"] == "header" for x in c["failures"]):
            header_mm.append({"id": c["id"], "failures": [x for x in c["failures"]
                                                          if x["kind"] == "header"]})
        for k in c.get("required_inputs", {}).get("keys", []):
            key_classes[k["class"]] = key_classes.get(k["class"], 0) + 1
            if k["class"] in ("dt1_missing_from_corpus", "level_type_unresolved"):
                unresolved.append({"id": c["id"], **k})
    return {
        "schema": "ds1edit.ds1-sweep/1",
        "baseline_digest": manifest.get("digest"),
        "enumeration": manifest.get("enumeration"),
        "accounting": {
            "manifest_ds1": len(ds1_assets),
            "cases": len(cases),
            "distinct_case_ids": len(set(ids)),
            "each_input_once": sorted(ids) == expected_ids,
            "missing": sorted(set(expected_ids) - set(ids))[:50],
            "duplicates": sorted({i for i in ids if ids.count(i) > 1})[:50]
            if len(ids) != len(set(ids)) else [],
        },
        "status_counts": by_status,
        "read_ok": sum(1 for c in cases if c.get("read", {}).get("status") == "ok"),
        "rejected": rejected,
        "round_trip_mismatches": rt_mismatch,
        "header_mismatches": header_mm,
        "version_families": dict(sorted(fam.items())),
        "versions": dict(sorted(versions.items(), key=lambda kv: int(kv[0]))),
        "feature_totals": feat_tot,
        "required_input_key_classes": key_classes,
        "unresolved_keys": unresolved[:200],
        "unresolved_keys_total": len(unresolved),
        "limitations": list(SWEEP_LIMITATIONS) + list(
            (manifest.get("enumeration") or {}).get("limits", [])),
    }


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

def run_sweep(manifest_path: Path, game_dir: Path, probe_exe: Path, out_dir: Path,
              jobs: int = 8, limit: int | None = None, reader_factory=None,
              provenance: dict | None = None) -> dict:
    quiet_windows_errors()
    manifest = C.load_json_object(manifest_path)
    if manifest.get("schema") != C.BASELINE_SCHEMA:
        raise C.ValidationError("not a %s manifest" % C.BASELINE_SCHEMA)
    out_dir = Path(out_dir).resolve()
    probe_exe = Path(probe_exe).resolve()
    corpus = out_dir / "corpus"
    work = out_dir / "work"
    cases_dir = out_dir / "cases"
    for d in (corpus, work, cases_dir):
        d.mkdir(parents=True, exist_ok=True)

    ds1s = sorted((a for a in manifest["assets"] if a.get("kind") == "ds1"),
                  key=lambda a: a["path"])
    if limit:
        ds1s = ds1s[:limit]
    tables = {EXCEL_DIR + n: None for n in ("lvlprest.txt", "levels.txt", "lvltypes.txt")}
    table_assets = [a for a in manifest["assets"] if a["path"] in tables]
    got = extract_assets(manifest, game_dir, corpus, ds1s + table_assets, reader_factory)

    tiles = None
    if all(got.get(p, (None, None))[0] is not None for p in tables):
        rd = lambda p: got[p][0].read_bytes().decode("latin-1")  # noqa: E731
        tiles = TileInputs(rd(EXCEL_DIR + "lvlprest.txt"), rd(EXCEL_DIR + "levels.txt"),
                           rd(EXCEL_DIR + "lvltypes.txt"),
                           {a["path"] for a in manifest["assets"]})

    if provenance is None:
        from . import replay
        provenance = {"platform": replay.platform_id(),
                      "candidate": replay.default_provenance(probe_exe)}

    def one(a):
        local, err = got.get(a["path"], (None, "not extracted"))
        return sweep_one(a, local, err, Path(probe_exe), work, tiles,
                         manifest.get("digest"), provenance)

    with ThreadPoolExecutor(max_workers=max(1, jobs)) as ex:
        cases = list(ex.map(one, ds1s))

    for c in cases:
        ds1_cases_path = cases_dir / (_slug(c["ds1"]["path"]) + ".json")
        with open(ds1_cases_path, "w", encoding="utf-8", newline="\n") as fh:
            json.dump(c, fh, indent=1, sort_keys=True)
    if limit:
        manifest = dict(manifest, assets=[a for a in manifest["assets"]
                                          if a.get("kind") != "ds1"] + ds1s)
    agg = aggregate(cases, manifest)
    agg["tables_resolved"] = tiles is not None
    with open(out_dir / "aggregate.json", "w", encoding="utf-8", newline="\n") as fh:
        json.dump(agg, fh, indent=2, sort_keys=True)
    return agg


def exit_code(agg: dict) -> int:
    st = agg["status_counts"]
    if not agg["accounting"]["each_input_once"] or st.get("unavailable") or st.get("not_run"):
        return 2
    return 1 if st.get("mismatch") else 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--manifest", required=True, type=Path)
    ap.add_argument("--game-dir", required=True, type=Path)
    ap.add_argument("--probe", required=True, type=Path)
    ap.add_argument("--out-dir", required=True, type=Path)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--limit", type=int)
    args = ap.parse_args(argv)
    agg = run_sweep(args.manifest, args.game_dir, args.probe, args.out_dir,
                    jobs=args.jobs, limit=args.limit)
    print(json.dumps({k: agg[k] for k in ("accounting", "status_counts", "read_ok",
                                          "versions", "required_input_key_classes",
                                          "unresolved_keys_total")}, indent=1))
    print("rejected: %d, round-trip mismatches: %d, header mismatches: %d"
          % (len(agg["rejected"]), len(agg["round_trip_mismatches"]),
             len(agg["header_mismatches"])))
    return exit_code(agg)


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
