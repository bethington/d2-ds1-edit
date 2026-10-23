/*
 * Installation resolution (src/core/d2install.c): how the four Blizzard
 * archives, and the Project Diablo 2 overlay in front of them, are found
 * from a configured install directory, and that explicit per-archive paths
 * always win. (Detection precedence is test_d2install_detect.c.)
 *
 * Every "install" is an owned temp directory holding empty placeholder files
 * with the archive names -- no game data. A configured d2_install is always
 * set, so the registry / common-location auto-detection is never consulted.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "platform.h"
#include "structs.h"
#include "core/d2install.h"

#include "support/test_tmp.h"

CONFIG_S glb_config;   /* normally defined in globals of the application */

static char root[256];
static char install[512];
static char pd2_folder[600];

/* Files only, no registry: the tests must not see the host's installs. */
static D2INSTALL_ENV files_only_env;

static void reset_config(void)
{
   int i;
   for (i = 0; i < MAX_MPQ_FILE; i++)
   {
      free(glb_config.mpq_file[i]);
      glb_config.mpq_file[i] = NULL;
   }
   memset(&glb_config, 0, sizeof(glb_config));
}

void setUp(void)
{
   memset(&glb_config, 0, sizeof(glb_config));
   TEST_ASSERT_TRUE(tt_root_create("d2install", root, sizeof(root)));
   snprintf(install, sizeof(install), "%sDiablo II copy", root);
   TEST_ASSERT_TRUE(tt_mkdir(install));
   files_only_env = *d2install_real_env();
   files_only_env.reg_install_path = NULL;
   files_only_env.reg_uninstall_location = NULL;
   d2install_set_env(&files_only_env);
   glb_config.d2_install = install;
   snprintf(pd2_folder, sizeof(pd2_folder), "%s" DS1_SEP_STR "ProjectD2", install);
}

void tearDown(void)
{
   glb_config.d2_install = NULL;   /* points at a static buffer */
   glb_config.pd2_dir = NULL;
   d2install_set_env(NULL);
   reset_config();
   tt_remove_tree(root);
}

static void touch(const char *name)
{
   char p[600];
   snprintf(p, sizeof(p), "%s" DS1_SEP_STR "%s", install, name);
   TEST_ASSERT_TRUE(tt_write_file(p, "", 0));
}

static void expect_slot_is(int slot, const char *name)
{
   char want[600];
   TEST_ASSERT_NOT_NULL_MESSAGE(glb_config.mpq_file[slot], name);
   TEST_ASSERT_TRUE_MESSAGE(tt_file_exists(glb_config.mpq_file[slot]), glb_config.mpq_file[slot]);
   snprintf(want, sizeof(want), "%s" DS1_SEP_STR "%s", install, name);
   TEST_ASSERT_EQUAL_STRING(want, glb_config.mpq_file[slot]);
}

void test_full_install_fills_every_slot_in_precedence_order(void)
{
   touch("Patch_D2.mpq");
   touch("d2exp.mpq");
   touch("d2data.mpq");
   touch("d2char.mpq");
   TEST_ASSERT_EQUAL_INT(4, d2install_resolve_mpqs());
   TEST_ASSERT_NOT_NULL(glb_config.mpq_file[MPQ_SLOT_PATCH_D2]);
   TEST_ASSERT_TRUE(tt_file_exists(glb_config.mpq_file[MPQ_SLOT_PATCH_D2]));   /* patch first */
   expect_slot_is(MPQ_SLOT_D2EXP, "d2exp.mpq");
   expect_slot_is(MPQ_SLOT_D2DATA, "d2data.mpq");
   expect_slot_is(MPQ_SLOT_D2CHAR, "d2char.mpq");
}

void test_explicit_archive_paths_are_never_overridden(void)
{
   static const char explicit_path[] = "elsewhere" DS1_SEP_STR "my_d2exp.mpq";
   char *mine = (char *) malloc(sizeof(explicit_path));
   memcpy(mine, explicit_path, sizeof(explicit_path));
   touch("patch_d2.mpq");
   touch("d2exp.mpq");
   touch("d2data.mpq");
   touch("d2char.mpq");
   glb_config.mpq_file[MPQ_SLOT_D2EXP] = mine;
   TEST_ASSERT_EQUAL_INT(3, d2install_resolve_mpqs());
   TEST_ASSERT_TRUE(glb_config.mpq_file[MPQ_SLOT_D2EXP] == mine);
   TEST_ASSERT_EQUAL_STRING(explicit_path, glb_config.mpq_file[MPQ_SLOT_D2EXP]);
}

void test_partial_install_fills_only_what_exists(void)
{
   touch("d2data.mpq");
   TEST_ASSERT_EQUAL_INT(1, d2install_resolve_mpqs());
   TEST_ASSERT_NULL(glb_config.mpq_file[MPQ_SLOT_PATCH_D2]);
   TEST_ASSERT_NULL(glb_config.mpq_file[MPQ_SLOT_D2EXP]);
   expect_slot_is(MPQ_SLOT_D2DATA, "d2data.mpq");
   TEST_ASSERT_NULL(glb_config.mpq_file[MPQ_SLOT_D2CHAR]);
}

