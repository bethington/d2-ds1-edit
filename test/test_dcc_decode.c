/* DCC decoding through the shared raw seam (dcc_decode_indices), the bit
 * reader it is built on, header validation, and the bitmap entry point
 * (dcc_decode) that the editor's other consumers use.
 *
 * Expected frames come from scripts/equivalence/decoder_cases.py, an
 * independent encoder of authored frames; this decoder never produces its own
 * expectations. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "core/dcc.h"
#include "core/index_frame.h"
#include "visual_test_support.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- bit reader ----------------------------------------------------------- */

static void make_bs(DCC_BITSTREAM_S *bs, UBYTE *data, UDWORD bits)
{
   memset(bs, 0, sizeof(*bs));
   bs->data = data;
   bs->size = bits;
}

void test_signed_fields_sign_extend_at_every_width(void)
{
   UBYTE data[8];
   DCC_BITSTREAM_S bs;
   UDWORD v;

   /* 32-bit signed -5, then a 1-bit signed 1 (= -1), then 3 bits 0b101 */
   memset(data, 0, sizeof(data));
   data[0] = 0xFB; data[1] = 0xFF; data[2] = 0xFF; data[3] = 0xFF;
   data[4] = 0x01 | (0x05 << 1);
   make_bs(&bs, data, 64);
   TEST_ASSERT_EQUAL_INT(0, dcc_read_bits(&bs, 32, TRUE, &v));
   TEST_ASSERT_EQUAL_INT32(-5, (int32_t) v);
   TEST_ASSERT_EQUAL_INT(0, dcc_read_bits(&bs, 1, TRUE, &v));
   TEST_ASSERT_EQUAL_INT32(-1, (int32_t) v);
   TEST_ASSERT_EQUAL_INT(0, dcc_read_bits(&bs, 3, TRUE, &v));
   TEST_ASSERT_EQUAL_INT32(-3, (int32_t) v);

   /* a positive 32-bit signed value must stay itself */
   memset(data, 0, sizeof(data));
   data[0] = 0x34; data[1] = 0x12; data[3] = 0x7F;
   make_bs(&bs, data, 64);
   TEST_ASSERT_EQUAL_INT(0, dcc_read_bits(&bs, 32, TRUE, &v));
   TEST_ASSERT_EQUAL_HEX32(0x7F001234, v);

   /* a zero-width signed field reads nothing and is 0 */
   make_bs(&bs, data, 64);
   TEST_ASSERT_EQUAL_INT(0, dcc_read_bits(&bs, 0, TRUE, &v));
   TEST_ASSERT_EQUAL_UINT32(0, v);
   TEST_ASSERT_EQUAL_UINT32(0, bs.cur_bit_num);
}

void test_fields_cross_byte_boundaries_lsb_first(void)
{
   UBYTE data[4] = { 0xA0, 0x5B, 0x00, 0x00 };   /* bits: 0000 0101 1101 1010 ... */
   DCC_BITSTREAM_S bs;
   UDWORD v;

   make_bs(&bs, data, 32);
   TEST_ASSERT_EQUAL_INT(0, dcc_read_bits(&bs, 5, FALSE, &v));
   TEST_ASSERT_EQUAL_UINT32(0x00, v);
   TEST_ASSERT_EQUAL_INT(0, dcc_read_bits(&bs, 7, FALSE, &v));   /* bits 5..11 */
   TEST_ASSERT_EQUAL_UINT32(0x5D, v);
   TEST_ASSERT_EQUAL_INT(1, dcc_read_bits(&bs, 21, FALSE, &v));  /* past the end */
}

/* ---- header validation ------------------------------------------------------ */

static void put32(UBYTE *p, int32_t v)
{
   p[0] = (UBYTE) v; p[1] = (UBYTE) (v >> 8); p[2] = (UBYTE) (v >> 16); p[3] = (UBYTE) (v >> 24);
}

