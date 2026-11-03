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

void test_lists_path_wants_the_whole_path_in_this_row(void)
{
   TEST_ASSERT_TRUE(lvlprest_lists_path(table, 1, "act1/town/townn1.DS1"));
   TEST_ASSERT_TRUE(lvlprest_lists_path(table, 167, "Act1\\Barracks\\CourtWb.ds1"));
   TEST_ASSERT_TRUE(lvlprest_lists_path(table, 200, "Act2/Last.ds1"));
   /* The same name in another folder, another row, or no row at all. */
   TEST_ASSERT_FALSE(lvlprest_lists_path(table, 1, "Act2/Town/TownN1.ds1"));
   TEST_ASSERT_FALSE(lvlprest_lists_path(table, 167, "Act1/Town/TownN1.ds1"));
   TEST_ASSERT_FALSE(lvlprest_lists_path(table, 999, "Act1/Town/TownN1.ds1"));
}

void test_slot_holds_path_looks_at_one_cell(void)
{
   TEST_ASSERT_TRUE(lvlprest_slot_holds_path(table, 1, 2, "act1/town/TOWNE1.ds1"));
   TEST_ASSERT_FALSE(lvlprest_slot_holds_path(table, 1, 1, "act1/town/TOWNE1.ds1"));
   TEST_ASSERT_FALSE(lvlprest_slot_holds_path(table, 1, 2, "x/TOWNE1.ds1"));
   TEST_ASSERT_FALSE(lvlprest_slot_holds_path(table, 999, 1, "Act1/Town/TownN1.ds1"));
}

void test_find_path_slot_says_when_the_table_is_unusable(void)
{
   char empty[] = "";

   TEST_ASSERT_EQUAL_INT(2, lvlprest_find_path_slot(table, 1, "Act1/Town/townE1.ds1"));
   TEST_ASSERT_EQUAL_INT(0, lvlprest_find_path_slot(table, 1, "x/townE1.ds1"));
   TEST_ASSERT_EQUAL_INT(LVLPREST_TABLE_UNUSABLE, lvlprest_find_path_slot(empty, 1, "a.ds1"));
}

typedef struct { int count; int last_def; char last[64]; } EACH_SEEN;

static void note_each(int def, const char * path, void * ctx)
{
   EACH_SEEN * seen = (EACH_SEEN *) ctx;

   seen->count++;
   seen->last_def = def;
   snprintf(seen->last, sizeof(seen->last), "%s", path);
}

void test_each_cleared_tells_namesakes_apart(void)
{
   char game[] = "Name\tDef\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "Bar\t50\tA\\Bar.ds1\tB\\Bar.ds1\t0\t0\t0\t0\r\n";
   char project[] = "Name\tDef\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                    "Bar\t50\t0\tb/bar.DS1\t0\t0\t0\t0\r\n";
   EACH_SEEN seen;

   memset(&seen, 0, sizeof(seen));
   lvlprest_each_cleared(game, project, note_each, &seen);
   TEST_ASSERT_EQUAL_INT(1, seen.count);
   TEST_ASSERT_EQUAL_STRING("A\\Bar.ds1", seen.last);
}

void test_each_file_visits_every_listed_cell(void)
{
   EACH_SEEN seen;

   memset(&seen, 0, sizeof(seen));
   lvlprest_each_file(table, note_each, &seen);
   TEST_ASSERT_EQUAL_INT(4, seen.count);
   TEST_ASSERT_EQUAL_INT(200, seen.last_def);
   TEST_ASSERT_EQUAL_STRING("Act2\\Last.ds1", seen.last);
}

static void note_def(int def, void * ctx)
{
   char * log = (char *) ctx;

   sprintf(log + strlen(log), "%d;", def);
}

void test_each_def_visits_every_row_even_one_with_no_files(void)
{
   char t[] = "Name\tDef\tFile1\tFile2\r\n"
              "A\t1\tx.ds1\t0\r\n"
              "Empty\t2\t0\t\r\n"
              "Bare\t3\t\t\r\n"
              "C\t40\ty.ds1\t0";
   char none[] = "Name\tFile1\r\nA\tx.ds1\r\n";
   char log[100] = "";

   /* each_file reports no cell of the all-empty rows 2 and 3 */
   lvlprest_each_def(t, note_def, log);
   TEST_ASSERT_EQUAL_STRING("1;2;3;40;", log);
   log[0] = '\0';
   lvlprest_each_def(none, note_def, log);
   TEST_ASSERT_EQUAL_STRING("", log);
}

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

void test_inserting_refuses_a_value_that_would_break_the_row(void)
{
   long len = 0;
   TEST_ASSERT_NULL(lvlprest_with_map_inserted(rows, 1, 1, "", &len));
   TEST_ASSERT_NULL(lvlprest_with_map_inserted(rows, 1, 1, "0", &len));
   TEST_ASSERT_NULL(lvlprest_with_map_inserted(rows, 1, 1, "a\tb.ds1", &len));
   TEST_ASSERT_NULL(lvlprest_with_map_inserted(rows, 1, 1, "a\nb.ds1", &len));
}

