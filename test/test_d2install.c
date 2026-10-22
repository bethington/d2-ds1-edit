/*
 * Installation resolution (src/core/d2install.c): how the four Blizzard
 * archives are found from a configured install directory, and that explicit
 * per-archive paths always win.
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
   glb_config.d2_install = install;
}

void tearDown(void)
{
   glb_config.d2_install = NULL;   /* points at a static buffer */
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
   TEST_ASSERT_NOT_NULL(glb_config.mpq_file[0]);
   TEST_ASSERT_TRUE(tt_file_exists(glb_config.mpq_file[0]));   /* patch first */
   expect_slot_is(1, "d2exp.mpq");
   expect_slot_is(2, "d2data.mpq");
   expect_slot_is(3, "d2char.mpq");
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
   glb_config.mpq_file[1] = mine;
   TEST_ASSERT_EQUAL_INT(3, d2install_resolve_mpqs());
   TEST_ASSERT_TRUE(glb_config.mpq_file[1] == mine);
   TEST_ASSERT_EQUAL_STRING(explicit_path, glb_config.mpq_file[1]);
}

void test_partial_install_fills_only_what_exists(void)
{
   touch("d2data.mpq");
   TEST_ASSERT_EQUAL_INT(1, d2install_resolve_mpqs());
   TEST_ASSERT_NULL(glb_config.mpq_file[0]);
   TEST_ASSERT_NULL(glb_config.mpq_file[1]);
   expect_slot_is(2, "d2data.mpq");
   TEST_ASSERT_NULL(glb_config.mpq_file[3]);
}

void test_missing_configured_install_fills_nothing(void)
{
   static char missing[600];
   snprintf(missing, sizeof(missing), "%snot installed", root);
   glb_config.d2_install = missing;
   TEST_ASSERT_EQUAL_INT(0, d2install_resolve_mpqs());
   TEST_ASSERT_NULL(glb_config.mpq_file[0]);
   TEST_ASSERT_NULL(glb_config.mpq_file[2]);
   TEST_ASSERT_TRUE(glb_config.d2_install == missing);   /* not replaced by detection */
}

void test_archive_name_case_does_not_matter(void)
{
   touch("D2DATA.MPQ");
   TEST_ASSERT_EQUAL_INT(1, d2install_resolve_mpqs());
   TEST_ASSERT_NOT_NULL(glb_config.mpq_file[2]);
   TEST_ASSERT_TRUE(tt_file_exists(glb_config.mpq_file[2]));
}

void test_install_path_with_trailing_separator(void)
{
   static char with_sep[600];
   snprintf(with_sep, sizeof(with_sep), "%s" DS1_SEP_STR, install);
   glb_config.d2_install = with_sep;
   touch("d2exp.mpq");
   TEST_ASSERT_EQUAL_INT(1, d2install_resolve_mpqs());
   TEST_ASSERT_TRUE(tt_file_exists(glb_config.mpq_file[1]));
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
   TEST_ASSERT_NULL(glb_config.mpq_file[2]);
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
   return UNITY_END();
}
