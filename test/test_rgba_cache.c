/*
 * Unit tests for the RGBA bitmap cache (rgba_cache.c).
 * Tests tile creation, destruction, and palette-to-RGBA rebuild.
 */

#include <string.h>
#include "unity/unity.h"
#include "core/rgba_cache.h"

void setUp(void) {}
void tearDown(void) {}


/* ---- creation / destruction ---- */

void test_create_tile(void)
{
    CACHED_TILE *tile = cache_tile_create(32, 16);

    TEST_ASSERT_NOT_NULL(tile);
    TEST_ASSERT_EQUAL_INT(32, tile->width);
    TEST_ASSERT_EQUAL_INT(16, tile->height);
    TEST_ASSERT_NOT_NULL(tile->indices);
    TEST_ASSERT_NOT_NULL(tile->rgba);

    /* Buffers should be zeroed */
    TEST_ASSERT_EQUAL_UINT8(0, tile->indices[0]);
    TEST_ASSERT_EQUAL_UINT32(0, tile->rgba[0]);
    TEST_ASSERT_EQUAL_UINT8(0, tile->indices[32 * 16 - 1]);
    TEST_ASSERT_EQUAL_UINT32(0, tile->rgba[32 * 16 - 1]);

    cache_tile_destroy(tile);
}


void test_create_invalid_dimensions(void)
{
    TEST_ASSERT_NULL(cache_tile_create(0, 10));
    TEST_ASSERT_NULL(cache_tile_create(10, 0));
    TEST_ASSERT_NULL(cache_tile_create(-1, 10));
    TEST_ASSERT_NULL(cache_tile_create(10, -1));
}


void test_destroy_null_safe(void)
{
    /* Should not crash */
    cache_tile_destroy(NULL);
}


/* ---- rebuild ---- */

void test_rebuild_index_zero_transparent(void)
{
    RGBA_PALETTE pal;
    CACHED_TILE *tile;

    memset(&pal, 0, sizeof(pal));
    pal.colors[0].r = 0;
    pal.colors[0].g = 0;
    pal.colors[0].b = 0;
    pal.colors[0].a = 0; /* transparent */

    tile = cache_tile_create(4, 4);
    TEST_ASSERT_NOT_NULL(tile);

    /* All indices are 0 (default from calloc) */
    cache_tile_rebuild(tile, &pal);

    /* All RGBA should be 0 (transparent) */
    TEST_ASSERT_EQUAL_UINT32(0, tile->rgba[0]);
    TEST_ASSERT_EQUAL_UINT32(0, tile->rgba[15]);

    cache_tile_destroy(tile);
}


void test_rebuild_single_color(void)
{
    RGBA_PALETTE pal;
    CACHED_TILE *tile;
    RGBA_COLOR expected;
    uint32_t packed;

    memset(&pal, 0, sizeof(pal));
    pal.colors[42].r = 100;
    pal.colors[42].g = 150;
    pal.colors[42].b = 200;
    pal.colors[42].a = 255;

    tile = cache_tile_create(2, 2);
    TEST_ASSERT_NOT_NULL(tile);

    /* Set all pixels to index 42 */
    tile->indices[0] = 42;
    tile->indices[1] = 42;
    tile->indices[2] = 42;
    tile->indices[3] = 42;

    cache_tile_rebuild(tile, &pal);

    expected.r = 100;
    expected.g = 150;
    expected.b = 200;
    expected.a = 255;
    packed = rgba_pack(expected);

    TEST_ASSERT_EQUAL_UINT32(packed, tile->rgba[0]);
    TEST_ASSERT_EQUAL_UINT32(packed, tile->rgba[1]);
    TEST_ASSERT_EQUAL_UINT32(packed, tile->rgba[2]);
    TEST_ASSERT_EQUAL_UINT32(packed, tile->rgba[3]);

    cache_tile_destroy(tile);
}


