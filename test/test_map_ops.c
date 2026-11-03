/* map_ops over a temp project: tables, new maps, backups, and what a failure
 * leaves behind. Nothing here touches a game install. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef WIN32
#include <io.h>
#endif

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
   "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
   "Town\t1\t2\tAct1\\Town\\TownN1.ds1\tAct1\\Town\\TownE1.ds1\t0\t0\t0\t0\r\n"
   "Full\t2\t6\ta.ds1\tb.ds1\tc.ds1\td.ds1\te.ds1\tf.ds1\r\n"
   "Only\t3\t1\tAct1\\Only.ds1\t0\t0\t0\t0\t0\r\n"
   "Own\t4\t0\tAct1\\Own.ds1\t0\t0\t0\t0\t0\r\n"
   "Tail\t5\t1\tAct1\\Tail1.ds1\tAct1\\Tail2.ds1\t0\t0\t0\t0\r\n";

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

   /* A name that fits without .ds1 but not with it is refused, not cut short. */
   {
      char small[8], mid[9];

      TEST_ASSERT_EQUAL_INT(0, map_ops_check_name("abc", small, sizeof(small), err, sizeof(err)));
      TEST_ASSERT_EQUAL_STRING("abc.ds1", small);
      TEST_ASSERT_EQUAL_INT(-1, map_ops_check_name("abcd", small, sizeof(small), err, sizeof(err)));
      TEST_ASSERT_NOT_EQUAL(0, (int) strlen(err));
      TEST_ASSERT_EQUAL_INT(0, map_ops_check_name("abcd.ds1", mid, sizeof(mid), err, sizeof(err)));
      TEST_ASSERT_EQUAL_INT(-1, map_ops_check_name("abcde.ds1", small, sizeof(small), err, sizeof(err)));
   }
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
   TEST_ASSERT_EQUAL_INT(3, lvlprest_find_file_slot(t, 1, "Act1/Town/New.ds1"));
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
      TEST_ASSERT_EQUAL_INT(3, lvlprest_find_file_slot(t, 1, "A.ds1"));
      TEST_ASSERT_EQUAL_INT(4, lvlprest_find_file_slot(t, 1, "B.ds1"));
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

void test_restore_goes_back_to_its_own_place_among_new_maps(void)
{
   char game[sizeof(GAME)];
   char *t;

   memcpy(game, GAME, sizeof(GAME));
   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, GAME, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, GAME, 1, "Act1/Town/New.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_slot(dir, GAME, game, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_INT(1, lvlprest_find_file_slot(t, 1, "TownN1.ds1"));
   TEST_ASSERT_EQUAL_INT(2, lvlprest_find_file_slot(t, 1, "TownE1.ds1"));
   TEST_ASSERT_EQUAL_INT(3, lvlprest_find_file_slot(t, 1, "New.ds1"));
   TEST_ASSERT_EQUAL_INT(3, lvlprest_files_value(t, 1));
   TEST_ASSERT_TRUE(lvlprest_row_is_simple(t, 1));
   free(t);
}

