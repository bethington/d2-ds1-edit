# Map Lifecycle Step 3 Fix: The Files Column Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every table the editor writes safe for the game, by keeping `LvlPrest.txt`'s `Files` column true when maps are added, removed and restored.

**Architecture:** `core/lvlprest_text.c` learns what a "simple" row is and how to insert and remove a map in one, moving later cells and updating `Files`. `core/map_ops.c` uses only those edits and refuses everything else with a sentence. The manager, dialog and interface ask before they confirm.

**Tech Stack:** C99, Unity tests, CMake. MSVC C mode (Win32), GCC and Clang.

**Spec:** `docs/superpowers/specs/2026-11-02-map-lifecycle-design.md`, amended by the "How the Game Uses File Cells" section of `docs/superpowers/specs/2026-11-03-map-lifecycle-gate-results.md`.

## Global Constraints

- A table the editor writes must never hold a `0` or blank cell at an index below `Files`. That crashes the game when the rooms generate.
- "Create is all-or-nothing: on a failed table write, the new `.ds1` is removed."
- "Every one of these confirms first, naming the map and what will happen."
- Nothing may write to the game install. Tests never write outside their temp folder.
- A fleet slot's MPQs are hard links. Never write through one.
- GUI runs stay windowed. All three platforms ship together.
- Commit trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Decisions this plan makes

| Topic | Decision | Why |
|---|---|---|
| A simple row | `Files` is 1 to 6, cells 1 to `Files` are filled, and the rest are empty | 1,007 of the game's 1,088 rows with a map. The game's random pick is known to read these cells |
| Create on a row that isn't simple, or is full | Refused with a message | `Files=0` rows and rows like Wild Border choose cells by their own rules. Raising `Files` on Wild Border crashes the game |
| Remove on a row's only map, or a row that isn't simple | Refused with a message, and the command stays available | The user chose this. The box says to edit the map instead |
| Delete on a New map | Same rule, through the same function | A New map only exists in a row Create raised, so a row left with no maps can't arise from the editor |
| Restore Slot | Inserts at the game's index, or the end if fewer maps are left, and raises `Files` | A single Remove and Restore round trips byte for byte. Two Removes restored out of order change the order only, which the game doesn't care about |
| The same file twice in a row | Remove takes every copy, and is refused if that leaves no map | 4 game rows do this |
| Cell length | `MAP_OPS_TABLE_PATH_MAX` becomes 41 | The game's `wsprintf` into a 60-byte buffer overflows past that, by static reading |
| Tables from the merged step 3 | No repair | It was never released. Such a row is simply "not simple" and gets refused |

---

### Task 1: Prove raising Files works on an ordinary row (controller)

The whole Create fix rests on this. **Run it first, by hand, in the controller.** If the "real" run crashes, stop and ask the user: Create would have no safe way to add a map, and the design changes.

**Files:**
- Modify: `tmp/gate/slot_table.py` (git-ignored)

- [ ] **Step 1: Add the cases**

In `tmp/gate/slot_table.py`, process every row instead of `DEFS`, and add:

```python
def raise_ordinary(row, last):
    """Add a cell to a row whose Files equals its filled cells."""
    cells = row[CELL1:CELL1 + 6]
    filled = sum(1 for c in cells if c not in ("", "0"))
    try:
        files = int(row[FILES])
    except ValueError:
        return False
    if files < 1 or files > 5 or files != filled:
        return False
    row[CELL1 + files] = row[CELL1] if last == "real" else "Act1/Outdoors/GateNoSuchMap.ds1"
    row[FILES] = str(files + 1)
    return True
```

Cases `raised-ordinary-real` and `raised-ordinary-missing` call it on every row with a numeric Def. Keep the `DEFS` filter for the older cases.

- [ ] **Step 2: Run `raised-ordinary-real`**

Start `D2Fleet.exe serve`. `fleet_launch` `lod-113c`, args `-w -direct -txt`, override `{"auto_enter":"none"}`, slot 0. Run `slot_table.py raised-ordinary-real 0`. Wait until `fleet_state` says title, click Single Player (400,307), wait 3 s, click TplBarbarian (430,305), wait 1 s, click OK (690,555). Walk with `fleet_click` to level 2, reading `fleet_map`'s `level.exits` for the way out, and walk on for at least 60 s of new rooms.

Expected: `D2*.txt` in the slot has no `UNHANDLED EXCEPTION`, `lvlprest.bin` was written, and `fleet_state` shows `level_id` 2.

- [ ] **Step 3: Run `raised-ordinary-missing`**

Same walk. Expected: a crash at `D2Common+0xA57C`, which proves the random pick reaches the new cell. No crash in a long walk means the pick wasn't reached, so the real run proved nothing: extend the walk.

- [ ] **Step 4: Record it**

Add the result to `docs/superpowers/specs/2026-11-03-map-lifecycle-gate-results.md`, under "How the Game Uses File Cells", and remove the matching "Not Proved Yet" bullet. Close the member and stop the console.

