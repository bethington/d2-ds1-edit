/* lvlprest_find_file_slot: map deletion asks the raw table it is about to
 * rewrite where the map is. "Not listed" lets a delete go ahead with no slot to
 * clear; "can't tell" must stop it before the file goes, or the table keeps
 * naming a map that no longer exists.
 */

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

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_finds_the_slot_by_file_name_in_any_case_and_separator);
   RUN_TEST(test_finds_the_last_slot_on_a_last_line_with_no_newline);
   RUN_TEST(test_a_map_the_table_does_not_list_has_no_slot_to_clear);
   RUN_TEST(test_a_table_without_the_expected_columns_is_an_error_not_a_miss);
   return UNITY_END();
}
