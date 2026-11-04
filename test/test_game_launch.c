/* game_profile_check, game_install and game_start over temp folders. game_start runs
 * only fake_game. Nothing here touches a game install. */
#ifndef _WIN32
#ifndef _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef WIN32
#include <direct.h>
#include <sys/utime.h>
#include <share.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#include <utime.h>
#endif
#include <allegro5/allegro.h>

#include "unity/unity.h"
#include "structs.h"
#include "core/file_copy.h"
#include "core/game_launch.h"
#include "core/mpq_build.h"
#include "core/project.h"
#include "support/test_tmp.h"

CONFIG_S glb_config;

#define ARGS_REFUSAL \
   "The test arguments need -direct and -txt, or the game ignores the project."

static char root[512];
static char game[640];
static char err[512];
static char cwd[512];
static char abs_root[1100];

void setUp(void)
{
   char exe[700];
   size_t n;

   TEST_ASSERT_TRUE(tt_root_create("game_launch", root, sizeof(root)));
   n = strlen(root);
   if (n > 0 && (root[n - 1] == '/' || root[n - 1] == '\\')) root[n - 1] = 0;
   /* The profile wants full paths, and tt_root_create gives a relative one. */
   TEST_ASSERT_NOT_NULL(getcwd(cwd, sizeof(cwd)));
   snprintf(abs_root, sizeof(abs_root), "%s/%s", cwd, root);
   snprintf(game, sizeof(game), "%s/game", abs_root);
   TEST_ASSERT_TRUE(tt_mkdir(game));
   snprintf(exe, sizeof(exe), "%s/Game.exe", game);
   TEST_ASSERT_TRUE(tt_write_file(exe, "", 0));
   err[0] = 0;
}

void tearDown(void)
{
   tt_remove_tree(root);
}

static GAME_PROFILE_S profile(const char *dir, const char *exe, const char *args)
{
   GAME_PROFILE_S p;

   p.game_dir = dir;
   p.exe      = exe;
   p.args     = args;
   p.wrapper  = "";
   return p;
}

static int check(const GAME_PROFILE_S *p, const char *const *installs, int n)
{
   err[0] = 0;
   return game_profile_check(p, installs, n, err, sizeof(err));
}

void test_a_complete_profile_is_accepted(void)
{
   GAME_PROFILE_S p = profile(game, "Game.exe", "-w -direct -txt");

   TEST_ASSERT_EQUAL_INT(0, check(&p, NULL, 0));
   TEST_ASSERT_EQUAL_STRING("", err);
}

void test_an_empty_game_folder_says_to_set_the_test_folder(void)
{
   GAME_PROFILE_S p = profile("", "Game.exe", "-direct -txt");

   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   TEST_ASSERT_NOT_NULL(strstr(err, "test folder"));
   p.game_dir = NULL;
   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   TEST_ASSERT_NOT_NULL(strstr(err, "test folder"));
}

void test_an_exe_missing_from_the_folder_is_named(void)
{
   GAME_PROFILE_S p = profile(game, "Other.exe", "-direct -txt");

   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   TEST_ASSERT_NOT_NULL(strstr(err, "Other.exe"));
}

void test_an_exe_with_a_parent_reference_or_rooted_is_refused(void)
{
   GAME_PROFILE_S p = profile(game, "../Game.exe", "-direct -txt");

   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   p.exe = "..\\Game.exe";
   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   p.exe = "sub/../Game.exe";
   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   p.exe = "/bin/sh";
   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   p.exe = "C:\\Windows\\notepad.exe";
   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   p.exe = "";
   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   TEST_ASSERT_NOT_EQUAL(0, (int) strlen(err));
}

void test_the_game_folder_may_not_be_an_install(void)
{
   GAME_PROFILE_S p = profile(game, "Game.exe", "-direct -txt");
   const char *installs[2];

   installs[0] = game;
   TEST_ASSERT_EQUAL_INT(-1, check(&p, installs, 1));
   TEST_ASSERT_NOT_NULL(strstr(err, "install"));

   /* Empty and NULL entries are skipped. */
   installs[0] = "";
   installs[1] = NULL;
   TEST_ASSERT_EQUAL_INT(0, check(&p, installs, 2));
}

void test_the_game_folder_in_another_case_and_slash_style_is_still_an_install(void)
{
#ifdef WIN32
   GAME_PROFILE_S p = profile(game, "Game.exe", "-direct -txt");
   char other[640];
   const char *installs[1];
   size_t i;

   snprintf(other, sizeof(other), "%s", game);
   for (i = 0; other[i] != 0; i++)
   {
      if (other[i] == '/') other[i] = '\\';
      else if (other[i] >= 'a' && other[i] <= 'z') other[i] = (char) (other[i] - 32);
   }
   installs[0] = other;
   TEST_ASSERT_EQUAL_INT(-1, check(&p, installs, 1));
#else
   TEST_IGNORE_MESSAGE("case-insensitive paths are a Windows rule");
#endif
}

