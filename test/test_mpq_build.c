/* mpq_build: which project files a build takes, what it refuses, and the archive it
 * writes over a base made with mpq_write. Nothing here touches a game install. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef WIN32
#include <sys/utime.h>
#define set_times _utime
#define TIMES_S _utimbuf
#else
#include <utime.h>
#define set_times utime
#define TIMES_S utimbuf
#endif

#include "unity/unity.h"
#include "structs.h"
#include "error.h"
#include "mpq/MpqView.h"
#include "mpq/mpqtypes.h"
#include "core/map_ops.h"
#include "core/mpq_build.h"
#include "core/mpq_write.h"
#include "core/project.h"
#include "platform.h"
#include "support/test_tmp.h"

CONFIG_S glb_config;

static GLB_MPQ_S mpq_struct;
static char dir[512];
static char proj[640];
static char test_copy[640];
static char base[768];   /* path_of() writes up to 768 */
static char out[640];
static char problems[4096];

void ds1edit_error(const char * text)
{
   TEST_FAIL_MESSAGE(text);
}

static char GAME[] =
   "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
   "Town\t1\t2\tAct1\\Town\\TownN1.ds1\tAct1\\Town\\TownE1.ds1\t0\t0\t0\t0\r\n"
   "Two\t2\t2\ta.ds1\tb.ds1\t0\t0\t0\t0\r\n"
   "Shipped\t915\t3\tAct1\\H1.ds1\t0\tAct1\\H3.ds1\t0\t0\t0\r\n";

static const char * const IN_GAME[] = {
   "Act1/Town/TownN1.ds1", "Act1/Town/TownE1.ds1", "a.ds1", "b.ds1",
   "Act1/H1.ds1", "Act1/H3.ds1", "Act1/Old/Town.ds1", NULL
};

static int in_game(const char * path, void * ctx)
{
   int i;

   (void) ctx;
   for (i = 0; IN_GAME[i] != NULL; i++)
   {
      const char * a = IN_GAME[i], * b = path;

      while (*a != '\0' && *b != '\0')
      {
         char x = *a == '\\' ? '/' : *a, y = *b == '\\' ? '/' : *b;

         if (x >= 'A' && x <= 'Z') x = (char) (x + 32);
         if (y >= 'A' && y <= 'Z') y = (char) (y + 32);
         if (x != y)
            break;
         a++;
         b++;
      }
      if (*a == '\0' && *b == '\0')
         return 1;
   }
   return 0;
}

static void path_of(char * dest, const char * rel)
{
   snprintf(dest, 768, "%s/%s", dir, rel);
}

static void put(const char * rel, const char * text)
{
   char path[768];

   path_of(path, rel);
   TEST_ASSERT_TRUE(project_ensure_parent_dirs(path));
   TEST_ASSERT_TRUE(tt_write_file(path, text, strlen(text)));
}

static int exists(const char * rel)
{
   char path[768];

   path_of(path, rel);
   return tt_file_exists(path);
}

static void make_base(void)
{
   MPQ_WRITE_FILE_S f[2];
   char a[768], b[768], err[256];

   put("src/base_only.txt", "only in the base");
   put("src/town.ds1", "the game's town");
   path_of(a, "src/base_only.txt");
   path_of(b, "src/town.ds1");
   f[0].archive_name = "data\\global\\excel\\base_only.txt";
   f[0].source_path = a;
   f[1].archive_name = "data\\global\\tiles\\act1\\town\\townn1.ds1";
   f[1].source_path = b;
   path_of(base, "install/patch_d2.mpq");
   TEST_ASSERT_TRUE(project_ensure_parent_dirs(base));
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, mpq_write(NULL, base, f, 2, err, sizeof(err)), err);
}

