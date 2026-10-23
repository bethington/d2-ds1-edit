/*
 * DS1 writer: save/reload preservation, the backup and temporary-file
 * policy, copy-on-save into a project, and behaviour under I/O failure.
 *
 * Maps are authored with test/support/ds1_fixture.c, loaded with the real
 * reader, saved with the real writer into an owned temp directory, and the
 * output is checked twice: by the real reader and by the fixture module's
 * own, independent v18 decoder.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "platform.h"
#include "structs.h"
#include "core/ds1.h"
#include "core/project.h"
#include "core/ds1_save_io.h"

#include "support/editor_test_state.h"
#include "support/ds1_fixture.h"
#include "support/ds1_expect.h"
#include "support/test_tmp.h"

#define SAVE(idx, tmp) save_map((idx), (tmp))

static char err[512];

/* Save through the production checked writer. ds1edit_error() must never be
 * reached: a failed save is reported, not fatal. */
static int save_map(int idx, int is_tmp)
{
   int rc = ds1_save_checked(idx, is_tmp, err, sizeof(err));
   TEST_ASSERT_EQUAL_INT(0, editor_test_error_count);
   return rc;
}

/* ------------------------------------------------------------------ */
/* Fault-injecting file operations: delegate to stdio, fail on demand  */
/* ------------------------------------------------------------------ */

typedef struct FAULTS
{
   long write_budget;   /* bytes accepted before writes start failing; -1 = no limit */
   int  fail_open;
   int  fail_close;
   int  fail_rename_from; /* 1-based: this rename call and every later one fail; 0 = none */
   int  fail_rename_only; /* if set, only the call numbered fail_rename_from fails */
   int  fail_remove;
   int  opens, closes, renames;
   long written;
} FAULTS;

static FAULTS faults;

static FILE *f_open(const char *p, const char *m)
{
   if (faults.fail_open)
      return NULL;
   faults.opens++;
   return fopen(p, m);
}

static size_t f_write(const void *d, size_t sz, size_t n, FILE *f)
{
   size_t bytes = sz * n;
   if (faults.write_budget >= 0 && faults.written + (long) bytes > faults.write_budget)
   {
      size_t room = (size_t) (faults.write_budget - faults.written);
      if (room > 0)
         fwrite(d, 1, room, f);   /* a short write: part of it lands */
      faults.written += (long) room;
      return sz ? room / sz : 0;
   }
   faults.written += (long) bytes;
   return fwrite(d, sz, n, f);
}

static int f_write_byte(int c, FILE *f)
{
   if (faults.write_budget >= 0 && faults.written + 1 > faults.write_budget)
      return EOF;
   faults.written++;
   return fputc(c, f);
}

static int f_close(FILE *f)
{
   int rc = fclose(f);   /* always release the handle */
   faults.closes++;
   return faults.fail_close ? EOF : rc;
}

static int f_rename(const char *a, const char *b)
{
   faults.renames++;
   if (faults.fail_rename_from > 0)
   {
      if (faults.fail_rename_only ? faults.renames == faults.fail_rename_from
                                  : faults.renames >= faults.fail_rename_from)
         return -1;
   }
   return rename(a, b);
}

static int f_remove(const char *p)
{
   if (faults.fail_remove)
      return -1;
   return remove(p);
}

static const DS1_SAVE_IO fault_io = {
   f_open, f_write, f_write_byte, f_close, f_rename, f_remove
};

static void reset_faults(void)
{
   memset(&faults, 0, sizeof(faults));
   faults.write_budget = -1;
}

static int save_faulty(int idx, int is_tmp)
{
   int rc = ds1_save_with_io(idx, is_tmp, &fault_io, err, sizeof(err));
   TEST_ASSERT_EQUAL_INT(0, editor_test_error_count);
   return rc;
}

void setUp(void)
{
   TEST_ASSERT_TRUE(editor_test_state_init(0, 1, 1));
   memset(&glb_project, 0, sizeof(glb_project));
   reset_faults();
}

void tearDown(void)
{
   memset(&glb_project, 0, sizeof(glb_project));
   editor_test_state_destroy();
}

static void root_path(char *out, size_t cap, const char *name)
{
   editor_test_state_path(out, cap, name);
}