---

### Task 2: Row edits in lvlprest_text

**Files:**
- Modify: `src/core/lvlprest_text.c`, `src/core/lvlprest_text.h`
- Test: `test/test_lvlprest_text.c`

**Interfaces:**
- Produces:
  - `int lvlprest_files_value(char *buf, int def)`: the row's `Files`, or -1 if there's no such row or column.
  - `int lvlprest_row_is_simple(char *buf, int def)`.
  - `char *lvlprest_with_map_inserted(char *buf, int def, int slot, const char *value, long *out_len)`.
  - `char *lvlprest_with_map_removed(char *buf, int def, int slot, long *out_len)`.
  Both editors return new `malloc`'d text, or NULL if the row isn't simple or the edit doesn't fit.

- [ ] **Step 1: Write the failing tests**

Add to `test/test_lvlprest_text.c`, and to `main`:

```c
static char rows[] =
   "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\tDt1Mask\r\n"
   "Two\t1\t2\ta.ds1\tb.ds1\t0\t0\t0\t0\t5\r\n"
   "One\t2\t1\tonly.ds1\t0\t0\t0\t0\t0\t5\r\n"
   "Own\t3\t0\tx.ds1\ty.ds1\t0\t0\t0\t0\t5\r\n"
   "Tail\t4\t3\tp.ds1\tq.ds1\tr.ds1\ts.ds1\tt.ds1\t0\t5\r\n"
   "Hole\t5\t3\ta.ds1\t0\tc.ds1\t0\t0\t0\t5\r\n"
   "Full\t6\t6\ta.ds1\tb.ds1\tc.ds1\td.ds1\te.ds1\tf.ds1\t5\r\n";

void test_files_value_reads_the_row(void)
{
   TEST_ASSERT_EQUAL_INT(2, lvlprest_files_value(rows, 1));
   TEST_ASSERT_EQUAL_INT(0, lvlprest_files_value(rows, 3));
   TEST_ASSERT_EQUAL_INT(-1, lvlprest_files_value(rows, 99));
}

void test_a_simple_row_counts_exactly_its_filled_cells(void)
{
   TEST_ASSERT_TRUE(lvlprest_row_is_simple(rows, 1));
   TEST_ASSERT_TRUE(lvlprest_row_is_simple(rows, 2));
   TEST_ASSERT_TRUE(lvlprest_row_is_simple(rows, 6));
   TEST_ASSERT_FALSE(lvlprest_row_is_simple(rows, 3));   /* Files 0 */
   TEST_ASSERT_FALSE(lvlprest_row_is_simple(rows, 4));   /* cells past Files */
   TEST_ASSERT_FALSE(lvlprest_row_is_simple(rows, 5));   /* a hole */
   TEST_ASSERT_FALSE(lvlprest_row_is_simple(rows, 99));
}

void test_a_table_with_no_files_column_has_no_simple_rows(void)
{
   TEST_ASSERT_FALSE(lvlprest_row_is_simple(table, 1));
}

void test_inserting_at_the_end_raises_files(void)
{
   long len = 0;
   char *out = lvlprest_with_map_inserted(rows, 1, 3, "new.ds1", &len);
   char cell[32];

   TEST_ASSERT_NOT_NULL(out);
   TEST_ASSERT_EQUAL_INT(3, lvlprest_files_value(out, 1));
   TEST_ASSERT_TRUE(lvlprest_get_cell(out, 1, 3, cell, sizeof(cell)));
   TEST_ASSERT_EQUAL_STRING("new.ds1", cell);
   TEST_ASSERT_TRUE(lvlprest_row_is_simple(out, 1));
   free(out);
}

void test_inserting_in_the_middle_moves_later_cells_down(void)
{
   long len = 0;
   char *out = lvlprest_with_map_inserted(rows, 1, 1, "new.ds1", &len);
   char cell[32];

   TEST_ASSERT_NOT_NULL(out);
   TEST_ASSERT_TRUE(lvlprest_get_cell(out, 1, 1, cell, sizeof(cell)));
   TEST_ASSERT_EQUAL_STRING("new.ds1", cell);
   TEST_ASSERT_TRUE(lvlprest_get_cell(out, 1, 3, cell, sizeof(cell)));
   TEST_ASSERT_EQUAL_STRING("b.ds1", cell);
   free(out);
}

void test_inserting_past_the_end_lands_at_the_end(void)
{
   long len = 0;
   char *out = lvlprest_with_map_inserted(rows, 2, 5, "new.ds1", &len);
   char cell[32];

   TEST_ASSERT_NOT_NULL(out);
   TEST_ASSERT_TRUE(lvlprest_get_cell(out, 2, 2, cell, sizeof(cell)));
   TEST_ASSERT_EQUAL_STRING("new.ds1", cell);
   TEST_ASSERT_EQUAL_INT(2, lvlprest_files_value(out, 2));
   free(out);
}

void test_inserting_is_refused_on_full_and_unusual_rows(void)
{
   long len = 0;
   TEST_ASSERT_NULL(lvlprest_with_map_inserted(rows, 6, 1, "new.ds1", &len));
   TEST_ASSERT_NULL(lvlprest_with_map_inserted(rows, 3, 1, "new.ds1", &len));
   TEST_ASSERT_NULL(lvlprest_with_map_inserted(rows, 4, 1, "new.ds1", &len));
   TEST_ASSERT_NULL(lvlprest_with_map_inserted(rows, 5, 2, "new.ds1", &len));
}

void test_removing_moves_later_cells_up_and_lowers_files(void)
{
   long len = 0;
   char *out = lvlprest_with_map_removed(rows, 1, 1, &len);
   char cell[32];

   TEST_ASSERT_NOT_NULL(out);
   TEST_ASSERT_EQUAL_INT(1, lvlprest_files_value(out, 1));
   TEST_ASSERT_TRUE(lvlprest_get_cell(out, 1, 1, cell, sizeof(cell)));
   TEST_ASSERT_EQUAL_STRING("b.ds1", cell);
   TEST_ASSERT_TRUE(lvlprest_get_cell(out, 1, 2, cell, sizeof(cell)));
   TEST_ASSERT_EQUAL_STRING("0", cell);
   TEST_ASSERT_TRUE(lvlprest_row_is_simple(out, 1));
   free(out);
}

void test_removing_is_refused_for_an_only_map_and_unusual_rows(void)
{
   long len = 0;
   TEST_ASSERT_NULL(lvlprest_with_map_removed(rows, 2, 1, &len));
   TEST_ASSERT_NULL(lvlprest_with_map_removed(rows, 3, 1, &len));
   TEST_ASSERT_NULL(lvlprest_with_map_removed(rows, 4, 1, &len));
   TEST_ASSERT_NULL(lvlprest_with_map_removed(rows, 5, 1, &len));
   TEST_ASSERT_NULL(lvlprest_with_map_removed(rows, 1, 3, &len));   /* an empty cell */
}

void test_remove_then_insert_at_the_same_slot_is_byte_identical(void)
{
   long len = 0;
   char *gone = lvlprest_with_map_removed(rows, 6, 2, &len);
   char *back;

   TEST_ASSERT_NOT_NULL(gone);
   back = lvlprest_with_map_inserted(gone, 6, 2, "b.ds1", &len);
   TEST_ASSERT_NOT_NULL(back);
   TEST_ASSERT_EQUAL_STRING(rows, back);
   free(gone);
   free(back);
}
```

