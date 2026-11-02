# Map Lifecycle, Step 1: Projects and Saving Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every map edit is saved safely into a project, unsaved edits are never lost without a prompt, and the area browser shows each map's state.

**Architecture:** A small `core/map_state` unit derives dirty and storage state from existing map and undo data. `ui/map_save_ui` holds the save, project-prompt and unsaved-prompt flows that `interface.c`, `project_menu.c` and an area-browser unload guard call.

**Tech Stack:** C99, Allegro 5.2, CMake and CTest, Unity (`test/unity`), the editor test fixtures in `test/support`.

**Spec:** `docs/superpowers/specs/2026-11-02-map-lifecycle-design.md` (Build order, step 1). Read it first.

**Later plans:** step 2 (commands and menus), step 3 (create and delete, including Removed rows in the browser) and step 4 (the MPQ gate, build and test) each get their own plan once the step before has landed.

## Global Constraints

Every task's requirements implicitly include all of these.

- **Starting point:** branch from a tree containing commit `32ba8b0` (the GTK fix) and the Delete key fix from `claude/xenodochial-nightingale-38462e`. Line numbers below are from `32ba8b0`, so locate edits by the quoted code.
- **Platforms:** C99 that builds with MSVC (Win32 x86), GCC and Clang. Windows, Linux and macOS ship together.
- **Portable tests:** tests use authored fixtures only, and need no game data or display.
- **Windowed only:** manual runs use `full_screen = NO`. Never run the editor fullscreen.
- **Game data:** nothing may write to the game install. Manual testing uses a scratch project folder.
- **Dialogs:** new prompts use the editor's `wmsg` dialogs, not `al_show_native_message_box`, so buttons behave the same on all platforms.
- **Commits:** imperative sentence subject, ending with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Windows build (Git Bash):**
  ```bash
  CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
  CTEST="${CMAKE%cmake.exe}ctest.exe"
  "$CMAKE" -S . -B tmp/build-lifecycle -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows
  "$CMAKE" --build tmp/build-lifecycle --config Release
  "$CTEST" --test-dir tmp/build-lifecycle -C Release -R '^(ds1_save|editor_undo|map_state)$' --output-on-failure
  ```

## File Structure

| File | Change | Responsibility |
|---|---|---|
| `src/core/map_state.c`, `.h` | Create | Dirty tracking and storage-state rules; no UI |
| `src/ui/map_save_ui.c`, `.h` | Create | Save with error box, project prompt, unsaved prompt |
| `src/core/project.c`, `.h` | Modify | Redirect relative names; restore `mod_dir` on close |
| `src/core/ds1_save_io.c` | Modify | Record a successful save |
| `src/editor/undo.c`, `objects.c`, `paths.c` | Modify | Report edits to `map_state` |
| `src/ui/props_panel.c` | Modify | Report DS1 field edits |
| `src/ui/dialogs.c`, `.h` | Modify | Shared three-button dialog and two new prompts |
| `src/ui/interface.c` | Modify | Save, quit and window-close flows |
| `src/ui/project_menu.c`, `.h` | Modify | Return results, prompt, reload the browser |
| `src/core/area_browser.c`, `.h` | Modify | Unload guard, reload, state marks |
| `src/structs.h` | Modify | New `DS1_S`, entry and browser fields |
| `test/test_map_state.c` | Create | Rules for state and path matching |
| `test/test_ds1_save.c`, `test/test_editor_undo.c` | Modify | Redirect, save and undo cases |

---

### Task 1: Redirect relative map names into the project

**Files:**
- Modify: `src/core/project.c`, `src/core/project.h`
- Test: `test/test_ds1_save.c`

**Interfaces:**
- Produces: `project_redirect_ds1_save_path` returns 1 for a relative name with no tiles segment, targeting `<project>/Global/Tiles/<name>`. Its signature is unchanged.

- [ ] **Step 1: Write the failing tests**

Add to `test/test_ds1_save.c` after `test_copy_on_save_parent_creation_failure_changes_nothing`, and add both to `main` with `PKGC_RUN`:

```c
/* The area browser names maps relative to Tiles, as in "Act1/Town/x.ds1". */
void test_copy_on_save_redirects_a_tiles_relative_name(void)
{
   FX_SPEC s;
   char src[512], target[512];

   make_source_tree();
   load_fixture(&s, 18, "src" DS1_SEP_STR "tiles" DS1_SEP_STR "act1" DS1_SEP_STR "town.ds1",
                0, src, sizeof(src));
   snprintf(glb_ds1[0].name, sizeof(glb_ds1[0].name), "Act1/Town/TownN1.ds1");
   open_project("proj");

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   snprintf(target, sizeof(target), "%s%sGlobal%sTiles%sAct1%sTown%sTownN1.ds1",
            glb_project.path, DS1_SEP_STR, DS1_SEP_STR, DS1_SEP_STR, DS1_SEP_STR, DS1_SEP_STR);
   TEST_ASSERT_TRUE(tt_file_exists(target));
   reload_and_expect(target, &s, 1);
}

void test_redirect_treats_backslashes_in_a_relative_name_as_separators(void)
{
   char dst[512], want[512];

   open_project("proj");
   TEST_ASSERT_EQUAL_INT(1, project_redirect_ds1_save_path("Act1\\Town\\x.ds1", dst, sizeof(dst)));
   snprintf(want, sizeof(want), "%s%sGlobal%sTiles%sAct1%sTown%sx.ds1",
            glb_project.path, DS1_SEP_STR, DS1_SEP_STR, DS1_SEP_STR, DS1_SEP_STR, DS1_SEP_STR);
   TEST_ASSERT_EQUAL_STRING(want, dst);
}
```

- [ ] **Step 2: Run them to confirm they fail**

Run: `"$CMAKE" --build tmp/build-lifecycle --config Release --target test_ds1_save && "$CTEST" --test-dir tmp/build-lifecycle -C Release -R '^ds1_save$' --output-on-failure`
Expected: both new tests FAIL, because the path passes through unchanged.

- [ ] **Step 3: Implement the redirect**

In `src/core/project.c`, add above `project_redirect_ds1_save_path`:

```c
// "/x", "\x" and "C:\x" name a real place on disk. Anything else is a name
// relative to Tiles, which is how the area browser names maps.
static int is_rooted_path(const char *path)
{
   if (path[0] == '/' || path[0] == '\\') return 1;
   return isalpha((unsigned char) path[0]) && path[1] == ':';
}
```

Replace these lines in `project_redirect_ds1_save_path`:

```c
   suffix = find_tiles_suffix(src);
   if (suffix == NULL || suffix[0] == 0)   return 0;
```

with:

```c
   suffix = find_tiles_suffix(src);
   if (suffix == NULL && !is_rooted_path(src))
      suffix = src;
   if (suffix == NULL || suffix[0] == 0)   return 0;
```

Then, immediately before the final `return 1;`, add:

```c
#ifndef WIN32
   // A table can spell a relative name with backslashes.
   {
      char *c;
      for (c = dst + strlen(glb_project.path); *c != 0; c++)
         if (*c == '\\') *c = '/';
   }
#endif
```

In `src/core/project.h`, change the return-value comment for `0` to read "no project, already inside the project -- at a path-component boundary -- or a rooted path with no tiles segment to rewrite from".

- [ ] **Step 4: Run the tests to confirm they pass**

Run: `"$CMAKE" --build tmp/build-lifecycle --config Release --target test_ds1_save && "$CTEST" --test-dir tmp/build-lifecycle -C Release -R '^ds1_save$' --output-on-failure`
Expected: PASS, including all existing copy-on-save tests.

- [ ] **Step 5: Commit**

```bash
git add src/core/project.c src/core/project.h test/test_ds1_save.c
git commit -m "Redirect relative map names into the open project"
```

---

### Task 2: Track unsaved edits

**Files:**
- Create: `src/core/map_state.c`, `src/core/map_state.h`, `test/test_map_state.c`
- Modify: `src/structs.h`, `src/core/ds1_save_io.c`, `src/editor/undo.c`, `src/editor/objects.c`, `src/editor/paths.c`, `src/ui/props_panel.c`, `CMakeLists.txt`, `test/cmake/map_integrity.cmake`, `test/test_editor_undo.c`, `test/test_ds1_save.c`

**Interfaces:**
- Produces, in `core/map_state.h`:
  ```c
  void map_dirty_mark(int ds1_idx);
  void map_dirty_before_tile_edit(int ds1_idx);
  void map_dirty_before_object_edit(int ds1_idx);
  void map_dirty_after_object_undo(int ds1_idx);
  void map_dirty_mark_saved(int ds1_idx);
  int  map_is_dirty(int ds1_idx);
  int  map_any_dirty(void);
  ```

- [ ] **Step 1: Add the fields and the module header**

In `src/structs.h`, in `DS1_S` immediately after `UDWORD save_count;`:

```c
   // unsaved-edit tracking; see core/map_state.h
   int  other_dirty;       // an object, path or DS1 field edit is unsaved
   int  undo_other_dirty;  // other_dirty as it was before the last object edit
   long saved_undo_depth;  // tile edits applied at the last save; -1 = unreachable
```

Create `src/core/map_state.h`:

```c
#ifndef _MAP_STATE_H_
#define _MAP_STATE_H_

/* Unsaved-edit tracking. A map is dirty when its tile undo position differs
 * from the position at the last save, or when another kind of edit is unsaved.
 * Undoing back to the saved position makes the map clean again. All fields
 * start at zero, which is the state of a freshly loaded map. */
void map_dirty_mark(int ds1_idx);               // an edit with no undo step
void map_dirty_before_tile_edit(int ds1_idx);   // a new tile undo buffer is starting
void map_dirty_before_object_edit(int ds1_idx); // the one-step object undo is being set
void map_dirty_after_object_undo(int ds1_idx);  // that undo was applied
void map_dirty_mark_saved(int ds1_idx);         // a real save reached the disk
int  map_is_dirty(int ds1_idx);
int  map_any_dirty(void);                       // any loaded map

#endif
```

- [ ] **Step 2: Write the failing tests**

Create `test/test_map_state.c`:

```c
/* Unsaved-edit rules, over the fixture's in-memory maps. Whole edit paths are
 * covered in test_editor_undo and test_ds1_save. */
#include <stdio.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "core/map_state.h"
#include "support/editor_test_state.h"

void setUp(void)
{
   TEST_ASSERT_TRUE(editor_test_state_init(2, 2, 2));
}

void tearDown(void)
{
   editor_test_state_destroy();
}

void test_a_fresh_map_is_clean(void)
{
   TEST_ASSERT_FALSE(map_is_dirty(0));
   TEST_ASSERT_FALSE(map_any_dirty());
}

void test_mark_and_save(void)
{
   map_dirty_mark(1);
   TEST_ASSERT_TRUE(map_is_dirty(1));
   TEST_ASSERT_FALSE(map_is_dirty(0));
   TEST_ASSERT_TRUE(map_any_dirty());
   map_dirty_mark_saved(1);
   TEST_ASSERT_FALSE(map_is_dirty(1));
   TEST_ASSERT_FALSE(map_any_dirty());
}

void test_object_undo_returns_to_the_state_before_the_edit(void)
{
   map_dirty_before_object_edit(0);
   TEST_ASSERT_TRUE(map_is_dirty(0));
   map_dirty_after_object_undo(0);
   TEST_ASSERT_FALSE(map_is_dirty(0));
}

void test_object_undo_after_a_save_is_an_unsaved_change(void)
{
   map_dirty_before_object_edit(0);
   map_dirty_mark_saved(0);
   TEST_ASSERT_FALSE(map_is_dirty(0));
   map_dirty_after_object_undo(0);
   TEST_ASSERT_TRUE(map_is_dirty(0));
}

void test_empty_slots_are_never_dirty(void)
{
   TEST_ASSERT_FALSE(map_is_dirty(-1));
   TEST_ASSERT_FALSE(map_is_dirty(DS1_MAX));
   TEST_ASSERT_FALSE(map_is_dirty(2));   /* a zeroed slot, which the fixture left unnamed */
}

int main(int argc, char **argv)
{
   pkgc_test_filter = argc > 1 ? argv[1] : NULL;
   UNITY_BEGIN();
   PKGC_RUN(test_a_fresh_map_is_clean);
   PKGC_RUN(test_mark_and_save);
   PKGC_RUN(test_object_undo_returns_to_the_state_before_the_edit);
   PKGC_RUN(test_object_undo_after_a_save_is_an_unsaved_change);
   PKGC_RUN(test_empty_slots_are_never_dirty);
   return UNITY_END();
}
```

Add to `test/test_editor_undo.c`, with `#include "core/map_state.h"` and `PKGC_RUN` entries:

```c
void test_tile_edits_and_undo_set_and_clear_dirty(void)
{
   TEST_ASSERT_TRUE(editor_test_state_init(1, 3, 2));
   fill_map(0);
   TEST_ASSERT_FALSE(map_is_dirty(0));
   select_cell(0, 1, 0);
   delete_selected(0);
   TEST_ASSERT_TRUE(map_is_dirty(0));
   undo(0);
   TEST_ASSERT_FALSE(map_is_dirty(0));
}

void test_a_new_edit_after_undoing_past_a_save_stays_dirty(void)
{
   TEST_ASSERT_TRUE(editor_test_state_init(1, 3, 2));
   fill_map(0);
   select_cell(0, 1, 0);
   delete_selected(0);
   map_dirty_mark_saved(0);
   TEST_ASSERT_FALSE(map_is_dirty(0));
   undo(0);
   TEST_ASSERT_TRUE(map_is_dirty(0));

   /* A different edit reaches the same depth, but not the saved content. */
   select_cell(0, 2, 0);
   delete_selected(0);
   TEST_ASSERT_TRUE(map_is_dirty(0));
}
```

Add to `test/test_ds1_save.c`, with `#include "core/map_state.h"` and `PKGC_RUN` entries:

```c
void test_a_real_save_clears_dirty_and_a_tmp_save_does_not(void)
{
   FX_SPEC s;
   char src[512];

   load_fixture(&s, 18, "dirty.ds1", 0, src, sizeof(src));
   map_dirty_mark(0);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, TRUE), err);
   TEST_ASSERT_TRUE(map_is_dirty(0));
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   TEST_ASSERT_FALSE(map_is_dirty(0));
}

void test_a_failed_save_leaves_the_map_dirty(void)
{
   FX_SPEC s;
   char src[512];

   load_fixture(&s, 18, "dirty.ds1", 0, src, sizeof(src));
   map_dirty_mark(0);
   faults.fail_open = 1;
   TEST_ASSERT_EQUAL_INT(-1, save_faulty(0, FALSE));
   TEST_ASSERT_TRUE(map_is_dirty(0));
}
```

- [ ] **Step 3: Register the new source and test**

In `CMakeLists.txt`, add `src/core/map_state.c` after `src/core/project.c`.

In `test/cmake/map_integrity.cmake`, add `${CMAKE_SOURCE_DIR}/src/core/map_state.c` to `DS1EDIT_PKGC_PRODUCTION` after `src/core/project.c`, and after the `ds1_save` test add:

```cmake
ds1edit_pkgc_test(test_map_state test_map_state.c)
add_test(NAME map_state COMMAND test_map_state)
set_tests_properties(map_state PROPERTIES LABELS "portable")
```

- [ ] **Step 4: Confirm the tests fail to link**

Run: `"$CMAKE" -S . -B tmp/build-lifecycle && "$CMAKE" --build tmp/build-lifecycle --config Release --target test_map_state`
Expected: FAIL, with `map_state.c` not found.

- [ ] **Step 5: Implement the module**

Create `src/core/map_state.c`:

```c
#include <string.h>

#include "structs.h"
#include "core/map_state.h"

static int slot_has_map(int ds1_idx)
{
   return ds1_idx >= 0 && ds1_idx < DS1_MAX && glb_ds1[ds1_idx].name[0] != 0;
}

void map_dirty_mark(int ds1_idx)
{
   if (slot_has_map(ds1_idx))
      glb_ds1[ds1_idx].other_dirty = 1;
}

void map_dirty_before_tile_edit(int ds1_idx)
{
   DS1_S *d;

   if (!slot_has_map(ds1_idx)) return;
   d = &glb_ds1[ds1_idx];

   // The saved state was undone and this edit discards it for good, so a
   // later edit reaching the same depth must not count as clean.
   if (d->saved_undo_depth > d->undo.cur_buf_num + 1)
      d->saved_undo_depth = -1;
}

void map_dirty_before_object_edit(int ds1_idx)
{
   DS1_S *d;

   if (!slot_has_map(ds1_idx)) return;
   d = &glb_ds1[ds1_idx];
   d->undo_other_dirty = d->other_dirty;
   d->other_dirty = 1;
}

void map_dirty_after_object_undo(int ds1_idx)
{
   if (slot_has_map(ds1_idx))
      glb_ds1[ds1_idx].other_dirty = glb_ds1[ds1_idx].undo_other_dirty;
}

void map_dirty_mark_saved(int ds1_idx)
{
   DS1_S *d;

   if (!slot_has_map(ds1_idx)) return;
   d = &glb_ds1[ds1_idx];
   d->other_dirty = 0;
   d->undo_other_dirty = 1;   // the object undo backup now differs from disk
   d->saved_undo_depth = d->undo.cur_buf_num + 1;
}

int map_is_dirty(int ds1_idx)
{
   const DS1_S *d;

   if (!slot_has_map(ds1_idx)) return 0;
   d = &glb_ds1[ds1_idx];
   return d->other_dirty || d->saved_undo_depth != d->undo.cur_buf_num + 1;
}

int map_any_dirty(void)
{
   int i;

   for (i = 0; i < DS1_MAX; i++)
      if (map_is_dirty(i))
         return 1;
   return 0;
}
```

- [ ] **Step 6: Report edits from the code that makes them**

Add `#include "core/map_state.h"` to `src/core/ds1_save_io.c`, `src/editor/undo.c`, `src/editor/objects.c`, `src/editor/paths.c` and `src/ui/props_panel.c`.

In `undo_new_tile_buffer` (`src/editor/undo.c`), immediately before the comment `// some undo have been made ?`:

```c
   map_dirty_before_tile_edit(ds1_idx);
```

In `editobj_prepare_undo` (`src/editor/objects.c`), after `glb_ds1[ds1_idx].can_undo_obj = TRUE;`:

```c
   map_dirty_before_object_edit(ds1_idx);
```

In `editobj_undo`, after `glb_ds1[ds1_idx].obj_num = glb_ds1[ds1_idx].obj_num_undo;`:

```c
   map_dirty_after_object_undo(ds1_idx);
```

In `src/editor/paths.c`, in the `case PEB_OK :` block, on the line before `pwin->obj_dirty = FALSE;`:

```c
                  map_dirty_mark(ds1_idx);
```

In `src/ui/props_panel.c`, replace these four cases:

```c
            case 1:  glb_ds1[idx].version  = val; break;
```
```c
            case 4:  glb_ds1[idx].act      = val; break;
            case 5:  glb_ds1[idx].txt_act  = (int)val; break;
            case 6:  glb_ds1[idx].tag_type = val; break;
```

with:

```c
            case 1:  glb_ds1[idx].version  = val; map_dirty_mark(idx); break;
```
```c
            case 4:  glb_ds1[idx].act      = val; map_dirty_mark(idx); break;
            case 5:  glb_ds1[idx].txt_act  = (int)val; map_dirty_mark(idx); break;
            case 6:  glb_ds1[idx].tag_type = val; map_dirty_mark(idx); break;
```

In `ds1_save_with_io` (`src/core/ds1_save_io.c`), replace:

```c
   d->save_count = new_save_count;
   return 0;
```