void test_a_game_folder_inside_an_install_is_refused(void)
{
   GAME_PROFILE_S p = profile(game, "Game.exe", "-direct -txt");
   const char *installs[1];

   installs[0] = abs_root;
   TEST_ASSERT_EQUAL_INT(-1, check(&p, installs, 1));
   TEST_ASSERT_NOT_NULL(strstr(err, "install"));
}

void test_a_relative_folder_or_one_with_a_parent_reference_is_refused(void)
{
   GAME_PROFILE_S p = profile(".", "Game.exe", "-direct -txt");
   char dotted[1300];
   const char *installs[1];

   installs[0] = game;
   TEST_ASSERT_EQUAL_INT(-1, check(&p, installs, 1));
   TEST_ASSERT_EQUAL_STRING("Choose the test folder by its full path.", err);
   p.game_dir = "..";
   TEST_ASSERT_EQUAL_INT(-1, check(&p, installs, 1));
   p.game_dir = "sub/x";
   TEST_ASSERT_EQUAL_INT(-1, check(&p, installs, 1));
   TEST_ASSERT_EQUAL_STRING("Choose the test folder by its full path.", err);
   p.game_dir = "..\\game";
   TEST_ASSERT_EQUAL_INT(-1, check(&p, installs, 1));

   /* Rooted, but it steps out and back into the install. */
   snprintf(dotted, sizeof(dotted), "%s/other/../game", abs_root);
   p.game_dir = dotted;
   TEST_ASSERT_EQUAL_INT(-1, check(&p, installs, 1));
   TEST_ASSERT_EQUAL_STRING("Choose the test folder by its full path.", err);
   /* Nothing was written. */
   TEST_ASSERT_EQUAL_INT(1, tt_count_entries(game));
}

void test_a_link_to_an_install_is_refused(void)
{
   GAME_PROFILE_S p;
   char link[1300], cmd[2800];
   const char *installs[1];
   int rc;

   snprintf(link, sizeof(link), "%s/link", abs_root);
#ifdef WIN32
   {
      size_t i;
      char a[1300], b[1300];

      snprintf(a, sizeof(a), "%s", link);
      snprintf(b, sizeof(b), "%s", game);
      for (i = 0; a[i]; i++) if (a[i] == '/') a[i] = '\\';
      for (i = 0; b[i]; i++) if (b[i] == '/') b[i] = '\\';
      snprintf(cmd, sizeof(cmd), "cmd /c mklink /J \"%s\" \"%s\" >nul 2>nul", a, b);
   }
#else
   snprintf(cmd, sizeof(cmd), "ln -s \"%s\" \"%s\"", game, link);
#endif
   rc = system(cmd);
   if (rc != 0) TEST_IGNORE_MESSAGE("this system can't make a link here");

   p = profile(link, "Game.exe", "-direct -txt");
   installs[0] = game;
   TEST_ASSERT_EQUAL_INT(-1, check(&p, installs, 1));
   TEST_ASSERT_NOT_NULL(strstr(err, "install"));
   TEST_ASSERT_EQUAL_INT(1, tt_count_entries(game));
}

void test_an_install_that_does_not_exist_falls_back_to_the_text_compare(void)
{
   GAME_PROFILE_S p = profile(game, "Game.exe", "-direct -txt");
   char missing[1300];
   const char *installs[1];

   snprintf(missing, sizeof(missing), "%s/game/nope", abs_root);
   installs[0] = missing;
   TEST_ASSERT_EQUAL_INT(-1, check(&p, installs, 1));   /* inside it, textually */
   snprintf(missing, sizeof(missing), "%s/nope", abs_root);
   TEST_ASSERT_EQUAL_INT(0, check(&p, installs, 1));    /* unrelated and absent */
}

void test_an_install_inside_the_game_folder_is_refused(void)
{
   GAME_PROFILE_S p = profile(game, "Game.exe", "-direct -txt");
   char inner[700];
   const char *installs[1];

   snprintf(inner, sizeof(inner), "%s/d2", game);
   TEST_ASSERT_TRUE(tt_mkdir(inner));
   installs[0] = inner;
   TEST_ASSERT_EQUAL_INT(-1, check(&p, installs, 1));
   TEST_ASSERT_NOT_NULL(strstr(err, "install"));
}

void test_a_sibling_with_the_same_prefix_is_not_an_install(void)
{
   GAME_PROFILE_S p = profile(game, "Game.exe", "-direct -txt");
   char sibling[700];
   const char *installs[1];

   snprintf(sibling, sizeof(sibling), "%s2", game);
   installs[0] = sibling;
   TEST_ASSERT_EQUAL_INT(0, check(&p, installs, 1));
}