- [ ] **Step 2: Run to see it fail**

Run: `cmake --build tmp/build-lifecycle --config Release --target test_lvlprest_text -- /m /v:q /nologo`
Expected: FAIL, `lvlprest_files_value` undefined.

- [ ] **Step 3: Implement**

Add to `lvlprest_text.h`:

```c
/* The row's Files column, the count the game's random pick uses. -1 if there's no
   such row or column. */
int lvlprest_files_value(char * buf, int def);

/* 1 if Files is 1 to 6, File1 to File<Files> are filled and the rest are empty.
   The game's random pick reads these cells. It indexes a cell blindly, so a blank
   below Files crashes it, and rows that aren't simple choose by their own rules. */
int lvlprest_row_is_simple(char * buf, int def);

/* The table with a map added to a simple row, at `slot` or at the end if that is
   past the last map, later cells moved along and Files raised. New malloc'd text
   with a NUL after *out_len bytes; NULL if the row isn't simple or is full. */
char * lvlprest_with_map_inserted(char * buf, int def, int slot, const char * value,
                                  long * out_len);

/* The table with File column `slot` taken out of a simple row, later cells moved
   back and Files lowered. NULL if the row isn't simple, the slot holds no map, or
   it is the row's only map. */
char * lvlprest_with_map_removed(char * buf, int def, int slot, long * out_len);
```

In `lvlprest_text.c`, split `find_cell`, so a column can be named:

```c
/* The bounds of a named column in this Def's row. 1 if found. */
static int find_named_cell(char * buf, int def, const char * col_name, char ** start, char ** end)
{
   int def_col, col_at, col = 0;
   char * row, * row_end, * cell;

   def_col = lvlprest_find_col(buf, "Def");
   col_at = lvlprest_find_col(buf, col_name);
   if (def_col < 0 || col_at < 0)
      return 0;
   row = lvlprest_find_row(buf, def_col, def, &row_end);
   if (row == NULL)
      return 0;

   cell = row;
   while (col < col_at && *cell != '\0' && *cell != '\n' && *cell != '\r')
   {
      if (*cell == '\t') col++;
      cell++;
   }
   if (col != col_at)
      return 0;

   *start = cell;
   while (*cell != '\0' && *cell != '\t' && *cell != '\n' && *cell != '\r')
      cell++;
   *end = cell;
   return 1;
}

/* The bounds of File column `slot` in this Def's row. 1 if found. */
static int find_cell(char * buf, int def, int slot, char ** start, char ** end)
{
   char col_name[16];

   if (slot < 1 || slot > 6)
      return 0;
   sprintf(col_name, "File%d", slot);
   return find_named_cell(buf, def, col_name, start, end);
}
```