static void assert_rejected(const UBYTE *bytes, size_t len, int direction, const char *why)
{
   DS1EDIT_INDEX_FRAME *frames = (DS1EDIT_INDEX_FRAME *) 1;
   size_t count = 5;
   char err[600] = "";

   TEST_ASSERT_EQUAL_INT_MESSAGE(0, dcc_decode_indices(bytes, len, direction, &frames,
                                                       &count, err, sizeof(err)), why);
   TEST_ASSERT_NULL(frames);
   TEST_ASSERT_EQUAL_size_t(0, count);
   TEST_ASSERT_TRUE_MESSAGE(err[0] != 0, why);
}

void test_headers_beyond_the_decoder_tables_are_rejected(void)
{
   /* 33 directions would overrun dir_offset[32]; 257 frames frame[][256]. */
   UBYTE buf[16 + 4 * 40];
   int d;

   memset(buf, 0, sizeof(buf));
   buf[0] = 0x74; buf[1] = 6; buf[2] = 33;
   put32(buf + 3, 1); put32(buf + 7, 1);
   for (d = 0; d < 33; d++)
      put32(buf + 15 + 4 * d, (int32_t) sizeof(buf) - 1);
   assert_rejected(buf, sizeof(buf), 0, "33 directions");

   buf[2] = 1;
   put32(buf + 3, 257);
   assert_rejected(buf, sizeof(buf), 0, "257 frames per direction");
   put32(buf + 3, 0);
   assert_rejected(buf, sizeof(buf), 0, "0 frames per direction");

   put32(buf + 3, 1);
   put32(buf + 15, 5000);                       /* direction past the file */
   assert_rejected(buf, sizeof(buf), 0, "direction offset past the file");
   put32(buf + 15, 3);                          /* direction inside the header */
   assert_rejected(buf, sizeof(buf), 0, "direction offset inside the header");

   assert_rejected(buf, 10, 0, "truncated header");
   buf[0] = 0x00;
   put32(buf + 15, 19);
   assert_rejected(buf, sizeof(buf), 0, "bad signature");
}

/* ---- fixtures ---------------------------------------------------------------- */

static char *slurp(const char *path, size_t *len)
{
   char *buf = (char *) visual_read_file(path, len);
   TEST_ASSERT_NOT_NULL_MESSAGE(buf, path);
   buf[*len] = 0;
   return buf;
}

static long json_int_after(const char *p, const char *key)
{
   const char *q = strstr(p, key);
   TEST_ASSERT_NOT_NULL_MESSAGE(q, key);
   return strtol(strchr(q, ':') + 1, NULL, 10);
}

/* Locate a case record in expected.json (one-space indent per level). */
static const char *find_case(const char *json, const char *case_id, char *file,
                             int *direction)
{
   char key[128];
   const char *rec;

   snprintf(key, sizeof(key), "\"id\": \"%s\"", case_id);
   rec = strstr(json, key);
   TEST_ASSERT_NOT_NULL_MESSAGE(rec, case_id);
   while (rec > json && strncmp(rec, "\n  {", 4) != 0)
      rec--;
   *direction = (int) json_int_after(rec, "\"direction\"");
   sscanf(strstr(rec, "\"file\": \"") + 9, "%63[^\"]", file);
   return rec;
}

static UBYTE *load_case(const char *case_id, size_t *len, int *direction,
                        char **json_out, const char **rec_out)
{
   char path[512], file[64];
   size_t json_len;
   char *json = slurp(DS1EDIT_DECODER_FIXTURES "/expected.json", &json_len);

   *rec_out = find_case(json, case_id, file, direction);
   *json_out = json;
   snprintf(path, sizeof(path), "%s/%s", DS1EDIT_DECODER_FIXTURES, file);
   return (UBYTE *) slurp(path, len);
}