void test_args_need_direct_and_txt_as_whole_words_in_any_case(void)
{
   GAME_PROFILE_S p = profile(game, "Game.exe", "-w -txt");

   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   TEST_ASSERT_EQUAL_STRING(ARGS_REFUSAL, err);
   p.args = "-direct";
   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   TEST_ASSERT_EQUAL_STRING(ARGS_REFUSAL, err);
   p.args = "-directx -txtfile";
   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   p.args = "x-direct -txt";
   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   p.args = "";
   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   p.args = NULL;
   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));

   p.args = "-DIRECT -Txt";
   TEST_ASSERT_EQUAL_INT(0, check(&p, NULL, 0));
   p.args = "  -txt\t-direct  -w ";
   TEST_ASSERT_EQUAL_INT(0, check(&p, NULL, 0));
}

void test_a_folder_that_cant_be_written_is_refused(void)
{
#ifdef WIN32
   TEST_IGNORE_MESSAGE("Windows ACLs can't be set up portably here");
#else
   GAME_PROFILE_S p = profile(game, "Game.exe", "-direct -txt");
   FILE *probe;
   char path[700];

   if (geteuid() == 0) TEST_IGNORE_MESSAGE("root can write anywhere");
   TEST_ASSERT_EQUAL_INT(0, chmod(game, 0555));
   snprintf(path, sizeof(path), "%s/probe.tmp", game);
   probe = fopen(path, "wb");
   if (probe != NULL)
   {
      fclose(probe);
      remove(path);
      chmod(game, 0755);
      TEST_IGNORE_MESSAGE("this folder is writable despite chmod");
   }
   TEST_ASSERT_EQUAL_INT(-1, check(&p, NULL, 0));
   TEST_ASSERT_NOT_NULL(strstr(err, "write"));
   chmod(game, 0755);
#endif
}

void test_the_write_check_leaves_nothing_behind(void)
{
   GAME_PROFILE_S p = profile(game, "Game.exe", "-direct -txt");

   TEST_ASSERT_EQUAL_INT(0, check(&p, NULL, 0));
   TEST_ASSERT_EQUAL_INT(1, tt_count_entries(game));   /* only Game.exe */
}

/* ---- game_install ----------------------------------------------------- */

static char proj[1200];
static int  installed;

static void put(const char *rel, const char *text)
{
   char path[1400];

   snprintf(path, sizeof(path), "%s/%s", proj, rel);
   TEST_ASSERT_TRUE(project_ensure_parent_dirs(path));
   TEST_ASSERT_TRUE(tt_write_file(path, text, strlen(text)));
}

static void put_game(const char *rel, const char *text)
{
   char path[1400];

   snprintf(path, sizeof(path), "%s/%s", game, rel);
   TEST_ASSERT_TRUE(project_ensure_parent_dirs(path));
   TEST_ASSERT_TRUE(tt_write_file(path, text, strlen(text)));
}

static int game_has(const char *rel)
{
   char path[1400];

   snprintf(path, sizeof(path), "%s/%s", game, rel);
   return tt_file_exists(path);
}

/* A file as NUL-ended text, or NULL; free it. */
static char *read_text(const char *path)
{
   size_t len;
   unsigned char *data = tt_read_file(path, &len);
   char *text;

   if (data == NULL) return NULL;
   text = (char *) malloc(len + 1);
   TEST_ASSERT_NOT_NULL(text);
   memcpy(text, data, len);
   text[len] = 0;
   free(data);
   return text;
}

/* A file of the game folder as text; free it. */
static char *game_text(const char *rel)
{
   char path[1400];
   char *text;

   snprintf(path, sizeof(path), "%s/%s", game, rel);
   text = read_text(path);
   TEST_ASSERT_NOT_NULL_MESSAGE(text, rel);
   return text;
}

static void expect_game_text(const char *rel, const char *text)
{
   char *got = game_text(rel);

   TEST_ASSERT_EQUAL_STRING(text, got);
   free(got);
}

static int install(void)
{
   err[0] = 0;
   installed = -1;
   return game_install(proj, game, &installed, err, sizeof(err));
}

static void make_project(void)
{
   snprintf(proj, sizeof(proj), "%s/proj", abs_root);
   TEST_ASSERT_TRUE(tt_mkdir(proj));
}

typedef struct { char log[2048]; int n; } SEEN_S;

static void note(const char *source, const char *rel, int is_table, void *ctx)
{
   SEEN_S *s = (SEEN_S *) ctx;

   (void) source;
   snprintf(s->log + strlen(s->log), sizeof(s->log) - strlen(s->log), "%s/%s\n",
            is_table ? "DATA/GLOBAL/EXCEL" : "DATA/GLOBAL/TILES", rel);
   s->n++;
}

void test_tiles_and_tables_land_in_their_folders(void)
{
   make_project();
   put("Global/Tiles/Act1/Town/Mine.ds1", "mine");
   put("Global/Tiles/Act1/Walls.dt1", "walls");
   put("Data/Global/Excel/Misc.txt", "misc");

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   TEST_ASSERT_EQUAL_INT(3, installed);
   expect_game_text("DATA/GLOBAL/TILES/Act1/Town/Mine.ds1", "mine");
   expect_game_text("DATA/GLOBAL/TILES/Act1/Walls.dt1", "walls");
   expect_game_text("DATA/GLOBAL/EXCEL/Misc.txt", "misc");
}