/* Author a fixture at `rel`, load it into `idx`. */
static void load_fixture(FX_SPEC *s, int version, const char *rel, int idx, char *out_path, size_t cap)
{
   char e[256];
   fx_default(s, version);
   root_path(out_path, cap, rel);
   TEST_ASSERT_TRUE(fx_write(s, out_path, -1));
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_read_checked(out_path, idx, 0, 0, e, sizeof(e)), e);
}

static void reload_and_expect(const char *file, const FX_SPEC *s, int idx)
{
   char e[256];
   size_t len;
   unsigned char *buf;
   FX_SPEC dec;

   editor_test_state_free_slot(idx);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_read_checked(file, idx, 0, 0, e, sizeof(e)), e);
   ds1_expect_matches_spec(idx, s, DS1X_AS_RESAVED);

   /* independent decode of the bytes on disk */
   buf = tt_read_file(file, &len);
   TEST_ASSERT_NOT_NULL(buf);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, fx_decode_v18(buf, len, &dec), "independent v18 decode");
   TEST_ASSERT_EQUAL_INT(18, dec.version);
   TEST_ASSERT_EQUAL_INT(s->width, dec.width);
   TEST_ASSERT_EQUAL_INT(s->height, dec.height);
   TEST_ASSERT_EQUAL_INT(fx_wall_layers(s), dec.wall_num);
   TEST_ASSERT_EQUAL_INT(fx_floor_layers(s), dec.floor_num);
   TEST_ASSERT_EQUAL_INT(s->obj_num, dec.obj_num);
   TEST_ASSERT_EQUAL_INT(fx_has_groups(s) ? s->group_num : 0, dec.group_num);
   TEST_ASSERT_EQUAL_MEMORY(s->floor[0][1][2], dec.floor[0][1][2], 4);
   TEST_ASSERT_EQUAL_MEMORY(s->shadow[1][0], dec.shadow[1][0], 4);
   if (fx_wall_layers(s) > 0)
      TEST_ASSERT_EQUAL_MEMORY(s->wall[fx_wall_layers(s) - 1][0][1],
                               dec.wall[fx_wall_layers(s) - 1][0][1], 4);
   free(buf);
}

/* ------------------------------------------------------------------ */
/* Successful saves                                                    */
/* ------------------------------------------------------------------ */

static void check_upgrade(int version)
{
   FX_SPEC s;
   char src[512], dst[512];
   char rel[64];

   snprintf(rel, sizeof(rel), "v%d.ds1", version);
   load_fixture(&s, version, rel, 0, src, sizeof(src));
   root_path(dst, sizeof(dst), "upgraded.ds1");
   snprintf(glb_ds1[0].name, sizeof(glb_ds1[0].name), "%s", dst);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   reload_and_expect(dst, &s, 1);
   remove(dst);
}

#define UPGRADE_TEST(v) void test_save_upgrades_v##v##_and_preserves_fields(void) { check_upgrade(v); }
UPGRADE_TEST(2)
UPGRADE_TEST(3)
UPGRADE_TEST(6)
UPGRADE_TEST(7)
UPGRADE_TEST(9)
UPGRADE_TEST(12)
UPGRADE_TEST(13)
UPGRADE_TEST(14)
UPGRADE_TEST(15)
UPGRADE_TEST(16)
UPGRADE_TEST(17)
UPGRADE_TEST(18)

void test_out_of_bounds_objects_survive_read_save_read(void)
{
   FX_SPEC s;
   char src[512], e[256];
   fx_default(&s, 18);
   s.obj_num = 4;
   s.obj[2].type = 2; s.obj[2].id = 11; s.obj[2].x = -3; s.obj[2].y = -7;
   s.obj[2].flags = 0x55;
   s.obj[3].type = 1; s.obj[3].id = 12; s.obj[3].x = 40; s.obj[3].y = 999;
   s.npc_num = 3;
   s.npc[1].path_num = 1; s.npc[1].x = -3; s.npc[1].y = -7;
   s.npc[1].pt[0][0] = -4; s.npc[1].pt[0][1] = -8; s.npc[1].pt[0][2] = 2;
   s.npc[2].path_num = 1; s.npc[2].x = 40; s.npc[2].y = 999;
   s.npc[2].pt[0][0] = 41; s.npc[2].pt[0][1] = 1000; s.npc[2].pt[0][2] = 1;
   root_path(src, sizeof(src), "oob objects.ds1");
   TEST_ASSERT_TRUE(fx_write(&s, src, -1));
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_read_checked(src, 0, 0, 0, e, sizeof(e)), e);
   TEST_ASSERT_EQUAL_INT(4, glb_ds1[0].obj_num);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   reload_and_expect(src, &s, 1);
}

