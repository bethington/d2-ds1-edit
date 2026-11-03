# Map Lifecycle Step 3: Create and Delete Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create and clone maps through a dialog, and delete, revert, remove and restore them by storage state, with backups kept in the project.

**Architecture:** All file and table work moves into `core/map_ops.c`, which takes explicit paths and has no editor globals, so temp-folder tests can cover it. `core/ds1_manager.c` stays the bridge between the area browser and `map_ops`. The command table learns each map's state; `ui/map_dialog.c` is the new form.

**Tech Stack:** C99, Allegro 5, Unity tests, CMake. MSVC C mode (Win32), GCC and Clang.

**Spec:** `docs/superpowers/specs/2026-11-02-map-lifecycle-design.md`, sections "Map states", "Create", "Delete", "Menus", and Build order step 3.

## Global Constraints

- Nothing may write to the game install. `project_path_in_game_install` guards every destination that isn't built from the project folder.
- Create is all-or-nothing: "on a failed table write, the new `.ds1` is removed".
- "Every one of these confirms first, naming the map and what will happen."
- "A backup is only recorded after the map data is copied, and an operation with nothing to back up stops with a message."
- "The name must be unique in the area and valid on all three platforms."
- All three platforms ship together. No platform-only code outside `platform.h`, `#ifdef WIN32` for mkdir, and existing `__APPLE__` modifier handling.
- `--headless` must stay byte-identical: no change to drawing with no project open and no Removed rows.
- GUI runs stay windowed. Tests never write outside their temp folder.
- Commit trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Decisions this plan makes