void test_both_folders_are_made_for_a_project_with_no_tables(void)
{
   make_project();
   put("Global/Tiles/Only.ds1", "x");

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   TEST_ASSERT_EQUAL_INT(1, installed);
   TEST_ASSERT_TRUE(game_has("DATA/GLOBAL/EXCEL"));
   TEST_ASSERT_TRUE(game_has("DATA/GLOBAL/TILES"));
}

void test_an_empty_project_still_installs_and_makes_the_folders(void)
{
   make_project();
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   TEST_ASSERT_EQUAL_INT(0, installed);
   TEST_ASSERT_TRUE(game_has("DATA/GLOBAL/EXCEL"));
   TEST_ASSERT_TRUE(game_has("DATA/GLOBAL/TILES"));
}

void test_the_files_are_the_ones_build_mpq_takes(void)
{
   SEEN_S s;
   char *manifest;

   make_project();
   put("Global/Tiles/Act1/Town/Mine.ds1", "m");
   put("Data/Global/Tiles/Act2/Deep/Walls.dt1", "w");
   put("Global/Tiles/readme.md", "x");
   put("Global/Tiles/Act1/Mine.ds1.bak", "x");
   put("Global/Excel/Misc.TXT", "t");
   put("Global/Excel/Misc.bin", "x");
   put("Global/Other/Thing.ds1", "x");
   memset(&s, 0, sizeof(s));
   mpq_build_each_file(proj, note, &s);
   TEST_ASSERT_EQUAL_INT(3, s.n);

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   TEST_ASSERT_EQUAL_INT(s.n, installed);
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/readme.md"));
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/Act1/Mine.ds1.bak"));
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/EXCEL/Misc.bin"));
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/Thing.ds1"));
   /* The manifest names what Build MPQ's callback named, in its order, one to a line,
      relative with forward slashes and no carriage returns. */
   manifest = game_text("ds1edit-installed.txt");
   TEST_ASSERT_EQUAL_STRING(s.log, manifest);
   free(manifest);
}

void test_a_second_install_removes_a_map_that_is_gone(void)
{
   char path[1400];

   make_project();
   put("Global/Tiles/Keep.ds1", "keep");
   put("Global/Tiles/Gone.ds1", "gone");
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   TEST_ASSERT_TRUE(game_has("DATA/GLOBAL/TILES/Gone.ds1"));

   snprintf(path, sizeof(path), "%s/Global/Tiles/Gone.ds1", proj);
   TEST_ASSERT_EQUAL_INT(0, remove(path));
   put("Global/Tiles/Keep.ds1", "keep, changed");
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   TEST_ASSERT_EQUAL_INT(1, installed);
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/Gone.ds1"));
   expect_game_text("DATA/GLOBAL/TILES/Keep.ds1", "keep, changed");
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/Keep.ds1.ds1edit-new"));
   TEST_ASSERT_FALSE(game_has("ds1edit-installed.txt.ds1edit-new"));
}

void test_only_listed_files_are_removed(void)
{
   char manifest[3000], outside[1400];

   make_project();
   put("Global/Tiles/A.ds1", "a");
   put_game("DATA/GLOBAL/TILES/Unrelated.ds1", "not ours");
   put_game("DATA/GLOBAL/EXCEL/Armor.txt", "the game's");
   put_game("victim.txt", "above the folders");
   snprintf(outside, sizeof(outside), "%s/outside.txt", abs_root);
   TEST_ASSERT_TRUE(tt_write_file(outside, "outside", 7));
   snprintf(manifest, sizeof(manifest),
            "DATA/GLOBAL/TILES/../../../victim.txt\n"
            "%s\n"
            "/outside.txt\n"
            "DATA\\GLOBAL\\TILES\\Unrelated.ds1\n"
            "Game.exe\n"
            "DATA/GLOBAL/TILES/Old.ds1\n", outside);
   put_game("ds1edit-installed.txt", manifest);
   put_game("DATA/GLOBAL/TILES/Old.ds1", "ours, from before");

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   expect_game_text("DATA/GLOBAL/TILES/Unrelated.ds1", "not ours");
   expect_game_text("DATA/GLOBAL/EXCEL/Armor.txt", "the game's");
   expect_game_text("victim.txt", "above the folders");
   TEST_ASSERT_TRUE(game_has("Game.exe"));
   TEST_ASSERT_TRUE(tt_file_exists(outside));
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/Old.ds1"));
   expect_game_text("DATA/GLOBAL/TILES/A.ds1", "a");
}

void test_a_tables_bin_is_removed_and_another_bin_is_not(void)
{
   make_project();
   put("Global/Excel/Misc.txt", "t");
   put_game("DATA/GLOBAL/EXCEL/Misc.bin", "stale");
   put_game("DATA/GLOBAL/EXCEL/Armor.bin", "the game's");

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/EXCEL/Misc.bin"));
   expect_game_text("DATA/GLOBAL/EXCEL/Armor.bin", "the game's");
   expect_game_text("DATA/GLOBAL/EXCEL/Misc.txt", "t");
}