void test_four_walls_and_flags_survive_save(void)
{
   FX_SPEC s;
   char src[512], e[256];
   fx_default(&s, 18);
   s.wall_num = 4;
   s.obj[0].flags = (int32_t) 0x80000001u;
   s.group[1].unk = (int32_t) 0xFFFFFFFFu;
   s.tag[1][2] = 0xFFFFFFFFu;
   root_path(src, sizeof(src), "four walls.ds1");
   TEST_ASSERT_TRUE(fx_write(&s, src, -1));
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_read_checked(src, 0, 0, 0, e, sizeof(e)), e);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   reload_and_expect(src, &s, 1);
}

void test_second_save_keeps_previous_bytes_as_numbered_backup(void)
{
   FX_SPEC s;
   char src[512], first[512], backup[512];
   size_t len;
   unsigned char *orig;

   load_fixture(&s, 18, "map one.ds1", 0, src, sizeof(src));
   orig = tt_read_file(src, &len);

   /* first save: the authored original becomes -000 */
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   root_path(backup, sizeof(backup), "map one-000.ds1");
   TEST_ASSERT_TRUE(tt_file_exists(backup));
   {
      size_t blen;
      unsigned char *b = tt_read_file(backup, &blen);
      TEST_ASSERT_EQUAL_UINT32(len, blen);
      TEST_ASSERT_EQUAL_MEMORY(orig, b, len);
      free(b);
   }
   root_path(first, sizeof(first), "first-save-copy.ds1");
   {
      size_t flen;
      unsigned char *b = tt_read_file(src, &flen);
      TEST_ASSERT_TRUE(tt_write_file(first, b, flen));
      free(b);
   }

   /* edit, save again: the first save becomes -001 */
   glb_ds1[0].floor_buff[0].prop1 = 0x42;
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   root_path(backup, sizeof(backup), "map one-001.ds1");
   TEST_ASSERT_TRUE(tt_files_equal(backup, first));
   s.floor[0][0][0][0] = 0x42;
   reload_and_expect(src, &s, 1);
   free(orig);
}

void test_existing_backup_slot_is_left_alone(void)
{
   FX_SPEC s;
   char src[512], b0[512], b1[512];
   static const char sentinel[] = "not a ds1, someone else's backup";

   load_fixture(&s, 18, "slot.ds1", 0, src, sizeof(src));
   root_path(b0, sizeof(b0), "slot-000.ds1");
   TEST_ASSERT_TRUE(tt_write_file(b0, sentinel, sizeof(sentinel)));
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   {
      size_t l;
      unsigned char *b = tt_read_file(b0, &l);
      TEST_ASSERT_EQUAL_UINT32(sizeof(sentinel), l);
      TEST_ASSERT_EQUAL_MEMORY(sentinel, b, l);
      free(b);
   }
   root_path(b1, sizeof(b1), "slot-001.ds1");
   TEST_ASSERT_TRUE(tt_file_exists(b1));
}

void test_tmp_save_writes_beside_the_map_and_changes_nothing_else(void)
{
   FX_SPEC s;
   char src[512], tmpf[512], copy[512];
   static const char old_tmp[] = "stale autosave";
   size_t len;
   unsigned char *orig;

   load_fixture(&s, 15, "auto save.ds1", 0, src, sizeof(src));
   orig = tt_read_file(src, &len);
   root_path(copy, sizeof(copy), "orig-copy.bin");
   TEST_ASSERT_TRUE(tt_write_file(copy, orig, len));
   root_path(tmpf, sizeof(tmpf), "auto save.tmp");
   TEST_ASSERT_TRUE(tt_write_file(tmpf, old_tmp, sizeof(old_tmp)));

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, TRUE), err);
   TEST_ASSERT_TRUE(tt_files_equal(src, copy));
   TEST_ASSERT_EQUAL_STRING(src, glb_ds1[0].name);
   {
      char b0[512];
      root_path(b0, sizeof(b0), "auto save-000.ds1");
      TEST_ASSERT_FALSE(tt_file_exists(b0));
   }
   reload_and_expect(tmpf, &s, 1);
   free(orig);
}

