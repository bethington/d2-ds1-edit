/* DC6 raw decoding through the shared seam (dc6_decode_indices) and the
 * direction layout every consumer uses (ds1edit_index_frames_to_canvases).
 *
 * Inputs are authored bytes: small streams built here for the opcode rules,
 * and the generated fixtures in test/equivalence/fixtures/decoders whose
 * expected frames come from scripts/equivalence/decoder_cases.py (an
 * independent encoder, not this decoder). */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "core/index_frame.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- authoring helpers ---------------------------------------------------- */

typedef struct
{
   int32_t w, h, ox, oy;      /* oy = DC6 offsetY: the bottom row */
   const uint8_t *data;
   int32_t len;
} AUTHORED_FRAME;

static void put32(uint8_t *p, int32_t v)
{
   p[0] = (uint8_t) v;
   p[1] = (uint8_t) ((uint32_t) v >> 8);
   p[2] = (uint8_t) ((uint32_t) v >> 16);
   p[3] = (uint8_t) ((uint32_t) v >> 24);
}

/* Build a DC6 file; caller frees. */
static uint8_t *build_dc6(int dirs, int fpd, const AUTHORED_FRAME *fr, size_t *len)
{
   size_t n = (size_t) dirs * fpd, i, size = 24 + 4 * n, pos;
   uint8_t *buf;

   for (i = 0; i < n; i++)
      size += 32 + (size_t) fr[i].len + 3;
   buf = (uint8_t *) calloc(size, 1);
   put32(buf + 0, 6);
   put32(buf + 4, 1);
   put32(buf + 8, 0);
   put32(buf + 12, (int32_t) 0xEEEEEEEE);
   put32(buf + 16, dirs);
   put32(buf + 20, fpd);
   pos = 24 + 4 * n;
   for (i = 0; i < n; i++)
   {
      put32(buf + 24 + 4 * i, (int32_t) pos);
      put32(buf + pos + 4, fr[i].w);
      put32(buf + pos + 8, fr[i].h);
      put32(buf + pos + 12, fr[i].ox);
      put32(buf + pos + 16, fr[i].oy);
      put32(buf + pos + 24, (int32_t) (pos + 32 + fr[i].len + 3));
      put32(buf + pos + 28, fr[i].len);
      memcpy(buf + pos + 32, fr[i].data, (size_t) fr[i].len);
      memset(buf + pos + 32 + fr[i].len, 0xEE, 3);
      pos += 32 + (size_t) fr[i].len + 3;
   }
   *len = size;
   return buf;
}

static void assert_pixels(const uint8_t *expected, const DS1EDIT_INDEX_FRAME *f)
{
   TEST_ASSERT_EQUAL_size_t((size_t) f->width * f->height, f->pixels_len);
   TEST_ASSERT_EQUAL_INT32(f->width, f->stride);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, f->pixels, f->pixels_len);
}

/* ---- opcode rules ----------------------------------------------------------- */

void test_rows_are_bottom_up_with_literal_skip_and_row_end(void)
{
   /* 4x3 frame. Bottom row first: 2 literals, end; middle: skip 1, 3
    * literals, end; top: skip 3, 1 literal, end. */
   static const uint8_t stream[] = {
      0x02, 11, 12, 0x80,
      0x81, 0x03, 21, 22, 23, 0x80,
      0x83, 0x01, 31, 0x80 };
   static const uint8_t expected[] = {
       0,  0,  0, 31,
       0, 21, 22, 23,
      11, 12,  0,  0 };
   AUTHORED_FRAME fr = { 4, 3, -2, 5, stream, (int32_t) sizeof(stream) };
   size_t len, count = 99;
   uint8_t *dc6 = build_dc6(1, 1, &fr, &len);
   DS1EDIT_INDEX_FRAME *frames = NULL;
   char err[128];

   TEST_ASSERT_EQUAL_INT(1, dc6_decode_indices(dc6, len, 0, &frames, &count, err, sizeof(err)));
   TEST_ASSERT_EQUAL_size_t(1, count);
   TEST_ASSERT_EQUAL_INT32(4, frames[0].width);
   TEST_ASSERT_EQUAL_INT32(3, frames[0].height);
   /* origin is the top-left: offsetY is the bottom row */
   TEST_ASSERT_EQUAL_INT32(-2, frames[0].origin_x);
   TEST_ASSERT_EQUAL_INT32(5 - 3 + 1, frames[0].origin_y);
   TEST_ASSERT_EQUAL_INT32(0, frames[0].frame);
   TEST_ASSERT_EQUAL_INT32(0, frames[0].direction);
   assert_pixels(expected, &frames[0]);
   ds1edit_index_frames_free(frames, count);
   free(dc6);
}