void setUp(void)
{
   size_t n;

   TEST_ASSERT_TRUE(tt_root_create("mpq_build", dir, sizeof(dir)));
   n = strlen(dir);
   if (n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\'))
      dir[n - 1] = 0;
   memset(&glb_config, 0, sizeof(glb_config));
   snprintf(proj, sizeof(proj), "%s/proj", dir);
   snprintf(test_copy, sizeof(test_copy), "%s/test", dir);
   snprintf(out, sizeof(out), "%s/build/patch_d2.mpq", proj);
   TEST_ASSERT_TRUE(tt_mkdir(proj));
   problems[0] = 0;
   make_base();
}

void tearDown(void)
{
   tt_remove_tree(dir);
}

/* The project's table, and a test copy that has compiled it (and any other named
   table, which must be in the project) and is the same game version as the base. */
static void project_table(const char * text)
{
   put("proj/Global/Excel/LvlPrest.txt", text);
}

static void compile(const char * name)
{
   char src[768], dst[768], bin[768];
   unsigned char * data;
   size_t len;

   snprintf(src, sizeof(src), "%s/Global/Excel/%s.txt", proj, name);
   data = tt_read_file(src, &len);
   TEST_ASSERT_NOT_NULL(data);
   snprintf(dst, sizeof(dst), "%s/DATA/GLOBAL/EXCEL/%s.txt", test_copy, name);
   TEST_ASSERT_TRUE(project_ensure_parent_dirs(dst));
   TEST_ASSERT_TRUE(tt_write_file(dst, data, len));
   free(data);
   snprintf(bin, sizeof(bin), "%s/DATA/GLOBAL/EXCEL/%s.bin", test_copy, name);
   TEST_ASSERT_TRUE(tt_write_file(bin, "compiled", 8));
}

static void drop(const char * rel)
{
   char path[768];

   path_of(path, rel);
   TEST_ASSERT_EQUAL_INT(0, remove(path));
}

static void test_copy_patch(void)
{
   char dst[768];
   unsigned char * data;
   size_t len;

   data = tt_read_file(base, &len);
   TEST_ASSERT_NOT_NULL(data);
   snprintf(dst, sizeof(dst), "%s/patch_d2.mpq", test_copy);
   TEST_ASSERT_TRUE(project_ensure_parent_dirs(dst));
   TEST_ASSERT_TRUE(tt_write_file(dst, data, len));
   free(data);
}

static int run_with(const char * test_dir, MPQ_BUILD_RESULT_S * res)
{
   MPQ_BUILD_ARGS_S a;

   memset(&a, 0, sizeof(a));
   a.project_dir = proj;
   a.base_mpq = base;
   a.test_game_dir = test_dir;
   a.out_mpq = out;
   a.game_table = GAME;
   a.in_game = in_game;
   problems[0] = 0;
   return mpq_build_run(&a, res, problems, sizeof(problems));
}

static int run(void)
{
   MPQ_BUILD_RESULT_S res;

   return run_with(test_copy, &res);
}

static void expect_ok(void)
{
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, run(), problems);
   TEST_ASSERT_TRUE(exists("proj/build/patch_d2.mpq"));
}

static void expect_problem(const char * text)
{
   char path[768];

   path_of(path, "proj/build/patch_d2.mpq");
   remove(path);
   TEST_ASSERT_EQUAL_INT(-1, run());
   TEST_ASSERT_NOT_NULL_MESSAGE(strstr(problems, text), problems);
   TEST_ASSERT_FALSE(exists("proj/build/patch_d2.mpq"));
}

/* Look name up in the archive through the editor's reader. -1 when absent;
   *buf is malloc'd when found. */
static int read_back(const char * name, unsigned char ** buf, long * len)
{
   int entry;

   memset(&mpq_struct, 0, sizeof(mpq_struct));
   glb_mpq = &mpq_struct;
   mpq_batch_open(out);
   *buf = NULL;
   *len = 0;
   entry = mpq_batch_load_in_mem((char *) name, (void **) buf, len, 0);
   mpq_batch_close();
   return entry;
}

static void expect_in_archive(const char * name, const char * text)
{
   unsigned char * got;
   long len;

   TEST_ASSERT_TRUE_MESSAGE(read_back(name, &got, &len) != -1, name);
   TEST_ASSERT_EQUAL_INT((int) strlen(text), (int) len);
   TEST_ASSERT_EQUAL_MEMORY(text, got, strlen(text));
   free(got);
}

static void expect_not_in_archive(const char * name)
{
   unsigned char * got;
   long len;

   TEST_ASSERT_EQUAL_INT(-1, read_back(name, &got, &len));
}

/* An archive file as NUL-ended text. */
static char * read_text(const char * name)
{
   unsigned char * got;
   char * text;
   long len;

   TEST_ASSERT_TRUE_MESSAGE(read_back(name, &got, &len) != -1, name);
   text = (char *) malloc((size_t) len + 1);
   TEST_ASSERT_NOT_NULL(text);
   memcpy(text, got, (size_t) len);
   text[len] = '\0';
   free(got);
   return text;
}

typedef struct { char log[2048]; } SEEN_S;

/* "T:" or "X:" for a tile or a table, the rel, then @D if the source is under the
   project's Data folder and @G if under Global. */
static void note(const char * source, const char * rel, int is_table, void * ctx)
{
   SEEN_S * s = (SEEN_S *) ctx;
   const char * tail = source + strlen(proj) + 1;   /* past the separator */

   snprintf(s->log + strlen(s->log), sizeof(s->log) - strlen(s->log), "%s%s@%c;",
            is_table ? "X:" : "T:", rel, strncasecmp(tail, "data", 4) == 0 ? 'D' : 'G');
}

void test_files_taken_by_extension_folder_and_case(void)
{
   SEEN_S s;

   put("proj/Global/Tiles/Act1/Town/Mine.ds1", "m");
   put("proj/Global/Tiles/Act1/Pal.DT1", "p");
   put("proj/Data/Global/Tiles/Act2/Deep/Walls.dt1", "w");
   put("proj/Global/Excel/Misc.TXT", "t");
   put("proj/Data/Global/Excel/Armor.txt", "t");
   put("proj/Global/Tiles/readme.md", "x");
   put("proj/Global/Tiles/Act1/Town/Mine.ds1.bak", "x");
   put("proj/Global/Excel/Misc.bin", "x");
   put("proj/Global/Excel/Sub/Nested.txt", "x");
   put("proj/Global/Other/Thing.ds1", "x");
   put("proj/Thing.ds1", "x");
   memset(&s, 0, sizeof(s));
   mpq_build_each_file(proj, note, &s);
   TEST_ASSERT_EQUAL_STRING("T:Act1/Pal.DT1@G;T:Act1/Town/Mine.ds1@G;T:Act2/Deep/Walls.dt1@D;"
                            "X:Armor.txt@D;X:Misc.TXT@G;", s.log);
}

