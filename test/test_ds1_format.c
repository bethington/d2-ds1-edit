/*
 * DS1 reader: authored version matrix, tolerated legacy tails, and
 * graceful rejection of malformed input.
 *
 * Every fixture is encoded by test/support/ds1_fixture.c -- an encoder that
 * shares no code with src/core/ds1.c -- written into an owned temp directory
 * and read back with the real ds1_read(). No game install or MPQ is used:
 * the archive hook answers "not found", so the reader takes its disk path.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "core/ds1.h"

#include "support/editor_test_state.h"
#include "support/ds1_fixture.h"
#include "support/ds1_expect.h"
#include "support/test_tmp.h"

static char path[512];

void setUp(void)
{
   TEST_ASSERT_TRUE(editor_test_state_init(0, 1, 1));
   editor_test_state_path(path, sizeof(path), "fixture.ds1");
}

void tearDown(void)
{
   editor_test_state_destroy();
}

/* Read `file` into slot 0 through the real reader; 1 when it reported an
 * error through ds1edit_error() instead of returning. */
static int read_reports_error(const char *file)
{
   int trapped = 0;
   int before = editor_test_error_count;
   EDITOR_TEST_TRAPS_ERROR(ds1_read(file, 0, 0, 0), trapped);
   return trapped || editor_test_error_count != before;
}

static void write_spec(const FX_SPEC *s, long truncate_to)
{
   TEST_ASSERT_TRUE(fx_write(s, path, truncate_to));
}

static void write_raw(const void *data, size_t len)
{
   TEST_ASSERT_TRUE(tt_write_file(path, data, len));
}

/* ------------------------------------------------------------------ */
/* Version matrix                                                      */
/* ------------------------------------------------------------------ */

static void check_version(int version)
{
   FX_SPEC s;
   fx_default(&s, version);
   write_spec(&s, -1);
   TEST_ASSERT_FALSE_MESSAGE(read_reports_error(path), editor_test_last_error);
   ds1_expect_matches_spec(0, &s, DS1X_AS_READ);
}

#define VERSION_TEST(v) void test_version_##v##_reads_every_field(void) { check_version(v); }
VERSION_TEST(2)
VERSION_TEST(3)
VERSION_TEST(4)
VERSION_TEST(5)
VERSION_TEST(6)
VERSION_TEST(7)
VERSION_TEST(8)
VERSION_TEST(9)
VERSION_TEST(10)
VERSION_TEST(12)
VERSION_TEST(13)
VERSION_TEST(14)
VERSION_TEST(15)
VERSION_TEST(16)
VERSION_TEST(17)
VERSION_TEST(18)

void test_encoder_offsets_match_the_documented_layout(void)
{
   /* Independent check of the encoder itself: a v18 3x2 map with two
    * walls, two floors, shadow and tag has 2*2 + 2 + 1 + 1 = 8 layers. */
   FX_SPEC s;
   FX_LAYOUT lay;
   size_t len;
   unsigned char *buf;

   fx_default(&s, 18);
   buf = fx_encode(&s, &len, &lay);
   TEST_ASSERT_NOT_NULL(buf);
   TEST_ASSERT_EQUAL_UINT32(20, lay.files_at);                    /* 5 dwords */
   TEST_ASSERT_EQUAL_UINT32(20 + 4 + 13 + 12 + 8, lay.layers_at); /* names + counts */
   TEST_ASSERT_EQUAL_UINT32(lay.layers_at + 8 * 6 * 4, lay.objects_at);
   TEST_ASSERT_EQUAL_UINT32(lay.objects_at + 4 + 2 * 20, lay.groups_at);
   TEST_ASSERT_EQUAL_UINT32(lay.groups_at + 8 + 2 * 20, lay.paths_at);
   TEST_ASSERT_EQUAL_UINT32(lay.paths_at + 4 + 12 + 2 * 12, lay.end);
   TEST_ASSERT_EQUAL_UINT32(len, lay.end);
   free(buf);

   /* v3: five fixed blocks, 16-byte objects, no groups, no paths */
   fx_default(&s, 3);
   buf = fx_encode(&s, &len, &lay);
   TEST_ASSERT_EQUAL_UINT32(12, lay.files_at);
   TEST_ASSERT_EQUAL_UINT32(lay.files_at + 4 + 13 + 12, lay.layers_at);
   TEST_ASSERT_EQUAL_UINT32(lay.layers_at + 5 * 6 * 4, lay.objects_at);
   TEST_ASSERT_EQUAL_UINT32(lay.objects_at + 4 + 2 * 16, len);
   free(buf);
}

