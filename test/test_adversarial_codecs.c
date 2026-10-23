/*
 * Package E regressions for the sprite and tile decoders: each test is the
 * minimized form of a case the adversarial campaign (adversarial-core)
 * found crashing, named after its case id. They run the production code on
 * the authored bytes and require a controlled outcome (rejection, or a block
 * that draws nothing) instead of a fault.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "core/dt1.h"
#include "core/dcc.h"
#include "core/index_frame.h"
#include "core/rgba_cache.h"
#include "visual_test_support.h"

void setUp(void) { TEST_ASSERT_TRUE(visual_allegro_init()); }
void tearDown(void) {}

/* ---- DCC ------------------------------------------------------------------ */

/* dcc.dcc_sequence.u8@29=0x00 and dcc.dcc_sequence.u8@111=0x00: the 20-bit
 * sub-bitstream sizes of a direction exceeded the direction's data, so the
 * pixel-mask/code streams started past the buffer (and the remainder size
 * underflowed) -> read access violation. */
static const unsigned char s_dcc_sub_bitstreams_overrun_a[172] = {
   0x74, 0x06, 0x02, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0xc2, 0x8c, 0x00, 0x00, 0x23, 0xb5, 0x8f, 0x18, 0xa9,
   0x7d, 0xc4, 0x48, 0xed, 0x23, 0x06, 0x00, 0x40, 0x00, 0x80, 0x00, 0x02,
   0x08, 0x20, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x84, 0xbe, 0xff, 0x0f, 0x10, 0x18, 0x20,
   0x80, 0x02, 0x21, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0xa8, 0x8a, 0xaa, 0xaa, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0xc2, 0xc8, 0x29, 0x00, 0x8d, 0x3e, 0x72, 0x11, 0xc1, 0x36, 0x1f,
   0xb9, 0x88, 0x40, 0xa3, 0x8f, 0x5c, 0x44, 0x20, 0x00, 0x00, 0x02, 0x00,
   0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x08, 0x08,
   0x40, 0x88, 0xd9, 0x01,
};

static const unsigned char s_dcc_sub_bitstreams_overrun_b[172] = {
   0x74, 0x06, 0x02, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0xc2, 0x8c, 0x15, 0x00, 0x23, 0xb5, 0x8f, 0x18, 0xa9,
   0x7d, 0xc4, 0x48, 0xed, 0x23, 0x06, 0x00, 0x40, 0x00, 0x80, 0x00, 0x02,
   0x08, 0x20, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x84, 0xbe, 0xff, 0x0f, 0x10, 0x18, 0x20,
   0x80, 0x02, 0x21, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0xa8, 0x8a, 0xaa, 0xaa, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0xc2, 0xc8, 0x00, 0x00, 0x8d, 0x3e, 0x72, 0x11, 0xc1, 0x36, 0x1f,
   0xb9, 0x88, 0x40, 0xa3, 0x8f, 0x5c, 0x44, 0x20, 0x00, 0x00, 0x02, 0x00,
   0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x08, 0x08,
   0x40, 0x88, 0xd9, 0x01,
};

static void assert_dcc_rejected(const unsigned char *bytes, size_t len)
{
   DS1EDIT_INDEX_FRAME *frames = NULL;
   size_t count = 0;
   char err[256] = "";

   TEST_ASSERT_FALSE(dcc_decode_indices(bytes, len, -1, &frames, &count, err, sizeof(err)));
   TEST_ASSERT_NOT_EQUAL(0, (int) strlen(err));
   TEST_ASSERT_NULL(frames);
}

void test_dcc_sequence_u8_at_29_sub_bitstream_sizes_beyond_the_direction(void)
{
   assert_dcc_rejected(s_dcc_sub_bitstreams_overrun_a, sizeof(s_dcc_sub_bitstreams_overrun_a));
}

void test_dcc_sequence_u8_at_111_sub_bitstream_sizes_beyond_the_direction(void)
{
   assert_dcc_rejected(s_dcc_sub_bitstreams_overrun_b, sizeof(s_dcc_sub_bitstreams_overrun_b));
}

/* ---- DT1 ------------------------------------------------------------------ */

static void put32(uint8_t *p, int32_t v)
{
   p[0] = (uint8_t) v; p[1] = (uint8_t) (v >> 8); p[2] = (uint8_t) (v >> 16); p[3] = (uint8_t) (v >> 24);
}

