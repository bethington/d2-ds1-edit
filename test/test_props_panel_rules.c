/* Which row the properties panel works on, and which DS1 header fields it edits.
 *
 * With BarW5 open, hovering BarNSEW3 in the area browser showed BarNSEW3's LvlPrest
 * row, and Apply edited it. The cases below are that one and its relatives.
 */

#include <string.h>
#include "unity/unity.h"
#include "ui/props_panel_rules.h"

void setUp(void) {}
void tearDown(void) {}

static AREA_DS1_ENTRY_S s_rows[3];
static AREA_GROUP_S s_groups[2];
static AREA_BROWSER_S s_ab;

/* BarW5 (Def 168) is open and is row 1 of group 0, as in the report. */
static void open_barw5(void)
{
   memset(s_rows, 0, sizeof(s_rows));
   memset(s_groups, 0, sizeof(s_groups));
   memset(&s_ab, 0, sizeof(s_ab));
   s_rows[0].lvlprest_def = 182;  /* BarNSEW3 */
   s_rows[1].lvlprest_def = 168;  /* BarW5 */
   s_rows[2].lvlprest_def = 900;  /* a row of group 1 */
   s_groups[0].entries = &s_rows[0];
   s_groups[0].entry_count = 2;
   s_groups[1].entries = &s_rows[2];
   s_groups[1].entry_count = 1;
   s_ab.groups = s_groups;
   s_ab.group_count = 2;
   s_ab.loaded_group = 0;
   s_ab.loaded_entry = 1;
   s_ab.selected_group = 0;
   s_ab.selected_entry = 1;
}

static void test_shows_the_open_map(void)
{
   open_barw5();
   TEST_ASSERT_EQUAL_INT(168, props_panel_open_row(&s_ab)->lvlprest_def);
}

static void test_hover_over_another_row_changes_nothing(void)
{
   open_barw5();
   s_ab.selected_entry = 0;
   TEST_ASSERT_EQUAL_INT(168, props_panel_open_row(&s_ab)->lvlprest_def);
}

/* selected_entry was read against loaded_group, so a row of one group picked a
   row of another. */
static void test_hover_in_another_group_changes_nothing(void)
{
   open_barw5();
   s_ab.selected_group = 1;
   s_ab.selected_entry = 0;
   TEST_ASSERT_EQUAL_INT(168, props_panel_open_row(&s_ab)->lvlprest_def);
}

/* Restore from Backup and Revert open the map directly, leaving the highlight
   wherever it was or at -1, which used to mean the group's first row. */
static void test_a_map_opened_without_the_highlight_shows_its_own_row(void)
{
   open_barw5();
   s_ab.selected_entry = -1;
   TEST_ASSERT_EQUAL_INT(168, props_panel_open_row(&s_ab)->lvlprest_def);
}

static void test_no_open_row_is_no_row_not_the_first(void)
{
   open_barw5();
   s_ab.loaded_entry = -1;
   TEST_ASSERT_NULL(props_panel_open_row(&s_ab));

   open_barw5();
   s_ab.loaded_group = -1;
   TEST_ASSERT_NULL(props_panel_open_row(&s_ab));
}

static void test_an_open_row_out_of_range_is_no_row(void)
{
   open_barw5();
   s_ab.loaded_entry = 2;
   TEST_ASSERT_NULL(props_panel_open_row(&s_ab));

   open_barw5();
   s_ab.loaded_group = 2;
   TEST_ASSERT_NULL(props_panel_open_row(&s_ab));
}

static void test_header_fields_that_can_be_edited(void)
{
   TEST_ASSERT_FALSE(props_panel_header_field_read_only(1));  /* version */
   TEST_ASSERT_FALSE(props_panel_header_field_read_only(4));  /* act */
   TEST_ASSERT_FALSE(props_panel_header_field_read_only(6));  /* tag_type */
}

static void test_header_fields_that_cannot_be_edited(void)
{
   static const int read_only[] = {0, 2, 3, 5, 7, 8, 9, 10, 11};
   size_t i;

   for (i = 0; i < sizeof(read_only) / sizeof(read_only[0]); i++)
      TEST_ASSERT_TRUE(props_panel_header_field_read_only(read_only[i]));
}