void test_zero_length_copy_and_early_row_end_are_legal(void)
{
   static const uint8_t stream[] = { 0x00, 0x80, 0x00, 0x01, 7, 0x80 };
   static const uint8_t expected[] = { 7, 0, 0,
                                       0, 0, 0 };
   AUTHORED_FRAME fr = { 3, 2, 0, 0, stream, (int32_t) sizeof(stream) };
   size_t len, count;
   uint8_t *dc6 = build_dc6(1, 1, &fr, &len);
   DS1EDIT_INDEX_FRAME *frames;

   TEST_ASSERT_EQUAL_INT(1, dc6_decode_indices(dc6, len, 0, &frames, &count, NULL, 0));
   assert_pixels(expected, &frames[0]);
   ds1edit_index_frames_free(frames, count);
   free(dc6);
}

void test_pixels_outside_the_frame_are_dropped(void)
{
   /* A copy past the right edge, a skip past it, and more rows than the
    * frame has: none of it may land in the buffer. */
   static const uint8_t stream[] = {
      0x05, 1, 2, 3, 4, 5, 0x80,
      0x90, 0x02, 6, 7, 0x80,
      0x02, 8, 9, 0x80,
      0x02, 8, 9, 0x80 };
   static const uint8_t expected[] = { 0, 0,
                                       1, 2 };
   AUTHORED_FRAME fr = { 2, 2, 0, 0, stream, (int32_t) sizeof(stream) };
   size_t len, count;
   uint8_t *dc6 = build_dc6(1, 1, &fr, &len);
   DS1EDIT_INDEX_FRAME *frames;

   TEST_ASSERT_EQUAL_INT(1, dc6_decode_indices(dc6, len, 0, &frames, &count, NULL, 0));
   assert_pixels(expected, &frames[0]);
   ds1edit_index_frames_free(frames, count);
   free(dc6);
}

/* ---- directions, frames, layout -------------------------------------------- */

void test_one_direction_and_all_directions_keep_file_order(void)
{
   static const uint8_t s[4][3] = { {0x01, 1, 0x80}, {0x01, 2, 0x80},
                                    {0x01, 3, 0x80}, {0x01, 4, 0x80} };
   AUTHORED_FRAME fr[4] = {
      {1, 1, 0, 0, s[0], 3}, {1, 1, 1, 0, s[1], 3},
      {1, 1, 2, 0, s[2], 3}, {1, 1, 3, 0, s[3], 3} };
   size_t len, count, i;
   uint8_t *dc6 = build_dc6(2, 2, fr, &len);
   DS1EDIT_INDEX_FRAME *frames;

   TEST_ASSERT_EQUAL_INT(1, dc6_decode_indices(dc6, len, 1, &frames, &count, NULL, 0));
   TEST_ASSERT_EQUAL_size_t(2, count);
   TEST_ASSERT_EQUAL_UINT8(3, frames[0].pixels[0]);
   TEST_ASSERT_EQUAL_UINT8(4, frames[1].pixels[0]);
   TEST_ASSERT_EQUAL_INT32(1, frames[1].direction);
   TEST_ASSERT_EQUAL_INT32(1, frames[1].frame);
   ds1edit_index_frames_free(frames, count);

   TEST_ASSERT_EQUAL_INT(1, dc6_decode_indices(dc6, len, DS1EDIT_ALL_DIRECTIONS,
                                               &frames, &count, NULL, 0));
   TEST_ASSERT_EQUAL_size_t(4, count);
   for (i = 0; i < 4; i++)
   {
      TEST_ASSERT_EQUAL_UINT8(i + 1, frames[i].pixels[0]);
      TEST_ASSERT_EQUAL_INT32((int32_t) (i / 2), frames[i].direction);
      TEST_ASSERT_EQUAL_INT32((int32_t) (i % 2), frames[i].frame);
      TEST_ASSERT_EQUAL_INT32((int32_t) i, frames[i].origin_x);
   }
   ds1edit_index_frames_free(frames, count);
   free(dc6);
}