static void check_frames_against_record(const char *case_id, const char *rec,
                                        const DS1EDIT_INDEX_FRAME *frames, size_t count)
{
   const char *p = strstr(rec, "\"frames\": [");
   size_t i, k, expected_count = 0;
   const char *q;

   /* count the frame objects of this record (they end where "id" begins) */
   for (q = p; q != NULL && q < strstr(rec, "\"id\""); q = strstr(q + 1, "\"pixels_hex\""))
      if (q != p) expected_count++;
   TEST_ASSERT_EQUAL_size_t_MESSAGE(expected_count, count, case_id);

   for (i = 0; i < count; i++)
   {
      const char *hex;
      p = strchr(p + 1, '{');
      TEST_ASSERT_EQUAL_INT32_MESSAGE(json_int_after(p, "\"direction\""), frames[i].direction, case_id);
      TEST_ASSERT_EQUAL_INT32_MESSAGE(json_int_after(p, "\"frame\""), frames[i].frame, case_id);
      TEST_ASSERT_EQUAL_INT32_MESSAGE(json_int_after(p, "\"width\""), frames[i].width, case_id);
      TEST_ASSERT_EQUAL_INT32_MESSAGE(json_int_after(p, "\"height\""), frames[i].height, case_id);
      TEST_ASSERT_EQUAL_INT32_MESSAGE(json_int_after(p, "\"origin_x\""), frames[i].origin_x, case_id);
      TEST_ASSERT_EQUAL_INT32_MESSAGE(json_int_after(p, "\"origin_y\""), frames[i].origin_y, case_id);
      hex = strstr(p, "\"pixels_hex\": \"") + 15;
      for (k = 0; k < frames[i].pixels_len; k++)
      {
         unsigned v;
         char msg[160];
         sscanf(hex + 2 * k, "%2x", &v);
         snprintf(msg, sizeof(msg), "%s frame %u pixel (%d,%d)", case_id, (unsigned) i,
                  (int) (k % frames[i].width), (int) (k / frames[i].width));
         TEST_ASSERT_EQUAL_HEX8_MESSAGE(v, frames[i].pixels[k], msg);
      }
   }
}

static void check_fixture_case(const char *case_id)
{
   size_t len, count;
   int direction;
   char *json;
   const char *rec;
   UBYTE *dcc = load_case(case_id, &len, &direction, &json, &rec);
   DS1EDIT_INDEX_FRAME *frames;
   char err[600];

   TEST_ASSERT_EQUAL_INT_MESSAGE(1, dcc_decode_indices(dcc, len, direction, &frames,
                                                       &count, err, sizeof(err)), err);
   check_frames_against_record(case_id, rec, frames, count);
   ds1edit_index_frames_free(frames, count);
   free(dcc);
   free(json);
}

void test_fixture_single_frame(void)    { check_fixture_case("dcc.single_frame"); }
void test_fixture_sequence(void)        { check_fixture_case("dcc.sequence"); }
void test_fixture_no_equal_cells(void)  { check_fixture_case("dcc.no_equal_cells"); }
void test_fixture_empty_direction(void) { check_fixture_case("dcc.empty_direction"); }

void test_single_direction_of_a_multi_direction_file(void)
{
   size_t len, count, all_count, i;
   int direction;
   char *json;
   const char *rec;
   UBYTE *dcc = load_case("dcc.sequence", &len, &direction, &json, &rec);
   DS1EDIT_INDEX_FRAME *one, *all;

   TEST_ASSERT_EQUAL_INT(1, dcc_decode_indices(dcc, len, 1, &one, &count, NULL, 0));
   TEST_ASSERT_EQUAL_INT(1, dcc_decode_indices(dcc, len, DS1EDIT_ALL_DIRECTIONS,
                                               &all, &all_count, NULL, 0));
   TEST_ASSERT_EQUAL_size_t(3, count);
   for (i = 0; i < count; i++)
   {
      TEST_ASSERT_EQUAL_INT32(1, one[i].direction);
      TEST_ASSERT_EQUAL_INT32(all[3 + i].origin_x, one[i].origin_x);
      TEST_ASSERT_EQUAL_HEX8_ARRAY(all[3 + i].pixels, one[i].pixels, one[i].pixels_len);
   }
   assert_rejected(dcc, len, 2, "no direction 2");
   ds1edit_index_frames_free(one, count);
   ds1edit_index_frames_free(all, all_count);
   free(dcc);
   free(json);
}

