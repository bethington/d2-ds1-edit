/*
 * Where a map is read from and where its save goes: project copy-on-save
 * paths fed through the real checked DS1 writer, and the mod-directory
 * overlay lookup (mod_load_in_mem) that a project relies on to find its
 * saved copies again.
 *
 * Everything happens under an owned temp directory; no game install, MPQ or
 * user project is consulted.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "platform.h"
#include "structs.h"
#include "core/ds1.h"
#include "core/ds1_save_io.h"
#include "core/project.h"

/* src/mpq/MpqView.c defines the overlay reader with a fifth `output`
 * argument that MpqView.h omits; declare the definition's real signature. */
int mod_load_in_mem(char *moddir, char *filename, void **buffer, long *buf_len, int output);

#include "support/editor_test_state.h"
#include "support/ds1_fixture.h"
#include "support/ds1_expect.h"
#include "support/test_tmp.h"

#define SEPS DS1_SEP_STR

static char err[768];

void setUp(void)
{
   TEST_ASSERT_TRUE(editor_test_state_init(0, 1, 1));
   memset(&glb_project, 0, sizeof(glb_project));
}

void tearDown(void)
{
   memset(&glb_project, 0, sizeof(glb_project));
   editor_test_state_destroy();
}

static void rootp(char *out, size_t cap, const char *rel)
{
   editor_test_state_path(out, cap, rel);
}

/* Create root-relative directories, one level per argument. */
static void mkdirs(const char *const *levels)
{
   char p[512] = "";
   size_t i;
   snprintf(p, sizeof(p), "%s", editor_test_state_root());
   for (i = 0; levels[i] != NULL; i++)
   {
      size_t n = strlen(p);
      snprintf(p + n, sizeof(p) - n, "%s%s", i ? SEPS : "", levels[i]);
      TEST_ASSERT_TRUE(tt_mkdir(p));
   }
}

/* Author a map at <root>/<dir>/tiles/act1/m.ds1 and load it into slot 0. */
static void load_source(const char *dir, FX_SPEC *s, char *src, size_t cap)
{
   const char *levels[] = { dir, "data", "global", "tiles", "act1", NULL };
   char rel[256];
   mkdirs(levels);
   snprintf(rel, sizeof(rel), "%s" SEPS "data" SEPS "global" SEPS "tiles" SEPS "act1" SEPS "m.ds1", dir);
   rootp(src, cap, rel);
   fx_default(s, 18);
   TEST_ASSERT_TRUE(fx_write(s, src, -1));
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_read_checked(src, 0, 0, 0, err, sizeof(err)), err);
}

static void open_project_at(const char *path_as_given)
{
   memset(&glb_project, 0, sizeof(glb_project));
   glb_project.is_open = 1;
   snprintf(glb_project.path, sizeof(glb_project.path), "%s", path_as_given);
}

static unsigned char *snapshot_bytes(const char *file, size_t *len)
{
   unsigned char *b = tt_read_file(file, len);
   TEST_ASSERT_NOT_NULL(b);
   return b;
}

static void expect_bytes(const char *file, const unsigned char *b, size_t len)
{
   size_t now_len;
   unsigned char *now = tt_read_file(file, &now_len);
   TEST_ASSERT_NOT_NULL_MESSAGE(now, file);
   TEST_ASSERT_EQUAL_UINT32(len, now_len);
   TEST_ASSERT_EQUAL_MEMORY(b, now, len);
   free(now);
}

static void expect_no_backup_beside(const char *src)
{
   char b0[600];
   snprintf(b0, sizeof(b0), "%.*s-000.ds1", (int) (strlen(src) - 4), src);
   TEST_ASSERT_FALSE_MESSAGE(tt_file_exists(b0), "the source map was backed up (i.e. saved over)");
}

/* ---------------- copy-on-save paths through the checked writer ---------------- */