void test_direction_layout_places_frames_by_signed_origin_without_overrun(void)
{
   /* Two frames with negative, different origins. The direction canvas is
    * their union; each frame lands at origin - union min. The canvas sits in
    * a poisoned buffer to catch writes outside it. */
   static const uint8_t a_s[] = { 0x02, 5, 6, 0x80 };          /* 2x1 */
   static const uint8_t b_s[] = { 0x01, 7, 0x80, 0x01, 8, 0x80 }; /* 1x2 */
   AUTHORED_FRAME fr[2] = { {2, 1, -3, -4, a_s, 4}, {1, 2, -1, -2, b_s, 6} };
   size_t len, count;
   uint8_t *dc6 = build_dc6(1, 2, fr, &len);
   DS1EDIT_INDEX_FRAME *frames;
   uint8_t **canvases;
   int w, h, ox, oy;
   uint8_t poisoned[64];

   TEST_ASSERT_EQUAL_INT(1, dc6_decode_indices(dc6, len, 0, &frames, &count, NULL, 0));
   TEST_ASSERT_EQUAL_INT(0, ds1edit_index_frames_to_canvases(frames, count, &canvases,
                                                            &w, &h, &ox, &oy));
   /* a: x -3..-2, y -4; b: x -1, y -3..-2 -> union x -3..-1, y -4..-2 */
   TEST_ASSERT_EQUAL_INT(3, w);
   TEST_ASSERT_EQUAL_INT(3, h);
   TEST_ASSERT_EQUAL_INT(-3, ox);
   TEST_ASSERT_EQUAL_INT(-4, oy);
   {
      static const uint8_t ca[9] = { 5, 6, 0,  0, 0, 0,  0, 0, 0 };
      /* b's first scanline is its bottom row */
      static const uint8_t cb[9] = { 0, 0, 0,  0, 0, 8,  0, 0, 7 };
      TEST_ASSERT_EQUAL_HEX8_ARRAY(ca, canvases[0], 9);
      TEST_ASSERT_EQUAL_HEX8_ARRAY(cb, canvases[1], 9);
   }

   /* placement into a poisoned, larger buffer: only the 3x3 window at (2,2)
    * of an 8x8 buffer may change */
   memset(poisoned, 0xCD, sizeof(poisoned));
   {
      int x, y;
      for (y = 0; y < 3; y++)
         for (x = 0; x < 3; x++)
            poisoned[(2 + y) * 8 + 2 + x] = 0;
      ds1edit_index_frame_place(&frames[0], poisoned, 8, 8, ox - 2, oy - 2);
      ds1edit_index_frame_place(&frames[1], poisoned, 8, 8, ox - 2, oy - 2);
      for (y = 0; y < 8; y++)
         for (x = 0; x < 8; x++)
            if (x < 2 || x > 4 || y < 2 || y > 4)
               TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xCD, poisoned[y * 8 + x], "write outside the frame");
      TEST_ASSERT_EQUAL_UINT8(5, poisoned[2 * 8 + 2]);
      TEST_ASSERT_EQUAL_UINT8(7, poisoned[4 * 8 + 4]);
   }

   ds1edit_index_canvases_free(canvases, count);
   ds1edit_index_frames_free(frames, count);
   free(dc6);
}