/* One floor block (160 x 80) with one 256-byte isometric sub-tile. */
static size_t build_one_block(uint8_t *f)
{
   size_t tiles_ptr = 276 + 96;
   int i;

   memset(f, 0, 4096);
   put32(f + 0, 7);
   put32(f + 4, 6);
   put32(f + 268, 1);
   put32(f + 272, 276);
   put32(f + 276 + 8, -80);          /* size_y */
   put32(f + 276 + 12, 160);         /* size_x */
   put32(f + 276 + 72, (int32_t) tiles_ptr);
   put32(f + 276 + 76, 20 + 256);
   put32(f + 276 + 80, 1);
   f[tiles_ptr + 8] = 1;             /* sub-tile format: isometric */
   f[tiles_ptr + 10] = 0;
   f[tiles_ptr + 11] = 1;            /* length 256 */
   put32(f + tiles_ptr + 16, 20);
   for (i = 0; i < 256; i++)
      f[tiles_ptr + 20 + i] = (uint8_t) (1 + i % 100);
   return tiles_ptr + 20 + 256;
}

static int load_patched(int offset, int32_t value, const char *name)
{
   static uint8_t f[4096];
   size_t len = build_one_block(f);

   if (offset >= 0)
      put32(f + offset, value);
   visual_fetch_reset();
   visual_fetch_add((char *) name, f, len);
   return dt1_add((char *) name);
}

void test_the_unpatched_block_loads_and_draws(void)
{
   int idx = load_patched(-1, 0, "Data\\Global\\Tiles\\Reg\\ok.dt1");
   CACHED_TILE *ct;

   TEST_ASSERT_TRUE(idx >= 0);
   ct = glb_dt1[idx].block_cache[ZM_11][0];
   TEST_ASSERT_NOT_NULL(ct);
   TEST_ASSERT_EQUAL_INT(160, ct->width);
   TEST_ASSERT_EQUAL_INT(80, ct->height);
   dt1_del(idx);
}

/* dt1.synthetic_floor_wall.u32@348=0x7fffffff (and @444): tiles_ptr + 20 * s
 * wrapped a 32-bit long, passed the bounds test, and read far outside the
 * file. */
void test_dt1_tiles_ptr_at_the_top_of_the_address_range_is_skipped(void)
{
   int idx = load_patched(276 + 72, 0x7fffffff, "Data\\Global\\Tiles\\Reg\\ptr.dt1");
   CACHED_TILE *ct;
   int i;

   TEST_ASSERT_TRUE(idx >= 0);
   ct = glb_dt1[idx].block_cache[ZM_11][0];
   TEST_ASSERT_NOT_NULL(ct);
   for (i = 0; i < ct->width * ct->height; i++)
      TEST_ASSERT_EQUAL_UINT8(0, ct->indices[i]);
   dt1_del(idx);
}

void test_dt1_sub_tile_data_offset_at_the_top_of_the_range_is_skipped(void)
{
   static uint8_t f[4096];
   size_t len = build_one_block(f);
   int idx, i;
   CACHED_TILE *ct;

   put32(f + 276 + 96 + 16, 0x7fffffff);       /* sub-tile data offset */
   visual_fetch_reset();
   visual_fetch_add("Data\\Global\\Tiles\\Reg\\off.dt1", f, len);
   idx = dt1_add("Data\\Global\\Tiles\\Reg\\off.dt1");
   TEST_ASSERT_TRUE(idx >= 0);
   ct = glb_dt1[idx].block_cache[ZM_11][0];
   TEST_ASSERT_NOT_NULL(ct);
   for (i = 0; i < ct->width * ct->height; i++)
      TEST_ASSERT_EQUAL_UINT8(0, ct->indices[i]);
   dt1_del(idx);
}

/* dt1.synthetic_floor_wall.u8@386=0x7f: a block width of 0x7f0000a0 made
 * w * h wrap the 32-bit allocation size, and the decoder wrote past it. The
 * block is now treated as empty. */
void test_dt1_absurd_block_width_is_an_empty_block(void)
{
   int idx = load_patched(276 + 12, 0x7f0000a0, "Data\\Global\\Tiles\\Reg\\wide.dt1");

   TEST_ASSERT_TRUE(idx >= 0);
   TEST_ASSERT_NULL(glb_dt1[idx].block_cache[ZM_11][0]);
   dt1_del(idx);
}

void test_dt1_negative_block_width_is_an_empty_block(void)
{
   int idx = load_patched(276 + 12, -160, "Data\\Global\\Tiles\\Reg\\neg.dt1");

   TEST_ASSERT_TRUE(idx >= 0);
   TEST_ASSERT_NULL(glb_dt1[idx].block_cache[ZM_11][0]);
   dt1_del(idx);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_dcc_sequence_u8_at_29_sub_bitstream_sizes_beyond_the_direction);
   RUN_TEST(test_dcc_sequence_u8_at_111_sub_bitstream_sizes_beyond_the_direction);
   RUN_TEST(test_the_unpatched_block_loads_and_draws);
   RUN_TEST(test_dt1_tiles_ptr_at_the_top_of_the_address_range_is_skipped);
   RUN_TEST(test_dt1_sub_tile_data_offset_at_the_top_of_the_range_is_skipped);
   RUN_TEST(test_dt1_absurd_block_width_is_an_empty_block);
   RUN_TEST(test_dt1_negative_block_width_is_an_empty_block);
   return UNITY_END();
}
