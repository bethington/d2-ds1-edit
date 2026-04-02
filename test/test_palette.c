/*
 * Unit tests for the palette abstraction layer (palette.c).
 * Tests conversion from D2 raw palette format to RGBA,
 * gamma correction, and colormap generation.
 */

#include <string.h>
#include "unity/unity.h"
#include "../Sources/palette.h"

void setUp(void) {}
void tearDown(void) {}


/* ---- palette_d2_to_rgba tests ---- */

void test_null_palette_produces_grayscale(void)
{
    RGBA_PALETTE pal;

    palette_d2_to_rgba(NULL, 0, NULL, &pal);

    /* Index 0: transparent black */
    TEST_ASSERT_EQUAL_UINT8(0, pal.colors[0].r);
    TEST_ASSERT_EQUAL_UINT8(0, pal.colors[0].g);
    TEST_ASSERT_EQUAL_UINT8(0, pal.colors[0].b);
    TEST_ASSERT_EQUAL_UINT8(0, pal.colors[0].a);

    /* Index 1: (1,1,1) opaque */
    TEST_ASSERT_EQUAL_UINT8(1, pal.colors[1].r);
    TEST_ASSERT_EQUAL_UINT8(255, pal.colors[1].a);

    /* Index 128: (128,128,128) opaque */
    TEST_ASSERT_EQUAL_UINT8(128, pal.colors[128].r);
    TEST_ASSERT_EQUAL_UINT8(128, pal.colors[128].g);
    TEST_ASSERT_EQUAL_UINT8(128, pal.colors[128].b);
    TEST_ASSERT_EQUAL_UINT8(255, pal.colors[128].a);

    /* Index 255: (255,255,255) opaque */
    TEST_ASSERT_EQUAL_UINT8(255, pal.colors[255].r);
    TEST_ASSERT_EQUAL_UINT8(255, pal.colors[255].a);
}


void test_d2_palette_basic_conversion(void)
{
    /* Build a small D2 palette: 256 entries * 4 bytes = 1024 bytes */
    uint8_t d2_pal[1024];
    RGBA_PALETTE pal;
    int i;

    memset(d2_pal, 0, sizeof(d2_pal));

    /* Set index 0 to black */
    d2_pal[0] = 0; d2_pal[1] = 0; d2_pal[2] = 0; d2_pal[3] = 0;

    /* Set index 1 to red (255,0,0) */
    d2_pal[4] = 255; d2_pal[5] = 0; d2_pal[6] = 0; d2_pal[7] = 0;

    /* Set index 2 to green (0,255,0) */
    d2_pal[8] = 0; d2_pal[9] = 255; d2_pal[10] = 0; d2_pal[11] = 0;

    /* Set index 3 to blue (0,0,255) */
    d2_pal[12] = 0; d2_pal[13] = 0; d2_pal[14] = 255; d2_pal[15] = 0;

    /* Set index 255 to white */
    d2_pal[255 * 4] = 255; d2_pal[255 * 4 + 1] = 255;
    d2_pal[255 * 4 + 2] = 255; d2_pal[255 * 4 + 3] = 0;

    palette_d2_to_rgba(d2_pal, 1024, NULL, &pal);

    /* Index 0: transparent */
    TEST_ASSERT_EQUAL_UINT8(0, pal.colors[0].a);

    /* Index 1: red, opaque */
    TEST_ASSERT_EQUAL_UINT8(255, pal.colors[1].r);
    TEST_ASSERT_EQUAL_UINT8(0, pal.colors[1].g);
    TEST_ASSERT_EQUAL_UINT8(0, pal.colors[1].b);
    TEST_ASSERT_EQUAL_UINT8(255, pal.colors[1].a);

    /* Index 2: green */
    TEST_ASSERT_EQUAL_UINT8(0, pal.colors[2].r);
    TEST_ASSERT_EQUAL_UINT8(255, pal.colors[2].g);

    /* Index 3: blue */
    TEST_ASSERT_EQUAL_UINT8(255, pal.colors[3].b);

    /* Index 255: white, opaque */
    TEST_ASSERT_EQUAL_UINT8(255, pal.colors[255].r);
    TEST_ASSERT_EQUAL_UINT8(255, pal.colors[255].g);
    TEST_ASSERT_EQUAL_UINT8(255, pal.colors[255].b);
    TEST_ASSERT_EQUAL_UINT8(255, pal.colors[255].a);

    /* Unset indices should be (0,0,0) but opaque (not index 0) */
    for (i = 4; i < 255; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0, pal.colors[i].r);
        TEST_ASSERT_EQUAL_UINT8(0, pal.colors[i].g);
        TEST_ASSERT_EQUAL_UINT8(0, pal.colors[i].b);
        TEST_ASSERT_EQUAL_UINT8(255, pal.colors[i].a);
    }
}