/* ---- rejection: bad input must fail cleanly, never read out of bounds ------- */

static void assert_rejected(const uint8_t *bytes, size_t len, int direction)
{
   DS1EDIT_INDEX_FRAME *frames = (DS1EDIT_INDEX_FRAME *) 1;
   size_t count = 77;
   char err[128] = "";

   TEST_ASSERT_EQUAL_INT(0, dc6_decode_indices(bytes, len, direction, &frames,
                                               &count, err, sizeof(err)));
   TEST_ASSERT_NULL(frames);
   TEST_ASSERT_EQUAL_size_t(0, count);
   TEST_ASSERT_TRUE(err[0] != 0);
}

void test_invalid_inputs_are_rejected(void)
{
   static const uint8_t s[] = { 0x01, 1, 0x80 };
   AUTHORED_FRAME fr = { 1, 1, 0, 0, s, 3 };
   size_t len;
   uint8_t *dc6 = build_dc6(1, 1, &fr, &len);
   uint8_t *bad = (uint8_t *) malloc(len);

   assert_rejected(dc6, 23, 0);                       /* short header */
   assert_rejected(dc6, len, 1);                      /* no such direction */
   assert_rejected(dc6, len, -2);

   memcpy(bad, dc6, len); put32(bad, 5);              /* version */
   assert_rejected(bad, len, 0);
   memcpy(bad, dc6, len); put32(bad + 20, 1000);      /* table past the file */
   assert_rejected(bad, len, 0);
   memcpy(bad, dc6, len); put32(bad + 24, (int32_t) len - 8); /* frame header cut */
   assert_rejected(bad, len, 0);
   memcpy(bad, dc6, len); put32(bad + 24, -4);        /* negative offset */
   assert_rejected(bad, len, 0);
   memcpy(bad, dc6, len); put32(bad + 28 + 28, 1000); /* data length past file */
   assert_rejected(bad, len, 0);
   memcpy(bad, dc6, len); bad[28 + 32] = 0x05;        /* copy past the stream */
   assert_rejected(bad, len, 0);
   memcpy(bad, dc6, len); put32(bad + 28 + 4, -1);    /* negative width */
   assert_rejected(bad, len, 0);

   free(bad);
   free(dc6);
}

void test_null_arguments(void)
{
   DS1EDIT_INDEX_FRAME *frames;
   size_t count;
   TEST_ASSERT_EQUAL_INT(0, dc6_decode_indices(NULL, 10, 0, &frames, &count, NULL, 0));
   TEST_ASSERT_NULL(frames);
   ds1edit_index_frames_free(NULL, 3);   /* NULL-safe */
}

/* ---- generated fixtures ------------------------------------------------------ */

#ifndef DS1EDIT_DECODER_FIXTURES
#error DS1EDIT_DECODER_FIXTURES must name test/equivalence/fixtures/decoders
#endif

/* Hex for the expected pixels lives in expected.json; read the fields this
 * test needs with a deliberately small scanner. */
static char *slurp(const char *path, size_t *len)
{
   FILE *fp = fopen(path, "rb");
   char *buf;
   long n;

   TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
   fseek(fp, 0, SEEK_END);
   n = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   buf = (char *) malloc((size_t) n + 1);
   TEST_ASSERT_EQUAL_size_t((size_t) n, fread(buf, 1, (size_t) n, fp));
   buf[n] = 0;
   fclose(fp);
   if (len) *len = (size_t) n;
   return buf;
}

static long json_int_after(const char *p, const char *key)
{
   const char *q = strstr(p, key);
   TEST_ASSERT_NOT_NULL_MESSAGE(q, key);
   q = strchr(q, ':');
   return strtol(q + 1, NULL, 10);
}

