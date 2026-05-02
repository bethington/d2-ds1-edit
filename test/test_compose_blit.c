#include <string.h>

#include "unity/unity.h"

#include "core/compose_blit.h"

void setUp(void) {}
void tearDown(void) {}

static unsigned char dst_buf[16 * 16 * 4];
static unsigned char src_buf[8  *  8 * 4];

static void fill_rgba(unsigned char *buf, int w, int h,
                      unsigned char r, unsigned char g,
                      unsigned char b, unsigned char a)
{
   int i;
   for (i = 0; i < w * h; i++)
   {
      buf[i * 4 + 0] = r;
      buf[i * 4 + 1] = g;
      buf[i * 4 + 2] = b;
      buf[i * 4 + 3] = a;
   }
}

static void get_pixel(const unsigned char *buf, int w, int x, int y,
                      unsigned char *r, unsigned char *g,
                      unsigned char *b, unsigned char *a)
{
   const unsigned char *p = buf + (y * w + x) * 4;
   *r = p[0]; *g = p[1]; *b = p[2]; *a = p[3];
}

static void test_blit_full_opaque_at_origin(void)
{
   unsigned char r, g, b, a;

   memset(dst_buf, 0, sizeof(dst_buf));
   fill_rgba(src_buf, 8, 8, 255, 100, 50, 255);

   compose_blit_rgba(dst_buf, 16, 16, src_buf, 8, 8, 0, 0);

   /* Pixel (0,0) and (7,7) should be opaque red-ish. */
   get_pixel(dst_buf, 16, 0, 0, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(255, r);
   TEST_ASSERT_EQUAL_UINT8(100, g);
   TEST_ASSERT_EQUAL_UINT8(50,  b);
   TEST_ASSERT_EQUAL_UINT8(255, a);

   get_pixel(dst_buf, 16, 7, 7, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(255, r);

   /* Pixel (8,8) should still be zero. */
   get_pixel(dst_buf, 16, 8, 8, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(0, r);
   TEST_ASSERT_EQUAL_UINT8(0, a);
}

static void test_blit_with_offset(void)
{
   unsigned char r, g, b, a;

   memset(dst_buf, 0, sizeof(dst_buf));
   fill_rgba(src_buf, 8, 8, 80, 160, 240, 255);

   compose_blit_rgba(dst_buf, 16, 16, src_buf, 8, 8, 4, 4);

   /* Pixel (4,4) should now be src; (3,3) still zero. */
   get_pixel(dst_buf, 16, 3, 3, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(0, a);

   get_pixel(dst_buf, 16, 4, 4, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(80,  r);
   TEST_ASSERT_EQUAL_UINT8(160, g);
   TEST_ASSERT_EQUAL_UINT8(240, b);
   TEST_ASSERT_EQUAL_UINT8(255, a);

   get_pixel(dst_buf, 16, 11, 11, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(255, a);

   /* Pixel (12,12) should still be zero. */
   get_pixel(dst_buf, 16, 12, 12, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(0, a);
}

static void test_blit_skips_transparent_pixels(void)
{
   unsigned char r, g, b, a;
   int i;

   /* Fill src with mixed alpha: even pixels opaque, odd pixels alpha=0. */
   for (i = 0; i < 8 * 8; i++)
   {
      src_buf[i * 4 + 0] = 200;
      src_buf[i * 4 + 1] = 0;
      src_buf[i * 4 + 2] = 0;
      src_buf[i * 4 + 3] = (i % 2 == 0) ? 255 : 0;
   }

   /* Pre-fill dst with a known background. */
   fill_rgba(dst_buf, 16, 16, 30, 60, 90, 255);

   compose_blit_rgba(dst_buf, 16, 16, src_buf, 8, 8, 0, 0);

   /* Even-index source pixel (0,0) -> dst red (200, 0, 0, 255) */
   get_pixel(dst_buf, 16, 0, 0, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(200, r);

   /* Odd-index source pixel (1,0) -> dst preserves background */
   get_pixel(dst_buf, 16, 1, 0, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(30,  r);
   TEST_ASSERT_EQUAL_UINT8(60,  g);
   TEST_ASSERT_EQUAL_UINT8(90,  b);
   TEST_ASSERT_EQUAL_UINT8(255, a);
}

static void test_blit_clips_negative_offset(void)
{
   unsigned char r, g, b, a;

   memset(dst_buf, 0, sizeof(dst_buf));
   fill_rgba(src_buf, 8, 8, 255, 0, 0, 255);

   /* dst_x = -4: only the right half of src lands in dst. */
   compose_blit_rgba(dst_buf, 16, 16, src_buf, 8, 8, -4, 0);

   /* dst pixel (0,0) corresponds to src (4,0), should be set. */
   get_pixel(dst_buf, 16, 0, 0, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(255, r);
   TEST_ASSERT_EQUAL_UINT8(255, a);

   /* dst pixel (3,0) corresponds to src (7,0), should be set. */
   get_pixel(dst_buf, 16, 3, 0, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(255, r);

   /* dst pixel (4,0) is past src; should still be zero. */
   get_pixel(dst_buf, 16, 4, 0, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(0, a);
}

static void test_blit_clips_overflow(void)
{
   unsigned char r, g, b, a;

   memset(dst_buf, 0, sizeof(dst_buf));
   fill_rgba(src_buf, 8, 8, 255, 0, 0, 255);

   /* Place src at (12, 12) of a 16x16 dst: only the top-left 4x4 lands. */
   compose_blit_rgba(dst_buf, 16, 16, src_buf, 8, 8, 12, 12);

   /* (12,12) -> set */
   get_pixel(dst_buf, 16, 12, 12, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(255, a);

   /* (15,15) -> set (last in-bounds pixel from src(3,3)) */
   get_pixel(dst_buf, 16, 15, 15, &r, &g, &b, &a);
   TEST_ASSERT_EQUAL_UINT8(255, a);
}

static void test_blit_fully_outside_no_op(void)
{
   memset(dst_buf, 0, sizeof(dst_buf));
   fill_rgba(src_buf, 8, 8, 255, 0, 0, 255);

   /* src placed entirely past dst right edge. */
   compose_blit_rgba(dst_buf, 16, 16, src_buf, 8, 8, 100, 100);

   /* dst should still be all zeros. */
   {
      int i;
      int sum = 0;
      for (i = 0; i < 16 * 16 * 4; i++)
         sum += dst_buf[i];
      TEST_ASSERT_EQUAL_INT(0, sum);
   }
}

static void test_blit_null_inputs_are_safe(void)
{
   memset(dst_buf, 0, sizeof(dst_buf));
   fill_rgba(src_buf, 8, 8, 255, 0, 0, 255);

   /* These calls should not crash. */
   compose_blit_rgba(NULL, 16, 16, src_buf, 8, 8, 0, 0);
   compose_blit_rgba(dst_buf, 16, 16, NULL, 8, 8, 0, 0);
   compose_blit_rgba(dst_buf, 0, 16, src_buf, 8, 8, 0, 0);
   compose_blit_rgba(dst_buf, 16, 16, src_buf, 0, 8, 0, 0);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_blit_full_opaque_at_origin);
   RUN_TEST(test_blit_with_offset);
   RUN_TEST(test_blit_skips_transparent_pixels);
   RUN_TEST(test_blit_clips_negative_offset);
   RUN_TEST(test_blit_clips_overflow);
   RUN_TEST(test_blit_fully_outside_no_op);
   RUN_TEST(test_blit_null_inputs_are_safe);
   return UNITY_END();
}