void test_save_count_is_written_only_by_a_save(void)
{
   FX_SPEC s;
   char src[512];
   glb_config.workspace_enable = 1;
   load_fixture(&s, 18, "count.ds1", 0, src, sizeof(src));
   TEST_ASSERT_EQUAL_UINT32(0, glb_ds1[0].save_count);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   TEST_ASSERT_EQUAL_UINT32(1, glb_ds1[0].save_count);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   TEST_ASSERT_EQUAL_UINT32(2, glb_ds1[0].save_count);
   /* the counter round-trips through the workspace entries */
   {
      char e[256];
      editor_test_state_free_slot(1);
      TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_read_checked(src, 1, 0, 0, e, sizeof(e)), e);
      TEST_ASSERT_EQUAL_UINT32(2, glb_ds1[1].save_count);
   }
}

/* ------------------------------------------------------------------ */
/* Copy-on-save into a project                                         */
/* ------------------------------------------------------------------ */

static void open_project(const char *rel)
{
   char p[512];
   root_path(p, sizeof(p), rel);
   /* no trailing separator, like a project path from the UI */
   if (p[strlen(p) - 1] == '/' || p[strlen(p) - 1] == '\\')
      p[strlen(p) - 1] = 0;
   memset(&glb_project, 0, sizeof(glb_project));
   glb_project.is_open = 1;
   snprintf(glb_project.path, sizeof(glb_project.path), "%s", p);
   TEST_ASSERT_TRUE(tt_mkdir(glb_project.path));
}

static void make_source_tree(void)
{
   char p[512];
   root_path(p, sizeof(p), "src"); tt_mkdir(p);
   root_path(p, sizeof(p), "src" DS1_SEP_STR "tiles"); tt_mkdir(p);
   root_path(p, sizeof(p), "src" DS1_SEP_STR "tiles" DS1_SEP_STR "act1"); tt_mkdir(p);
}

void test_copy_on_save_writes_into_the_project_and_keeps_the_original(void)
{
   FX_SPEC s;
   char src[512], copy[512], target[512];
   size_t len;
   unsigned char *orig;

   make_source_tree();
   load_fixture(&s, 18, "src" DS1_SEP_STR "tiles" DS1_SEP_STR "act1" DS1_SEP_STR "town.ds1",
                0, src, sizeof(src));
   orig = tt_read_file(src, &len);
   root_path(copy, sizeof(copy), "orig.bin");
   TEST_ASSERT_TRUE(tt_write_file(copy, orig, len));
   open_project("proj");

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   snprintf(target, sizeof(target), "%s%sGlobal%sTiles%sact1%stown.ds1",
            glb_project.path, DS1_SEP_STR, DS1_SEP_STR, DS1_SEP_STR, DS1_SEP_STR);
   TEST_ASSERT_EQUAL_STRING(target, glb_ds1[0].name);
   TEST_ASSERT_TRUE(tt_files_equal(src, copy));
   {
      char b0[512];
      root_path(b0, sizeof(b0), "src" DS1_SEP_STR "tiles" DS1_SEP_STR "act1" DS1_SEP_STR "town-000.ds1");
      TEST_ASSERT_FALSE(tt_file_exists(b0));
   }
   reload_and_expect(target, &s, 1);
   free(orig);
}