void test_folders_are_found_in_any_case(void)
{
   SEEN_S s;

   put("proj/DATA/GLOBAL/TILES/Act1/X.ds1", "x");
   put("proj/GLOBAL/EXCEL/Misc.txt", "x");
   memset(&s, 0, sizeof(s));
   mpq_build_each_file(proj, note, &s);
   TEST_ASSERT_EQUAL_STRING("T:Act1/X.ds1@D;X:Misc.txt@G;", s.log);
}

void test_data_wins_over_global_for_the_same_path(void)
{
   SEEN_S s;

   put("proj/Global/Tiles/Act1/Same.ds1", "global");
   put("proj/Data/Global/Tiles/ACT1/same.DS1", "data");
   put("proj/Global/Excel/Foo.txt", "global");
   put("proj/Data/Global/Excel/foo.TXT", "data");
   memset(&s, 0, sizeof(s));
   mpq_build_each_file(proj, note, &s);
   TEST_ASSERT_EQUAL_STRING("T:ACT1/same.DS1@D;X:foo.TXT@D;", s.log);
}

void test_left_out_files(void)
{
   SEEN_S s;

   put("proj/project.ini", "x");
   put("proj/backup/2026_x/Global/Tiles/Old.ds1", "x");
   put("proj/build/Global/Tiles/Built.ds1", "x");
   put("proj/Global/Tiles/Keep.ds1", "x");
   put("proj/Global/Tiles/Keep.ds1.tmp", "x");
   put("proj/Global/Tiles/Keep.ds1.staging", "x");
   put("proj/Global/Tiles/Scratch.tmp", "x");
   put("proj/Global/Excel/Misc.txt.old", "x");
   put("proj/Global/Excel/Armor.txt.tmp", "x");
   memset(&s, 0, sizeof(s));
   mpq_build_each_file(proj, note, &s);
   TEST_ASSERT_EQUAL_STRING("T:Keep.ds1@G;", s.log);
}

void test_a_numbered_backup_is_left_out_unless_a_table_lists_it(void)
{
   SEEN_S s;

   put("proj/Global/Tiles/m/map.ds1", "x");
   put("proj/Global/Tiles/m/map-001.ds1", "x");
   put("proj/Global/Tiles/m/MAP-002.DS1", "x");
   put("proj/Global/Tiles/m/lonely-001.ds1", "x");
   put("proj/Global/Tiles/m/listed.ds1", "x");
   put("proj/Global/Tiles/m/listed-001.ds1", "x");
   put("proj/Global/Tiles/m/odd-01.ds1", "x");
   put("proj/Global/Tiles/m/odd.ds1", "x");
   put("proj/Global/Tiles/other/map-003.ds1", "x");
   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "L\t7\t1\tm\\Listed-001.DS1\t0\t0\t0\t0\t0\r\n");
   memset(&s, 0, sizeof(s));
   mpq_build_each_file(proj, note, &s);
   TEST_ASSERT_EQUAL_STRING("T:m/listed-001.ds1@G;T:m/listed.ds1@G;T:m/lonely-001.ds1@G;"
                            "T:m/map.ds1@G;T:m/odd-01.ds1@G;T:m/odd.ds1@G;"
                            "T:other/map-003.ds1@G;X:LvlPrest.txt@G;", s.log);
}

void test_archive_names_use_backslashes_and_keep_case(void)
{
   char * listing;

   put("proj/Global/Tiles/Act1/Sub/Pal.DT1", "pal");
   put("proj/Data/Global/Tiles/Act1/Town/Mine.ds1", "mine");
   put("proj/Global/Tiles/Act1/Town/Mine.ds1", "global mine");
   put("proj/Global/Tiles/Act1/Town/Mine-001.ds1", "backup");
   put("proj/Global/Excel/Misc.txt", "table");
   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "M\t7\t1\tAct1\\Town\\Mine.ds1\t0\t0\t0\t0\t0\r\n");
   compile("LvlPrest");
   compile("Misc");
   test_copy_patch();
   expect_ok();
   expect_in_archive("data\\global\\tiles\\Act1\\Sub\\Pal.DT1", "pal");
   expect_in_archive("data\\global\\tiles\\Act1\\Town\\Mine.ds1", "mine");
   expect_in_archive("data\\global\\excel\\Misc.bin", "compiled");
   expect_not_in_archive("data\\global\\tiles\\Act1\\Town\\Mine-001.ds1");
   listing = read_text("(listfile)");
   TEST_ASSERT_NOT_NULL(strstr(listing, "data\\global\\tiles\\Act1\\Sub\\Pal.DT1\r\n"));
   TEST_ASSERT_NOT_NULL(strstr(listing, "data\\global\\excel\\Misc.txt\r\n"));
   TEST_ASSERT_NULL(strchr(listing, '/'));
   free(listing);
}