void test_project_with_spaces_receives_the_copy(void)
{
   FX_SPEC s;
   char src[512], proj[512], target[600];
   size_t len;
   unsigned char *orig;

   load_source("game files", &s, src, sizeof(src));
   orig = snapshot_bytes(src, &len);
   rootp(proj, sizeof(proj), "my project");
   TEST_ASSERT_TRUE(tt_mkdir(proj));
   open_project_at(proj);

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_save_checked(0, FALSE, err, sizeof(err)), err);
   snprintf(target, sizeof(target), "%s" SEPS "Global" SEPS "Tiles" SEPS "act1" SEPS "m.ds1", proj);
   TEST_ASSERT_EQUAL_STRING(target, glb_ds1[0].name);
   TEST_ASSERT_TRUE(tt_file_exists(target));
   expect_bytes(src, orig, len);
   expect_no_backup_beside(src);

   /* the second save stays in the project and backs up the project copy */
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_save_checked(0, FALSE, err, sizeof(err)), err);
   expect_bytes(src, orig, len);
   {
      char b0[600];
      snprintf(b0, sizeof(b0), "%s" SEPS "Global" SEPS "Tiles" SEPS "act1" SEPS "m-000.ds1", proj);
      TEST_ASSERT_TRUE(tt_file_exists(b0));
   }
   free(orig);
}

void test_project_path_with_trailing_separator(void)
{
   FX_SPEC s;
   char src[512], proj[512], with_sep[520];
   size_t len;
   unsigned char *orig;

   load_source("game", &s, src, sizeof(src));
   orig = snapshot_bytes(src, &len);
   rootp(proj, sizeof(proj), "proj");
   TEST_ASSERT_TRUE(tt_mkdir(proj));
   snprintf(with_sep, sizeof(with_sep), "%s" SEPS, proj);
   open_project_at(with_sep);

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_save_checked(0, FALSE, err, sizeof(err)), err);
   TEST_ASSERT_TRUE(tt_file_exists(glb_ds1[0].name));
   TEST_ASSERT_EQUAL_INT(0, strncmp(glb_ds1[0].name, proj, strlen(proj)));
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_save_checked(0, FALSE, err, sizeof(err)), err);
   expect_bytes(src, orig, len);
   expect_no_backup_beside(src);
   free(orig);
}

void test_sibling_directory_with_the_project_name_as_prefix_is_not_inside_it(void)
{
   /* project "<root>proj"; map in "<root>proj2/..." -- a different folder */
   FX_SPEC s;
   char src[512], proj[512];
   size_t len;
   unsigned char *orig;

   load_source("proj2", &s, src, sizeof(src));
   orig = snapshot_bytes(src, &len);
   rootp(proj, sizeof(proj), "proj");
   TEST_ASSERT_TRUE(tt_mkdir(proj));
   open_project_at(proj);

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_save_checked(0, FALSE, err, sizeof(err)), err);
   expect_bytes(src, orig, len);
   expect_no_backup_beside(src);
   TEST_ASSERT_EQUAL_INT(0, strncmp(glb_ds1[0].name, proj, strlen(proj)));
   TEST_ASSERT_TRUE(glb_ds1[0].name[strlen(proj)] == '\\' || glb_ds1[0].name[strlen(proj)] == '/');
   free(orig);
}

void test_project_path_too_long_for_the_copy_does_not_save_over_the_source(void)
{
   FX_SPEC s;
   char src[512], proj[512];
   size_t len, n;
   unsigned char *orig;

   load_source("game", &s, src, sizeof(src));
   orig = snapshot_bytes(src, &len);
   /* a project path the copy cannot fit in the map name (255 chars) */
   rootp(proj, sizeof(proj), "");
   n = strlen(proj);
   memset(proj + n, 'p', 240 - n);
   proj[240] = 0;
   open_project_at(proj);

   TEST_ASSERT_EQUAL_INT(-1, ds1_save_checked(0, FALSE, err, sizeof(err)));
   TEST_ASSERT_EQUAL_STRING(src, glb_ds1[0].name);
   expect_bytes(src, orig, len);
   expect_no_backup_beside(src);
   free(orig);
}

void test_map_already_in_the_project_saves_in_place(void)
{
   FX_SPEC s;
   char src[512], proj[512];
   const char *levels[] = { "proj", NULL };

   mkdirs(levels);
   load_source("proj" SEPS "Global", &s, src, sizeof(src));
   rootp(proj, sizeof(proj), "proj");
   open_project_at(proj);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_save_checked(0, FALSE, err, sizeof(err)), err);
   TEST_ASSERT_EQUAL_STRING(src, glb_ds1[0].name);
}

