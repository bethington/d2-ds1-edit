#include <string.h>

#include "unity/unity.h"

#include "core/dc6_header.h"

void setUp(void) {}
void tearDown(void) {}

static unsigned char single_frame_dc6[24] = {
   0x06, 0x00, 0x00, 0x00,  /* version */
   0x01, 0x00, 0x00, 0x00,  /* flags */
   0x00, 0x00, 0x00, 0x00,  /* encoding */
   0x00, 0x00, 0x00, 0x00,  /* termination */
   0x01, 0x00, 0x00, 0x00,  /* directions = 1 */
   0x01, 0x00, 0x00, 0x00,  /* frames_per_direction = 1 */
};

static unsigned char multi_frame_dc6[24] = {
   0x06, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00,  /* directions = 1 */
   0x11, 0x00, 0x00, 0x00,  /* frames_per_direction = 17 (flippy animation) */
};

static unsigned char multi_direction_dc6[24] = {
   0x06, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x08, 0x00, 0x00, 0x00,  /* directions = 8 */
   0x01, 0x00, 0x00, 0x00,  /* frames_per_direction = 1 */
};

static unsigned char zero_directions_dc6[24] = {
   0x06, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,  /* directions = 0 (invalid) */
   0x01, 0x00, 0x00, 0x00,
};

static unsigned char absurd_directions_dc6[24] = {
   0x06, 0x00, 0x00, 0x00,
   0x01, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x10, 0x00,  /* directions = 1048576 (way too many) */
   0x01, 0x00, 0x00, 0x00,
};

static void test_single_frame_returns_1(void)
{
   TEST_ASSERT_EQUAL_INT(1, dc6_header_is_single_frame(single_frame_dc6, 24));
}

static void test_multi_frame_returns_0(void)
{
   TEST_ASSERT_EQUAL_INT(0, dc6_header_is_single_frame(multi_frame_dc6, 24));
}

static void test_multi_direction_returns_0(void)
{
   TEST_ASSERT_EQUAL_INT(0, dc6_header_is_single_frame(multi_direction_dc6, 24));
}

static void test_zero_directions_returns_negative(void)
{
   TEST_ASSERT_EQUAL_INT(-1, dc6_header_is_single_frame(zero_directions_dc6, 24));
}

static void test_absurd_directions_returns_negative(void)
{
   TEST_ASSERT_EQUAL_INT(-1, dc6_header_is_single_frame(absurd_directions_dc6, 24));
}

static void test_short_buffer_returns_negative(void)
{
   TEST_ASSERT_EQUAL_INT(-1, dc6_header_is_single_frame(single_frame_dc6, 23));
   TEST_ASSERT_EQUAL_INT(-1, dc6_header_is_single_frame(single_frame_dc6, 0));
}

static void test_null_buffer_returns_negative(void)
{
   TEST_ASSERT_EQUAL_INT(-1, dc6_header_is_single_frame(NULL, 24));
}

static void test_oversized_buffer_works(void)
{
   /* Real DC6 files are much larger; only first 24 bytes should be inspected. */
   unsigned char big[1024];
   memset(big, 0xFF, sizeof(big));
   memcpy(big, single_frame_dc6, 24);
   TEST_ASSERT_EQUAL_INT(1, dc6_header_is_single_frame(big, sizeof(big)));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_single_frame_returns_1);
   RUN_TEST(test_multi_frame_returns_0);
   RUN_TEST(test_multi_direction_returns_0);
   RUN_TEST(test_zero_directions_returns_negative);
   RUN_TEST(test_absurd_directions_returns_negative);
   RUN_TEST(test_short_buffer_returns_negative);
   RUN_TEST(test_null_buffer_returns_negative);
   RUN_TEST(test_oversized_buffer_works);
   return UNITY_END();
}
