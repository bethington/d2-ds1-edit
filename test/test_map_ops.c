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

/* test_tmp.h's names for the brief's test_tmp_dir, test_tmp_remove and
 * test_tmp_mkdir. tt_root_create leaves a trailing separator; drop it. */
static int test_tmp_dir(char *out, size_t cap)
{
   size_t n;

   if (!tt_root_create("map_ops", out, cap)) return 0;
   n = strlen(out);
   if (n > 0 && (out[n - 1] == '/' || out[n - 1] == '\\')) out[n - 1] = 0;
   return 1;
}
#define test_tmp_remove tt_remove_tree
#define test_tmp_mkdir  tt_mkdir

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
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
}

void test_unregistering_an_unlisted_map_leaves_no_table(void)
{
   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, GAME, 1, "Nope.ds1", err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
}

void test_overwriting_the_table_leaves_no_side_files(void)
{
   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, GAME, 1, "A.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, GAME, 1, "B.ds1", err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt.old"));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt.staging"));
   {
      char *t = table();
      TEST_ASSERT_NOT_NULL(t);
      TEST_ASSERT_EQUAL_INT(2, lvlprest_find_file_slot(t, 1, "A.ds1"));
      TEST_ASSERT_EQUAL_INT(3, lvlprest_find_file_slot(t, 1, "B.ds1"));
      free(t);
   }
}

void test_nothing_is_written_to_a_project_inside_the_game_install(void)
{
   char out[512], game[sizeof(GAME)];

   memcpy(game, GAME, sizeof(GAME));
   glb_config.d2_install = dir;
   TEST_ASSERT_EQUAL_INT(-1, map_ops_register(dir, GAME, 1, "x.ds1", err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "game install"));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_unregister(dir, GAME, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_restore_slot(dir, GAME, game, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_create_empty(dir, GAME, 1, "x.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_backup_file(dir, "x.ds1", "A", "2026-11-02", "{}\n",
                                                 out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_restore_file(dir, "nothing.ds1", "x.ds1", err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
   TEST_ASSERT_FALSE(exists("Global/Tiles/x.ds1"));
   TEST_ASSERT_FALSE(exists("backup"));
   glb_config.d2_install = NULL;
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
   RUN_TEST(test_unregistering_an_unlisted_map_leaves_no_table);
   RUN_TEST(test_overwriting_the_table_leaves_no_side_files);
   RUN_TEST(test_nothing_is_written_to_a_project_inside_the_game_install);
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