/* Changing a row's own key in the panel: the browser's rows still hold the old
   value, so the panel found no table row until a restart. */
static void test_a_def_edit_moves_the_rows_with_that_def(void)
{
   open_barw5();
   s_rows[2].lvlprest_def = 168;  /* a second row of the same Def */
   props_panel_follow_key_edit(&s_ab, PFS_LVLPREST, "Def", 168, 170);
   TEST_ASSERT_EQUAL_INT(170, props_panel_open_row(&s_ab)->lvlprest_def);
   TEST_ASSERT_EQUAL_INT(170, s_rows[2].lvlprest_def);
   TEST_ASSERT_EQUAL_INT(182, s_rows[0].lvlprest_def);
}

static void test_a_lvltype_id_edit_moves_its_rows_and_group(void)
{
   open_barw5();
   s_groups[0].lvltype_id = 4;
   s_rows[0].lvltype_id = 4;
   s_rows[1].lvltype_id = 4;
   s_groups[1].lvltype_id = 9;
   s_rows[2].lvltype_id = 9;
   props_panel_follow_key_edit(&s_ab, PFS_LVLTYPES, "Id", 4, 5);
   TEST_ASSERT_EQUAL_INT(5, s_groups[0].lvltype_id);
   TEST_ASSERT_EQUAL_INT(5, s_rows[0].lvltype_id);
   TEST_ASSERT_EQUAL_INT(5, s_rows[1].lvltype_id);
   TEST_ASSERT_EQUAL_INT(9, s_groups[1].lvltype_id);
   TEST_ASSERT_EQUAL_INT(9, s_rows[2].lvltype_id);
}

/* Levels.txt's Id, and any other column, aren't what the rows hold. */
static void test_other_columns_move_nothing(void)
{
   open_barw5();
   s_rows[1].lvltype_id = 168;
   props_panel_follow_key_edit(&s_ab, PFS_LVLPREST, "Name", 168, 170);
   props_panel_follow_key_edit(&s_ab, PFS_LVLPREST, "LevelId", 168, 170);
   props_panel_follow_key_edit(&s_ab, PFS_LEVELS, "Id", 168, 170);
   TEST_ASSERT_EQUAL_INT(168, s_rows[1].lvlprest_def);
   TEST_ASSERT_EQUAL_INT(168, s_rows[1].lvltype_id);
}

void test_a_scroll_past_the_end_stops_with_the_last_rows_showing(void)
{
   TEST_ASSERT_EQUAL_INT(70, pp_scroll_clamp(120, 100, 30));
   TEST_ASSERT_EQUAL_INT(70, pp_scroll_clamp(70, 100, 30));
   TEST_ASSERT_EQUAL_INT(69, pp_scroll_clamp(69, 100, 30));
}

void test_a_list_that_fits_or_a_negative_offset_stays_at_the_top(void)
{
   TEST_ASSERT_EQUAL_INT(0, pp_scroll_clamp(5, 20, 30));
   TEST_ASSERT_EQUAL_INT(0, pp_scroll_clamp(-3, 100, 30));
   TEST_ASSERT_EQUAL_INT(0, pp_scroll_clamp(0, 0, 0));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_shows_the_open_map);
   RUN_TEST(test_hover_over_another_row_changes_nothing);
   RUN_TEST(test_hover_in_another_group_changes_nothing);
   RUN_TEST(test_a_map_opened_without_the_highlight_shows_its_own_row);
   RUN_TEST(test_no_open_row_is_no_row_not_the_first);
   RUN_TEST(test_an_open_row_out_of_range_is_no_row);
   RUN_TEST(test_header_fields_that_can_be_edited);
   RUN_TEST(test_header_fields_that_cannot_be_edited);
   RUN_TEST(test_a_def_edit_moves_the_rows_with_that_def);
   RUN_TEST(test_a_lvltype_id_edit_moves_its_rows_and_group);
   RUN_TEST(test_other_columns_move_nothing);
   RUN_TEST(test_a_scroll_past_the_end_stops_with_the_last_rows_showing);
   RUN_TEST(test_a_list_that_fits_or_a_negative_offset_stays_at_the_top);
   return UNITY_END();
}
