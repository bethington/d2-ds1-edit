# Map Lifecycle, Step 2: Commands and Menus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One command table drives a drawn menu bar and the lifecycle hotkeys, with exact modifier matching, so a menu can't disagree with a hotkey and `Ctrl+N` can't also fire plain `N`.

**Architecture:** `ui/commands.c` holds the table, availability rules and menu navigation as pure logic with unit tests. `ui/menu_bar.c` draws the bar, runs the modal menu loop and polls hotkeys. `interface.c` runs a chosen command through one switch, and polled keys it still owns (layers, zoom, grid, panels) are reached from menus by marking their key as hit for one frame.

**Tech Stack:** C99, Allegro 5.2, CMake and CTest, Unity (`test/unity`).

**Spec:** `docs/superpowers/specs/2026-11-02-map-lifecycle-design.md` ("Menus", "Commands and hotkeys", Build order step 2). Read it first.

**Later plans:** step 3 (create and delete) implements the commands this plan shows dimmed in the Map menu. Step 4 (the MPQ gate, build and test) implements the Build menu and Project Settings.

## Global Constraints

Every task's requirements implicitly include all of these.

- **Starting point:** `release-2.2.1-prep` at `20b55f6` or later. Line numbers below are from `20b55f6`, so locate edits by the quoted code.
- **Platforms:** C99 that builds with MSVC (Win32 x86), GCC and Clang. Windows, Linux and macOS ship together.
- **Drawn menus:** "A menu bar drawn by the editor, not native OS menus."
- **One table:** "One table drives menus and hotkeys."
- **Input rules, verbatim from the spec:** `Alt+F`, `E`, `M`, `V` and `B` open a menu. Arrow keys move, `Enter` runs a command and `Esc` closes. An open menu owns all input, so nothing reaches the map behind it. `Esc` closes a menu first; only with no menu open does it reach the quit dialog. Unavailable commands appear dimmed, and show their hotkey either way.
- **macOS:** `Cmd` is accepted wherever `Ctrl` is, on that platform only.
- **Golden images:** headless renders must stay pixel-identical. The bar has height 0 in `--headless`.
- **Windowed only:** manual runs use `full_screen = NO`. Never run the editor fullscreen.
- **Game data:** nothing may write to the game install. Manual testing uses a scratch project folder.
- **Commits:** imperative sentence subject, ending with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Windows build (PowerShell, literal paths):**
  ```powershell
  & "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B tmp/build-lifecycle -G "Visual Studio 17 2022" -A Win32 -DCMAKE_PREFIX_PATH=C:/Users/benam/source/cpp/d2-ds1-edit/build/vcpkg_installed/x86-windows
  & "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build tmp/build-lifecycle --config Release -- /m /v:q /nologo
  & "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir tmp/build-lifecycle -C Release -R "^(commands|input_edges)$" --output-on-failure
  ```

## Decisions this plan makes

- **The bar overlays the map.** The HUD rows and side panels move down by the bar's height, and the map keeps drawing under them, as it already does under the HUD. Nothing about map coordinates changes, so hit testing stays correct; a press on the bar is taken by the menu code before the map sees it.
- **F12 is not in the View menu.** It holds gamma while pressed rather than toggling a layer, so a menu row can't drive it. `F1` to `F11` are listed.
- **Map commands act on the open map, not the hovered row.** The browser's selected row follows the mouse, so `Ctrl+Del` on it would delete whatever the pointer happens to rest over. `Ctrl+Del`, `Ctrl+N` and `Ctrl+D` use `loaded_group` and `loaded_entry`.
- **Step 2 keeps today's New and Clone.** `Ctrl+N` and `Ctrl+D` run the existing create and clone code; the dialog comes in step 3.
- **The close-during-"saved" bug is fixed here.** Moving `Ctrl+S` into the command switch drops the `msg_save_main` branch that set `done`, so a pending close reaches the quit check.

## File Structure

| File | Change | Responsibility |
|---|---|---|
| `src/ui/commands.c`, `.h` | Create | Command table, key lookup, availability, menu navigation and geometry; pure |
| `src/ui/menu_bar.c`, `.h` | Create | Draw the bar, run the modal menu, poll hotkeys |
| `src/ui/ui_layout.h` | Create | Bar height and top-of-panels height |
| `src/ui/interface.c` | Modify | Command switch; remove moved handlers |
| `src/ui/project_menu.c`, `.h` | Modify | Public close and export actions; remove the shortcut poll |
| `src/ui/preset_picker.c`, `.h` | Modify | Public open action; remove the shortcut poll |
| `src/ui/compat.h` | Modify | macOS `Cmd` reads as `Ctrl` |
| `src/render/preview.c` | Modify | Move HUD rows below the bar |
| `src/core/area_browser.c`, `src/ui/props_panel.c` | Modify | Panels start below the bar |
| `src/misc.c` | Modify | Draw the bar when presenting |
| `test/test_commands.c` | Create | Table, conflicts, availability, navigation |
| `README.md`, `docs/getting-started/manual.md`, `docs/testing/native-release-checklist.md`, `CHANGELOG.md` | Modify | Controls and checklist |

---

### Task 1: The command table

**Files:**
- Create: `src/ui/commands.c`, `src/ui/commands.h`, `test/test_commands.c`
- Modify: `CMakeLists.txt`, `test/CMakeLists.txt`

**Interfaces:**
- Produces, in `ui/commands.h`:
  ```c
  CMD_ID commands_find_key(int keycode, int mods);
  const CMD_DEF *commands_get(CMD_ID id);
  int commands_is_available(CMD_ID id, const CMD_CTX *ctx);
  int commands_conflict_count(void);
  int commands_menu_item_count(int menu);
  CMD_ID commands_menu_item(int menu, int index);
  void menu_state_open(MENU_STATE *st, int menu);
  CMD_ID menu_state_key(MENU_STATE *st, int keycode, const CMD_CTX *ctx);
  int menu_title_x(int menu);
  int menu_title_at(int mx, int my, int bar_h);
  int menu_item_at(int menu, int mx, int my, int bar_h);
  ```

- [ ] **Step 1: Write the header**

Create `src/ui/commands.h`:

```c
/* The editor's commands: one table for menus and hotkeys, so the two can't
 * disagree. Everything here is pure: no display, no input state. ui/menu_bar.c
 * connects it to the keyboard and screen, and ui/interface.c runs the result. */
#ifndef DS1EDIT_COMMANDS_H
#define DS1EDIT_COMMANDS_H

typedef enum
{
   CMD_NONE = 0,
   /* File */
   CMD_NEW_PROJECT, CMD_OPEN_PROJECT, CMD_CLOSE_PROJECT, CMD_SAVE, CMD_SAVE_ALL,
   CMD_OPEN_PRESET, CMD_EXPORT_ASSETS, CMD_PROJECT_SETTINGS, CMD_QUIT,
   /* Edit */
   CMD_UNDO, CMD_CUT, CMD_COPY, CMD_DELETE_SELECTION,
   /* Map */
   CMD_MAP_NEW, CMD_MAP_CLONE, CMD_MAP_REVERT, CMD_MAP_DELETE, CMD_MAP_REMOVE,
   CMD_MAP_RESTORE_SLOT, CMD_MAP_RESTORE_BACKUP, CMD_MAP_PROPERTIES,
   /* View */
   CMD_VIEW_FLOOR1, CMD_VIEW_FLOOR2, CMD_VIEW_ANIMS, CMD_VIEW_OBJECTS,
   CMD_VIEW_WALL1, CMD_VIEW_WALL2, CMD_VIEW_WALL3, CMD_VIEW_WALL4,
   CMD_VIEW_SPECIALS, CMD_VIEW_PATHS, CMD_VIEW_SHADOWS,
   CMD_VIEW_ZOOM_IN, CMD_VIEW_ZOOM_OUT, CMD_VIEW_GRID,
   CMD_VIEW_AREAS, CMD_VIEW_PROPERTIES,
   /* Build */
   CMD_BUILD_MPQ, CMD_BUILD_AND_TEST, CMD_LAUNCH_PROFILE,
   CMD_COUNT
} CMD_ID;

enum { MENU_FILE, MENU_EDIT, MENU_MAP, MENU_VIEW, MENU_BUILD, MENU_COUNT };

#define CMD_MOD_CTRL  1
#define CMD_MOD_SHIFT 2
#define CMD_MOD_ALT   4

#define MENU_ITEM_H   14
#define MENU_DROP_W   248

typedef struct CMD_DEF
{
   CMD_ID      id;
   const char *label;
   int         menu;
   const char *shortcut;   /* shown in the menu; "" for none */
   int         key;        /* ALLEGRO_KEY_* this table dispatches; 0 if none */
   int         mods;       /* exact CMD_MOD_* that must be held with key */
   int         inject_key; /* key the existing polled code answers; 0 if none */
} CMD_DEF;

/* What availability depends on, gathered by the caller. */
typedef struct CMD_CTX
{
   int tile_mode;
   int object_mode;
   int has_map;
   int has_open_entry;   /* the open map has a row in the area browser */
   int any_dirty;
   int paste_active;
   int can_undo_object;
} CMD_CTX;

/* An open menu. menu is -1 when closed. */
typedef struct MENU_STATE
{
   int menu;
   int item;
} MENU_STATE;

/* The table entry the keyboard dispatches for this exact key and modifier set,
 * or CMD_NONE. Keys the existing polled code still owns are never returned. */
CMD_ID commands_find_key(int keycode, int mods);

const CMD_DEF *commands_get(CMD_ID id);
int commands_is_available(CMD_ID id, const CMD_CTX *ctx);

/* Pairs of table entries that dispatch the same key and modifiers. */
int commands_conflict_count(void);

int commands_menu_item_count(int menu);
CMD_ID commands_menu_item(int menu, int index);

void menu_state_open(MENU_STATE *st, int menu);

/* Apply a key to an open menu. Returns the command Enter chose, else CMD_NONE.
 * st->menu becomes -1 once the menu should close. */
CMD_ID menu_state_key(MENU_STATE *st, int keycode, const CMD_CTX *ctx);

/* Geometry, from the 8-pixel built-in font. */
const char *menu_title(int menu);
int menu_title_x(int menu);
int menu_title_at(int mx, int my, int bar_h);
int menu_item_at(int menu, int mx, int my, int bar_h);

#endif
```