| Topic | Decision | Why |
|---|---|---|
| Where the logic lives | New `core/map_ops.c`, rather than growing `ds1_manager.c` as the spec's component table says | `ds1_manager.c` needs the browser's globals, and the spec wants temp-folder tests |
| Target of menu commands | The open map's row, as step 2 settled. The browser's right-click menu acts on the clicked row | Selection follows the pointer, so it can't survive a trip to the menu bar |
| Tileset field | Choosing a LvlPrest row (Def) in the area. The row sets the tileset mask and owns the File slots | A DS1 has no tileset of its own |
| Seeding the project's table | From what the editor reads now, with overlays. With no overlay that is the MPQ chain the spec names | A project on top of a `mod_dir` mod must keep that mod's rows |
| After Create or Clone | Stay on the current map. Expand the group so the `N` row shows | Opening unloads every open map |
| After Revert of the open map | Reopen it from the game, through the unsaved-edits guard | Otherwise the screen keeps showing what was just reverted |
| After Delete of the open map | Map stays in memory, row cleared (step 2's ruling). The confirmation says so if it has unsaved edits | Closing it would discard other open maps' edits |
| Removed rows | Can't be opened: the project's table no longer lists them. Right click or the open row offers Restore Slot | `read_lvlprest_txt` would fail on them |
| Restore Slot when the slot is taken | Use the first empty slot. No empty slot stops with a message | The game picks among File1 to File6, so which one doesn't matter |
| Old `./backup` | Still listed, beside the project's own. Restoring one moves it into the project | The Backup group may hold the only copy of a deleted map, and a project must be able to take it in |
| Two backups on one day | Second folder gets `_2`, `_3` and so on | Today the second overwrites the first |
| Name rules | ASCII letters, digits, `_`, `-`, `.`, no Windows reserved names, no trailing `.`, `.ds1` added if missing, table path at most 59 characters | Valid on all three platforms and fits the game's File column |
| Map size | 1 to 1000 each way | The reader rejects only overflow; this keeps a typo from allocating gigabytes |
| No as the delete box's default | Not done: Allegro's native box has no such option | Step 2's held-Enter wait covers the accidental route |

---

### Task 1: Editing LvlPrest.txt as text

**Files:**
- Modify: `src/core/lvlprest_text.c`, `src/core/lvlprest_text.h`
- Test: `test/test_lvlprest_text.c`

**Interfaces:**
- Consumes: `lvlprest_find_col`, `lvlprest_find_row`, `lvlprest_find_file_slot` (existing).
- Produces:
  - `int lvlprest_empty_slot(char *buf, int def)`: 1 to 6, 0 for none or no such Def, `LVLPREST_TABLE_UNUSABLE`.
  - `int lvlprest_get_cell(char *buf, int def, int slot, char *out, int cap)`: 1 if the cell was copied, else 0.
  - `char *lvlprest_with_cell(char *buf, int def, int slot, const char *value, long *out_len)`: new `malloc`'d NUL-terminated text with one cell replaced and all else byte-identical, or NULL.
  - `typedef void (*LVLPREST_ROW_CB)(int def, const char *ds1_path, void *ctx); void lvlprest_each_cleared(char *game, char *project, LVLPREST_ROW_CB cb, void *ctx)`: calls `cb` for each file the game's row lists that the same Def's row in `project` doesn't.

- [ ] **Step 1: Write the failing tests**

Add to `test/test_lvlprest_text.c`, and add each to `main` with `RUN_TEST`:

```c
void test_empty_slot_is_the_first_blank_or_zero_cell(void)
{
   TEST_ASSERT_EQUAL_INT(3, lvlprest_empty_slot(table, 1));
   TEST_ASSERT_EQUAL_INT(2, lvlprest_empty_slot(table, 167));
   TEST_ASSERT_EQUAL_INT(0, lvlprest_empty_slot(table, 999));
}

void test_empty_slot_is_none_when_all_six_are_used(void)
{
   char full[] = "Name\tDef\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "F\t5\ta.ds1\tb.ds1\tc.ds1\td.ds1\te.ds1\tf.ds1\r\n";
   TEST_ASSERT_EQUAL_INT(0, lvlprest_empty_slot(full, 5));
}

void test_get_cell_copies_one_cell(void)
{
   char out[64];
   TEST_ASSERT_TRUE(lvlprest_get_cell(table, 1, 2, out, sizeof(out)));
   TEST_ASSERT_EQUAL_STRING("Act1\\Town\\townE1.ds1", out);
   TEST_ASSERT_FALSE(lvlprest_get_cell(table, 999, 1, out, sizeof(out)));
}

void test_with_cell_changes_only_that_cell(void)
{
   long len = 0;
   char *out = lvlprest_with_cell(table, 167, 2, "Act1/New.ds1", &len);
   char expected[sizeof(table) + 32];

   TEST_ASSERT_NOT_NULL(out);
   snprintf(expected, sizeof(expected), "%s", table);
   {
      char *at = strstr(expected, "CourtWb.ds1\t0");
      char tail[sizeof(table)];
      TEST_ASSERT_NOT_NULL(at);
      at += strlen("CourtWb.ds1\t");
      snprintf(tail, sizeof(tail), "%s", at + 1);
      snprintf(at, sizeof(expected) - (at - expected), "Act1/New.ds1%s", tail);
   }
   TEST_ASSERT_EQUAL_STRING(expected, out);
   TEST_ASSERT_EQUAL_INT((int) strlen(expected), (int) len);
   free(out);
}

void test_with_cell_replaces_the_last_cell_of_a_file_with_no_newline(void)
{
   long len = 0;
   char *out = lvlprest_with_cell(table, 200, 6, "0", &len);

   TEST_ASSERT_NOT_NULL(out);
   TEST_ASSERT_EQUAL_INT(0, lvlprest_find_file_slot(out, 200, "Act2/Last.ds1"));
   TEST_ASSERT_EQUAL_INT('0', out[len - 1]);
   free(out);
}

static void count_cleared(int def, const char *path, void *ctx)
{
   char *log = ctx;
   char line[80];
   snprintf(line, sizeof(line), "%d:%s;", def, path);
   strcat(log, line);
}

void test_each_cleared_lists_game_files_the_project_row_lacks(void)
{
   char game[] = "Name\tDef\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "A\t1\tx.ds1\ty.ds1\t0\t0\t0\t0\r\n"
                 "B\t2\tz.ds1\t0\t0\t0\t0\t0\r\n";
   char proj[] = "Name\tDef\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "A\t1\tx.ds1\t0\tnew.ds1\t0\t0\t0\r\n"
                 "B\t2\tz.ds1\t0\t0\t0\t0\t0\r\n";
   char log[200] = "";

   lvlprest_each_cleared(game, proj, count_cleared, log);
   TEST_ASSERT_EQUAL_STRING("1:y.ds1;", log);
}
```

Add `#include <stdio.h>` and `#include <stdlib.h>` at the top.

- [ ] **Step 2: Run to see it fail**

Run: `cmake --build tmp/build-lifecycle --config Release --target test_lvlprest_text -- /m /v:q /nologo`
Expected: FAIL, `lvlprest_empty_slot` undefined.

- [ ] **Step 3: Implement**

Add to `lvlprest_text.h`, before `#endif`:

```c
/* The first File column (1-6) of this Def's row that is blank or "0". 0 if all
   six are used or the Def isn't there. LVLPREST_TABLE_UNUSABLE as above. */
int lvlprest_empty_slot(char * buf, int def);

/* Copy File column `slot` of this Def's row. 1 if copied, 0 if there's no such
   cell or it doesn't fit. */
int lvlprest_get_cell(char * buf, int def, int slot, char * out, int cap);

/* The table with one File cell replaced, every other byte as it was. New
   malloc'd text with a NUL after *out_len bytes; NULL if there's no such cell. */
char * lvlprest_with_cell(char * buf, int def, int slot, const char * value,
                          long * out_len);

/* Calls cb for each file a row of `game` lists that the same Def's row in
   `project` doesn't. A Def with no row in `project` is skipped. */
typedef void (* LVLPREST_ROW_CB)(int def, const char * ds1_path, void * ctx);
void lvlprest_each_cleared(char * game, char * project, LVLPREST_ROW_CB cb,
                           void * ctx);
```

Add to `lvlprest_text.c`. Refactor `lvlprest_find_file_slot`'s cell walk into `find_cell`, and have it use the helper:

```c
/* The bounds of File column `slot` in this Def's row. 1 if found. */
static int find_cell(char * buf, int def, int slot, char ** start, char ** end)
{
   char col_name[16];
   int def_col, file_col, col = 0;
   char * row, * row_end, * cell;

   if (slot < 1 || slot > 6)
      return 0;
   sprintf(col_name, "File%d", slot);
   def_col = lvlprest_find_col(buf, "Def");
   file_col = lvlprest_find_col(buf, col_name);
   if (def_col < 0 || file_col < 0)
      return 0;
   row = lvlprest_find_row(buf, def_col, def, &row_end);
   if (row == NULL)
      return 0;

   cell = row;
   while (col < file_col && *cell != '\0' && *cell != '\n' && *cell != '\r')
   {
      if (*cell == '\t') col++;
      cell++;
   }
   if (col != file_col)
      return 0;

   *start = cell;
   while (*cell != '\0' && *cell != '\t' && *cell != '\n' && *cell != '\r')
      cell++;
   *end = cell;
   return 1;
}

static int cell_is_empty(const char * start, const char * end)
{
   return start == end || (end - start == 1 && *start == '0');
}

int lvlprest_empty_slot(char * buf, int def)
{
   char * start, * end;
   int f;

   if (lvlprest_find_col(buf, "Def") < 0 || lvlprest_find_col(buf, "File1") < 0)
      return LVLPREST_TABLE_UNUSABLE;
   for (f = 1; f <= 6; f++)
      if (find_cell(buf, def, f, &start, &end) && cell_is_empty(start, end))
         return f;
   return 0;
}

int lvlprest_get_cell(char * buf, int def, int slot, char * out, int cap)
{
   char * start, * end;
   int n;

   if (!find_cell(buf, def, slot, &start, &end))
      return 0;
   n = (int)(end - start);
   if (n >= cap)
      return 0;
   memcpy(out, start, n);
   out[n] = '\0';
   return 1;
}

char * lvlprest_with_cell(char * buf, int def, int slot, const char * value,
                          long * out_len)
{
   char * start, * end, * out;
   long before, after, val = (long) strlen(value);

   if (!find_cell(buf, def, slot, &start, &end))
      return NULL;
   before = (long)(start - buf);
   after = (long) strlen(end);
   out = (char *) malloc(before + val + after + 1);
   if (out == NULL)
      return NULL;
   memcpy(out, buf, before);
   memcpy(out + before, value, val);
   memcpy(out + before + val, end, after + 1);
   *out_len = before + val + after;
   return out;
}

void lvlprest_each_cleared(char * game, char * project, LVLPREST_ROW_CB cb,
                           void * ctx)
{
   int def_col = lvlprest_find_col(game, "Def");
   int proj_def_col = lvlprest_find_col(project, "Def");
   char * line = game;
   int is_header = 1;

   if (def_col < 0 || proj_def_col < 0)
      return;

   while (*line != '\0')
   {
      char * next = line;
      while (*next != '\0' && *next != '\n' && *next != '\r')
         next++;

      if (!is_header)
      {
         char * p = line;
         char * row_end;
         int col = 0;

         while (col < def_col && p < next)
         {
            if (*p == '\t') col++;
            p++;
         }
         if (col == def_col)
         {
            int def = atoi(p);
            int f;

            if (def > 0 && lvlprest_find_row(project, proj_def_col, def, &row_end) != NULL)
            {
               for (f = 1; f <= 6; f++)
               {
                  char path[256];

                  if (!lvlprest_get_cell(game, def, f, path, sizeof(path)) ||
                      path[0] == '\0' || strcmp(path, "0") == 0)
                     continue;
                  if (lvlprest_find_file_slot(project, def, path) == 0)
                     cb(def, path, ctx);
               }
            }
         }
      }

      while (*next == '\n' || *next == '\r') next++;
      line = next;
      is_header = 0;
   }
}
```

- [ ] **Step 4: Run the tests**

Run: `ctest --test-dir tmp/build-lifecycle -C Release -R "^lvlprest_text$" --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core/lvlprest_text.c src/core/lvlprest_text.h test/test_lvlprest_text.c
git commit -m "Edit and compare LvlPrest.txt cells as text"
```

---

### Task 2: map_ops, the project's file and table work

**Files:**
- Create: `src/core/map_ops.c`, `src/core/map_ops.h`, `test/test_map_ops.c`
- Modify: `CMakeLists.txt` (add `src/core/map_ops.c` beside `src/core/lvlprest_text.c`), `test/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1; `project_ensure_parent_dirs`, `project_path_in_game_install`; `file_copy_checked`.
- Produces, all returning 0 on success and -1 with a sentence in `error`:

```c
int map_ops_check_name(const char *name, char *out_file, int out_cap, char *error, int cap);
int map_ops_check_size(int width, int height, char *error, int cap);
int map_ops_table_path(const char *project_dir, char *out, int cap);
int map_ops_tile_path(const char *project_dir, const char *ds1_path, char *out, int cap);
char *map_ops_read_file(const char *path, long *out_len);
int map_ops_write_empty_ds1(const char *path, int width, int height, int act);
int map_ops_register(const char *project_dir, const char *seed, int def,
                     const char *ds1_path, char *error, int cap);
int map_ops_unregister(const char *project_dir, const char *seed, int def,
                       const char *ds1_path, char *error, int cap);
int map_ops_restore_slot(const char *project_dir, const char *seed, char *game_table,
                         int def, const char *ds1_path, char *error, int cap);
int map_ops_create_empty(const char *project_dir, const char *seed, int def,
                         const char *ds1_path, int width, int height, int act,
                         char *error, int cap);
int map_ops_backup_file(const char *project_dir, const char *ds1_path,
                        const char *label, const char *date, const char *json,
                        char *out_ds1, int out_cap, char *error, int cap);
int map_ops_restore_file(const char *project_dir, const char *backup_ds1,
                         const char *ds1_path, char *error, int cap);
```

`seed` is the table text to copy in if the project has no `Global/Excel/LvlPrest.txt` yet; `NULL` means none is available.

- [ ] **Step 1: Write the failing test**

Create `test/test_map_ops.c`:

```c
/* map_ops over a temp project: tables, new maps, backups, and what a failure
 * leaves behind. Nothing here touches a game install. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "unity/unity.h"
#include "structs.h"
#include "core/lvlprest_text.h"
#include "core/map_ops.h"
#include "core/project.h"
#include "support/test_tmp.h"

CONFIG_S glb_config;

static char dir[512];
static char err[512];

static const char GAME[] =
   "Name\tDef\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
   "Town\t1\tAct1\\Town\\TownN1.ds1\t0\t0\t0\t0\t0\r\n"
   "Full\t2\ta.ds1\tb.ds1\tc.ds1\td.ds1\te.ds1\tf.ds1\r\n";

static int exists(const char *rel)
{
   char path[768];
   FILE *f;

   snprintf(path, sizeof(path), "%s/%s", dir, rel);
   f = fopen(path, "rb");
   if (f == NULL) return 0;
   fclose(f);
   return 1;
}

static char *table(void)
{
   char path[768];
   long len;

   snprintf(path, sizeof(path), "%s/Global/Excel/LvlPrest.txt", dir);
   return map_ops_read_file(path, &len);
}

void setUp(void)
{
   TEST_ASSERT_TRUE(test_tmp_dir(dir, sizeof(dir)));
   memset(&glb_config, 0, sizeof(glb_config));
   err[0] = 0;
}

void tearDown(void)
{
   test_tmp_remove(dir);
}

void test_names_get_the_extension_and_bad_ones_are_refused(void)
{
   char out[128];

   TEST_ASSERT_EQUAL_INT(0, map_ops_check_name("TownX", out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_EQUAL_STRING("TownX.ds1", out);
   TEST_ASSERT_EQUAL_INT(0, map_ops_check_name("a_b-c.DS1", out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_EQUAL_STRING("a_b-c.DS1", out);

   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_name("", out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_name("a/b", out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_name("a b", out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_name("..", out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_name("CON", out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_name("lpt1.ds1", out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_name("trailing.", out, sizeof(out), err, sizeof(err)));
}

void test_sizes_have_limits(void)
{
   TEST_ASSERT_EQUAL_INT(0, map_ops_check_size(1, 1000, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_size(0, 10, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_size(10, 1001, err, sizeof(err)));
}

void test_register_seeds_the_table_then_fills_a_slot(void)
{
   char *t;

   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, GAME, 1, "Act1/Town/New.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_NOT_NULL(t);
   TEST_ASSERT_EQUAL_INT(2, lvlprest_find_file_slot(t, 1, "Act1/Town/New.ds1"));
   free(t);
}

void test_register_with_no_table_and_no_seed_says_so(void)
{
   TEST_ASSERT_EQUAL_INT(-1, map_ops_register(dir, NULL, 1, "x.ds1", err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "LvlPrest.txt"));
}

void test_register_refuses_a_full_row_and_a_missing_def(void)
{
   TEST_ASSERT_EQUAL_INT(-1, map_ops_register(dir, GAME, 2, "x.ds1", err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "slot"));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_register(dir, GAME, 999, "x.ds1", err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "999"));
}

void test_unregister_then_restore_returns_the_table_to_the_original(void)
{
   char game[sizeof(GAME)];
   char *t;

   memcpy(game, GAME, sizeof(GAME));
   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, GAME, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_INT(0, lvlprest_find_file_slot(t, 1, "Act1/Town/TownN1.ds1"));
   free(t);

   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_slot(dir, GAME, game, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_STRING(GAME, t);
   free(t);
}

void test_restore_uses_another_slot_when_its_own_was_taken(void)
{
   char game[sizeof(GAME)];
   char *t;

   memcpy(game, GAME, sizeof(GAME));
   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, GAME, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, GAME, 1, "Act1/Town/New.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_slot(dir, GAME, game, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_INT(1, lvlprest_find_file_slot(t, 1, "New.ds1"));
   TEST_ASSERT_EQUAL_INT(2, lvlprest_find_file_slot(t, 1, "TownN1.ds1"));
   free(t);
}

void test_create_writes_the_map_and_registers_it(void)
{
   TEST_ASSERT_EQUAL_INT(0, map_ops_create_empty(dir, GAME, 1, "Act1/Town/New.ds1",
                                                 10, 20, 1, err, sizeof(err)));
   TEST_ASSERT_TRUE(exists("Global/Tiles/Act1/Town/New.ds1"));
}

void test_a_failed_table_write_removes_the_new_map(void)
{
   char blocked[768];
   FILE *f;

   /* A file where the Excel folder should be stops the table write. */
   snprintf(blocked, sizeof(blocked), "%s/Global", dir);
   TEST_ASSERT_TRUE(test_tmp_mkdir(blocked));
   snprintf(blocked, sizeof(blocked), "%s/Global/Excel", dir);
   f = fopen(blocked, "wb");
   TEST_ASSERT_NOT_NULL(f);
   fclose(f);

   TEST_ASSERT_EQUAL_INT(-1, map_ops_create_empty(dir, GAME, 1, "New.ds1", 10, 20, 1, err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Global/Tiles/New.ds1"));
}

void test_create_refuses_an_existing_file(void)
{
   TEST_ASSERT_EQUAL_INT(0, map_ops_create_empty(dir, GAME, 1, "New.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_create_empty(dir, GAME, 1, "New.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "already"));
}

void test_empty_map_header_matches_the_reader(void)
{
   char path[768];
   long len = 0;

   snprintf(path, sizeof(path), "%s/e.ds1", dir);
   TEST_ASSERT_EQUAL_INT(0, map_ops_write_empty_ds1(path, 3, 2, 4));
   {
      char *raw = map_ops_read_file(path, &len);
      int w[8];
      TEST_ASSERT_NOT_NULL(raw);
      memcpy(w, raw, sizeof(w));
      TEST_ASSERT_EQUAL_INT(18, w[0]);
      TEST_ASSERT_EQUAL_INT(2, w[1]);
      TEST_ASSERT_EQUAL_INT(1, w[2]);
      TEST_ASSERT_EQUAL_INT(3, w[3]);
      /* header 32, four layers of 3x2 cells, object and path counts */
      TEST_ASSERT_EQUAL_INT(32 + 4 * 4 * 6 + 8, (int) len);
      free(raw);
   }
}

void test_backup_moves_the_file_and_restore_brings_it_back(void)
{
   char out[512], again[512];

   TEST_ASSERT_EQUAL_INT(0, map_ops_create_empty(dir, GAME, 1, "Act1/New.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_backup_file(dir, "Act1/New.ds1", "Act1-Town", "2026-11-02",
                                                "{}\n", out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Global/Tiles/Act1/New.ds1"));
   TEST_ASSERT_TRUE(exists("backup/2026-11-02_Act1-Town_New/New.ds1"));
   TEST_ASSERT_TRUE(exists("backup/2026-11-02_Act1-Town_New/New.json"));

   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_file(dir, out, "Act1/New.ds1", err, sizeof(err)));
   TEST_ASSERT_TRUE(exists("Global/Tiles/Act1/New.ds1"));
   TEST_ASSERT_FALSE(exists("backup/2026-11-02_Act1-Town_New/New.ds1"));

   /* Backed up, recreated and backed up again the same day: the first stays. */
   TEST_ASSERT_EQUAL_INT(0, map_ops_backup_file(dir, "Act1/New.ds1", "Act1-Town", "2026-11-02",
                                                "{}\n", again, sizeof(again), err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_create_empty(dir, GAME, 1, "Act1/New.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_backup_file(dir, "Act1/New.ds1", "Act1-Town", "2026-11-02",
                                                "{}\n", out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_TRUE(strcmp(again, out) != 0);
   TEST_ASSERT_TRUE(exists("backup/2026-11-02_Act1-Town_New_2/New.ds1"));
}

void test_backup_with_nothing_to_back_up_stops_with_a_message(void)
{
   char out[512];

   TEST_ASSERT_EQUAL_INT(-1, map_ops_backup_file(dir, "Act1/None.ds1", "Act1", "2026-11-02",
                                                 "{}\n", out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "None.ds1"));
   TEST_ASSERT_FALSE(exists("backup/2026-11-02_Act1_None/None.json"));
}

void test_restore_refuses_to_replace_a_file(void)
{
   char out[512];

   TEST_ASSERT_EQUAL_INT(0, map_ops_create_empty(dir, GAME, 1, "New.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_backup_file(dir, "New.ds1", "A", "2026-11-02", "{}\n",
                                                out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_create_empty(dir, GAME, 1, "New.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_restore_file(dir, out, "New.ds1", err, sizeof(err)));
   TEST_ASSERT_TRUE(exists("backup/2026-11-02_A_New/New.ds1"));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_names_get_the_extension_and_bad_ones_are_refused);
   RUN_TEST(test_sizes_have_limits);
   RUN_TEST(test_register_seeds_the_table_then_fills_a_slot);
   RUN_TEST(test_register_with_no_table_and_no_seed_says_so);
   RUN_TEST(test_register_refuses_a_full_row_and_a_missing_def);
   RUN_TEST(test_unregister_then_restore_returns_the_table_to_the_original);
   RUN_TEST(test_restore_uses_another_slot_when_its_own_was_taken);
   RUN_TEST(test_create_writes_the_map_and_registers_it);
   RUN_TEST(test_a_failed_table_write_removes_the_new_map);
   RUN_TEST(test_create_refuses_an_existing_file);
   RUN_TEST(test_empty_map_header_matches_the_reader);
   RUN_TEST(test_backup_moves_the_file_and_restore_brings_it_back);
   RUN_TEST(test_backup_with_nothing_to_back_up_stops_with_a_message);
   RUN_TEST(test_restore_refuses_to_replace_a_file);
   return UNITY_END();
}
```

Before writing it, read `test/support/test_tmp.h`. If its helpers have other names than `test_tmp_dir`, `test_tmp_remove` and `test_tmp_mkdir`, use its names, keeping the same meaning.

Add to `test/CMakeLists.txt`, after the `lvlprest_text` test:

```cmake
# Creating, registering and backing up maps in a project folder
add_executable(test_map_ops test_map_ops.c
    support/test_tmp.c
    ../src/core/map_ops.c ../src/core/lvlprest_text.c ../src/core/file_copy.c
    ../src/core/project.c ../src/core/preferences.c)
target_link_libraries(test_map_ops PRIVATE unity ds1edit_allegro)
target_include_directories(test_map_ops PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/test)
target_compile_definitions(test_map_ops PRIVATE DS1EDIT_VERSION_STR="${PROJECT_VERSION}")
add_test(NAME map_ops COMMAND test_map_ops)
```

Add `test_map_ops` to the `DS1EDIT_ALLEGRO_TESTS` list near line 336, so it gets the same runtime handling as `test_project`, and give it the `portable` label the way that list's other members get theirs. If `test_tmp` isn't a standalone `.c`, follow how `test_ds1_save` links it.

- [ ] **Step 2: Run to see it fail**

Run: `cmake --build tmp/build-lifecycle --config Release --target test_map_ops -- /m /v:q /nologo`
Expected: FAIL, `core/map_ops.h` not found.

- [ ] **Step 3: Implement**

Create `src/core/map_ops.h`:

```c
/* Creating, registering and backing up maps in a project folder. Everything
 * takes explicit paths and reports problems as a sentence in `error`, so it can
 * run without a display or a loaded game. Functions return 0, or -1 with
 * nothing changed unless a comment says otherwise.
 *
 * `seed` is the LvlPrest.txt text to start from if the project has no copy yet.
 */
#ifndef DS1EDIT_MAP_OPS_H
#define DS1EDIT_MAP_OPS_H

/* The longest table path that fits the game's File columns. */
#define MAP_OPS_TABLE_PATH_MAX 59
#define MAP_OPS_SIZE_MAX 1000

int map_ops_check_name(const char * name, char * out_file, int out_cap,
                       char * error, int cap);
int map_ops_check_size(int width, int height, char * error, int cap);

int map_ops_table_path(const char * project_dir, char * out, int cap);
int map_ops_tile_path(const char * project_dir, const char * ds1_path,
                      char * out, int cap);

/* A whole file as malloc'd text with a NUL after *out_len bytes, or NULL. */
char * map_ops_read_file(const char * path, long * out_len);

int map_ops_write_empty_ds1(const char * path, int width, int height, int act);

/* Put ds1_path in this Def's first empty File column. */
int map_ops_register(const char * project_dir, const char * seed, int def,
                     const char * ds1_path, char * error, int cap);
/* Clear the column naming ds1_path. A map the row doesn't list is not an error. */
int map_ops_unregister(const char * project_dir, const char * seed, int def,
                       const char * ds1_path, char * error, int cap);
/* Put the game's cell for ds1_path back: in its own column, or the first empty
 * one if that was taken. */
int map_ops_restore_slot(const char * project_dir, const char * seed, char * game_table,
                         int def, const char * ds1_path, char * error, int cap);

/* Write an empty map and register it. If registering fails, the map is removed. */
int map_ops_create_empty(const char * project_dir, const char * seed, int def,
                         const char * ds1_path, int width, int height, int act,
                         char * error, int cap);

/* Move the project's copy of ds1_path to backup/<date>_<label>_<name>[_N]/, with
 * <name>.json holding `json`. Sets out_ds1 to the backup's path. The original is
 * removed last, after the copy and the JSON are in place. */
int map_ops_backup_file(const char * project_dir, const char * ds1_path,
                        const char * label, const char * date, const char * json,
                        char * out_ds1, int out_cap, char * error, int cap);

/* Move a backup to the project's copy of ds1_path. Refuses to replace a file. */
int map_ops_restore_file(const char * project_dir, const char * backup_ds1,
                         const char * ds1_path, char * error, int cap);

#endif
```

Create `src/core/map_ops.c`:

```c
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "core/file_copy.h"
#include "core/lvlprest_text.h"
#include "core/map_ops.h"
#include "core/project.h"

#define PATH_CAP 768

static int fail(char * error, int cap, const char * fmt, ...)
{
   va_list ap;

   if (error != NULL && cap > 0)
   {
      va_start(ap, fmt);
      vsnprintf(error, (size_t) cap, fmt, ap);
      va_end(ap);
   }
   return -1;
}

static int file_exists(const char * path)
{
   FILE * f = fopen(path, "rb");

   if (f == NULL)
      return 0;
   fclose(f);
   return 1;
}

static const char * base_name(const char * path)
{
   const char * base = path, * p;

   for (p = path; *p != '\0'; p++)
      if (*p == '/' || *p == '\\')
         base = p + 1;
   return base;
}

int map_ops_check_name(const char * name, char * out_file, int out_cap,
                       char * error, int cap)
{
   static const char * const reserved[] = {
      "con", "prn", "aux", "nul",
      "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
      "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"
   };
   size_t n = strlen(name), stem, i;
   const char * dot;

   if (n == 0)
      return fail(error, cap, "The map needs a name.");
   for (i = 0; i < n; i++)
   {
      unsigned char c = (unsigned char) name[i];
      if (!isalnum(c) && c != '_' && c != '-' && c != '.')
         return fail(error, cap, "Names can use letters, digits, _ - and . only.");
   }
   if (name[0] == '.' || name[n - 1] == '.')
      return fail(error, cap, "A name can't start or end with a dot.");

   dot = strchr(name, '.');
   stem = dot != NULL ? (size_t)(dot - name) : n;
   for (i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++)
      if (strlen(reserved[i]) == stem && strncasecmp(name, reserved[i], stem) == 0)
         return fail(error, cap, "%s is a reserved name on Windows.", reserved[i]);

   if (n >= 4 && strcasecmp(name + n - 4, ".ds1") == 0)
      snprintf(out_file, (size_t) out_cap, "%s", name);
   else
      snprintf(out_file, (size_t) out_cap, "%s.ds1", name);
   return 0;
}

int map_ops_check_size(int width, int height, char * error, int cap)
{
   if (width < 1 || height < 1 || width > MAP_OPS_SIZE_MAX || height > MAP_OPS_SIZE_MAX)
      return fail(error, cap, "Width and height must be 1 to %d.", MAP_OPS_SIZE_MAX);
   return 0;
}

int map_ops_table_path(const char * project_dir, char * out, int cap)
{
   int n = snprintf(out, (size_t) cap, "%s/Global/Excel/LvlPrest.txt", project_dir);
   return n > 0 && n < cap ? 0 : -1;
}

int map_ops_tile_path(const char * project_dir, const char * ds1_path, char * out, int cap)
{
   int n = snprintf(out, (size_t) cap, "%s/Global/Tiles/%s", project_dir, ds1_path);
   char * p;

   if (n <= 0 || n >= cap)
      return -1;
   for (p = out; *p != '\0'; p++)
      if (*p == '\\')
         *p = '/';
   return 0;
}

char * map_ops_read_file(const char * path, long * out_len)
{
   FILE * f = fopen(path, "rb");
   char * buf;
   long len;

   if (f == NULL)
      return NULL;
   fseek(f, 0, SEEK_END);
   len = ftell(f);
   fseek(f, 0, SEEK_SET);
   buf = (char *) malloc(len + 1);
   if (buf != NULL && fread(buf, 1, (size_t) len, f) != (size_t) len)
   {
      free(buf);
      buf = NULL;
   }
   fclose(f);
   if (buf != NULL)
   {
      buf[len] = '\0';
      *out_len = len;
   }
   return buf;
}

/* Replace a file only once the new bytes are safely beside it. */
static int write_whole_file(const char * path, const char * data, long len)
{
   char staging[PATH_CAP];
   FILE * f;

   if (snprintf(staging, sizeof(staging), "%s.staging", path) >= (int) sizeof(staging))
      return -1;
   if (!project_ensure_parent_dirs(path))
      return -1;
   f = fopen(staging, "wb");
   if (f == NULL)
      return -1;
   if (fwrite(data, 1, (size_t) len, f) != (size_t) len)
   {
      fclose(f);
      remove(staging);
      return -1;
   }
   if (fclose(f) != 0)
   {
      remove(staging);
      return -1;
   }
   remove(path);
   if (rename(staging, path) != 0)
   {
      remove(staging);
      return -1;
   }
   return 0;
}

/* The project's table, seeding it first if there isn't one. */
static char * table_for_edit(const char * project_dir, const char * seed,
                             long * out_len, char * error, int cap)
{
   char path[PATH_CAP];
   char * text;

   if (map_ops_table_path(project_dir, path, sizeof(path)) != 0)
   {
      fail(error, cap, "The project's path is too long for LvlPrest.txt.");
      return NULL;
   }
   text = map_ops_read_file(path, out_len);
   if (text != NULL)
      return text;
   if (seed == NULL)
   {
      fail(error, cap, "The project has no LvlPrest.txt, and the game's couldn't be read.");
      return NULL;
   }
   if (write_whole_file(path, seed, (long) strlen(seed)) != 0)
   {
      fail(error, cap, "Couldn't write %s.", path);
      return NULL;
   }
   return map_ops_read_file(path, out_len);
}

static int save_table(const char * project_dir, const char * text, long len,
                      char * error, int cap)
{
   char path[PATH_CAP];

   if (map_ops_table_path(project_dir, path, sizeof(path)) != 0 ||
       write_whole_file(path, text, len) != 0)
      return fail(error, cap, "Couldn't write the project's LvlPrest.txt.");
   return 0;
}

static int set_cell_and_save(const char * project_dir, char * table, int def, int slot,
                             const char * value, char * error, int cap)
{
   long len = 0;
   char * edited = lvlprest_with_cell(table, def, slot, value, &len);
   int rc;

   if (edited == NULL)
      return fail(error, cap, "Couldn't edit Def %d's row in LvlPrest.txt.", def);
   rc = save_table(project_dir, edited, len, error, cap);
   free(edited);
   return rc;
}

int map_ops_register(const char * project_dir, const char * seed, int def,
                     const char * ds1_path, char * error, int cap)
{
   long len;
   char * table;
   int slot, rc, def_col;
   char * row_end;

   if ((int) strlen(ds1_path) > MAP_OPS_TABLE_PATH_MAX)
      return fail(error, cap, "%s is longer than the %d characters LvlPrest.txt allows.",
                  ds1_path, MAP_OPS_TABLE_PATH_MAX);

   table = table_for_edit(project_dir, seed, &len, error, cap);
   if (table == NULL)
      return -1;

   def_col = lvlprest_find_col(table, "Def");
   if (def_col < 0 || lvlprest_find_row(table, def_col, def, &row_end) == NULL)
   {
      free(table);
      return fail(error, cap, "Def %d isn't in LvlPrest.txt.", def);
   }
   slot = lvlprest_empty_slot(table, def);
   if (slot <= 0)
   {
      free(table);
      return fail(error, cap, "Def %d has no free File slot in LvlPrest.txt.", def);
   }
   rc = set_cell_and_save(project_dir, table, def, slot, ds1_path, error, cap);
   free(table);
   return rc;
}

int map_ops_unregister(const char * project_dir, const char * seed, int def,
                       const char * ds1_path, char * error, int cap)
{
   long len;
   char * table = table_for_edit(project_dir, seed, &len, error, cap);
   int slot, rc = 0;

   if (table == NULL)
      return -1;
   slot = lvlprest_find_file_slot(table, def, ds1_path);
   if (slot == LVLPREST_TABLE_UNUSABLE)
      rc = fail(error, cap, "The project's LvlPrest.txt has no Def or File columns.");
   else if (slot > 0)
      rc = set_cell_and_save(project_dir, table, def, slot, "0", error, cap);
   free(table);
   return rc;
}

int map_ops_restore_slot(const char * project_dir, const char * seed, char * game_table,
                         int def, const char * ds1_path, char * error, int cap)
{
   long len;
   char * table;
   char cell[256], current[256];
   int game_slot, slot, rc;

   game_slot = lvlprest_find_file_slot(game_table, def, ds1_path);
   if (game_slot <= 0 || !lvlprest_get_cell(game_table, def, game_slot, cell, sizeof(cell)))
      return fail(error, cap, "The game's LvlPrest.txt doesn't list %s under Def %d.",
                  ds1_path, def);

   table = table_for_edit(project_dir, seed, &len, error, cap);
   if (table == NULL)
      return -1;

   slot = game_slot;
   if (!lvlprest_get_cell(table, def, slot, current, sizeof(current)) ||
       (current[0] != '\0' && strcmp(current, "0") != 0))
      slot = lvlprest_empty_slot(table, def);
   if (slot <= 0)
   {
      free(table);
      return fail(error, cap, "Def %d has no free File slot in LvlPrest.txt.", def);
   }
   rc = set_cell_and_save(project_dir, table, def, slot, cell, error, cap);
   free(table);
   return rc;
}

static int write_le32(FILE * f, int v)
{
   unsigned char b[4];

   b[0] = (unsigned char) (v & 0xFF);
   b[1] = (unsigned char) ((v >> 8) & 0xFF);
   b[2] = (unsigned char) ((v >> 16) & 0xFF);
   b[3] = (unsigned char) ((v >> 24) & 0xFF);
   return fwrite(b, 1, 4, f) == 4;
}

int map_ops_write_empty_ds1(const char * path, int width, int height, int act)
{
   FILE * f;
   int ok, i, cells = width * height;

   if (!project_ensure_parent_dirs(path))
      return -1;
   f = fopen(path, "wb");
   if (f == NULL)
      return -1;

   ok = write_le32(f, 18) &&          /* version */
        write_le32(f, width - 1) &&
        write_le32(f, height - 1) &&
        write_le32(f, act - 1) &&
        write_le32(f, 0) &&           /* tag type: none */
        write_le32(f, 0) &&           /* no embedded file names */
        write_le32(f, 1) &&           /* wall layers */
        write_le32(f, 1);             /* floor layers */

   /* wall properties, wall orientations, floor properties, shadow properties */
   for (i = 0; ok && i < cells * 4; i++)
      ok = write_le32(f, 0);

   ok = ok && write_le32(f, 0) &&     /* objects */
        write_le32(f, 0);             /* NPC paths */

   if (fclose(f) != 0)
      ok = 0;
   if (!ok)
   {
      remove(path);
      return -1;
   }
   return 0;
}

int map_ops_create_empty(const char * project_dir, const char * seed, int def,
                         const char * ds1_path, int width, int height, int act,
                         char * error, int cap)
{
   char path[PATH_CAP];

   if (map_ops_check_size(width, height, error, cap) != 0)
      return -1;
   if (map_ops_tile_path(project_dir, ds1_path, path, sizeof(path)) != 0)
      return fail(error, cap, "The path for %s is too long.", ds1_path);
   if (file_exists(path))
      return fail(error, cap, "%s already exists in the project.", ds1_path);
   if (map_ops_write_empty_ds1(path, width, height, act) != 0)
      return fail(error, cap, "Couldn't write %s.", path);
   if (map_ops_register(project_dir, seed, def, ds1_path, error, cap) != 0)
   {
      remove(path);
      return -1;
   }
   return 0;
}

/* The first backup folder not already there. */
static int free_backup_dir(const char * project_dir, const char * date, const char * label,
                           const char * stem, const char * file, char * out, int out_cap)
{
   int i, n;

   for (i = 1; i < 1000; i++)
   {
      char probe[PATH_CAP + 16];

      if (i == 1)
         n = snprintf(out, (size_t) out_cap, "%s/backup/%s_%s_%s",
                      project_dir, date, label, stem);
      else
         n = snprintf(out, (size_t) out_cap, "%s/backup/%s_%s_%s_%d",
                      project_dir, date, label, stem, i);
      if (n <= 0 || n >= out_cap)
         return -1;
      snprintf(probe, sizeof(probe), "%s/%s", out, file);
      if (!file_exists(probe))
         return 0;
   }
   return -1;
}

int map_ops_backup_file(const char * project_dir, const char * ds1_path,
                        const char * label, const char * date, const char * json,
                        char * out_ds1, int out_cap, char * error, int cap)
{
   char src[PATH_CAP], dir[PATH_CAP], json_path[PATH_CAP + 16], stem[128], dst[PATH_CAP + 16];
   const char * file = base_name(ds1_path);
   const char * dot = strrchr(file, '.');
   size_t stem_len = dot != NULL ? (size_t)(dot - file) : strlen(file);

   if (stem_len >= sizeof(stem))
      return fail(error, cap, "The name %s is too long to back up.", file);
   memcpy(stem, file, stem_len);
   stem[stem_len] = '\0';

   if (map_ops_tile_path(project_dir, ds1_path, src, sizeof(src)) != 0)
      return fail(error, cap, "The path for %s is too long.", ds1_path);
   if (!file_exists(src))
      return fail(error, cap, "The project has no file for %s, so there's nothing to back up.",
                  ds1_path);

   if (free_backup_dir(project_dir, date, label, stem, file, dir, sizeof(dir)) != 0)
      return fail(error, cap, "Couldn't pick a backup folder for %s.", ds1_path);
   snprintf(dst, sizeof(dst), "%s/%s", dir, file);
   snprintf(json_path, sizeof(json_path), "%s/%s.json", dir, stem);
   if ((int) strlen(dst) >= out_cap)
      return fail(error, cap, "The backup path for %s is too long.", ds1_path);
   if (!project_ensure_parent_dirs(dst))
      return fail(error, cap, "Couldn't create %s.", dir);

   if (file_copy_checked(src, dst) != FILE_COPY_OK)
      return fail(error, cap, "Couldn't copy %s to %s.", src, dst);
   if (write_whole_file(json_path, json, (long) strlen(json)) != 0)
   {
      remove(dst);
      return fail(error, cap, "Couldn't write %s.", json_path);
   }
   if (remove(src) != 0)
   {
      remove(dst);
      remove(json_path);
      return fail(error, cap, "Couldn't remove %s, so it wasn't moved to the backup.", src);
   }
   snprintf(out_ds1, (size_t) out_cap, "%s", dst);
   return 0;
}

int map_ops_restore_file(const char * project_dir, const char * backup_ds1,
                         const char * ds1_path, char * error, int cap)
{
   char dst[PATH_CAP];

   if (map_ops_tile_path(project_dir, ds1_path, dst, sizeof(dst)) != 0)
      return fail(error, cap, "The path for %s is too long.", ds1_path);
   if (file_exists(dst))
      return fail(error, cap, "The project already has a file for %s.", ds1_path);
   if (!project_ensure_parent_dirs(dst) || file_copy_checked(backup_ds1, dst) != FILE_COPY_OK)
      return fail(error, cap, "Couldn't copy %s to %s.", backup_ds1, dst);
   if (remove(backup_ds1) != 0)
      return fail(error, cap, "%s was restored, but its backup couldn't be removed.", ds1_path);
   return 0;
}
```

`strcasecmp` and `strncasecmp` come from `platform.h`, as `lvlprest_text.c` already relies on. Note that `test_unregister_then_restore_returns_the_table_to_the_original` passes a table path with `/` while the game cell has `\`: `lvlprest_find_file_slot` matches by file name, and the restored cell uses the game's own text, so the table comes back byte for byte.

- [ ] **Step 4: Run the tests**

Run: `ctest --test-dir tmp/build-lifecycle -C Release -R "^map_ops$" --output-on-failure`
Expected: PASS. Reconfigure first if CMake hasn't picked up the new target.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/core/map_ops.c src/core/map_ops.h test/CMakeLists.txt test/test_map_ops.c
git commit -m "Add project-folder map operations with temp-folder tests"
```

---

### Task 3: Saving a copy of the map in memory

**Files:**
- Modify: `src/core/ds1_save_io.c`, `src/core/ds1_save_io.h`
- Test: `test/test_ds1_save.c`

**Interfaces:**
- Produces: `int ds1_save_copy_checked(int ds1_idx, const char *dest, char *error, size_t error_cap);`. Writes the map as it is in memory to a new file at `dest`, with no change to its name, save counter or unsaved state. Refuses an existing `dest` or one inside the game install.

- [ ] **Step 1: Write the failing test**

Add to `test/test_ds1_save.c`, following its fixtures. First read how an existing test such as the first copy-on-save test builds and loads a map; use the same helpers for "load a fixture map into slot 0" (shown as `load_fixture_map` below, replace it with that file's helper):

```c
void test_save_copy_writes_memory_and_leaves_the_map_alone(void)
{
   char dest[512], name_before[sizeof(glb_ds1[0].name)];
   UDWORD count_before;

   load_fixture_map(0);
   snprintf(dest, sizeof(dest), "%s/copy.ds1", test_tmp_path());
   map_dirty_mark(0);
   snprintf(name_before, sizeof(name_before), "%s", glb_ds1[0].name);
   count_before = glb_ds1[0].save_count;

   TEST_ASSERT_EQUAL_INT(0, ds1_save_copy_checked(0, dest, err, sizeof(err)));

   TEST_ASSERT_EQUAL_STRING(name_before, glb_ds1[0].name);
   TEST_ASSERT_EQUAL_UINT32(count_before, glb_ds1[0].save_count);
   TEST_ASSERT_TRUE(map_is_dirty(0));
   TEST_ASSERT_TRUE(path_exists_for_test(dest));
}

void test_save_copy_refuses_to_replace_a_file(void)
{
   char dest[512];

   load_fixture_map(0);
   snprintf(dest, sizeof(dest), "%s/copy.ds1", test_tmp_path());
   TEST_ASSERT_EQUAL_INT(0, ds1_save_copy_checked(0, dest, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, ds1_save_copy_checked(0, dest, err, sizeof(err)));
}
```

Use that file's own helpers for the temp path and file-exists check. Add both to `main`.

- [ ] **Step 2: Run to see it fail**

Run: `cmake --build tmp/build-lifecycle --config Release --target test_ds1_save -- /m /v:q /nologo`
Expected: FAIL, `ds1_save_copy_checked` undefined.

- [ ] **Step 3: Implement**

Add to `ds1_save_io.h`:

```c
/* Write the map as it is in memory to a new file at `dest`. Its name, save
 * counter and unsaved state don't change. Refuses a file that exists, a dest in
 * the Diablo II install, and pre-version-4 maps, as a real save does. */
int ds1_save_copy_checked(int ds1_idx, const char * dest, char * error, size_t error_cap);
```

Add to `ds1_save_io.c`, after `ds1_save_with_io`:

```c
int ds1_save_copy_checked(int ds1_idx, const char * dest, char * error, size_t error_cap)
{
   DS1_WRITER w;
   char       staging[sizeof(glb_ds1[0].name) + 16 + sizeof(DS1_SAVE_STAGING_SUFFIX)];
   int        close_rc;

   if (error != NULL && error_cap > 0)
      error[0] = 0;
   if ((ds1_idx < 0) || (ds1_idx >= DS1_MAX) || (strlen(glb_ds1[ds1_idx].name) == 0))
      return fail(error, error_cap, "no map to copy%s%s", "", NULL);
   if (glb_ds1[ds1_idx].version < 4)
      return fail(error, error_cap, "%s uses an old DS1 format (before version 4) that "
                  "this editor cannot save without losing its tag layer%s",
                  glb_ds1[ds1_idx].name, NULL);
   if (project_path_in_game_install(dest))
      return fail(error, error_cap, "%s is inside the Diablo II install, and saved maps "
                  "must never touch the game files%s", dest, NULL);
   if (path_exists(dest))
      return fail(error, error_cap, "%s already exists%s", dest, NULL);
   if (snprintf(staging, sizeof(staging), "%s%s", dest, DS1_SAVE_STAGING_SUFFIX) >=
       (int) sizeof(staging))
      return fail(error, error_cap, "the path %s is too long%s", dest, NULL);
   if (! project_ensure_parent_dirs(dest))
      return fail(error, error_cap, "can't create the folder for %s%s", dest, NULL);

   w.io = & default_io;
   w.ok = 1;
   w.f  = w.io->open(staging, "wb");
   if (w.f == NULL)
      return fail(error, error_cap, "can't write %s%s", staging, NULL);
   ds1_serialize(& w, ds1_idx, glb_ds1[ds1_idx].save_count);
   close_rc = w.io->close(w.f);
   if (! w.ok || close_rc != 0)
   {
      w.io->remove(staging);
      return fail(error, error_cap, "writing %s failed (disk full or I/O error)%s",
                  staging, NULL);
   }
   if (w.io->rename(staging, dest) != 0)
   {
      w.io->remove(staging);
      return fail(error, error_cap, "couldn't create %s%s", dest, NULL);
   }
   return 0;
}
```

Match `fail`'s real signature in that file (it takes a format with two string arguments, as its callers show). Check `ds1_serialize` doesn't change map state; if it does, restore it here and say so in your report.

- [ ] **Step 4: Run the tests**

Run: `ctest --test-dir tmp/build-lifecycle -C Release -R "ds1_save" --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core/ds1_save_io.c src/core/ds1_save_io.h test/test_ds1_save.c
git commit -m "Save a copy of the map in memory without touching its state"
```

---

### Task 4: Browser and manager work by state

**Files:**
- Modify: `src/structs.h` (`AREA_DS1_ENTRY_S`), `src/core/area_browser.c`, `src/core/area_browser.h`, `src/core/ds1_manager.c`, `src/core/ds1_manager.h`, `src/ui/interface.c`, `test/python/test_delete_key_routing.py`

**Interfaces:**
- Consumes: Tasks 1 to 3.
- Produces, in `ds1_manager.h`, all returning 0, or -1 with a sentence in `error`:

```c
int ds1_manager_delete(int group_idx, int entry_idx, char *error, int cap);        /* New */
int ds1_manager_revert(int group_idx, int entry_idx, char *error, int cap);        /* Modified */
int ds1_manager_remove_slot(int group_idx, int entry_idx, char *error, int cap);   /* Game */
int ds1_manager_restore_slot(int group_idx, int entry_idx, char *error, int cap);  /* Removed */
int ds1_manager_restore(int group_idx, int entry_idx, char *error, int cap);       /* Backup row */
int ds1_manager_delete_permanent(int group_idx, int entry_idx);
int ds1_manager_create_empty(int group_idx, int def, const char *file_name,
                             int width, int height, char *error, int cap);
int ds1_manager_clone(int src_ds1_idx, int group_idx, int def, const char *file_name,
                      char *error, int cap);
int ds1_manager_name_in_use(int group_idx, const char *file_name);
```

- In `area_browser.h`: `int area_browser_find_row(const char *ds1_path, int def, int *group_idx, int *entry_idx);` and `void area_browser_rescan_backups(void);`.
- `AREA_DS1_ENTRY_S` gains `int slot_cleared;`.
- Removes `ds1_manager_backup`, `ds1_manager_write_backup_json`, `ds1_manager_lvlprest_set_file`, `ds1_manager_lvlprest_clear_file` and `ds1_manager_lvlprest_find_empty_slot`. Before removing each, `grep -rn` for other callers; keep any `props_panel.c` needs. `ds1_manager_suggest_name` and `ds1_manager_txt_set_cell` stay.

No new unit test here: `map_ops` holds the logic, and this task connects it to browser state. Its proof is the pinned-order Python test below, and Task 7's GUI pass.

- [ ] **Step 1: Update the pinned-order test first**

In `test/python/test_delete_key_routing.py`, replace `MapDeleteOrderTests` with:

```python
class MapDeleteOrderTests(unittest.TestCase):
    """Deleting a New map backs it up, and then clears its slot."""

    def setUp(self):
        manager = (ROOT / "src" / "core" / "ds1_manager.c").read_text(encoding="utf-8")
        match = re.search(r"^int ds1_manager_delete\([^)]*\)\n\{\n(.*?)^\}", manager,
                          re.MULTILINE | re.DOTALL)
        self.body = match.group(1)

    def test_the_backup_comes_before_the_table_changes(self):
        steps = [self.body.index(step) for step in (
            "lvlprest_find_file_slot(", "map_ops_backup_file(", "map_ops_unregister(")]
        self.assertEqual(steps, sorted(steps))

    def test_a_failed_table_update_puts_the_file_back(self):
        after = self.body[self.body.index("map_ops_unregister("):]
        self.assertIn("map_ops_restore_file(", after)
```

Run: `ctest --test-dir tmp/build-lifecycle -C Release -R "delete_key" --output-on-failure`
Expected: FAIL.

- [ ] **Step 2: Add the entry field and browser helpers**

In `src/structs.h`, `AREA_DS1_ENTRY_S`, after `state`:

```c
   int  slot_cleared;   /* in the game's table, but cleared in the project's */
```

In `area_group_add_entry`, set `e->slot_cleared = 0;`.

In `area_browser_refresh_states`, replace the loop body so that Removed rows win and a backup or no-project row stays Game:

```c
         if (g->is_backup || !glb_project.is_open)
            e->state = MAP_STATE_GAME;
         else if (e->slot_cleared)
            e->state = MAP_STATE_REMOVED;
         else if (!project_has_map(e->ds1_path))
            e->state = MAP_STATE_GAME;
         else
            e->state = map_state_classify(game_has_map(e->ds1_path), 1, 0);
```

Add to `area_browser.c` and declare in `area_browser.h`:

```c
int area_browser_find_row(const char * ds1_path, int def, int * group_idx, int * entry_idx)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int i, j;

   for (i = 0; i < ab->group_count; i++)
   {
      if (ab->groups[i].is_backup)
         continue;
      for (j = 0; j < ab->groups[i].entry_count; j++)
      {
         AREA_DS1_ENTRY_S * e = &ab->groups[i].entries[j];
         if (e->lvlprest_def == def && map_state_path_matches(e->ds1_path, ds1_path))
         {
            *group_idx = i;
            *entry_idx = j;
            return 1;
         }
      }
   }
   return 0;
}
```

Make `area_browser_reload` keep the open map's row. After `rc = area_browser_init();`, replace the two `-1` assignments with:

```c
   ab->loaded_group = -1;
   ab->loaded_entry = -1;
   if (glb_ds1edit.has_loaded_ds1 && glb_ds1[0].name[0] != '\0')
      area_browser_adopt_open_map(0);
```

with this helper above it:

```c
/* After the rows are rebuilt, find the row of the map open in this slot. */
static void area_browser_adopt_open_map(int ds1_idx)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int i, j;

   for (i = 0; i < ab->group_count; i++)
   {
      if (ab->groups[i].is_backup)
         continue;
      for (j = 0; j < ab->groups[i].entry_count; j++)
      {
         if (map_state_path_matches(glb_ds1[ds1_idx].name, ab->groups[i].entries[j].ds1_path))
         {
            ab->loaded_group = i;
            ab->loaded_entry = j;
            return;
         }
      }
   }
}
```

This fixes the step 2 finding that opening a project at the first-save prompt dims Delete and Clone until the map is reopened.

- [ ] **Step 3: Remember which groups a Def belongs to, and add Removed rows**

In `area_browser.c`, add above `area_browser_build`:

```c
/* Which groups each LvlPrest row went to during this build, including rows with
 * no file left, so a Removed map can be put back in its own group. */
typedef struct { int def; int lvltype_id; } DEF_GROUP;
static DEF_GROUP * s_def_groups;
static int s_def_group_count, s_def_group_max;

static void area_note_def_group(int def, int lvltype_id)
{
   int i;

   for (i = 0; i < s_def_group_count; i++)
      if (s_def_groups[i].def == def && s_def_groups[i].lvltype_id == lvltype_id)
         return;
   if (s_def_group_count >= s_def_group_max)
   {
      int new_max = s_def_group_max > 0 ? s_def_group_max * 2 : 256;
      DEF_GROUP * tmp = (DEF_GROUP *) realloc(s_def_groups, sizeof(DEF_GROUP) * new_max);
      if (tmp == NULL)
         return;
      s_def_groups = tmp;
      s_def_group_max = new_max;
   }
   s_def_groups[s_def_group_count].def = def;
   s_def_groups[s_def_group_count].lvltype_id = lvltype_id;
   s_def_group_count++;
}

/* Add a map the game lists and the project's table has cleared. */
static void area_add_removed_row(int def, const char * ds1_path, void * ctx)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int i, g;

   (void) ctx;
   for (i = 0; i < s_def_group_count; i++)
   {
      if (s_def_groups[i].def != def)
         continue;
      for (g = 0; g < ab->group_count; g++)
      {
         AREA_GROUP_S * grp = &ab->groups[g];

         if (grp->is_backup || grp->lvltype_id != s_def_groups[i].lvltype_id)
            continue;
         area_group_add_entry(grp, grp->lvltype_id, def, ds1_path);
         grp->entries[grp->entry_count - 1].slot_cleared = 1;
      }
   }
}

/* The union the spec asks for: rows the project's table cleared stay visible. */
static void area_add_removed_rows(void)
{
   char path[768];
   char lvlprest[] = "Data\\Global\\Excel\\LvlPrest.txt";
   long len;
   char * project, * game;

   if (!glb_project.is_open ||
       map_ops_table_path(glb_project.path, path, sizeof(path)) != 0)
      return;
   project = map_ops_read_file(path, &len);
   if (project == NULL)
      return;
   game = (char *) txt_read_in_mem_mpq_only(lvlprest);
   if (game != NULL)
   {
      lvlprest_each_cleared(game, project, area_add_removed_row, NULL);
      free(game);
   }
   free(project);
}
```

At the top of `area_browser_build`, reset the list: `s_def_group_count = 0;`. In each of the three places that loop `for (f = 0; f < 6; f++)` over `area_group_add_entry`, add immediately before that loop:

```c
            area_note_def_group((int)lp_def, grp->lvltype_id);
```

Call `area_add_removed_rows();` immediately before the `qsort` of groups in `area_browser_build`. In `area_browser_destroy`, free `s_def_groups`, and set it to NULL and both counts to 0. Add `#include "core/map_ops.h"` and `#include "core/lvlprest_text.h"`.

Confirm `txt_read_in_mem_mpq_only`'s buffer is NUL-terminated; `txt_read_in_mem` at line 545 writes one, and the MPQ-only variant's `realloc` at line 505 must do the same. If it doesn't, terminate it there.

Refuse to open a Removed row, at the top of `area_browser_switch_single`, after the bounds checks:

```c
   if (ab->groups[group_idx].entries[entry_idx].slot_cleared)
   {
      printf("area_browser: %s is removed from the project; restore its slot to open it\n",
             ab->groups[group_idx].entries[entry_idx].ds1_path);
      return -1;
   }
```

In the row drawing near `area_browser.c:2125`, dim and strike Removed rows. `fc` is the row's text colour; before its `al_draw_textf(... fname)`:

```c
               if (g->entries[j].slot_cleared)
                  fc = al_map_rgb(110, 110, 110);
```

and after that call:

```c
               if (g->entries[j].slot_cleared)
                  al_draw_line((float)(SB_MARGIN_X + 22), (float)y + 4.5f,
                               (float)(SB_MARGIN_X + 22 + al_get_text_width(a5_font, fname)),
                               (float)y + 4.5f, fc, 1.0f);
```

`map_state_mark` must return `""` for Removed, so the row shows only the strike. Check `map_state.c`, and its test, already say so.

- [ ] **Step 4: Scan backups from the project**

Rename the body of `area_browser_scan_backups` to a static `scan_backup_root(const char * root)`, leaving out its final re-sort and `printf`. Use `root` in place of every literal `"backup"` when opening the directory and building `sub_path` and the entry's `ds1_path`. A path that doesn't fit `AREA_DS1_ENTRY_S.ds1_path` must be skipped with a `printf`, never truncated: check `snprintf`'s result against `sizeof(e->ds1_path)` before `area_group_add_entry`. Then:

```c
void area_browser_scan_backups(void)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;

   /* A project keeps its own. The editor's old folder stays listed, since it may
      hold the only copy of a deleted map. */
   if (glb_project.is_open)
   {
      char root[512];
      snprintf(root, sizeof(root), "%s" DS1_SEP_STR "backup", glb_project.path);
      scan_backup_root(root);
   }
   scan_backup_root("backup");

   /* the re-sort and printf that ended the old function, unchanged */
}

/* Rebuild the Backup rows after one was added. The re-sort can move groups, so
 * the open map's row is found again by its path. */
void area_browser_rescan_backups(void)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   char open_path[256] = "";
   int open_def = 0, i, gi, ei;

   if (ab->loaded_group >= 0 && ab->loaded_group < ab->group_count &&
       !ab->groups[ab->loaded_group].is_backup &&
       ab->loaded_entry >= 0 && ab->loaded_entry < ab->groups[ab->loaded_group].entry_count)
   {
      AREA_DS1_ENTRY_S * e = &ab->groups[ab->loaded_group].entries[ab->loaded_entry];
      snprintf(open_path, sizeof(open_path), "%s", e->ds1_path);
      open_def = e->lvlprest_def;
   }

   for (i = 0; i < ab->group_count; i++)
      if (ab->groups[i].is_backup)
         ab->groups[i].entry_count = 0;
   area_browser_scan_backups();

   ab->selected_group = -1;
   ab->selected_entry = -1;
   ab->loaded_group = -1;
   ab->loaded_entry = -1;
   if (open_path[0] != '\0' && area_browser_find_row(open_path, open_def, &gi, &ei))
   {
      ab->loaded_group = gi;
      ab->loaded_entry = ei;
   }
}
```

Read the old function's end for the exact re-sort and `printf` lines, and keep them where the comment says.

- [ ] **Step 5: Rewrite the manager**

Add to `src/core/ds1_manager.c` these helpers, and `#include "core/map_ops.h"`, `"core/project.h"`, `"core/ds1_save_io.h"`, `"core/lvlprest_text.h"`:

```c
/* What a project without its own table starts from: what the editor reads now,
 * overlays included. Caller frees. */
static char * manager_seed(void)
{
   char name[] = "Data\\Global\\Excel\\LvlPrest.txt";
   return (char *) txt_read_in_mem(name);
}

static void manager_tables_changed(void)
{
   if (glb_ds1edit.lvlprest_buff != NULL)
      glb_ds1edit.lvlprest_buff = txt_destroy(glb_ds1edit.lvlprest_buff);
}

static int manager_need_project(char * error, int cap)
{
   if (glb_project.is_open)
      return 1;
   snprintf(error, cap, "No project is open.");
   return 0;
}

static void manager_remove_row(AREA_GROUP_S * g, int entry_idx)
{
   memmove(&g->entries[entry_idx], &g->entries[entry_idx + 1],
           sizeof(AREA_DS1_ENTRY_S) * (g->entry_count - entry_idx - 1));
   g->entry_count--;
}

/* "<date>" and "Act1-Town" for a backup folder, and the metadata that goes
 * with it. */
static void manager_backup_details(const AREA_GROUP_S * g, const AREA_DS1_ENTRY_S * e,
                                   char * date, int date_cap, char * label, int label_cap,
                                   char * json, int json_cap)
{
   time_t now;
   char stamp[64];

   time(&now);
   strftime(date, date_cap, "%Y-%m-%d", localtime(&now));
   strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));
   if (g->act > 0)
      snprintf(label, label_cap, "Act%d-%s", g->act, g->name);
   else
      snprintf(label, label_cap, "%s", g->name);
   snprintf(json, json_cap,
            "{\n"
            "  \"backup_date\": \"%s\",\n"
            "  \"original_path\": \"%s\",\n"
            "  \"lvlprest\": {\n"
            "    \"def\": %d,\n"
            "    \"lvltype_id\": %d,\n"
            "    \"area\": \"%s\"\n"
            "  }\n"
            "}\n",
            stamp, e->ds1_path, e->lvlprest_def, e->lvltype_id, label);
}

/* The folder part of a table path, slash included, or "" for none. */
static void manager_dir_of(const char * path, char * out, int cap)
{
   const char * a = strrchr(path, '/');
   const char * b = strrchr(path, '\\');
   const char * end = (a != NULL && (b == NULL || a > b)) ? a : b;

   if (end == NULL)
      out[0] = '\0';
   else
      snprintf(out, cap, "%.*s", (int)(end - path) + 1, path);
}

/* The table path for a new map in this Def: beside the Def's other files. */
static int manager_new_path(const AREA_GROUP_S * g, int def, const char * file_name,
                            char * out, int cap, char * error, int error_cap)
{
   char dir[256];
   int j;

   dir[0] = '\0';
   for (j = 0; j < g->entry_count; j++)
   {
      if (g->entries[j].lvlprest_def == def)
      {
         manager_dir_of(g->entries[j].ds1_path, dir, sizeof(dir));
         break;
      }
   }
   if (dir[0] == '\0' && g->entry_count > 0)
      manager_dir_of(g->entries[0].ds1_path, dir, sizeof(dir));
   if (dir[0] == '\0')
   {
      snprintf(error, error_cap, "Couldn't tell which folder %s goes in.", g->name);
      return -1;
   }
   if (snprintf(out, cap, "%s%s", dir, file_name) >= cap)
   {
      snprintf(error, error_cap, "The path for %s is too long.", file_name);
      return -1;
   }
   return 0;
}

int ds1_manager_name_in_use(int group_idx, const char * file_name)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   int j;

   if (group_idx < 0 || group_idx >= ab->group_count)
      return 0;
   g = &ab->groups[group_idx];
   for (j = 0; j < g->entry_count; j++)
   {
      const char * p = g->entries[j].ds1_path;
      const char * a = strrchr(p, '/');
      const char * b = strrchr(p, '\\');
      const char * base = (a != NULL && (b == NULL || a > b)) ? a + 1 : (b != NULL ? b + 1 : p);

      if (stricmp(base, file_name) == 0)
         return 1;
   }
   return 0;
}
```

Replace `ds1_manager_delete`:

```c
/* A New map: to the project's backup folder, and out of the table. */
int ds1_manager_delete(int group_idx, int entry_idx, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   AREA_DS1_ENTRY_S e;
   char date[32], label[128], json[1024], backup[512], path[768];
   char * table, * seed;
   long len;
   int rc;

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   g = &ab->groups[group_idx];
   if (entry_idx < 0 || entry_idx >= g->entry_count)
      return -1;
   e = g->entries[entry_idx];

   /* A table that can't be read stops this before the file moves. */
   if (map_ops_table_path(glb_project.path, path, sizeof(path)) != 0)
      return -1;
   table = map_ops_read_file(path, &len);
   if (table == NULL)
   {
      snprintf(error, cap, "The project's LvlPrest.txt can't be read.");
      return -1;
   }
   rc = lvlprest_find_file_slot(table, e.lvlprest_def, e.ds1_path);
   free(table);
   if (rc == LVLPREST_TABLE_UNUSABLE)
   {
      snprintf(error, cap, "The project's LvlPrest.txt has no Def or File columns.");
      return -1;
   }

   manager_backup_details(g, &e, date, sizeof(date), label, sizeof(label), json, sizeof(json));
   if (map_ops_backup_file(glb_project.path, e.ds1_path, label, date, json,
                           backup, sizeof(backup), error, cap) != 0)
      return -1;

   seed = manager_seed();
   rc = map_ops_unregister(glb_project.path, seed, e.lvlprest_def, e.ds1_path, error, cap);
   free(seed);
   if (rc != 0)
   {
      /* The table still names it, so the file goes back. */
      char ignored[8];
      map_ops_restore_file(glb_project.path, backup, e.ds1_path, ignored, sizeof(ignored));
      return -1;
   }

   manager_tables_changed();

   /* Find the open map's row again by path, before this row goes. A deleted open
      map then has no row, and the rescan's lookup finds none. */
   {
      char open_path[256] = "";
      int open_def = 0, was_this = ab->loaded_group == group_idx && ab->loaded_entry == entry_idx;

      if (!was_this && ab->loaded_group >= 0 && ab->loaded_group < ab->group_count &&
          ab->loaded_entry >= 0 && ab->loaded_entry < ab->groups[ab->loaded_group].entry_count)
      {
         snprintf(open_path, sizeof(open_path), "%s",
                  ab->groups[ab->loaded_group].entries[ab->loaded_entry].ds1_path);
         open_def = ab->groups[ab->loaded_group].entries[ab->loaded_entry].lvlprest_def;
      }
      manager_remove_row(g, entry_idx);
      ab->loaded_group = -1;
      ab->loaded_entry = -1;
      if (open_path[0] != '\0')
         area_browser_find_row(open_path, open_def, &ab->loaded_group, &ab->loaded_entry);
   }
   area_browser_rescan_backups();
   return 0;
}
```

`area_browser_rescan_backups` can add groups and re-sort them, so callers must not hold a `group_idx` across it. It finds the open map's row again by itself, and `interface_delete_map` in Task 6 handles the case where the deleted row was that one.

Add the other three state operations:

```c
/* A Modified map: the project's copy goes to the backup folder. */
int ds1_manager_revert(int group_idx, int entry_idx, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   char date[32], label[128], json[1024], backup[512];

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   g = &ab->groups[group_idx];
   if (entry_idx < 0 || entry_idx >= g->entry_count)
      return -1;

   manager_backup_details(g, &g->entries[entry_idx], date, sizeof(date), label, sizeof(label),
                          json, sizeof(json));
   if (map_ops_backup_file(glb_project.path, g->entries[entry_idx].ds1_path, label, date, json,
                           backup, sizeof(backup), error, cap) != 0)
      return -1;
   area_browser_rescan_backups();
   area_browser_refresh_states();
   return 0;
}

/* A Game map: its slot is cleared in the project's table. The file isn't touched. */
int ds1_manager_remove_slot(int group_idx, int entry_idx, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_DS1_ENTRY_S * e;
   char * seed;
   int rc;

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   if (entry_idx < 0 || entry_idx >= ab->groups[group_idx].entry_count)
      return -1;
   e = &ab->groups[group_idx].entries[entry_idx];

   seed = manager_seed();
   rc = map_ops_unregister(glb_project.path, seed, e->lvlprest_def, e->ds1_path, error, cap);
   free(seed);
   if (rc != 0)
      return -1;

   manager_tables_changed();
   e->slot_cleared = 1;
   area_browser_refresh_states();
   return 0;
}

/* A Removed map: the game's value goes back in the project's table. */
int ds1_manager_restore_slot(int group_idx, int entry_idx, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_DS1_ENTRY_S * e;
   char name[] = "Data\\Global\\Excel\\LvlPrest.txt";
   char * game, * seed;
   int rc;

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   if (entry_idx < 0 || entry_idx >= ab->groups[group_idx].entry_count)
      return -1;
   e = &ab->groups[group_idx].entries[entry_idx];

   game = (char *) txt_read_in_mem_mpq_only(name);
   if (game == NULL)
   {
      snprintf(error, cap, "The game's LvlPrest.txt can't be read.");
      return -1;
   }
   seed = manager_seed();
   rc = map_ops_restore_slot(glb_project.path, seed, game, e->lvlprest_def, e->ds1_path,
                             error, cap);
   free(seed);
   free(game);
   if (rc != 0)
      return -1;

   manager_tables_changed();
   e->slot_cleared = 0;
   area_browser_refresh_states();
   return 0;
}
```

Replace `ds1_manager_create_empty` and `ds1_manager_clone`:

```c
int ds1_manager_create_empty(int group_idx, int def, const char * file_name,
                             int width, int height, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   char rel[256];
   char * seed;
   int rc;

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   g = &ab->groups[group_idx];
   if (manager_new_path(g, def, file_name, rel, sizeof(rel), error, cap) != 0)
      return -1;

   seed = manager_seed();
   rc = map_ops_create_empty(glb_project.path, seed, def, rel, width, height,
                             g->act > 0 ? g->act : 1, error, cap);
   free(seed);
   if (rc != 0)
      return -1;

   manager_tables_changed();
   area_group_add_entry(g, g->lvltype_id, def, rel);
   g->is_expanded = TRUE;
   area_browser_refresh_states();
   return 0;
}

/* The map as it is in memory, so unsaved edits come along and a map that exists
 * only in an MPQ can be cloned. */
int ds1_manager_clone(int src_ds1_idx, int group_idx, int def, const char * file_name,
                      char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   char rel[256], dest[768];
   char * seed;
   int rc;

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   g = &ab->groups[group_idx];
   if (manager_new_path(g, def, file_name, rel, sizeof(rel), error, cap) != 0)
      return -1;
   if (map_ops_tile_path(glb_project.path, rel, dest, sizeof(dest)) != 0)
   {
      snprintf(error, cap, "The path for %s is too long.", file_name);
      return -1;
   }
   if (ds1_save_copy_checked(src_ds1_idx, dest, error, (size_t) cap) != 0)
      return -1;

   seed = manager_seed();
   rc = map_ops_register(glb_project.path, seed, def, rel, error, cap);
   free(seed);
   if (rc != 0)
   {
      remove(dest);
      return -1;
   }

   manager_tables_changed();
   area_group_add_entry(g, g->lvltype_id, def, rel);
   g->is_expanded = TRUE;
   area_browser_refresh_states();
   return 0;
}
```

`glb_ds1[].name` must be long enough for `dest` in `ds1_save_copy_checked`'s staging buffer; its size check covers a longer path by refusing it.

Change `ds1_manager_restore` to take `char *error, int cap`, parse the JSON as it does now, and then, instead of its hand-written copy and `lvlprest` calls:

```c
   if (!manager_need_project(error, cap))
      return -1;
   if (map_ops_restore_file(glb_project.path, backup_ds1_path, original_path, error, cap) != 0)
      return -1;

   /* A New map's slot was cleared when it was deleted. A Modified map's never
      was, and map_ops_register would then list it twice. */
   {
      char * seed = manager_seed();
      char path[768];
      long len;
      char * table;
      int listed = 0;

      if (map_ops_table_path(glb_project.path, path, sizeof(path)) == 0)
      {
         table = map_ops_read_file(path, &len);
         if (table != NULL)
         {
            listed = lvlprest_find_file_slot(table, def, original_path) > 0;
            free(table);
         }
      }
      if (!listed && def > 0 &&
          map_ops_register(glb_project.path, seed, def, original_path, error, cap) != 0)
      {
         free(seed);
         return -1;
      }
      free(seed);
   }
```

and keep its code that adds a row to the regular group, only if `area_browser_find_row(original_path, def, &gi, &ei)` finds none; remove the backup row, remove the JSON and the now-empty folder as it does today (`remove(backup_json_path); RMDIR(backup_dir);`, no longer the `.ds1`, which `map_ops_restore_file` moved), then call `manager_tables_changed()` and `area_browser_refresh_states()`. A row with no JSON returns -1 with a sentence in `error`, not a `printf`.

Delete `ds1_manager_backup`, `ds1_manager_write_backup_json`, `write_empty_ds1`, and `ds1_manager_lvlprest_*` once `grep -rn` shows no callers. `ds1_manager_txt_set_cell` and its raw helpers stay for the properties panel.

Add `#define`s removal: `DS1_DELETE_ERR_BACKUP` and `DS1_DELETE_ERR_TABLE` go, since messages now travel in `error`. Update `ds1_manager.h` to the signatures listed under Interfaces.

`interface.c` won't compile until Task 6. To keep this commit building, change its callers now, with a minimal message box:

```c
/* interface_delete_map */
   {
      char error[512] = "";
      if (ds1_manager_delete(gi, ei, error, sizeof(error)) != 0)
      {
         al_show_native_message_box(a5_display, "Delete Map",
            "The map wasn't deleted.", error, NULL, ALLEGRO_MESSAGEBOX_ERROR);
         input_suppress_held();
         input_discard_pending();
         return;
      }
   }
```

and the same `error` handling for `ds1_manager_restore` in the context menu. In `interface_new_map`, temporarily use the suggested name so Task 5 and Task 6 have something to replace:

```c
   char file[128], error[512] = "";
   ds1_manager_suggest_name(g->entries[ei >= 0 ? ei : 0].ds1_path, g, file, sizeof(file));
   /* clone: */ rc = ds1_manager_clone(ds1_idx, gi, g->entries[ei >= 0 ? ei : 0].lvlprest_def, file, error, sizeof(error));
   /* new:   */ rc = ds1_manager_create_empty(gi, g->entries[ei >= 0 ? ei : 0].lvlprest_def, file, new_w, new_h, error, sizeof(error));
```

keeping its existing failure box, with `error` as the box's text.

- [ ] **Step 6: Build and run the tests**

Run: `cmake --build tmp/build-lifecycle --config Release -- /m /v:q /nologo`, then `ctest --test-dir tmp/build-lifecycle -C Release -L portable -j 3 --timeout 300`.
Expected: all pass. Golden tests must not change: `--headless` has no project and no Removed rows.

- [ ] **Step 7: Commit**

```bash
git add src/structs.h src/core/area_browser.c src/core/area_browser.h src/core/ds1_manager.c src/core/ds1_manager.h src/ui/interface.c test/python/test_delete_key_routing.py
git commit -m "Delete, revert, remove and restore maps by state, with backups in the project"
```

---

### Task 5: Commands know each map's state

**Files:**
- Modify: `src/ui/commands.c`, `src/ui/commands.h`, `src/ui/menu_bar.c`, `src/ui/interface.c` (`interface_command_context`)
- Test: `test/test_commands.c`

**Interfaces:**
- `CMD_CTX` gains `int open_state; /* MAP_STATE_E of the open map's row */` and `int open_is_backup; /* that row is in a Backup group */`.
- `has_open_entry` keeps its meaning: the open map has a regular row. Backup rows are now reported through `open_is_backup` with `has_open_entry` 0.
- Produces: `CMD_ID commands_ctrl_del_target(const CMD_CTX *ctx);`: `CMD_MAP_DELETE`, `CMD_MAP_REVERT`, `CMD_MAP_REMOVE`, or `CMD_NONE`.

`commands.c` is pure and must not include `map_state.h`'s dirty-tracking users, but the enum is harmless: `#include "core/map_state.h"`.

- [ ] **Step 1: Write the failing tests**

Add to `test/test_commands.c`, and to `main`:

```c
static CMD_CTX ctx_with_state(int state)
{
   CMD_CTX c;
   memset(&c, 0, sizeof(c));
   c.has_map = 1;
   c.has_open_entry = 1;
   c.open_state = state;
   return c;
}

void test_map_commands_follow_the_open_rows_state(void)
{
   CMD_CTX game = ctx_with_state(MAP_STATE_GAME);
   CMD_CTX mod = ctx_with_state(MAP_STATE_MODIFIED);
   CMD_CTX fresh = ctx_with_state(MAP_STATE_NEW);
   CMD_CTX gone = ctx_with_state(MAP_STATE_REMOVED);

   TEST_ASSERT_TRUE(commands_is_available(CMD_MAP_REMOVE, &game));
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_REVERT, &game));
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_DELETE, &game));

   TEST_ASSERT_TRUE(commands_is_available(CMD_MAP_REVERT, &mod));
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_REMOVE, &mod));
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_DELETE, &mod));

   TEST_ASSERT_TRUE(commands_is_available(CMD_MAP_DELETE, &fresh));
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_REVERT, &fresh));

   TEST_ASSERT_TRUE(commands_is_available(CMD_MAP_RESTORE_SLOT, &gone));
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_DELETE, &gone));
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_CLONE, &gone));
}

void test_restore_from_backup_needs_an_open_backup_row(void)
{
   CMD_CTX c;
   memset(&c, 0, sizeof(c));
   c.has_map = 1;
   c.open_is_backup = 1;
   TEST_ASSERT_TRUE(commands_is_available(CMD_MAP_RESTORE_BACKUP, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_CLONE, &c));
   c.open_is_backup = 0;
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_RESTORE_BACKUP, &c));
}

void test_ctrl_del_runs_what_applies_to_the_state(void)
{
   CMD_CTX c;
   c = ctx_with_state(MAP_STATE_NEW);
   TEST_ASSERT_EQUAL_INT(CMD_MAP_DELETE, commands_ctrl_del_target(&c));
   c = ctx_with_state(MAP_STATE_MODIFIED);
   TEST_ASSERT_EQUAL_INT(CMD_MAP_REVERT, commands_ctrl_del_target(&c));
   c = ctx_with_state(MAP_STATE_GAME);
   TEST_ASSERT_EQUAL_INT(CMD_MAP_REMOVE, commands_ctrl_del_target(&c));
   c = ctx_with_state(MAP_STATE_REMOVED);
   TEST_ASSERT_EQUAL_INT(CMD_NONE, commands_ctrl_del_target(&c));
   memset(&c, 0, sizeof(c));
   TEST_ASSERT_EQUAL_INT(CMD_NONE, commands_ctrl_del_target(&c));
}
```

Add `#include "core/map_state.h"` and `#include <string.h>` if missing. Any existing test that sets `has_open_entry` and expects `CMD_MAP_DELETE` available must now also set `open_state = MAP_STATE_NEW`; fix those to match, since the spec's state rules replace step 2's placeholder.

- [ ] **Step 2: Run to see it fail**

Run: `cmake --build tmp/build-lifecycle --config Release --target test_commands -- /m /v:q /nologo`
Expected: FAIL, `open_state` isn't a member.

- [ ] **Step 3: Implement**

In `commands_is_available`, replace the Map cases. Remove `CMD_MAP_REVERT`, `CMD_MAP_REMOVE`, `CMD_MAP_RESTORE_SLOT` and `CMD_MAP_RESTORE_BACKUP` from the "Step 3 and step 4" group, and rename its comment to "Step 4 implements these.":

```c
   case CMD_MAP_CLONE:
      return c->has_map && c->has_open_entry && c->open_state != MAP_STATE_REMOVED;
   case CMD_MAP_DELETE:
      return c->has_map && c->has_open_entry && c->open_state == MAP_STATE_NEW;
   case CMD_MAP_REVERT:
      return c->has_map && c->has_open_entry && c->open_state == MAP_STATE_MODIFIED;
   case CMD_MAP_REMOVE:
      return c->has_map && c->has_open_entry && c->open_state == MAP_STATE_GAME;
   case CMD_MAP_RESTORE_SLOT:
      return c->has_map && c->has_open_entry && c->open_state == MAP_STATE_REMOVED;
   case CMD_MAP_RESTORE_BACKUP:
      return c->has_map && c->open_is_backup;
```

Add:

```c
CMD_ID commands_ctrl_del_target(const CMD_CTX *c)
{
   if (!c->has_map || !c->has_open_entry)
      return CMD_NONE;
   switch (c->open_state)
   {
   case MAP_STATE_NEW:      return CMD_MAP_DELETE;
   case MAP_STATE_MODIFIED: return CMD_MAP_REVERT;
   case MAP_STATE_GAME:     return CMD_MAP_REMOVE;
   default:                 return CMD_NONE;
   }
}
```

Declare it in `commands.h`, with this comment: "`Ctrl+Del` has one key, and three commands by state."

In `menu_bar_poll`, in the table lookup loop, right after `a5_key_suppressed[key] = 1;` and the `keys_enabled` check (read the current shape from step 2's fix; `id` must be consumed either way):

```c
            if (id == CMD_MAP_DELETE)
               return commands_ctrl_del_target(ctx);
```

In `interface_command_context`, fill the new fields. `interface_loaded_row_is_open_map` rejects backup rows, so test those on their own:

```c
   c.has_open_entry = interface_loaded_row_is_open_map(ds1_idx);
   if (c.has_open_entry)
      c.open_state = ab->groups[ab->loaded_group].entries[ab->loaded_entry].state;
   c.open_is_backup = c.has_map && ab->loaded_group >= 0 &&
                      ab->loaded_group < ab->group_count &&
                      ab->groups[ab->loaded_group].is_backup &&
                      ab->loaded_entry >= 0 &&
                      ab->loaded_entry < ab->groups[ab->loaded_group].entry_count;
```

Confirm opening a backup row sets `loaded_group` and `loaded_entry`, in `area_browser_switch_single`. If it doesn't, set them there.

- [ ] **Step 4: Run the tests**

Run: `ctest --test-dir tmp/build-lifecycle -C Release -R "^commands$" --output-on-failure`
Expected: PASS. The conflict test stays at 0: only `CMD_MAP_DELETE` dispatches `Ctrl+Del`.

- [ ] **Step 5: Commit**

```bash
git add src/ui/commands.c src/ui/commands.h src/ui/menu_bar.c src/ui/interface.c test/test_commands.c
git commit -m "Offer map commands by the open map's state and route Ctrl+Del by it"
```

---

### Task 6: The new-map dialog and confirmations

**Files:**
- Create: `src/ui/map_dialog.c`, `src/ui/map_dialog.h`
- Modify: `CMakeLists.txt`, `src/ui/interface.c`, `src/ui/commands.c`, `src/ui/commands.h`, `src/ui/map_save_ui.c`, `src/ui/map_save_ui.h`, `test/test_commands.c`, `test/python/test_delete_key_routing.py`

**Interfaces:**
- Consumes: Tasks 4 and 5; `map_ops_check_name`, `map_ops_check_size`; `ds1_manager_name_in_use`, `ds1_manager_suggest_name`.
- Produces:
  - `int map_save_ui_require_project(void);`: the existing static `require_project`, renamed and exported.
  - `int map_dialog_show(MAP_DIALOG_S *d);`:

```c
typedef struct MAP_DIALOG_S
{
   int  is_clone;       /* hides width and height */
   int  group_idx;      /* in: the default area. out: the chosen one */
   int  def;            /* in: the default row. out: the chosen one */
   int  width, height;  /* in: defaults. out: chosen */
   char file_name[64];  /* in: suggested name. out: checked, with .ds1 */
} MAP_DIALOG_S;
```

Returns 1 for OK, with `d` filled and every rule checked, or 0 for cancel. `Esc`, Cancel and a window close all cancel; a window close stays pending for the main loop's quit check.

- [ ] **Step 1: Write the dialog**

Fields, in order: Name, Area (Left/Right to cycle regular groups with a row), Tileset (Left/Right to cycle the Area's Defs, shown as the row's name plus `Def N`), Width, Height. `Tab`/`Shift+Tab` or a click moves between them. `Enter` or OK tries to accept: a failed rule shows its sentence in the status row and keeps the dialog up.

Create `src/ui/map_dialog.h` with the struct above and:

```c
#ifndef DS1EDIT_MAP_DIALOG_H
#define DS1EDIT_MAP_DIALOG_H
/* struct here */
int map_dialog_show(MAP_DIALOG_S * d);
#endif
```

Create `src/ui/map_dialog.c`, modelled on `src/ui/text_input_modal.c`: reuse its colours and event loop. In full:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "misc.h"
#include "core/ds1_manager.h"
#include "core/map_ops.h"
#include "core/txtread.h"
#include "ui/compat.h"
#include "ui/input.h"
#include "ui/map_dialog.h"

#define DLG_W        520
#define DLG_H        218
#define TITLE_H      20
#define ROW_H        26
#define LABEL_W      84
#define BTN_W        64
#define BTN_H        22
#define STATUS_H     18
#define FIELD_COUNT  5

enum { F_NAME, F_AREA, F_TILESET, F_WIDTH, F_HEIGHT };

typedef struct DLG
{
   MAP_DIALOG_S * d;
   char name[64], w[8], h[8];
   int  focus;
   int  defs[64];
   int  def_count, def_index;
   char status[160];
   int  x0, y0;
} DLG;

static int rect_hit(int mx, int my, int x, int y, int w, int h)
{
   return mx >= x && mx < x + w && my >= y && my < y + h;
}

static int usable_group(int gi)
{
   AREA_GROUP_S * g = &glb_ds1edit.area_browser.groups[gi];
   return !g->is_backup && g->entry_count > 0;
}

/* The distinct Defs in the chosen area, keeping the chosen one if it's there. */
static void load_defs(DLG * m)
{
   AREA_GROUP_S * g = &glb_ds1edit.area_browser.groups[m->d->group_idx];
   int j, k, wanted = m->d->def;

   m->def_count = 0;
   m->def_index = 0;
   for (j = 0; j < g->entry_count && m->def_count < 64; j++)
   {
      int def = g->entries[j].lvlprest_def, seen = 0;

      for (k = 0; k < m->def_count; k++)
         seen = seen || m->defs[k] == def;
      if (seen || def <= 0)
         continue;
      if (def == wanted)
         m->def_index = m->def_count;
      m->defs[m->def_count++] = def;
   }
   m->d->def = m->def_count > 0 ? m->defs[m->def_index] : 0;
}

static void cycle_area(DLG * m, int dir)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int gi = m->d->group_idx, tries;

   for (tries = 0; tries < ab->group_count; tries++)
   {
      gi = (gi + dir + ab->group_count) % ab->group_count;
      if (usable_group(gi))
      {
         m->d->group_idx = gi;
         load_defs(m);
         return;
      }
   }
}

static void cycle_tileset(DLG * m, int dir)
{
   if (m->def_count == 0)
      return;
   m->def_index = (m->def_index + dir + m->def_count) % m->def_count;
   m->d->def = m->defs[m->def_index];
}

static char * focused_text(DLG * m, int * cap, int * digits_only)
{
   *digits_only = 0;
   switch (m->focus)
   {
   case F_NAME:   *cap = (int) sizeof(m->name); return m->name;
   case F_WIDTH:  *cap = (int) sizeof(m->w); *digits_only = 1; return m->w;
   case F_HEIGHT: *cap = (int) sizeof(m->h); *digits_only = 1; return m->h;
   default:       return NULL;
   }
}

static void next_focus(DLG * m, int dir)
{
   do
      m->focus = (m->focus + dir + FIELD_COUNT) % FIELD_COUNT;
   while (m->d->is_clone && (m->focus == F_WIDTH || m->focus == F_HEIGHT));
}

/* 1 if every rule holds; otherwise the first problem goes in the status row. */
static int try_accept(DLG * m)
{
   char file[64];

   if (map_ops_check_name(m->name, file, sizeof(file), m->status, sizeof(m->status)) != 0)
      return 0;
   if (ds1_manager_name_in_use(m->d->group_idx, file))
   {
      snprintf(m->status, sizeof(m->status), "This area already has a map named %s.", file);
      return 0;
   }
   if (m->def_count == 0)
   {
      snprintf(m->status, sizeof(m->status), "This area has no LvlPrest row to add a map to.");
      return 0;
   }
   if (!m->d->is_clone)
   {
      int w = atoi(m->w), h = atoi(m->h);
      if (map_ops_check_size(w, h, m->status, sizeof(m->status)) != 0)
         return 0;
      m->d->width = w;
      m->d->height = h;
   }
   snprintf(m->d->file_name, sizeof(m->d->file_name), "%s", file);
   return 1;
}

static void draw_field(const DLG * m, int field, const char * label, const char * value,
                       int chooser)
{
   int row = field;
   int y = m->y0 + TITLE_H + 8 + row * ROW_H;
   int x = m->x0 + 12 + LABEL_W;
   int w = DLG_W - 24 - LABEL_W;
   int focused = m->focus == field;

   al_draw_text(a5_font, al_map_rgb(180, 200, 220), (float)(m->x0 + 12), (float)(y + 6), 0, label);
   al_draw_filled_rectangle((float)x, (float)y, (float)(x + w), (float)(y + 20),
                            al_map_rgb(30, 32, 40));
   al_draw_rectangle((float)x + 0.5f, (float)y + 0.5f, (float)(x + w) - 0.5f, (float)(y + 20) - 0.5f,
                     focused ? al_map_rgb(255, 230, 80) : al_map_rgb(80, 100, 130), 1.0f);
   al_draw_textf(a5_font, al_map_rgb(255, 255, 255), (float)(x + 6), (float)(y + 6), 0,
                 chooser ? "< %s >" : "%s", value);
}

static void tileset_label(const DLG * m, char * out, int cap)
{
   TXT_S * txt = glb_ds1edit.lvlprest_buff;

   snprintf(out, cap, "Def %d", m->d->def);
   if (txt != NULL)
   {
      int def_col = misc_get_txt_column_num(RQ_LVLPREST, "Def");
      int name_col = misc_get_txt_column_num(RQ_LVLPREST, "Name");
      int i;

      for (i = 0; i < txt->line_num; i++)
      {
         long * def = (long *)(txt->data + i * txt->line_size + txt->col[def_col].offset);
         if (*def == m->d->def)
         {
            snprintf(out, cap, "%s (Def %d)",
                     txt->data + i * txt->line_size + txt->col[name_col].offset, m->d->def);
            return;
         }
      }
   }
}

static void draw_dialog(const DLG * m, int hover_ok, int hover_cancel)
{
   AREA_GROUP_S * g = &glb_ds1edit.area_browser.groups[m->d->group_idx];
   ALLEGRO_BITMAP * prev = al_get_target_bitmap();
   int by = m->y0 + DLG_H - STATUS_H - BTN_H - 6;
   int ok_x = m->x0 + DLG_W / 2 - BTN_W - 6, cancel_x = m->x0 + DLG_W / 2 + 6;
   char area[120], tileset[160];

   al_set_target_bitmap(glb_ds1edit.screen_buff);
   al_draw_filled_rectangle((float)m->x0, (float)m->y0, (float)(m->x0 + DLG_W), (float)(m->y0 + DLG_H),
                            al_map_rgba(10, 12, 16, 235));
   al_draw_rectangle((float)m->x0 + 0.5f, (float)m->y0 + 0.5f, (float)(m->x0 + DLG_W) - 0.5f,
                     (float)(m->y0 + DLG_H) - 0.5f, al_map_rgb(120, 160, 200), 1.0f);
   al_draw_filled_rectangle((float)m->x0, (float)m->y0, (float)(m->x0 + DLG_W), (float)(m->y0 + TITLE_H),
                            al_map_rgb(20, 40, 60));
   al_draw_text(a5_font, al_map_rgb(200, 220, 255), (float)(m->x0 + 8), (float)(m->y0 + 6), 0,
                m->d->is_clone ? "Clone Map" : "New Map");

   if (g->act > 0)
      snprintf(area, sizeof(area), "Act %d - %s", g->act, g->name);
   else
      snprintf(area, sizeof(area), "%s", g->name);
   tileset_label(m, tileset, sizeof(tileset));

   draw_field(m, F_NAME, "Name", m->name, 0);
   draw_field(m, F_AREA, "Area", area, 1);
   draw_field(m, F_TILESET, "Tileset", tileset, 1);
   if (!m->d->is_clone)
   {
      draw_field(m, F_WIDTH, "Width", m->w, 0);
      draw_field(m, F_HEIGHT, "Height", m->h, 0);
   }

   al_draw_filled_rectangle((float)ok_x, (float)by, (float)(ok_x + BTN_W), (float)(by + BTN_H),
                            hover_ok ? al_map_rgb(60, 90, 130) : al_map_rgb(40, 60, 90));
   al_draw_text(a5_font, al_map_rgb(220, 230, 250), (float)(ok_x + BTN_W / 2), (float)(by + 7),
                ALLEGRO_ALIGN_CENTRE, "OK");
   al_draw_filled_rectangle((float)cancel_x, (float)by, (float)(cancel_x + BTN_W), (float)(by + BTN_H),
                            hover_cancel ? al_map_rgb(60, 90, 130) : al_map_rgb(40, 60, 90));
   al_draw_text(a5_font, al_map_rgb(220, 230, 250), (float)(cancel_x + BTN_W / 2), (float)(by + 7),
                ALLEGRO_ALIGN_CENTRE, "Cancel");

   al_draw_filled_rectangle((float)m->x0, (float)(m->y0 + DLG_H - STATUS_H),
                            (float)(m->x0 + DLG_W), (float)(m->y0 + DLG_H), al_map_rgb(20, 40, 60));
   al_draw_text(a5_font, m->status[0] ? al_map_rgb(255, 170, 120) : al_map_rgb(140, 160, 180),
                (float)(m->x0 + 8), (float)(m->y0 + DLG_H - STATUS_H + 5), 0,
                m->status[0] ? m->status : "[Tab] next field   [Left/Right] choose   [Enter] OK   [Esc] Cancel");

   if (prev != NULL)
      al_set_target_bitmap(prev);
}

static void handle_key(DLG * m, const ALLEGRO_EVENT * ev)
{
   int kc = ev->keyboard.keycode;
   unsigned int uch = ev->keyboard.unichar;
   int cap = 0, digits = 0;
   char * text = focused_text(m, &cap, &digits);

   m->status[0] = '\0';
   if (kc == ALLEGRO_KEY_TAB)
   {
      next_focus(m, (ev->keyboard.modifiers & ALLEGRO_KEYMOD_SHIFT) ? -1 : 1);
      return;
   }
   if (kc == ALLEGRO_KEY_LEFT || kc == ALLEGRO_KEY_RIGHT)
   {
      int dir = kc == ALLEGRO_KEY_LEFT ? -1 : 1;

      if (m->focus == F_AREA)
         cycle_area(m, dir);
      else if (m->focus == F_TILESET)
         cycle_tileset(m, dir);
      return;
   }
   if (text == NULL)
      return;
   if (kc == ALLEGRO_KEY_BACKSPACE)
   {
      size_t n = strlen(text);
      if (n > 0)
         text[n - 1] = '\0';
      return;
   }
   if (uch >= 32 && uch < 127 && (!digits || (uch >= '0' && uch <= '9')))
   {
      size_t n = strlen(text);
      if ((int) n + 1 < cap)
      {
         text[n] = (char) uch;
         text[n + 1] = '\0';
      }
   }
}

int map_dialog_show(MAP_DIALOG_S * d)
{
   DLG m;
   int done = 0, accepted = 0, prev_mb = 1;

   memset(&m, 0, sizeof(m));
   m.d = d;
   snprintf(m.name, sizeof(m.name), "%s", d->file_name);
   snprintf(m.w, sizeof(m.w), "%d", d->width);
   snprintf(m.h, sizeof(m.h), "%d", d->height);
   m.x0 = (glb_config.screen.width - DLG_W) / 2;
   m.y0 = (glb_config.screen.height - DLG_H) / 2;
   txt_ensure_lvlprest();
   load_defs(&m);

   input_suppress_held();
   input_pump();
   while (!done)
   {
      ALLEGRO_EVENT ev;
      int mx, my, mb, by, ok_x, cancel_x, hover_ok, hover_cancel, f;

      /* Key events go to the fields, so this loop reads them itself rather than
         through input_pump(), as text_input_modal does. */
      while (al_get_next_event(a5_event_queue, &ev))
      {
         if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
         {
            a5_display_closed = 1;
            done = 1;
            break;
         }
         if (ev.type == ALLEGRO_EVENT_KEY_CHAR)
         {
            int kc = ev.keyboard.keycode;

            if (kc == ALLEGRO_KEY_ESCAPE)
            {
               done = 1;
               break;
            }
            if (kc == ALLEGRO_KEY_ENTER || kc == ALLEGRO_KEY_PAD_ENTER)
            {
               if (try_accept(&m))
               {
                  accepted = 1;
                  done = 1;
                  break;
               }
               continue;
            }
            handle_key(&m, &ev);
         }
      }

      al_get_mouse_state(&a5_ms_state);
      mx = a5_mouse_x;
      my = a5_mouse_y;
      mb = a5_mouse_b;
      by = m.y0 + DLG_H - STATUS_H - BTN_H - 6;
      ok_x = m.x0 + DLG_W / 2 - BTN_W - 6;
      cancel_x = m.x0 + DLG_W / 2 + 6;
      hover_ok = rect_hit(mx, my, ok_x, by, BTN_W, BTN_H);
      hover_cancel = rect_hit(mx, my, cancel_x, by, BTN_W, BTN_H);

      if (mb && !prev_mb)
      {
         if (hover_ok)
         {
            if (try_accept(&m))
            {
               accepted = 1;
               done = 1;
            }
         }
         else if (hover_cancel)
            done = 1;
         else
         {
            for (f = 0; f < FIELD_COUNT; f++)
            {
               if ((f == F_WIDTH || f == F_HEIGHT) && d->is_clone)
                  continue;
               if (rect_hit(mx, my, m.x0 + 12, m.y0 + TITLE_H + 8 + f * ROW_H, DLG_W - 24, 20))
               {
                  m.focus = f;
                  /* A click on the left or right half of a chooser steps it. */
                  if (f == F_AREA || f == F_TILESET)
                  {
                     int dir = mx < m.x0 + DLG_W / 2 ? -1 : 1;
                     if (f == F_AREA) cycle_area(&m, dir); else cycle_tileset(&m, dir);
                  }
               }
            }
         }
      }
      prev_mb = mb;

      draw_dialog(&m, hover_ok, hover_cancel);
      misc_draw_screen(mx, my);
      al_rest(0.016);
   }

   /* Keys and the click that closed this mustn't reach the map. */
   input_pump();
   while ((a5_mouse_b & 1) && !a5_display_closed)
   {
      al_rest(0.01);
      input_pump();
   }
   input_suppress_held();
   return accepted;
}
```

Check `a5_display_closed` is writable from here, as `input.c` defines it. If `text_input_modal.c`'s comment or a test says modal loops must not set it, follow the convention used by `menu_bar.c`'s `run_menu`, which leaves a close pending, and say so in your report. Rows use `F_*` as row numbers, so Width and Height stay in their rows and a clone's last two rows are blank; `DLG_H` fits five rows, the buttons and the status row.

Add `src/ui/map_dialog.c` to `CMakeLists.txt`, beside `src/ui/menu_bar.c`. Check how the sources are listed: there may be more than one list, as with the editor and headless builds. Add it wherever `menu_bar.c` appears.

- [ ] **Step 2: Wire New and Clone**

In `src/ui/map_save_ui.c`, rename `require_project` to `map_save_ui_require_project`, make it non-static, and update its two callers. Declare in the header:

```c
// Ask for a project if none is open. 1 once one is open, 0 if the user declined.
int map_save_ui_require_project(void);
```

Replace `interface_new_map` in `interface.c`:

```c
/* Create a map, or clone the open one, through the dialog. "The first save or
 * new map asks the user" for a project. */
static void interface_new_map(int ds1_idx, int clone)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   int use_loaded = interface_loaded_row_is_open_map(ds1_idx);
   MAP_DIALOG_S d;
   char error[512] = "";
   int gi, ei, rc, i;

   if (clone && !use_loaded)
      return;
   if (!map_save_ui_require_project())
      return;

   /* A project just opened rebuilds the rows. */
   use_loaded = interface_loaded_row_is_open_map(ds1_idx);
   gi = use_loaded ? ab->loaded_group : ab->selected_group;
   ei = use_loaded ? ab->loaded_entry : ab->selected_entry;
   if (gi < 0 || gi >= ab->group_count || ab->groups[gi].is_backup ||
       ab->groups[gi].entry_count == 0)
   {
      gi = -1;
      for (i = 0; i < ab->group_count && gi < 0; i++)
         if (!ab->groups[i].is_backup && ab->groups[i].entry_count > 0)
            gi = i;
      ei = 0;
      if (gi < 0)
         return;
   }
   if (ei < 0 || ei >= ab->groups[gi].entry_count)
      ei = 0;

   memset(&d, 0, sizeof(d));
   d.is_clone = clone;
   d.group_idx = gi;
   d.def = ab->groups[gi].entries[ei].lvlprest_def;
   d.width = 20;
   d.height = 20;
   if (glb_ds1edit.has_loaded_ds1 && glb_ds1[ds1_idx].name[0] != '\0')
   {
      d.width = (int) glb_ds1[ds1_idx].width;
      d.height = (int) glb_ds1[ds1_idx].height;
   }
   ds1_manager_suggest_name(ab->groups[gi].entries[ei].ds1_path, &ab->groups[gi],
                            d.file_name, sizeof(d.file_name));

   if (!map_dialog_show(&d))
      return;

   if (clone)
      rc = ds1_manager_clone(ds1_idx, d.group_idx, d.def, d.file_name, error, sizeof(error));
   else
      rc = ds1_manager_create_empty(d.group_idx, d.def, d.file_name, d.width, d.height,
                                    error, sizeof(error));
   if (rc != 0)
   {
      al_show_native_message_box(a5_display, clone ? "Clone Map" : "New Map",
         "The map wasn't created. Nothing was changed.", error, NULL,
         ALLEGRO_MESSAGEBOX_WARN);
      input_suppress_held();
   }
}
```

`ds1_manager_suggest_name` writes with `sprintf` and no bound. Its names are short in practice, but `d.file_name` is 64 bytes and a long root would overflow. Pass a 128-byte local to it, and `snprintf` into `d.file_name`.

Add `CMD_MAP_NEW` and `CMD_MAP_CLONE` to the list of dialog-capable commands in `interfac_user_handler`'s `switch (cmd)`, since they now always open one.

- [ ] **Step 3: State-based confirmations**

Replace `interface_delete_map`, keeping its name and the pinned shape. It covers the New, Modified and Game rows; the two restores follow it:

```c
/* Ask, naming the map and what will happen. */
static int interface_confirm_map_op(const char *title, const char *heading_fmt,
                                    const char *path, const char *text, int warn_unsaved)
{
   char heading[600], body[1024];
   int rc, k, dirty = 0;

   for (k = 0; k < DS1_MAX && warn_unsaved && !dirty; k++)
      dirty = glb_ds1[k].name[0] != 0 && map_is_dirty(k) &&
              map_state_path_matches(glb_ds1[k].name, path);

   snprintf(heading, sizeof(heading), heading_fmt, path);
   snprintf(body, sizeof(body), "%s%s", text,
            dirty ? "\n\nThis map has unsaved edits, which won't be saved." : "");
   rc = al_show_native_message_box(a5_display, title, heading, body, NULL,
                                   ALLEGRO_MESSAGEBOX_YES_NO | ALLEGRO_MESSAGEBOX_WARN);
   input_suppress_held();
   input_discard_pending();
   return rc == 1;
}

static void interface_map_op_failed(const char *title, const char *heading, const char *error)
{
   al_show_native_message_box(a5_display, title, heading, error, NULL, ALLEGRO_MESSAGEBOX_ERROR);
   input_suppress_held();
   input_discard_pending();
}

/* Delete a New map from the project. Undo can't bring it back: this is a file
 * operation, not a map edit, so it takes a confirmation, and Restore from Backup. */
static void interface_delete_map(int gi, int ei)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   char error[512] = "", path[256];

   snprintf(path, sizeof(path), "%s", ab->groups[gi].entries[ei].ds1_path);
   if (!interface_confirm_map_op("Delete Map", "Delete %s?", path,
         "This moves the map's file to the project's backup folder and clears its "
         "slot in LvlPrest.txt. Map > Restore from Backup brings it back.", TRUE))
      return;

   /* ds1_manager_delete removes the row, then finds the open map's row again by
      path. If this was the open map, it has none now and stays in memory. */
   if (ds1_manager_delete(gi, ei, error, sizeof(error)) != 0)
      interface_map_op_failed("Delete Map", "The map wasn't deleted.", error);
}

static void interface_revert_map(int gi, int ei)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   char error[512] = "", path[256];
   int was_open = ab->loaded_group == gi && ab->loaded_entry == ei;
   int def = ab->groups[gi].entries[ei].lvlprest_def;

   snprintf(path, sizeof(path), "%s", ab->groups[gi].entries[ei].ds1_path);
   if (!interface_confirm_map_op("Revert to Game Version", "Revert %s to the game's version?", path,
         "This moves the project's copy to the project's backup folder. The game's "
         "version is used from now on. Map > Restore from Backup brings the copy back.", TRUE))
      return;

   if (ds1_manager_revert(gi, ei, error, sizeof(error)) != 0)
   {
      interface_map_op_failed("Revert to Game Version", "The map wasn't reverted.", error);
      return;
   }

   /* Show what was just reverted to. Other open maps with unsaved edits prompt here,
      and a cancel leaves this one showing until it's reopened. */
   if (was_open && area_browser_find_row(path, def, &gi, &ei))
   {
      int k;
      for (k = 0; k < DS1_MAX; k++)
         if (glb_ds1[k].name[0] != 0 && map_state_path_matches(glb_ds1[k].name, path))
            map_dirty_mark_saved(k);
      area_browser_switch_single(gi, ei);
   }
}

static void interface_remove_map(int gi, int ei)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   char error[512] = "", path[256];

   if (!map_save_ui_require_project())
      return;
   snprintf(path, sizeof(path), "%s", ab->groups[gi].entries[ei].ds1_path);
   if (!interface_confirm_map_op("Remove from Project", "Remove %s from the project?", path,
         "This clears its slot in the project's LvlPrest.txt, so the game won't use "
         "this map with your mod. The game's files don't change. Map > Restore Slot "
         "puts it back.", FALSE))
      return;

   if (ds1_manager_remove_slot(gi, ei, error, sizeof(error)) != 0)
      interface_map_op_failed("Remove from Project", "The slot wasn't cleared.", error);
}

static void interface_restore_slot(int gi, int ei)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   char error[512] = "", path[256];

   snprintf(path, sizeof(path), "%s", ab->groups[gi].entries[ei].ds1_path);
   if (!interface_confirm_map_op("Restore Slot", "Restore %s's slot?", path,
         "This puts the game's value back in the project's LvlPrest.txt.", FALSE))
      return;
   if (ds1_manager_restore_slot(gi, ei, error, sizeof(error)) != 0)
      interface_map_op_failed("Restore Slot", "The slot wasn't restored.", error);
}

static void interface_restore_backup(int gi, int ei)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   char error[512] = "", path[256];
   int i, j, found = 0;

   snprintf(path, sizeof(path), "%s", ab->groups[gi].entries[ei].ds1_path);
   if (!map_save_ui_require_project())
      return;

   /* Opening a project rebuilds the rows, so find this one again. */
   for (i = 0; i < ab->group_count && !found; i++)
      for (j = 0; ab->groups[i].is_backup && j < ab->groups[i].entry_count && !found; j++)
         if (strcmp(ab->groups[i].entries[j].ds1_path, path) == 0)
         {
            gi = i;
            ei = j;
            found = 1;
         }
   if (!found)
      return;

   if (!interface_confirm_map_op("Restore from Backup", "Restore the backup %s?", path,
         "This moves it back to the project and registers it in LvlPrest.txt if needed.", FALSE))
      return;
   if (ds1_manager_restore(gi, ei, error, sizeof(error)) != 0)
      interface_map_op_failed("Restore from Backup", "The backup wasn't restored.", error);
}
```

The `Ctrl+Del` confirmation on a Modified map's open row marks that map saved only after the file has moved and the user said yes, so the unload guard doesn't ask about edits the box already warned of. That's deliberate.

`interface_delete_map`'s "no project" box goes: `CMD_MAP_DELETE` is available only for New rows, which need a project.

`commands_loaded_after_delete` has no caller now. Delete it from `commands.c` and `commands.h`, and its tests from `test/test_commands.c`.

In `interface_run_command`, add:

```c
   case CMD_MAP_DELETE:
      interface_delete_map(ab->loaded_group, ab->loaded_entry);
      break;
   case CMD_MAP_REVERT:
      interface_revert_map(ab->loaded_group, ab->loaded_entry);
      break;
   case CMD_MAP_REMOVE:
      interface_remove_map(ab->loaded_group, ab->loaded_entry);
      break;
   case CMD_MAP_RESTORE_SLOT:
      interface_restore_slot(ab->loaded_group, ab->loaded_entry);
      break;
   case CMD_MAP_RESTORE_BACKUP:
      interface_restore_backup(ab->loaded_group, ab->loaded_entry);
      break;
```

- [ ] **Step 4: The browser's right-click menu**

The context menu near `interface.c:900` offers one item for regular rows and two for backups. Make the regular row's item follow state, with this helper above the main loop:

```c
/* The command a right click on this row offers, by its state. */
static const char *interface_row_action(int state, int *cmd)
{
   switch (state)
   {
   case MAP_STATE_NEW:      *cmd = CMD_MAP_DELETE;       return "Delete map...";
   case MAP_STATE_MODIFIED: *cmd = CMD_MAP_REVERT;       return "Revert to game version...";
   case MAP_STATE_REMOVED:  *cmd = CMD_MAP_RESTORE_SLOT; return "Restore slot...";
   default:                 *cmd = CMD_MAP_REMOVE;       return "Remove from project...";
   }
}
```

Use it for the drawn text in place of the fixed `"Delete map..."`, and widen `menu_w` if `"Revert to game version..."` doesn't fit. Replace the click handling for a non-backup row:

```c
                        if (!rc_is_backup)
                        {
                           int row_cmd;
                           interface_row_action(ab->groups[rc_gi].entries[rc_ei].state, &row_cmd);
                           switch (row_cmd)
                           {
                           case CMD_MAP_DELETE:       interface_delete_map(rc_gi, rc_ei);   break;
                           case CMD_MAP_REVERT:       interface_revert_map(rc_gi, rc_ei);   break;
                           case CMD_MAP_RESTORE_SLOT: interface_restore_slot(rc_gi, rc_ei); break;
                           default:                   interface_remove_map(rc_gi, rc_ei);   break;
                           }
                        }
                        else if (menu_sel == 0)
                           interface_restore_backup(rc_gi, rc_ei);
```

Read how the menu's `ab` variable is named in that scope, and any row-state refresh it needs.

- [ ] **Step 5: Update the pinned route test**

In `test/python/test_delete_key_routing.py`, `MapDeleteRouteTests` keeps checking `ds1_manager_delete(` is called once and only from `interface_delete_map`. `test_map_delete_asks_first_and_stops_unless_the_answer_is_yes` currently looks for `ALLEGRO_MESSAGEBOX_YES_NO` in that function's body; change it to look for `interface_confirm_map_op(` before `ds1_manager_delete(`, with a `return` between them:

```python
    def test_map_delete_asks_first_and_stops_unless_the_answer_is_yes(self):
        body = function_body(INTERFACE, "interface_delete_map")
        asked = body.index("interface_confirm_map_op(")
        deleted = body.index("ds1_manager_delete(")
        self.assertLess(asked, deleted)
        self.assertIn("return;", body[asked:deleted])
```

Read the rest of that class. Any assertion about the old body text moves to `interface_confirm_map_op` in the same way, keeping its meaning. The check that backups confirm stays as it is.

- [ ] **Step 6: Build and run the tests**

Run: `cmake --build tmp/build-lifecycle --config Release -- /m /v:q /nologo`, then `ctest --test-dir tmp/build-lifecycle -C Release -L portable -j 3 --timeout 300`.
Expected: all pass.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/ui/map_dialog.c src/ui/map_dialog.h src/ui/interface.c src/ui/commands.c src/ui/commands.h src/ui/map_save_ui.c src/ui/map_save_ui.h test/test_commands.c test/python/test_delete_key_routing.py
git commit -m "Add the new map dialog and state-based map confirmations"
```

---

### Task 7: Docs and end-to-end check

**Files:**
- Modify: `README.md`, `docs/getting-started/manual.md`, `docs/testing/native-release-checklist.md`, `CHANGELOG.md`

- [ ] **Step 1: Update the docs**

`README.md`: the Map section says New and Clone open a dialog; "clone" copies the map as it is in memory, unsaved edits included, and works on MPQ maps. Remove "copies the saved file". `Ctrl+Del` runs Delete, Revert or Remove by the map's state.

`docs/getting-started/manual.md`: add a subsection under Menus, "Deleting maps", with the spec's table:

```markdown
| Map's mark | Command | What happens | Undo by |
|---|---|---|---|
| `N` | Delete Map | The file moves to the project's `backup` folder and its slot is cleared | Restore from Backup |
| `M` | Revert to Game Version | The project's copy moves to the `backup` folder | Restore from Backup |
| none | Remove from Project | Its slot is cleared in the project's `LvlPrest.txt` | Restore Slot |
| struck through | Restore Slot | The game's value goes back in the slot | Remove from Project |
```

Add one paragraph: "These act on the open map. Right-click a row in the area browser to do the same for any row. A struck-through row can't be opened until its slot is restored." State that backups now live in `<project>/backup`, and that backups from older versions in the editor's own `backup` folder stay listed and move into the project when restored. Update the line near 752 about deleting.

`docs/testing/native-release-checklist.md`: extend E15, keeping one item and total 15, with: Map > New with no project asks for one; New creates a map that opens and shows `N`; a bad, duplicate or too-long name keeps the dialog up with a message; a full Def row says so; Clone of a map with unsaved edits keeps those edits in the copy and leaves the original unsaved; Clone of an MPQ-only map works; `Ctrl+Del` on `N`, `M` and unmarked rows offers Delete, Revert and Remove; a Removed row is struck through, won't open, and Restore Slot brings it back; Restore from Backup round trips; `<project>/backup` holds the backups; nothing changes in the game folder.

`CHANGELOG.md`, in the unreleased section, plain language:

```markdown
- **Map > New and Clone open a dialog** for the name, area, tileset and size.
  Clone copies the map as it is in memory, so it includes unsaved edits and works
  on maps that exist only in an MPQ. A failed table write removes the new map.
- **Deleting follows the map's state.** A map new to the project is deleted, a
  modified one is reverted to the game's version, and a game map is removed from
  the project's table, where it stays listed, struck through, until its slot is
  restored. Each one asks first and can be undone.
- **Backups now live in the project's `backup` folder**, and a second backup of a
  map on the same day no longer replaces the first.
```

- [ ] **Step 2: Run everything**

Windows: `ctest --test-dir tmp/build-lifecycle -C Release -L portable -j 3 --timeout 300`.
Linux: `wsl.exe bash tmp/wsl/build.sh` from PowerShell, after pointing its `SRC=` line at this worktree. Don't run these two at once: they share `bin/`.
Expected: Windows all pass; Linux fails only the Pillow and git-in-WSL environment tests.

- [ ] **Step 3: Commit**

```bash
git add README.md docs/getting-started/manual.md docs/testing/native-release-checklist.md CHANGELOG.md
git commit -m "Document creating and deleting maps by state"
```

- [ ] **Step 4: Controller's windowed GUI pass (Linux WSLg)**

Windowed only, with a scratch project and no writes to `C:\Diablo2`. Confirm with `d.sh state` at the end. Checks:

1. `Ctrl+N` with no project asks for one, then shows the dialog with the suggested name.
2. Duplicate name, `CON`, a name with a space, and width 0 each keep the dialog up with a message. `Esc` cancels with nothing written.
3. OK creates the file and `Global/Excel/LvlPrest.txt` in the project; the row shows `N`; opening it works.
4. Edit it, don't save, `Ctrl+D`: the copy has the edit, the original still shows `*`.
5. `Ctrl+Del` on the `N` row: box names the map, No leaves it, Yes moves it to `<project>/backup/...` and the slot is `0` in the table.
6. Right click its backup row: Restore brings the file and slot back.
7. Open a game map, edit, `Ctrl+S` to get `M`. `Ctrl+Del` offers Revert; Yes moves the copy, the row loses `M` and the screen shows the game's version.
8. `Ctrl+Del` on an unmarked game map offers Remove; Yes strikes the row; the row won't open; Map > Restore Slot (with the map still open) or a right click puts the table back byte for byte against a saved copy.
9. Close and reopen the project: the Removed row is still struck through.
10. Delete a recreated map of the same name on the same day: a `_2` backup folder appears, and the first stays.