void test_copy_on_save_parent_creation_failure_changes_nothing(void)
{
   FX_SPEC s;
   char src[512], copy[512], blocker[512];
   size_t len;
   unsigned char *orig;
   int rc;

   make_source_tree();
   load_fixture(&s, 18, "src" DS1_SEP_STR "tiles" DS1_SEP_STR "act1" DS1_SEP_STR "town.ds1",
                0, src, sizeof(src));
   orig = tt_read_file(src, &len);
   root_path(copy, sizeof(copy), "orig.bin");
   TEST_ASSERT_TRUE(tt_write_file(copy, orig, len));
   open_project("proj");
   /* a regular file where the project's Global directory must go */
   snprintf(blocker, sizeof(blocker), "%s%sGlobal", glb_project.path, DS1_SEP_STR);
   TEST_ASSERT_TRUE(tt_write_file(blocker, "x", 1));

   rc = SAVE(0, FALSE);
   TEST_ASSERT_EQUAL_INT_MESSAGE(-1, rc, "save into an uncreatable directory must fail");
   TEST_ASSERT_EQUAL_STRING(src, glb_ds1[0].name);
   TEST_ASSERT_TRUE(tt_files_equal(src, copy));
   TEST_ASSERT_EQUAL_INT(1, tt_count_entries(glb_project.path)); /* only the blocker */
   free(orig);
}

/* ------------------------------------------------------------------ */
/* I/O failures: reported, recoverable, no state change                */
/* ------------------------------------------------------------------ */

typedef struct SNAP
{
   char     name[256];
   UDWORD   save_count;
   unsigned char *bytes;
   size_t   len;
   int      entries;
} SNAP;

static void snap(SNAP *sn, const char *file)
{
   snprintf(sn->name, sizeof(sn->name), "%s", glb_ds1[0].name);
   sn->save_count = glb_ds1[0].save_count;
   sn->bytes = tt_read_file(file, &sn->len);
   sn->entries = tt_count_entries(editor_test_state_root());
}

/* The failed save changed nothing: same name and counter, the destination
 * holds exactly its old bytes, no backup or staging file was left behind. */
static void expect_unchanged(SNAP *sn, const char *file)
{
   size_t len;
   unsigned char *now = tt_read_file(file, &len);
   TEST_ASSERT_EQUAL_STRING(sn->name, glb_ds1[0].name);
   TEST_ASSERT_EQUAL_UINT32(sn->save_count, glb_ds1[0].save_count);
   TEST_ASSERT_NOT_NULL(now);
   TEST_ASSERT_EQUAL_UINT32(sn->len, len);
   TEST_ASSERT_EQUAL_MEMORY(sn->bytes, now, len);
   TEST_ASSERT_EQUAL_INT(sn->entries, tt_count_entries(editor_test_state_root()));
   TEST_ASSERT_TRUE(strlen(err) > 0);
   free(now);
   free(sn->bytes);
   sn->bytes = NULL;
}

/* After any failure a plain save must still work. */
static void expect_next_save_works(const FX_SPEC *s, const char *file)
{
   reset_faults();
   glb_config.workspace_enable = 0;   /* compare the file list verbatim */
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   reload_and_expect(file, s, 1);
}

static void setup_existing(FX_SPEC *s, char *src, size_t cap, SNAP *sn)
{
   glb_config.workspace_enable = 1;
   load_fixture(s, 18, "existing.ds1", 0, src, cap);
   glb_ds1[0].floor_buff[0].prop1 = 0x42;   /* an unsaved edit */
   s->floor[0][0][0][0] = 0x42;
   snap(sn, src);
}

void test_short_write_is_reported_and_keeps_the_destination(void)
{
   FX_SPEC s; SNAP sn; char src[512];
   setup_existing(&s, src, sizeof(src), &sn);
   faults.write_budget = 30;    /* inside the file-name list */
   TEST_ASSERT_EQUAL_INT(-1, save_faulty(0, FALSE));
   TEST_ASSERT_EQUAL_INT(faults.opens, faults.closes);
   expect_unchanged(&sn, src);
   expect_next_save_works(&s, src);
}

void test_failed_tile_byte_write_is_reported(void)
{
   FX_SPEC s; SNAP sn; char src[512];
   setup_existing(&s, src, sizeof(src), &sn);
   faults.write_budget = 300;   /* inside the per-byte tile layers */
   TEST_ASSERT_EQUAL_INT(-1, save_faulty(0, FALSE));
   expect_unchanged(&sn, src);
   expect_next_save_works(&s, src);
}