void test_tag_type_zero_has_no_tag_layer_and_no_groups(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.tag_type = 0;
   s.group_num = 0;
   write_spec(&s, -1);
   TEST_ASSERT_FALSE(read_reports_error(path));
   ds1_expect_matches_spec(0, &s, DS1X_AS_READ);
}

void test_tag_type_two_reads_tag_layer_and_groups(void)
{
   FX_SPEC s;
   fx_default(&s, 16);
   s.tag_type = 2;
   write_spec(&s, -1);
   TEST_ASSERT_FALSE(read_reports_error(path));
   ds1_expect_matches_spec(0, &s, DS1X_AS_READ);
}

void test_zero_objects_and_zero_paths(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.obj_num = 0;
   s.npc_num = 0;
   write_spec(&s, -1);
   TEST_ASSERT_FALSE(read_reports_error(path));
   ds1_expect_matches_spec(0, &s, DS1X_AS_READ);
}

void test_wall_and_floor_counts_may_be_zero(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.wall_num = 0;
   s.floor_num = 0;
   write_spec(&s, -1);
   TEST_ASSERT_FALSE(read_reports_error(path));
   ds1_expect_matches_spec(0, &s, DS1X_AS_READ);
}

void test_four_walls_two_floors_keep_distinct_layer_order(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.wall_num = 4;
   s.floor_num = 2;
   write_spec(&s, -1);
   TEST_ASSERT_FALSE(read_reports_error(path));
   ds1_expect_matches_spec(0, &s, DS1X_AS_READ);
}

void test_legacy_orientation_table_covers_all_values(void)
{
   FX_SPEC s;
   int raw, x, y;
   fx_default(&s, 6);
   s.width = 5;
   s.height = 5;
   for (raw = 0; raw < 25; raw++)
      s.orient[0][raw / 5][raw % 5] = (uint8_t) raw;
   s.obj_num = 0;
   write_spec(&s, -1);
   TEST_ASSERT_FALSE(read_reports_error(path));
   for (y = 0; y < 5; y++)
      for (x = 0; x < 5; x++)
         TEST_ASSERT_EQUAL_INT(fx_legacy_orientation(y * 5 + x),
            glb_ds1[0].wall_buff[y * glb_ds1[0].wall_line + x * glb_ds1[0].wall_num].orientation);
}

void test_act_is_clamped_to_five(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.act = 9;
   write_spec(&s, -1);
   TEST_ASSERT_FALSE(read_reports_error(path));
   TEST_ASSERT_EQUAL_INT(5, glb_ds1[0].act);
}

/* The original D2Common 1.13c loader never bounds-checks object positions
 * (vanilla maps carry such objects), so the reader must keep them, with their
 * paths, untouched. Other bounds (counts, truncation) stay enforced below. */