void test_a_name_over_200_characters_or_not_plain_ascii_is_not_ok(void)
{
   char big[260];
   int i;

   TEST_ASSERT_TRUE(mpq_build_name_ok("data\\global\\tiles\\Act1\\Town.ds1"));
   memset(big, 'a', sizeof(big));
   memcpy(big, "data\\global\\tiles\\", 18);
   big[200] = '\0';
   TEST_ASSERT_TRUE(mpq_build_name_ok(big));
   big[200] = 'a';
   big[201] = '\0';
   TEST_ASSERT_FALSE(mpq_build_name_ok(big));
   TEST_ASSERT_FALSE(mpq_build_name_ok("data\\global\\tiles\\caf\xe9.ds1"));
   TEST_ASSERT_FALSE(mpq_build_name_ok("data\\global\\tiles\\a\x01.ds1"));
   TEST_ASSERT_FALSE(mpq_build_name_ok("data\\global\\tiles\\a\x7f.ds1"));
   for (i = 0x20; i < 0x7f; i++)
   {
      char n[4];

      n[0] = 'a';
      n[1] = (char) i;
      n[2] = '\0';
      TEST_ASSERT_TRUE(mpq_build_name_ok(n));
   }
}

void test_a_file_whose_name_the_archive_cannot_take_is_a_problem(void)
{
   char path[768];
   FILE * f;
   int rc;

   snprintf(path, sizeof(path), "%s/Global/Tiles/Act1/caf\xe9.dt1", proj);
   TEST_ASSERT_TRUE(project_ensure_parent_dirs(path));
   f = fopen(path, "wb");
   if (f == NULL)
      TEST_IGNORE_MESSAGE("this filesystem refuses a name that isn't plain ASCII");
   fclose(f);
   rc = run();
   remove(path);   /* the test folder's cleanup doesn't know this name */
   TEST_ASSERT_EQUAL_INT(-1, rc);
   TEST_ASSERT_NOT_NULL_MESSAGE(strstr(problems, "Act1/caf?.dt1"), problems);
   TEST_ASSERT_NOT_NULL(strstr(problems, "plain"));
   TEST_ASSERT_FALSE(exists("proj/build/patch_d2.mpq"));
}

void test_added_cells_must_be_in_the_project_or_the_game(void)
{
   put("proj/Global/Tiles/Act1/Town/Mine.ds1", "mine");
   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "Town\t1\t3\tAct1\\Town\\TownN1.ds1\tAct1\\Town\\TownE1.ds1\t"
                 "Act1\\Town\\Mine.ds1\t0\t0\t0\r\n");
   compile("LvlPrest");
   test_copy_patch();
   expect_ok();

   /* A map of the game's own MPQs that the project doesn't have. */
   drop("proj/Global/Tiles/Act1/Town/Mine.ds1");
   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "Town\t1\t3\tAct1\\Town\\TownN1.ds1\tAct1\\Town\\TownE1.ds1\t"
                 "Act1\\Old\\Town.ds1\t0\t0\t0\r\n");
   compile("LvlPrest");
   expect_ok();

   /* Neither. */
   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "Town\t1\t3\tAct1\\Town\\TownN1.ds1\tAct1\\Town\\TownE1.ds1\t"
                 "Act1\\Town\\Ghost.ds1\t0\t0\t0\r\n");
   compile("LvlPrest");
   expect_problem("Act1\\Town\\Ghost.ds1");
   TEST_ASSERT_NOT_NULL(strstr(problems, "Def 1"));

   /* A game map added to another Def's row is added there too, and fine. */
   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "Two\t2\t2\ta.ds1\tAct1\\Town\\TownN1.ds1\t0\t0\t0\t0\r\n");
   compile("LvlPrest");
   expect_ok();
}

/* "Act1\Town\" and then letters to make the whole cell this long. */
static void cell_of_length(char * cell, int n)
{
   int pre = (int) strlen("Act1\\Town\\");

   strcpy(cell, "Act1\\Town\\");
   memset(cell + pre, 'A', (size_t) (n - pre - 4));
   strcpy(cell + n - 4, ".ds1");
}

static void table_with_cell(char * table, const char * cell)
{
   sprintf(table, "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                  "Town\t1\t3\tAct1\\Town\\TownN1.ds1\tAct1\\Town\\TownE1.ds1\t%s\t0\t0\t0\r\n",
           cell);
}

void test_an_added_cell_longer_than_the_game_takes_is_a_problem(void)
{
   /* 41 characters is the most the game takes. */
   char fits[64], longer[64], table[400], rel[128];

   cell_of_length(fits, 41);
   cell_of_length(longer, 42);

   sprintf(rel, "proj/Global/Tiles/Act1/Town/%s", fits + 10);
   put(rel, "x");
   table_with_cell(table, fits);
   project_table(table);
   compile("LvlPrest");
   test_copy_patch();
   expect_ok();
   drop(rel);

   sprintf(rel, "proj/Global/Tiles/Act1/Town/%s", longer + 10);
   put(rel, "x");
   table_with_cell(table, longer);
   project_table(table);
   compile("LvlPrest");
   expect_problem(longer);
   TEST_ASSERT_NOT_NULL(strstr(problems, "Def 1"));
   TEST_ASSERT_NOT_NULL(strstr(problems, "41"));
}