- [ ] **Step 2: Write the failing tests**

Create `test/test_commands.c`:

```c
/* The command table: what each key does, what is available when, and how an
 * open menu reacts to keys and the mouse. No display or input state involved. */
#include <string.h>
#include <allegro5/keycodes.h>

#include "unity/unity.h"
#include "ui/commands.h"

void setUp(void) {}
void tearDown(void) {}

static CMD_CTX tile_ctx(void)
{
   CMD_CTX c;
   memset(&c, 0, sizeof(c));
   c.tile_mode = 1;
   c.has_map = 1;
   c.has_open_entry = 1;
   return c;
}

void test_no_two_commands_share_a_key_and_modifiers(void)
{
   TEST_ASSERT_EQUAL_INT(0, commands_conflict_count());
}

void test_modifiers_must_match_exactly(void)
{
   TEST_ASSERT_EQUAL_INT(CMD_SAVE, commands_find_key(ALLEGRO_KEY_S, CMD_MOD_CTRL));
   TEST_ASSERT_EQUAL_INT(CMD_SAVE_ALL, commands_find_key(ALLEGRO_KEY_S, CMD_MOD_CTRL | CMD_MOD_SHIFT));
   TEST_ASSERT_EQUAL_INT(CMD_NONE, commands_find_key(ALLEGRO_KEY_S, 0));
   TEST_ASSERT_EQUAL_INT(CMD_NONE, commands_find_key(ALLEGRO_KEY_S, CMD_MOD_SHIFT));
   TEST_ASSERT_EQUAL_INT(CMD_MAP_NEW, commands_find_key(ALLEGRO_KEY_N, CMD_MOD_CTRL));
   TEST_ASSERT_EQUAL_INT(CMD_NEW_PROJECT, commands_find_key(ALLEGRO_KEY_N, CMD_MOD_CTRL | CMD_MOD_SHIFT));
   TEST_ASSERT_EQUAL_INT(CMD_NONE, commands_find_key(ALLEGRO_KEY_N, 0));
}

void test_spec_bindings(void)
{
   TEST_ASSERT_EQUAL_INT(CMD_OPEN_PROJECT, commands_find_key(ALLEGRO_KEY_O, CMD_MOD_CTRL | CMD_MOD_SHIFT));
   TEST_ASSERT_EQUAL_INT(CMD_CLOSE_PROJECT, commands_find_key(ALLEGRO_KEY_W, CMD_MOD_CTRL | CMD_MOD_SHIFT));
   TEST_ASSERT_EQUAL_INT(CMD_MAP_CLONE, commands_find_key(ALLEGRO_KEY_D, CMD_MOD_CTRL));
   TEST_ASSERT_EQUAL_INT(CMD_MAP_DELETE, commands_find_key(ALLEGRO_KEY_DELETE, CMD_MOD_CTRL));
   TEST_ASSERT_EQUAL_INT(CMD_BUILD_MPQ, commands_find_key(ALLEGRO_KEY_B, CMD_MOD_CTRL));
   TEST_ASSERT_EQUAL_INT(CMD_BUILD_AND_TEST, commands_find_key(ALLEGRO_KEY_B, CMD_MOD_CTRL | CMD_MOD_SHIFT));
   TEST_ASSERT_EQUAL_INT(CMD_OPEN_PRESET, commands_find_key(ALLEGRO_KEY_P, CMD_MOD_CTRL | CMD_MOD_SHIFT));
   TEST_ASSERT_EQUAL_INT(CMD_EXPORT_ASSETS, commands_find_key(ALLEGRO_KEY_A, CMD_MOD_CTRL | CMD_MOD_SHIFT));
}

void test_keys_the_polled_code_owns_are_not_dispatched(void)
{
   TEST_ASSERT_EQUAL_INT(CMD_NONE, commands_find_key(ALLEGRO_KEY_Z, CMD_MOD_CTRL));
   TEST_ASSERT_EQUAL_INT(CMD_NONE, commands_find_key(ALLEGRO_KEY_X, CMD_MOD_CTRL));
   TEST_ASSERT_EQUAL_INT(CMD_NONE, commands_find_key(ALLEGRO_KEY_DELETE, 0));
   TEST_ASSERT_EQUAL_INT(CMD_NONE, commands_find_key(ALLEGRO_KEY_INSERT, 0));
   TEST_ASSERT_EQUAL_INT(CMD_NONE, commands_find_key(ALLEGRO_KEY_F1, 0));
}

void test_every_command_is_in_exactly_one_menu(void)
{
   int m, i, seen[CMD_COUNT];

   memset(seen, 0, sizeof(seen));
   for (m = 0; m < MENU_COUNT; m++)
      for (i = 0; i < commands_menu_item_count(m); i++)
         seen[commands_menu_item(m, i)]++;
   for (i = CMD_NONE + 1; i < CMD_COUNT; i++)
      TEST_ASSERT_EQUAL_INT_MESSAGE(1, seen[i], commands_get((CMD_ID) i)->label);
}

void test_menus_follow_the_spec_order(void)
{
   TEST_ASSERT_EQUAL_INT(CMD_NEW_PROJECT, commands_menu_item(MENU_FILE, 0));
   TEST_ASSERT_EQUAL_INT(CMD_UNDO, commands_menu_item(MENU_EDIT, 0));
   TEST_ASSERT_EQUAL_INT(CMD_MAP_NEW, commands_menu_item(MENU_MAP, 0));
   TEST_ASSERT_EQUAL_INT(CMD_BUILD_MPQ, commands_menu_item(MENU_BUILD, 0));
   TEST_ASSERT_EQUAL_STRING("Ctrl+Del", commands_get(CMD_MAP_DELETE)->shortcut);
}

void test_availability_follows_map_mode_and_paste(void)
{
   CMD_CTX c = tile_ctx();

   TEST_ASSERT_TRUE(commands_is_available(CMD_CUT, &c));
   TEST_ASSERT_TRUE(commands_is_available(CMD_SAVE, &c));
   TEST_ASSERT_TRUE(commands_is_available(CMD_MAP_DELETE, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_SAVE_ALL, &c));

   c.any_dirty = 1;
   TEST_ASSERT_TRUE(commands_is_available(CMD_SAVE_ALL, &c));

   c.paste_active = 1;
   TEST_ASSERT_FALSE(commands_is_available(CMD_CUT, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_UNDO, &c));

   c = tile_ctx();
   c.tile_mode = 0;
   c.object_mode = 1;
   TEST_ASSERT_FALSE(commands_is_available(CMD_CUT, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_UNDO, &c));
   c.can_undo_object = 1;
   TEST_ASSERT_TRUE(commands_is_available(CMD_UNDO, &c));

   c = tile_ctx();
   c.has_map = 0;
   c.has_open_entry = 0;
   TEST_ASSERT_FALSE(commands_is_available(CMD_SAVE, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_DELETE, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_VIEW_GRID, &c));
   TEST_ASSERT_TRUE(commands_is_available(CMD_NEW_PROJECT, &c));
   TEST_ASSERT_TRUE(commands_is_available(CMD_VIEW_AREAS, &c));
   TEST_ASSERT_TRUE(commands_is_available(CMD_QUIT, &c));
}

void test_later_steps_stay_dimmed(void)
{
   CMD_CTX c = tile_ctx();

   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_REVERT, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_REMOVE, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_RESTORE_SLOT, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_MAP_RESTORE_BACKUP, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_PROJECT_SETTINGS, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_BUILD_MPQ, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_BUILD_AND_TEST, &c));
   TEST_ASSERT_FALSE(commands_is_available(CMD_LAUNCH_PROFILE, &c));
}

void test_arrow_keys_move_and_wrap(void)
{
   CMD_CTX c = tile_ctx();
   MENU_STATE st;
   int last = commands_menu_item_count(MENU_FILE) - 1;

   menu_state_open(&st, MENU_FILE);
   TEST_ASSERT_EQUAL_INT(0, st.item);
   menu_state_key(&st, ALLEGRO_KEY_UP, &c);
   TEST_ASSERT_EQUAL_INT(last, st.item);
   menu_state_key(&st, ALLEGRO_KEY_DOWN, &c);
   TEST_ASSERT_EQUAL_INT(0, st.item);

   menu_state_key(&st, ALLEGRO_KEY_LEFT, &c);
   TEST_ASSERT_EQUAL_INT(MENU_BUILD, st.menu);
   TEST_ASSERT_EQUAL_INT(0, st.item);
   menu_state_key(&st, ALLEGRO_KEY_RIGHT, &c);
   TEST_ASSERT_EQUAL_INT(MENU_FILE, st.menu);
}

void test_enter_runs_available_commands_only(void)
{
   CMD_CTX c = tile_ctx();
   MENU_STATE st;

   menu_state_open(&st, MENU_FILE);
   TEST_ASSERT_EQUAL_INT(CMD_NEW_PROJECT, menu_state_key(&st, ALLEGRO_KEY_ENTER, &c));
   TEST_ASSERT_EQUAL_INT(-1, st.menu);

   /* A dimmed row: Enter does nothing, and the menu stays open. */
   menu_state_open(&st, MENU_BUILD);
   TEST_ASSERT_EQUAL_INT(CMD_NONE, menu_state_key(&st, ALLEGRO_KEY_ENTER, &c));
   TEST_ASSERT_EQUAL_INT(MENU_BUILD, st.menu);
}

void test_escape_closes_without_a_command(void)
{
   CMD_CTX c = tile_ctx();
   MENU_STATE st;

   menu_state_open(&st, MENU_MAP);
   TEST_ASSERT_EQUAL_INT(CMD_NONE, menu_state_key(&st, ALLEGRO_KEY_ESCAPE, &c));
   TEST_ASSERT_EQUAL_INT(-1, st.menu);
}

void test_mouse_geometry(void)
{
   int bar_h = 14, x = menu_title_x(MENU_MAP);

   TEST_ASSERT_EQUAL_INT(MENU_MAP, menu_title_at(x + 1, 3, bar_h));
   TEST_ASSERT_EQUAL_INT(-1, menu_title_at(x + 1, bar_h, bar_h));
   TEST_ASSERT_EQUAL_INT(-1, menu_title_at(1000, 3, bar_h));

   TEST_ASSERT_EQUAL_INT(0, menu_item_at(MENU_MAP, x + 1, bar_h + 1, bar_h));
   TEST_ASSERT_EQUAL_INT(1, menu_item_at(MENU_MAP, x + 1, bar_h + MENU_ITEM_H + 1, bar_h));
   TEST_ASSERT_EQUAL_INT(-1, menu_item_at(MENU_MAP, x + MENU_DROP_W, bar_h + 1, bar_h));
   TEST_ASSERT_EQUAL_INT(-1, menu_item_at(-1, x + 1, bar_h + 1, bar_h));
   TEST_ASSERT_EQUAL_INT(-1, menu_item_at(MENU_MAP, x + 1, bar_h + 400, bar_h));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_no_two_commands_share_a_key_and_modifiers);
   RUN_TEST(test_modifiers_must_match_exactly);
   RUN_TEST(test_spec_bindings);
   RUN_TEST(test_keys_the_polled_code_owns_are_not_dispatched);
   RUN_TEST(test_every_command_is_in_exactly_one_menu);
   RUN_TEST(test_menus_follow_the_spec_order);
   RUN_TEST(test_availability_follows_map_mode_and_paste);
   RUN_TEST(test_later_steps_stay_dimmed);
   RUN_TEST(test_arrow_keys_move_and_wrap);
   RUN_TEST(test_enter_runs_available_commands_only);
   RUN_TEST(test_escape_closes_without_a_command);
   RUN_TEST(test_mouse_geometry);
   return UNITY_END();
}
```

