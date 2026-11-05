# Map Lifecycle Step 4: Build and Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** From the editor, build a project into a distributable `patch_d2.mpq`, and with one command install the project into a separate test copy of the game and start it.

**Architecture:** `core/mpq_write.c` writes archives with no third-party library, either fresh or by appending to a copy of retail's `patch_d2.mpq`. `core/mpq_build.c` validates a project and chooses the files. `core/game_launch.c` installs loose files into the test copy and starts the game. None of those three uses the UI: they take explicit paths and report errors as text. `ui/fields_dialog.c` and handlers in `ui/interface.c` connect them to the Build menu.

**Tech Stack:** C99 (MSVC C mode, GCC, Clang), Allegro 5 for UI only, Unity for C tests, CMake.

**Spec:** `docs/superpowers/specs/2026-11-02-map-lifecycle-design.md`, with its gate answered in `docs/superpowers/specs/2026-11-03-map-lifecycle-gate-results.md`. Read both. Format details and addresses are in `docs/superpowers/specs/2026-11-03-map-lifecycle-gate-re-findings.md`.

## Global Constraints

- Windows, Linux and macOS ship together. No new library, and no new runtime file.
- Nothing writes to the game install. `C:\Diablo2` is never written, by code or by a test.
- Never open an existing file in a game folder for writing. A fleet slot's MPQs are hard links, so writing through one changes every copy. Write a sibling staging file and rename it over the target.
- Never run the editor fullscreen. Every GUI and game run is windowed.
- Archive files: flag `0x80000000` only, format 0, header size 32, sector shift 3, header at offset 0, no `(attributes)`, no `0x01000000`.
- Names in an archive use backslashes only, locale 0, platform 0. `data\global\tiles\...` and `data\global\excel\...`.
- Build MPQ's output is `<project>/build/patch_d2.mpq`, and it carries retail's entries.
- Build & Test installs loose files under the test copy's `DATA\GLOBAL\...`, creates `DATA\GLOBAL\EXCEL` and `DATA\GLOBAL\TILES` itself, sets the game's working folder to the test copy, and removes the previous install's loose files.
- The game loads compiled `.bin` tables unless started with `-direct -txt`. **Decided by the maintainer:** Build MPQ packs the `.bin` files the game itself compiled in the test copy, and refuses if a table changed since a game compiled it.
- User-facing text is plain and says what to do. Match surrounding code: three-space indent, `/* */` comments in new core files.
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Don't push or merge.

## Shell Notes

Windows build and portable tests, from PowerShell in the worktree:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build tmp/build-step4 --config Release -- /m /v:q /nologo
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir tmp/build-step4 -C Release -L portable -j 2 --timeout 300
```

Configure `tmp/build-step4` once with `-A Win32 -DCMAKE_PREFIX_PATH=<main checkout>/build/vcpkg_installed/x86-windows`. `py_release_record` can time out under load; re-run it alone before calling it a failure.

## File Structure

| File | Responsibility |
|---|---|
| `src/core/mpq_crypt.c/.h` (new) | MPQ hashing and table encryption |
| `src/core/mpq_write.c/.h` (new) | Write an archive, fresh or appended to a base |
| `src/core/mpq_build.c/.h` (new) | Validate a project, choose files, call the writer |
| `src/core/game_launch.c/.h` (new) | Check a profile, install to the test copy, start the game |
| `src/core/project.c/.h` | Launch profile keys in `project.ini` |
| `src/core/lvlprest_text.c/.h` | `lvlprest_each_def` |
| `src/core/area_browser.c/.h` | Public `area_browser_game_has_map`, `area_browser_game_table` |
| `src/ui/fields_dialog.c/.h` (new) | A modal of labelled text fields |
| `src/ui/commands.c/.h` | `has_project` in `CMD_CTX`, Build availability |
| `src/ui/interface.c` | Build MPQ, Build & Test, Launch Profile, Project Settings handlers |
| `test/test_mpq_write.c`, `test/test_mpq_build.c`, `test/test_game_launch.c`, `test/support/fake_game.c` (new) | Tests |

`src/mpq/MpqView.c` keeps every read. Its hashing works through the global `glb_mpq`, so the writer has its own `mpq_crypt.c` rather than sharing it as the spec's table says, and the read-back test proves they agree.

---

### Task 1: Find Out Whether a Map of a Different Size Works

The gate lists this as a step 4 blocker. No code decision can be made until it's known, so it goes first. It needs no DS1 writer: a row's cells can point at real maps from a row of another size.

**Files:**
- Modify: `tmp/gate/slot_table.py` (git-ignored; copy from `.worktrees/release-2.2.1/tmp/gate` if absent)
- Modify: `docs/superpowers/specs/2026-11-03-map-lifecycle-gate-results.md`

**Interfaces:**
- Produces: the outcome line later tasks read. "Different sizes work" means no code change; anything else means Step 5 below.

- [ ] **Step 1: Add two cases to `slot_table.py`.** Rows are `Name` column 0, `Def` 1, and find `SizeX`, `SizeY`, `Files`, `File1` from the header. Take simple sized Act 1 rows (name starts `Act 1`, `SizeX` > 0, `Files` 1-6 equal to filled cells).

```python
def size_rows(lines):
    head = lines[0].split("\t")
    sx, sy = head.index("SizeX"), head.index("SizeY")
    out = []
    for i, line in enumerate(lines):
        c = line.split("\t")
        if i == 0 or len(c) <= CELL1 + 5 or not c[0].startswith("Act 1"):
            continue
        try:
            files, x, y = int(c[FILES]), int(c[sx]), int(c[sy])
        except ValueError:
            continue
        filled = sum(1 for v in c[CELL1:CELL1 + 6] if v not in ("", "0"))
        if x > 0 and 1 <= files <= 6 and files == filled:
            out.append((i, c, (x, y)))
    return out