void test_a_hole_is_a_blank_or_zero_cell_below_files(void)
{
   char holes[] =
      "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
      "Zero\t1\t3\ta.ds1\t0\tc.ds1\t0\t0\t0\r\n"
      "Blank\t2\t2\t\tb.ds1\t0\t0\t0\t0\r\n"
      "Past\t3\t1\ta.ds1\t0\tc.ds1\t0\t0\t0\r\n"
      "Own\t4\t0\t0\t0\t0\t0\t0\t0\r\n"
      "Over\t5\t9\ta.ds1\tb.ds1\tc.ds1\td.ds1\te.ds1\tf.ds1\r\n"
      "Short\t6\t2\ta.ds1\r\n";

   TEST_ASSERT_EQUAL_INT(2, lvlprest_files_hole(holes, 1));
   TEST_ASSERT_EQUAL_INT(1, lvlprest_files_hole(holes, 2));
   TEST_ASSERT_EQUAL_INT(0, lvlprest_files_hole(holes, 3));   /* past Files is no hole */
   TEST_ASSERT_EQUAL_INT(0, lvlprest_files_hole(holes, 4));   /* Files 0 counts nothing */
   TEST_ASSERT_EQUAL_INT(0, lvlprest_files_hole(holes, 5));
   TEST_ASSERT_EQUAL_INT(2, lvlprest_files_hole(holes, 6));   /* a row cut short */
   TEST_ASSERT_EQUAL_INT(0, lvlprest_files_hole(holes, 99));
}

void test_a_row_has_a_size_unless_both_are_zero(void)
{
   char sized[] =
      "Name\tDef\tSizeX\tSizeY\tFiles\r\n"
      "Room\t168\t10\t14\t4\r\n"
      "Tri\t300\t0\t0\t1\r\n"
      "Wide\t4\t8\t0\t1\r\n"
      "Tall\t5\t0\t8\t1\r\n"
      "Blank\t6\t\t\t1\r\n";
   int x = -1, y = -1;

   TEST_ASSERT_TRUE(lvlprest_row_has_size(sized, 168));
   TEST_ASSERT_FALSE(lvlprest_row_has_size(sized, 300));
   TEST_ASSERT_TRUE(lvlprest_row_has_size(sized, 4));
   TEST_ASSERT_TRUE(lvlprest_row_has_size(sized, 5));
   TEST_ASSERT_FALSE(lvlprest_row_has_size(sized, 6));

   TEST_ASSERT_TRUE(lvlprest_row_size(sized, 168, &x, &y));
   TEST_ASSERT_EQUAL_INT(10, x);
   TEST_ASSERT_EQUAL_INT(14, y);
   TEST_ASSERT_FALSE(lvlprest_row_size(sized, 99, &x, &y));

   /* A table that doesn't say isn't saying 0. */
   TEST_ASSERT_TRUE(lvlprest_row_has_size(table, 1));
   TEST_ASSERT_FALSE(lvlprest_row_size(table, 1, &x, &y));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_get_cell_copies_one_cell);
   RUN_TEST(test_with_cell_changes_only_that_cell);
   RUN_TEST(test_with_cell_replaces_the_last_cell_of_a_file_with_no_newline);
   RUN_TEST(test_each_cleared_lists_game_files_the_project_row_lacks);
   RUN_TEST(test_finds_the_slot_by_file_name_in_any_case_and_separator);
   RUN_TEST(test_finds_the_last_slot_on_a_last_line_with_no_newline);
   RUN_TEST(test_a_map_the_table_does_not_list_has_no_slot_to_clear);
   RUN_TEST(test_a_table_without_the_expected_columns_is_an_error_not_a_miss);
   RUN_TEST(test_lists_path_wants_the_whole_path_in_this_row);
   RUN_TEST(test_slot_holds_path_looks_at_one_cell);
   RUN_TEST(test_find_path_slot_says_when_the_table_is_unusable);
   RUN_TEST(test_each_file_visits_every_listed_cell);
   RUN_TEST(test_each_def_visits_every_row_even_one_with_no_files);
   RUN_TEST(test_each_cleared_tells_namesakes_apart);
   RUN_TEST(test_files_value_reads_the_row);
   RUN_TEST(test_a_simple_row_counts_exactly_its_filled_cells);
   RUN_TEST(test_a_table_with_no_files_column_has_no_simple_rows);
   RUN_TEST(test_inserting_at_the_end_raises_files);
   RUN_TEST(test_inserting_in_the_middle_moves_later_cells_down);
   RUN_TEST(test_inserting_past_the_end_lands_at_the_end);
   RUN_TEST(test_inserting_is_refused_on_full_and_unusual_rows);
   RUN_TEST(test_removing_moves_later_cells_up_and_lowers_files);
   RUN_TEST(test_removing_is_refused_for_an_only_map_and_unusual_rows);
   RUN_TEST(test_remove_then_insert_at_the_same_slot_is_byte_identical);
   RUN_TEST(test_inserting_refuses_a_value_that_would_break_the_row);
   RUN_TEST(test_a_hole_is_a_blank_or_zero_cell_below_files);
   RUN_TEST(test_a_row_has_a_size_unless_both_are_zero);
   return UNITY_END();
}