void test_a_hole_the_game_does_not_have_is_a_problem_naming_def_and_cell(void)
{
   /* Def 915 ships with a hole at File2: a table that keeps it is fine. */
   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "Shipped\t915\t3\tAct1\\H1.ds1\t0\tAct1\\H3.ds1\t0\t0\t0\r\n"
                 "Two\t2\t2\ta.ds1\tb.ds1\t0\t0\t0\t0\r\n");
   compile("LvlPrest");
   test_copy_patch();
   expect_ok();

   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "Shipped\t915\t3\tAct1\\H1.ds1\t0\tAct1\\H3.ds1\t0\t0\t0\r\n"
                 "Two\t2\t2\ta.ds1\t0\t0\t0\t0\t0\r\n");
   compile("LvlPrest");
   expect_problem("Def 2");
   TEST_ASSERT_NOT_NULL(strstr(problems, "File2"));
   TEST_ASSERT_NULL(strstr(problems, "Def 915"));

   /* A row the game doesn't have has no hole to share. */
   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "Shipped\t915\t3\tAct1\\H1.ds1\t0\tAct1\\H3.ds1\t0\t0\t0\r\n"
                 "New\t900\t2\ta.ds1\t\t0\t0\t0\t0\r\n");
   compile("LvlPrest");
   expect_problem("Def 900");

   /* A second hole beside the shipped one is still a new hole. */
   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "Shipped\t915\t3\t0\t0\tAct1\\H3.ds1\t0\t0\t0\r\n");
   compile("LvlPrest");
   expect_problem("Def 915");
   TEST_ASSERT_NOT_NULL(strstr(problems, "File1"));
}

void test_a_map_the_game_cannot_reach_is_a_problem(void)
{
   put("proj/Global/Tiles/Act9/New.ds1", "x");
   expect_problem("Act9/New.ds1 isn't in LvlPrest.txt, so the game can't reach it.");

   /* In a File cell of the project's table, of any Def and in any case. */
   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "Two\t2\t2\ta.ds1\tact9\\NEW.ds1\t0\t0\t0\t0\r\n");
   compile("LvlPrest");
   test_copy_patch();
   expect_ok();

   /* Or in the game's MPQs, whatever the table says. */
   drop("proj/Global/Tiles/Act9/New.ds1");
   put("proj/Data/Global/Tiles/Act1/Old/Town.ds1", "edited");
   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "Two\t2\t2\ta.ds1\tb.ds1\t0\t0\t0\t0\r\n");
   compile("LvlPrest");
   expect_ok();
}

void test_a_table_the_game_has_not_compiled_since_it_changed_is_a_problem(void)
{
   const char * msg = "Misc.txt has changed since the game last compiled it. "
                      "Run Build & Test and start a game first.";

   put("proj/Global/Excel/Misc.txt", "one");
   compile("Misc");
   test_copy_patch();
   expect_ok();
   expect_in_archive("data\\global\\excel\\Misc.bin", "compiled");

   put("proj/Global/Excel/Misc.txt", "two");   /* edited since */
   expect_problem(msg);

   put("proj/Global/Excel/Misc.txt", "one");
   put("test/DATA/GLOBAL/EXCEL/Misc.txt", "one, but longer");
   expect_problem(msg);

   /* Same text, no .bin. */
   put("test/DATA/GLOBAL/EXCEL/Misc.txt", "one");
   drop("test/DATA/GLOBAL/EXCEL/Misc.bin");
   expect_problem(msg);

   /* No copy of the table at all. */
   put("test/DATA/GLOBAL/EXCEL/Misc.bin", "compiled");
   drop("test/DATA/GLOBAL/EXCEL/Misc.txt");
   expect_problem(msg);
}

void test_the_test_copy_is_read_in_any_case(void)
{
   put("proj/Global/Excel/Misc.txt", "one");
   put("test/data/Global/EXCEL/MISC.TXT", "one");
   put("test/data/Global/EXCEL/misc.BIN", "lower");
   test_copy_patch();
   expect_ok();
   expect_in_archive("data\\global\\excel\\Misc.bin", "lower");
}

void test_with_no_test_folder_a_table_is_a_problem_and_a_map_is_not(void)
{
   MPQ_BUILD_RESULT_S res;

   put("proj/Data/Global/Tiles/Act1/Old/Town.ds1", "edited");
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, run_with("", &res), problems);
   put("proj/Global/Excel/Misc.txt", "one");
   TEST_ASSERT_EQUAL_INT(-1, run_with("", &res));
   TEST_ASSERT_NOT_NULL(strstr(problems, "Misc.txt has changed since the game last compiled it. "
                                         "Run Build & Test and start a game first."));
}

void test_compiled_tables_need_the_same_game_version(void)
{
   const char * msg = "The test copy is a different game version from the install, "
                      "so its compiled tables can't be used.";
   char path[768];
   unsigned char * data;
   size_t len;

   put("proj/Global/Excel/Misc.txt", "one");
   compile("Misc");
   put("test/patch_d2.mpq", "not the same size as the base");
   expect_problem(msg);

   test_copy_patch();
   expect_ok();

   /* The base's bytes and one more. */
   snprintf(path, sizeof(path), "%s/patch_d2.mpq", test_copy);
   data = tt_read_file(path, &len);
   TEST_ASSERT_NOT_NULL(data);
   data = (unsigned char *) realloc(data, len + 1);
   TEST_ASSERT_NOT_NULL(data);
   data[len] = 0;
   TEST_ASSERT_TRUE(tt_write_file(path, data, len + 1));
   free(data);
   expect_problem(msg);

   remove(path);
   expect_problem(msg);
}