def folder(path):
    return path.replace("\\", "/").rsplit("/", 1)[0].lower()


def size_case(lines, case):
    rows = size_rows(lines)
    # Read before any row changes. Same folder keeps the tiles the same, so a crash
    # can only be about the size.
    first = [(c[CELL1], folder(c[CELL1]), size) for i, c, size in rows]
    n = 0
    for i, c, size in rows:
        other = next((m for m, f, s in first if f == folder(c[CELL1]) and s != size), None)
        if other is None:
            continue
        count = int(c[FILES])
        if case == "size-control":        # proves these rows get picked on the walk
            c[CELL1:CELL1 + count] = ["Act1/Outdoors/GateNoSuchMap.ds1"] * count
        else:                             # size-swap: a real map from a row of another size
            c[CELL1:CELL1 + count] = [other] * count
        lines[i] = "	".join(c)
        n += 1
    return n
```

Call `size_case` from `main()` when `case.startswith("size-")`, before the loop.

- [ ] **Step 2: Run `size-control`.** Start `& "$HOME\source\cpp\d2-fleet\build\Release\D2Fleet.exe" serve` in the background. `fleet_launch` `lod-113c`, args `-w -direct -txt`, override `{"auto_enter":"none"}`, slot 0. Right after launch, `python tmp/gate/slot_table.py size-control 0`. Enter a game: title click (400,307), wait, OK (690,555), repeating the title click if it stays on the title. Walk with `fleet_click` (60,370), about 5 s each, past Blood Moor. Check the slot folder for a `D2*.txt` crash log, excluding `d2registry`.

Expected: a crash at or after the row's rooms generate. **No crash means these rows weren't picked, and the swap run below proves nothing.**

- [ ] **Step 3: Run `size-swap` the same way,** with `fleet_quit` and a pause first so the slot frees, on the same character. Take a `fleet_frame` at the end of the walk.

- [ ] **Step 4: Record the result** in the gate results. Move the size bullet out of "Not Proved Yet" into "How the Game Uses File Cells" with what ran and what happened. Stop the fleet server. Commit.

```bash
git add docs/superpowers/specs/2026-11-03-map-lifecycle-gate-results.md
git commit -m "Record whether the game takes a map of a different size"
```

- [ ] **Step 5: Only if `size-swap` crashed, or `size-control` didn't crash (inconclusive counts as unsafe), refuse a different size.** In `src/core/map_ops.c`, with a declaration and comment in `map_ops.h`:

```c
/* Must a map of this size fit this row? 0, or -1 with why. A row with no size takes any. */
int map_ops_check_row_size(char * table, int def, int width, int height, char * error, int cap)
{
   int want_w = 0, want_h = 0;

   map_ops_default_size(table, def, &want_w, &want_h);
   if (want_w == 0 || (width == want_w && height == want_h))
      return 0;
   snprintf(error, cap, "Def %d's maps are %d by %d, and the game can't use another size here.",
            def, want_w, want_h);
   return -1;
}
```

`map_ops_default_size` leaves its outputs alone for a row with no size. Call it from `try_accept` in `src/ui/map_dialog.c` after `map_ops_check_size`, and from `ds1_manager_clone` with the source map's size. Test in `test/test_map_ops.c`: a different size refused, the row's size accepted, a row with no size accepted. Update the manual's New section. Commit as "Refuse a map whose size its row can't use".

---

### Task 2: The MPQ Writer

**Files:**
- Create: `src/core/mpq_crypt.c`, `src/core/mpq_crypt.h`, `src/core/mpq_write.c`, `src/core/mpq_write.h`, `test/test_mpq_write.c`
- Modify: `CMakeLists.txt` (after `src/core/map_ops.c`), `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `file_copy.h`, `mpq_load_in_mem` from `src/mpq/MpqView.h` (tests only).
- Produces:

```c
/* mpq_crypt.h */
#define MPQ_HASH_OFFSET 0
#define MPQ_HASH_A      1
#define MPQ_HASH_B      2
#define MPQ_HASH_KEY    3
unsigned int mpq_hash_string(const char * name, int type);
void mpq_encrypt(unsigned int * data, int dwords, unsigned int key);
void mpq_decrypt(unsigned int * data, int dwords, unsigned int key);

/* mpq_write.h */
typedef struct MPQ_WRITE_FILE_S
{
   const char * archive_name;   /* data\global\... with backslashes */
   const char * source_path;    /* a file on disk */
} MPQ_WRITE_FILE_S;

/* Write out_path from base_path (NULL for a new archive) plus these files. A file
 * whose name base_path has replaces it there. Writes a staging file and renames it
 * over out_path only when complete, so a failure leaves out_path as it was. base_path
 * is only read. 0, or -1 with why in error. */
int mpq_write(const char * base_path, const char * out_path,
              const MPQ_WRITE_FILE_S * files, int count, char * error, int cap);
```