Add at the end of the file:

```c
#define ROW_CELL_CAP 256

int lvlprest_files_value(char * buf, int def)
{
   char * start, * end;
   char num[16];
   int n;

   if (!find_named_cell(buf, def, "Files", &start, &end))
      return -1;
   n = (int)(end - start);
   if (n >= (int) sizeof(num))
      return -1;
   memcpy(num, start, n);
   num[n] = '\0';
   return atoi(num);
}

int lvlprest_row_is_simple(char * buf, int def)
{
   int files = lvlprest_files_value(buf, def);
   char * start, * end;
   int f;

   if (files < 1 || files > 6)
      return 0;
   for (f = 1; f <= 6; f++)
   {
      if (!find_cell(buf, def, f, &start, &end))
         return 0;
      if (cell_is_empty(start, end) != (f > files))
         return 0;
   }
   return 1;
}

static int read_row_cells(char * buf, int def, char cells[6][ROW_CELL_CAP])
{
   int f;

   for (f = 1; f <= 6; f++)
      if (!lvlprest_get_cell(buf, def, f, cells[f - 1], ROW_CELL_CAP))
         return 0;
   return 1;
}

/* A new table with this row's Files and six File columns replaced. */
static char * table_with_row(char * buf, int def, char cells[6][ROW_CELL_CAP], int files,
                             long * out_len)
{
   char * start, * end, * text, * next;
   char num[16];
   long before, after, val;
   int f;

   if (!find_named_cell(buf, def, "Files", &start, &end))
      return NULL;
   sprintf(num, "%d", files);
   val = (long) strlen(num);
   before = (long)(start - buf);
   after = (long) strlen(end);
   text = (char *) malloc(before + val + after + 1);
   if (text == NULL)
      return NULL;
   memcpy(text, buf, before);
   memcpy(text + before, num, val);
   memcpy(text + before + val, end, after + 1);
   *out_len = before + val + after;

   for (f = 1; f <= 6; f++)
   {
      next = lvlprest_with_cell(text, def, f, cells[f - 1], out_len);
      free(text);
      if (next == NULL)
         return NULL;
      text = next;
   }
   return text;
}

char * lvlprest_with_map_inserted(char * buf, int def, int slot, const char * value,
                                  long * out_len)
{
   char cells[6][ROW_CELL_CAP];
   int files, f;

   if (!lvlprest_row_is_simple(buf, def) || strlen(value) >= ROW_CELL_CAP)
      return NULL;
   files = lvlprest_files_value(buf, def);
   if (files >= 6 || slot < 1 || !read_row_cells(buf, def, cells))
      return NULL;
   if (slot > files + 1)
      slot = files + 1;
   for (f = files; f >= slot; f--)
      strcpy(cells[f], cells[f - 1]);
   strcpy(cells[slot - 1], value);
   return table_with_row(buf, def, cells, files + 1, out_len);
}

char * lvlprest_with_map_removed(char * buf, int def, int slot, long * out_len)
{
   char cells[6][ROW_CELL_CAP];
   int files, f;

   if (!lvlprest_row_is_simple(buf, def))
      return NULL;
   files = lvlprest_files_value(buf, def);
   if (files < 2 || slot < 1 || slot > files || !read_row_cells(buf, def, cells))
      return NULL;
   for (f = slot; f < files; f++)
      strcpy(cells[f - 1], cells[f]);
   strcpy(cells[files - 1], "0");
   return table_with_row(buf, def, cells, files - 1, out_len);
}
```

Replace the `find_cell` definition with the two above, keeping them where `find_cell` is now, above `cell_is_empty`'s users. `cell_is_empty` returns 0 or 1, as the comparison needs.

- [ ] **Step 4: Run the tests**

Run: `ctest --test-dir tmp/build-lifecycle -C Release -R "^lvlprest_text$" --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core/lvlprest_text.c src/core/lvlprest_text.h test/test_lvlprest_text.c
git commit -m "Insert and remove maps in a LvlPrest row, keeping Files true"
```

---

### Task 3: map_ops writes only safe tables

**Files:**
- Modify: `src/core/map_ops.c`, `src/core/map_ops.h`
- Test: `test/test_map_ops.c`

**Interfaces:**
- Consumes: Task 2.
- Produces: `int map_ops_check_register(char *table, int def, char *error, int cap);` and `int map_ops_check_unregister(char *table, int def, const char *ds1_path, char *error, int cap);`, each 0 or -1 with a sentence. `map_ops_unregister`, `map_ops_register` and `map_ops_restore_slot` keep their signatures.

