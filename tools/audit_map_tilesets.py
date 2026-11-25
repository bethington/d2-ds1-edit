#!/usr/bin/env python3
r"""Check the map browser's filing against what each preset's tiles can draw.

The game draws a room from its level's LvlTypes row, filtered by the preset's
Dt1Mask. Most LvlPrest rows have no level, so nothing in the tables says which
row they belong to. Their tiles do: this scores each preset under its Dt1Mask
against every LvlTypes row and reports presets filed under a row that does not
fit best. It loads every DT1, so it runs here rather than in the editor.

Few-tile maps can fit a wrong row by chance, so read a report as leads to check.

    ds1edit list-files > files.txt
    python tools/audit_map_tilesets.py files.txt C:\Diablo2\ProjectD2\pd2data.mpq C:\Diablo2\patch_d2.mpq ...

Give the archives in search order, highest priority first.
"""

import argparse
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from d2mpq import D2Archive

TILES = "data\\global\\tiles\\"
EXCEL = "data\\global\\excel\\"
LISTING = re.compile(r"\s+(\S.*?)\s+LvlType=(-?\d+)\s+Def=(-?\d+)\s+\((.*)\)\s*$")
# More rows than this tie when a map has too few tiles to tell them apart.
MAX_TIES = 4
# Below this, the map does not fit the row it scored best on either.
MIN_FIT = 0.95


def read_file(archives, name):
    for archive in archives:
        data = archive.read(name)
        if data is not None:
            return data
    return None


def read_table(archives, name):
    lines = read_file(archives, EXCEL + name).decode("latin1").replace("\r", "").split("\n")
    header = lines[0].split("\t")
    return [dict(zip(header, line.split("\t"))) for line in lines[1:] if line.strip()]


def dt1_tiles(archives, path):
    """The (orientation, main, sub) of every block in a DT1."""
    data = read_file(archives, TILES + path.replace("/", "\\"))
    if data is None or len(data) < 276:
        return set()
    count, offset = struct.unpack_from("<ii", data, 268)
    return {struct.unpack_from("<iii", data, offset + i * 96 + 20) for i in range(count)}


def ds1_tiles(data):
    """The tiles a DS1's wall and floor layers use, special tiles left out."""
    pos = 0

    def i32():
        nonlocal pos
        value = struct.unpack_from("<i", data, pos)[0]
        pos += 4
        return value

    version, width, height = i32(), i32() + 1, i32() + 1
    if version >= 8:
        i32()
    if version >= 10:
        i32()
    if version >= 3:
        for _ in range(i32()):
            pos = data.index(0, pos) + 1
    if 9 <= version <= 13:
        pos += 8
    walls = floors = 1
    if version >= 4:
        walls = i32()
        if version >= 16:
            floors = i32()

    size = width * height * 4
    tiles = set()

    def add(layer, orientations):
        for cell in range(0, size, 4):
            if layer[cell]:
                orientation = orientations[cell] if orientations else 0
                main = (layer[cell + 2] >> 4) + ((layer[cell + 3] & 3) << 4)
                if not (orientation in (10, 11) and main >= 30):
                    tiles.add((orientation, main, layer[cell + 1]))

    if version < 4:
        add(data[pos:pos + size], data[pos + 2 * size:pos + 3 * size])
        add(data[pos + size:pos + 2 * size], None)
        return tiles
    for _ in range(walls):
        add(data[pos:pos + size], data[pos + size:pos + 2 * size])
        pos += 2 * size
    for _ in range(floors):
        add(data[pos:pos + size], None)
        pos += size
    return tiles


def holds(tileset, tile):
    orientation, main, sub = tile
    if tile in tileset:
        return True
    # The engine treats these two as one wall, as the editor's lookup does.
    return orientation in (18, 19) and any((o, main, s) in tileset for o in (18, 19) for s in (sub, 0))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("listing", help="output of ds1edit list-files")
    parser.add_argument("archives", nargs="+", help="MPQs in search order")
    args = parser.parse_args()

    archives = [D2Archive(path) for path in args.archives]

    filed = {}
    with open(args.listing, errors="replace") as listing:
        for line in listing:
            match = LISTING.match(line)
            if match:
                filed[match[1].replace("/", "\\").lower()] = (match[4], int(match[2]), int(match[3]))

    types = {}
    for row in read_table(archives, "LvlTypes.txt"):
        if row.get("Id", "").isdigit() and row["Id"] != "0":
            types[int(row["Id"])] = (row["Name"], [row.get("File %d" % i, "0").strip() for i in range(1, 33)])

    dt1 = {}
    for _, files in types.values():
        for path in files:
            if path not in ("", "0") and path not in dt1:
                dt1[path] = dt1_tiles(archives, path)

    checked = 0
    for row in read_table(archives, "LvlPrest.txt"):
        if not row.get("Def", "").isdigit() or row["Def"] == "0":
            continue
        mask = int(row.get("Dt1Mask") or 0) & 0xFFFFFFFF
        for path in (row.get("File%d" % i, "0").strip() for i in range(1, 7)):
            entry = filed.get(path.replace("/", "\\").lower())
            if entry is None or entry[1] <= 0 or entry[2] != int(row["Def"]):
                continue
            data = read_file(archives, TILES + path.replace("/", "\\"))
            tiles = ds1_tiles(data) if data else None
            if not tiles:
                continue

            scores = {}
            for type_id, (_, files) in types.items():
                tileset = set()
                for bit, file in enumerate(files):
                    if (mask >> bit) & 1 and file in dt1:
                        tileset |= dt1[file]
                scores[type_id] = sum(holds(tileset, tile) for tile in tiles) / len(tiles)

            best = max(scores.values())
            winners = [type_id for type_id, score in scores.items() if score == best]
            checked += 1
            if entry[1] not in winners and len(winners) <= MAX_TIES and best >= MIN_FIT:
                print("%s\n  row \"%s\" filed under %s (%.0f%%), fits %s (%.0f%%)" % (
                    path, row["Name"], entry[0], scores.get(entry[1], 0) * 100,
                    types[winners[0]][0], best * 100))

    print("%d presets checked" % checked)


if __name__ == "__main__":
    main()
