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