- [ ] **Step 3: Register the test and the source**

In `test/CMakeLists.txt`, add after the `platform_dialogs` test:

```cmake
# Command table: bindings, conflicts, availability, menu navigation
add_executable(test_commands test_commands.c ../src/ui/commands.c)
target_link_libraries(test_commands PRIVATE unity ds1edit_allegro)
target_include_directories(test_commands PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/test)
add_test(NAME commands COMMAND test_commands)
set_tests_properties(commands PROPERTIES LABELS "portable")
```

Add `test_commands` to the `DS1EDIT_ALLEGRO_TESTS` list in `test/CMakeLists.txt`. In `CMakeLists.txt`, add `src/ui/commands.c` after `src/ui/project_menu.c`.

- [ ] **Step 4: Confirm it fails**

Run the Windows configure command, then build target `test_commands`.
Expected: FAIL, with `commands.c` not found.

- [ ] **Step 5: Write the table and logic**

Create `src/ui/commands.c`:

```c
#include <string.h>
#include <allegro5/keycodes.h>

#include "ui/commands.h"

#define CS (CMD_MOD_CTRL | CMD_MOD_SHIFT)

/* Rows appear in their menus in this order. A row with `key` is dispatched from
 * this table; a row with `inject_key` reaches the polled code that owns that key. */
static const CMD_DEF s_table[] =
{
   { CMD_NEW_PROJECT,   "New Project",      MENU_FILE, "Ctrl+Shift+N", ALLEGRO_KEY_N, CS, 0 },
   { CMD_OPEN_PROJECT,  "Open Project",     MENU_FILE, "Ctrl+Shift+O", ALLEGRO_KEY_O, CS, 0 },
   { CMD_CLOSE_PROJECT, "Close Project",    MENU_FILE, "Ctrl+Shift+W", ALLEGRO_KEY_W, CS, 0 },
   { CMD_SAVE,          "Save",             MENU_FILE, "Ctrl+S", ALLEGRO_KEY_S, CMD_MOD_CTRL, 0 },
   { CMD_SAVE_ALL,      "Save All",         MENU_FILE, "Ctrl+Shift+S", ALLEGRO_KEY_S, CS, 0 },
   { CMD_OPEN_PRESET,   "Open Preset",      MENU_FILE, "Ctrl+Shift+P", ALLEGRO_KEY_P, CS, 0 },
   { CMD_EXPORT_ASSETS, "Export Assets",    MENU_FILE, "Ctrl+Shift+A", ALLEGRO_KEY_A, CS, 0 },
   { CMD_PROJECT_SETTINGS, "Project Settings", MENU_FILE, "", 0, 0, 0 },
   { CMD_QUIT,          "Quit",             MENU_FILE, "Esc", 0, 0, 0 },

   { CMD_UNDO,          "Undo",             MENU_EDIT, "Ctrl+Z", 0, 0, 0 },
   { CMD_CUT,           "Cut",              MENU_EDIT, "Ctrl+X", 0, 0, 0 },
   { CMD_COPY,          "Copy",             MENU_EDIT, "Ctrl+C", 0, 0, 0 },
   { CMD_DELETE_SELECTION, "Delete Selection", MENU_EDIT, "Del", 0, 0, ALLEGRO_KEY_DELETE },

   { CMD_MAP_NEW,       "New",              MENU_MAP, "Ctrl+N", ALLEGRO_KEY_N, CMD_MOD_CTRL, 0 },
   { CMD_MAP_CLONE,     "Clone",            MENU_MAP, "Ctrl+D", ALLEGRO_KEY_D, CMD_MOD_CTRL, 0 },
   { CMD_MAP_REVERT,    "Revert to Game Version", MENU_MAP, "", 0, 0, 0 },
   { CMD_MAP_DELETE,    "Delete Map",       MENU_MAP, "Ctrl+Del", ALLEGRO_KEY_DELETE, CMD_MOD_CTRL, 0 },
   { CMD_MAP_REMOVE,    "Remove from Project", MENU_MAP, "", 0, 0, 0 },
   { CMD_MAP_RESTORE_SLOT, "Restore Slot",  MENU_MAP, "", 0, 0, 0 },
   { CMD_MAP_RESTORE_BACKUP, "Restore from Backup", MENU_MAP, "", 0, 0, 0 },
   { CMD_MAP_PROPERTIES, "Properties",      MENU_MAP, "]", 0, 0, ALLEGRO_KEY_CLOSEBRACE },

   { CMD_VIEW_FLOOR1,   "Floor 1",          MENU_VIEW, "F1", 0, 0, ALLEGRO_KEY_F1 },
   { CMD_VIEW_FLOOR2,   "Floor 2",          MENU_VIEW, "F2", 0, 0, ALLEGRO_KEY_F2 },
   { CMD_VIEW_ANIMS,    "Animations",       MENU_VIEW, "F3", 0, 0, ALLEGRO_KEY_F3 },
   { CMD_VIEW_OBJECTS,  "Objects",          MENU_VIEW, "F4", 0, 0, ALLEGRO_KEY_F4 },
   { CMD_VIEW_WALL1,    "Wall 1",           MENU_VIEW, "F5", 0, 0, ALLEGRO_KEY_F5 },
   { CMD_VIEW_WALL2,    "Wall 2",           MENU_VIEW, "F6", 0, 0, ALLEGRO_KEY_F6 },
   { CMD_VIEW_WALL3,    "Wall 3",           MENU_VIEW, "F7", 0, 0, ALLEGRO_KEY_F7 },
   { CMD_VIEW_WALL4,    "Wall 4",           MENU_VIEW, "F8", 0, 0, ALLEGRO_KEY_F8 },
   { CMD_VIEW_SPECIALS, "Specials",         MENU_VIEW, "F9", 0, 0, ALLEGRO_KEY_F9 },
   { CMD_VIEW_PATHS,    "Paths",            MENU_VIEW, "F10", 0, 0, ALLEGRO_KEY_F10 },
   { CMD_VIEW_SHADOWS,  "Shadows",          MENU_VIEW, "F11", 0, 0, ALLEGRO_KEY_F11 },
   { CMD_VIEW_ZOOM_IN,  "Zoom In",          MENU_VIEW, "+", 0, 0, ALLEGRO_KEY_EQUALS },
   { CMD_VIEW_ZOOM_OUT, "Zoom Out",         MENU_VIEW, "-", 0, 0, ALLEGRO_KEY_MINUS },
   { CMD_VIEW_GRID,     "Grid",             MENU_VIEW, "G", 0, 0, ALLEGRO_KEY_G },
   { CMD_VIEW_AREAS,    "Areas Panel",      MENU_VIEW, "[", 0, 0, ALLEGRO_KEY_OPENBRACE },
   { CMD_VIEW_PROPERTIES, "Properties Panel", MENU_VIEW, "]", 0, 0, ALLEGRO_KEY_CLOSEBRACE },

   { CMD_BUILD_MPQ,     "Build MPQ",        MENU_BUILD, "Ctrl+B", ALLEGRO_KEY_B, CMD_MOD_CTRL, 0 },
   { CMD_BUILD_AND_TEST, "Build & Test",    MENU_BUILD, "Ctrl+Shift+B", ALLEGRO_KEY_B, CS, 0 },
   { CMD_LAUNCH_PROFILE, "Launch Profile",  MENU_BUILD, "", 0, 0, 0 },
};

#define TABLE_COUNT ((int) (sizeof(s_table) / sizeof(s_table[0])))

static const char * const s_titles[MENU_COUNT] = { "File", "Edit", "Map", "View", "Build" };

/* Titles are this far apart, in the built-in font's 8-pixel cells. */
#define TITLE_PAD_X 8
#define TITLE_GAP   16

CMD_ID commands_find_key(int keycode, int mods)
{
   int i;

   for (i = 0; i < TABLE_COUNT; i++)
      if (s_table[i].key != 0 && s_table[i].key == keycode && s_table[i].mods == mods)
         return s_table[i].id;
   return CMD_NONE;
}

const CMD_DEF *commands_get(CMD_ID id)
{
   int i;

   for (i = 0; i < TABLE_COUNT; i++)
      if (s_table[i].id == id)
         return &s_table[i];
   return NULL;
}

int commands_is_available(CMD_ID id, const CMD_CTX *c)
{
   switch (id)
   {
   case CMD_NEW_PROJECT:
   case CMD_OPEN_PROJECT:
   case CMD_CLOSE_PROJECT:
   case CMD_OPEN_PRESET:
   case CMD_EXPORT_ASSETS:
   case CMD_QUIT:
   case CMD_MAP_NEW:
   case CMD_VIEW_AREAS:
   case CMD_VIEW_PROPERTIES:
   case CMD_MAP_PROPERTIES:
      return 1;

   case CMD_SAVE_ALL:
      return c->any_dirty;

   case CMD_UNDO:
      if (c->tile_mode)
         return c->has_map && !c->paste_active;
      return c->object_mode && c->can_undo_object;

   case CMD_CUT:
   case CMD_COPY:
      return c->tile_mode && c->has_map && !c->paste_active;

   case CMD_DELETE_SELECTION:
      return c->has_map && !c->paste_active && (c->tile_mode || c->object_mode);

   case CMD_MAP_CLONE:
   case CMD_MAP_DELETE:
      return c->has_map && c->has_open_entry;

   /* Step 3 and step 4 implement these. */
   case CMD_MAP_REVERT:
   case CMD_MAP_REMOVE:
   case CMD_MAP_RESTORE_SLOT:
   case CMD_MAP_RESTORE_BACKUP:
   case CMD_PROJECT_SETTINGS:
   case CMD_BUILD_MPQ:
   case CMD_BUILD_AND_TEST:
   case CMD_LAUNCH_PROFILE:
      return 0;

   /* Save and the rest of View need a map on screen. */
   default:
      return c->has_map;
   }
}

int commands_conflict_count(void)
{
   int i, j, n = 0;

   for (i = 0; i < TABLE_COUNT; i++)
      for (j = i + 1; j < TABLE_COUNT; j++)
         if (s_table[i].key != 0 && s_table[i].key == s_table[j].key &&
             s_table[i].mods == s_table[j].mods)
            n++;
   return n;
}

int commands_menu_item_count(int menu)
{
   int i, n = 0;

   for (i = 0; i < TABLE_COUNT; i++)
      if (s_table[i].menu == menu)
         n++;
   return n;
}

CMD_ID commands_menu_item(int menu, int index)
{
   int i, n = 0;

   for (i = 0; i < TABLE_COUNT; i++)
      if (s_table[i].menu == menu && n++ == index)
         return s_table[i].id;
   return CMD_NONE;
}

void menu_state_open(MENU_STATE *st, int menu)
{
   st->menu = menu;
   st->item = 0;
}

CMD_ID menu_state_key(MENU_STATE *st, int keycode, const CMD_CTX *ctx)
{
   int count;
   CMD_ID id;

   if (st->menu < 0)
      return CMD_NONE;
   count = commands_menu_item_count(st->menu);

   switch (keycode)
   {
   case ALLEGRO_KEY_ESCAPE:
      st->menu = -1;
      break;
   case ALLEGRO_KEY_UP:
      st->item = (st->item + count - 1) % count;
      break;
   case ALLEGRO_KEY_DOWN:
      st->item = (st->item + 1) % count;
      break;
   case ALLEGRO_KEY_LEFT:
      menu_state_open(st, (st->menu + MENU_COUNT - 1) % MENU_COUNT);
      break;
   case ALLEGRO_KEY_RIGHT:
      menu_state_open(st, (st->menu + 1) % MENU_COUNT);
      break;
   case ALLEGRO_KEY_ENTER:
   case ALLEGRO_KEY_PAD_ENTER:
      id = commands_menu_item(st->menu, st->item);
      if (commands_is_available(id, ctx))
      {
         st->menu = -1;
         return id;
      }
      break;
   default:
      break;
   }
   return CMD_NONE;
}

const char *menu_title(int menu)
{
   return s_titles[menu];
}

int menu_title_x(int menu)
{
   int i, x = TITLE_PAD_X;

   for (i = 0; i < menu; i++)
      x += (int) strlen(s_titles[i]) * 8 + TITLE_GAP;
   return x;
}

int menu_title_at(int mx, int my, int bar_h)
{
   int i;

   if (my < 0 || my >= bar_h)
      return -1;
   for (i = 0; i < MENU_COUNT; i++)
   {
      int x = menu_title_x(i);
      if (mx >= x - TITLE_GAP / 2 &&
          mx < x + (int) strlen(s_titles[i]) * 8 + TITLE_GAP / 2)
         return i;
   }
   return -1;
}

int menu_item_at(int menu, int mx, int my, int bar_h)
{
   int x, row;

   if (menu < 0)
      return -1;
   x = menu_title_x(menu);
   if (mx < x || mx >= x + MENU_DROP_W || my < bar_h)
      return -1;
   row = (my - bar_h) / MENU_ITEM_H;
   return row < commands_menu_item_count(menu) ? row : -1;
}
```