with:

```c
   d->save_count = new_save_count;
   if (! is_tmp_file)
      map_dirty_mark_saved(ds1_idx);
   return 0;
```

- [ ] **Step 7: Audit for edits that skip these hooks**

Run: `grep -n "undo_new_tile_buffer\|editobj_prepare_undo" src/ui/edit_window.c src/editor/objects.c src/editor/tiles.c | wc -l` and `grep -nE "\.(obj|path)\[[^]]*\]\.[a-z_]+ *=[^=]" src/ui/*.c`.
Expected: the first prints a nonzero count, and any hit from the second belongs to a function that calls `editobj_prepare_undo` or `map_dirty_mark`. For any hit that doesn't, add `map_dirty_mark(ds1_idx);` after the edit, and list it in the commit message.

- [ ] **Step 8: Run the tests to confirm they pass**

Run: `"$CMAKE" --build tmp/build-lifecycle --config Release && "$CTEST" --test-dir tmp/build-lifecycle -C Release -R '^(ds1_save|editor_undo|map_state|object_path_edits)$' --output-on-failure`
Expected: PASS. The editor executable also links.

- [ ] **Step 9: Commit**

```bash
git add src test CMakeLists.txt
git commit -m "Track unsaved edits per map"
```

---

### Task 3: Restore mod_dir when a project closes

**Files:**
- Modify: `src/core/project.c`, `src/ui/project_menu.c`
- Test: `test/test_ds1_save.c`

**Interfaces:**
- Produces: `project_close()` restores the `mod_dir[0]` that was in place before `project_apply_to_config()`. Signatures are unchanged.

- [ ] **Step 1: Write the failing test**

Add to `test/test_ds1_save.c`, with a `PKGC_RUN` entry:

```c
void test_closing_a_project_restores_the_configured_mod_dir(void)
{
   char configured[] = "my_mod";
   char *before = glb_config.mod_dir[0];

   glb_config.mod_dir[0] = configured;
   open_project("proj");
   project_apply_to_config();
   TEST_ASSERT_EQUAL_STRING(glb_project.path, glb_config.mod_dir[0]);

   project_close();
   TEST_ASSERT_TRUE(glb_config.mod_dir[0] == configured);
   glb_config.mod_dir[0] = before;
}
```

- [ ] **Step 2: Run it to confirm it fails**

Run: `"$CMAKE" --build tmp/build-lifecycle --config Release --target test_ds1_save && "$CTEST" --test-dir tmp/build-lifecycle -C Release -R '^ds1_save$' --output-on-failure`
Expected: FAIL, because `mod_dir[0]` still holds the project path.

- [ ] **Step 3: Implement**

In `src/core/project.c`, add after `PROJECT_S glb_project;`:

```c
// The mod_dir[0] the project replaced, restored when it closes.
static char *s_configured_mod_dir;
static int   s_mod_dir_replaced;
```

Replace `project_close`:

```c
void project_close(void)
{
   if (s_mod_dir_replaced)
   {
      glb_config.mod_dir[0] = s_configured_mod_dir;
      s_mod_dir_replaced = 0;
   }
   memset(&glb_project, 0, sizeof(glb_project));
}
```

In `project_apply_to_config`, replace:

```c
      memcpy(buf, glb_project.path, len + 1);
      glb_config.mod_dir[0] = buf;
```

with:

```c
      memcpy(buf, glb_project.path, len + 1);
      if (!s_mod_dir_replaced)
      {
         s_configured_mod_dir = glb_config.mod_dir[0];
         s_mod_dir_replaced = 1;
      }
      glb_config.mod_dir[0] = buf;
```

In `action_close_project` (`src/ui/project_menu.c`), delete the line `glb_config.mod_dir[0] = NULL; // clear overlay`.

- [ ] **Step 4: Run the test to confirm it passes**

Run: `"$CMAKE" --build tmp/build-lifecycle --config Release --target test_ds1_save && "$CTEST" --test-dir tmp/build-lifecycle -C Release -R '^ds1_save$' --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core/project.c src/ui/project_menu.c test/test_ds1_save.c
git commit -m "Restore the configured mod_dir when a project closes"
```

---

### Task 4: Save, project prompt and unsaved prompts

**Files:**
- Create: `src/ui/map_save_ui.c`, `src/ui/map_save_ui.h`
- Modify: `src/ui/dialogs.c`, `src/ui/dialogs.h`, `src/ui/interface.c`, `src/ui/project_menu.c`, `src/ui/project_menu.h`, `src/core/area_browser.c`, `src/core/area_browser.h`, `src/structs.h`, `src/main.c`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `map_is_dirty`, `map_any_dirty` from Task 2.
- Produces:
  ```c
  // ui/map_save_ui.h
  int map_save_ui_save(int ds1_idx, int is_tmp_file);  // 0 saved, -1 not saved
  int map_save_ui_save_dirty(void);                    // 0 all saved, -1 any failed or cancelled
  int map_save_ui_resolve_unsaved(const char *what);   // 1 proceed, 0 cancelled
  // ui/project_menu.h
  int project_menu_new_project(void);                  // 1 if a project opened
  int project_menu_open_project(void);                 // 1 if a project opened
  // ui/dialogs.h
  int msg_unsaved_main(const char *text);              // 0 save all, 1 discard, 2 cancel, -1 error
  int msg_project_required_main(void);                 // 0 new, 1 open, 2 cancel, -1 error
  // core/area_browser.h
  void area_browser_set_unload_guard(int (*guard)(void));
  void area_browser_unload_all(void);
  ```

These flows drive the display and event loop, so they are checked by the manual steps below; the rules they rely on have unit tests from Task 2.

- [ ] **Step 1: Add the two dialogs**

In `src/ui/dialogs.c`, add above `msg_quit_main`:

```c
// A warning window with three buttons, styled like the quit window.
// return value : -1 : error, otherwise the index of the chosen button
static int msg_three_buttons(char *title, char *text,
                             char *label0, int key0,
                             char *label1, int key1)
{
   WMSG_BUT_S buttons[4]; // 4th button is NULL, needed
   WMSG_S     wmsg;
   int        col_black    = makecol(0, 0, 0),
              col_midgreen = makecol(0, 128, 0),
              col_white    = makecol(255, 255, 255),
              col_green    = makecol(0, 255, 0);
   int        i;
   char      *labels[3];
   int        keys[3];

   labels[0] = label0;           keys[0] = key0;
   labels[1] = label1;           keys[1] = key1;
   labels[2] = " (Esc) CANCEL "; keys[2] = KEY_ESC;

   memset(buttons, 0, sizeof(buttons));
   for (i = 0; i < 3; i++)
   {
      buttons[i].text               = labels[i];
      buttons[i].right_spaces       = (i < 2) ? 3 : 0;
      buttons[i].on.fg              = col_black;
      buttons[i].on.bg              = col_midgreen;
      buttons[i].on.border          = col_white;
      buttons[i].off.fg             = col_green;
      buttons[i].off.bg             = col_black;
      buttons[i].off.border         = col_green;
      buttons[i].shortcut[0].key[0] = keys[i];
   }

   wmsg.title        = title;
   wmsg.text         = text;
   wmsg.font_width   = 8;
   wmsg.font_height  = 8;
   wmsg.col_win.bg   = col_black;
   wmsg.col_win.fg   = col_white;
   wmsg.col_title.bg = col_green;
   wmsg.col_title.fg = col_black;
   wmsg.col_text.bg  = -1;
   wmsg.col_text.fg  = col_green;
   wmsg.button       = & buttons[0];

   return wmsg_main( & wmsg);
}

int msg_unsaved_main(const char *text)
{
   return msg_three_buttons("WARNING", (char *) text,
                            "(S) SAVE ALL", KEY_S,
                            "(D) DISCARD", KEY_D);
}

int msg_project_required_main(void)
{
   return msg_three_buttons("PROJECT NEEDED",
      "\nSaved maps live in a project folder. Choose one to continue.",
      "(N) NEW PROJECT", KEY_N,
      "(O) OPEN PROJECT", KEY_O);
}
```

In `src/ui/dialogs.h`, add after `msg_save_main`:

```c
int msg_unsaved_main          (const char *text);
int msg_project_required_main (void);
```

- [ ] **Step 2: Make the project actions return a result**

In `src/ui/project_menu.c`, change `action_new_project` and `action_open_project` from `static void` to `static int`. Every early `return;` becomes `return 0;`, and each function ends with `return 1;` after its `fprintf`. Then add under `/* ---- public entry points ---- */`:

```c
int project_menu_new_project(void)  { return action_new_project(); }
int project_menu_open_project(void) { return action_open_project(); }
```

In `src/ui/project_menu.h`, add:

```c
// Run the New or Open Project flow. Returns 1 if a project is now open.
int project_menu_new_project(void);
int project_menu_open_project(void);
```

- [ ] **Step 3: Create the save module**

Create `src/ui/map_save_ui.h`:

```c
#ifndef _MAP_SAVE_UI_H_
#define _MAP_SAVE_UI_H_

// Save one map. A real save with no project open asks for one first, and a
// failure shows why. Returns 0 if saved, -1 if not. Edits stay in memory.
int map_save_ui_save(int ds1_idx, int is_tmp_file);

// Save every map with unsaved edits. Returns 0 if all saved, -1 otherwise.
int map_save_ui_save_dirty(void);

// Ask what to do about unsaved maps before `what` happens, for example
// "\nOpening this map closes the open maps. What do you want to do ?".
// Returns 1 to go ahead (nothing unsaved, saved, or discarded) and 0 to stop.
int map_save_ui_resolve_unsaved(const char *what);

#endif
```

Create `src/ui/map_save_ui.c`:

```c
#include <stdio.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_native_dialog.h>

#include "structs.h"
#include "ui/compat.h"
#include "ui/dialogs.h"
#include "ui/input.h"
#include "ui/map_save_ui.h"
#include "ui/project_menu.h"
#include "core/area_browser.h"
#include "core/ds1_save_io.h"
#include "core/map_state.h"
#include "core/project.h"

static int require_project(void)
{
   if (glb_project.is_open)
      return 1;

   switch (msg_project_required_main())
   {
   case 0:  return project_menu_new_project();
   case 1:  return project_menu_open_project();
   default: return 0;
   }
}

int map_save_ui_save(int ds1_idx, int is_tmp_file)
{
   char error[768];

   if (!is_tmp_file && glb_ds1[ds1_idx].name[0] != 0 && !require_project())
      return -1;

   if (ds1_save_checked(ds1_idx, is_tmp_file, error, sizeof(error)) == 0)
   {
      if (!is_tmp_file)
         area_browser_refresh_states();
      return 0;
   }
   al_show_native_message_box(a5_display, "DS1Edit",
      is_tmp_file ? "The autosave (.tmp) was not written"
                  : "The map was not saved",
      error, NULL, ALLEGRO_MESSAGEBOX_ERROR);
   return -1;
}

int map_save_ui_save_dirty(void)
{
   int i, rc = 0;

   for (i = 0; i < DS1_MAX; i++)
      if (map_is_dirty(i) && map_save_ui_save(i, FALSE) != 0)
         rc = -1;
   return rc;
}

int map_save_ui_resolve_unsaved(const char *what)
{
   if (!map_any_dirty())
      return 1;

   switch (msg_unsaved_main(what))
   {
   case 0:  return map_save_ui_save_dirty() == 0;
   case 1:  return 1;
   default: return 0;
   }
}
```

`area_browser_refresh_states` comes from Task 5. For this task, add it to `src/core/area_browser.h` with an empty body in `area_browser.c`:

```c
void area_browser_refresh_states(void);
```
```c
void area_browser_refresh_states(void)
{
}
```

Add `src/ui/map_save_ui.c` to `CMakeLists.txt` after `src/ui/project_menu.c`.

- [ ] **Step 4: Use it in the save, Save All and quit flows**

In `src/ui/interface.c`, add `#include "ui/map_save_ui.h"` and `#include "core/map_state.h"`, and delete the static `interface_save_map` function. Replace remaining calls to `interface_save_map(` with `map_save_ui_save(`.

Add above the main interface function:

```c
/* Decide whether the editor may close. Only unsaved maps need a question.
 * Returns TRUE to close. */
static int interface_confirm_quit(void)
{
   int i;

   if (!map_any_dirty())
      return TRUE;

   /* The close request has been taken; the dialog must not see it as well. */
   a5_display_closed = 0;

   switch (msg_quit_main())
   {
   case 0:
      // stay open if any map could not be saved
      return map_save_ui_save_dirty() == 0;

   case -1:
   case 1:
      for (i = 0; i < DS1_MAX; i++)
         if (map_is_dirty(i))
            map_save_ui_save(i, TRUE); // save a .TMP map
      return TRUE;

   default:
      return FALSE;
   }
}
```

Replace:

```c
      if (a5_display_closed)
         done = TRUE;
```

with:

```c
      if (a5_display_closed)
      {
         done = interface_confirm_quit();
         a5_display_closed = 0;
      }
```

Replace the whole body of the `// quit` block, leaving its condition unchanged:

```c
      // quit
      if (key_hit(KEY_ESC) && (glb_ds1[ds1_idx].draw_edit_obj == FALSE))
      {
         done = interface_confirm_quit();
      }
```