static void check_fixture_case(const char *case_id)
{
   char path[512], key[128];
   size_t json_len, dc6_len, count, i;
   char *json = slurp(DS1EDIT_DECODER_FIXTURES "/expected.json", &json_len);
   const char *rec, *file_key, *frames_key, *p;
   char file[64];
   uint8_t *dc6;
   int direction;
   DS1EDIT_INDEX_FRAME *frames;

   snprintf(key, sizeof(key), "\"id\": \"%s\"", case_id);
   rec = strstr(json, key);
   TEST_ASSERT_NOT_NULL_MESSAGE(rec, case_id);
   /* expected.json is indented by one space per level: a case record opens
    * with "\n  {", a frame inside it with "\n    {" */
   while (rec > json && strncmp(rec, "\n  {", 4) != 0)
      rec--;
   direction = (int) json_int_after(rec, "\"direction\"");
   file_key = strstr(rec, "\"file\": \"");
   TEST_ASSERT_NOT_NULL(file_key);
   sscanf(file_key + 9, "%63[^\"]", file);
   snprintf(path, sizeof(path), "%s/%s", DS1EDIT_DECODER_FIXTURES, file);
   dc6 = (uint8_t *) slurp(path, &dc6_len);

   TEST_ASSERT_EQUAL_INT(1, dc6_decode_indices(dc6, dc6_len, direction, &frames,
                                               &count, NULL, 0));
   frames_key = strstr(rec, "\"frames\": [");
   TEST_ASSERT_NOT_NULL(frames_key);
   p = frames_key;
   for (i = 0; i < count; i++)
   {
      const char *hex;
      size_t k;
      p = strchr(p + 1, '{');
      TEST_ASSERT_NOT_NULL(p);
      TEST_ASSERT_EQUAL_INT32(json_int_after(p, "\"direction\""), frames[i].direction);
      TEST_ASSERT_EQUAL_INT32(json_int_after(p, "\"frame\""), frames[i].frame);
      TEST_ASSERT_EQUAL_INT32(json_int_after(p, "\"width\""), frames[i].width);
      TEST_ASSERT_EQUAL_INT32(json_int_after(p, "\"height\""), frames[i].height);
      TEST_ASSERT_EQUAL_INT32(json_int_after(p, "\"origin_x\""), frames[i].origin_x);
      TEST_ASSERT_EQUAL_INT32(json_int_after(p, "\"origin_y\""), frames[i].origin_y);
      hex = strstr(p, "\"pixels_hex\": \"") + 15;
      for (k = 0; k < frames[i].pixels_len; k++)
      {
         unsigned v;
         sscanf(hex + 2 * k, "%2x", &v);
         TEST_ASSERT_EQUAL_HEX8_MESSAGE(v, frames[i].pixels[k], case_id);
      }
   }
   ds1edit_index_frames_free(frames, count);
   free(dc6);
   free(json);
}

void test_fixture_dc6_rle_basic(void)        { check_fixture_case("dc6.rle.basic"); }
void test_fixture_dc6_multi_direction(void)  { check_fixture_case("dc6.multi_direction"); }
void test_fixture_dc6_clip(void)             { check_fixture_case("dc6.clip_and_zero_copy"); }

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_rows_are_bottom_up_with_literal_skip_and_row_end);
   RUN_TEST(test_zero_length_copy_and_early_row_end_are_legal);
   RUN_TEST(test_pixels_outside_the_frame_are_dropped);
   RUN_TEST(test_one_direction_and_all_directions_keep_file_order);
   RUN_TEST(test_direction_layout_places_frames_by_signed_origin_without_overrun);
   RUN_TEST(test_invalid_inputs_are_rejected);
   RUN_TEST(test_null_arguments);
   RUN_TEST(test_fixture_dc6_rle_basic);
   RUN_TEST(test_fixture_dc6_multi_direction);
   RUN_TEST(test_fixture_dc6_clip);
   return UNITY_END();
}