void test_missing_configured_install_fills_nothing(void)
{
   static char missing[600];
   snprintf(missing, sizeof(missing), "%snot installed", root);
   glb_config.d2_install = missing;
   TEST_ASSERT_EQUAL_INT(0, d2install_resolve_mpqs());
   TEST_ASSERT_NULL(glb_config.mpq_file[MPQ_SLOT_PATCH_D2]);
   TEST_ASSERT_NULL(glb_config.mpq_file[MPQ_SLOT_D2DATA]);
   TEST_ASSERT_TRUE(glb_config.d2_install == missing);   /* not replaced by detection */
}

void test_archive_name_case_does_not_matter(void)
{
   touch("D2DATA.MPQ");
   TEST_ASSERT_EQUAL_INT(1, d2install_resolve_mpqs());
   TEST_ASSERT_NOT_NULL(glb_config.mpq_file[MPQ_SLOT_D2DATA]);
   TEST_ASSERT_TRUE(tt_file_exists(glb_config.mpq_file[MPQ_SLOT_D2DATA]));
}

void test_install_path_with_trailing_separator(void)
{
   static char with_sep[600];
   snprintf(with_sep, sizeof(with_sep), "%s" DS1_SEP_STR, install);
   glb_config.d2_install = with_sep;
   touch("d2exp.mpq");
   TEST_ASSERT_EQUAL_INT(1, d2install_resolve_mpqs());
   TEST_ASSERT_TRUE(tt_file_exists(glb_config.mpq_file[MPQ_SLOT_D2EXP]));
}

void test_overlong_install_path_fails_without_a_truncated_match(void)
{
   static char longp[800];
   size_t n;
   touch("d2data.mpq");
   snprintf(longp, sizeof(longp), "%s", install);
   n = strlen(longp);
   memset(longp + n, 'x', 600 - n);
   longp[600] = 0;
   glb_config.d2_install = longp;
   TEST_ASSERT_EQUAL_INT(0, d2install_resolve_mpqs());
   TEST_ASSERT_NULL(glb_config.mpq_file[MPQ_SLOT_D2DATA]);
}

/* ---- Project Diablo 2 through the real environment --------------------- */

static void touch_pd2(const char *name)
{
   char p[700];
   tt_mkdir(pd2_folder);
   snprintf(p, sizeof(p), "%s" DS1_SEP_STR "%s", pd2_folder, name);
   TEST_ASSERT_TRUE(tt_write_file(p, "", 0));
}

static void touch_full_pd2(void)
{
   touch_pd2("pd2data.mpq");
   touch_pd2("pd2maps.mpq");
   touch_pd2("pd2assets.mpq");
   touch_pd2("pd2monchars.mpq");
   touch_pd2("patch_d2.mpq");
}

static void expect_pd2_slot(int slot, const char *name)
{
   char want[700];
   TEST_ASSERT_NOT_NULL_MESSAGE(glb_config.mpq_file[slot], name);
   snprintf(want, sizeof(want), "%s" DS1_SEP_STR "%s", pd2_folder, name);
   TEST_ASSERT_EQUAL_STRING(want, glb_config.mpq_file[slot]);
}

void test_pd2_sibling_folder_is_searched_before_the_vanilla_chain(void)
{
   touch("patch_d2.mpq"); touch("d2exp.mpq"); touch("d2data.mpq"); touch("d2char.mpq");
   touch_full_pd2();
   TEST_ASSERT_EQUAL_INT(9, d2install_resolve_mpqs());
   expect_pd2_slot(0, "pd2data.mpq");
   expect_pd2_slot(1, "pd2maps.mpq");
   expect_pd2_slot(2, "pd2assets.mpq");
   expect_pd2_slot(3, "pd2monchars.mpq");
   expect_pd2_slot(4, "patch_d2.mpq");
   expect_slot_is(MPQ_SLOT_PATCH_D2, "patch_d2.mpq");
   expect_slot_is(MPQ_SLOT_D2EXP, "d2exp.mpq");
   expect_slot_is(MPQ_SLOT_D2DATA, "d2data.mpq");
   expect_slot_is(MPQ_SLOT_D2CHAR, "d2char.mpq");
   TEST_ASSERT_EQUAL_STRING("Project Diablo 2", d2install_data_set_name());
}

void test_use_pd2_no_keeps_the_chain_vanilla(void)
{
   int i;
   touch("d2data.mpq");
   touch_full_pd2();
   glb_config.use_pd2 = D2_USE_PD2_NO;
   TEST_ASSERT_EQUAL_INT(1, d2install_resolve_mpqs());
   for (i = 0; i < MAX_PD2_MPQ; i++)
      TEST_ASSERT_NULL(glb_config.mpq_file[MPQ_SLOT_PD2_FIRST + i]);
   expect_slot_is(MPQ_SLOT_D2DATA, "d2data.mpq");
   TEST_ASSERT_EQUAL_STRING("vanilla", d2install_data_set_name());
}

