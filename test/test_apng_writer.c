#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "unity/unity.h"

#include "core/apng_writer.h"

static const char *TEST_OUT = "test_apng_writer_out.png";

void setUp(void)
{
   remove(TEST_OUT);
}

void tearDown(void)
{
   remove(TEST_OUT);
}

static long file_size(const char *path)
{
   FILE *fp;
   long sz;
   fp = fopen(path, "rb");
   if (fp == NULL)
      return -1;
   fseek(fp, 0, SEEK_END);
   sz = ftell(fp);
   fclose(fp);
   return sz;
}

static int read_file_bytes(const char *path, unsigned char *buf, int max)
{
   FILE *fp;
   int n;
   fp = fopen(path, "rb");
   if (fp == NULL)
      return -1;
   n = (int) fread(buf, 1, max, fp);
   fclose(fp);
   return n;
}

static uint32_t read_u32_be(const unsigned char *p)
{
   return  ((uint32_t) p[0] << 24)
         | ((uint32_t) p[1] << 16)
         | ((uint32_t) p[2] <<  8)
         | ((uint32_t) p[3]);
}

/* Walk the file's chunk list and append each chunk's 4-char type to
 * `types_out`. Returns the number of chunks found, or -1 on parse
 * error. Skips the PNG signature first. */
static int collect_chunk_types(const unsigned char *buf, int buf_len,
                               char types_out[][5], int max_chunks)
{
   int pos = 8; /* after signature */
   int n = 0;
   while (pos + 8 <= buf_len)
   {
      uint32_t len = read_u32_be(buf + pos);
      if (pos + 8 + (int) len + 4 > buf_len)
         return -1;
      if (n < max_chunks)
      {
         memcpy(types_out[n], buf + pos + 4, 4);
         types_out[n][4] = 0;
      }
      n++;
      if (memcmp(buf + pos + 4, "IEND", 4) == 0)
         break;
      pos += 8 + (int) len + 4;
   }
   return n;
}

static void make_solid_frame(unsigned char *out, int w, int h,
                             unsigned char r, unsigned char g,
                             unsigned char b, unsigned char a)
{
   int i;
   for (i = 0; i < w * h; i++)
   {
      out[i * 4 + 0] = r;
      out[i * 4 + 1] = g;
      out[i * 4 + 2] = b;
      out[i * 4 + 3] = a;
   }
}

static void test_signature_and_basic_chunks(void)
{
   APNG_WRITER_S *w;
   unsigned char frame[2 * 2 * 4];
   unsigned char buf[1024];
   int n;
   char types[16][5];
   int chunk_count;

   make_solid_frame(frame, 2, 2, 255, 0, 0, 255);

   w = apng_writer_open(TEST_OUT, 2, 2, 2, 0);
   TEST_ASSERT_NOT_NULL(w);
   TEST_ASSERT_EQUAL_INT(1, apng_writer_write_frame(w, frame, 1, 25));
   TEST_ASSERT_EQUAL_INT(1, apng_writer_write_frame(w, frame, 1, 25));
   TEST_ASSERT_EQUAL_INT(1, apng_writer_close(w));

   n = read_file_bytes(TEST_OUT, buf, sizeof(buf));
   TEST_ASSERT_TRUE(n > 0);

   /* PNG signature */
   TEST_ASSERT_EQUAL_HEX8(0x89, buf[0]);
   TEST_ASSERT_EQUAL_HEX8('P', buf[1]);
   TEST_ASSERT_EQUAL_HEX8('N', buf[2]);
   TEST_ASSERT_EQUAL_HEX8('G', buf[3]);

   chunk_count = collect_chunk_types(buf, n, types, 16);
   TEST_ASSERT_TRUE(chunk_count >= 6);

   /* Expected order: IHDR, acTL, fcTL, IDAT, fcTL, fdAT, IEND. */
   TEST_ASSERT_EQUAL_STRING("IHDR", types[0]);
   TEST_ASSERT_EQUAL_STRING("acTL", types[1]);
   TEST_ASSERT_EQUAL_STRING("fcTL", types[2]);
   TEST_ASSERT_EQUAL_STRING("IDAT", types[3]);
   TEST_ASSERT_EQUAL_STRING("fcTL", types[4]);
   TEST_ASSERT_EQUAL_STRING("fdAT", types[5]);
}