- [ ] **Step 6: Run the tests**

Build target `test_commands` and run `ctest -R "^commands$"`.
Expected: PASS, 12 tests.

- [ ] **Step 7: Commit**

```bash
git add src/ui/commands.c src/ui/commands.h test/test_commands.c test/CMakeLists.txt CMakeLists.txt
git commit -m "Add a command table for menus and hotkeys"
```

---

### Task 2: Make room for the bar and draw it

**Files:**
- Create: `src/ui/ui_layout.h`, `src/ui/menu_bar.c`, `src/ui/menu_bar.h`
- Modify: `src/render/preview.c`, `src/core/area_browser.c`, `src/ui/props_panel.c`, `src/ui/interface.c`, `src/misc.c`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `menu_title`, `menu_title_x`, `commands_menu_item`, `commands_menu_item_count`, `commands_get`, `commands_is_available` from Task 1.
- Produces:
  ```c
  // ui/ui_layout.h
  int ui_menu_bar_height(void);  // 0 in --headless
  int ui_top_bar_height(void);   // bar plus HUD rows
  // ui/menu_bar.h
  void menu_bar_draw(void);      // draws onto the current target; Task 3 adds input
  ```

- [ ] **Step 1: Add the layout helpers**

Create `src/ui/ui_layout.h`:

```c
/* Rows across the top of the window, from the top down: the menu bar, then one
 * or two HUD rows, then the side panels. Headless renders have no bar, so their
 * golden images don't change. */
#ifndef DS1EDIT_UI_LAYOUT_H
#define DS1EDIT_UI_LAYOUT_H

#include "structs.h"

#define UI_MENU_BAR_H 14

static inline int ui_menu_bar_height(void)
{
   return glb_ds1edit.cmd_line.headless_mode ? 0 : UI_MENU_BAR_H;
}

static inline int ui_top_bar_height(void)
{
   return ui_menu_bar_height() + (glb_ds1edit.show_2nd_row ? 20 : 9);
}

#endif
```

- [ ] **Step 2: Start the panels below the bar**

Add `#include "ui/ui_layout.h"` to `src/core/area_browser.c`, `src/ui/props_panel.c` and `src/ui/interface.c`.

In `src/core/area_browser.c` (three places) and `src/ui/props_panel.c` (three places), replace:

```c
   int top_bar_h = glb_ds1edit.show_2nd_row ? 20 : 9;
```

with:

```c
   int top_bar_h = ui_top_bar_height();
```

In `src/ui/interface.c`, replace:

```c
            int s_top = glb_ds1edit.show_2nd_row ? 20 : 9;
```

with:

```c
            int s_top = ui_top_bar_height();
```

Confirm with a search: `show_2nd_row ? 20 : 9` now appears only in `ui_layout.h`.

- [ ] **Step 3: Move the HUD rows down**

In `src/render/preview.c`, add `#include "ui/ui_layout.h"`. Directly above the comment `// make up & bottom border black, with white line`, add:

```c
   /* The HUD sits under the menu bar; the bar covers the rows above it. */
   const int hud_y = ui_menu_bar_height();
```

The project is C99, so a declaration here is fine.

Replace the top-border block:

```c
   if (glb_ds1edit.show_2nd_row == TRUE)
   {
      wpreview_draw_rectfill(0, 0, glb_config.screen.width, 19, 0);
      wpreview_draw_hline(0, 9, glb_config.screen.width, 29);
      wpreview_draw_hline(0, 20, glb_config.screen.width, 255);

      // 2nd row datas
      wpreview_textprintf(0, 11, 255, "Set:");
      wpreview_textprintf(32, 11, 109, "%i", glb_ds1edit.ds1_group_idx + 1);

      wpreview_textprintf(65, 11, 255, "Ds1Index:");
      wpreview_textprintf(137, 11, 109, "%i", ds1_idx + 1);

      wpreview_textprintf(175, 11, 255, "File:");
      wpreview_textprintf(215, 11, 109, "%s", glb_ds1[ds1_idx].name);
   }
   else
   {
      wpreview_draw_rectfill(0, 0, glb_config.screen.width, 8, 0);
      wpreview_draw_hline(0, 9, glb_config.screen.width, 255);
   }
```

with:

```c
   if (glb_ds1edit.show_2nd_row == TRUE)
   {
      wpreview_draw_rectfill(0, 0, glb_config.screen.width, hud_y + 19, 0);
      wpreview_draw_hline(0, hud_y + 9, glb_config.screen.width, 29);
      wpreview_draw_hline(0, hud_y + 20, glb_config.screen.width, 255);

      // 2nd row datas
      wpreview_textprintf(0, hud_y + 11, 255, "Set:");
      wpreview_textprintf(32, hud_y + 11, 109, "%i", glb_ds1edit.ds1_group_idx + 1);

      wpreview_textprintf(65, hud_y + 11, 255, "Ds1Index:");
      wpreview_textprintf(137, hud_y + 11, 109, "%i", ds1_idx + 1);

      wpreview_textprintf(175, hud_y + 11, 255, "File:");
      wpreview_textprintf(215, hud_y + 11, 109, "%s", glb_ds1[ds1_idx].name);
   }
   else
   {
      wpreview_draw_rectfill(0, 0, glb_config.screen.width, hud_y + 8, 0);
      wpreview_draw_hline(0, hud_y + 9, glb_config.screen.width, 255);
   }
```

From the comment `// layers toggle` to the end of that function, change the `y` argument from `0` to `hud_y` in every `wpreview_textprintf` whose `y` is literally `0`. That covers the `f%i`, `anims`, `obj`, `spl`, `w%i`, `path` and `shad` labels, `zoom=`, `gamma=`, the file name at `x = 606`, and the multi-line `fps=` call at `glb_config.screen.width - 80`. Leave every call at `glb_config.screen.height - 8` and the debug line at `y = 40` alone.

- [ ] **Step 4: Draw the bar**

Create `src/ui/menu_bar.h`:

```c
#ifndef DS1EDIT_MENU_BAR_H
#define DS1EDIT_MENU_BAR_H

// Draw the bar and any open menu onto the current target. Does nothing when the
// bar has no height, as in headless renders.
void menu_bar_draw(void);

#endif
```

Create `src/ui/menu_bar.c`:

```c
#include <stdio.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "ui/compat.h"
#include "ui/commands.h"
#include "ui/menu_bar.h"
#include "ui/ui_layout.h"

/* The open menu, or -1. Only the modal loop opens one. */
static MENU_STATE s_state = { -1, 0 };
static CMD_CTX    s_ctx;

static void draw_dropdown(int bar_h)
{
   ALLEGRO_COLOR col_bg     = al_map_rgb(24, 20, 16);
   ALLEGRO_COLOR col_border = al_map_rgb(120, 110, 90);
   ALLEGRO_COLOR col_hover  = al_map_rgb(60, 80, 130);
   ALLEGRO_COLOR col_text   = al_map_rgb(235, 225, 200);
   ALLEGRO_COLOR col_dim    = al_map_rgb(110, 105, 95);
   int count = commands_menu_item_count(s_state.menu);
   int x = menu_title_x(s_state.menu);
   int i;

   al_draw_filled_rectangle((float) x, (float) bar_h, (float) (x + MENU_DROP_W),
                            (float) (bar_h + count * MENU_ITEM_H), col_bg);
   al_draw_rectangle((float) x + 0.5f, (float) bar_h + 0.5f,
                     (float) (x + MENU_DROP_W) - 0.5f,
                     (float) (bar_h + count * MENU_ITEM_H) - 0.5f, col_border, 1.0f);

   for (i = 0; i < count; i++)
   {
      const CMD_DEF *def = commands_get(commands_menu_item(s_state.menu, i));
      int y = bar_h + i * MENU_ITEM_H;
      ALLEGRO_COLOR col = commands_is_available(def->id, &s_ctx) ? col_text : col_dim;

      if (i == s_state.item)
         al_draw_filled_rectangle((float) x + 1, (float) y, (float) (x + MENU_DROP_W) - 1,
                                  (float) (y + MENU_ITEM_H), col_hover);
      al_draw_text(a5_font, col, (float) (x + 6), (float) (y + 3), 0, def->label);
      al_draw_text(a5_font, col, (float) (x + MENU_DROP_W - 6), (float) (y + 3),
                   ALLEGRO_ALIGN_RIGHT, def->shortcut);
   }
}

void menu_bar_draw(void)
{
   int h = ui_menu_bar_height();
   int w, i;

   if (h == 0)
      return;
   w = al_get_display_width(a5_display);

   al_draw_filled_rectangle(0, 0, (float) w, (float) h, al_map_rgb(36, 30, 24));
   al_draw_line(0, (float) h - 0.5f, (float) w, (float) h - 0.5f, al_map_rgb(120, 110, 90), 1.0f);

   for (i = 0; i < MENU_COUNT; i++)
   {
      int x = menu_title_x(i);
      if (i == s_state.menu)
         al_draw_filled_rectangle((float) (x - 4), 0,
                                  (float) (x + (int) strlen(menu_title(i)) * 8 + 4),
                                  (float) (h - 1), al_map_rgb(60, 80, 130));
      al_draw_text(a5_font, al_map_rgb(235, 225, 200), (float) x, 3, 0, menu_title(i));
   }

   if (s_state.menu >= 0)
      draw_dropdown(h);
}
```

