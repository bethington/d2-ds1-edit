/*
 * Unit tests for the palette abstraction layer (palette.c).
 * Tests conversion from D2 raw palette format to RGBA,
 * gamma correction, and colormap generation.
 */

#include <string.h>
#include "unity/unity.h"
#include "core/palette.h"

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
    d2_pal[0] = 0;
    d2_pal[1] = 0;
    d2_pal[2] = 0;
    d2_pal[3] = 0;

    /* PL2 display RGB: index 1 is red (255,0,0). */
    d2_pal[4] = 255;
    d2_pal[5] = 0;
    d2_pal[6] = 0;
    d2_pal[7] = 0;

    /* Set index 2 to green (0,255,0) */
    d2_pal[8] = 0;
    d2_pal[9] = 255;
    d2_pal[10] = 0;
    d2_pal[11] = 0;

    /* Index 3 is blue (0,0,255). */
    d2_pal[12] = 0;
    d2_pal[13] = 0;
    d2_pal[14] = 255;
    d2_pal[15] = 0;

    /* Set index 255 to white */
    d2_pal[255 * 4] = 255;
    d2_pal[255 * 4 + 1] = 255;
    d2_pal[255 * 4 + 2] = 255;
    d2_pal[255 * 4 + 3] = 0;

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

void test_act3_bog_ground_uses_pl2_display_order(void)
{
    uint8_t pl2_base[1024] = {0};
    RGBA_PALETTE pal;

    /* Act 3 Pal.PL2 entry 230 is 20,16,4. BogE.ds1 uses this index for
     * ground: it must render brown, not the blue 4,16,20. */
    pl2_base[230 * 4 + 0] = 20;
    pl2_base[230 * 4 + 1] = 16;
    pl2_base[230 * 4 + 2] = 4;
    palette_d2_to_rgba(pl2_base, sizeof(pl2_base), NULL, &pal);

    TEST_ASSERT_EQUAL_UINT8(20, pal.colors[230].r);
    TEST_ASSERT_EQUAL_UINT8(16, pal.colors[230].g);
    TEST_ASSERT_EQUAL_UINT8(4, pal.colors[230].b);
}