void test_failed_close_is_reported_and_the_handle_released(void)
{
   FX_SPEC s; SNAP sn; char src[512];
   setup_existing(&s, src, sizeof(src), &sn);
   faults.fail_close = 1;
   TEST_ASSERT_EQUAL_INT(-1, save_faulty(0, FALSE));
   TEST_ASSERT_EQUAL_INT(1, faults.opens);
   TEST_ASSERT_EQUAL_INT(1, faults.closes);
   expect_unchanged(&sn, src);
   expect_next_save_works(&s, src);
}

void test_denied_open_is_reported(void)
{
   FX_SPEC s; SNAP sn; char src[512];
   setup_existing(&s, src, sizeof(src), &sn);
   faults.fail_open = 1;
   TEST_ASSERT_EQUAL_INT(-1, save_faulty(0, FALSE));
   expect_unchanged(&sn, src);
   expect_next_save_works(&s, src);
}

void test_failed_backup_rename_changes_nothing(void)
{
   FX_SPEC s; SNAP sn; char src[512];
   setup_existing(&s, src, sizeof(src), &sn);
   faults.fail_rename_from = 1;    /* existing -> backup */
   faults.fail_rename_only = 1;
   TEST_ASSERT_EQUAL_INT(-1, save_faulty(0, FALSE));
   expect_unchanged(&sn, src);
   expect_next_save_works(&s, src);
}

void test_failed_publish_rolls_the_backup_back(void)
{
   FX_SPEC s; SNAP sn; char src[512];
   setup_existing(&s, src, sizeof(src), &sn);
   faults.fail_rename_from = 2;    /* staging -> destination */
   faults.fail_rename_only = 1;
   TEST_ASSERT_EQUAL_INT(-1, save_faulty(0, FALSE));
   TEST_ASSERT_EQUAL_INT(3, faults.renames);   /* backup, publish, rollback */
   expect_unchanged(&sn, src);
   expect_next_save_works(&s, src);
}

void test_failed_publish_and_rollback_names_the_backup(void)
{
   FX_SPEC s; SNAP sn; char src[512], backup[512];
   size_t len;
   unsigned char *b;
   setup_existing(&s, src, sizeof(src), &sn);
   faults.fail_rename_from = 2;    /* publish and rollback both refused */
   TEST_ASSERT_EQUAL_INT(-1, save_faulty(0, FALSE));
   root_path(backup, sizeof(backup), "existing-000.ds1");
   TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, "existing-000.ds1"), err);
   b = tt_read_file(backup, &len);
   TEST_ASSERT_NOT_NULL(b);
   TEST_ASSERT_EQUAL_UINT32(sn.len, len);
   TEST_ASSERT_EQUAL_MEMORY(sn.bytes, b, len);   /* old bytes recoverable */
   TEST_ASSERT_EQUAL_STRING(sn.name, glb_ds1[0].name);
   TEST_ASSERT_EQUAL_UINT32(sn.save_count, glb_ds1[0].save_count);
   free(b);
   free(sn.bytes);
}

void test_failed_tmp_save_leaves_map_and_original_alone(void)
{
   FX_SPEC s; SNAP sn; char src[512], tmpf[512];
   setup_existing(&s, src, sizeof(src), &sn);
   faults.write_budget = 100;
   TEST_ASSERT_EQUAL_INT(-1, save_faulty(0, TRUE));
   root_path(tmpf, sizeof(tmpf), "existing.tmp");
   TEST_ASSERT_FALSE(tt_file_exists(tmpf));
   expect_unchanged(&sn, src);
}

void test_failed_copy_on_save_keeps_the_original_name(void)
{
   FX_SPEC s;
   char src[512], target[512];
   make_source_tree();
   load_fixture(&s, 18, "src" DS1_SEP_STR "tiles" DS1_SEP_STR "act1" DS1_SEP_STR "town.ds1",
                0, src, sizeof(src));
   open_project("proj");
   faults.write_budget = 10;
   TEST_ASSERT_EQUAL_INT(-1, save_faulty(0, FALSE));
   TEST_ASSERT_EQUAL_STRING(src, glb_ds1[0].name);
   snprintf(target, sizeof(target), "%s%sGlobal%sTiles%sact1%stown.ds1",
            glb_project.path, DS1_SEP_STR, DS1_SEP_STR, DS1_SEP_STR, DS1_SEP_STR);
   TEST_ASSERT_FALSE(tt_file_exists(target));
   /* and the retry lands in the project */
   reset_faults();
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, SAVE(0, FALSE), err);
   TEST_ASSERT_EQUAL_STRING(target, glb_ds1[0].name);
}