void test_gamma_correction_applied(void)
{
    uint8_t d2_pal[1024];
    uint8_t gamma_table[256];
    RGBA_PALETTE pal;
    int i;

    memset(d2_pal, 0, sizeof(d2_pal));

    /* Gamma table that halves all values */
    for (i = 0; i < 256; i++)
        gamma_table[i] = (uint8_t)(i / 2);

    /* Set index 1 to (200, 100, 50) */
    d2_pal[4] = 200; d2_pal[5] = 100; d2_pal[6] = 50; d2_pal[7] = 0;

    palette_d2_to_rgba(d2_pal, 1024, gamma_table, &pal);

    /* After gamma: (100, 50, 25) */
    TEST_ASSERT_EQUAL_UINT8(100, pal.colors[1].r);
    TEST_ASSERT_EQUAL_UINT8(50, pal.colors[1].g);
    TEST_ASSERT_EQUAL_UINT8(25, pal.colors[1].b);
}


void test_identity_gamma_no_change(void)
{
    uint8_t d2_pal[1024];
    uint8_t gamma_table[256];
    RGBA_PALETTE pal;
    int i;

    memset(d2_pal, 0, sizeof(d2_pal));

    /* Identity gamma: every value maps to itself */
    for (i = 0; i < 256; i++)
        gamma_table[i] = (uint8_t)i;

    /* Set index 10 to (42, 84, 168) */
    d2_pal[40] = 42; d2_pal[41] = 84; d2_pal[42] = 168; d2_pal[43] = 0;

    palette_d2_to_rgba(d2_pal, 1024, gamma_table, &pal);

    TEST_ASSERT_EQUAL_UINT8(42, pal.colors[10].r);
    TEST_ASSERT_EQUAL_UINT8(84, pal.colors[10].g);
    TEST_ASSERT_EQUAL_UINT8(168, pal.colors[10].b);
}


/* ---- palette_find_closest tests ---- */