void test_the_installed_text_is_not_given_the_sources_old_time(void)
{
   char src[1400], dst[1400];
   struct stat b;
#ifdef WIN32
   struct _utimbuf t;
#else
   struct utimbuf t;
#endif

   make_project();
   put("Global/Excel/Misc.txt", "t");
   snprintf(src, sizeof(src), "%s/Global/Excel/Misc.txt", proj);
   snprintf(dst, sizeof(dst), "%s/DATA/GLOBAL/EXCEL/Misc.txt", game);
   t.actime = t.modtime = 1000000000;   /* 2001 */
#ifdef WIN32
   TEST_ASSERT_EQUAL_INT(0, _utime(src, &t));
#else
   TEST_ASSERT_EQUAL_INT(0, utime(src, &t));
#endif
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   TEST_ASSERT_EQUAL_INT(0, stat(dst, &b));
   TEST_ASSERT_TRUE(b.st_mtime > 1000000000 + 86400);
}

/* Make a file unreadable until unlock_file. 0 if this system can't. */
#ifdef WIN32
static FILE *lock_fp;
static int lock_file(const char *path)
{
   lock_fp = _fsopen(path, "rb", _SH_DENYRW);
   return lock_fp != NULL;
}
static void unlock_file(const char *path) { (void) path; fclose(lock_fp); }
#else
static int lock_file(const char *path)
{
   FILE *fp;

   if (chmod(path, 0) != 0) return 0;
   fp = fopen(path, "rb");
   if (fp != NULL) { fclose(fp); chmod(path, 0644); return 0; }   /* root */
   return 1;
}
static void unlock_file(const char *path) { chmod(path, 0644); }
#endif

void test_a_project_file_that_cant_be_read_changes_nothing(void)
{
   char path[1400], tiles_dir[1400];
   char *old;

   make_project();
   put("Global/Tiles/A.ds1", "new a");
   put("Global/Tiles/Z.ds1", "new z");
   put_game("DATA/GLOBAL/TILES/A.ds1", "old a");
   put_game("ds1edit-installed.txt", "DATA/GLOBAL/TILES/A.ds1\n");
   snprintf(path, sizeof(path), "%s/Global/Tiles/Z.ds1", proj);
   if (!lock_file(path)) TEST_IGNORE_MESSAGE("can't make a file unreadable here");

   TEST_ASSERT_EQUAL_INT(-1, install());
   unlock_file(path);
   TEST_ASSERT_NOT_NULL(strstr(err, "Z.ds1"));
   expect_game_text("DATA/GLOBAL/TILES/A.ds1", "old a");
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/A.ds1.ds1edit-new"));
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/Z.ds1.ds1edit-new"));
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/Z.ds1"));
   old = game_text("ds1edit-installed.txt");
   TEST_ASSERT_EQUAL_STRING("DATA/GLOBAL/TILES/A.ds1\n", old);
   free(old);
   snprintf(tiles_dir, sizeof(tiles_dir), "%s/DATA/GLOBAL/TILES", game);
   TEST_ASSERT_EQUAL_INT(1, tt_count_entries(tiles_dir));   /* A.ds1 only */
}

void test_an_install_never_writes_through_an_existing_file(void)
{
#ifdef WIN32
   TEST_IGNORE_MESSAGE("hard links to a game's files are a POSIX concern here");
#else
   char target[1400], other[1400];

   make_project();
   put("Global/Tiles/A.ds1", "new");
   put_game("DATA/GLOBAL/TILES/A.ds1", "the original bytes");
   snprintf(target, sizeof(target), "%s/DATA/GLOBAL/TILES/A.ds1", game);
   snprintf(other, sizeof(other), "%s/linked-name", game);
   if (link(target, other) != 0) TEST_IGNORE_MESSAGE("this system can't hard-link here");

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   expect_game_text("DATA/GLOBAL/TILES/A.ds1", "new");
   expect_game_text("linked-name", "the original bytes");
#endif
}

void test_a_file_the_game_holds_open_says_the_game_may_be_running(void)
{
#ifndef WIN32
   TEST_IGNORE_MESSAGE("a held-open file can be replaced on POSIX");
#else
   char target[1400];
   FILE *held;

   make_project();
   put("Global/Tiles/A.ds1", "new");
   put_game("DATA/GLOBAL/TILES/A.ds1", "old");
   snprintf(target, sizeof(target), "%s/DATA/GLOBAL/TILES/A.ds1", game);
   held = _fsopen(target, "rb", _SH_DENYWR);   /* like a game with the file open */
   TEST_ASSERT_NOT_NULL(held);
   TEST_ASSERT_EQUAL_INT(-1, install());
   fclose(held);
   TEST_ASSERT_EQUAL_STRING("The game may be running. Close it and try again.", err);
   expect_game_text("DATA/GLOBAL/TILES/A.ds1", "old");
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/A.ds1.ds1edit-new"));
#endif
}