- [ ] **Step 1: Update the fixture and write the failing tests**

Replace `GAME` in `test/test_map_ops.c`:

```c
static const char GAME[] =
   "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
   "Town\t1\t2\tAct1\\Town\\TownN1.ds1\tAct1\\Town\\TownE1.ds1\t0\t0\t0\t0\r\n"
   "Full\t2\t6\ta.ds1\tb.ds1\tc.ds1\td.ds1\te.ds1\tf.ds1\r\n"
   "Only\t3\t1\tAct1\\Only.ds1\t0\t0\t0\t0\t0\r\n"
   "Own\t4\t0\tAct1\\Own.ds1\t0\t0\t0\t0\t0\r\n"
   "Tail\t5\t1\tAct1\\Tail1.ds1\tAct1\\Tail2.ds1\t0\t0\t0\t0\r\n";
```

Add these, and to `main`:

```c
void test_register_raises_files(void)
{
   char *t;

   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, GAME, 1, "Act1/Town/New.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_INT(3, lvlprest_files_value(t, 1));
   TEST_ASSERT_EQUAL_INT(3, lvlprest_find_path_slot(t, 1, "Act1/Town/New.ds1"));
   TEST_ASSERT_TRUE(lvlprest_row_is_simple(t, 1));
   free(t);
}

void test_register_refuses_rows_that_choose_their_own_maps(void)
{
   TEST_ASSERT_EQUAL_INT(-1, map_ops_register(dir, GAME, 4, "x.ds1", err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "own"));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_register(dir, GAME, 5, "x.ds1", err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
}

void test_register_refuses_a_path_past_the_games_limit(void)
{
   char path[64];

   memset(path, 'a', sizeof(path));
   memcpy(path + 42 - 4, ".ds1", 5);   /* 42 characters */
   TEST_ASSERT_EQUAL_INT(-1, map_ops_register(dir, GAME, 1, path, err, sizeof(err)));
   path[41 - 4] = 0;
   strcat(path, ".ds1");               /* 41 characters */
   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, GAME, 1, path, err, sizeof(err)));
}

void test_create_that_cant_register_removes_the_map(void)
{
   TEST_ASSERT_EQUAL_INT(-1, map_ops_create_empty(dir, GAME, 4, "New.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Global/Tiles/New.ds1"));
}

void test_unregister_leaves_no_hole_and_lowers_files(void)
{
   char *t;

   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, GAME, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_INT(1, lvlprest_files_value(t, 1));
   TEST_ASSERT_EQUAL_INT(1, lvlprest_find_path_slot(t, 1, "Act1/Town/TownE1.ds1"));
   TEST_ASSERT_TRUE(lvlprest_row_is_simple(t, 1));
   free(t);
}

void test_unregister_refuses_a_rows_only_map(void)
{
   TEST_ASSERT_EQUAL_INT(-1, map_ops_unregister(dir, GAME, 3, "Act1/Only.ds1", err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "only map"));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
}

void test_unregister_refuses_rows_that_choose_their_own_maps(void)
{
   TEST_ASSERT_EQUAL_INT(-1, map_ops_unregister(dir, GAME, 4, "Act1/Own.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_unregister(dir, GAME, 5, "Act1/Tail2.ds1", err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
}

void test_a_row_listing_only_one_file_twice_counts_as_an_only_map(void)
{
   static const char TWICE[] =
      "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
      "Dup\t1\t2\td.ds1\td.ds1\t0\t0\t0\t0\r\n";

   TEST_ASSERT_EQUAL_INT(-1, map_ops_unregister(dir, TWICE, 1, "d.ds1", err, sizeof(err)));
}

void test_restore_slot_raises_files_again(void)
{
   char game[sizeof(GAME)];
   char *t;

   memcpy(game, GAME, sizeof(GAME));
   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, GAME, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_slot(dir, GAME, game, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_STRING(GAME, t);
   free(t);
}
```

Existing tests must follow the new fixture and rules, keeping what each one means:
- Tests using Def 1 with one map, such as `test_create_writes_the_map_and_registers_it`, work as they are, since Def 1 now holds two.
- `test_register_seeds_the_table_then_fills_a_slot`: the new map lands in slot 3.
- `test_restore_uses_another_slot_when_its_own_was_taken`: after removing `TownN1` and registering `New`, the row is `TownE1, New`, and restoring puts `TownN1` at slot 1, so `New` ends at slot 3 and `Files` is 3. Rename it `test_restore_goes_back_to_its_own_place_among_new_maps`.
- `test_unregister_then_restore_returns_the_table_to_the_original`, `test_the_same_file_twice_in_a_row_round_trips`, `test_restore_puts_back_only_the_copies_that_are_missing`, `test_unregister_leaves_a_namesake_in_another_folder`, `test_namesakes_round_trip_one_at_a_time` and `test_restore_slot_wants_the_game_to_list_the_whole_path`: give each local table a `Files` column whose value equals its filled cells, with at least one other map in the row. The byte-for-byte assertions stay.
- Any test whose table has no `Files` column: add one.