- [ ] **Step 1: Write the failing tests** in `test/test_mpq_write.c`, using `support/test_tmp.h` as `test_map_ops.c` does. The test defines `CONFIG_S glb_config;`, allocates `glb_mpq` the way the editor does, and defines a `ds1edit_error` stub that fails the test. Tests:

  - `known_keys`: `mpq_hash_string("(hash table)", MPQ_HASH_KEY) == 0xC3AF3770` and `"(block table)"` gives `0xEC83B3A3`.
  - `encrypt_round_trips`: encrypting then decrypting 8 dwords returns them.
  - `new_archive_reads_back`: two source files, read each back with `mpq_load_in_mem`, compare bytes and length.
  - `name_case_and_slash`: an archive name with `/` is refused; a lookup in another case succeeds.
  - `flags_and_header`: parse the output's bytes: `MPQ\x1a`, header size 32, format 0, shift 3, archive size equals the file size, every block flag `0x80000000`, hash count a power of two.
  - `append_keeps_base`: build a base with `a` and `b`, append `c` and a new `b`. `a` and `c` read back from the output, `b` is the new bytes, and the base file is byte-identical to before.
  - `append_drops_attributes`: a base holding `(attributes)`: the output has no such entry when looked up.
  - `append_refuses_unusual_base`: tables not at the end, shift other than 3, or no `MPQ\x1a` at offset 0 gives -1 and no output.
  - `full_table_refused`: a base with hash count 4 holding 3 names, plus 2 more, gives -1 and leaves an existing output unchanged.
  - `failure_keeps_old_output`: a missing source file gives -1, the old output is unchanged, and no staging file is left.

- [ ] **Step 2: Wire the test.** In `test/CMakeLists.txt`:

```cmake
add_executable(test_mpq_write test_mpq_write.c
    ../src/core/mpq_crypt.c ../src/core/mpq_write.c ../src/core/file_copy.c
    ../src/mpq/MpqView.c ../src/mpq/Explode.c ../src/mpq/Dcl_tbl.c ../src/mpq/Wav_unp.c
    ../src/platform.c)
target_link_libraries(test_mpq_write PRIVATE unity ZLIB::ZLIB)
target_include_directories(test_mpq_write PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/mpq)
add_test(NAME mpq_write COMMAND test_mpq_write)
set_tests_properties(mpq_write PROPERTIES LABELS "portable")
```

Add any further stub the link asks for to the test file. Run it and see it fail to link.

- [ ] **Step 3: Implement `mpq_crypt.c`.**

```c
#include <ctype.h>
#include "core/mpq_crypt.h"

static unsigned int s_table[0x500];
static int s_ready = 0;

static void prepare(void)
{
   unsigned int seed = 0x00100001, index1, index2;
   int i;

   if (s_ready)
      return;
   for (index1 = 0; index1 < 0x100; index1++)
      for (index2 = index1, i = 0; i < 5; i++, index2 += 0x100)
      {
         unsigned int t1, t2;
         seed = (seed * 125 + 3) % 0x2AAAAB;
         t1 = (seed & 0xFFFF) << 16;
         seed = (seed * 125 + 3) % 0x2AAAAB;
         t2 = seed & 0xFFFF;
         s_table[index2] = t1 | t2;
      }
   s_ready = 1;
}

unsigned int mpq_hash_string(const char * name, int type)
{
   unsigned int seed1 = 0x7FED7FED, seed2 = 0xEEEEEEEE;

   prepare();
   for (; *name != '\0'; name++)
   {
      unsigned int c = (unsigned int) toupper((unsigned char) *name);
      seed1 = s_table[(type << 8) + c] ^ (seed1 + seed2);
      seed2 = c + seed1 + seed2 + (seed2 << 5) + 3;
   }
   return seed1;
}

void mpq_encrypt(unsigned int * data, int dwords, unsigned int key)
{
   unsigned int seed = 0xEEEEEEEE;
   int i;

   prepare();
   for (i = 0; i < dwords; i++)
   {
      unsigned int plain = data[i];
      seed += s_table[0x400 + (key & 0xFF)];
      data[i] = plain ^ (key + seed);
      key = ((~key << 21) + 0x11111111) | (key >> 11);
      seed = plain + seed + (seed << 5) + 3;
   }
}

void mpq_decrypt(unsigned int * data, int dwords, unsigned int key)
{
   unsigned int seed = 0xEEEEEEEE;
   int i;

   prepare();
   for (i = 0; i < dwords; i++)
   {
      unsigned int plain;
      seed += s_table[0x400 + (key & 0xFF)];
      plain = data[i] ^ (key + seed);
      data[i] = plain;
      key = ((~key << 21) + 0x11111111) | (key >> 11);
      seed = plain + seed + (seed << 5) + 3;
   }
}
```