Replace the Ctrl+S condition:

```c
      if (key_hit(KEY_S) && (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL)))
```

with:

```c
      if (key_hit(KEY_S) && (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL)) &&
          (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT)))
      {
         // CTRL + SHIFT + S : save every map with unsaved edits
         map_save_ui_save_dirty();
      }
      else if (key_hit(KEY_S) && (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL)) &&
               glb_ds1[ds1_idx].name[0] != 0)
```

Below the existing call `project_menu_handle_shortcuts();`, add:

```c
      if (!glb_ds1edit.has_loaded_ds1)
         ds1_idx = 0;  /* closing a project can discard the open maps */
```

- [ ] **Step 5: Guard browser switches, which unload every map**

In `src/structs.h`, in `AREA_BROWSER_S` after `loaded_group`:

```c
   int  loaded_entry;          /* entry within loaded_group, -1=none   */
```

In `src/core/area_browser.h`, add:

```c
// `guard` runs before anything unloads the open maps, and returns 0 to stop.
void area_browser_set_unload_guard(int (*guard)(void));
void area_browser_unload_all(void);
```

In `src/core/area_browser.c`, add after `area_browser_unload_current`:

```c
static int (*s_unload_guard)(void);

void area_browser_set_unload_guard(int (*guard)(void))
{
   s_unload_guard = guard;
}

void area_browser_unload_all(void)
{
   area_browser_unload_current();
   glb_ds1edit.has_loaded_ds1 = FALSE;
   glb_ds1edit.area_browser.loaded_group = -1;
   glb_ds1edit.area_browser.loaded_entry = -1;
}

/* Returns TRUE if the open maps may go. Otherwise it puts the highlight back
 * on the map that stays open, since navigation moves it before switching. */
static int area_browser_may_unload(void)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;

   if (s_unload_guard == NULL || s_unload_guard())
      return TRUE;
   ab->selected_group = ab->loaded_group;
   ab->selected_entry = ab->loaded_entry;
   return FALSE;
}
```

Add a forward declaration `static int area_browser_may_unload(void);` near the top if a use precedes it.

In `area_browser_switch_area`, directly above `printf("Switching to area: ");`:

```c
   if (!area_browser_may_unload())
      return -1;
```

and after `area_browser_finish_load(group_idx, 0, opened);`:

```c
   ab->loaded_entry = 0;
```

In `area_browser_switch_single`, directly above `printf("Switching to DS1 entry %d in group %d\n", entry_idx, group_idx);`:

```c
   if (!area_browser_may_unload())
      return -1;
```

and after `area_browser_finish_load(group_idx, 0, 1);`:

```c
   ab->loaded_entry = entry_idx;
```

Add to `src/ui/map_save_ui.c` and its header:

```c
int map_save_ui_unload_guard(void)
{
   return map_save_ui_resolve_unsaved(
      "\nOpening this map closes the open maps, and they have unsaved edits.");
}
```
```c
// Stops the area browser from unloading maps with unsaved edits.
int map_save_ui_unload_guard(void);
```

In `src/main.c`, add `#include "ui/map_save_ui.h"`, and directly before `interfac_user_handler` is called add:

```c
   area_browser_set_unload_guard(map_save_ui_unload_guard);
```

- [ ] **Step 6: Prompt before a project closes or changes**

In `action_close_project` (`src/ui/project_menu.c`), replace the confirmation box, from `snprintf(msg, ...` through `if (rc != 1) return; // Yes == 1`, with:

```c
   if (map_any_dirty())
   {
      int choice = msg_unsaved_main(
         "\nClosing the project closes its open maps, and they have unsaved edits.");

      if (choice == 0 && map_save_ui_save_dirty() != 0)
         return;
      if (choice != 0 && choice != 1)
         return;
   }
```

Remove the now-unused `msg` variable. Then, after `project_close();`, add:

```c
   area_browser_unload_all();
```

Add these includes to `src/ui/project_menu.c`: `"core/area_browser.h"`, `"core/map_state.h"`, `"ui/dialogs.h"`, `"ui/map_save_ui.h"`.

At the start of `action_open_project`, before the folder picker, add:

```c
   if (!map_save_ui_resolve_unsaved(
          "\nSave the open maps before changing project ?"))
      return 0;
```

Discarded edits must not follow the map into the new project. In the same function, immediately before its `project_apply_to_config();`, add:

```c
   if (map_any_dirty())
      area_browser_unload_all();
```

- [ ] **Step 7: Build and run the unit tests**

Run: `"$CMAKE" --build tmp/build-lifecycle --config Release && "$CTEST" --test-dir tmp/build-lifecycle -C Release -L portable -j 4 --timeout 300`
Expected: PASS. Rerun any Python test that times out on its own, since they can exceed their limit under load.

- [ ] **Step 8: Check the flows by hand**

Use a scratch extraction with an INI containing `full_screen = NO`, `screen_width = 1280`, `screen_height = 720` and `d2_install` set to the game folder. Run the extracted or built editor with no arguments. Confirm each of these:

1. Open a map and quit with `Esc`: no prompt, and it exits.
2. Open a map, cut a tile with `Ctrl+X`, and press `Ctrl+S`: the project prompt appears. Press `Esc`: nothing is written and the edit stays.
3. Press `Ctrl+S` again and create a project in a scratch folder: the map saves to `<project>/Global/Tiles/...`, and "Your DS1 has been saved" appears.
4. Undo past the save, then click another map in the browser: the unsaved prompt appears. `Esc` keeps the map open and the highlight stays on it.
5. Press `Ctrl+Down` with an unsaved edit: the same prompt appears.
6. Press `Ctrl+S` with no map open: nothing happens and no message appears.
7. Close the window with an unsaved edit: the quit dialog appears, and Cancel keeps the editor open.
8. Press `Ctrl+Shift+W` with an unsaved edit and choose Discard: the maps close and the editor keeps running.

- [ ] **Step 9: Commit**

```bash
git add src CMakeLists.txt
git commit -m "Ask for a project on first save and prompt before losing unsaved maps"
```

---

### Task 5: Reload the browser and show map states

**Files:**
- Modify: `src/core/map_state.c`, `src/core/map_state.h`, `src/core/area_browser.c`, `src/core/area_browser.h`, `src/structs.h`, `src/ui/project_menu.c`
- Test: `test/test_map_state.c`