void test_objects_outside_the_map_are_kept(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.obj_num = 5;
   s.obj[1].x = 15;  /* 3 cells * 5 sub-tiles = 15: one past the edge */
   s.obj[2].type = 2; s.obj[2].id = 11; s.obj[2].x = -3; s.obj[2].y = -7;
   s.obj[2].flags = 0x55;
   s.obj[3].type = 1; s.obj[3].id = 12; s.obj[3].x = 40; s.obj[3].y = 999;
   s.obj[4].type = 1; s.obj[4].id = 13; s.obj[4].x = 2000000; s.obj[4].y = -1;
   s.npc_num = 3;
   s.npc[1].path_num = 2; s.npc[1].x = -3; s.npc[1].y = -7;
   s.npc[1].pt[0][0] = -4; s.npc[1].pt[0][1] = -8; s.npc[1].pt[0][2] = 2;
   s.npc[1].pt[1][0] = 5;  s.npc[1].pt[1][1] = 6;  s.npc[1].pt[1][2] = 3;
   s.npc[2].path_num = 1; s.npc[2].x = 40; s.npc[2].y = 999;
   s.npc[2].pt[0][0] = 41; s.npc[2].pt[0][1] = 1000; s.npc[2].pt[0][2] = 1;
   write_spec(&s, -1);
   TEST_ASSERT_FALSE(read_reports_error(path));
   ds1_expect_matches_spec(0, &s, DS1X_AS_READ);
   TEST_ASSERT_EQUAL_INT(5, glb_ds1[0].obj_num);
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].obj[2].path_num);
   TEST_ASSERT_EQUAL_INT(1, glb_ds1[0].obj[3].path_num);
}

void test_paths_of_two_objects_on_one_subtile_are_discarded(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.obj[1].x = s.obj[0].x;
   s.obj[1].y = s.obj[0].y;
   write_spec(&s, -1);
   TEST_ASSERT_FALSE(read_reports_error(path));
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].obj_num);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].obj[0].path_num);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].obj[1].path_num);
}

void test_unmatched_path_block_is_not_given_to_another_object(void)
{
   /* Paths are keyed by the anchor coordinate. A block whose anchor matches
    * no object must be dropped, not attached to object 0. */
   FX_SPEC s;
   fx_default(&s, 18);
   s.npc[0].x = 1;
   s.npc[0].y = 1;
   write_spec(&s, -1);
   TEST_ASSERT_FALSE(read_reports_error(path));
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].obj_num);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].obj[0].path_num);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].obj[1].path_num);
}

void test_resize_on_load_crops_and_extends(void)
{
   FX_SPEC s;
   int trapped = 0;
   fx_default(&s, 18);
   write_spec(&s, -1);
   EDITOR_TEST_TRAPS_ERROR(ds1_read(path, 0, 2, 4), trapped);
   TEST_ASSERT_FALSE(trapped);
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].width);
   TEST_ASSERT_EQUAL_INT(4, glb_ds1[0].height);
   /* (1,1) kept, (0,3) new and empty */
   TEST_ASSERT_EQUAL_UINT8(s.floor[0][1][1][0],
      glb_ds1[0].floor_buff[1 * glb_ds1[0].floor_line + 1 * glb_ds1[0].floor_num].prop1);
   TEST_ASSERT_EQUAL_UINT8(0,
      glb_ds1[0].floor_buff[3 * glb_ds1[0].floor_line].prop1);
}

/* ------------------------------------------------------------------ */
/* Tolerated legacy tails (docs/guides/ds1-map-format.md)              */
/* ------------------------------------------------------------------ */

void test_trailing_zero_dword_after_v13_is_ignored(void)
{
   FX_SPEC s;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 13);
   buf = fx_encode(&s, &len, NULL);
   buf = (unsigned char *) realloc(buf, len + 48);
   memset(buf + len, 0, 48);
   write_raw(buf, len + 48);   /* swamp2.ds1 carries 48 spare bytes */
   free(buf);
   TEST_ASSERT_FALSE(read_reports_error(path));
   ds1_expect_matches_spec(0, &s, DS1X_AS_READ);
}

void test_group_block_cut_after_last_tile_x_still_loads(void)
{
   /* trees.ds1 shape: the file ends after the last group's x. */
   FX_SPEC s;
   FX_LAYOUT lay;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 13);
   buf = fx_encode(&s, &len, &lay);
   free(buf);
   write_spec(&s, (long) (lay.groups_at + 4 + 20 + 4));
   TEST_ASSERT_FALSE_MESSAGE(read_reports_error(path), editor_test_last_error);
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].group_num);
   TEST_ASSERT_EQUAL_UINT32(2, glb_ds1[0].group[1].tile_x);
   TEST_ASSERT_EQUAL_UINT32(0, glb_ds1[0].group[1].tile_y);
   TEST_ASSERT_EQUAL_UINT32(0, glb_ds1[0].group[1].unk);
   TEST_ASSERT_EQUAL_UINT32(0x5A5A, glb_ds1[0].group[0].unk);
}