void test_rebuild_mixed_indices(void)
{
    RGBA_PALETTE pal;
    CACHED_TILE *tile;

    memset(&pal, 0, sizeof(pal));
    pal.colors[0].a = 0;   /* transparent */
    pal.colors[1].r = 255; pal.colors[1].a = 255;  /* red */
    pal.colors[2].g = 255; pal.colors[2].a = 255;  /* green */

    tile = cache_tile_create(2, 1);
    TEST_ASSERT_NOT_NULL(tile);

    tile->indices[0] = 1; /* red */
    tile->indices[1] = 2; /* green */

    cache_tile_rebuild(tile, &pal);

    /* Verify pixel 0 is red */
    RGBA_COLOR c0 = rgba_unpack(tile->rgba[0]);
    TEST_ASSERT_EQUAL_UINT8(255, c0.r);
    TEST_ASSERT_EQUAL_UINT8(0, c0.g);
    TEST_ASSERT_EQUAL_UINT8(0, c0.b);
    TEST_ASSERT_EQUAL_UINT8(255, c0.a);

    /* Verify pixel 1 is green */
    RGBA_COLOR c1 = rgba_unpack(tile->rgba[1]);
    TEST_ASSERT_EQUAL_UINT8(0, c1.r);
    TEST_ASSERT_EQUAL_UINT8(255, c1.g);
    TEST_ASSERT_EQUAL_UINT8(0, c1.b);
    TEST_ASSERT_EQUAL_UINT8(255, c1.a);

    cache_tile_destroy(tile);
}


void test_rebuild_preserves_indices(void)
{
    RGBA_PALETTE pal;
    CACHED_TILE *tile;

    memset(&pal, 0, sizeof(pal));
    pal.colors[7].r = 42; pal.colors[7].a = 255;

    tile = cache_tile_create(3, 1);
    TEST_ASSERT_NOT_NULL(tile);

    tile->indices[0] = 7;
    tile->indices[1] = 0;
    tile->indices[2] = 7;

    cache_tile_rebuild(tile, &pal);

    /* Indices should be untouched */
    TEST_ASSERT_EQUAL_UINT8(7, tile->indices[0]);
    TEST_ASSERT_EQUAL_UINT8(0, tile->indices[1]);
    TEST_ASSERT_EQUAL_UINT8(7, tile->indices[2]);

    cache_tile_destroy(tile);
}


/* ---- pack/unpack roundtrip ---- */

void test_rgba_pack_unpack_roundtrip(void)
{
    RGBA_COLOR in, out;

    in.r = 0x12;
    in.g = 0x34;
    in.b = 0x56;
    in.a = 0x78;

    out = rgba_unpack(rgba_pack(in));

    TEST_ASSERT_EQUAL_UINT8(in.r, out.r);
    TEST_ASSERT_EQUAL_UINT8(in.g, out.g);
    TEST_ASSERT_EQUAL_UINT8(in.b, out.b);
    TEST_ASSERT_EQUAL_UINT8(in.a, out.a);
}


void test_rgba_pack_byte_order(void)
{
    RGBA_COLOR c;
    uint32_t packed;

    c.r = 0xAA;
    c.g = 0xBB;
    c.b = 0xCC;
    c.a = 0xDD;

    packed = rgba_pack(c);

    /* Expected ABGR format: 0xDDCCBBAA */
    TEST_ASSERT_EQUAL_HEX32(0xDDCCBBAA, packed);
}


/* ---- main ---- */

int main(void)
{
    UNITY_BEGIN();

    /* Creation / destruction */
    RUN_TEST(test_create_tile);
    RUN_TEST(test_create_invalid_dimensions);
    RUN_TEST(test_destroy_null_safe);

    /* Rebuild */
    RUN_TEST(test_rebuild_index_zero_transparent);
    RUN_TEST(test_rebuild_single_color);
    RUN_TEST(test_rebuild_mixed_indices);
    RUN_TEST(test_rebuild_preserves_indices);

    /* Pack/unpack */
    RUN_TEST(test_rgba_pack_unpack_roundtrip);
    RUN_TEST(test_rgba_pack_byte_order);

    return UNITY_END();
}
