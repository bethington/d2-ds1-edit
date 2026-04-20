/*
 * Unit tests for preferences.c + project.c.
 *
 * Covers file round-trip, the LRU recent-projects logic, and project
 * create/load/save against a scratch directory under the test runner's CWD.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef WIN32
   #include <direct.h>
   #include <io.h>
#endif

#include <allegro5/allegro.h>

#include "unity/unity.h"

#include "structs.h"
#include "core/preferences.h"
#include "core/project.h"

#define SCRATCH_DIR   "test_scratch_project_2a"
#define PREFS_FILE    SCRATCH_DIR "/prefs.ini"
#define PROJECT_DIR   SCRATCH_DIR "/myproject"

// glb_config is declared in structs.h but normally defined in globals.c, which
// we don't link into the test. Define a minimal stub.
CONFIG_S glb_config;

static void rmrf(const char *path)
{
#ifdef WIN32
   char cmd[512];
   snprintf(cmd, sizeof(cmd), "cmd /c rmdir /s /q \"%s\" 2>nul", path);
   system(cmd);
#else
   char cmd[512];
   snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
   system(cmd);
#endif
}

void setUp(void)
{
   rmrf(SCRATCH_DIR);
#ifdef WIN32
   _mkdir(SCRATCH_DIR);
#else
   mkdir(SCRATCH_DIR, 0755);
#endif
   memset(&glb_prefs,   0, sizeof(glb_prefs));
   memset(&glb_project, 0, sizeof(glb_project));
   memset(&glb_config,  0, sizeof(glb_config));
}

void tearDown(void)
{
   rmrf(SCRATCH_DIR);
}


/* ---- preferences ---- */

void test_prefs_load_missing_file_returns_zero(void)
{
   int rc = prefs_load_from(PREFS_FILE);
   TEST_ASSERT_EQUAL_INT(0, rc);
   TEST_ASSERT_EQUAL_INT(0, glb_prefs.loaded);
   TEST_ASSERT_EQUAL_INT(0, glb_prefs.recent_count);
   TEST_ASSERT_EQUAL_STRING("", glb_prefs.last_d2_install);
}

void test_prefs_roundtrip(void)
{
   int rc;

   strncpy(glb_prefs.last_d2_install, "C:\\Diablo2",
           sizeof(glb_prefs.last_d2_install) - 1);
   strncpy(glb_prefs.recent_projects[0], "C:\\mods\\alpha",
           PREFS_PATH_MAX - 1);
   strncpy(glb_prefs.recent_projects[1], "C:\\mods\\beta",
           PREFS_PATH_MAX - 1);
   glb_prefs.recent_count = 2;

   rc = prefs_save_to(PREFS_FILE);
   TEST_ASSERT_EQUAL_INT(1, rc);

   memset(&glb_prefs, 0, sizeof(glb_prefs));

   rc = prefs_load_from(PREFS_FILE);
   TEST_ASSERT_EQUAL_INT(1, rc);
   TEST_ASSERT_EQUAL_INT(1, glb_prefs.loaded);
   TEST_ASSERT_EQUAL_STRING("C:\\Diablo2", glb_prefs.last_d2_install);
   TEST_ASSERT_EQUAL_INT(2, glb_prefs.recent_count);
   TEST_ASSERT_EQUAL_STRING("C:\\mods\\alpha", glb_prefs.recent_projects[0]);
   TEST_ASSERT_EQUAL_STRING("C:\\mods\\beta",  glb_prefs.recent_projects[1]);
}

void test_prefs_record_recent_adds_to_front(void)
{
   prefs_record_recent_project("C:\\mods\\a");
   prefs_record_recent_project("C:\\mods\\b");
   prefs_record_recent_project("C:\\mods\\c");

   TEST_ASSERT_EQUAL_INT(3, glb_prefs.recent_count);
   TEST_ASSERT_EQUAL_STRING("C:\\mods\\c", glb_prefs.recent_projects[0]);
   TEST_ASSERT_EQUAL_STRING("C:\\mods\\b", glb_prefs.recent_projects[1]);
   TEST_ASSERT_EQUAL_STRING("C:\\mods\\a", glb_prefs.recent_projects[2]);
}

void test_prefs_record_recent_dedupes(void)
{
   prefs_record_recent_project("C:\\mods\\a");
   prefs_record_recent_project("C:\\mods\\b");
   prefs_record_recent_project("C:\\mods\\a"); // re-record first

   TEST_ASSERT_EQUAL_INT(2, glb_prefs.recent_count);
   TEST_ASSERT_EQUAL_STRING("C:\\mods\\a", glb_prefs.recent_projects[0]);
   TEST_ASSERT_EQUAL_STRING("C:\\mods\\b", glb_prefs.recent_projects[1]);
}