void test_v18_missing_npc_count_reads_as_no_paths(void)
{
   /* xtransition.ds1 shape: the file stops where the path count would be. */
   FX_SPEC s;
   FX_LAYOUT lay;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 18);
   buf = fx_encode(&s, &len, &lay);
   free(buf);
   write_spec(&s, (long) lay.paths_at);
   TEST_ASSERT_FALSE_MESSAGE(read_reports_error(path), editor_test_last_error);
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].obj_num);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].obj[0].path_num);
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].group_num);
}

void test_v18_group_block_cut_short_reads_as_zero_and_no_paths(void)
{
   FX_SPEC s;
   FX_LAYOUT lay;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 18);
   buf = fx_encode(&s, &len, &lay);
   free(buf);
   write_spec(&s, (long) (lay.groups_at + 8 + 20 + 8));
   TEST_ASSERT_FALSE_MESSAGE(read_reports_error(path), editor_test_last_error);
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].group_num);
   TEST_ASSERT_EQUAL_UINT32(1, glb_ds1[0].group[1].tile_y);
   TEST_ASSERT_EQUAL_UINT32(0, glb_ds1[0].group[1].width);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].obj[0].path_num);
}

/* ------------------------------------------------------------------ */
/* Malformed input must be rejected, not read past its end             */
/* ------------------------------------------------------------------ */

static void expect_rejected_bytes(const unsigned char *buf, size_t len, const char *what)
{
   write_raw(buf, len);
   TEST_ASSERT_TRUE_MESSAGE(read_reports_error(path), what);
}

static void expect_rejected_spec(const FX_SPEC *s, long truncate_to, const char *what)
{
   write_spec(s, truncate_to);
   TEST_ASSERT_TRUE_MESSAGE(read_reports_error(path), what);
}

void test_rejects_empty_file(void)
{
   expect_rejected_bytes((const unsigned char *) "", 0, "empty file");
}

void test_rejects_header_shorter_than_three_dwords(void)
{
   static const unsigned char hdr[8] = { 18, 0, 0, 0, 2, 0, 0, 0 };
   expect_rejected_bytes(hdr, sizeof(hdr), "8-byte header");
}

void test_rejects_unterminated_filename(void)
{
   FX_SPEC s;
   FX_LAYOUT lay;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 18);
   buf = fx_encode(&s, &len, &lay);
   free(buf);
   /* cut inside the first name: no NUL before end of file */
   expect_rejected_spec(&s, (long) (lay.files_at + 4 + 5), "unterminated filename");
}

void test_rejects_filename_count_beyond_file(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.file_num = 2;
   write_spec(&s, -1);
   {
      size_t len;
      unsigned char *buf = tt_read_file(path, &len);
      buf[20] = 0xFF; buf[21] = 0xFF; buf[22] = 0xFF; buf[23] = 0x7F;
      expect_rejected_bytes(buf, len, "file_num 0x7fffffff");
      free(buf);
   }
}

void test_rejects_negative_filename_count(void)
{
   FX_SPEC s;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 18);
   buf = fx_encode(&s, &len, NULL);
   buf[20] = 0xFF; buf[21] = 0xFF; buf[22] = 0xFF; buf[23] = 0xFF;
   expect_rejected_bytes(buf, len, "file_num -1");
   free(buf);
}

void test_rejects_more_than_four_wall_layers(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.wall_num = 5;   /* encoder writes the count; layer data only for 4 */
   expect_rejected_spec(&s, -1, "wall_num 5");
}

void test_rejects_negative_wall_layer_count(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.wall_num = -1;
   expect_rejected_spec(&s, -1, "wall_num -1");
}

void test_rejects_more_than_two_floor_layers(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.floor_num = 3;
   expect_rejected_spec(&s, -1, "floor_num 3");
}