- [ ] **Step 4: Implement `mpq_write.c`.** Use `unsigned int` for every on-disk dword, and write them through a little-endian helper so big-endian hosts stay correct.

  The steps inside `mpq_write`:

  1. Refuse any `archive_name` that holds `/`, or is empty.
  2. Load the base if given: read the 32-byte header and refuse unless `MPQ\x1a` is at offset 0, header size 32, format 0, shift 3, hash count a power of two, `hash_pos + hash_count*16 == block_pos`, and `block_pos + block_count*16 == archive_size == file size`. Read and `mpq_decrypt` both tables, with keys from `mpq_hash_string("(hash table)", MPQ_HASH_KEY)` and `"(block table)"`.
  3. With no base, the hash count is the smallest power of two at least `2 * (count + 1)` and at least 16, every slot `0xFFFFFFFF`, and the data starts at offset 32.
  4. Open `<out_path>.staging` with `"wb"`. Copy the base's bytes `[0, hash_pos)`, or write 32 zero bytes.
  5. For each file: note the offset, append the source's bytes, add a block `{offset, size, size, 0x80000000}`, and call `set_slot` below. A source that can't be read fails the build.
  6. If the base has `(attributes)`, mark its slot `0xFFFFFFFE`.
  7. `mpq_encrypt` and write the hash table, then the block table, then seek to 0 and write the header: `MPQ\x1a`, 32, archive size, format 0, shift 3, positions, counts.
  8. `fclose`, checking it. Then replace the target: remove `out_path` if present and rename the staging file. On any failure, remove the staging file and leave `out_path` alone.

```c
/* Point the slot for this name at this block. A name already there is replaced, in
   whatever locale it has. Otherwise the first empty or deleted slot along its probe. */
static int set_slot(unsigned int * hash, unsigned int hash_count, const char * name,
                    unsigned int block)
{
   unsigned int a = mpq_hash_string(name, MPQ_HASH_A);
   unsigned int b = mpq_hash_string(name, MPQ_HASH_B);
   unsigned int start = mpq_hash_string(name, MPQ_HASH_OFFSET) & (hash_count - 1);
   unsigned int i, free_slot = hash_count;

   for (i = 0; i < hash_count; i++)
   {
      unsigned int s = (start + i) & (hash_count - 1);
      unsigned int * e = hash + s * 4;

      if (e[3] == 0xFFFFFFFF)
      {
         if (free_slot == hash_count)
            free_slot = s;
         break;
      }
      if (e[3] == 0xFFFFFFFE)
      {
         if (free_slot == hash_count)
            free_slot = s;
         continue;
      }
      if (e[0] == a && e[1] == b)
      {
         e[3] = block;
         return 0;
      }
   }
   if (free_slot == hash_count)
      return -1;
   {
      unsigned int * e = hash + free_slot * 4;
      e[0] = a;
      e[1] = b;
      e[2] = 0;
      e[3] = block;
   }
   return 0;
}
```

A failed `set_slot` reports "The archive's hash table is full."

- [ ] **Step 5: Run the tests.** Expected: `mpq_write` passes. Fix any disagreement with the reader in the writer, not the reader.

- [ ] **Step 6: Commit** as "Write MPQ archives with no third-party library".

---

### Task 3: Launch Profile and Profile Checks

**Files:**
- Modify: `src/core/project.c`, `src/core/project.h`, `test/test_project.c`
- Create: `src/core/game_launch.c`, `src/core/game_launch.h`, `test/test_game_launch.c`
- Modify: `CMakeLists.txt`, `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `project_path_is_inside(path, dir)`.
- Produces:

```c
/* project.h, in PROJECT_S */
char test_game_dir[PROJECT_PATH_MAX];
char test_exe[128];                   /* default "Game.exe" */
char test_args[256];                  /* default PROJECT_DEFAULT_TEST_ARGS */
char test_wrapper[PROJECT_PATH_MAX];  /* default "" on Windows, "wine" elsewhere */
#define PROJECT_DEFAULT_TEST_ARGS "-w -direct -txt"

/* game_launch.h */
typedef struct GAME_PROFILE_S
{
   const char * game_dir;
   const char * exe;
   const char * args;
   const char * wrapper;
} GAME_PROFILE_S;

/* Is this profile usable? 0, or -1 with why. `installs` are folders it may not be,
   contain or be inside; NULL or empty entries are skipped. */
int game_profile_check(const GAME_PROFILE_S * p, const char * const * installs, int install_count,
                       char * error, int cap);