void test_a_partial_install_is_cleaned_up_by_the_next_one(void)
{
   char blocker[1400];

   make_project();
   put("Global/Tiles/A.ds1", "new a");
   put("Global/Tiles/B.ds1", "new b");
   put_game("DATA/GLOBAL/TILES/Old.ds1", "old");
   put_game("ds1edit-installed.txt", "DATA/GLOBAL/TILES/Old.ds1\n");
   /* A folder where B.ds1 belongs: the rename for B fails after A is in place. */
   snprintf(blocker, sizeof(blocker), "%s/DATA/GLOBAL/TILES/B.ds1", game);
   TEST_ASSERT_TRUE(tt_mkdir(blocker));

   TEST_ASSERT_EQUAL_INT(-1, install());
   TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, "only partly installed"), err);
   TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, "B.ds1"), err);
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/Old.ds1"));
   expect_game_text("DATA/GLOBAL/TILES/A.ds1", "new a");
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/B.ds1.ds1edit-new"));

   /* The next, good install removes everything the half one left. */
   tt_remove_tree(blocker);
   snprintf(blocker, sizeof(blocker), "%s/Global/Tiles/A.ds1", proj);
   TEST_ASSERT_EQUAL_INT(0, remove(blocker));
   snprintf(blocker, sizeof(blocker), "%s/Global/Tiles/B.ds1", proj);
   TEST_ASSERT_EQUAL_INT(0, remove(blocker));
   put("Global/Tiles/C.ds1", "c");
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/A.ds1"));
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/B.ds1"));
   expect_game_text("DATA/GLOBAL/TILES/C.ds1", "c");
}

void test_a_folder_in_a_files_place_before_any_change_is_not_called_partial(void)
{
   char blocker[1400];

   make_project();
   put("Global/Tiles/A.ds1", "new a");
   snprintf(blocker, sizeof(blocker), "%s/DATA/GLOBAL/TILES/A.ds1", game);
   TEST_ASSERT_TRUE(project_ensure_parent_dirs(blocker));
   TEST_ASSERT_TRUE(tt_mkdir(blocker));

   TEST_ASSERT_EQUAL_INT(-1, install());
   TEST_ASSERT_NULL_MESSAGE(strstr(err, "partly"), err);
   TEST_ASSERT_NULL_MESSAGE(strstr(err, "may be running"), err);
   TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, "folder"), err);
}

void test_a_stale_staging_file_is_replaced_not_written_through(void)
{
   make_project();
   put("Global/Tiles/A.ds1", "new a");
   put_game("DATA/GLOBAL/TILES/A.ds1.ds1edit-new", "stale");
   put_game("ds1edit-installed.txt.ds1edit-new", "stale manifest");
#ifndef WIN32
   {
      char stale[1400], other[1400], stale_m[1400], other_m[1400];

      snprintf(stale, sizeof(stale), "%s/DATA/GLOBAL/TILES/A.ds1.ds1edit-new", game);
      snprintf(other, sizeof(other), "%s/linked-stage", game);
      snprintf(stale_m, sizeof(stale_m), "%s/ds1edit-installed.txt.ds1edit-new", game);
      snprintf(other_m, sizeof(other_m), "%s/linked-manifest", game);
      if (link(stale, other) != 0 || link(stale_m, other_m) != 0)
         TEST_IGNORE_MESSAGE("this system can't hard-link here");
   }
#endif
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   expect_game_text("DATA/GLOBAL/TILES/A.ds1", "new a");
   TEST_ASSERT_FALSE(game_has("DATA/GLOBAL/TILES/A.ds1.ds1edit-new"));
   TEST_ASSERT_FALSE(game_has("ds1edit-installed.txt.ds1edit-new"));
#ifndef WIN32
   expect_game_text("linked-stage", "stale");
   expect_game_text("linked-manifest", "stale manifest");
#endif
}

void test_manifest_lines_that_win32_would_normalise_or_that_name_no_file_are_ignored(void)
{
   char dir[1400];

   make_project();
   put("Global/Tiles/A.ds1", "a");
   put_game("DATA/GLOBAL/TILES/Keep.ds1", "keep");
   snprintf(dir, sizeof(dir), "%s/DATA/GLOBAL/TILES/emptydir", game);
   TEST_ASSERT_TRUE(tt_mkdir(dir));
   put_game("ds1edit-installed.txt",
            "DATA/GLOBAL/TILES/Keep.ds1.\n"
            "DATA/GLOBAL/TILES/Keep.ds1 \n"
            "DATA/GLOBAL/TILES/sub./x\n"
            "DATA/GLOBAL/TILES/.\n"
            "DATA/GLOBAL/TILES/\n"
            "DATA/GLOBAL/TILES\n"
            "DATA/GLOBAL/TILES//Keep.ds1\n"
            "DATA/GLOBAL/TILES/emptydir\n");

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, install(), err);
   expect_game_text("DATA/GLOBAL/TILES/Keep.ds1", "keep");
   TEST_ASSERT_TRUE(game_has("DATA/GLOBAL/TILES/emptydir"));
   TEST_ASSERT_TRUE(game_has("DATA/GLOBAL/TILES"));
   expect_game_text("DATA/GLOBAL/TILES/A.ds1", "a");
}