Any test that writes a table with a row and `0` below `Files` is wrong by this plan: fix the test.

- [ ] **Step 2: Run to see it fail**

Run: `cmake --build tmp/build-lifecycle --config Release --target test_map_ops -- /m /v:q /nologo`, then `ctest --test-dir tmp/build-lifecycle -C Release -R "^map_ops$" --output-on-failure`.
Expected: FAIL on the new tests.

- [ ] **Step 3: Implement**

In `map_ops.h`, change the constant and its comment:

```c
/* The longest table path the game takes. It builds "DATA\GLOBAL\TILES\<cell>" with
 * wsprintf in a 60-byte stack buffer. */
#define MAP_OPS_TABLE_PATH_MAX 41
```

Add to `map_ops.h`:

```c
/* May a map be added to, or taken from, this row of this table? 0, or -1 with a
 * sentence. Rows whose Files doesn't match their cells choose maps by their own rules,
 * and the game crashes on some edits to them. */
int map_ops_check_register(char * table, int def, char * error, int cap);
int map_ops_check_unregister(char * table, int def, const char * ds1_path, char * error, int cap);
```

In `map_ops.c`, add above `map_ops_register`:

```c
int map_ops_check_register(char * table, int def, char * error, int cap)
{
   int def_col = lvlprest_find_col(table, "Def");
   char * row_end;

   if (def_col < 0 || lvlprest_find_row(table, def_col, def, &row_end) == NULL)
      return fail(error, cap, "Def %d isn't in LvlPrest.txt.", def);
   if (!lvlprest_row_is_simple(table, def))
      return fail(error, cap, "Def %d's row chooses its maps by its own rules, so the game "
                  "would never use a map added to it. Pick another tileset.", def);
   if (lvlprest_files_value(table, def) >= 6)
      return fail(error, cap, "Def %d has no free File slot in LvlPrest.txt.", def);
   return 0;
}

int map_ops_check_unregister(char * table, int def, const char * ds1_path, char * error, int cap)
{
   int copies = 0, f;

   if (lvlprest_find_path_slot(table, def, ds1_path) == LVLPREST_TABLE_UNUSABLE)
      return fail(error, cap, "The project's LvlPrest.txt has no Def or File columns.");
   for (f = 1; f <= 6; f++)
      if (lvlprest_slot_holds_path(table, def, f, ds1_path))
         copies++;
   if (copies == 0)
      return 0;
   if (!lvlprest_row_is_simple(table, def))
      return fail(error, cap, "Def %d's row chooses its maps by its own rules, and taking "
                  "one out can crash the game. Edit the map instead.", def);
   if (copies >= lvlprest_files_value(table, def))
      return fail(error, cap, "%s is the only map in Def %d's row, so its level would have "
                  "nothing to load. Edit the map instead.", ds1_path, def);
   return 0;
}
```

Replace the body of `map_ops_register` after the path-length check:

```c
   table = table_for_edit(project_dir, seed, &len, error, cap);
   if (table == NULL)
      return -1;

   rc = map_ops_check_register(table, def, error, cap);
   if (rc == 0)
   {
      char * edited = lvlprest_with_map_inserted(table, def, 6, ds1_path, &len);

      if (edited == NULL)
         rc = fail(error, cap, "Couldn't edit Def %d's row in LvlPrest.txt.", def);
      else
      {
         rc = save_table(project_dir, edited, len, error, cap);
         free(edited);
      }
   }
   free(table);
   return rc;
```

Slot 6 lands at the end, since the row has fewer than six maps. Remove the now-unused locals `slot`, `def_col` and `row_end`. Remove `set_cell_and_save` and `table_with_cell`, and add this in their place:

```c
/* Replace *table with an edited copy. `edited` is NULL if the edit couldn't be made. */
static int take_edit(char ** table, char * edited, int def, char * error, int cap)
{
   if (edited == NULL)
      return fail(error, cap, "Couldn't edit Def %d's row in LvlPrest.txt.", def);
   free(*table);
   *table = edited;
   return 0;
}
```

Replace `map_ops_unregister`'s body after `table_for_edit`:

```c
   rc = map_ops_check_unregister(table, def, ds1_path, error, cap);
   slot = rc == 0 ? lvlprest_find_path_slot(table, def, ds1_path) : 0;
   while (rc == 0 && slot > 0)
   {
      rc = take_edit(&table, lvlprest_with_map_removed(table, def, slot, &len), def, error, cap);
      changed = 1;
      if (rc == 0)
         slot = lvlprest_find_path_slot(table, def, ds1_path);
   }
   if (rc == 0 && changed)
      rc = save_table(project_dir, table, len, error, cap);
   free(table);
   return rc;
```