```

- [ ] **Step 1: Write the failing tests.** In `test_project.c`: a new project has the defaults; values survive save and load; a `project.ini` from before this step gets defaults. In `test_game_launch.c`, over a temp folder holding an empty `Game.exe`:

  - accepted when complete;
  - empty `game_dir` refused, saying to set the test folder;
  - `exe` missing from the folder refused, naming it;
  - `exe` with `..` or rooted refused;
  - `game_dir` equal to an install refused, in another case and slash style on Windows;
  - `game_dir` inside an install, and an install inside `game_dir`, both refused;
  - `args` lacking `-direct` or `-txt` refused: "The test arguments need -direct and -txt, or the game ignores the project.", matched as whole words, any case;
  - a folder that can't be written refused, skipped where a test can't make one (running as root or on Windows ACLs).

- [ ] **Step 2: Implement.** Add the fields to `project_load`, `project_save` and `project_create` under keys `test_game_dir`, `test_exe`, `test_args`, `test_wrapper`. Empty `test_exe` or `test_args` after load gets the default. Write `game_profile_check`, proving the folder is writable by creating and removing `<game_dir>/ds1edit-write-test.tmp` with `"wb"`, a new name that can't be a linked file.

`-w` isn't from the gate: it keeps the game windowed, so a test run doesn't take over the screen.

- [ ] **Step 3: Run the tests,** then commit as "Store a launch profile in the project and check it".

---

### Task 4: Build MPQ's Core

**Files:**
- Create: `src/core/mpq_build.c`, `src/core/mpq_build.h`, `test/test_mpq_build.c`
- Modify: `src/core/lvlprest_text.c/.h`, `test/test_lvlprest_text.c`, `CMakeLists.txt`, `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `mpq_write`, `map_ops_read_table`, `lvlprest_each_file`, `lvlprest_each_cleared`, `lvlprest_files_hole`, `MAP_OPS_TABLE_PATH_MAX`, the directory iterator in `platform.h`.
- Produces:

```c
/* lvlprest_text.h */
typedef void (* LVLPREST_DEF_CB)(int def, void * ctx);
void lvlprest_each_def(char * table, LVLPREST_DEF_CB cb, void * ctx);

/* mpq_build.h */
typedef int (* MPQ_BUILD_IN_GAME_CB)(const char * ds1_path, void * ctx);

typedef struct MPQ_BUILD_ARGS_S
{
   const char * project_dir;
   const char * base_mpq;        /* the install's patch_d2.mpq; only read */
   const char * test_game_dir;   /* "" if none; where compiled .bin files come from */
   const char * out_mpq;
   char * game_table;            /* the game's own LvlPrest.txt */
   MPQ_BUILD_IN_GAME_CB in_game; /* is this map in the game's MPQs? */
   void * ctx;
} MPQ_BUILD_ARGS_S;

typedef struct MPQ_BUILD_RESULT_S
{
   int files;
   long bytes;
} MPQ_BUILD_RESULT_S;

/* Validate, choose and write. 0, or -1 with every problem in `problems`, one to a line,
   and nothing written. */
int mpq_build_run(const MPQ_BUILD_ARGS_S * a, MPQ_BUILD_RESULT_S * out, char * problems, int cap);

/* Calls cb for each project file that belongs in a build or a test install, by the
   "Files taken" and "Files left out" rules. rel is relative to the Tiles or Excel
   folder, with forward slashes. is_table says which. Game_install uses this too. */
typedef void (* MPQ_BUILD_FILE_CB)(const char * source_path, const char * rel, int is_table,
                                   void * ctx);
void mpq_build_each_file(const char * project_dir, MPQ_BUILD_FILE_CB cb, void * ctx);
```

The rules, each with a test over a temp project and a base built with `mpq_write`:

| Rule | Detail |
|---|---|
| Files taken | `.ds1` and `.dt1` under `Global/Tiles` and `Data/Global/Tiles`, `.txt` under `Global/Excel` and `Data/Global/Excel`, in any case. `Data/` wins over `Global/` for the same path, as reads do |
| Files left out | `backup/`, `build/`, `project.ini`, `.tmp`, staging files, `<table>.old`, and a numbered save backup: `<name>-NNN.ds1` when `<name>.ds1` is beside it and no table lists it |
| Archive names | `data\global\tiles\<rest>` and `data\global\excel\<name>`, backslashes, keeping the rest's case. A name over 200 characters, or with a byte outside printable ASCII, is a problem |
| Added cells | Each path `lvlprest_each_cleared(project_table, game_table, ...)` reports must be in the project or `in_game`, and fit `MAP_OPS_TABLE_PATH_MAX` |
| Holes | For each Def: a `lvlprest_files_hole` in the project's row that the game's row doesn't have is a problem naming Def and cell. Def 915 ships with one, so a hole the game also has is not |
| New maps | A project `.ds1` that isn't `in_game` must be in a File cell of the project's table: "<path> isn't in LvlPrest.txt, so the game can't reach it." |
| Compiled tables | For each project `.txt`, `<test>/DATA/GLOBAL/EXCEL/<same name>` must be byte-identical and `<name>.bin` must exist, found in any case. Otherwise: "<name>.txt has changed since the game last compiled it. Run Build & Test and start a game first." With no test folder set, the same message. The `.bin` is packed as `data\global\excel\<name>.bin` |
| Same version | If any `.bin` is packed, the test copy's `patch_d2.mpq` must be the same size as `base_mpq`: "The test copy is a different game version from the install, so its compiled tables can't be used." |
| Base | `base_mpq` missing: "The game folder has no patch_d2.mpq to build on." |
| Listfile | A `(listfile)` of the project's names joined by CRLF, written to a temp file in `build/`, removed afterwards |

