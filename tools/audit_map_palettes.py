#!/usr/bin/env python3
r"""Check each map's palette against the one the game would use.

The game colors a level with Levels.Pal and nothing else. A preset whose
LvlPrest row names a level of the tileset it is filed under gets that level's
Pal. A preset with no level row is drawn by whichever levels use its tileset,
so it is proven only when all those levels share one Pal. This sorts every map
into one of:

  level     its row's level decides
  tileset   every level of its tileset uses one Pal
  mixed     levels of its tileset disagree; the editor takes the commonest
  none      no level uses its tileset, or it has none; the game never says
  unnamed   no LvlPrest row names it, so the game never loads it and its
            tileset is the editor's guess from its folder

With --chosen, the palettes the editor picked are checked against that: a map
in "level" or "tileset" whose palette differs is a mismatch, and a "mixed" map
gets flagged when its palette is not one any level of the tileset uses.

    ds1edit list-files > files.txt
    python tools/audit_map_palettes.py files.txt C:\Diablo2\ProjectD2\pd2data.mpq C:\Diablo2\patch_d2.mpq ...

--chosen takes "path<TAB>act" lines, or JSON lines with "path" and "txt_act"
(act 1-5, as the editor's "txt_act =" line prints it). Give the archives in
search order, highest priority first.
"""

import argparse
import collections
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from d2mpq import D2Archive

EXCEL = "data\\global\\excel\\"
LISTING = re.compile(r"\s+(\S.*?)\s+LvlType=(-?\d+)\s+Def=(-?\d+)\s+\((.*)\)\s*$")
CLASSES = ("level", "tileset", "mixed", "none", "unnamed")


def read_table(archives, name):
    for archive in archives:
        data = archive.read(EXCEL + name)
        if data is not None:
            lines = data.decode("latin1").replace("\r", "").split("\n")
            header = lines[0].split("\t")
            return [dict(zip(header, line.split("\t"))) for line in lines[1:] if line.strip()]
    sys.exit("%s is in none of the archives" % name)


def number(row, column, blank=-1):
    """A numeric cell. The game reads a blank one as 0; pass blank=0 for that."""
    value = (row.get(column) or "").strip()
    if not value:
        return blank
    return int(value) if value.lstrip("-").isdigit() else -1


def norm(path):
    return path.replace("/", "\\").lower()


def read_chosen(path):
    chosen = {}
    with open(path, errors="replace") as lines:
        for line in lines:
            line = line.strip()
            if not line:
                continue
            if line.startswith("{"):
                record = json.loads(line)
                name, act = record["path"], record.get("txt_act")
            else:
                name, _, act = line.rpartition("\t")
            try:
                chosen[norm(name)] = int(act)
            except (TypeError, ValueError):
                pass
    return chosen


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("listing", help="output of ds1edit list-files")
    parser.add_argument("archives", nargs="+", help="MPQs in search order")
    parser.add_argument("--chosen", help="the editor's palette per map")
    parser.add_argument("--all", action="store_true", help="list every map, not only problems")
    args = parser.parse_args()

    archives = [D2Archive(path) for path in args.archives]

    levels = {}
    type_pals = collections.defaultdict(collections.Counter)
    type_levels = collections.defaultdict(list)
    for row in read_table(archives, "Levels.txt"):
        level_id, pal, level_type = number(row, "Id"), number(row, "Pal", 0), number(row, "LevelType")
        if level_id <= 0:
            continue
        levels[level_id] = (row.get("Name", ""), level_type, pal)
        if level_type > 0 and 0 <= pal <= 4:
            type_pals[level_type][pal + 1] += 1
            type_levels[level_type].append((row.get("Name", ""), pal + 1))

    type_names = {number(row, "Id"): row.get("Name", "") for row in read_table(archives, "LvlTypes.txt")}
    presets = {number(row, "Def"): number(row, "LevelId") for row in read_table(archives, "LvlPrest.txt")}

    chosen = read_chosen(args.chosen) if args.chosen else None

    counts = collections.Counter()
    problems = collections.defaultdict(list)
    for line in open(args.listing, errors="replace"):
        match = LISTING.match(line)
        if not match:
            continue
        path, level_type, preset, group = match[1], int(match[2]), int(match[3]), match[4]

        level = levels.get(presets.get(preset, -1))
        pals = type_pals.get(level_type, collections.Counter())
        if preset <= 0 or preset not in presets:
            kind, expected, why = "unnamed", set(), "no LvlPrest row"
        elif level is not None and level_type > 0 and level[1] == level_type and 0 <= level[2] <= 4:
            kind, expected, why = "level", {level[2] + 1}, "level %s" % level[0]
        elif len(pals) == 1:
            kind, expected, why = "tileset", set(pals), "%d level(s) of %s, all one Pal" % (
                sum(pals.values()), type_names.get(level_type, level_type))
        elif pals:
            kind, expected, why = "mixed", set(pals), "%s: %s" % (
                type_names.get(level_type, level_type),
                ", ".join("Act %d x%d" % item for item in sorted(pals.items())))
        else:
            kind, expected, why = "none", set(), "LvlType %d" % level_type
        counts[kind] += 1

        act = None if chosen is None else chosen.get(norm(path))
        if chosen is not None and act is None:
            problem = "not opened"
        elif act is not None and kind in ("level", "tileset") and act not in expected:
            problem = "mismatch"
        elif act is not None and kind == "mixed" and act not in expected:
            problem = "mismatch"
        elif kind == "mixed":
            problem = "mixed"
        elif args.all:
            problem = kind
        else:
            continue
        problems[problem].append("  %-55s %-32s editor %-4s game %-9s (%s)" % (
            path, group[:32], "-" if act is None else act,
            "/".join(str(a) for a in sorted(expected)) or "?", why))

    print("maps by proof: " + ", ".join("%s %d" % (kind, counts[kind]) for kind in CLASSES))
    for problem in ("mismatch", "not opened") + CLASSES:
        if problems[problem]:
            print("\n%s (%d)" % (problem, len(problems[problem])))
            print("\n".join(sorted(problems[problem])))

    print("\ntilesets whose levels disagree on Pal")
    for level_type in sorted(type_pals):
        if len(type_pals[level_type]) > 1:
            print("  %3d %-24s %s" % (level_type, type_names.get(level_type, ""), ", ".join(
                "%s=Act %d" % item for item in type_levels[level_type])))


if __name__ == "__main__":
    main()
