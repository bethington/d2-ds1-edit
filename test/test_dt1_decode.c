/*
 * Unit tests for DT1 sub-tile decoding functions.
 * Tests the new buffer-based decode functions that don't depend on Allegro.
 */

#include <string.h>
#include "unity/unity.h"
#include "../src/dt1_draw.h"

void setUp(void) {}
void tearDown(void) {}


/* ---- decode_sub_tile_isometric ---- */

void test_isometric_decode_produces_diamond(void)
{
    /*
     * Isometric sub-tile is a 32x16 diamond shape, 256 bytes.
     * Row widths: 4,8,12,16,20,24,28,32,28,24,20,16,12,8,4
     * X offsets:  14,12,10,8,6,4,2,0,2,4,6,8,10,12,14
     */
    uint8_t data[256];
    uint8_t buf[32 * 16];
    int i, x, y, count;

    /* Fill with sequential non-zero values */
    for (i = 0; i < 256; i++)
        data[i] = (uint8_t)((i % 254) + 1); /* 1-254, never 0 */

    memset(buf, 0, sizeof(buf));
    decode_sub_tile_isometric(buf, 32, 16, 0, 0, data, 256);

    /* Count non-zero pixels — should be 256 */
    count = 0;
    for (i = 0; i < 32 * 16; i++)
    {
        if (buf[i] != 0)
            count++;
    }
    TEST_ASSERT_EQUAL_INT(256, count);

    /* Row 0 (top): 4 pixels starting at x=14 */
    for (x = 0; x < 14; x++)
        TEST_ASSERT_EQUAL_UINT8(0, buf[0 * 32 + x]);
    for (x = 14; x < 18; x++)
        TEST_ASSERT_NOT_EQUAL(0, buf[0 * 32 + x]);

    /* Row 7 (middle): 32 pixels starting at x=0 */
    for (x = 0; x < 32; x++)
        TEST_ASSERT_NOT_EQUAL(0, buf[7 * 32 + x]);
}


void test_isometric_wrong_length_does_nothing(void)
{
    uint8_t data[128];
    uint8_t buf[32 * 16];

    memset(data, 42, sizeof(data));
    memset(buf, 0, sizeof(buf));

    /* Length != 256 should be rejected */
    decode_sub_tile_isometric(buf, 32, 16, 0, 0, data, 128);

    /* Buffer should be untouched */
    TEST_ASSERT_EQUAL_UINT8(0, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0, buf[32 * 16 - 1]);
}


void test_isometric_with_offset(void)
{
    uint8_t data[256];
    uint8_t buf[64 * 32];
    int i;

    for (i = 0; i < 256; i++)
        data[i] = 1;

    memset(buf, 0, sizeof(buf));
    decode_sub_tile_isometric(buf, 64, 32, 10, 5, data, 256);

    /* Pixel at (10+14, 5+0) should be 1 (first pixel of row 0) */
    TEST_ASSERT_EQUAL_UINT8(1, buf[5 * 64 + 24]);

    /* Pixel at (10+0, 5+7) should be 1 (first pixel of row 7) */
    TEST_ASSERT_EQUAL_UINT8(1, buf[12 * 64 + 10]);
}


/* ---- decode_sub_tile_normal ---- */

