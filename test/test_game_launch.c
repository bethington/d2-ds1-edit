/* game_profile_check over a temp folder holding an empty Game.exe. Nothing here
 * touches a game install. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

#include "unity/unity.h"
#include "structs.h"
#include "core/game_launch.h"
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
   return UNITY_END();
}