static void test_acTL_records_frame_count(void)
{
   APNG_WRITER_S *w;
   unsigned char frame[1 * 1 * 4] = { 64, 128, 200, 255 };
   unsigned char buf[512];
   int i, n;
   uint32_t num_frames, num_plays;

   w = apng_writer_open(TEST_OUT, 1, 1, 3, 0);
   TEST_ASSERT_NOT_NULL(w);
   for (i = 0; i < 3; i++)
      TEST_ASSERT_EQUAL_INT(1, apng_writer_write_frame(w, frame, 40, 1000));
   TEST_ASSERT_EQUAL_INT(1, apng_writer_close(w));

   n = read_file_bytes(TEST_OUT, buf, sizeof(buf));
   TEST_ASSERT_TRUE(n > 0);

   /* Find acTL chunk and parse num_frames + num_plays. */
   {
      int pos = 8;
      int found = 0;
      while (pos + 8 <= n)
      {
         uint32_t len = read_u32_be(buf + pos);
         if (memcmp(buf + pos + 4, "acTL", 4) == 0)
         {
            num_frames = read_u32_be(buf + pos + 8);
            num_plays  = read_u32_be(buf + pos + 12);
            found = 1;
            break;
         }
         pos += 8 + (int) len + 4;
      }
      TEST_ASSERT_TRUE(found);
   }

   TEST_ASSERT_EQUAL_UINT32(3, num_frames);
   TEST_ASSERT_EQUAL_UINT32(0, num_plays);
}

static void test_open_rejects_bad_args(void)
{
   TEST_ASSERT_NULL(apng_writer_open(NULL, 1, 1, 1, 0));
   TEST_ASSERT_NULL(apng_writer_open(TEST_OUT, 0, 1, 1, 0));
   TEST_ASSERT_NULL(apng_writer_open(TEST_OUT, 1, 0, 1, 0));
   TEST_ASSERT_NULL(apng_writer_open(TEST_OUT, 1, 1, 0, 0));
}

static void test_write_frame_after_count_fails(void)
{
   APNG_WRITER_S *w;
   unsigned char frame[1 * 1 * 4] = { 0, 0, 0, 255 };

   w = apng_writer_open(TEST_OUT, 1, 1, 1, 0);
   TEST_ASSERT_NOT_NULL(w);
   TEST_ASSERT_EQUAL_INT(1, apng_writer_write_frame(w, frame, 1, 25));
   TEST_ASSERT_EQUAL_INT(0, apng_writer_write_frame(w, frame, 1, 25));
   apng_writer_close(w);
}

static void test_close_fails_if_underfilled(void)
{
   APNG_WRITER_S *w;
   unsigned char frame[1 * 1 * 4] = { 0, 0, 0, 255 };

   w = apng_writer_open(TEST_OUT, 1, 1, 5, 0);
   TEST_ASSERT_NOT_NULL(w);
   TEST_ASSERT_EQUAL_INT(1, apng_writer_write_frame(w, frame, 1, 25));
   /* Only wrote 1 of 5 frames; close should report failure. */
   TEST_ASSERT_EQUAL_INT(0, apng_writer_close(w));
}

static void test_file_grows_with_frame_count(void)
{
   APNG_WRITER_S *w;
   unsigned char frame[8 * 8 * 4];
   long size_2_frames, size_5_frames;
   int i;

   make_solid_frame(frame, 8, 8, 100, 150, 200, 255);

   w = apng_writer_open(TEST_OUT, 8, 8, 2, 0);
   TEST_ASSERT_NOT_NULL(w);
   for (i = 0; i < 2; i++)
      TEST_ASSERT_EQUAL_INT(1, apng_writer_write_frame(w, frame, 1, 25));
   apng_writer_close(w);
   size_2_frames = file_size(TEST_OUT);

   remove(TEST_OUT);

   w = apng_writer_open(TEST_OUT, 8, 8, 5, 0);
   TEST_ASSERT_NOT_NULL(w);
   for (i = 0; i < 5; i++)
      TEST_ASSERT_EQUAL_INT(1, apng_writer_write_frame(w, frame, 1, 25));
   apng_writer_close(w);
   size_5_frames = file_size(TEST_OUT);

   TEST_ASSERT_TRUE(size_2_frames > 0);
   TEST_ASSERT_TRUE(size_5_frames > size_2_frames);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_signature_and_basic_chunks);
   RUN_TEST(test_acTL_records_frame_count);
   RUN_TEST(test_open_rejects_bad_args);
   RUN_TEST(test_write_frame_after_count_fails);
   RUN_TEST(test_close_fails_if_underfilled);
   RUN_TEST(test_file_grows_with_frame_count);
   return UNITY_END();
}