In `CMakeLists.txt`, add `src/ui/menu_bar.c` after `src/ui/commands.c`.

In `src/misc.c`, add `#include "ui/menu_bar.h"` and, in `misc_draw_screen`, directly above the comment `/* Hand the pointer to the compositor when we can`, add:

```c
   menu_bar_draw();
```

- [ ] **Step 5: Build and test**

Build everything, then run the portable label: `ctest --test-dir tmp/build-lifecycle -C Release -L portable -j 4 --timeout 300`. Rerun anything that times out alone.
Expected: PASS. The editor shows a File, Edit, Map, View and Build bar, which doesn't respond yet. The golden tests stay valid because `--headless` gives the bar no height.

- [ ] **Step 6: Commit**

Stage the files listed in this task by path, then:

```bash
git commit -m "Draw a menu bar above the HUD rows"
```

---

### Task 3: Open menus and poll hotkeys

**Files:**
- Modify: `src/ui/menu_bar.c`, `src/ui/menu_bar.h`

**Interfaces:**
- Consumes: Task 1's `CMD_CTX`, `menu_state_open`, `menu_state_key`, `menu_title_at`, `menu_item_at`, `commands_find_key`; `input_pump`, `input_suppress_held`, `key_hit`, `mouse_hit` from `ui/input.h`; `misc_draw_screen` from `misc.h`.
- Produces:
  ```c
  // Run a pressed menu or hotkey. Returns an available command, else CMD_NONE.
  CMD_ID menu_bar_poll(const CMD_CTX *ctx, int keys_enabled);
  ```

The behaviour here needs a display, so it is checked by hand in Task 5. Navigation and geometry are already unit-tested.

- [ ] **Step 1: Add the header entry**

In `src/ui/menu_bar.h`, add `#include "ui/commands.h"` and, before the final `#endif`:

```c
// Call once a frame. Opens a menu on Alt+F, E, M, V or B or a press on the bar,
// and runs it modally: an open menu owns all input until it closes. Otherwise
// looks up this frame's hotkey, unless keys_enabled is 0 because a text field
// has the keyboard. A dispatched key is used up, so polled code can't also act
// on it. Returns a command to run, or CMD_NONE. A window close while a menu is
// open closes the menu and stays pending for the caller.
CMD_ID menu_bar_poll(const CMD_CTX *ctx, int keys_enabled);
```

- [ ] **Step 2: Implement it**

Add to the includes of `src/ui/menu_bar.c`:

```c
#include "misc.h"
#include "ui/input.h"
```

Append to `src/ui/menu_bar.c`:

```c
static int held_mods(void)
{
   int mods = 0;

   if (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
      mods |= CMD_MOD_CTRL;
   if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
      mods |= CMD_MOD_SHIFT;
   if (key_pressed(KEY_ALT))
      mods |= CMD_MOD_ALT;
   return mods;
}

/* Run an open menu until it closes. start_menu is -1 for a press on an empty
 * part of the bar, which only waits for the release. */
static CMD_ID run_menu(int start_menu, const CMD_CTX *ctx)
{
   static const int nav_keys[] = {
      ALLEGRO_KEY_ESCAPE, ALLEGRO_KEY_UP, ALLEGRO_KEY_DOWN, ALLEGRO_KEY_LEFT,
      ALLEGRO_KEY_RIGHT, ALLEGRO_KEY_ENTER, ALLEGRO_KEY_PAD_ENTER
   };
   int bar_h = ui_menu_bar_height();
   CMD_ID chosen = CMD_NONE;
   size_t k;

   s_ctx = *ctx;
   s_state.menu = -1;
   if (start_menu >= 0)
      menu_state_open(&s_state, start_menu);
   input_suppress_held();

   while (s_state.menu >= 0 && !a5_display_closed)
   {
      int title, item;

      input_pump();

      for (k = 0; k < sizeof(nav_keys) / sizeof(nav_keys[0]) && s_state.menu >= 0; k++)
         if (key_hit(nav_keys[k]))
            chosen = menu_state_key(&s_state, nav_keys[k], ctx);

      if (s_state.menu >= 0)
      {
         title = menu_title_at(a5_mouse_x, a5_mouse_y, bar_h);
         item = menu_item_at(s_state.menu, a5_mouse_x, a5_mouse_y, bar_h);

         if (title >= 0 && title != s_state.menu)
            menu_state_open(&s_state, title);
         else if (item >= 0)
            s_state.item = item;

         if (mouse_hit(1))
         {
            if (item >= 0)
            {
               CMD_ID id = commands_menu_item(s_state.menu, item);
               if (commands_is_available(id, ctx))
               {
                  chosen = id;
                  s_state.menu = -1;
               }
            }
            else
            {
               /* The title that's open, or anywhere else, closes it. */
               s_state.menu = -1;
            }
         }
      }

      misc_draw_screen(a5_mouse_x, a5_mouse_y);
      al_rest(0.01);
   }

   s_state.menu = -1;

   /* The press that closed the menu mustn't land on the map behind it. */
   input_pump();
   while ((a5_mouse_b & 1) && !a5_display_closed)
   {
      al_rest(0.01);
      input_pump();
   }
   input_suppress_held();
   return chosen;
}

CMD_ID menu_bar_poll(const CMD_CTX *ctx, int keys_enabled)
{
   static const struct { int key; int menu; } mnemonics[] = {
      { ALLEGRO_KEY_F, MENU_FILE }, { ALLEGRO_KEY_E, MENU_EDIT },
      { ALLEGRO_KEY_M, MENU_MAP },  { ALLEGRO_KEY_V, MENU_VIEW },
      { ALLEGRO_KEY_B, MENU_BUILD }
   };
   int bar_h = ui_menu_bar_height();
   size_t i;
   int key;

   if (bar_h == 0)
      return CMD_NONE;

   if (mouse_hit(1) && a5_mouse_y < bar_h)
      return run_menu(menu_title_at(a5_mouse_x, a5_mouse_y, bar_h), ctx);

   if (!keys_enabled)
      return CMD_NONE;

   if (held_mods() == CMD_MOD_ALT)
      for (i = 0; i < sizeof(mnemonics) / sizeof(mnemonics[0]); i++)
         if (key_hit(mnemonics[i].key))
            return run_menu(mnemonics[i].menu, ctx);

   for (key = 1; key < ALLEGRO_KEY_MAX; key++)
   {
      if (a5_key_hit[key])
      {
         CMD_ID id = commands_find_key(key, held_mods());
         if (id != CMD_NONE)
         {
            /* Used up: polled code mustn't see it, even as held, until it's
               pressed again. So Ctrl+N doesn't toggle night mode and Ctrl+S
               doesn't scroll. */
            a5_key_hit[key] = 0;
            a5_key_suppressed[key] = 1;
            return commands_is_available(id, ctx) ? id : CMD_NONE;
         }
      }
   }
   return CMD_NONE;
}
```

- [ ] **Step 3: Build**

Build everything.
Expected: no errors. `menu_bar_poll` is unused until Task 4.

- [ ] **Step 4: Commit**

Stage `src/ui/menu_bar.c` and `src/ui/menu_bar.h`, then:

```bash
git commit -m "Open menus and look up hotkeys from the command table"
```

---

### Task 4: Run commands and move the bindings

**Files:**
- Modify: `src/ui/interface.c`, `src/ui/project_menu.c`, `src/ui/project_menu.h`, `src/ui/preset_picker.c`, `src/ui/preset_picker.h`, `src/ui/compat.h`, `src/ui/text_input_modal.c`

**Interfaces:**
- Consumes: `menu_bar_poll`, `CMD_CTX`, `commands_get` from earlier tasks; `interface_delete_map`, `interface_confirm_quit` in `interface.c`; `map_save_ui_save`, `map_save_ui_save_dirty`; `map_any_dirty`.
- Produces: `void project_menu_close_project(void); void project_menu_export(void); void preset_picker_open(void);`

- [ ] **Step 1: Make the actions public and remove their pollers**

In `src/ui/project_menu.c`, delete `project_menu_handle_shortcuts` and the now-unused `wait_release`. Below `project_menu_open_project` add:

```c
void project_menu_close_project(void) { action_close_project(); }
void project_menu_export(void)        { action_export_unified(); }
```

In `src/ui/project_menu.h`, remove the `project_menu_handle_shortcuts` declaration and its comment, keep the note about the `Project:` indicator, and add:

```c
// Close the project, and run the asset export flow. These block while native
// file dialogs are up, as the rest of the editor's modal UI does.
void project_menu_close_project(void);
void project_menu_export(void);
```

In `src/ui/preset_picker.c`, replace these lines in `preset_picker_handle_shortcut`:

```c
void preset_picker_handle_shortcut(void)
{
   int ctrl, shift;
   PICKER_S s;
   int preset_idx;

   ctrl  = key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL);
   shift = key_pressed(KEY_LSHIFT)   || key_pressed(KEY_RSHIFT);

   if (!ctrl || !shift || !key_hit(KEY_P)) return;
   if (!mpq_index_is_ready()) return;

   /* debounce */

```

with:

```c
void preset_picker_open(void)
{
   PICKER_S s;
   int preset_idx;

   if (!mpq_index_is_ready()) return;

```

In `src/ui/preset_picker.h`, rename the declaration to `void preset_picker_open(void);`.

- [ ] **Step 2: Treat Cmd as Ctrl on macOS**

In `src/ui/compat.h`, replace `ds1_key_is_down` with:

```c
static inline bool ds1_key_is_down(int k)
{
   if (k <= 0 || k >= ALLEGRO_KEY_MAX)
      return false;
#ifdef __APPLE__
   /* Cmd works wherever Ctrl does. */
   if ((k == ALLEGRO_KEY_LCTRL || k == ALLEGRO_KEY_RCTRL) &&
       al_key_down(&a5_kb_state, ALLEGRO_KEY_COMMAND) &&
       !a5_key_suppressed[ALLEGRO_KEY_COMMAND])
      return true;
#endif
   return al_key_down(&a5_kb_state, k) && !a5_key_suppressed[k];
}
```

Add directly above it:

```c
/* The keymod that means "Ctrl" in a KEY_CHAR event. */
#ifdef __APPLE__
   #define DS1_KEYMOD_PRIMARY (ALLEGRO_KEYMOD_CTRL | ALLEGRO_KEYMOD_COMMAND)
#else
   #define DS1_KEYMOD_PRIMARY ALLEGRO_KEYMOD_CTRL
#endif
```

In `src/ui/interface.c`, replace `(event.keyboard.modifiers & ALLEGRO_KEYMOD_CTRL)` with `(event.keyboard.modifiers & DS1_KEYMOD_PRIMARY)`. In `src/ui/text_input_modal.c`, replace `(mods & ALLEGRO_KEYMOD_CTRL)` with `(mods & DS1_KEYMOD_PRIMARY)`.

- [ ] **Step 3: Add the command functions in interface.c**

In `src/ui/interface.c`, add `#include "ui/commands.h"` and `#include "ui/menu_bar.h"`. Change the declaration at the top of `interfac_user_handler` so `cx` and `cy` start at 0:

```c
   int ds1_idx, done, cx = 0, cy = 0, n, i, dx, dy, old_ds1_idx = 0;
```

Add above `interfac_user_handler`:

```c
/* Start a copy or cut: the selection follows the mouse until a click pastes it. */
static void interface_begin_paste(int ds1_idx, int cx, int cy, PASTE_POS_S *paste, int is_cut)
{
   int i;

   if (paste->start != FALSE)
      return;
   for (i = 0; i < DS1_MAX; i++)
   {
      if (strlen(glb_ds1[i].name))
         edittile_paste_prepare(i);
   }
   paste->src_ds1_idx = ds1_idx;
   paste->old_ds1_idx = ds1_idx;
   paste->start = TRUE;
   paste->is_cut = is_cut;
   paste->cut_saved = FALSE;
   paste->old_x = cx;
   paste->old_y = cy;
   edittile_middle_select(ds1_idx, &paste->start_x, &paste->start_y);
   edittile_paste_preview(ds1_idx, cx - paste->start_x, cy - paste->start_y, paste);
}

/* Create a map beside the open one, or clone it. The browser's selected row
 * follows the mouse, so these act on the open map's row. */
static void interface_new_map(int ds1_idx, int clone)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   int gi = ab->loaded_group >= 0 ? ab->loaded_group : ab->selected_group;
   int ei = ab->loaded_group >= 0 ? ab->loaded_entry : ab->selected_entry;
   AREA_GROUP_S *g;

   if (gi < 0 || gi >= ab->group_count)
      return;
   g = &ab->groups[gi];
   if (g->entry_count <= 0)
      return;

   if (clone)
   {
      if (glb_ds1edit.has_loaded_ds1 && ds1_manager_clone(ds1_idx, gi, ei) == 0)
         printf("Map: cloned DS1 successfully\n");
   }
   else
   {
      int new_act = g->act > 0 ? g->act : 1;
      int new_w = 20, new_h = 20;

      /* Copy dimensions from the open DS1 if there is one */
      if (glb_ds1edit.has_loaded_ds1 && ds1_idx >= 0 && ds1_idx < DS1_MAX &&
          glb_ds1[ds1_idx].name[0] != '\0')
      {
         new_w = (int)glb_ds1[ds1_idx].width;
         new_h = (int)glb_ds1[ds1_idx].height;
         new_act = (int)glb_ds1[ds1_idx].act;
      }
      if (ds1_manager_create_empty(gi, ei, new_w, new_h, new_act) == 0)
         printf("Map: created new empty DS1 (%dx%d)\n", new_w, new_h);
   }
   fflush(stdout);
}

static CMD_CTX interface_command_context(int ds1_idx, const PASTE_POS_S *paste)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   CMD_CTX c;

   memset(&c, 0, sizeof(c));
   c.tile_mode = glb_ds1edit.mode == MOD_T;
   c.object_mode = glb_ds1edit.mode == MOD_O;
   c.has_map = glb_ds1edit.has_loaded_ds1 && glb_ds1[ds1_idx].name[0] != 0;
   c.has_open_entry = c.has_map && ab->loaded_group >= 0 && ab->loaded_entry >= 0 &&
                      ab->loaded_group < ab->group_count &&
                      ab->loaded_entry < ab->groups[ab->loaded_group].entry_count;
   c.any_dirty = map_any_dirty();
   c.paste_active = paste->start != FALSE;
   c.can_undo_object = c.has_map && glb_ds1[ds1_idx].can_undo_obj != FALSE;
   return c;
}

/* Run a command chosen from a menu or a hotkey. Commands the polled code owns
 * mark their key as hit, so it runs them later in this frame. */
static void interface_run_command(CMD_ID cmd, int ds1_idx, int cx, int cy,
                                  PASTE_POS_S *paste, int *done)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   const CMD_DEF *def = commands_get(cmd);

   if (def != NULL && def->inject_key != 0)
   {
      a5_key_hit[def->inject_key] = 1;
      return;
   }

   switch (cmd)
   {
   case CMD_NEW_PROJECT:   project_menu_new_project();   break;
   case CMD_OPEN_PROJECT:  project_menu_open_project();  break;
   case CMD_CLOSE_PROJECT: project_menu_close_project(); break;
   case CMD_OPEN_PRESET:   preset_picker_open();         break;
   case CMD_EXPORT_ASSETS: project_menu_export();        break;

   case CMD_SAVE:
      /* The "saved" message only after a save that actually happened. A window
         close while it's up stays pending for the quit check. */
      if (map_save_ui_save(ds1_idx, FALSE) == 0)
         msg_save_main();
      break;

   case CMD_SAVE_ALL:
      if (map_save_ui_save_dirty() == 0)
         msg_save_main();
      break;

   case CMD_QUIT:
      *done = interface_confirm_quit();
      break;

   case CMD_UNDO:
      if (glb_ds1edit.mode == MOD_T)
         undo_apply_tile_buffer(ds1_idx);
      else if (glb_ds1edit.mode == MOD_O)
         editobj_undo(ds1_idx);
      break;

   case CMD_CUT:  interface_begin_paste(ds1_idx, cx, cy, paste, TRUE);  break;
   case CMD_COPY: interface_begin_paste(ds1_idx, cx, cy, paste, FALSE); break;

   case CMD_MAP_NEW:   interface_new_map(ds1_idx, FALSE); break;
   case CMD_MAP_CLONE: interface_new_map(ds1_idx, TRUE);  break;

   case CMD_MAP_DELETE:
      interface_delete_map(ab->loaded_group, ab->loaded_entry);
      break;

   default:
      break;
   }
}
```

Place `interface_begin_paste` and the rest after `interface_confirm_quit` and `interface_delete_map`.

- [ ] **Step 4: Dispatch once a frame**

In `interfac_user_handler`, replace:

```c
      /* Project workflow: Ctrl+Shift+{N,O,W} */
      project_menu_handle_shortcuts();
      if (!glb_ds1edit.has_loaded_ds1)
         ds1_idx = 0;  /* closing a project can discard the open maps */

      /* Preset picker: Ctrl+Shift+P */
      preset_picker_handle_shortcut();
```

with:

```c
      /* Menus and table-driven hotkeys. This runs before the polled keys below,
         so a command that marks a key as hit reaches them this frame. */
      {
         CMD_CTX cmd_ctx = interface_command_context(ds1_idx, &paste_pos);
         CMD_ID cmd = menu_bar_poll(&cmd_ctx, !glb_ds1edit.props_panel.editing);

         if (cmd != CMD_NONE)
            interface_run_command(cmd, ds1_idx, cx, cy, &paste_pos, &done);
      }
      if (!glb_ds1edit.has_loaded_ds1)
         ds1_idx = 0;  /* closing a project can discard the open maps */
```