void test_a_test_copy_of_another_version_does_not_matter_when_no_table_is_packed(void)
{
   put("proj/Data/Global/Tiles/Act1/Old/Town.ds1", "edited");
   put("test/patch_d2.mpq", "another game version");
   expect_ok();
}

void test_only_changed_maps_need_no_test_copy(void)
{
   MPQ_BUILD_RESULT_S res;

   put("proj/Data/Global/Tiles/Act1/Town/TownN1.ds1", "edited");
   put("proj/Global/Tiles/a.dt1", "edited");
   /* no test folder at all, no patch_d2.mpq beside it, no tables in the project */
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, run_with("", &res), problems);
   TEST_ASSERT_EQUAL_INT(2, res.files);
   TEST_ASSERT_TRUE(res.bytes > 0);
   expect_in_archive("data\\global\\tiles\\Act1\\Town\\TownN1.ds1", "edited");
   expect_in_archive("data\\global\\tiles\\a.dt1", "edited");
   TEST_ASSERT_EQUAL_STRING("", problems);
}

void test_a_missing_base_is_a_problem(void)
{
   put("proj/Global/Tiles/a.dt1", "x");
   drop("install/patch_d2.mpq");
   expect_problem("The game folder has no patch_d2.mpq to build on.");
}

void test_the_listfile_names_the_project_files_and_leaves_no_temp_file(void)
{
   char * listing;
   char dirpath[768];

   put("proj/Global/Tiles/Act1/Town/B.ds1", "x");
   put("proj/Global/Tiles/Act1/Town/A.dt1", "x");
   put("proj/Global/Excel/Misc.txt", "x");
   project_table("Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\r\n"
                 "B\t3\t1\tAct1\\Town\\B.ds1\t0\t0\t0\t0\t0\r\n");
   compile("LvlPrest");
   compile("Misc");
   test_copy_patch();
   expect_ok();
   listing = read_text("(listfile)");
   TEST_ASSERT_EQUAL_STRING("data\\global\\tiles\\Act1\\Town\\A.dt1\r\n"
                            "data\\global\\tiles\\Act1\\Town\\B.ds1\r\n"
                            "data\\global\\excel\\LvlPrest.txt\r\n"
                            "data\\global\\excel\\Misc.txt\r\n"
                            "data\\global\\excel\\LvlPrest.bin\r\n"
                            "data\\global\\excel\\Misc.bin", listing);
   free(listing);
   snprintf(dirpath, sizeof(dirpath), "%s/build", proj);
   TEST_ASSERT_EQUAL_INT(1, tt_count_entries(dirpath));
}

void test_base_entries_survive(void)
{
   put("proj/Global/Tiles/a.dt1", "mine");
   expect_ok();
   expect_in_archive("data\\global\\excel\\base_only.txt", "only in the base");
   expect_in_archive("data\\global\\tiles\\act1\\town\\townn1.ds1", "the game's town");
   expect_in_archive("data\\global\\tiles\\a.dt1", "mine");
}

void test_a_project_file_replaces_the_base_entry_of_its_name(void)
{
   put("proj/Data/Global/Tiles/Act1/Town/TownN1.ds1", "edited");
   expect_ok();
   expect_in_archive("data\\global\\tiles\\act1\\town\\townn1.ds1", "edited");
   expect_in_archive("data\\global\\excel\\base_only.txt", "only in the base");
}

void test_problems_write_nothing(void)
{
   unsigned char * before, * after;
   size_t before_len, after_len;

   put("proj/Global/Tiles/a.dt1", "x");
   expect_ok();
   before = tt_read_file(out, &before_len);
   TEST_ASSERT_NOT_NULL(before);

   put("proj/Global/Tiles/Act9/New.ds1", "x");   /* not in any table */
   TEST_ASSERT_EQUAL_INT(-1, run());
   after = tt_read_file(out, &after_len);
   TEST_ASSERT_NOT_NULL(after);
   TEST_ASSERT_EQUAL_INT((int) before_len, (int) after_len);
   TEST_ASSERT_EQUAL_MEMORY(before, after, before_len);
   free(before);
   free(after);
   TEST_ASSERT_FALSE(exists("proj/build/patch_d2.mpq.staging"));
}

void test_a_problem_does_not_even_make_the_build_folder(void)
{
   put("proj/Global/Tiles/Act9/New.ds1", "x");
   TEST_ASSERT_EQUAL_INT(-1, run());
   TEST_ASSERT_FALSE(exists("proj/build"));
}