void test_normal_decode_basic(void)
{
    /*
     * Normal sub-tile format:
     *   b1 b2 pairs: if both 0 = newline, else skip b1 pixels then b2 literal pixels
     * Build a simple pattern: row 0 has 3 pixels at x=2, then newline
     */
    uint8_t data[] = {
        2, 3,        /* skip 2, then 3 pixels */
        10, 20, 30,  /* pixel values */
        0, 0,        /* newline */
        1, 2,        /* skip 1, then 2 pixels */
        40, 50,      /* pixel values */
        0, 0,        /* newline */
    };
    int length = sizeof(data);
    uint8_t buf[16 * 4];

    memset(buf, 0, sizeof(buf));
    decode_sub_tile_normal(buf, 16, 4, 0, 0, data, length);

    /* Row 0: pixels at x=2,3,4 */
    TEST_ASSERT_EQUAL_UINT8(0, buf[0 * 16 + 0]);
    TEST_ASSERT_EQUAL_UINT8(0, buf[0 * 16 + 1]);
    TEST_ASSERT_EQUAL_UINT8(10, buf[0 * 16 + 2]);
    TEST_ASSERT_EQUAL_UINT8(20, buf[0 * 16 + 3]);
    TEST_ASSERT_EQUAL_UINT8(30, buf[0 * 16 + 4]);
    TEST_ASSERT_EQUAL_UINT8(0, buf[0 * 16 + 5]);

    /* Row 1: pixels at x=1,2 */
    TEST_ASSERT_EQUAL_UINT8(0, buf[1 * 16 + 0]);
    TEST_ASSERT_EQUAL_UINT8(40, buf[1 * 16 + 1]);
    TEST_ASSERT_EQUAL_UINT8(50, buf[1 * 16 + 2]);
    TEST_ASSERT_EQUAL_UINT8(0, buf[1 * 16 + 3]);
}


void test_normal_decode_with_offset(void)
{
    uint8_t data[] = {
        0, 2,       /* 0 skip, 2 pixels */
        77, 88,
        0, 0,       /* newline */
    };
    uint8_t buf[16 * 4];

    memset(buf, 0, sizeof(buf));
    decode_sub_tile_normal(buf, 16, 4, 5, 2, data, sizeof(data));

    /* Row 2: pixels at x=5,6 */
    TEST_ASSERT_EQUAL_UINT8(77, buf[2 * 16 + 5]);
    TEST_ASSERT_EQUAL_UINT8(88, buf[2 * 16 + 6]);
}


void test_normal_decode_bounds_check(void)
{
    /* Decode data that tries to write outside buffer — should not crash */
    uint8_t data[] = {
        30, 5,          /* skip 30, then 5 pixels — goes past 16-wide buffer */
        1, 2, 3, 4, 5,
        0, 0,
    };
    uint8_t buf[16 * 2];

    memset(buf, 0, sizeof(buf));
    decode_sub_tile_normal(buf, 16, 2, 0, 0, data, sizeof(data));

    /* Should not have written anything at x >= 16 (bounds check) */
    /* But also should not have crashed */
    TEST_ASSERT_EQUAL_UINT8(0, buf[0]);
}


/* ---- index_buf_scale_down ---- */

void test_scale_down_2x(void)
{
    uint8_t src[4 * 4] = {
        1, 1, 2, 2,
        1, 1, 2, 2,
        3, 3, 4, 4,
        3, 3, 4, 4,
    };
    uint8_t dst[2 * 2];

    index_buf_scale_down(src, 4, 4, dst, 2, 2);

    /* Nearest-neighbor should pick (0,0), (2,0), (0,2), (2,2) */
    TEST_ASSERT_EQUAL_UINT8(1, dst[0]);
    TEST_ASSERT_EQUAL_UINT8(2, dst[1]);
    TEST_ASSERT_EQUAL_UINT8(3, dst[2]);
    TEST_ASSERT_EQUAL_UINT8(4, dst[3]);
}


void test_scale_down_1x(void)
{
    uint8_t src[3 * 2] = { 10, 20, 30, 40, 50, 60 };
    uint8_t dst[3 * 2];

    index_buf_scale_down(src, 3, 2, dst, 3, 2);

    /* 1:1 scale should be identity */
    TEST_ASSERT_EQUAL_UINT8_ARRAY(src, dst, 6);
}


/* ---- main ---- */

int main(void)
{
    UNITY_BEGIN();

    /* Isometric decode */
    RUN_TEST(test_isometric_decode_produces_diamond);
    RUN_TEST(test_isometric_wrong_length_does_nothing);
    RUN_TEST(test_isometric_with_offset);

    /* Normal decode */
    RUN_TEST(test_normal_decode_basic);
    RUN_TEST(test_normal_decode_with_offset);
    RUN_TEST(test_normal_decode_bounds_check);

    /* Scale down */
    RUN_TEST(test_scale_down_2x);
    RUN_TEST(test_scale_down_1x);

    return UNITY_END();
}
