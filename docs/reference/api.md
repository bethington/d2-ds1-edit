# DS1Edit Command-Line Reference

`ds1edit` has two modes. With no arguments, or with a DS1/INI file, it opens
the GUI. With a recognised *verb* as the first argument it runs headless and
exits.

The binary's own `ds1edit -h` is the authoritative list; this page explains the
parts that need more than one line.

## GUI mode

```
ds1edit                                   open empty
ds1edit <file.ds1> [LevelType] [LevelDef] open one area
ds1edit <list.ini>                        open several areas
```

- **LevelType** -- ID from `LvlTypes.txt`, selecting the tileset.
- **LevelDef** -- DEF value from `LvlPrest.txt`, selecting the preset.

Both are optional; the editor resolves them from the MPQ tables when it can.
For the ID tables and how to find the right values, see
[maze-and-ds1.md](maze-and-ds1.md).

### Multi-file INI

One area per line, whitespace separated, `#` comments ignored:

```ini
# LevelType  LevelDef  path
21 575 assets/tiles/ACT3/Jungle/clearing1.ds1
21 576 assets/tiles/ACT3/Jungle/clearing2.ds1
```

### GUI flags

| Flag | Effect |
|------|--------|
| `-force_dt1 <f.dt1>...` | Use these DT1s (1-32) instead of resolving from the tables. Suppresses the LevelType/LevelDef arguments. |
| `-force_pal <act>` | Force the palette for act 1-5 instead of deriving it. |
| `-no_check_act` | Skip the check that the DS1's internal act matches the tileset. |
| `-resize <w> <h>` | Resize the map on load. |
| `-debug` / `-no_vis_debug` | Verbose logging / suppress the on-screen overlay. |
| `--headless <out.png>` | Render one frame, save it, exit. |
| `--selftest[=N]` | Render N frames (default 30) through the real GUI path, then exit. |
| `--switchbench=GROUP[:N]` | Cycle the first N maps (default 3) of GROUP, reporting switch cost. |

## CLI verbs

Run `ds1edit <verb>` with no arguments for that verb's own usage text.

### Inspecting the MPQ chain
| Verb | Purpose |
|------|---------|
| `list-mpqs` | Which MPQ slots are open, and how many files each holds. |
| `probe <virtual-path>` | Resolve a path through the chain, e.g. `data\Global\AnimData.d2`. |
| `dump-listfile` | Print each open slot's in-MPQ listfile. |
| `dump-txt-row <txt> <id>` | Dump one Excel row as `column=value` pairs. |

### Areas and presets
| Verb | Purpose |
|------|---------|
| `list-areas` | Every area the browser found, grouped by act. |
| `list-files [filter]` | Known DS1 files, optionally substring-filtered. |
| `list-presets` | Parsed `[char_mode_presets]` / `[char_weapon_presets]`. |
| `audit-lvltypes` | `LvlTypes.txt` rows whose name prefix disagrees with their Act column. |
| `help` (`-h`) | The full verb and flag list, straight from the binary. |

### Animation and export
| Verb | Purpose |
|------|---------|
| `probe-cof <cat> <token> <mode> [weapon]` | Build, load and dump a COF. |
| `list-tokens <cat>` | Tokens found in a category (`chars`/`monsters`/`npcs`/`objects`). |
| `export-compose` (alias `export`) | Compose layered DCCs into APNG animations. |
| `export-raw` | Raw DCC/DC6/DT1 frames to per-frame PNG, mirroring the GUI flow. |

### Common flags

```
--ini=<path>         load this INI instead of Ds1edit.ini
--no-ini             ignore INIs; use only CLI flags
--d2-install=<dir>   override d2_install (auto-fills MPQ slots)
--mod-dir=<dir>      override mod_dir[0] (overlay)
--mpq=<path>         append an MPQ to the chain (repeatable)
-v, -vv              increase verbosity
```

### export / export-compose flags

```
--target=TOKEN/MODE[/WEAPON]   single-tuple shortcut
--category=<chars|monsters|npcs|objects>
--token=<code>                 per-token filter for bulk runs
--mode=<list|all>              comma-separated codes, or 'all'
--weapon=<list|all>            chars only; ignored elsewhere
--direction=<n|n,m|n-m|all>    direction selector
--upscale=<1|2|4>              nearest-neighbour APNG scale
--out=<dir>                    else [export_defaults] compose_output
```

### export-raw flags

```
--type=<dcc|dc6|dt1|cof|all>   content-type filter
--target=<path or glob>        single file or pattern
--scope=<all|folder>           bulk scope (area is GUI-only)
--folder=<prefix>              required when --scope=folder
--preset=<name>                named [export_presets] preset
--pattern=<glob>               ad-hoc glob
--listfile=<path>              external listfile, only if the
                               in-MPQ one fails to decompress
--out=<dir>                    else raw_<type>_output
```

## Examples

```bash
ds1edit list-mpqs
ds1edit probe "data\Global\AnimData.d2"
ds1edit probe-cof chars NE WL HTH
ds1edit export --target=NE/WL/HTH --direction=all --upscale=2
ds1edit export --category=monsters --mode=NU --direction=0
ds1edit export-raw --type=dt1 --scope=all --out=C:\tiles
```

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | Clean run |
| 1 | Some failures |
| 2 | Nothing produced |
| 3 | Bad arguments |

## Scripting

Because the verbs are line-oriented on stdout, they compose directly:

```bash
ds1edit list-files jungle | while read f; do
    ds1edit export-raw --type=dt1 --target="$f" --out=out/ || echo "FAILED: $f"
done
```

Helper scripts live in [scripts/python/](../../scripts/python/) --
`generate_level_inis.py` builds multi-file INIs from `LvlPrest.txt`,
`test_ini.py` validates them.
