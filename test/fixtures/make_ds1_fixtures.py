#!/usr/bin/env python3
"""
Authored DS1 fixtures for the map-integrity tests (package C).

Every fixture is synthetic: small maps whose bytes are chosen to be distinct
and non-zero so a reader that mixes up layers, offsets or field order shows up
as a value mismatch. No game data is read or embedded.

For each case the generator writes ``<id>.ds1`` and records, in
``manifest.json``, the snapshot the editor's reader is expected to produce
(schema ``ds1edit.map/1``, see test/test_ds1_reference_probe.c) together with
the byte layout it encoded. The layout is checked here against an independent
size formula before anything is written, so the encoder and the expectation
cannot silently drift together.

The format rules follow docs/guides/ds1-map-format.md ("Version
differences"). The encoder shares no code with src/core/ds1.c or with the C
fixture encoder in test/support/ds1_fixture.c.

Usage:
    python test/fixtures/make_ds1_fixtures.py --out DIR
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import struct
from pathlib import Path

SCHEMA = "ds1edit.map/1"
MANIFEST_SCHEMA = "ds1edit.fixtures/1"

# Versions with a distinct layout threshold, plus every version vanilla
# 1.13c ships (3, 8, 12, 13, 15, 16, 17, 18).
VERSIONS = [2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18]

DIR_LOOKUP = [
    0x00, 0x01, 0x02, 0x01, 0x02, 0x03, 0x03, 0x05, 0x05, 0x06,
    0x06, 0x07, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x14,
]


# --------------------------------------------------------------------------
# authored specs

def default_spec(version: int) -> dict:
    """3x2 map, distinct non-zero bytes, as many layers as the version allows."""
    w, h = 3, 2
    spec = {
        "version": version,
        "width": w,
        "height": h,
        "act": 2 if version >= 8 else 1,           # 1-based
        "tag_type": 2 if version >= 10 else 0,
        "files": ["gen\\one.dt1", "gen\\two.dt1"] if version >= 3 else [],
        "skip": [0x33333333, 0x44444444],           # the v9..13 pair
        "wall_num": 2 if version >= 4 else 1,
        "floor_num": 2 if version >= 16 else 1,
        "walls": [], "orient": [], "floors": [], "shadow": [], "tags": [],
        "objects": [],
        "pre_group": 0x0BADF00D,
        "groups": [],
        "npcs": [],
    }
    for layer in range(4):
        spec["walls"].append([[[(0x05 + layer * 0x20 + (y * w + x) * 4 + k) & 0xFF
                                for k in range(4)] for x in range(w)] for y in range(h)])
        if version < 7:
            spec["orient"].append([[(y * w + x + layer * 5 + 2) % 25
                                    for x in range(w)] for y in range(h)])
        else:
            spec["orient"].append([[0x02 + layer * 0x09 + y * w + x
                                    for x in range(w)] for y in range(h)])
    for layer in range(2):
        spec["floors"].append([[[(0x85 + layer * 0x20 + (y * w + x) * 4 + k) & 0xFF
                                 for k in range(4)] for x in range(w)] for y in range(h)])
    spec["shadow"] = [[[(0xC5 + (y * w + x) * 4 + k) & 0xFF for k in range(4)]
                       for x in range(w)] for y in range(h)]
    spec["tags"] = [[0xB0000000 | (y * w + x + 7) for x in range(w)] for y in range(h)]
    if version >= 2:
        spec["objects"] = [
            {"type": 1, "id": 4, "x": 6, "y": 2, "flags": 0x20 if version >= 6 else 0},
            {"type": 2, "id": 11, "x": 13, "y": 8, "flags": 0},
        ]
    if has_groups(spec):
        spec["groups"] = [
            {"tile_x": 1, "tile_y": 0, "width": 2, "height": 2,
             "unk": 0x77 if version >= 13 else 0},
            {"tile_x": 0, "tile_y": 1, "width": 1, "height": 1,
             "unk": 3 if version >= 13 else 0},
        ]
    if version >= 14:
        spec["npcs"] = [
            {"x": 13, "y": 8, "points": [[12, 7, 2], [14, 9, 5], [13, 6, 1]]},
        ]
    return spec


def wall_layers(spec: dict) -> int:
    return spec["wall_num"] if spec["version"] >= 4 else 1


def floor_layers(spec: dict) -> int:
    return spec["floor_num"] if spec["version"] >= 16 else 1


def has_tag_layer(spec: dict) -> bool:
    if spec["version"] < 4:
        return True
    return spec["version"] >= 10 and spec["tag_type"] in (1, 2)


def has_groups(spec: dict) -> bool:
    return spec["version"] >= 12 and spec["tag_type"] in (1, 2)


# --------------------------------------------------------------------------
# encoder

def encode(spec: dict) -> tuple[bytes, dict]:
    v = spec["version"]
    w, h = spec["width"], spec["height"]
    out = bytearray()
    layout = {}

    def i32(value: int) -> None:
        out.extend(struct.pack("<I", value & 0xFFFFFFFF))

    def cells4(grid) -> None:
        for y in range(h):
            for x in range(w):
                out.extend(bytes(grid[y][x]))

    def orient(layer: int) -> None:
        for y in range(h):
            for x in range(w):
                out.extend(bytes([spec["orient"][layer][y][x], 0xDD, 0xDD, 0xDD]))

    def tags() -> None:
        for y in range(h):
            for x in range(w):
                i32(spec["tags"][y][x])

    i32(v)
    i32(w - 1)
    i32(h - 1)
    if v >= 8:
        i32(spec["act"] - 1)
    if v >= 10:
        i32(spec["tag_type"])
    layout["files_at"] = len(out)
    if v >= 3:
        i32(len(spec["files"]))
        for name in spec["files"]:
            out.extend(name.encode("ascii") + b"\0")
    if 9 <= v <= 13:
        i32(spec["skip"][0])
        i32(spec["skip"][1])
    if v >= 4:
        i32(spec["wall_num"])
        if v >= 16:
            i32(spec["floor_num"])
    layout["layers_at"] = len(out)
    if v < 4:
        cells4(spec["walls"][0])
        cells4(spec["floors"][0])
        orient(0)
        tags()
        cells4(spec["shadow"])
    else:
        for layer in range(spec["wall_num"]):
            cells4(spec["walls"][layer])
            orient(layer)
        for layer in range(floor_layers(spec)):
            cells4(spec["floors"][layer])
        cells4(spec["shadow"])
        if has_tag_layer(spec):
            tags()
    layout["objects_at"] = len(out)
    if v >= 2:
        i32(len(spec["objects"]))
        for o in spec["objects"]:
            i32(o["type"]); i32(o["id"]); i32(o["x"]); i32(o["y"])
            if v >= 6:
                i32(o["flags"])
    layout["groups_at"] = len(out)
    if has_groups(spec):
        if v >= 18:
            i32(spec["pre_group"])
        i32(len(spec["groups"]))
        for g in spec["groups"]:
            i32(g["tile_x"]); i32(g["tile_y"]); i32(g["width"]); i32(g["height"])
            if v >= 13:
                i32(g["unk"])
    layout["paths_at"] = len(out)
    if v >= 14:
        i32(len(spec["npcs"]))
        for n in spec["npcs"]:
            i32(len(n["points"])); i32(n["x"]); i32(n["y"])
            for p in n["points"]:
                i32(p[0]); i32(p[1])
                if v >= 15:
                    i32(p[2])
    layout["end"] = len(out)
    return bytes(out), layout


def expected_layout(spec: dict) -> dict:
    """Section offsets from a closed-form size formula (independent of encode)."""
    v = spec["version"]
    cells = spec["width"] * spec["height"]
    header = 12 + (4 if v >= 8 else 0) + (4 if v >= 10 else 0)
    files = (4 + sum(len(n) + 1 for n in spec["files"])) if v >= 3 else 0
    skip = 8 if 9 <= v <= 13 else 0
    counts = (4 if v >= 4 else 0) + (4 if v >= 16 else 0)
    if v < 4:
        layers = 5
    else:
        layers = 2 * spec["wall_num"] + floor_layers(spec) + 1 + (1 if has_tag_layer(spec) else 0)
    obj_rec = 20 if v >= 6 else 16
    objects = (4 + obj_rec * len(spec["objects"])) if v >= 2 else 0
    grp_rec = 20 if v >= 13 else 16
    groups = ((4 if v >= 18 else 0) + 4 + grp_rec * len(spec["groups"])) if has_groups(spec) else 0
    pt = 12 if v >= 15 else 8
    paths = (4 + sum(12 + pt * len(n["points"]) for n in spec["npcs"])) if v >= 14 else 0
    lay = {"files_at": header}
    lay["layers_at"] = header + files + skip + counts
    lay["objects_at"] = lay["layers_at"] + layers * cells * 4
    lay["groups_at"] = lay["objects_at"] + objects
    lay["paths_at"] = lay["groups_at"] + groups
    lay["end"] = lay["paths_at"] + paths
    return lay


def check_layout(spec: dict, data: bytes, layout: dict) -> None:
    want = expected_layout(spec)
    if layout != want:
        raise AssertionError(f"encoder layout {layout} != formula {want}")
    if len(data) != layout["end"]:
        raise AssertionError(f"encoded {len(data)} bytes, layout says {layout['end']}")
    version, = struct.unpack_from("<i", data, 0)
    if version != spec["version"]:
        raise AssertionError("version dword mismatch")


# --------------------------------------------------------------------------
# what the editor's reader should make of a spec (ds1edit.map/1)

def expected_snapshot(spec: dict, missing_tail: dict | None = None) -> dict:
    v = spec["version"]
    w, h = spec["width"], spec["height"]
    walls, floors = wall_layers(spec), floor_layers(spec)
    tag = has_tag_layer(spec)
    act = spec["act"] if v >= 8 else 1
    act = min(act, 5)
    cells = []
    for y in range(h):
        for x in range(w):
            cell = {"x": x, "y": y, "walls": [], "floors": [], "shadows": [], "tags": []}
            for layer in range(walls):
                raw = spec["orient"][layer][y][x]
                o = DIR_LOOKUP[raw] if v < 7 else raw
                cell["walls"].append(list(spec["walls"][layer][y][x]) + [o])
            for layer in range(floors):
                cell["floors"].append(list(spec["floors"][layer][y][x]))
            cell["shadows"].append(list(spec["shadow"][y][x]))
            if tag:
                cell["tags"].append(spec["tags"][y][x])
            cells.append(cell)
    objects = [{"type": o["type"], "id": o["id"], "x": o["x"], "y": o["y"],
                "flags": o["flags"] if v >= 6 else 0} for o in spec["objects"]]
    groups = []
    if has_groups(spec):
        groups = [{"tile_x": g["tile_x"], "tile_y": g["tile_y"], "width": g["width"],
                   "height": g["height"], "unk": g["unk"] if v >= 13 else 0}
                  for g in spec["groups"]]
    paths = []
    if v >= 14:
        for i, o in enumerate(spec["objects"]):
            owners = [n for n in spec["npcs"] if n["x"] == o["x"] and n["y"] == o["y"]]
            same_spot = [p for p in spec["objects"] if p["x"] == o["x"] and p["y"] == o["y"]]
            if owners and len(same_spot) == 1:
                n = owners[-1]
                paths.append({"object": i, "x": o["x"], "y": o["y"],
                              "points": [[p[0], p[1], p[2] if v >= 15 else 1]
                                         for p in n["points"]]})
    snap = {
        "schema": SCHEMA,
        "status": "ok",
        "version": v,
        "width": w,
        "height": h,
        "act": act,
        "tag_type": spec["tag_type"] if v >= 10 else 0,
        "layers": {"walls": walls, "floors": floors, "shadows": 1, "tags": 1 if tag else 0},
        "files": list(spec["files"]) if v >= 3 else [],
        "cells": cells,
        "objects": objects,
        "groups": groups,
        "paths": paths,
    }
    if missing_tail:
        for key, value in missing_tail.items():
            snap[key] = value
    return snap


# --------------------------------------------------------------------------
# the case matrix

def build_cases() -> list[dict]:
    cases = []
    for v in VERSIONS:
        spec = default_spec(v)
        cases.append({"id": f"v{v:02d}_default", "spec": spec, "truncate": None,
                      "append": 0, "expected": expected_snapshot(spec)})

    spec = default_spec(18)
    spec["tag_type"] = 0
    spec["groups"] = []
    cases.append({"id": "v18_tag_type_0", "spec": spec, "truncate": None, "append": 0,
                  "expected": expected_snapshot(spec)})

    spec = default_spec(18)
    spec["wall_num"] = 4
    cases.append({"id": "v18_four_walls", "spec": spec, "truncate": None, "append": 0,
                  "expected": expected_snapshot(spec)})

    spec = default_spec(18)
    spec["objects"] = []
    spec["npcs"] = []
    cases.append({"id": "v18_zero_objects", "spec": spec, "truncate": None, "append": 0,
                  "expected": expected_snapshot(spec)})

    spec = default_spec(6)
    spec["width"], spec["height"] = 5, 5
    spec["objects"] = []
    for layer in range(4):
        spec["walls"][layer] = [[[0x10 + layer, y, x, 0x40] for x in range(5)] for y in range(5)]
        spec["orient"][layer] = [[y * 5 + x for x in range(5)] for y in range(5)]
    spec["floors"] = [[[[0x90, y, x, 1] for x in range(5)] for y in range(5)] for _ in range(2)]
    spec["shadow"] = [[[0xD0, y, x, 2] for x in range(5)] for y in range(5)]
    cases.append({"id": "v06_all_legacy_orientations", "spec": spec, "truncate": None,
                  "append": 0, "expected": expected_snapshot(spec)})

    # tolerated tails (docs/guides/ds1-map-format.md)
    spec = default_spec(13)
    _, lay = encode(spec)
    cut = lay["groups_at"] + 4 + 20 + 4          # trees.ds1: stops after the last tile_x
    exp = expected_snapshot(spec)
    exp["groups"][1].update({"tile_y": 0, "width": 0, "height": 0, "unk": 0})
    cases.append({"id": "v13_group_block_cut_after_tile_x", "spec": spec, "truncate": cut,
                  "append": 0, "expected": exp})

    spec = default_spec(18)
    _, lay = encode(spec)
    exp = expected_snapshot(spec)
    exp["paths"] = []
    cases.append({"id": "v18_missing_npc_count", "spec": spec, "truncate": lay["paths_at"],
                  "append": 0, "expected": exp})

    spec = default_spec(13)
    cases.append({"id": "v13_trailing_48_zero_bytes", "spec": spec, "truncate": None,
                  "append": 48, "expected": expected_snapshot(spec)})

    # malformed inputs the reader must reject
    spec = default_spec(18)
    _, lay = encode(spec)
    cases.append({"id": "bad_header_8_bytes", "spec": spec, "truncate": 8, "append": 0,
                  "expected": {"schema": SCHEMA, "status": "error"}})
    cases.append({"id": "bad_tile_layers_cut", "spec": spec,
                  "truncate": lay["layers_at"] + 30, "append": 0,
                  "expected": {"schema": SCHEMA, "status": "error"}})
    spec = default_spec(18)
    spec["wall_num"] = 5
    spec["walls"].append(spec["walls"][0])
    spec["orient"].append(spec["orient"][0])
    cases.append({"id": "bad_five_wall_layers", "spec": spec, "truncate": None, "append": 0,
                  "expected": {"schema": SCHEMA, "status": "error"}})
    return cases


def case_bytes(case: dict) -> bytes:
    data, layout = encode(case["spec"])
    check_layout(case["spec"], data, layout)
    if case["truncate"] is not None:
        data = data[:case["truncate"]]
    if case["append"]:
        data = data + bytes(case["append"])
    return data


def generate(out_dir: Path) -> dict:
    out_dir.mkdir(parents=True, exist_ok=True)
    manifest = {"schema": MANIFEST_SCHEMA, "map_schema": SCHEMA, "cases": []}
    for case in build_cases():
        data = case_bytes(case)
        name = f"{case['id']}.ds1"
        (out_dir / name).write_bytes(data)
        _, layout = encode(case["spec"])
        manifest["cases"].append({
            "id": case["id"],
            "file": name,
            "version": case["spec"]["version"],
            "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
            "layout": layout,
            "expected": copy.deepcopy(case["expected"]),
        })
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    return manifest


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--out", required=True, type=Path, help="output directory")
    args = ap.parse_args(argv)
    manifest = generate(args.out)
    print(f"wrote {len(manifest['cases'])} fixtures to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