void test_lists_every_problem(void)
{
   const char * p;
   int lines = 0;

   put("proj/Global/Tiles/Act9/One.ds1", "x");
   put("proj/Global/Tiles/Act9/Two.ds1", "x");
   put("proj/Global/Excel/Misc.txt", "x");
   drop("install/patch_d2.mpq");
   TEST_ASSERT_EQUAL_INT(-1, run());
   TEST_ASSERT_NOT_NULL(strstr(problems, "Act9/One.ds1 isn't in LvlPrest.txt"));
   TEST_ASSERT_NOT_NULL(strstr(problems, "Act9/Two.ds1 isn't in LvlPrest.txt"));
   TEST_ASSERT_NOT_NULL(strstr(problems, "Misc.txt has changed"));
   TEST_ASSERT_NOT_NULL(strstr(problems, "The game folder has no patch_d2.mpq"));
   /* one to a line */
   for (p = problems; *p != '\0'; p++)
      if (*p == '\n')
         lines++;
   TEST_ASSERT_EQUAL_INT(3, lines);
   TEST_ASSERT_NULL(strchr(problems, '\r'));
}

void test_a_project_with_nothing_to_pack_is_a_problem(void)
{
   expect_problem("nothing to pack");
}

void test_the_output_cannot_be_the_base(void)
{
   MPQ_BUILD_ARGS_S a;
   MPQ_BUILD_RESULT_S res;
   unsigned char * before, * after;
   size_t bl, al;

   put("proj/Global/Tiles/a.dt1", "x");
   before = tt_read_file(base, &bl);
   TEST_ASSERT_NOT_NULL(before);
   memset(&a, 0, sizeof(a));
   a.project_dir = proj;
   a.base_mpq = base;
   a.test_game_dir = "";
   a.out_mpq = base;
   a.game_table = GAME;
   a.in_game = in_game;
   TEST_ASSERT_EQUAL_INT(-1, mpq_build_run(&a, &res, problems, sizeof(problems)));
   TEST_ASSERT_NOT_NULL(strstr(problems, "patch_d2.mpq"));
   after = tt_read_file(base, &al);
   TEST_ASSERT_NOT_NULL(after);
   TEST_ASSERT_EQUAL_INT((int) bl, (int) al);
   TEST_ASSERT_EQUAL_MEMORY(before, after, bl);
   free(before);
   free(after);
}

void test_problems_that_do_not_fit_are_cut_short_and_terminated(void)
{
   MPQ_BUILD_ARGS_S a;
   MPQ_BUILD_RESULT_S res;
   char small[40];

   put("proj/Global/Tiles/Act9/One.ds1", "x");
   put("proj/Global/Tiles/Act9/Two.ds1", "x");
   memset(&a, 0, sizeof(a));
   a.project_dir = proj;
   a.base_mpq = base;
   a.test_game_dir = "";
   a.out_mpq = out;
   a.game_table = GAME;
   a.in_game = in_game;
   TEST_ASSERT_EQUAL_INT(-1, mpq_build_run(&a, &res, small, sizeof(small)));
   TEST_ASSERT_TRUE(strlen(small) < sizeof(small));
   TEST_ASSERT_TRUE(strlen(small) > 0);
}

static void make_old(const char * rel)
{
   char path[768];
   struct TIMES_S t;

   path_of(path, rel);
   t.actime = 1000000000;
   t.modtime = 1000000000;
   TEST_ASSERT_EQUAL_INT(0, set_times(path, &t));
}

void test_an_empty_or_older_bin_is_not_a_compiled_table(void)
{
   const char * msg = "Misc.txt has changed since the game last compiled it. "
                      "Run Build & Test and start a game first.";

   put("proj/Global/Excel/Misc.txt", "one");
   compile("Misc");
   test_copy_patch();
   expect_ok();

   put("test/DATA/GLOBAL/EXCEL/Misc.bin", "");
   expect_problem(msg);

   /* The same text, but the .bin predates it. */
   put("test/DATA/GLOBAL/EXCEL/Misc.bin", "compiled");
   make_old("test/DATA/GLOBAL/EXCEL/Misc.bin");
   expect_problem(msg);

   /* The .bin as new as the text is fine. */
   put("test/DATA/GLOBAL/EXCEL/Misc.bin", "compiled");
   expect_ok();
}

#define SIZE_HEAD "Name\tDef\tFiles\tFile1\tFile2\tFile3\tFile4\tFile5\tFile6\tSizeX\tSizeY\r\n"
#define MINE "Act1\\Town\\Mine.ds1"

/* One row, Def 900, which the game's table doesn't have, with its size as stored. */
static void sized_table(int w_stored, int h_stored)
{
   char t[600];

   sprintf(t, SIZE_HEAD "Own\t900\t1\t" MINE "\t0\t0\t0\t0\t0\t%d\t%d\r\n", w_stored, h_stored);
   project_table(t);
   compile("LvlPrest");
   test_copy_patch();
}

static void make_ds1(const char * rel, int width, int height)
{
   char path[768];

   path_of(path, rel);
   TEST_ASSERT_EQUAL_INT(0, map_ops_write_empty_ds1(path, width, height, 1));
}