A project with only changed maps needs no test run, since a map in the archive wins with no table change.

- [ ] **Step 1: Write the failing test for `lvlprest_each_def`** in `test_lvlprest_text.c`, including a row whose cells are all empty, which `lvlprest_each_file` wouldn't report. Implement it beside `lvlprest_each_file`. Run the tests.

- [ ] **Step 2: Write the failing tests in `test_mpq_build.c`.** One test per row of the table, plus `only_changed_maps_need_no_test_copy`, `base_entries_survive` (a name only in the base still reads back from the output), `problems_write_nothing` (an existing output stays byte-identical), and `lists_every_problem` (two problems, both named). Wire it as `test_map_ops` is wired, with `ds1edit_allegro` for what `map_ops.c` needs, and add it to `DS1EDIT_ALLEGRO_TESTS`.

- [ ] **Step 3: Implement `mpq_build.c`.** Collect into a growing array of `{archive_name, source_path}`, run every validation and gather problems, then return -1 if there are any. Create `build/` with `project_ensure_parent_dirs`. Write through `mpq_write`, then remove the listfile.

- [ ] **Step 4: Run the tests,** then commit as "Validate a project and build its patch_d2.mpq".

---

### Task 5: Install and Start the Game

**Files:**
- Modify: `src/core/game_launch.c/.h`, `test/test_game_launch.c`, `test/CMakeLists.txt`
- Create: `test/support/fake_game.c`

**Interfaces:**
- Consumes: `GAME_PROFILE_S`, `file_copy_checked`, `mpq_build_each_file`.
- Produces:

```c
/* Install the project's loose files in the game folder: remove what the last install
   listed and the .bin of each table, then copy. Copies go to staging files first, so a
   copy that fails changes nothing. Writes <game_dir>/ds1edit-installed.txt. 0, or -1
   with why. *out_files gets the count. */
int game_install(const char * project_dir, const char * game_dir, int * out_files,
                 char * error, int cap);

/* Start the game and don't wait for it. The working folder is game_dir. 0, or -1 with
   the reason and the full command line in error. */
int game_start(const GAME_PROFILE_S * p, char * error, int cap);

/* The command line as text, for messages and tests. */
void game_command_text(const GAME_PROFILE_S * p, char * out, int cap);
```

- [ ] **Step 1: Write the failing install tests:**

  - tiles land in `DATA/GLOBAL/TILES/...` and tables in `DATA/GLOBAL/EXCEL/...`, with both folders made even for a project with no tables;
  - the same files as Build MPQ, through `mpq_build_each_file` from Task 4;
  - a second install with a map gone removes that map's loose file;
  - **only listed files are removed:** an unrelated file in `DATA/GLOBAL/TILES` survives, and a manifest line with `..` or a rooted path is ignored and left alone;
  - a table's `.bin` is removed, so a stale one can't be read as fresh;
  - a project file that can't be read leaves the folder unchanged and no staging file;
  - no write goes through an existing file: on Linux and macOS, hard-link a target to a second name, install, and check the second name's bytes are unchanged. Skip on Windows.

- [ ] **Step 2: Implement `game_install`.** Copy each file to `<target>.ds1edit-new`. If all succeed, read the old manifest and remove its files and the `.bin` of each project table. Then remove each target and rename its staging file over it. If a rename or remove fails: "The game may be running. Close it and try again." Finish by writing the manifest through a staging file. Names in it are relative, one to a line, with forward slashes.

- [ ] **Step 3: Write `test/support/fake_game.c` and the start tests.** It writes its working folder and every argument, one to a line, to `fake_game.out` in its working folder, and exits. Add `add_executable(fake_game support/fake_game.c)`, and pass `$<TARGET_FILE:fake_game>` to `test_game_launch` as a compile definition or argument. Tests poll for the output for up to 5 seconds:

  - arguments and working folder arrive exactly, including a quoted argument with a space;
  - with a wrapper, the wrapper runs first and gets the executable's full path, then the arguments;
  - a missing wrapper or executable returns -1 with the reason and the command line;
  - `game_command_text` matches what ran.

For a wrapper test, copy `fake_game` into the temp folder as the wrapper and use any file as the exe.

- [ ] **Step 4: Implement `game_start`.** Split `args` on whitespace, keeping double-quoted runs whole.

  - **Windows:** `CreateProcessA` with `lpCurrentDirectory = game_dir`, a command line quoted by the Windows rules, `CloseHandle` on both handles.
  - **POSIX:** a pipe with `FD_CLOEXEC`; `fork`; in the child `setsid`, `chdir(game_dir)`, then a second `fork` so nothing is left to reap; `execvp`, on failure writing `errno` to the pipe and `_exit(127)`. The parent `waitpid`s the first child and reads the pipe: bytes mean failure, with `strerror`. EOF means it started.

- [ ] **Step 5: Run the tests,** then commit as "Install a project in the test copy and start the game".

---

### Task 6: The Build Menu