/* ---- game_start ------------------------------------------------------- */

static void make_executable(const char *path)
{
   TEST_ASSERT_EQUAL_INT(FILE_COPY_OK, file_copy_checked(FAKE_GAME_PATH, path));
#ifndef WIN32
   TEST_ASSERT_EQUAL_INT(0, chmod(path, 0755));
#endif
}

/* What the fake game wrote, NUL-ended, or NULL after 5 seconds. Free it. */
static char *wait_for_output(void)
{
   char path[1400];
   int i;

   snprintf(path, sizeof(path), "%s/fake_game.out", game);
   for (i = 0; i < 100; i++)
   {
      char *text = read_text(path);

      if (text != NULL)
         return text;
      al_rest(0.05);
   }
   return NULL;
}

/* A folder as typed and as the game saw it may differ in slash style and case. */
static void expect_same_path(const char *want, const char *got, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      char a = want[i] == '\\' ? '/' : want[i], b = got[i] == '\\' ? '/' : got[i];

      if (a >= 'A' && a <= 'Z') a = (char) (a + 32);
      if (b >= 'A' && b <= 'Z') b = (char) (b + 32);
      TEST_ASSERT_EQUAL_CHAR_MESSAGE(a, b, got);
   }
}

void test_the_game_gets_its_folder_and_every_argument(void)
{
   GAME_PROFILE_S p;
   char exe[1400], *out, *line;

   snprintf(exe, sizeof(exe), "%s/Game.exe", game);
   make_executable(exe);
   p = profile(game, "Game.exe", "-w -direct  \"two words\" -txt \"\"");
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, game_start(&p, err, sizeof(err)), err);
   out = wait_for_output();
   TEST_ASSERT_NOT_NULL_MESSAGE(out, "the game never ran");
   expect_same_path(game, out, (int) strlen(game));
   line = strchr(out, '\n') + 1;
   TEST_ASSERT_EQUAL_STRING("-w\n-direct\ntwo words\n-txt\n\n", line);
   free(out);
}

void test_a_wrapper_runs_first_and_gets_the_exe_then_the_arguments(void)
{
   GAME_PROFILE_S p;
   char wrapper[1400], exe[1400], *out, *line;

   snprintf(wrapper, sizeof(wrapper), "%s/wrapper.exe", abs_root);
   make_executable(wrapper);
   p = profile(game, "Game.exe", "-direct -txt");
   p.wrapper = wrapper;
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, game_start(&p, err, sizeof(err)), err);
   out = wait_for_output();
   TEST_ASSERT_NOT_NULL_MESSAGE(out, "the wrapper never ran");
   expect_same_path(game, out, (int) strlen(game));
   line = strchr(out, '\n') + 1;
   snprintf(exe, sizeof(exe), "%s/Game.exe", game);
   expect_same_path(exe, line, (int) strlen(exe));
   line = strchr(line, '\n') + 1;
   TEST_ASSERT_EQUAL_STRING("-direct\n-txt\n", line);
   free(out);
}

void test_a_missing_executable_is_refused_with_the_command_line(void)
{
   GAME_PROFILE_S p = profile(game, "Nope.exe", "-direct -txt");
   char cmd[1400];

   TEST_ASSERT_EQUAL_INT(-1, game_start(&p, err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "Nope.exe"));
   game_command_text(&p, cmd, sizeof(cmd));
   TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, cmd), err);
}

void test_a_missing_wrapper_is_refused_with_the_command_line(void)
{
   GAME_PROFILE_S p = profile(game, "Game.exe", "-direct -txt");
   char wrapper[1400], cmd[1400];

   snprintf(wrapper, sizeof(wrapper), "%s/no-such-wrapper", abs_root);
   p.wrapper = wrapper;
   TEST_ASSERT_EQUAL_INT(-1, game_start(&p, err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "no-such-wrapper"));
   game_command_text(&p, cmd, sizeof(cmd));
   TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, cmd), err);
}

void test_a_program_that_cant_run_returns_a_reason_and_the_command(void)
{
   /* Present, but not something the system can start. */
   GAME_PROFILE_S p = profile(game, "Game.exe", "-direct -txt");
   char exe[1400], cmd[1400];

   snprintf(exe, sizeof(exe), "%s/Game.exe", game);
   TEST_ASSERT_TRUE(tt_write_file(exe, "not a program", 13));
#ifndef WIN32
   TEST_ASSERT_EQUAL_INT(0, chmod(exe, 0755));
#endif
   TEST_ASSERT_EQUAL_INT(-1, game_start(&p, err, sizeof(err)));
   game_command_text(&p, cmd, sizeof(cmd));
   TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, cmd), err);
   TEST_ASSERT_TRUE(strlen(err) > strlen(cmd));
}

