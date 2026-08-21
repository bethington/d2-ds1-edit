# Preserved documentation from paul.siramy.free.fr

A copy of the Diablo II documentation published at
<http://paul.siramy.free.fr/>, taken exactly as it stands.

Captured 21 August 2026: **775 files, 44 MB**. The pages carry dates from 2002
to 2011 — the two decades Paul Siramy spent writing and maintaining ds1edit and
explaining, patiently and in detail, how Diablo II's file formats work.

    siramy/         755 files — Paul Siramy's own work
    third-party/     20 files — Diablo II material he hosted, written by others

## Why it is here

Free.fr's personal pages are a fragile home for twenty-year-old work, and this
is the reference documentation for the tool DS1Edit continues. If the site goes
away, the tutorials that taught most of the Diablo II mapping community how DS1
files work go with it. A copy costs 44 MB.

The Markdown conversions under `docs/getting-started`, `docs/tutorials` and
`docs/guides` are derived from these pages and read more comfortably on GitHub.
**These originals are the authoritative version.** Where the two disagree,
Paul's is right.

## What is in `siramy/`

| | |
|---|---|
| `_divers/ds1/` | ds1edit: the [documentation](siramy/paul.siramy.free.fr/_divers/ds1/doc/index.html) (2007), [tutorial 1](siramy/paul.siramy.free.fr/_divers/ds1/doc/tut01/index.html) (2011), the [download page](siramy/paul.siramy.free.fr/_divers/ds1/dl_ds1edit.html) (2011), screenshots, work in progress |
| `_divers/dt1_doc/`, `_divers/dt1/` | the DT1 tile format, and his DT1 tools |
| `_divers/d2_anim/` | animation notes |
| `_divers/d2_gif_palettes/` | palette references |
| `_divers2/tut_any_units_ds1/` | [adding any monsters and objects to a DS1](siramy/paul.siramy.free.fr/_divers2/tut_any_units_ds1/index.html) (2010) |
| `_divers2/tmptutcmap/`, `_divers2/view_pl2_cmaps/` | colormap tutorial and a PL2 colormap viewer |
| `d2ref/`, `fra/d2ref/` | *D2S Unofficial Documentation* — the .d2s save file format, English and French |
| `d2_sets/` | *Les collections* — item sets, co-authored |

The D2S documentation is easy to mistake for someone else's: it credits the
Diablo II Save Game Mapping Project and reads like a community reference. It is
his. The page's own "contact the author" address is `paul.siramy@free.fr`, and
he published it in both English and French.

## What is in `third-party/`

`_divers/old_tools/` — "Some backup of old Tools Forum", a saved copy of forum
threads written by other people. Kept because it is Diablo II material worth
preserving, and kept apart because it is not Paul's writing. See
[third-party/README.md](third-party/README.md).

## How it was taken

Files are stored byte for byte under the site's own directory structure, so the
relative links between pages and their images still work when opened from disk.
Nothing has been rewritten, reformatted, corrected or modernised. Open
`siramy/paul.siramy.free.fr/_divers/ds1/doc/index.html` in a browser and you get
the page as it was served.

The live site supplied all but one file. The Wayback Machine's index supplied
something more useful than bytes: the ds1edit documentation page only links a
fraction of what Paul published, and the DT1 format reference, the DT1 tools,
the animation notes, the colormap tutorial and the D2S documentation were all
found through it rather than by following links.

`MANIFEST.tsv` lists every file with its size, SHA-256, source URL, and whether
it came from the live site or the archive, so this copy can be checked against
the original for as long as the original exists.

Off-site links inside the pages are left as they were. They point at the live
web, which is correct for an archive: this records what the pages said, not
what still answers.

## What was not preserved

**The `.zip` and `.rar` downloads.** The tutorials link to example files
(`house.zip`, `tent.zip` and others) that contain Diablo II data — `.ds1` files
and excerpts of Blizzard's `.txt` tables. This project does not redistribute
game data, which is the same reason `docs/04-Examples` was removed from the
repository. The pages still name them; the files remain on Paul's site.

**His unrelated projects.** The site also hosts work with nothing to do with
Diablo II — an online game, avatars, other tools. Out of scope here.

## Attribution and terms

Everything under `siramy/` is Paul Siramy's. Neither his site nor the ds1edit
source archive carries a licence text or a copyright notice; he simply
published the editor and its source next to each other, from 2002 onward, and
answered questions about it for years. This copy is kept on that basis, with
attribution, as preservation — not as a claim to it. [NOTICE](../../NOTICE) at
the repository root records the full provenance of the code as well as the
documentation.

Paul: if you find this and would prefer it removed, or the attribution or terms
stated differently, open an issue at
<https://github.com/bethington/d2-ds1-edit> and it will be done.