Replace `map_ops_restore_slot`'s loop, from `for (i = 0; ...` through the end of its body:

```c
   for (i = 0; i < game_count && have < game_count && rc == 0; i++)
   {
      int slot = game_slots[i];

      if (lvlprest_slot_holds_path(table, def, slot, ds1_path))
         continue;
      if (!lvlprest_get_cell(game_table, def, slot, cell, sizeof(cell)))
      {
         rc = fail(error, cap, "The game's LvlPrest.txt doesn't list %s under Def %d.",
                   ds1_path, def);
         break;
      }
      rc = map_ops_check_register(table, def, error, cap);
      if (rc == 0)
         rc = take_edit(&table, lvlprest_with_map_inserted(table, def, slot, cell, &len),
                        def, error, cap);
      have++;
   }
   if (rc == 0)
      rc = save_table(project_dir, table, len, error, cap);
   free(table);
   return rc;
```

Remove the `current` local. Each function's `char * table` stays a local that `take_edit` can replace.

- [ ] **Step 4: Run the tests**

Run: `ctest --test-dir tmp/build-lifecycle -C Release -R "^(map_ops|lvlprest_text)$" --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core/map_ops.c src/core/map_ops.h test/test_map_ops.c
git commit -m "Refuse table edits the game can't load, and keep Files true"
```

---

### Task 4: Ask before confirming

**Files:**
- Modify: `src/core/ds1_manager.c`, `src/core/ds1_manager.h`, `src/ui/interface.c`, `src/ui/map_dialog.c`

**Interfaces:**
- Consumes: Task 3.
- Produces:
  - `int ds1_manager_remove_check(int group_idx, int entry_idx, char *error, int cap);`: 0 if Remove from Project may go ahead, else -1 with a sentence.
  - `int ds1_manager_def_can_take_map(int def, char *error, int cap);`, replacing `ds1_manager_def_has_free_slot`: 1 if it can, else 0 with a sentence.

These need the editor's globals, so there's no unit test. The controller checks them on screen in Task 5.

- [ ] **Step 1: Manager**

In `ds1_manager.c`, add a helper above `ds1_manager_remove_slot`:

```c
/* The table an edit would start from: the project's, or the one it would be seeded
 * from. NULL if neither can be read. Caller frees. */
static char * manager_current_table(void)
{
   char * table = NULL;
   long len;

   if (glb_project.is_open)
      table = map_ops_read_table(glb_project.path, &len);
   return table != NULL ? table : manager_seed();
}
```

Move the "game table lists it" check out of `ds1_manager_remove_slot` into:

```c
int ds1_manager_remove_check(int group_idx, int entry_idx, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_DS1_ENTRY_S * e;
   char name[] = "Data\\Global\\Excel\\LvlPrest.txt";
   char * game, * table;
   int listed, rc;

   if (group_idx < 0 || group_idx >= ab->group_count ||
       entry_idx < 0 || entry_idx >= ab->groups[group_idx].entry_count)
      return -1;
   e = &ab->groups[group_idx].entries[entry_idx];

   /* Restore Slot puts back what the game's table says. A row it doesn't list
      would have nothing changed and nothing to come back from. */
   game = (char *) txt_read_in_mem_mpq_only(name);
   if (game == NULL)
   {
      snprintf(error, cap, "The game's LvlPrest.txt can't be read.");
      return -1;
   }
   listed = e->lvlprest_def > 0 && lvlprest_lists_path(game, e->lvlprest_def, e->ds1_path);
   free(game);
   if (!listed)
   {
      snprintf(error, cap, "The game's LvlPrest.txt doesn't list %s here, so it has no slot "
               "to clear.", e->ds1_path);
      return -1;
   }

   table = manager_current_table();
   if (table == NULL)
   {
      snprintf(error, cap, "LvlPrest.txt can't be read.");
      return -1;
   }
   rc = map_ops_check_unregister(table, e->lvlprest_def, e->ds1_path, error, cap);
   free(table);
   return rc;
}
```

`ds1_manager_remove_slot` calls `ds1_manager_remove_check` first and returns -1 if it fails, keeping its bounds and project checks. `map_ops_unregister` checks again before it writes, so a stale check can't write a bad table.

Replace `ds1_manager_def_has_free_slot`:

```c
int ds1_manager_def_can_take_map(int def, char * error, int cap)
{
   char * table = manager_current_table();
   int rc;

   if (table == NULL)
      return 1;   /* nothing to judge by: the create itself will say */
   rc = map_ops_check_register(table, def, error, cap);
   free(table);
   return rc == 0;
}
```

Update `ds1_manager.h` for both.

- [ ] **Step 2: Dialog**

In `map_dialog.c` `try_accept`, replace the free-slot block:

```c
   if (!ds1_manager_def_can_take_map(m->d->def, m->status, sizeof(m->status)))
      return 0;
```

`m->status` is 160 characters, and the row message is longer than the panel is wide at 8 px per glyph. Shorten what the dialog shows: if the sentence is over 62 characters, replace it with `Def %d can't take another map. Pick another tileset.`. The full sentence still reaches the box from the manager if a create fails later.

- [ ] **Step 3: Interface**

In `interface_remove_map`, after the state check and before the confirmation:

```c
   if (ds1_manager_remove_check(gi, ei, error, sizeof(error)) != 0)
   {
      interface_map_op_failed("Remove from Project", "This map can't be removed.", error);
      return;
   }
```

Change the confirmation's text to say what now happens:

```c
         "This takes it out of the project's LvlPrest.txt, so the game won't use "
         "this map with your mod. The game's files don't change. Map > Restore Slot "
         "puts it back.", FALSE))
```

- [ ] **Step 4: Build and run the tests**

Run: `cmake --build tmp/build-lifecycle --config Release -- /m /v:q /nologo`, then `ctest --test-dir tmp/build-lifecycle -C Release -L portable -j 3 --timeout 300`.
Expected: all pass. `grep -rn ds1_manager_def_has_free_slot src` finds nothing.

- [ ] **Step 5: Commit**

```bash
git add src/core/ds1_manager.c src/core/ds1_manager.h src/ui/interface.c src/ui/map_dialog.c
git commit -m "Say why a map can't be added or removed before asking to confirm"
```

---

### Task 5: Docs and proof in the game

**Files:**
- Modify: `docs/superpowers/specs/2026-11-02-map-lifecycle-design.md`, `docs/superpowers/specs/2026-11-03-map-lifecycle-gate-results.md`, `docs/getting-started/manual.md`, `docs/testing/native-release-checklist.md`, `CHANGELOG.md`, `README.md` if it mentions any of this

- [ ] **Step 1: Update the docs**

Spec, "Create": replace "If no File slot is free, or the Def is not found, the dialog says so and writes nothing." with:

```markdown
- A map can only join a row whose `Files` column equals its filled cells. Create adds it
  at the end and raises `Files`. If the row is full, chooses its maps by its own rules, or
  the Def is not found, the dialog says so and writes nothing.
```

Spec, "Delete", after the table:

```markdown
Taking a map out of a row moves later cells back and lowers `Files`, because the game
crashes on a blank cell below `Files`. A row's only map can't be removed, and neither can a
map in a row that chooses its maps by its own rules: the command says so and changes nothing.
```

Gate results, "How the Game Uses File Cells": change "**Step 3 as merged doesn't match this.**" to say this plan fixed it, and name the plan.

Manual, under "Deleting maps" and "Creating maps": say the same in plain words, including that a town or other special area can't take a new map, and that replacing a level's only map means editing it. `manual.md`'s path limit becomes 41 characters.

Checklist E15, keeping one item and total 15, add: New on a town row is refused in the dialog; Remove on a row's only map is refused before any confirmation; after Remove, the row in `LvlPrest.txt` has `Files` lowered and no `0` before the last map; restore returns the table byte for byte.

`CHANGELOG.md`: in the entry about deleting by state, state that Remove and Delete keep `Files` true and refuse a row's only map. The change never shipped, so don't write it as a fix.

- [ ] **Step 2: Commit**

```bash
git add docs CHANGELOG.md README.md
git commit -m "Document how maps join and leave a LvlPrest row"
```

- [ ] **Step 3: Controller's windowed GUI pass (Linux WSLg)**

Windowed only, scratch project, no writes to `C:\Diablo2`. Checks:

1. Clone `TownN1` keeping Act 1 Town: the dialog stays up with "can't take another map".
2. New in a two-map row such as Barracks' `Bridge`-style row: the file appears, `N` shows, and that row's `Files` went up by one in `Global/Excel/LvlPrest.txt`.
3. `Ctrl+Del` on it: Yes moves it to `backup` and the row's `Files` comes back down, with no `0` before the last map.
4. Remove on a one-map game row: a box says it can't, with no confirmation before it, and the table isn't created or changed.
5. Remove on a two-map game row, then Restore Slot: `cmp` against a copy from before shows the table byte for byte.
6. Reopen the project between steps 2 and 3: the rows and marks hold.

- [ ] **Step 4: Controller's proof in the game**

Copy the table from check 2, with its map, into a fleet slot's `DATA\GLOBAL\EXCEL` and `DATA\GLOBAL\TILES`. Launch `lod-113c` with `-w -direct -txt` and the override `{"auto_enter":"none"}`. Enter the game, and walk through the changed row's area if it's near, or use a Blood Moor row from Task 1. Then repeat with a table from check 5 with the Remove still applied. Expected: no `UNHANDLED EXCEPTION` in the slot's log for either. Add the result to the gate results.
