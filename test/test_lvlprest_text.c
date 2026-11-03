/* lvlprest_find_file_slot: map deletion asks the raw table it is about to
 * rewrite where the map is. "Not listed" lets a delete go ahead with no slot to
 * clear; "can't tell" must stop it before the file goes, or the table keeps
 * naming a map that no longer exists.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity/unity.h"

#include "core/lvlprest_text.h"

static char table[] =
   "Name\tDef\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
   "Town\t1\tAct1\\Town\\TownN1.ds1\tAct1\\Town\\townE1.ds1\t0\t0\t0\t0\r\n"
   "Court\t167\tAct1/Barracks/CourtWb.ds1\t0\t0\t0\t0\t0\r\n"
   "Last\t200\t0\t0\t0\t0\t0\tAct2\\Last.ds1";

void setUp(void)
{
}

void tearDown(void)
{
}

void test_finds_the_slot_by_file_name_in_any_case_and_separator(void)
{
   TEST_ASSERT_EQUAL_INT(1, lvlprest_find_file_slot(table, 1, "Act1/Town/townn1.ds1"));
   TEST_ASSERT_EQUAL_INT(2, lvlprest_find_file_slot(table, 1, "Act1\\Town\\TownE1.DS1"));
   TEST_ASSERT_EQUAL_INT(1, lvlprest_find_file_slot(table, 167, "Act1\\Barracks\\CourtWb.ds1"));
}

void test_finds_the_last_slot_on_a_last_line_with_no_newline(void)
{
   TEST_ASSERT_EQUAL_INT(6, lvlprest_find_file_slot(table, 200, "Act2/Last.ds1"));
}

void test_a_map_the_table_does_not_list_has_no_slot_to_clear(void)
{
   TEST_ASSERT_EQUAL_INT(0, lvlprest_find_file_slot(table, 1, "Act1/Town/Other.ds1"));
   TEST_ASSERT_EQUAL_INT(0, lvlprest_find_file_slot(table, 999, "Act1/Town/TownN1.ds1"));
   /* Another row naming the file doesn't make it this row's. */
   TEST_ASSERT_EQUAL_INT(0, lvlprest_find_file_slot(table, 167, "Act1/Town/TownN1.ds1"));
}

void test_a_table_without_the_expected_columns_is_an_error_not_a_miss(void)
{
   char empty[] = "";
   char no_def[] = "Name\tFile1\r\nTown\tTownN1.ds1\r\n";
   char no_files[] = "Name\tDef\r\nTown\t1\r\n";

   TEST_ASSERT_EQUAL_INT(LVLPREST_TABLE_UNUSABLE,
                         lvlprest_find_file_slot(empty, 1, "TownN1.ds1"));
   TEST_ASSERT_EQUAL_INT(LVLPREST_TABLE_UNUSABLE,
                         lvlprest_find_file_slot(no_def, 1, "TownN1.ds1"));
   TEST_ASSERT_EQUAL_INT(LVLPREST_TABLE_UNUSABLE,
                         lvlprest_find_file_slot(no_files, 1, "TownN1.ds1"));
}

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

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_empty_slot_is_the_first_blank_or_zero_cell);
   RUN_TEST(test_empty_slot_is_none_when_all_six_are_used);
   RUN_TEST(test_get_cell_copies_one_cell);
   RUN_TEST(test_with_cell_changes_only_that_cell);
   RUN_TEST(test_with_cell_replaces_the_last_cell_of_a_file_with_no_newline);
   RUN_TEST(test_each_cleared_lists_game_files_the_project_row_lacks);
   RUN_TEST(test_finds_the_slot_by_file_name_in_any_case_and_separator);
   RUN_TEST(test_finds_the_last_slot_on_a_last_line_with_no_newline);
   RUN_TEST(test_a_map_the_table_does_not_list_has_no_slot_to_clear);
   RUN_TEST(test_a_table_without_the_expected_columns_is_an_error_not_a_miss);
   return UNITY_END();
}