void test_a_map_must_be_the_size_of_its_row(void)
{
   make_ds1("proj/Global/Tiles/Act1/Town/Mine.ds1", 5, 5);
   sized_table(7, 9);   /* 8 by 10 */
   expect_problem("Act1/Town/Mine.ds1 is 5 by 5, but Def 900's maps are 8 by 10. "
                  "The game can't use it.");
   TEST_ASSERT_FALSE(exists("proj/build"));

   make_ds1("proj/Global/Tiles/Act1/Town/Mine.ds1", 8, 10);
   expect_ok();

   /* A row with one side set holds the map to that side only. */
   sized_table(7, 0);
   expect_ok();
   make_ds1("proj/Global/Tiles/Act1/Town/Mine.ds1", 9, 3);
   expect_problem("is 9 by 3, but Def 900's maps are 8 wide.");
}

void test_a_row_with_no_size_takes_any_map(void)
{
   make_ds1("proj/Global/Tiles/Act1/Town/Mine.ds1", 5, 5);
   sized_table(0, 0);
   expect_ok();
}

void test_a_map_shared_by_rows_is_checked_against_each(void)
{
   make_ds1("proj/Global/Tiles/Act1/Town/Mine.ds1", 8, 10);
   put("proj/Global/Excel/LvlPrest.txt",
       SIZE_HEAD
       "A\t900\t1\t" MINE "\t0\t0\t0\t0\t0\t7\t9\r\n"
       "B\t901\t1\t" MINE "\t0\t0\t0\t0\t0\t4\t4\r\n");
   compile("LvlPrest");
   test_copy_patch();
   expect_problem("Def 901's maps are 5 by 5");
   TEST_ASSERT_NULL(strstr(problems, "Def 900"));
}

void test_a_map_too_short_for_a_header_is_a_problem(void)
{
   put("proj/Global/Tiles/Act1/Town/Mine.ds1", "short");
   sized_table(7, 9);
   expect_problem("Act1/Town/Mine.ds1 is too short to be a map");
}

void test_a_map_of_a_row_only_the_game_lists_is_not_checked(void)
{
   MPQ_BUILD_ARGS_S a;
   MPQ_BUILD_RESULT_S res;
   static char game[] =
      SIZE_HEAD "Town\t1\t1\tAct1\\Town\\TownN1.ds1\t0\t0\t0\t0\t0\t7\t9\r\n";

   make_ds1("proj/Data/Global/Tiles/Act1/Town/TownN1.ds1", 3, 3);
   memset(&a, 0, sizeof(a));
   a.project_dir = proj;
   a.base_mpq = base;
   a.test_game_dir = "";
   a.out_mpq = out;
   a.game_table = game;
   a.in_game = in_game;
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, mpq_build_run(&a, &res, problems, sizeof(problems)),
                                 problems);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_an_empty_or_older_bin_is_not_a_compiled_table);
   RUN_TEST(test_a_map_must_be_the_size_of_its_row);
   RUN_TEST(test_a_row_with_no_size_takes_any_map);
   RUN_TEST(test_a_map_shared_by_rows_is_checked_against_each);
   RUN_TEST(test_a_map_too_short_for_a_header_is_a_problem);
   RUN_TEST(test_a_map_of_a_row_only_the_game_lists_is_not_checked);
   RUN_TEST(test_files_taken_by_extension_folder_and_case);
   RUN_TEST(test_folders_are_found_in_any_case);
   RUN_TEST(test_data_wins_over_global_for_the_same_path);
   RUN_TEST(test_left_out_files);
   RUN_TEST(test_a_numbered_backup_is_left_out_unless_a_table_lists_it);
   RUN_TEST(test_archive_names_use_backslashes_and_keep_case);
   RUN_TEST(test_a_name_over_200_characters_or_not_plain_ascii_is_not_ok);
   RUN_TEST(test_a_file_whose_name_the_archive_cannot_take_is_a_problem);
   RUN_TEST(test_added_cells_must_be_in_the_project_or_the_game);
   RUN_TEST(test_an_added_cell_longer_than_the_game_takes_is_a_problem);
   RUN_TEST(test_a_hole_the_game_does_not_have_is_a_problem_naming_def_and_cell);
   RUN_TEST(test_a_map_the_game_cannot_reach_is_a_problem);
   RUN_TEST(test_a_table_the_game_has_not_compiled_since_it_changed_is_a_problem);
   RUN_TEST(test_the_test_copy_is_read_in_any_case);
   RUN_TEST(test_with_no_test_folder_a_table_is_a_problem_and_a_map_is_not);
   RUN_TEST(test_compiled_tables_need_the_same_game_version);
   RUN_TEST(test_a_test_copy_of_another_version_does_not_matter_when_no_table_is_packed);
   RUN_TEST(test_only_changed_maps_need_no_test_copy);
   RUN_TEST(test_a_missing_base_is_a_problem);
   RUN_TEST(test_the_listfile_names_the_project_files_and_leaves_no_temp_file);
   RUN_TEST(test_base_entries_survive);
   RUN_TEST(test_a_project_file_replaces_the_base_entry_of_its_name);
   RUN_TEST(test_problems_write_nothing);
   RUN_TEST(test_a_problem_does_not_even_make_the_build_folder);
   RUN_TEST(test_lists_every_problem);
   RUN_TEST(test_a_project_with_nothing_to_pack_is_a_problem);
   RUN_TEST(test_the_output_cannot_be_the_base);
   RUN_TEST(test_problems_that_do_not_fit_are_cut_short_and_terminated);
   return UNITY_END();
}