void test_tmp_autosave_is_never_redirected(void)
{
   FX_SPEC s;
   char src[512], proj[512], tmpf[600];
   size_t len;
   unsigned char *orig;

   load_source("game", &s, src, sizeof(src));
   orig = snapshot_bytes(src, &len);
   rootp(proj, sizeof(proj), "proj");
   TEST_ASSERT_TRUE(tt_mkdir(proj));
   open_project_at(proj);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_save_checked(0, TRUE, err, sizeof(err)), err);
   snprintf(tmpf, sizeof(tmpf), "%.*s.tmp", (int) (strlen(src) - 4), src);
   TEST_ASSERT_TRUE(tt_file_exists(tmpf));
   TEST_ASSERT_EQUAL_STRING(src, glb_ds1[0].name);
   TEST_ASSERT_EQUAL_INT(0, tt_count_entries(proj));
   expect_bytes(src, orig, len);
   free(orig);
}

/* ---------------- mod-directory overlay ---------------- */

static void put(const char *rel, const char *text)
{
   char p[512];
   rootp(p, sizeof(p), rel);
   TEST_ASSERT_TRUE(tt_write_file(p, text, strlen(text)));
}

static int overlay(const char *moddir_rel, const char *virt, char *out, size_t cap)
{
   char mod[512];
   void *buf = NULL;
   long len = 0;
   int rc;
   rootp(mod, sizeof(mod), moddir_rel);
   if (mod[strlen(mod) - 1] == '\\' || mod[strlen(mod) - 1] == '/')
      mod[strlen(mod) - 1] = 0;
   rc = mod_load_in_mem(mod, (char *) virt, &buf, &len, 0);
   if (rc != -1 && buf != NULL)
      snprintf(out, cap, "%.*s", (int) len, (const char *) buf);
   else
      out[0] = 0;
   free(buf);
   return rc;
}

void test_overlay_finds_the_project_layout_for_a_virtual_path(void)
{
   const char *levels[] = { "proj", "Global", "Tiles", "act1", NULL };
   char got[64];
   mkdirs(levels);
   put("proj" SEPS "Global" SEPS "Tiles" SEPS "act1" SEPS "m.ds1", "project copy");
   TEST_ASSERT_NOT_EQUAL(-1, overlay("proj", "Data\\Global\\Tiles\\act1\\m.ds1", got, sizeof(got)));
   TEST_ASSERT_EQUAL_STRING("project copy", got);
}

void test_overlay_prefers_the_full_data_layout(void)
{
   const char *a[] = { "mod", "Global", "Tiles", "act1", NULL };
   const char *b[] = { "mod", "Data", "Global", "Tiles", "act1", NULL };
   char got[64];
   mkdirs(a);
   mkdirs(b);
   put("mod" SEPS "Global" SEPS "Tiles" SEPS "act1" SEPS "m.ds1", "short layout");
   put("mod" SEPS "Data" SEPS "Global" SEPS "Tiles" SEPS "act1" SEPS "m.ds1", "data layout");
   TEST_ASSERT_NOT_EQUAL(-1, overlay("mod", "Data\\Global\\Tiles\\act1\\m.ds1", got, sizeof(got)));
   TEST_ASSERT_EQUAL_STRING("data layout", got);
}

void test_overlay_misses_cleanly_and_never_matches_by_basename(void)
{
   const char *a[] = { "mod", "Global", "Tiles", "act2", NULL };
   char got[64];
   mkdirs(a);
   put("mod" SEPS "Global" SEPS "Tiles" SEPS "act2" SEPS "m.ds1", "act2 namesake");
   TEST_ASSERT_EQUAL_INT(-1, overlay("mod", "Data\\Global\\Tiles\\act1\\m.ds1", got, sizeof(got)));
   TEST_ASSERT_EQUAL_STRING("", got);
}

int main(int argc, char **argv)
{
   pkgc_test_filter = argc > 1 ? argv[1] : NULL;
   UNITY_BEGIN();
   PKGC_RUN(test_project_with_spaces_receives_the_copy);
   PKGC_RUN(test_project_path_with_trailing_separator);
   PKGC_RUN(test_sibling_directory_with_the_project_name_as_prefix_is_not_inside_it);
   PKGC_RUN(test_project_path_too_long_for_the_copy_does_not_save_over_the_source);
   PKGC_RUN(test_map_already_in_the_project_saves_in_place);
   PKGC_RUN(test_tmp_autosave_is_never_redirected);
   PKGC_RUN(test_overlay_finds_the_project_layout_for_a_virtual_path);
   PKGC_RUN(test_overlay_prefers_the_full_data_layout);
   PKGC_RUN(test_overlay_misses_cleanly_and_never_matches_by_basename);
   return UNITY_END();
}