void test_rejects_negative_dimensions(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.width = -4;
   s.obj_num = 0;
   s.npc_num = 0;
   expect_rejected_spec(&s, -1, "width -4");
}

void test_rejects_dimensions_larger_than_the_file(void)
{
   FX_SPEC s;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 18);
   buf = fx_encode(&s, &len, NULL);
   /* width-1 = 0x00FFFFFF, height-1 = 0x00FFFFFF */
   buf[4] = 0xFF; buf[5] = 0xFF; buf[6] = 0xFF; buf[7] = 0x00;
   buf[8] = 0xFF; buf[9] = 0xFF; buf[10] = 0xFF; buf[11] = 0x00;
   expect_rejected_bytes(buf, len, "16M x 16M map in a small file");
   free(buf);
}

void test_rejects_truncated_tile_layers(void)
{
   FX_SPEC s;
   FX_LAYOUT lay;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 18);
   buf = fx_encode(&s, &len, &lay);
   free(buf);
   expect_rejected_spec(&s, (long) (lay.layers_at + 30), "tile layers cut short");
}

void test_rejects_object_count_beyond_file(void)
{
   FX_SPEC s;
   FX_LAYOUT lay;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 18);
   buf = fx_encode(&s, &len, &lay);
   buf[lay.objects_at + 0] = 0x00;
   buf[lay.objects_at + 1] = 0x00;
   buf[lay.objects_at + 2] = 0x01;
   buf[lay.objects_at + 3] = 0x00;   /* 65536 objects */
   expect_rejected_bytes(buf, len, "obj_num 65536");
   free(buf);
}

void test_rejects_negative_object_count(void)
{
   FX_SPEC s;
   FX_LAYOUT lay;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 18);
   buf = fx_encode(&s, &len, &lay);
   memset(buf + lay.objects_at, 0xFF, 4);
   expect_rejected_bytes(buf, len, "obj_num -1");
   free(buf);
}

void test_rejects_truncated_object_record(void)
{
   FX_SPEC s;
   FX_LAYOUT lay;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 8);   /* no groups / paths after the objects */
   buf = fx_encode(&s, &len, &lay);
   free(buf);
   expect_rejected_spec(&s, (long) (lay.objects_at + 4 + 20 + 8), "object record cut");
}

void test_rejects_group_count_far_beyond_file(void)
{
   FX_SPEC s;
   FX_LAYOUT lay;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 18);
   buf = fx_encode(&s, &len, &lay);
   /* v18: pre dword, then the count */
   buf[lay.groups_at + 4] = 0x00;
   buf[lay.groups_at + 5] = 0x00;
   buf[lay.groups_at + 6] = 0x00;
   buf[lay.groups_at + 7] = 0x40;   /* 0x40000000 groups */
   expect_rejected_bytes(buf, len, "group_num 0x40000000");
   free(buf);
}

void test_rejects_negative_group_count(void)
{
   FX_SPEC s;
   FX_LAYOUT lay;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 18);
   buf = fx_encode(&s, &len, &lay);
   memset(buf + lay.groups_at + 4, 0xFF, 4);
   expect_rejected_bytes(buf, len, "group_num -1");
   free(buf);
}

void test_rejects_truncated_path_points(void)
{
   FX_SPEC s;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 18);
   buf = fx_encode(&s, &len, NULL);
   free(buf);
   expect_rejected_spec(&s, (long) (len - 8), "last path point cut");
}

void test_rejects_negative_path_count(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   s.npc[0].path_num = -3;
   expect_rejected_spec(&s, -1, "path_num -3");
}

void test_rejects_unmatched_path_block_running_past_the_end(void)
{
   FX_SPEC s;
   FX_LAYOUT lay;
   size_t len;
   unsigned char *buf;
   fx_default(&s, 18);
   s.npc[0].x = 1;  /* no object there: the reader skips the points */
   buf = fx_encode(&s, &len, &lay);
   /* claim 0x10000000 points */
   buf[lay.paths_at + 4 + 0] = 0x00;
   buf[lay.paths_at + 4 + 1] = 0x00;
   buf[lay.paths_at + 4 + 2] = 0x00;
   buf[lay.paths_at + 4 + 3] = 0x10;
   expect_rejected_bytes(buf, len, "unmatched path block 0x10000000 points");
   free(buf);
}