void test_act1_header_red_uses_pl2_display_order(void)
{
    uint8_t pl2_base[1024] = {0};
    RGBA_PALETTE pal;

    /* All five shipped act PL2s use 252,44,0 at index 98. The editor's
     * top header uses that index for red labels; a channel swap makes it
     * blue (0,44,252). */
    pl2_base[98 * 4 + 0] = 252;
    pl2_base[98 * 4 + 1] = 44;
    pl2_base[98 * 4 + 2] = 0;
    palette_d2_to_rgba(pl2_base, sizeof(pl2_base), NULL, &pal);

    TEST_ASSERT_EQUAL_UINT8(252, pal.colors[98].r);
    TEST_ASSERT_EQUAL_UINT8(44, pal.colors[98].g);
    TEST_ASSERT_EQUAL_UINT8(0, pal.colors[98].b);
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

    /* PL2 display RGB for (200, 100, 50). */
    d2_pal[4] = 200;
    d2_pal[5] = 100;
    d2_pal[6] = 50;
    d2_pal[7] = 0;

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

    /* PL2 display RGB for (42, 84, 168). */
    d2_pal[40] = 42;
    d2_pal[41] = 84;
    d2_pal[42] = 168;
    d2_pal[43] = 0;

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
    pal.colors[1].r = 255;
    pal.colors[1].a = 255;
    /* Index 2: pure green */
    pal.colors[2].g = 255;
    pal.colors[2].a = 255;
    /* Index 3: pure blue */
    pal.colors[3].b = 255;
    pal.colors[3].a = 255;

    TEST_ASSERT_EQUAL_UINT8(1, palette_find_closest(&pal, 255, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(2, palette_find_closest(&pal, 0, 255, 0));
    TEST_ASSERT_EQUAL_UINT8(3, palette_find_closest(&pal, 0, 0, 255));
}

void test_find_closest_approximate(void)
{
    RGBA_PALETTE pal;
    memset(&pal, 0, sizeof(pal));

    /* Index 1: (100,0,0) */
    pal.colors[1].r = 100;
    pal.colors[1].a = 255;
    /* Index 2: (200,0,0) */
    pal.colors[2].r = 200;
    pal.colors[2].a = 255;

    /* (190,0,0) should match index 2 (closer to 200) */
    TEST_ASSERT_EQUAL_UINT8(2, palette_find_closest(&pal, 190, 0, 0));

    /* (110,0,0) should match index 1 (closer to 100) */
    TEST_ASSERT_EQUAL_UINT8(1, palette_find_closest(&pal, 110, 0, 0));
}

void test_resolve_act_prefers_lvltypes_over_ds1(void)
{
    TEST_ASSERT_EQUAL_INT(2, palette_resolve_act(2, 1));
    TEST_ASSERT_EQUAL_INT(4, palette_resolve_act(4, 2));
}

void test_resolve_act_falls_back_to_ds1_when_lvltypes_missing(void)
{
    TEST_ASSERT_EQUAL_INT(3, palette_resolve_act(0, 3));
    TEST_ASSERT_EQUAL_INT(5, palette_resolve_act(-1, 5));
}

void test_resolve_index_clamps_invalid_values_to_act1(void)
{
    TEST_ASSERT_EQUAL_INT(0, palette_resolve_index(0, 0));
    TEST_ASSERT_EQUAL_INT(0, palette_resolve_index(9, -4));
    TEST_ASSERT_EQUAL_INT(4, palette_resolve_index(5, 1));
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
    pal.colors[1].r = 255;
    pal.colors[1].a = 255;
    pal.colors[2].g = 255;
    pal.colors[2].a = 255;
    pal.colors[3].b = 255;
    pal.colors[3].a = 255;
    for (i = 4; i < 256; i++)
        pal.colors[i].a = 255;

    palette_build_trans_colormap(&pal, &cmap);

    /* 50% blend is symmetric */
    TEST_ASSERT_EQUAL_UINT8(cmap.data[1][2], cmap.data[2][1]);
    TEST_ASSERT_EQUAL_UINT8(cmap.data[1][3], cmap.data[3][1]);
    TEST_ASSERT_EQUAL_UINT8(cmap.data[2][3], cmap.data[3][2]);
}

/* ---- package D: palette vectors ------------------------------------------
 * Assumptions not yet confirmed against the original game are named in the
 * test and listed in test/equivalence/contracts/visual-values-113c.json:
 *   PL2_BASE_ORDER_RGBX  - the first 1024 bytes of a .pl2 are R,G,B,pad
 *                          (the guide's prose says B,G,R,pad; disputed)
 *   NEAREST_TIE_LOWEST   - equal-distance candidates resolve to the lowest
 *                          index (the editor's choice; game policy unknown)
 */

void test_assumption_PL2_BASE_ORDER_RGBX_distinguishes_red_and_blue(void)
{
   uint8_t pl2[1024] = {0};
   RGBA_PALETTE pal;

   pl2[7 * 4 + 0] = 201;   /* first byte  */
   pl2[7 * 4 + 1] = 17;
   pl2[7 * 4 + 2] = 3;     /* third byte  */
   pl2[7 * 4 + 3] = 99;    /* pad: never a channel */
   palette_d2_to_rgba(pl2, sizeof(pl2), NULL, &pal);
   TEST_ASSERT_EQUAL_UINT8(201, pal.colors[7].r);
   TEST_ASSERT_EQUAL_UINT8(17, pal.colors[7].g);
   TEST_ASSERT_EQUAL_UINT8(3, pal.colors[7].b);
   TEST_ASSERT_EQUAL_UINT8(255, pal.colors[7].a);
}

void test_index_0_is_transparent_whatever_its_bytes_and_gamma(void)
{
   uint8_t pl2[1024];
   uint8_t gamma[256];
   RGBA_PALETTE pal;
   int i;

   memset(pl2, 0xC8, sizeof(pl2));
   for (i = 0; i < 256; i++)
      gamma[i] = (uint8_t) (255 - i);
   palette_d2_to_rgba(pl2, sizeof(pl2), gamma, &pal);
   TEST_ASSERT_EQUAL_UINT8(0, pal.colors[0].a);
   for (i = 1; i < 256; i++)
   {
      TEST_ASSERT_EQUAL_UINT8(255, pal.colors[i].a);
      TEST_ASSERT_EQUAL_UINT8(255 - 0xC8, pal.colors[i].r);
   }
}

void test_gamma_maps_each_channel_independently(void)
{
   uint8_t pl2[1024] = {0};
   uint8_t gamma[256];
   RGBA_PALETTE pal;
   int i;

   for (i = 0; i < 256; i++)
      gamma[i] = (uint8_t) ((i * 3 + 1) & 0xFF);
   pl2[4] = 10; pl2[5] = 20; pl2[6] = 30;
   palette_d2_to_rgba(pl2, sizeof(pl2), gamma, &pal);
   TEST_ASSERT_EQUAL_UINT8(31, pal.colors[1].r);
   TEST_ASSERT_EQUAL_UINT8(61, pal.colors[1].g);
   TEST_ASSERT_EQUAL_UINT8(91, pal.colors[1].b);
}

void test_short_base_table_falls_back_to_the_grey_ramp(void)
{
   uint8_t pl2[1023];
   RGBA_PALETTE pal;

   memset(pl2, 0x40, sizeof(pl2));
   palette_d2_to_rgba(pl2, sizeof(pl2), NULL, &pal);
   TEST_ASSERT_EQUAL_UINT8(77, pal.colors[77].r);
   TEST_ASSERT_EQUAL_UINT8(77, pal.colors[77].b);
   TEST_ASSERT_EQUAL_UINT8(0, pal.colors[0].a);
}

void test_all_five_act_selection_paths(void)
{
   int act;
   for (act = 1; act <= 5; act++)
   {
      /* LvlTypes act wins over the DS1's */
      TEST_ASSERT_EQUAL_INT(act, palette_resolve_act(act, (act % 5) + 1));
      /* no LvlTypes act: the DS1's */
      TEST_ASSERT_EQUAL_INT(act, palette_resolve_act(0, act));
      TEST_ASSERT_EQUAL_INT(act - 1, palette_resolve_index(0, act));
   }
   TEST_ASSERT_EQUAL_INT(1, palette_resolve_act(6, 6));
   TEST_ASSERT_EQUAL_INT(1, palette_resolve_act(-1, 0));
}

void test_darkness_rows_follow_the_pl2_layout(void)
{
   /* palettes-and-colour.md: 32 darkness levels at 0x400 + (L-1)*256. The
    * editor's shade c uses level row c / 8. Each row is authored distinct. */
   static uint8_t pl2[1024 + 32 * 256];
   INDEX_COLORMAP cmap;
   int c, i;

   for (c = 0; c < 32; c++)
      for (i = 0; i < 256; i++)
         pl2[1024 + c * 256 + i] = (uint8_t) (c * 7 + i);
   palette_build_shadow_colormap(pl2, sizeof(pl2), &cmap);
   for (c = 0; c < 256; c++)
      for (i = 0; i < 256; i += 51)
         TEST_ASSERT_EQUAL_UINT8((uint8_t) ((c / 8) * 7 + i), cmap.data[c][i]);
}

void test_short_darkness_table_reads_nothing_past_its_end(void)
{
   /* only the first 1.5 levels are present */
   static uint8_t pl2[1024 + 384];
   INDEX_COLORMAP cmap;

   memset(pl2 + 1024, 0x33, 384);
   palette_build_shadow_colormap(pl2, sizeof(pl2), &cmap);
   TEST_ASSERT_EQUAL_UINT8(0x33, cmap.data[0][255]);
   TEST_ASSERT_EQUAL_UINT8(0x33, cmap.data[8][127]);
   TEST_ASSERT_EQUAL_UINT8(0, cmap.data[8][128]);
   TEST_ASSERT_EQUAL_UINT8(0, cmap.data[255][0]);
}

void test_assumption_NEAREST_TIE_LOWEST_with_duplicate_colours(void)
{
   RGBA_PALETTE pal;
   memset(&pal, 0, sizeof(pal));
   pal.colors[9].r = pal.colors[5].r = 120;
   pal.colors[9].g = pal.colors[5].g = 60;
   pal.colors[9].b = pal.colors[5].b = 200;
   pal.colors[3].r = 255; pal.colors[3].g = 255; pal.colors[3].b = 255;
   /* exact duplicates */
   TEST_ASSERT_EQUAL_UINT8(5, palette_find_closest(&pal, 120, 60, 200));
   /* equal distance to two different colours: 0 and 2 away from 10/12 */
   memset(&pal, 0xFF, sizeof(pal));
   pal.colors[40].r = 10; pal.colors[40].g = 0; pal.colors[40].b = 0;
   pal.colors[20].r = 12; pal.colors[20].g = 0; pal.colors[20].b = 0;
   TEST_ASSERT_EQUAL_UINT8(20, palette_find_closest(&pal, 11, 0, 0));
}

void test_nearest_never_answers_the_transparent_index(void)
{
   RGBA_PALETTE pal;
   memset(&pal, 0xFF, sizeof(pal));
   pal.colors[0].r = pal.colors[0].g = pal.colors[0].b = 0;
   pal.colors[200].r = 1; pal.colors[200].g = 1; pal.colors[200].b = 1;
   TEST_ASSERT_EQUAL_UINT8(200, palette_find_closest(&pal, 0, 0, 0));
}

/* ---- main ---- */

int main(void)
{
    UNITY_BEGIN();

    /* Palette conversion */
    RUN_TEST(test_null_palette_produces_grayscale);
    RUN_TEST(test_d2_palette_basic_conversion);
    RUN_TEST(test_act3_bog_ground_uses_pl2_display_order);
    RUN_TEST(test_act1_header_red_uses_pl2_display_order);
    RUN_TEST(test_gamma_correction_applied);
    RUN_TEST(test_identity_gamma_no_change);

    /* Find closest */
    RUN_TEST(test_find_closest_exact_match);
    RUN_TEST(test_find_closest_approximate);
    RUN_TEST(test_resolve_act_prefers_lvltypes_over_ds1);
    RUN_TEST(test_resolve_act_falls_back_to_ds1_when_lvltypes_missing);
    RUN_TEST(test_resolve_index_clamps_invalid_values_to_act1);

    /* Colormaps */
    RUN_TEST(test_shadow_colormap_basic);
    RUN_TEST(test_shadow_colormap_null_palette);
    RUN_TEST(test_select_colormap_shadow_entry);
    RUN_TEST(test_trans_colormap_symmetry);

    /* Package D vectors */
    RUN_TEST(test_assumption_PL2_BASE_ORDER_RGBX_distinguishes_red_and_blue);
    RUN_TEST(test_index_0_is_transparent_whatever_its_bytes_and_gamma);
    RUN_TEST(test_gamma_maps_each_channel_independently);
    RUN_TEST(test_short_base_table_falls_back_to_the_grey_ramp);
    RUN_TEST(test_all_five_act_selection_paths);
    RUN_TEST(test_darkness_rows_follow_the_pl2_layout);
    RUN_TEST(test_short_darkness_table_reads_nothing_past_its_end);
    RUN_TEST(test_assumption_NEAREST_TIE_LOWEST_with_duplicate_colours);
    RUN_TEST(test_nearest_never_answers_the_transparent_index);

    return UNITY_END();
}