void test_signed_offsets_survive_in_the_frame_header(void)
{
   /* dcc.sequence direction 0 sits at x = -300; on LP64 hosts the offsets
    * used to be `long` filled through a 32-bit reader and read ~4e9. */
   size_t len;
   int direction;
   char *json;
   const char *rec;
   UBYTE *bytes = load_case("dcc.sequence", &len, &direction, &json, &rec);
   DCC_S *dcc = dcc_mem_load(bytes, (int) len);

   TEST_ASSERT_NOT_NULL(dcc);
   TEST_ASSERT_EQUAL_INT(0, dcc_decode_raw(dcc, 1L));
   TEST_ASSERT_EQUAL_INT32(-300, dcc->frame[0][0].xoffset);
   TEST_ASSERT_EQUAL_INT32(-113, dcc->frame[0][0].yoffset);
   TEST_ASSERT_EQUAL_INT32(-300, (int32_t) dcc->direction[0].box.xmin);
   TEST_ASSERT_EQUAL_INT32(-120, (int32_t) dcc->direction[0].box.ymin);
   dcc_destroy(dcc);
   free(bytes);
   free(json);
}

/* ---- the bitmap entry point uses the same indices ---------------------------- */

void test_bitmap_frames_are_the_index_frames_through_the_palette(void)
{
   /* A palette that repeats colours: the bitmap must still be exactly
    * palette[index], i.e. built from the indices, not re-derived. */
   static RGBA_PALETTE pal;
   size_t len, count, i;
   int direction, x, y, c;
   char *json;
   const char *rec;
   UBYTE *bytes = load_case("dcc.sequence", &len, &direction, &json, &rec);
   DS1EDIT_INDEX_FRAME *frames;
   DCC_S *dcc;

   TEST_ASSERT_TRUE(visual_allegro_init());
   for (c = 0; c < 256; c++)
   {
      pal.colors[c].r = (UBYTE) (c & 0x0F);       /* 16 distinct colours only */
      pal.colors[c].g = (UBYTE) (255 - (c & 0x0F));
      pal.colors[c].b = 7;
      pal.colors[c].a = (c == 0) ? 0 : 255;
   }
   a5_current_palette = &pal;

   TEST_ASSERT_EQUAL_INT(1, dcc_decode_indices(bytes, len, DS1EDIT_ALL_DIRECTIONS,
                                               &frames, &count, NULL, 0));
   dcc = dcc_mem_load(bytes, (int) len);
   TEST_ASSERT_EQUAL_INT(0, dcc_decode(dcc, 0x3));
   for (i = 0; i < count; i++)
   {
      ALLEGRO_BITMAP *bmp = dcc->frame[frames[i].direction][frames[i].frame].bmp;
      TEST_ASSERT_NOT_NULL(bmp);
      TEST_ASSERT_EQUAL_INT(frames[i].width, al_get_bitmap_width(bmp));
      TEST_ASSERT_EQUAL_INT(frames[i].height, al_get_bitmap_height(bmp));
      for (y = 0; y < frames[i].height; y++)
         for (x = 0; x < frames[i].width; x++)
         {
            unsigned char r, g, b, a;
            UBYTE idx = frames[i].pixels[y * frames[i].width + x];
            al_unmap_rgba(al_get_pixel(bmp, x, y), &r, &g, &b, &a);
            TEST_ASSERT_EQUAL_UINT8(pal.colors[idx].a, a);
            if (idx != 0)
            {
               TEST_ASSERT_EQUAL_UINT8(pal.colors[idx].r, r);
               TEST_ASSERT_EQUAL_UINT8(pal.colors[idx].g, g);
               TEST_ASSERT_EQUAL_UINT8(pal.colors[idx].b, b);
            }
         }
   }
   a5_current_palette = NULL;
   dcc_destroy(dcc);
   ds1edit_index_frames_free(frames, count);
   free(bytes);
   free(json);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_signed_fields_sign_extend_at_every_width);
   RUN_TEST(test_fields_cross_byte_boundaries_lsb_first);
   RUN_TEST(test_headers_beyond_the_decoder_tables_are_rejected);
   RUN_TEST(test_fixture_single_frame);
   RUN_TEST(test_fixture_sequence);
   RUN_TEST(test_fixture_no_equal_cells);
   RUN_TEST(test_fixture_empty_direction);
   RUN_TEST(test_single_direction_of_a_multi_direction_file);
   RUN_TEST(test_signed_offsets_survive_in_the_frame_header);
   RUN_TEST(test_bitmap_frames_are_the_index_frames_through_the_palette);
   return UNITY_END();
}