**Files:**
- Create: `src/ui/fields_dialog.c`, `src/ui/fields_dialog.h`
- Modify: `src/ui/commands.c/.h`, `test/test_commands.c`, `src/ui/menu_bar.c` (the context it fills), `src/ui/interface.c`, `src/core/area_browser.c/.h`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `mpq_build_run`, `game_profile_check`, `game_install`, `game_start`, `map_save_ui_save_dirty`, `map_any_dirty`.
- Produces:

```c
/* fields_dialog.h */
typedef struct FIELD_S
{
   const char * label;
   char * value;   /* in: the default. out: the entry */
   int cap;
} FIELD_S;

typedef int (* FIELDS_CHECK_CB)(char * status, int cap, void * ctx);

/* A modal of text fields, built like map_dialog.c: Tab moves, Enter is OK, Esc cancels.
   A field may be left empty. On OK, check runs and a -1 keeps the dialog up showing its
   status, with the values restored on cancel. 1 for OK, 0 for cancel. */
int fields_dialog_show(const char * title, FIELD_S * fields, int count,
                       FIELDS_CHECK_CB check, void * ctx);

/* area_browser.h */
int area_browser_game_has_map(const char * ds1_path);  /* today's static game_has_map */
char * area_browser_game_table(void);                  /* the game's LvlPrest.txt with no project overlay; caller frees */
```

`area_browser.c` already reads the game's table to find Removed maps: expose that read rather than adding a second one. `manager_seed` isn't it, since overlays reach that.

- [ ] **Step 1: Availability, test first.** Add `int has_project;` to `CMD_CTX`. In `test_commands.c`: all four commands are off with no project and on with one, and Build MPQ doesn't need an open map. In `commands_is_available` replace the "Step 4 implements these" group with:

```c
   case CMD_PROJECT_SETTINGS:
   case CMD_BUILD_MPQ:
   case CMD_BUILD_AND_TEST:
   case CMD_LAUNCH_PROFILE:
      return c->has_project;
```

Set the field where the context is filled, from `glb_project.is_open`. Run the tests.

- [ ] **Step 2: `fields_dialog.c`.** Follow `map_dialog.c`'s structure: its event loop, `draw_field`, `handle_key`, OK and Cancel hit tests, and the window-close rule. It holds its working copies in a local struct and copies back on OK. Max 6 fields, values shown scrolled to their end when too long for the field.

- [ ] **Step 3: Handlers in `interface.c`,** added to `interface_run_command`, and to the switch after it so keys held in a dialog don't reach the map:

  - **Launch Profile:** `fields_dialog_show` with Test game folder, Executable, Arguments, Wrapper. `check` copies the values into a `GAME_PROFILE_S` and calls `game_profile_check` with the configured and project `d2_install`. On OK, store and `project_save`.
  - **Project Settings:** Name and Game folder (`d2_install`), the one project setting not otherwise editable. On OK, `project_save`, and if the folder changed, say that it takes effect the next time the project opens.
  - **Build MPQ:** `interface_build_mpq(int quiet)`.
    1. If `map_any_dirty()`, ask with a Yes/No box "Save all maps and build?". No cancels. Yes runs `map_save_ui_save_dirty()` and stops if it fails.
    2. Fill `MPQ_BUILD_ARGS_S`: `base_mpq` is `<d2_install>/patch_d2.mpq`, `out_mpq` is `<project>/build/patch_d2.mpq`, `in_game` wraps `area_browser_game_has_map`.
    3. Time `mpq_build_run` with `al_get_time`. Show problems in an error box. On success, unless quiet, show the path, files, size and seconds, plus: "Install it by replacing patch_d2.mpq in a copy of the game."
  - **Build & Test:**
    1. Save as above.
    2. If `game_profile_check` fails, open the Launch Profile dialog; stop on cancel.
    3. `game_install`, then `game_start`, with failures in an error box.
    4. Say nothing on success: the game's window is the answer.

    It doesn't call `mpq_build_run`, because the game needs only the loose files, and a project's first test run has no compiled tables to pack. **This departs from the spec's "builds, installs the result".** Update the spec's Test section to say so.

The result box and all reporting use `interface_map_op_failed`'s pattern, with `input_suppress_held` and `input_discard_pending` afterwards.

- [ ] **Step 4: Run the tests, and build on Linux** with `.worktrees/release-2.2.1/tmp/wsl/build.sh` copied into this worktree with `SRC` changed. Don't run the Windows and Linux builds at once: they share `bin/`.

- [ ] **Step 5: On screen, Linux WSLg, windowed.** Using `tmp/wsl/d.sh`:
  - with no project, the Build menu is dimmed;
  - Build MPQ with a New map and a table but no test folder is refused with the compile message, and writes nothing;
  - Launch Profile refuses the folder equal to `d2_install` in its status row;
  - Build MPQ with only a changed map succeeds and reports its counts;
  - `Ctrl+B` and `Ctrl+Shift+B` work.

`wine` isn't available here, so don't start a game on Linux. Say so in the report.

- [ ] **Step 6: Commit** as "Connect Build MPQ, Build & Test and the profile dialogs".

---

### Task 7: Docs