void test_find_closest_exact_match(void)
{
    RGBA_PALETTE pal;
    memset(&pal, 0, sizeof(pal));

    /* Index 1: pure red */
    pal.colors[1].r = 255; pal.colors[1].a = 255;
    /* Index 2: pure green */
    pal.colors[2].g = 255; pal.colors[2].a = 255;
    /* Index 3: pure blue */
    pal.colors[3].b = 255; pal.colors[3].a = 255;

    TEST_ASSERT_EQUAL_UINT8(1, palette_find_closest(&pal, 255, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(2, palette_find_closest(&pal, 0, 255, 0));
    TEST_ASSERT_EQUAL_UINT8(3, palette_find_closest(&pal, 0, 0, 255));
}


void test_find_closest_approximate(void)
{
    RGBA_PALETTE pal;
    memset(&pal, 0, sizeof(pal));

    /* Index 1: (100,0,0) */
    pal.colors[1].r = 100; pal.colors[1].a = 255;
    /* Index 2: (200,0,0) */
    pal.colors[2].r = 200; pal.colors[2].a = 255;

    /* (190,0,0) should match index 2 (closer to 200) */
    TEST_ASSERT_EQUAL_UINT8(2, palette_find_closest(&pal, 190, 0, 0));

    /* (110,0,0) should match index 1 (closer to 100) */
    TEST_ASSERT_EQUAL_UINT8(1, palette_find_closest(&pal, 110, 0, 0));
}


/* ---- colormap tests ---- */

void test_shadow_colormap_basic(void)
{
    /* Create a minimal D2 palette with shadow data */
    uint8_t d2_pal[1024 + 256 * 32]; /* enough for 32 groups of 256 */
    INDEX_COLORMAP cmap;
    int i;

    memset(d2_pal, 0, sizeof(d2_pal));

    /* Fill shadow data: for c=0, start=1024+0, fill with sequential values */
    for (i = 0; i < 256; i++)
        d2_pal[1024 + i] = (uint8_t)(255 - i);

    palette_build_shadow_colormap(d2_pal, sizeof(d2_pal), &cmap);

    /* c=0: data[0][i] should come from offset 1024 + 256*(0/8) = 1024 */
    for (i = 0; i < 256; i++)
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(255 - i), cmap.data[0][i]);

    /* c=1 through c=7: same group (1024 + 256*(c/8) = 1024) */
    TEST_ASSERT_EQUAL_UINT8(cmap.data[0][42], cmap.data[5][42]);
}


void test_shadow_colormap_null_palette(void)
{
    INDEX_COLORMAP cmap;
    memset(&cmap, 0xFF, sizeof(cmap)); /* fill with junk */

    palette_build_shadow_colormap(NULL, 0, &cmap);

    /* Should be zeroed */
    TEST_ASSERT_EQUAL_UINT8(0, cmap.data[0][0]);
    TEST_ASSERT_EQUAL_UINT8(0, cmap.data[128][128]);
    TEST_ASSERT_EQUAL_UINT8(0, cmap.data[255][255]);
}


void test_select_colormap_shadow_entry(void)
{
    /* COL_SHADOW (168) as source should produce white output */
    RGBA_PALETTE pal;
    INDEX_COLORMAP cmap;
    int i;

    memset(&pal, 0, sizeof(pal));

    /* Set up a palette with a white entry at index 255 */
    pal.colors[255].r = 255;
    pal.colors[255].g = 255;
    pal.colors[255].b = 255;
    pal.colors[255].a = 255;

    /* Set alpha for all non-zero entries */
    for (i = 1; i < 256; i++)
        pal.colors[i].a = 255;

    palette_build_select_colormap(&pal, &cmap);

    /* Source=168 (COL_SHADOW), any dst should map to closest-to-white = 255 */
    TEST_ASSERT_EQUAL_UINT8(255, cmap.data[168][0]);
    TEST_ASSERT_EQUAL_UINT8(255, cmap.data[168][128]);
}


void test_trans_colormap_symmetry(void)
{
    /*
     * TRANS colormap should be symmetric: blend(a,b) == blend(b,a)
     * because it's a 50% mix of the two palette colors.
     */
    RGBA_PALETTE pal;
    INDEX_COLORMAP cmap;
    int i;

    memset(&pal, 0, sizeof(pal));

    /* Set up some distinct colors */
    pal.colors[1].r = 255; pal.colors[1].a = 255;
    pal.colors[2].g = 255; pal.colors[2].a = 255;
    pal.colors[3].b = 255; pal.colors[3].a = 255;
    for (i = 4; i < 256; i++)
        pal.colors[i].a = 255;

    palette_build_trans_colormap(&pal, &cmap);

    /* 50% blend is symmetric */
    TEST_ASSERT_EQUAL_UINT8(cmap.data[1][2], cmap.data[2][1]);
    TEST_ASSERT_EQUAL_UINT8(cmap.data[1][3], cmap.data[3][1]);
    TEST_ASSERT_EQUAL_UINT8(cmap.data[2][3], cmap.data[3][2]);
}


/* ---- main ---- */

int main(void)
{
    UNITY_BEGIN();

    /* Palette conversion */
    RUN_TEST(test_null_palette_produces_grayscale);
    RUN_TEST(test_d2_palette_basic_conversion);
    RUN_TEST(test_gamma_correction_applied);
    RUN_TEST(test_identity_gamma_no_change);

    /* Find closest */
    RUN_TEST(test_find_closest_exact_match);
    RUN_TEST(test_find_closest_approximate);

    /* Colormaps */
    RUN_TEST(test_shadow_colormap_basic);
    RUN_TEST(test_shadow_colormap_null_palette);
    RUN_TEST(test_select_colormap_shadow_entry);
    RUN_TEST(test_trans_colormap_symmetry);

    return UNITY_END();
}