void test_prefs_record_recent_caps_at_max(void)
{
   char name[32];
   int i;

   for (i = 0; i < PREFS_RECENT_MAX + 5; i++)
   {
      snprintf(name, sizeof(name), "C:\\mods\\%d", i);
      prefs_record_recent_project(name);
   }

   TEST_ASSERT_EQUAL_INT(PREFS_RECENT_MAX, glb_prefs.recent_count);
   // Most-recent is the last inserted
   snprintf(name, sizeof(name), "C:\\mods\\%d", PREFS_RECENT_MAX + 4);
   TEST_ASSERT_EQUAL_STRING(name, glb_prefs.recent_projects[0]);
}


/* ---- project ---- */

void test_project_create_and_load(void)
{
   int rc;

   rc = project_create(PROJECT_DIR, "My Test Mod", "C:\\Diablo2");
   TEST_ASSERT_EQUAL_INT(1, rc);

   rc = project_load(PROJECT_DIR);
   TEST_ASSERT_EQUAL_INT(1, rc);
   TEST_ASSERT_EQUAL_INT(1, glb_project.is_open);
   TEST_ASSERT_EQUAL_STRING("My Test Mod", glb_project.name);
   TEST_ASSERT_EQUAL_STRING("C:\\Diablo2",  glb_project.d2_install);
   TEST_ASSERT_EQUAL_INT(0, glb_project.extra_count);
}

void test_project_create_refuses_overwrite(void)
{
   int rc;

   rc = project_create(PROJECT_DIR, "First", "");
   TEST_ASSERT_EQUAL_INT(1, rc);

   rc = project_create(PROJECT_DIR, "Second", "");
   TEST_ASSERT_EQUAL_INT(0, rc);
}

void test_project_load_missing_returns_zero(void)
{
   int rc = project_load(PROJECT_DIR); // never created
   TEST_ASSERT_EQUAL_INT(0, rc);
   TEST_ASSERT_EQUAL_INT(0, glb_project.is_open);
}

void test_project_save_persists_changes(void)
{
   int rc;

   rc = project_create(PROJECT_DIR, "Original", "");
   TEST_ASSERT_EQUAL_INT(1, rc);

   rc = project_load(PROJECT_DIR);
   TEST_ASSERT_EQUAL_INT(1, rc);

   strncpy(glb_project.name, "Renamed", PROJECT_NAME_MAX - 1);
   strncpy(glb_project.extra_mod_mpqs[0], "C:\\plugy.mpq",
           PROJECT_PATH_MAX - 1);
   glb_project.extra_count = 1;

   rc = project_save();
   TEST_ASSERT_EQUAL_INT(1, rc);

   memset(&glb_project, 0, sizeof(glb_project));
   rc = project_load(PROJECT_DIR);
   TEST_ASSERT_EQUAL_INT(1, rc);
   TEST_ASSERT_EQUAL_STRING("Renamed", glb_project.name);
   TEST_ASSERT_EQUAL_INT(1, glb_project.extra_count);
   TEST_ASSERT_EQUAL_STRING("C:\\plugy.mpq", glb_project.extra_mod_mpqs[0]);
}

void test_project_apply_to_config(void)
{
   int rc;

   rc = project_create(PROJECT_DIR, "Configured", "C:\\Diablo2");
   TEST_ASSERT_EQUAL_INT(1, rc);
   rc = project_load(PROJECT_DIR);
   TEST_ASSERT_EQUAL_INT(1, rc);

   project_apply_to_config();

   TEST_ASSERT_NOT_NULL(glb_config.mod_dir[0]);
   TEST_ASSERT_EQUAL_STRING(PROJECT_DIR, glb_config.mod_dir[0]);
   TEST_ASSERT_NOT_NULL(glb_config.d2_install);
   TEST_ASSERT_EQUAL_STRING("C:\\Diablo2", glb_config.d2_install);
}


/* ---- main ---- */

int main(void)
{
   al_init();

   UNITY_BEGIN();

   RUN_TEST(test_prefs_load_missing_file_returns_zero);
   RUN_TEST(test_prefs_roundtrip);
   RUN_TEST(test_prefs_record_recent_adds_to_front);
   RUN_TEST(test_prefs_record_recent_dedupes);
   RUN_TEST(test_prefs_record_recent_caps_at_max);

   RUN_TEST(test_project_create_and_load);
   RUN_TEST(test_project_create_refuses_overwrite);
   RUN_TEST(test_project_load_missing_returns_zero);
   RUN_TEST(test_project_save_persists_changes);
   RUN_TEST(test_project_apply_to_config);

   return UNITY_END();
}