- [ ] **Step 5: Remove the bindings that moved**

In `interfac_user_handler`, delete these whole blocks:

- The block under `// Insert key — create new DS1 or clone current`, since `Insert` returns to its object-mode role in `src/editor/objects.c`.
- The `// S` block's two branches for `Ctrl+Shift+S` and `Ctrl+S`, starting at `if (key_hit(KEY_S) && (key_pressed(KEY_LCONTROL)` and ending before the `Shift+S` unhide check. Keep the unhide check. Then make it a plain `if`.

Replace the `// CTRL + C` body:

```c
               // CTRL + C : copy selected layers (copy / paste)
               if (paste_pos.start == FALSE)
               {
                  for (i = 0; i < DS1_MAX; i++)
                  {
                     if (strlen(glb_ds1[i].name))
                        edittile_paste_prepare(i);
                  }
                  paste_pos.src_ds1_idx = ds1_idx;
                  paste_pos.old_ds1_idx = ds1_idx;
                  paste_pos.start = TRUE;
                  paste_pos.is_cut = FALSE; // just a 'COPY'
                  paste_pos.cut_saved = FALSE;
                  paste_pos.old_x = cx;
                  paste_pos.old_y = cy;
                  edittile_middle_select(
                      ds1_idx,
                      &paste_pos.start_x,
                      &paste_pos.start_y);
                  edittile_paste_preview(ds1_idx,
                                         cx - paste_pos.start_x,
                                         cy - paste_pos.start_y,
                                         &paste_pos);

               }
```

with:

```c
               // CTRL + C : copy selected layers (copy / paste)
               interface_begin_paste(ds1_idx, cx, cy, &paste_pos, FALSE);
```

Replace the `// CTRL + X` block's inner body:

```c
            // TILE mode
            if (paste_pos.start == FALSE)
            {
               for (i = 0; i < DS1_MAX; i++)
               {
                  if (strlen(glb_ds1[i].name))
                     edittile_paste_prepare(i);
               }
               paste_pos.src_ds1_idx = ds1_idx;
               paste_pos.old_ds1_idx = ds1_idx;
               paste_pos.start = TRUE;
               paste_pos.is_cut = TRUE; // copy with 'CUT'
               paste_pos.cut_saved = FALSE;
               paste_pos.old_x = cx;
               paste_pos.old_y = cy;
               edittile_middle_select(
                   ds1_idx,
                   &paste_pos.start_x,
                   &paste_pos.start_y);
               edittile_paste_preview(ds1_idx,
                                      cx - paste_pos.start_x,
                                      cy - paste_pos.start_y,
                                      &paste_pos);
            }
```

with:

```c
            // TILE mode
            interface_begin_paste(ds1_idx, cx, cy, &paste_pos, TRUE);
```

Remove any local variable this leaves unused, such as `ret`, only if nothing else in the function uses it.

- [ ] **Step 6: Build and test**

Build everything and run the portable label: `ctest --test-dir tmp/build-lifecycle -C Release -L portable -j 4 --timeout 300`. Rerun anything that times out alone. `test/python/test_delete_key_routing.py` checks the Delete routing in `interface.c`; if it asserts on text this task moved, update it to the new location while keeping what it proves: plain `Del` never reaches a map delete.
Expected: PASS.

- [ ] **Step 7: Commit**

Stage the files changed in this task by path, then:

```bash
git commit -m "Run lifecycle commands from the table and move their hotkeys"
```

---

### Task 5: Docs and verification

**Files:**
- Modify: `README.md`, `docs/getting-started/manual.md`, `docs/testing/native-release-checklist.md`, `CHANGELOG.md`

- [ ] **Step 1: Update the README controls table**

In `README.md`, replace the rows for `Right-click a map in the area browser`, `Ctrl+Shift+S` and `Ctrl+Shift+N / O / W` with:

```markdown
| Right-click a map in the area browser, or **Ctrl+Del** | **Delete map...** (**Ctrl+Del** acts on the open map): asks first, then backs the map up and removes it from the project or mod folder |
| **Ctrl+Shift+S** | Save every map with unsaved edits |
| **Ctrl+Shift+N / O / W** | New, Open or Close Project |
| **Ctrl+N** / **Ctrl+D** | New map / clone the open map |
| **Ctrl+Shift+P** / **Ctrl+Shift+A** | Open a preset / export assets |
| **Alt+F, E, M, V, B** | Open the File, Edit, Map, View or Build menu. Arrow keys move, **Enter** runs and **Esc** closes |
```

Add below the table:

```markdown
On macOS, **Cmd** works wherever **Ctrl** does. Dimmed menu commands arrive in later releases.
```

- [ ] **Step 2: Update the manual**

In `docs/getting-started/manual.md`, find any statement that `Insert` or `Shift+Insert` creates or clones a map with `grep -n "Insert" docs/getting-started/manual.md`, and change it to `Ctrl+N` and `Ctrl+D`, leaving text about inserting objects in object mode unchanged. Add a short section beside the keyboard reference:

```markdown
### Menus

The bar across the top holds **File**, **Edit**, **Map**, **View** and **Build**. Click a title, or press **Alt** with its first letter. Arrow keys move, **Enter** runs the highlighted command, and **Esc** closes the menu. Every command shows its hotkey, and commands that don't apply right now are dimmed.
```

- [ ] **Step 3: Update the checklist and changelog**

In `docs/testing/native-release-checklist.md`, add after **E14**, and change `Total items: 14.` to `Total items: 15.`, the `E1 to E14` heading to `E1 to E15`, and the recording example to `E1-E15` and `--items 15 --total 15`:

```markdown
- [ ] **E15 Menus and hotkeys.** Click each menu title, and open each with **Alt**
  plus its letter. Arrow keys move and wrap, **Enter** runs, and **Esc** closes the
  menu without reaching the quit dialog. While a menu is open, the map behind it
  ignores keys and clicks, and a click that closes the menu doesn't touch the map.
  Dimmed rows do nothing. **Ctrl+N** doesn't toggle night mode, **Ctrl+S** doesn't
  scroll, and **Insert** in tile mode creates no map. Side panels and the HUD sit
  below the bar with no overlap. On macOS, repeat with **Cmd**.
```

In `CHANGELOG.md`, in the section for the next unreleased version, add under `### Added`, creating that heading if the section has none:

```markdown
- **Menu bar.** File, Edit, Map, View and Build menus, opened with a click or **Alt** plus the first letter. Every command shows its hotkey. Build commands and some Map commands are dimmed until they arrive.

- **Hotkeys for maps.** **Ctrl+N** creates a map, **Ctrl+D** clones the open one, and **Ctrl+Del** deletes the open one after asking. These replace **Insert** and **Shift+Insert**, and **Insert** keeps its object-mode role.
```

Add under `### Fixed`:

```markdown
- **Hotkeys match their exact modifiers.** **Ctrl+N** no longer also toggles night mode, **Ctrl+Shift+N** no longer does either, and **Ctrl+S** no longer scrolls the map.

- **Closing the window during the "saved" message no longer quits without asking.**
```

- [ ] **Step 4: Run the full tests**

Run: `ctest --test-dir tmp/build-lifecycle -C Release -L portable -j 4 --timeout 300`. Rerun anything that times out alone.
Expected: PASS.

- [ ] **Step 5: Check by hand, windowed**

Use a scratch project and an INI with `full_screen = NO`. Confirm each of these on Linux through WSLg, and on Windows:

1. All five menus open by click and by `Alt` plus the letter, with hotkeys shown and later-step rows dimmed.
2. `Esc` closes an open menu and the editor stays open. A second `Esc` with nothing unsaved quits.
3. With a menu open, the map doesn't scroll or change, and a click outside only closes the menu.
4. View rows toggle the same things as their keys. Edit's Undo, Cut, Copy and Delete Selection work.
5. File's Save, Save All, New Project, Open Project and Close Project run the existing flows.
6. `Ctrl+N` doesn't change night mode. `Ctrl+S` doesn't scroll. `Insert` in tile mode does nothing, and in object mode it still inserts an object.
7. With the mouse over another browser row, `Ctrl+Del` names the open map, not that row. Choose No.
8. Side panels, their click targets and the HUD line up below the bar with one and two HUD rows (`~` toggles the second).
9. No file changes under the game folder.

- [ ] **Step 6: Run the golden check if a game install is configured**

Run `ctest --test-dir tmp/build-lifecycle -C Release -R "^golden_core$" --output-on-failure` if that test is registered in this build.
Expected: PASS, showing headless output is unchanged. If it isn't registered, say so in the report.

- [ ] **Step 7: Commit**

```bash
git add README.md docs/getting-started/manual.md docs/testing/native-release-checklist.md CHANGELOG.md
git commit -m "Document the menu bar and new map hotkeys"
```