**Files:**
- Modify: `docs/superpowers/specs/2026-11-02-map-lifecycle-design.md`, `docs/getting-started/manual.md`, `CHANGELOG.md`, `docs/testing/native-release-checklist.md`, `docs/guides/mods-and-hooking-1.13c.md`, `README.md` if it lists Build commands

- [ ] **Step 1: Spec.** In Build: output name and retail entries, the compiled-table rule, what the holes rule allows. In Test: loose files, `-w -direct -txt`, no MPQ step, and the manifest. Replace the Gate section's outcome list with a pointer to the gate results.

- [ ] **Step 2: Manual.** A Build section:
  - the two commands and how they differ;
  - why Build & Test comes before Build MPQ when a table changed;
  - that the MPQ replaces `patch_d2.mpq` in a player's copy, and that it's built from your install's version;
  - setting up a test copy, which is a real copy of the game folder and never the one the editor reads;
  - that the test copy is the only place the editor writes in a game folder, and only under `DATA\GLOBAL` and `ds1edit-installed.txt`;
  - that **the game compiles tables when a game starts, not at launch**, so Build MPQ needs a game started once.

- [ ] **Step 3: Checklist and changelog.** Add one per-platform item for the end-to-end run, with the total updated: create a map, edit, delete, build, see it in the game. Add a changelog entry. Fix the guide's priorities near lines 219-222 and 1028-1030: `d2exp` is 3000 and `patch_d2` is 5000.

- [ ] **Step 4: Commit** as "Document building and testing a project".

---

### Task 8: End-to-End in the Real Game

The spec requires seeing a changed map in the running game before step 4 ships. **No fleet slot here:** a slot is rewritten at launch, and this tests the editor's own launch.

**Files:**
- Modify: `docs/superpowers/specs/2026-11-03-map-lifecycle-gate-results.md`, `docs/testing/native-release-checklist.md`

- [ ] **Step 1: Make a test copy.** `Copy-Item -Recurse C:\D2Fleet\versions\lod-113c C:\D2Test\lod-113c`, which makes real files rather than links. Confirm with `fsutil hardlink list C:\D2Test\lod-113c\patch_d2.mpq` showing one name. Start `Game.exe -w` there once by hand to see that it runs outside the fleet. If it won't, report what it needs: registration is a question for the maintainer, so don't change the registry.

- [ ] **Step 2: A changed map, seen.** In the Windows editor, windowed, with a project whose `d2_install` is the reference `C:\D2Fleet\versions\lod-113c`, open `Act1/Town/TownN1.ds1`, and remove an obvious building's walls or objects by the Rogue camp's entrance. Save. Set the profile to `C:\D2Test\lod-113c`. Press `Ctrl+Shift+B`. Enter a game and take a screenshot with the `screenshot` skill. The changed spot must show. Close the game.

- [ ] **Step 3: A new table.** Add `BarW5` by New, with the map's floor visibly different. Build & Test, start a game, and check `DATA\GLOBAL\EXCEL\LvlPrest.bin` exists. Build MPQ now succeeds, and its archive holds `data\global\excel\LvlPrest.bin`.

- [ ] **Step 4: The built MPQ at startup, without `-direct`.** In a second copy, `C:\D2Test\lod-113c-mpq`, replace `patch_d2.mpq` with the built one. Start `Game.exe -w` with no `-direct -txt`, and enter town. Expected: it runs, and the changed `TownN1` shows, proving the archive mounts at startup and wins. No `D2*.txt` crash log appears in the folder, leaving out `d2registry`.

- [ ] **Step 5: Update the gate results.** Move each thing now proved out of "Not Proved Yet" with what ran. Those left stay there by name: `wine`, 1.14, the read-only folder, File cell length. Tick the Windows end-to-end item, and leave Linux and macOS open.

- [ ] **Step 6: Commit** as "Record the end-to-end game test".

If any of this fails, stop and report it with what was on screen: don't adjust the code to fit in this task.

---

## Self-Review

**Spec coverage:**

| Spec requirement | Task |
|---|---|
| Own writer, version 1, stored, no library | 2 |
| Listfile | 4 |
| Excludes backups, `build/`, `project.ini` | 4 |
| Refuses unsaved maps, Save All and Build or Cancel | 6 |
| Validates tables, registration, path fit; nothing written on failure | 4 |
| Reports files, size, duration | 6 |
| Replace previous output only when complete | 2 |
| Profile keys and defaults | 3 |
| Launch Profile dialog; Build & Test opens it until complete | 6 |
| Refuses `test_game_dir` equal to `d2_install` | 3 |
| First-use validation of folder, exe and write | 3 |
| Game running: says to close it | 5 |
| Launch failure shows reason and command line | 5 |
| Gate: changed map seen in the game | 8 |
| Size blocker | 1 |
| MPQ read back with the editor's reader, validation failures write nothing | 2, 4 |
| Launch test with a fake executable; install folder refused | 3, 5 |
| Checklist end-to-end item per platform | 7, 8 |
| Commands in the table, availability, hotkeys | 6 |

**Known not covered:** `wine` on Linux and macOS, and 1.14 installs, stay under "Not Proved Yet". `Project Settings` has no behaviour in the spec, so Task 6 keeps it to name and game folder.