void test_pd2_folder_without_pd2data_is_not_used(void)
{
   int i;
   touch("d2data.mpq");
   touch_pd2("pd2maps.mpq");        /* not enough: pd2data.mpq decides */
   touch_pd2("patch_d2.mpq");
   TEST_ASSERT_EQUAL_INT(1, d2install_resolve_mpqs());
   for (i = 0; i < MAX_PD2_MPQ; i++)
      TEST_ASSERT_NULL(glb_config.mpq_file[MPQ_SLOT_PD2_FIRST + i]);
   TEST_ASSERT_EQUAL_STRING("vanilla", d2install_data_set_name());
}

void test_use_pd2_yes_without_a_pd2_install_falls_back_to_vanilla(void)
{
   touch("d2data.mpq");
   glb_config.use_pd2 = D2_USE_PD2_YES;
   TEST_ASSERT_EQUAL_INT(1, d2install_resolve_mpqs());
   TEST_ASSERT_NULL(glb_config.mpq_file[0]);
   expect_slot_is(MPQ_SLOT_D2DATA, "d2data.mpq");
}

void test_pd2_dir_from_the_ini_is_used(void)
{
   static char elsewhere[700];
   char p[800];
   touch("d2data.mpq");
   snprintf(elsewhere, sizeof(elsewhere), "%sMyPD2", root);
   TEST_ASSERT_TRUE(tt_mkdir(elsewhere));
   snprintf(p, sizeof(p), "%s" DS1_SEP_STR "pd2data.mpq", elsewhere);
   TEST_ASSERT_TRUE(tt_write_file(p, "", 0));
   glb_config.pd2_dir = elsewhere;
   TEST_ASSERT_EQUAL_INT(2, d2install_resolve_mpqs());
   TEST_ASSERT_EQUAL_STRING(p, glb_config.mpq_file[0]);
}

void test_explicit_vanilla_paths_still_win_when_pd2_is_active(void)
{
   static const char explicit_path[] = "elsewhere" DS1_SEP_STR "my_d2data.mpq";
   char *mine = (char *) malloc(sizeof(explicit_path));
   memcpy(mine, explicit_path, sizeof(explicit_path));
   touch("d2data.mpq");
   touch_full_pd2();
   glb_config.mpq_file[MPQ_SLOT_D2DATA] = mine;
   TEST_ASSERT_EQUAL_INT(5, d2install_resolve_mpqs());
   TEST_ASSERT_TRUE(glb_config.mpq_file[MPQ_SLOT_D2DATA] == mine);
   expect_pd2_slot(0, "pd2data.mpq");
}

void test_pd2_overlay_sits_ahead_of_every_vanilla_slot(void)
{
   TEST_ASSERT_TRUE(MPQ_SLOT_PD2_FIRST + MAX_PD2_MPQ <= MPQ_SLOT_PATCH_D2);
   TEST_ASSERT_TRUE(MPQ_SLOT_PATCH_D2 < MPQ_SLOT_D2EXP);
   TEST_ASSERT_TRUE(MPQ_SLOT_D2EXP < MPQ_SLOT_D2DATA);
   TEST_ASSERT_TRUE(MPQ_SLOT_D2DATA < MPQ_SLOT_D2CHAR);
   TEST_ASSERT_EQUAL_INT(MPQ_SLOT_D2CHAR + 1, MAX_MPQ_FILE);
}

void test_real_env_validates_a_directory(void)
{
   TEST_ASSERT_FALSE(d2install_is_install_dir(install));
   touch("d2exp.mpq");
   TEST_ASSERT_TRUE(d2install_is_install_dir(install));
   TEST_ASSERT_FALSE(d2install_is_install_dir(NULL));
   TEST_ASSERT_FALSE(d2install_is_install_dir(""));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_full_install_fills_every_slot_in_precedence_order);
   RUN_TEST(test_explicit_archive_paths_are_never_overridden);
   RUN_TEST(test_partial_install_fills_only_what_exists);
   RUN_TEST(test_missing_configured_install_fills_nothing);
   RUN_TEST(test_archive_name_case_does_not_matter);
   RUN_TEST(test_install_path_with_trailing_separator);
   RUN_TEST(test_overlong_install_path_fails_without_a_truncated_match);
   RUN_TEST(test_pd2_sibling_folder_is_searched_before_the_vanilla_chain);
   RUN_TEST(test_use_pd2_no_keeps_the_chain_vanilla);
   RUN_TEST(test_pd2_folder_without_pd2data_is_not_used);
   RUN_TEST(test_use_pd2_yes_without_a_pd2_install_falls_back_to_vanilla);
   RUN_TEST(test_pd2_dir_from_the_ini_is_used);
   RUN_TEST(test_explicit_vanilla_paths_still_win_when_pd2_is_active);
   RUN_TEST(test_pd2_overlay_sits_ahead_of_every_vanilla_slot);
   RUN_TEST(test_real_env_validates_a_directory);
   return UNITY_END();
}