/* ------------------------------------------------------------------ */
/* Path names                                                          */
/* ------------------------------------------------------------------ */

void test_long_basename_does_not_overrun_neighbouring_state(void)
{
   FX_SPEC s;
   char name[128];
   memset(name, 'n', 70);
   strcpy(name + 70, ".ds1");
   editor_test_state_path(path, sizeof(path), name);
   fx_default(&s, 18);
   write_spec(&s, -1);
   TEST_ASSERT_FALSE_MESSAGE(read_reports_error(path), editor_test_last_error);
   TEST_ASSERT_EQUAL_STRING(path, glb_ds1[0].name);
   TEST_ASSERT_TRUE(strlen(glb_ds1[0].filename) < sizeof(glb_ds1[0].filename));
   TEST_ASSERT_EQUAL_INT(0, strncmp(glb_ds1[0].filename, name, sizeof(glb_ds1[0].filename) - 1));
   TEST_ASSERT_NULL(glb_ds1[0].undo.fptr);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].undo.buf_num);
   TEST_ASSERT_EQUAL_INT(-1, glb_ds1[0].undo.cur_buf_num);
}

void test_file_without_directory_keeps_its_name(void)
{
   FX_SPEC s;
   fx_default(&s, 18);
   write_spec(&s, -1);
   TEST_ASSERT_FALSE(read_reports_error(path));
   TEST_ASSERT_EQUAL_STRING("fixture.ds1", glb_ds1[0].filename);
}

void test_checked_read_reports_and_leaves_the_slot_reusable(void)
{
   static const unsigned char hdr[8] = { 18, 0, 0, 0, 2, 0, 0, 0 };
   char err[256];
   FX_SPEC s;

   write_raw(hdr, sizeof(hdr));
   TEST_ASSERT_EQUAL_INT(-1, ds1_read_checked(path, 0, 0, 0, err, sizeof(err)));
   TEST_ASSERT_NOT_NULL(strstr(err, "too short"));
   TEST_ASSERT_EQUAL_INT(0, editor_test_error_count);
   TEST_ASSERT_EQUAL_CHAR(0, glb_ds1[0].name[0]);
   TEST_ASSERT_NULL(glb_ds1[0].obj);
   TEST_ASSERT_NULL(glb_ds1[0].floor_buff);

   fx_default(&s, 18);
   write_spec(&s, -1);
   TEST_ASSERT_EQUAL_INT(0, ds1_read_checked(path, 0, 0, 0, err, sizeof(err)));
   ds1_expect_matches_spec(0, &s, DS1X_AS_READ);
}

void test_checked_read_rejects_a_path_longer_than_the_name_field(void)
{
   char longpath[400], err[256];
   memset(longpath, 'p', sizeof(longpath) - 1);
   longpath[sizeof(longpath) - 1] = 0;
   TEST_ASSERT_EQUAL_INT(-1, ds1_read_checked(longpath, 0, 0, 0, err, sizeof(err)));
   TEST_ASSERT_EQUAL_CHAR(0, glb_ds1[0].name[0]);
}