void test_empty_slot_is_not_an_error(void)
{
   TEST_ASSERT_EQUAL_INT(0, SAVE(5, FALSE));
}

void test_legacy_wrapper_returns_the_failure(void)
{
   FX_SPEC s;
   char src[512], blocker[512];
   make_source_tree();
   load_fixture(&s, 18, "src" DS1_SEP_STR "tiles" DS1_SEP_STR "act1" DS1_SEP_STR "town.ds1",
                0, src, sizeof(src));
   open_project("proj");
   snprintf(blocker, sizeof(blocker), "%s%sGlobal", glb_project.path, DS1_SEP_STR);
   TEST_ASSERT_TRUE(tt_write_file(blocker, "x", 1));
   TEST_ASSERT_EQUAL_INT(-1, ds1_save(0, FALSE));
   TEST_ASSERT_EQUAL_INT(0, editor_test_error_count);   /* not fatal */
   TEST_ASSERT_EQUAL_STRING(src, glb_ds1[0].name);
}

int main(int argc, char **argv)
{
   pkgc_test_filter = argc > 1 ? argv[1] : NULL;
   UNITY_BEGIN();
   PKGC_RUN(test_save_upgrades_v2_and_preserves_fields);
   PKGC_RUN(test_save_upgrades_v3_and_preserves_fields);
   PKGC_RUN(test_save_upgrades_v6_and_preserves_fields);
   PKGC_RUN(test_save_upgrades_v7_and_preserves_fields);
   PKGC_RUN(test_save_upgrades_v9_and_preserves_fields);
   PKGC_RUN(test_save_upgrades_v12_and_preserves_fields);
   PKGC_RUN(test_save_upgrades_v13_and_preserves_fields);
   PKGC_RUN(test_save_upgrades_v14_and_preserves_fields);
   PKGC_RUN(test_save_upgrades_v15_and_preserves_fields);
   PKGC_RUN(test_save_upgrades_v16_and_preserves_fields);
   PKGC_RUN(test_save_upgrades_v17_and_preserves_fields);
   PKGC_RUN(test_save_upgrades_v18_and_preserves_fields);
   PKGC_RUN(test_four_walls_and_flags_survive_save);
   PKGC_RUN(test_out_of_bounds_objects_survive_read_save_read);
   PKGC_RUN(test_second_save_keeps_previous_bytes_as_numbered_backup);
   PKGC_RUN(test_existing_backup_slot_is_left_alone);
   PKGC_RUN(test_tmp_save_writes_beside_the_map_and_changes_nothing_else);
   PKGC_RUN(test_save_count_is_written_only_by_a_save);
   PKGC_RUN(test_copy_on_save_writes_into_the_project_and_keeps_the_original);
   PKGC_RUN(test_copy_on_save_parent_creation_failure_changes_nothing);
   PKGC_RUN(test_short_write_is_reported_and_keeps_the_destination);
   PKGC_RUN(test_failed_tile_byte_write_is_reported);
   PKGC_RUN(test_failed_close_is_reported_and_the_handle_released);
   PKGC_RUN(test_denied_open_is_reported);
   PKGC_RUN(test_failed_backup_rename_changes_nothing);
   PKGC_RUN(test_failed_publish_rolls_the_backup_back);
   PKGC_RUN(test_failed_publish_and_rollback_names_the_backup);
   PKGC_RUN(test_failed_tmp_save_leaves_map_and_original_alone);
   PKGC_RUN(test_failed_copy_on_save_keeps_the_original_name);
   PKGC_RUN(test_empty_slot_is_not_an_error);
   PKGC_RUN(test_legacy_wrapper_returns_the_failure);
   return UNITY_END();
}