**Interfaces:**
- Consumes: `map_is_dirty` from Task 2; the `area_browser_refresh_states` stub from Task 4.
- Produces, in `core/map_state.h`:
  ```c
  typedef enum { MAP_STATE_GAME, MAP_STATE_MODIFIED, MAP_STATE_NEW, MAP_STATE_REMOVED } MAP_STATE_E;
  MAP_STATE_E map_state_classify(int in_game, int in_project, int slot_cleared);
  const char *map_state_mark(MAP_STATE_E state);
  int map_state_path_matches(const char *map_name, const char *ds1_path);
  ```
  And in `core/area_browser.h`: `int area_browser_reload(void);`

- [ ] **Step 1: Write the failing tests**

Add to `test/test_map_state.c`, with `PKGC_RUN` entries:

```c
void test_classify_covers_every_state(void)
{
   TEST_ASSERT_EQUAL_INT(MAP_STATE_GAME,     map_state_classify(1, 0, 0));
   TEST_ASSERT_EQUAL_INT(MAP_STATE_MODIFIED, map_state_classify(1, 1, 0));
   TEST_ASSERT_EQUAL_INT(MAP_STATE_NEW,      map_state_classify(0, 1, 0));
   TEST_ASSERT_EQUAL_INT(MAP_STATE_REMOVED,  map_state_classify(1, 0, 1));
   TEST_ASSERT_EQUAL_INT(MAP_STATE_REMOVED,  map_state_classify(1, 1, 1));
}

void test_marks(void)
{
   TEST_ASSERT_EQUAL_STRING("",  map_state_mark(MAP_STATE_GAME));
   TEST_ASSERT_EQUAL_STRING("M", map_state_mark(MAP_STATE_MODIFIED));
   TEST_ASSERT_EQUAL_STRING("N", map_state_mark(MAP_STATE_NEW));
}

void test_path_matching_ignores_case_and_slash_style_at_a_boundary(void)
{
   TEST_ASSERT_TRUE(map_state_path_matches("Act1/Town/TownN1.ds1", "act1\\town\\townn1.ds1"));
   TEST_ASSERT_TRUE(map_state_path_matches("C:\\p\\Global\\Tiles\\Act1\\Town\\TownN1.ds1",
                                           "Act1/Town/TownN1.ds1"));
   TEST_ASSERT_FALSE(map_state_path_matches("x/XAct1/Town/TownN1.ds1", "Act1/Town/TownN1.ds1"));
   TEST_ASSERT_FALSE(map_state_path_matches("Town.ds1", "Act1/Town.ds1"));
   TEST_ASSERT_FALSE(map_state_path_matches("", "a.ds1"));
}
```

- [ ] **Step 2: Run them to confirm they fail**

Run: `"$CMAKE" --build tmp/build-lifecycle --config Release --target test_map_state`
Expected: FAIL to compile, with `map_state_classify` undeclared.

- [ ] **Step 3: Implement the rules**

Add to `src/core/map_state.h`, before the final `#endif`:

```c
/* Where a map stands relative to the game. Derived, never stored on disk. */
typedef enum
{
   MAP_STATE_GAME,      // in the game's MPQs, with no project copy
   MAP_STATE_MODIFIED,  // in the game's MPQs, with a project copy at that path
   MAP_STATE_NEW,       // only in the project
   MAP_STATE_REMOVED    // in the game, but its slot is cleared in the project
} MAP_STATE_E;

MAP_STATE_E map_state_classify(int in_game, int in_project, int slot_cleared);

// The browser's letter for a state; "" for one that needs none.
const char *map_state_mark(MAP_STATE_E state);

// Does an open map's name refer to this table path? A saved map's name can have
// the project folder in front. Compares case-insensitively, either slash style.
int map_state_path_matches(const char *map_name, const char *ds1_path);
```

Add to `src/core/map_state.c`, and add `#include <ctype.h>` at its top:

```c
MAP_STATE_E map_state_classify(int in_game, int in_project, int slot_cleared)
{
   if (in_game && slot_cleared) return MAP_STATE_REMOVED;
   if (!in_project)             return MAP_STATE_GAME;
   return in_game ? MAP_STATE_MODIFIED : MAP_STATE_NEW;
}

const char *map_state_mark(MAP_STATE_E state)
{
   switch (state)
   {
   case MAP_STATE_MODIFIED: return "M";
   case MAP_STATE_NEW:      return "N";
   default:                 return "";
   }
}

static int same_path_char(char a, char b)
{
   if (a == '\\') a = '/';
   if (b == '\\') b = '/';
   return tolower((unsigned char) a) == tolower((unsigned char) b);
}

int map_state_path_matches(const char *map_name, const char *ds1_path)
{
   size_t nlen = strlen(map_name), plen = strlen(ds1_path), i;
   const char *tail;

   if (plen == 0 || nlen < plen) return 0;
   tail = map_name + (nlen - plen);
   for (i = 0; i < plen; i++)
      if (!same_path_char(tail[i], ds1_path[i]))
         return 0;
   return tail == map_name || tail[-1] == '/' || tail[-1] == '\\';
}
```

- [ ] **Step 4: Run the tests to confirm they pass**

Run: `"$CMAKE" --build tmp/build-lifecycle --config Release --target test_map_state && "$CTEST" --test-dir tmp/build-lifecycle -C Release -R '^map_state$' --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Reload the browser when the project changes**

In `src/structs.h`, add to `AREA_DS1_ENTRY_S` after `ds1_path`:

```c
   int  state;          /* MAP_STATE_E, set by area_browser_refresh_states */
```

In `src/core/area_browser.h`, add:

```c
// Rebuild the browser from the tables now in effect, after a project change.
int  area_browser_reload(void);
```

In `src/core/area_browser.c`, add `#include "core/map_state.h"`, `#include "core/project.h"` and `#include "platform.h"` if absent, and replace the Task 4 stub of `area_browser_refresh_states` with:

```c
static int project_has_map(const char * ds1_path)
{
   char rel[512], found[1024];

   snprintf(rel, sizeof(rel), "Global/Tiles/%s", ds1_path);
   if (ds1_path_resolve_nocase(glb_project.path, rel, found, sizeof(found)))
      return 1;
   snprintf(rel, sizeof(rel), "Data/Global/Tiles/%s", ds1_path);
   return ds1_path_resolve_nocase(glb_project.path, rel, found, sizeof(found));
}

/* In the game's own archives, ignoring any overlay? Loads the file, so it is
 * only asked about maps the project also has. */
static int game_has_map(const char * ds1_path)
{
   char  name[512];
   char *saved[MAX_MOD_DIR];
   char *buff = NULL;
   long  len = 0;
   int   i, entry;

   snprintf(name, sizeof(name), "Data\\Global\\Tiles\\%s", ds1_path);
   for (i = 0; i < MAX_MOD_DIR; i++)
   {
      saved[i] = glb_config.mod_dir[i];
      glb_config.mod_dir[i] = NULL;
   }
   entry = misc_load_mpq_file(name, &buff, &len, FALSE);
   for (i = 0; i < MAX_MOD_DIR; i++)
      glb_config.mod_dir[i] = saved[i];

   if (buff != NULL)
      free(buff);
   return entry != -1;
}

void area_browser_refresh_states(void)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int i, j;

   for (i = 0; i < ab->group_count; i++)
   {
      AREA_GROUP_S * g = &ab->groups[i];

      for (j = 0; j < g->entry_count; j++)
      {
         AREA_DS1_ENTRY_S * e = &g->entries[j];

         if (g->is_backup || !glb_project.is_open || !project_has_map(e->ds1_path))
            e->state = MAP_STATE_GAME;
         else
            e->state = map_state_classify(game_has_map(e->ds1_path), 1, 0);
      }
   }
}

int area_browser_reload(void)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int rc;

   if (glb_ds1edit.lvlprest_buff != NULL)
      glb_ds1edit.lvlprest_buff = txt_destroy(glb_ds1edit.lvlprest_buff);
   if (glb_ds1edit.lvltypes_buff != NULL)
      glb_ds1edit.lvltypes_buff = txt_destroy(glb_ds1edit.lvltypes_buff);
   if (glb_ds1edit.levels_buff != NULL)
      glb_ds1edit.levels_buff = txt_destroy(glb_ds1edit.levels_buff);

   rc = area_browser_init();
   ab->loaded_group = -1;
   ab->loaded_entry = -1;
   return rc;
}
```

At the end of `area_browser_init`, replace `return 0;` with:

```c
   area_browser_refresh_states();
   return 0;
```

Add `area_browser_reload();` in `src/ui/project_menu.c` immediately after `project_apply_to_config();` in both the New and Open actions, and after `area_browser_unload_all();` in the Close action.

- [ ] **Step 6: Draw the marks**

In `area_browser_draw_sidebar`, replace:

```c
               al_draw_textf(a5_font, fc, (float)(SB_MARGIN_X + 22), (float)y, 0,
                              "%s", fname);
```

with:

```c
               al_draw_textf(a5_font, fc, (float)(SB_MARGIN_X + 22), (float)y, 0,
                              "%s", fname);
               {
                  /* State letter, then * if an open copy has unsaved edits. */
                  int k, entry_dirty = 0;

                  for (k = 0; k < DS1_MAX && !entry_dirty; k++)
                     entry_dirty = glb_ds1[k].name[0] != 0 && map_is_dirty(k) &&
                        map_state_path_matches(glb_ds1[k].name, g->entries[j].ds1_path);

                  al_draw_textf(a5_font, fc, (float)(width - 28), (float)y, 0, "%s%s",
                                map_state_mark((MAP_STATE_E) g->entries[j].state),
                                entry_dirty ? "*" : "");
               }
```

- [ ] **Step 7: Build and check by hand**

Run: `"$CMAKE" --build tmp/build-lifecycle --config Release && "$CTEST" --test-dir tmp/build-lifecycle -C Release -L portable -j 4 --timeout 300`
Expected: PASS.

Then, with the windowed scratch setup from Task 4, confirm:

1. With no project open, no row has a mark.
2. An edited map shows `*` in the browser. After saving into a project, the row shows `M` and the `*` clears.
3. Quit and open the same project: the row still shows `M`, and opening the map shows the saved edit.
4. Open the project with `Ctrl+Shift+O` while the browser is open: rows update without a restart.
5. Close the project: the `M` rows go back to unmarked, and the map reads from the game again.

- [ ] **Step 8: Commit**

```bash
git add src test/test_map_state.c
git commit -m "Reload the area browser on project changes and mark map states"
```

---

### Task 6: Documentation and full verification

**Files:**
- Modify: `CHANGELOG.md`, `README.md`, `docs/testing/native-release-checklist.md`

- [ ] **Step 1: Update the docs**

Add to `CHANGELOG.md`, in the section for the next unreleased version, under `### Changed`:

```markdown
- **Saved maps go into a project.** The first save asks you to create or open one, and maps opened from the game's archives now save into it, instead of failing or writing to a relative path. `mod_dir` still works as a read overlay. Closing a project restores it.
- **Unsaved edits are tracked.** Opening a map from the area browser used to discard every open map's edits with no warning; it now asks first. So do closing a project and quitting, and only when something is unsaved. The browser marks unsaved maps with `*`, project copies of game maps with `M`, and project-only maps with `N`.
- **Ctrl+Shift+S saves every map with unsaved edits, and Ctrl+S with no map open does nothing,** instead of reporting a save that never happened.
```

Add to the controls table in `README.md`, near the existing `Ctrl+U` row:

```markdown
| **Ctrl+S** | Save the current map into the project, asking for a project first if none is open |
| **Ctrl+Shift+S** | Save every map with unsaved edits |
| **Ctrl+Shift+N / O / W** | New, Open or Close Project |
```

In `docs/testing/native-release-checklist.md`, replace the **E11** item with:

```markdown
- [ ] **E11 Unsaved-change handling.** With no unsaved edits, quitting asks
  nothing. With an unsaved edit, quit: the WARNING dialog offers (S) Save all and
  quit, (Q) Quit, (Esc) Cancel. Esc returns to the editor with the edit intact. Q
  quits and leaves a `.tmp` autosave and does not overwrite the map. S saves and
  quits. Opening another map from the browser with an unsaved edit asks first, and
  (Esc) keeps the map open with the edit intact.
```

Add after **E13**, changing `Total items: 13.` to `Total items: 14.`:

```markdown
- [ ] **E14 Project prompt and browser marks.** With no project open, edit a map
  from the game's archives and press **Ctrl+S**: the editor asks for a project,
  and (Esc) writes nothing. Create one: the map saves under
  `<project>/Global/Tiles/`, nothing changes in the game folder, and the browser
  row shows `M`. Unsaved edits show `*`.
```

- [ ] **Step 2: Run the full Windows test lane**

Run: `"$CTEST" --test-dir tmp/build-lifecycle -C Release -L portable -j 4 --timeout 300 --output-on-failure`
Expected: PASS. Rerun anything that times out on its own.

- [ ] **Step 3: Run the Linux lane**

Run the same CTest label on a Linux build, in CI or WSL. Pillow is needed for the image tests, and `py_release_workflows` must run from a checkout WSL's `git` can read.
Expected: PASS.

- [ ] **Step 4: Confirm the game folder is untouched**

After the manual checks, list recently changed files in the game folder.
Expected: none.

- [ ] **Step 5: Commit**

```bash
git add CHANGELOG.md README.md docs/testing/native-release-checklist.md
git commit -m "Document project saving and unsaved-edit prompts"
```