void test_checked_read_rejects_a_used_slot_without_touching_it(void)
{
   FX_SPEC s;
   char err[256];
   fx_default(&s, 18);
   write_spec(&s, -1);
   TEST_ASSERT_EQUAL_INT(0, ds1_read_checked(path, 0, 0, 0, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, ds1_read_checked(path, 0, 0, 0, err, sizeof(err)));
   ds1_expect_matches_spec(0, &s, DS1X_AS_READ);
   TEST_ASSERT_EQUAL_INT(-1, ds1_read_checked(path, DS1_MAX, 0, 0, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(-1, ds1_read_checked(path, -1, 0, 0, err, sizeof(err)));
}

int main(int argc, char **argv)
{
   pkgc_test_filter = argc > 1 ? argv[1] : NULL;
   UNITY_BEGIN();
   PKGC_RUN(test_encoder_offsets_match_the_documented_layout);
   PKGC_RUN(test_version_2_reads_every_field);
   PKGC_RUN(test_version_3_reads_every_field);
   PKGC_RUN(test_version_4_reads_every_field);
   PKGC_RUN(test_version_5_reads_every_field);
   PKGC_RUN(test_version_6_reads_every_field);
   PKGC_RUN(test_version_7_reads_every_field);
   PKGC_RUN(test_version_8_reads_every_field);
   PKGC_RUN(test_version_9_reads_every_field);
   PKGC_RUN(test_version_10_reads_every_field);
   PKGC_RUN(test_version_12_reads_every_field);
   PKGC_RUN(test_version_13_reads_every_field);
   PKGC_RUN(test_version_14_reads_every_field);
   PKGC_RUN(test_version_15_reads_every_field);
   PKGC_RUN(test_version_16_reads_every_field);
   PKGC_RUN(test_version_17_reads_every_field);
   PKGC_RUN(test_version_18_reads_every_field);
   PKGC_RUN(test_tag_type_zero_has_no_tag_layer_and_no_groups);
   PKGC_RUN(test_tag_type_two_reads_tag_layer_and_groups);
   PKGC_RUN(test_zero_objects_and_zero_paths);
   PKGC_RUN(test_wall_and_floor_counts_may_be_zero);
   PKGC_RUN(test_four_walls_two_floors_keep_distinct_layer_order);
   PKGC_RUN(test_legacy_orientation_table_covers_all_values);
   PKGC_RUN(test_act_is_clamped_to_five);
   PKGC_RUN(test_objects_outside_the_map_are_kept);
   PKGC_RUN(test_paths_of_two_objects_on_one_subtile_are_discarded);
   PKGC_RUN(test_unmatched_path_block_is_not_given_to_another_object);
   PKGC_RUN(test_resize_on_load_crops_and_extends);
   PKGC_RUN(test_trailing_zero_dword_after_v13_is_ignored);
   PKGC_RUN(test_group_block_cut_after_last_tile_x_still_loads);
   PKGC_RUN(test_v18_missing_npc_count_reads_as_no_paths);
   PKGC_RUN(test_v18_group_block_cut_short_reads_as_zero_and_no_paths);
   PKGC_RUN(test_rejects_empty_file);
   PKGC_RUN(test_rejects_header_shorter_than_three_dwords);
   PKGC_RUN(test_rejects_unterminated_filename);
   PKGC_RUN(test_rejects_filename_count_beyond_file);
   PKGC_RUN(test_rejects_negative_filename_count);
   PKGC_RUN(test_rejects_more_than_four_wall_layers);
   PKGC_RUN(test_rejects_negative_wall_layer_count);
   PKGC_RUN(test_rejects_more_than_two_floor_layers);
   PKGC_RUN(test_rejects_negative_dimensions);
   PKGC_RUN(test_rejects_dimensions_larger_than_the_file);
   PKGC_RUN(test_rejects_truncated_tile_layers);
   PKGC_RUN(test_rejects_object_count_beyond_file);
   PKGC_RUN(test_rejects_negative_object_count);
   PKGC_RUN(test_rejects_truncated_object_record);
   PKGC_RUN(test_rejects_group_count_far_beyond_file);
   PKGC_RUN(test_rejects_negative_group_count);
   PKGC_RUN(test_rejects_truncated_path_points);
   PKGC_RUN(test_rejects_negative_path_count);
   PKGC_RUN(test_rejects_unmatched_path_block_running_past_the_end);
   PKGC_RUN(test_long_basename_does_not_overrun_neighbouring_state);
   PKGC_RUN(test_file_without_directory_keeps_its_name);
   PKGC_RUN(test_checked_read_reports_and_leaves_the_slot_reusable);
   PKGC_RUN(test_checked_read_rejects_a_path_longer_than_the_name_field);
   PKGC_RUN(test_checked_read_rejects_a_used_slot_without_touching_it);
   return UNITY_END();
}