void test_the_command_text_quotes_what_needs_it(void)
{
   GAME_PROFILE_S p = profile("/my game", "Game.exe", "-w \"two words\" -txt");
   char text[1400];

   game_command_text(&p, text, sizeof(text));
   TEST_ASSERT_EQUAL_STRING("\"/my game/Game.exe\" -w \"two words\" -txt", text);
   p.wrapper = "wine";
   game_command_text(&p, text, sizeof(text));
   TEST_ASSERT_EQUAL_STRING("wine \"/my game/Game.exe\" -w \"two words\" -txt", text);
   p = profile("/g", "Game.exe", "-a  -b");
   game_command_text(&p, text, sizeof(text));
   TEST_ASSERT_EQUAL_STRING("/g/Game.exe -a -b", text);
}

void test_the_command_text_is_what_ran(void)
{
   GAME_PROFILE_S p;
   char exe[1400], text[2800], want[2800], *out;

   if (strchr(game, ' ') != NULL) TEST_IGNORE_MESSAGE("the test folder has a space in it");
   snprintf(exe, sizeof(exe), "%s/Game.exe", game);
   make_executable(exe);
   p = profile(game, "Game.exe", "-direct \"a b\" -txt");
   game_command_text(&p, text, sizeof(text));
   snprintf(want, sizeof(want), "%s/Game.exe -direct \"a b\" -txt", game);
   TEST_ASSERT_EQUAL_STRING(want, text);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, game_start(&p, err, sizeof(err)), err);
   out = wait_for_output();
   TEST_ASSERT_NOT_NULL(out);
   TEST_ASSERT_NOT_NULL(strstr(out, "-direct\na b\n-txt\n"));
   free(out);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_a_complete_profile_is_accepted);
   RUN_TEST(test_an_empty_game_folder_says_to_set_the_test_folder);
   RUN_TEST(test_an_exe_missing_from_the_folder_is_named);
   RUN_TEST(test_an_exe_with_a_parent_reference_or_rooted_is_refused);
   RUN_TEST(test_the_game_folder_may_not_be_an_install);
   RUN_TEST(test_the_game_folder_in_another_case_and_slash_style_is_still_an_install);
   RUN_TEST(test_a_game_folder_inside_an_install_is_refused);
   RUN_TEST(test_a_relative_folder_or_one_with_a_parent_reference_is_refused);
   RUN_TEST(test_a_link_to_an_install_is_refused);
   RUN_TEST(test_an_install_that_does_not_exist_falls_back_to_the_text_compare);
   RUN_TEST(test_an_install_inside_the_game_folder_is_refused);
   RUN_TEST(test_a_sibling_with_the_same_prefix_is_not_an_install);
   RUN_TEST(test_args_need_direct_and_txt_as_whole_words_in_any_case);
   RUN_TEST(test_a_folder_that_cant_be_written_is_refused);
   RUN_TEST(test_the_write_check_leaves_nothing_behind);
   RUN_TEST(test_tiles_and_tables_land_in_their_folders);
   RUN_TEST(test_both_folders_are_made_for_a_project_with_no_tables);
   RUN_TEST(test_an_empty_project_still_installs_and_makes_the_folders);
   RUN_TEST(test_the_files_are_the_ones_build_mpq_takes);
   RUN_TEST(test_a_second_install_removes_a_map_that_is_gone);
   RUN_TEST(test_only_listed_files_are_removed);
   RUN_TEST(test_a_tables_bin_is_removed_and_another_bin_is_not);
   RUN_TEST(test_the_installed_text_is_not_given_the_sources_old_time);
   RUN_TEST(test_a_project_file_that_cant_be_read_changes_nothing);
   RUN_TEST(test_an_install_never_writes_through_an_existing_file);
   RUN_TEST(test_a_file_the_game_holds_open_says_the_game_may_be_running);
   RUN_TEST(test_a_partial_install_is_cleaned_up_by_the_next_one);
   RUN_TEST(test_a_folder_in_a_files_place_before_any_change_is_not_called_partial);
   RUN_TEST(test_a_stale_staging_file_is_replaced_not_written_through);
   RUN_TEST(test_manifest_lines_that_win32_would_normalise_or_that_name_no_file_are_ignored);
   RUN_TEST(test_the_game_gets_its_folder_and_every_argument);
   RUN_TEST(test_a_wrapper_runs_first_and_gets_the_exe_then_the_arguments);
   RUN_TEST(test_a_missing_executable_is_refused_with_the_command_line);
   RUN_TEST(test_a_missing_wrapper_is_refused_with_the_command_line);
   RUN_TEST(test_a_program_that_cant_run_returns_a_reason_and_the_command);
   RUN_TEST(test_the_command_text_quotes_what_needs_it);
   RUN_TEST(test_the_command_text_is_what_ran);
   return UNITY_END();
}