void test_restore_slot_writes_nothing_when_the_row_already_lists_the_map(void)
{
   char game[sizeof(GAME)];
   char *before, *after;

   memcpy(game, GAME, sizeof(GAME));
   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, GAME, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   before = table();
   TEST_ASSERT_NOT_NULL(before);
   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_slot(dir, GAME, game, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   after = table();
   TEST_ASSERT_EQUAL_STRING(before, after);
   free(before);
   free(after);
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

/* ---- Where a project keeps its files ---- */

static void write_rel(const char *rel, const char *text)
{
   char path[768];

   snprintf(path, sizeof(path), "%s/%s", dir, rel);
   TEST_ASSERT_TRUE(project_ensure_parent_dirs(path));
   TEST_ASSERT_TRUE(tt_write_file(path, text, strlen(text)));
}

static char *read_rel(const char *rel)
{
   char path[768];
   long len;

   snprintf(path, sizeof(path), "%s/%s", dir, rel);
   return map_ops_read_file(path, &len);
}

void test_read_file_reports_no_length_for_a_missing_file(void)
{
   char path[768];
   long len = 99;

   snprintf(path, sizeof(path), "%s/none.txt", dir);
   TEST_ASSERT_NULL(map_ops_read_file(path, &len));
   TEST_ASSERT_EQUAL_INT(0, (int) len);
}

void test_a_table_left_as_old_is_recovered_not_reseeded(void)
{
   char *t;

   /* A crash between moving the table aside and moving the new one in. */
   write_rel("Global/Excel/LvlPrest.txt.old", GAME);
   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, "Name\tDef\tFiles\tFile1\r\nOther\t9\t1\tx\r\n", 1,
                                             "Act1/Town/New.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_NOT_NULL(t);
   TEST_ASSERT_EQUAL_INT(1, lvlprest_find_file_slot(t, 1, "TownN1.ds1"));
   TEST_ASSERT_EQUAL_INT(3, lvlprest_find_file_slot(t, 1, "New.ds1"));
   free(t);
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt.old"));
}

void test_read_table_recovers_an_old_table(void)
{
   long len;
   char *t;

   write_rel("Data/Global/Excel/LvlPrest.txt.old", GAME);
   t = map_ops_read_table(dir, &len);
   TEST_ASSERT_NOT_NULL(t);
   TEST_ASSERT_EQUAL_STRING(GAME, t);
   free(t);
   TEST_ASSERT_TRUE(exists("Data/Global/Excel/LvlPrest.txt"));
   TEST_ASSERT_FALSE(exists("Data/Global/Excel/LvlPrest.txt.old"));
}

void test_a_table_that_cant_be_read_is_not_replaced_by_the_seed(void)
{
   char path[768];

   /* A folder at the table's path: there, but no text can be read from it. */
   snprintf(path, sizeof(path), "%s/Global", dir);
   TEST_ASSERT_TRUE(tt_mkdir(path));
   snprintf(path, sizeof(path), "%s/Global/Excel", dir);
   TEST_ASSERT_TRUE(tt_mkdir(path));
   snprintf(path, sizeof(path), "%s/Global/Excel/LvlPrest.txt", dir);
   TEST_ASSERT_TRUE(tt_mkdir(path));

   TEST_ASSERT_EQUAL_INT(-1, map_ops_register(dir, GAME, 1, "New.ds1", err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "LvlPrest.txt"));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_unregister(dir, GAME, 1, "Act1/Town/TownN1.ds1", err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt.staging"));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt.old"));
}

void test_a_data_layout_is_used_where_it_is(void)
{
   char out[512], *t;

   write_rel("Data/Global/Excel/LvlPrest.txt", GAME);
   write_rel("Data/Global/Tiles/Act1/Town/Old.ds1", "x");

   /* The table stays where it is. */
   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, NULL, 1, "Act1/Town/Old.ds1", err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
   t = read_rel("Data/Global/Excel/LvlPrest.txt");
   TEST_ASSERT_NOT_NULL(t);
   TEST_ASSERT_EQUAL_INT(3, lvlprest_find_file_slot(t, 1, "Old.ds1"));
   free(t);

   /* A file there counts as the project's, for exists checks and backups. */
   TEST_ASSERT_EQUAL_INT(-1, map_ops_create_empty(dir, NULL, 1, "Act1/Town/Old.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "already"));
   TEST_ASSERT_EQUAL_INT(0, map_ops_backup_file(dir, "Act1/Town/Old.ds1", "A", "2026-11-02", "{}\n",
                                                out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Data/Global/Tiles/Act1/Town/Old.ds1"));

   /* New tiles stay under Global/Tiles. */
   TEST_ASSERT_EQUAL_INT(0, map_ops_create_empty(dir, NULL, 1, "Act1/Town/New.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_TRUE(exists("Global/Tiles/Act1/Town/New.ds1"));
}

void test_the_reader_s_order_decides_between_layouts(void)
{
   char *t;

   /* mod_load_in_mem tries <project>/Data/... before <project>/... */
   write_rel("Data/Global/Excel/LvlPrest.txt", GAME);
   write_rel("Global/Excel/LvlPrest.txt", "Name\tDef\tFiles\tFile1\r\n");
   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, NULL, 1, "New.ds1", err, sizeof(err)));
   t = read_rel("Data/Global/Excel/LvlPrest.txt");
   TEST_ASSERT_EQUAL_INT(3, lvlprest_find_file_slot(t, 1, "New.ds1"));
   free(t);
   t = read_rel("Global/Excel/LvlPrest.txt");
   TEST_ASSERT_EQUAL_STRING("Name\tDef\tFiles\tFile1\r\n", t);
   free(t);
}

void test_a_different_case_file_is_the_same_file(void)
{
   char found[768], out[512];

   write_rel("global/tiles/ACT1/town/OLD.DS1", "x");
   TEST_ASSERT_EQUAL_INT(1, map_ops_find_tile(dir, "Act1/Town/Old.ds1", found, sizeof(found)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_create_empty(dir, GAME, 1, "Act1/Town/Old.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_backup_file(dir, "Act1/Town/Old.ds1", "A", "2026-11-02", "{}\n",
                                                out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("global/tiles/ACT1/town/OLD.DS1"));
}

void test_a_path_cant_reach_outside_the_project(void)
{
   char out[768], backup[768];

   TEST_ASSERT_EQUAL_INT(-1, map_ops_tile_path(dir, "../evil.ds1", out, sizeof(out)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_tile_path(dir, "Act1\\..\\..\\evil.ds1", out, sizeof(out)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_tile_path(dir, "/evil.ds1", out, sizeof(out)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_tile_path(dir, "\\evil.ds1", out, sizeof(out)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_tile_path(dir, "C:\\evil.ds1", out, sizeof(out)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_tile_path(dir, "Act1/a..b.ds1", out, sizeof(out)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_find_tile(dir, "../evil.ds1", out, sizeof(out)));

   /* original_path comes from a backup's JSON. */
   write_rel("backup/b.ds1", "x");
   snprintf(backup, sizeof(backup), "%s/backup/b.ds1", dir);
   TEST_ASSERT_EQUAL_INT(-1, map_ops_restore_file(dir, backup, "../../evil.ds1", err, sizeof(err)));
   TEST_ASSERT_TRUE(exists("backup/b.ds1"));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_backup_file(dir, "../backup/b.ds1", "A", "2026-11-02", "{}\n",
                                                 out, sizeof(out), err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_create_empty(dir, GAME, 1, "../New.ds1", 5, 5, 1, err, sizeof(err)));

   /* And the final path can't be in the game's folder. */
   glb_config.d2_install = dir;
   TEST_ASSERT_EQUAL_INT(-1, map_ops_tile_path(dir, "Act1/New.ds1", out, sizeof(out)));
   glb_config.d2_install = NULL;
}

static const char DUPES[] =
   "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
   "Dup\t3\t3\tAct1\\Dup.ds1\tAct1\\Dup.ds1\tAct1\\Other.ds1\t0\t0\t0\r\n";

void test_the_same_file_twice_in_a_row_round_trips(void)
{
   char game[sizeof(DUPES)];
   char *t;

   memcpy(game, DUPES, sizeof(DUPES));
   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, DUPES, 3, "Act1/Dup.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_INT(0, lvlprest_find_file_slot(t, 3, "Act1/Dup.ds1"));
   TEST_ASSERT_EQUAL_INT(1, lvlprest_find_file_slot(t, 3, "Act1/Other.ds1"));
   free(t);

   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_slot(dir, DUPES, game, 3, "Act1/Dup.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_STRING(DUPES, t);
   free(t);
}

void test_restore_puts_back_only_the_copies_that_are_missing(void)
{
   char game[sizeof(DUPES)];
   char *t;

   memcpy(game, DUPES, sizeof(DUPES));
   write_rel("Global/Excel/LvlPrest.txt",
             "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
             "Dup\t3\t2\tAct1\\Dup.ds1\tAct1\\Other.ds1\t0\t0\t0\t0\r\n");
   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_slot(dir, DUPES, game, 3, "Act1/Dup.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_STRING(DUPES, t);
   free(t);
}

static const char NAMESAKES[] =
   "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
   "Bar\t50\t3\tZ\\Other.ds1\tA\\Bar.ds1\tB\\Bar.ds1\t0\t0\t0\r\n";

void test_unregister_leaves_a_namesake_in_another_folder(void)
{
   char *t;

   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, NAMESAKES, 50, "a/BAR.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_NOT_NULL(t);
   TEST_ASSERT_FALSE(lvlprest_lists_path(t, 50, "A/Bar.ds1"));
   TEST_ASSERT_TRUE(lvlprest_lists_path(t, 50, "B/Bar.ds1"));
   free(t);
}

void test_namesakes_round_trip_one_at_a_time(void)
{
   char game[sizeof(NAMESAKES)];
   char *t;

   memcpy(game, NAMESAKES, sizeof(NAMESAKES));
   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, NAMESAKES, 50, "A/Bar.ds1", err, sizeof(err)));
   /* The other one still listed mustn't count as this one already listed. */
   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_slot(dir, NAMESAKES, game, 50, "A/Bar.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_STRING(NAMESAKES, t);
   free(t);

   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, NAMESAKES, 50, "B/Bar.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, NAMESAKES, 50, "A/Bar.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_slot(dir, NAMESAKES, game, 50, "B/Bar.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_slot(dir, NAMESAKES, game, 50, "A/Bar.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_STRING(NAMESAKES, t);
   free(t);
}

void test_restore_slot_wants_the_game_to_list_the_whole_path(void)
{
   char game[sizeof(NAMESAKES)];

   memcpy(game, NAMESAKES, sizeof(NAMESAKES));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_restore_slot(dir, NAMESAKES, game, 50, "C/Bar.ds1", err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "doesn't list"));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
}

void test_a_backup_that_cant_be_removed_is_only_a_warning(void)
{
   char out[512], lock[768];
   int kept;

   TEST_ASSERT_EQUAL_INT(0, map_ops_create_empty(dir, GAME, 1, "New.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_backup_file(dir, "New.ds1", "A", "2026-11-02", "{}\n",
                                                out, sizeof(out), err, sizeof(err)));
   /* Read-only stops a remove on Windows; a read-only folder does elsewhere. */
#ifdef WIN32
   snprintf(lock, sizeof(lock), "%s", out);
   TEST_ASSERT_EQUAL_INT(0, _chmod(lock, _S_IREAD));
#else
   snprintf(lock, sizeof(lock), "%s/backup/2026-11-02_A_New", dir);
   TEST_ASSERT_EQUAL_INT(0, chmod(lock, 0555));
#endif

   err[0] = 0;
   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_file(dir, out, "New.ds1", err, sizeof(err)));
   TEST_ASSERT_TRUE(exists("Global/Tiles/New.ds1"));
   kept = exists("backup/2026-11-02_A_New/New.ds1");

#ifdef WIN32
   _chmod(lock, _S_IREAD | _S_IWRITE);
#else
   chmod(lock, 0755);
#endif
   /* Running as root on POSIX, the remove still succeeds. */
   if (kept)
      TEST_ASSERT_NOT_NULL(strstr(err, "backup"));
   else
      TEST_ASSERT_EQUAL_STRING("", err);
}

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

/* ---- The Files column: what may join and leave a row ---- */

static const char ROWS[] =
   "Name\tDef\tSizeX\tSizeY\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
   "Room\t168\t10\t14\t2\tr1.ds1\tr2.ds1\t0\t0\t0\t0\r\n"
   "Tri\t300\t0\t0\t1\tTri.ds1\t0\t0\t0\t0\t0\r\n"
   "Tail\t5\t8\t8\t1\tt1.ds1\tnew.ds1\tt3.ds1\t0\t0\t0\r\n"
   "Hole\t6\t8\t8\t3\th1.ds1\t0\th3.ds1\tnew.ds1\t0\t0\r\n"
   "Blank\t7\t8\t8\t2\t\tb2.ds1\t0\t0\t0\t0\r\n"
   "Own\t8\t8\t8\t0\to1.ds1\tnew.ds1\t0\t0\t0\t0\r\n"
   "Only\t9\t8\t8\t1\tnew.ds1\t0\t0\t0\t0\t0\r\n"
   "Wide\t10\t8\t0\t1\tw.ds1\t0\t0\t0\t0\t0\r\n"
   "Huge\t11\t999\t1000\t1\tu.ds1\t0\t0\t0\t0\t0\r\n"
   "Full\t12\t8\t8\t6\ta.ds1\tb.ds1\tc.ds1\td.ds1\te.ds1\tf.ds1\r\n";

/* The fixture as writable text, since the table functions take a char *. */
static char rows[sizeof(ROWS)];

static void fresh_rows(void)
{
   memcpy(rows, ROWS, sizeof(ROWS));
}

void test_a_map_the_table_lists_needs_no_register(void)
{
   /* No project table: the seed decides, so a Modified map isn't listed twice,
      and a town row isn't asked to take a map it already has. */
   TEST_ASSERT_TRUE(map_ops_lists_map(dir, ROWS, 300, "Tri.ds1"));
   TEST_ASSERT_FALSE(map_ops_lists_map(dir, ROWS, 300, "Other.ds1"));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));

   /* A project table wins over the seed. */
   write_rel("Global/Excel/LvlPrest.txt", GAME);
   TEST_ASSERT_FALSE(map_ops_lists_map(dir, ROWS, 300, "Tri.ds1"));
   TEST_ASSERT_TRUE(map_ops_lists_map(dir, ROWS, 3, "Act1/Only.ds1"));
}

void test_no_table_and_no_seed_lists_nothing(void)
{
   TEST_ASSERT_FALSE(map_ops_lists_map(dir, NULL, 300, "Tri.ds1"));
}

void test_delete_clears_a_new_map_past_files_and_leaves_files_alone(void)
{
   char *t;

   fresh_rows();
   TEST_ASSERT_EQUAL_INT(0, map_ops_check_unregister_new(rows, 5, "new.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister_new(dir, ROWS, 5, "new.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_NOT_NULL(t);
   TEST_ASSERT_EQUAL_INT(1, lvlprest_files_value(t, 5));
   TEST_ASSERT_FALSE(lvlprest_lists_path(t, 5, "new.ds1"));
   /* The row's other extra cell stays where its own rules look for it. */
   TEST_ASSERT_EQUAL_INT(3, lvlprest_find_path_slot(t, 5, "t3.ds1"));
   free(t);
}

void test_remove_refuses_that_same_cell(void)
{
   fresh_rows();
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_unregister(rows, 5, "new.ds1", err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "own rules"));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_unregister(dir, ROWS, 5, "new.ds1", err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
}

void test_delete_moves_a_counted_map_out_like_remove(void)
{
   char *t;

   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister_new(dir, ROWS, 168, "r1.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_INT(1, lvlprest_files_value(t, 168));
   TEST_ASSERT_EQUAL_INT(1, lvlprest_find_path_slot(t, 168, "r2.ds1"));
   TEST_ASSERT_TRUE(lvlprest_row_is_simple(t, 168));
   free(t);
}

void test_delete_refuses_a_new_map_in_a_row_with_a_hole(void)
{
   TEST_ASSERT_EQUAL_INT(-1, map_ops_unregister_new(dir, ROWS, 6, "new.ds1", err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "File2"));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
}

void test_delete_of_a_rows_only_counted_map_says_what_to_do(void)
{
   TEST_ASSERT_EQUAL_INT(-1, map_ops_unregister_new(dir, ROWS, 9, "new.ds1", err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "only map"));
   TEST_ASSERT_NOT_NULL(strstr(err, "Restore Slot"));
   TEST_ASSERT_NOT_NULL(strstr(err, "by hand"));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
}

void test_delete_refuses_a_row_with_files_0(void)
{
   /* The generator chooses these cells itself, so the game may well read it. */
   TEST_ASSERT_EQUAL_INT(-1, map_ops_unregister_new(dir, ROWS, 8, "new.ds1", err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "by hand"));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
}

void test_delete_of_a_counted_map_in_an_unusual_row_is_refused(void)
{
   TEST_ASSERT_EQUAL_INT(-1, map_ops_unregister_new(dir, ROWS, 5, "t1.ds1", err, sizeof(err)));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));
}

void test_a_hole_below_files_is_reported_by_every_check(void)
{
   static const char HOLE[] = "Def 6's File2 is empty, but Files counts it, which can crash "
                              "the game. Fix that row in LvlPrest.txt by hand.";

   fresh_rows();
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_register(rows, 6, err, sizeof(err)));
   TEST_ASSERT_EQUAL_STRING(HOLE, err);
   err[0] = 0;
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_unregister(rows, 6, "h1.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_STRING(HOLE, err);
   err[0] = 0;
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_unregister_new(rows, 6, "new.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_STRING(HOLE, err);

   /* A blank counts as well as a 0. */
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_register(rows, 7, err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "Def 7's File1 is empty"));

   /* No automatic rewrite. */
   TEST_ASSERT_EQUAL_STRING(ROWS, rows);
}

void test_a_hole_doesnt_stop_a_map_the_row_doesnt_list(void)
{
   fresh_rows();
   TEST_ASSERT_EQUAL_INT(0, map_ops_check_unregister(rows, 6, "elsewhere.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_check_unregister_new(rows, 6, "elsewhere.ds1", err, sizeof(err)));
}

void test_new_is_refused_and_clone_allowed_on_a_whole_level(void)
{
   fresh_rows();
   TEST_ASSERT_EQUAL_INT(0, map_ops_check_register(rows, 300, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_new_empty(rows, 300, err, sizeof(err)));
   TEST_ASSERT_EQUAL_STRING("Def 300's row is a whole level, and an empty map would join the "
                            "game's random pick for it. Clone a working map instead.", err);
   TEST_ASSERT_EQUAL_INT(0, map_ops_check_new_empty(rows, 168, err, sizeof(err)));
   /* One size is a size. */
   TEST_ASSERT_EQUAL_INT(0, map_ops_check_new_empty(rows, 10, err, sizeof(err)));
   /* map_ops_check_register's reasons come first. */
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_new_empty(rows, 8, err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "own rules"));
}

void test_create_refuses_a_whole_level_and_writes_nothing(void)
{
   TEST_ASSERT_EQUAL_INT(-1, map_ops_create_empty(dir, ROWS, 300, "New.ds1", 5, 5, 1, err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "whole level"));
   TEST_ASSERT_FALSE(exists("Global/Tiles/New.ds1"));
   TEST_ASSERT_FALSE(exists("Global/Excel/LvlPrest.txt"));

   /* Clone's route, registering a map, still takes it. */
   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, ROWS, 300, "New.ds1", err, sizeof(err)));
   TEST_ASSERT_TRUE(exists("Global/Excel/LvlPrest.txt"));
}

void test_a_table_with_no_size_columns_doesnt_call_a_row_a_whole_level(void)
{
   char game[sizeof(GAME)];

   memcpy(game, GAME, sizeof(GAME));
   TEST_ASSERT_EQUAL_INT(0, map_ops_check_new_empty(game, 1, err, sizeof(err)));
}

void test_size_defaults_are_the_map_s_width_not_the_stored_value(void)
{
   int w = 20, h = 30;

   /* barW.ds1 stores 10 and 14, its width and height minus one, and Def 168's
      SizeX and SizeY are 10 and 14. */
   fresh_rows();
   map_ops_default_size(rows, 168, &w, &h);
   TEST_ASSERT_EQUAL_INT(11, w);
   TEST_ASSERT_EQUAL_INT(15, h);

   w = 20;
   h = 30;
   map_ops_default_size(rows, 300, &w, &h);    /* no size: today's default */
   TEST_ASSERT_EQUAL_INT(20, w);
   TEST_ASSERT_EQUAL_INT(30, h);
   map_ops_default_size(rows, 10, &w, &h);     /* one nonzero */
   TEST_ASSERT_EQUAL_INT(9, w);
   TEST_ASSERT_EQUAL_INT(30, h);
   w = 20;
   map_ops_default_size(rows, 999, &w, &h);    /* no row */
   TEST_ASSERT_EQUAL_INT(20, w);
   map_ops_default_size(rows, 11, &w, &h);     /* SizeY 1000 would be past the limit */
   TEST_ASSERT_EQUAL_INT(1000, w);
   TEST_ASSERT_EQUAL_INT(30, h);
}

void test_a_row_takes_only_its_own_size(void)
{
   char err[256] = "";

   fresh_rows();
   TEST_ASSERT_EQUAL_INT(0, map_ops_check_row_size(rows, 168, 11, 15, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_row_size(rows, 168, 12, 15, err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "11 by 15"));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_row_size(rows, 168, 11, 16, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, map_ops_check_row_size(rows, 300, 7, 9, err, sizeof(err)));  /* no size */
   TEST_ASSERT_EQUAL_INT(0, map_ops_check_row_size(rows, 999, 7, 9, err, sizeof(err)));  /* no row */
}

void test_the_shared_refusal_doesnt_mention_a_tileset(void)
{
   char game[sizeof(GAME)];

   memcpy(game, GAME, sizeof(GAME));
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_register(game, 4, err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "own rules"));
   TEST_ASSERT_NULL(strstr(err, "tileset"));
}

void test_a_table_with_no_files_column_says_so_first(void)
{
   char no_files[] = "Name\tDef\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                     "A\t1\ta.ds1\t0\t0\t0\t0\t0\r\n";
   char no_file3[] = "Name\tDef\tFiles\tFile1\tFile2\tFile4\tFile5\tFile6\r\n"
                     "A\t1\t1\ta.ds1\t0\t0\t0\t0\r\n";
   static const char NO_FILES[] = "The project's LvlPrest.txt has no Files column, so maps can't "
                                  "be added to or removed from it.";

   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_register(no_files, 1, err, sizeof(err)));
   TEST_ASSERT_EQUAL_STRING(NO_FILES, err);
   err[0] = 0;
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_unregister(no_files, 1, "a.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_STRING(NO_FILES, err);
   err[0] = 0;
   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_unregister_new(no_files, 1, "a.ds1", err, sizeof(err)));
   TEST_ASSERT_EQUAL_STRING(NO_FILES, err);

   TEST_ASSERT_EQUAL_INT(-1, map_ops_check_register(no_file3, 1, err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "no File3 column"));
}

void test_a_path_past_the_limit_says_to_rename_it(void)
{
   char path[64];

   memset(path, 'a', sizeof(path));
   memcpy(path + 42 - 4, ".ds1", 5);
   TEST_ASSERT_EQUAL_INT(-1, map_ops_register(dir, GAME, 1, path, err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "renamed by hand"));
}

/* The dialog's status row fits about 62 glyphs. */
static void assert_status(char * text, int def, int empty_map, const char * part)
{
   int rc = empty_map ? map_ops_check_new_empty(text, def, err, sizeof(err))
                      : map_ops_check_register(text, def, err, sizeof(err));

   TEST_ASSERT_EQUAL_INT(-1, rc);
   map_ops_dialog_status(text, def, err, sizeof(err));
   TEST_ASSERT_NOT_NULL(strstr(err, part));
   TEST_ASSERT_TRUE(strlen(err) <= 62);
}

void test_the_dialog_gets_a_short_status_that_fits_the_case(void)
{
   char no_files[] = "Name\tDef\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                     "A\t1090\ta.ds1\t0\t0\t0\t0\t0\r\n";
   char big_hole[] = "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                     "A\t1090\t6\ta.ds1\tb.ds1\tc.ds1\td.ds1\te.ds1\t0\r\n";

   fresh_rows();
   assert_status(rows, 300, 1, "whole level");
   assert_status(rows, 300, 1, "Clone");
   assert_status(rows, 6, 0, "File2");
   assert_status(rows, 8, 0, "Pick another tileset");
   assert_status(rows, 12, 0, "no free File slot");
   assert_status(rows, 999, 0, "isn't in");
   assert_status(big_hole, 1090, 0, "File6");
   assert_status(no_files, 1090, 0, "no Files column");
   TEST_ASSERT_NULL(strstr(err, "tileset"));
}

void test_the_same_file_either_side_of_another_round_trips(void)
{
   static const char SPLIT[] =
      "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
      "Dup\t3\t3\tAct1\\Dup.ds1\tAct1\\Other.ds1\tAct1\\Dup.ds1\t0\t0\t0\r\n";
   char game[sizeof(SPLIT)];
   char *t;

   memcpy(game, SPLIT, sizeof(SPLIT));
   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, SPLIT, 3, "Act1/Dup.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_INT(1, lvlprest_files_value(t, 3));
   TEST_ASSERT_EQUAL_INT(1, lvlprest_find_path_slot(t, 3, "Act1/Other.ds1"));
   TEST_ASSERT_FALSE(lvlprest_lists_path(t, 3, "Act1/Dup.ds1"));
   free(t);

   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_slot(dir, SPLIT, game, 3, "Act1/Dup.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_STRING(SPLIT, t);
   free(t);
}

static void assert_restore_slot_refused(int def)
{
   char game[] =
      "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
      "Row\t12\t1\tgone.ds1\t0\t0\t0\t0\t0\r\n"
      "Row\t8\t1\tgone.ds1\t0\t0\t0\t0\t0\r\n";
   char *t;

   write_rel("Global/Excel/LvlPrest.txt", ROWS);
   TEST_ASSERT_EQUAL_INT(-1, map_ops_restore_slot(dir, NULL, game, def, "gone.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_STRING(ROWS, t);
   free(t);
}

void test_restore_slot_refuses_a_full_row_and_changes_nothing(void)
{
   assert_restore_slot_refused(12);
   TEST_ASSERT_NOT_NULL(strstr(err, "no free File slot"));
}

void test_restore_slot_refuses_an_unusual_row_and_changes_nothing(void)
{
   assert_restore_slot_refused(8);
   TEST_ASSERT_NOT_NULL(strstr(err, "own rules"));
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
   RUN_TEST(test_restore_goes_back_to_its_own_place_among_new_maps);
   RUN_TEST(test_restore_slot_writes_nothing_when_the_row_already_lists_the_map);
   RUN_TEST(test_create_writes_the_map_and_registers_it);
   RUN_TEST(test_a_failed_table_write_removes_the_new_map);
   RUN_TEST(test_create_refuses_an_existing_file);
   RUN_TEST(test_empty_map_header_matches_the_reader);
   RUN_TEST(test_backup_moves_the_file_and_restore_brings_it_back);
   RUN_TEST(test_backup_with_nothing_to_back_up_stops_with_a_message);
   RUN_TEST(test_restore_refuses_to_replace_a_file);
   RUN_TEST(test_read_file_reports_no_length_for_a_missing_file);
   RUN_TEST(test_a_table_left_as_old_is_recovered_not_reseeded);
   RUN_TEST(test_read_table_recovers_an_old_table);
   RUN_TEST(test_a_table_that_cant_be_read_is_not_replaced_by_the_seed);
   RUN_TEST(test_a_data_layout_is_used_where_it_is);
   RUN_TEST(test_the_reader_s_order_decides_between_layouts);
   RUN_TEST(test_a_different_case_file_is_the_same_file);
   RUN_TEST(test_a_path_cant_reach_outside_the_project);
   RUN_TEST(test_the_same_file_twice_in_a_row_round_trips);
   RUN_TEST(test_restore_puts_back_only_the_copies_that_are_missing);
   RUN_TEST(test_a_backup_that_cant_be_removed_is_only_a_warning);
   RUN_TEST(test_unregister_leaves_a_namesake_in_another_folder);
   RUN_TEST(test_namesakes_round_trip_one_at_a_time);
   RUN_TEST(test_restore_slot_wants_the_game_to_list_the_whole_path);
   RUN_TEST(test_register_raises_files);
   RUN_TEST(test_register_refuses_rows_that_choose_their_own_maps);
   RUN_TEST(test_register_refuses_a_path_past_the_games_limit);
   RUN_TEST(test_create_that_cant_register_removes_the_map);
   RUN_TEST(test_unregister_leaves_no_hole_and_lowers_files);
   RUN_TEST(test_unregister_refuses_a_rows_only_map);
   RUN_TEST(test_unregister_refuses_rows_that_choose_their_own_maps);
   RUN_TEST(test_a_row_listing_only_one_file_twice_counts_as_an_only_map);
   RUN_TEST(test_restore_slot_raises_files_again);
   RUN_TEST(test_a_map_the_table_lists_needs_no_register);
   RUN_TEST(test_no_table_and_no_seed_lists_nothing);
   RUN_TEST(test_delete_clears_a_new_map_past_files_and_leaves_files_alone);
   RUN_TEST(test_remove_refuses_that_same_cell);
   RUN_TEST(test_delete_moves_a_counted_map_out_like_remove);
   RUN_TEST(test_delete_refuses_a_new_map_in_a_row_with_a_hole);
   RUN_TEST(test_delete_of_a_rows_only_counted_map_says_what_to_do);
   RUN_TEST(test_delete_refuses_a_row_with_files_0);
   RUN_TEST(test_delete_of_a_counted_map_in_an_unusual_row_is_refused);
   RUN_TEST(test_a_hole_below_files_is_reported_by_every_check);
   RUN_TEST(test_a_hole_doesnt_stop_a_map_the_row_doesnt_list);
   RUN_TEST(test_new_is_refused_and_clone_allowed_on_a_whole_level);
   RUN_TEST(test_create_refuses_a_whole_level_and_writes_nothing);
   RUN_TEST(test_a_table_with_no_size_columns_doesnt_call_a_row_a_whole_level);
   RUN_TEST(test_size_defaults_are_the_map_s_width_not_the_stored_value);
   RUN_TEST(test_a_row_takes_only_its_own_size);
   RUN_TEST(test_the_shared_refusal_doesnt_mention_a_tileset);
   RUN_TEST(test_a_table_with_no_files_column_says_so_first);
   RUN_TEST(test_a_path_past_the_limit_says_to_rename_it);
   RUN_TEST(test_the_dialog_gets_a_short_status_that_fits_the_case);
   RUN_TEST(test_the_same_file_either_side_of_another_round_trips);
   RUN_TEST(test_restore_slot_refuses_a_full_row_and_changes_nothing);
   RUN_TEST(test_restore_slot_refuses_an_unusual_row_and_changes_nothing);
   return UNITY_END();
}
