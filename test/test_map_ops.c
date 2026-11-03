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
   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, "Name\tDef\tFile1\r\nOther\t9\t0\r\n", 1,
                                             "Act1/Town/New.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_NOT_NULL(t);
   TEST_ASSERT_EQUAL_INT(1, lvlprest_find_file_slot(t, 1, "TownN1.ds1"));
   TEST_ASSERT_EQUAL_INT(2, lvlprest_find_file_slot(t, 1, "New.ds1"));
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
   TEST_ASSERT_EQUAL_INT(2, lvlprest_find_file_slot(t, 1, "Old.ds1"));
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
   write_rel("Global/Excel/LvlPrest.txt", "Name\tDef\tFile1\r\n");
   TEST_ASSERT_EQUAL_INT(0, map_ops_register(dir, NULL, 1, "New.ds1", err, sizeof(err)));
   t = read_rel("Data/Global/Excel/LvlPrest.txt");
   TEST_ASSERT_EQUAL_INT(2, lvlprest_find_file_slot(t, 1, "New.ds1"));
   free(t);
   t = read_rel("Global/Excel/LvlPrest.txt");
   TEST_ASSERT_EQUAL_STRING("Name\tDef\tFile1\r\n", t);
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
   "Name\tDef\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
   "Dup\t3\tAct1\\Dup.ds1\t0\tAct1\\Dup.ds1\tAct1\\Other.ds1\t0\t0\r\n";

void test_the_same_file_twice_in_a_row_round_trips(void)
{
   char game[sizeof(DUPES)];
   char *t;

   memcpy(game, DUPES, sizeof(DUPES));
   TEST_ASSERT_EQUAL_INT(0, map_ops_unregister(dir, DUPES, 3, "Act1/Dup.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_INT(0, lvlprest_find_file_slot(t, 3, "Act1/Dup.ds1"));
   TEST_ASSERT_EQUAL_INT(4, lvlprest_find_file_slot(t, 3, "Act1/Other.ds1"));
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
             "Name\tDef\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
             "Dup\t3\tAct1\\Dup.ds1\t0\t0\tAct1\\Other.ds1\t0\t0\r\n");
   TEST_ASSERT_EQUAL_INT(0, map_ops_restore_slot(dir, DUPES, game, 3, "Act1/Dup.ds1", err, sizeof(err)));
   t = table();
   TEST_ASSERT_EQUAL_STRING(DUPES, t);
   free(t);
}

static const char NAMESAKES[] =
   "Name\tDef\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
   "Bar\t50\tA\\Bar.ds1\tB\\Bar.ds1\t0\t0\t0\t0\r\n";

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
   return UNITY_END();
}
