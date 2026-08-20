#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"

#include "core/compose_scale.h"

void setUp(void) {}
void tearDown(void) {}

/* 2x2 RGBA fixture: top-left red, top-right green,
 * bottom-left blue, bottom-right white. */
static const unsigned char SRC_2X2[] = {
   0xFF, 0x00, 0x00, 0xFF,   /* (0,0) red */
   0x00, 0xFF, 0x00, 0xFF,   /* (1,0) green */
   0x00, 0x00, 0xFF, 0xFF,   /* (0,1) blue */
   0xFF, 0xFF, 0xFF, 0xFF    /* (1,1) white */
};

static void assert_pixel(const unsigned char *buf, int w, int x, int y,
                         unsigned char r, unsigned char g, unsigned char b,
                         unsigned char a)
{
   const unsigned char *p = buf + ((size_t) y * w + x) * 4;
   TEST_ASSERT_EQUAL_UINT8(r, p[0]);
   TEST_ASSERT_EQUAL_UINT8(g, p[1]);
   TEST_ASSERT_EQUAL_UINT8(b, p[2]);
   TEST_ASSERT_EQUAL_UINT8(a, p[3]);
}

static void test_scale_1x_is_identity(void)
{
   unsigned char dst[sizeof(SRC_2X2)];
   memset(dst, 0xAA, sizeof(dst));
   TEST_ASSERT_EQUAL_INT(1,
      compose_scale_nn_rgba(SRC_2X2, 2, 2, dst, 1));
   TEST_ASSERT_EQUAL_INT(0,
      memcmp(SRC_2X2, dst, sizeof(SRC_2X2)));
}

static void test_scale_2x_block_per_source_pixel(void)
{
   /* 2x scale: each source pixel becomes a 2x2 block. */
   unsigned char dst[4 * 4 * 4];
   int dst_w = 4;
   memset(dst, 0xAA, sizeof(dst));
   TEST_ASSERT_EQUAL_INT(1,
      compose_scale_nn_rgba(SRC_2X2, 2, 2, dst, 2));

   /* Top-left 2x2 block should all be red. */
   assert_pixel(dst, dst_w, 0, 0, 0xFF, 0x00, 0x00, 0xFF);
   assert_pixel(dst, dst_w, 1, 0, 0xFF, 0x00, 0x00, 0xFF);
   assert_pixel(dst, dst_w, 0, 1, 0xFF, 0x00, 0x00, 0xFF);
   assert_pixel(dst, dst_w, 1, 1, 0xFF, 0x00, 0x00, 0xFF);
   /* Top-right green. */
   assert_pixel(dst, dst_w, 2, 0, 0x00, 0xFF, 0x00, 0xFF);
   assert_pixel(dst, dst_w, 3, 1, 0x00, 0xFF, 0x00, 0xFF);
   /* Bottom-left blue. */
   assert_pixel(dst, dst_w, 0, 2, 0x00, 0x00, 0xFF, 0xFF);
   assert_pixel(dst, dst_w, 1, 3, 0x00, 0x00, 0xFF, 0xFF);
   /* Bottom-right white. */
   assert_pixel(dst, dst_w, 2, 2, 0xFF, 0xFF, 0xFF, 0xFF);
   assert_pixel(dst, dst_w, 3, 3, 0xFF, 0xFF, 0xFF, 0xFF);
}

static void test_scale_4x_dimensions_and_pixel(void)
{
   /* 4x: 1x1 source -> 4x4 dst. Use a single mid-grey pixel. */
   unsigned char src[] = { 0x80, 0x80, 0x80, 0xFF };
   unsigned char dst[4 * 4 * 4];
   int dx, dy;
   memset(dst, 0, sizeof(dst));
   TEST_ASSERT_EQUAL_INT(1, compose_scale_nn_rgba(src, 1, 1, dst, 4));
   for (dy = 0; dy < 4; dy++)
      for (dx = 0; dx < 4; dx++)
         assert_pixel(dst, 4, dx, dy, 0x80, 0x80, 0x80, 0xFF);
}

static void test_scale_preserves_alpha(void)
{
   unsigned char src[]  = { 0x10, 0x20, 0x30, 0x7F };
   unsigned char dst[4 * 4];  /* 1x1 src -> 2x2 dst, 4 pixels * 4 bytes */
   int i;
   compose_scale_nn_rgba(src, 1, 1, dst, 2);
   for (i = 0; i < 4; i++)
   {
      TEST_ASSERT_EQUAL_UINT8(0x10, dst[i*4+0]);
      TEST_ASSERT_EQUAL_UINT8(0x20, dst[i*4+1]);
      TEST_ASSERT_EQUAL_UINT8(0x30, dst[i*4+2]);
      TEST_ASSERT_EQUAL_UINT8(0x7F, dst[i*4+3]);
   }
}

static void test_scale_rejects_bad_args(void)
{
   unsigned char buf[4] = {0};
   TEST_ASSERT_EQUAL_INT(0, compose_scale_nn_rgba(NULL, 1, 1, buf, 2));
   TEST_ASSERT_EQUAL_INT(0, compose_scale_nn_rgba(buf, 1, 1, NULL, 2));
   TEST_ASSERT_EQUAL_INT(0, compose_scale_nn_rgba(buf, 0, 1, buf, 2));
   TEST_ASSERT_EQUAL_INT(0, compose_scale_nn_rgba(buf, 1, 0, buf, 2));
   TEST_ASSERT_EQUAL_INT(0, compose_scale_nn_rgba(buf, 1, 1, buf, 0));
   TEST_ASSERT_EQUAL_INT(0, compose_scale_nn_rgba(buf, 1, 1, buf, -1));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_scale_1x_is_identity);
   RUN_TEST(test_scale_2x_block_per_source_pixel);
   RUN_TEST(test_scale_4x_dimensions_and_pixel);
   RUN_TEST(test_scale_preserves_alpha);
   RUN_TEST(test_scale_rejects_bad_args);
   return UNITY_END();
}
