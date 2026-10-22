/* DT1 loading through the real dt1_add (header, block headers, sub-tile
 * headers, isometric and RLE sub-tile decoding, block image layout). Only
 * the archive fetch is replaced; it serves DT1 bytes authored below from the
 * documented layout (docs/guides/dt1-tile-format.md):
 *
 *   file header 276 bytes: version 7, flags 6, block count @268, headers @272
 *   block header 96 bytes: direction, roof_y(u16), sound, animated, size_y,
 *     size_x, -, orientation, main, sub, rarity, -, 25 flags, -, tiles_ptr,
 *     tiles_length, tiles_number
 *   sub-tile header 20 bytes: x, y (i16), -, grid x, grid y, format (u16),
 *     length (u16), -, data offset (i32, from tiles_ptr) */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "core/dt1.h"
#include "core/rgba_cache.h"
#include "visual_test_support.h"

#define SENTINEL 0xAB

static uint8_t s_file[8192];
static size_t  s_len;
static RGBA_PALETTE s_pal;

static void put16(uint8_t *p, int v) { p[0] = (uint8_t) v; p[1] = (uint8_t) (v >> 8); }
static void put32(uint8_t *p, int32_t v)
{
   p[0] = (uint8_t) v; p[1] = (uint8_t) (v >> 8); p[2] = (uint8_t) (v >> 16); p[3] = (uint8_t) (v >> 24);
}

typedef struct
{
   int x, y, format;
   const uint8_t *data;
   int length;            /* the header's length field */
   int data_len;          /* bytes actually stored (may exceed `length`) */
   int32_t forced_offset; /* nonzero: write this data offset instead */
} AUTHORED_SUBTILE;

typedef struct
{
   int32_t orientation, main_index, sub_index, rarity, size_x, size_y, direction;
   int roof_y, animated;
   const AUTHORED_SUBTILE *subtiles;
   int n;
} AUTHORED_BLOCK;

/* Lay out: header | block headers | per block: subtile headers, data. */
static void build_dt1(const AUTHORED_BLOCK *blocks, int nb)
{
   size_t pos;
   int b, s;

   memset(s_file, SENTINEL, sizeof(s_file));
   memset(s_file, 0, 276 + 96 * (size_t) nb);
   put32(s_file + 0, 7);
   put32(s_file + 4, 6);
   put32(s_file + 268, nb);
   put32(s_file + 272, 276);
   pos = 276 + 96 * (size_t) nb;
   for (b = 0; b < nb; b++)
   {
      uint8_t *bh = s_file + 276 + 96 * b;
      size_t tiles_ptr = pos, data_pos = pos + 20 * (size_t) blocks[b].n;

      put32(bh + 0, blocks[b].direction);
      put16(bh + 4, blocks[b].roof_y);
      bh[7] = (uint8_t) blocks[b].animated;
      put32(bh + 8, blocks[b].size_y);
      put32(bh + 12, blocks[b].size_x);
      put32(bh + 20, blocks[b].orientation);
      put32(bh + 24, blocks[b].main_index);
      put32(bh + 28, blocks[b].sub_index);
      put32(bh + 32, blocks[b].rarity);
      put32(bh + 72, (int32_t) tiles_ptr);
      put32(bh + 80, blocks[b].n);
      for (s = 0; s < blocks[b].n; s++)
      {
         const AUTHORED_SUBTILE *st = &blocks[b].subtiles[s];
         uint8_t *sh = s_file + tiles_ptr + 20 * s;
         memset(sh, 0, 20);
         put16(sh + 0, st->x);
         put16(sh + 2, st->y);
         put16(sh + 8, st->format);
         put16(sh + 10, st->length);
         put32(sh + 16, st->forced_offset ? st->forced_offset
                                          : (int32_t) (data_pos - tiles_ptr));
         memcpy(s_file + data_pos, st->data, (size_t) st->data_len);
         data_pos += (size_t) st->data_len + 4;   /* 4 sentinel bytes between */
      }
      put32(bh + 76, (int32_t) (data_pos - tiles_ptr));
      pos = data_pos;
   }
   s_len = pos + 16;                              /* trailing sentinels */
}

static int load(const char *name)
{
   visual_fetch_reset();
   visual_fetch_add(name, s_file, s_len);
   return dt1_add((char *) name);
}

void setUp(void)
{
   int i;
   TEST_ASSERT_TRUE(visual_allegro_init());
   for (i = 0; i < 256; i++)
   {
      s_pal.colors[i].r = (uint8_t) i;
      s_pal.colors[i].g = (uint8_t) (i ^ 0x55);
      s_pal.colors[i].b = (uint8_t) (255 - i);
      s_pal.colors[i].a = (i == 0) ? 0 : 255;
   }
   a5_current_palette = &s_pal;
}

void tearDown(void) {}

static CACHED_TILE *tile(int idx, int b)
{
   TEST_ASSERT_NOT_NULL(glb_dt1[idx].block_cache[ZM_11]);
   return glb_dt1[idx].block_cache[ZM_11][b];
}

static void assert_no_sentinel(const CACHED_TILE *ct)
{
   int i;
   for (i = 0; i < ct->width * ct->height; i++)
      TEST_ASSERT_NOT_EQUAL_MESSAGE(SENTINEL, ct->indices[i], "bytes outside the sub-tile were drawn");
}

static void assert_bitmap_matches_indices(int idx, int b)
{
   CACHED_TILE *ct = tile(idx, b);
   ALLEGRO_BITMAP *bmp = glb_dt1[idx].block_zoom[ZM_11][b];
   int x, y;

   TEST_ASSERT_NOT_NULL(bmp);
   TEST_ASSERT_EQUAL_INT(ct->width, al_get_bitmap_width(bmp));
   TEST_ASSERT_EQUAL_INT(ct->height, al_get_bitmap_height(bmp));
   for (y = 0; y < ct->height; y++)
      for (x = 0; x < ct->width; x++)
      {
         unsigned char r, g, bl, a;
         uint8_t v = ct->indices[y * ct->width + x];
         al_unmap_rgba(al_get_pixel(bmp, x, y), &r, &g, &bl, &a);
         TEST_ASSERT_EQUAL_UINT8(s_pal.colors[v].a, a);
         if (v != 0)
         {
            TEST_ASSERT_EQUAL_UINT8(s_pal.colors[v].r, r);
            TEST_ASSERT_EQUAL_UINT8(s_pal.colors[v].b, bl);
         }
      }
}

/* ---- authored tiles ----------------------------------------------------------- */

static uint8_t s_iso[256];
static const uint8_t s_rle[] = {
   /* row 0: skip 2, 3 pixels */      2, 3, 11, 12, 13,
   /* row 0: skip 1, 1 pixel  */      1, 1, 14,
   /* end row */                      0, 0,
   /* row 1: 0 skip, 2 pixels */      0, 2, 21, 22,
   0, 0 };
/* a run claiming 5 pixels when only 2 bytes of the sub-tile remain */
static const uint8_t s_rle_overrun[] = { 0, 5, 31, 32 };

void test_floor_and_wall_blocks_decode_with_their_metadata(void)
{
   AUTHORED_SUBTILE floor_st[1] = { { 64, 0, 1, s_iso, 256, 256, 0 } };
   AUTHORED_SUBTILE wall_st[1]  = { { 10, -64, 0, s_rle, (int) sizeof(s_rle), (int) sizeof(s_rle), 0 } };
   AUTHORED_BLOCK blocks[2] = {
      { 0, 5, 2, 7, 160, -80, 3, 0, 0, floor_st, 1 },
      { 1, 9, 4, 1, 160, -96, 0, 40, 1, wall_st, 1 } };
   int idx, i, x, y, n;
   BLOCK_S *bh;
   CACHED_TILE *ct;
   static const int xjump[15] = {14, 12, 10, 8, 6, 4, 2, 0, 2, 4, 6, 8, 10, 12, 14};
   static const int nbpix[15] = {4, 8, 12, 16, 20, 24, 28, 32, 28, 24, 20, 16, 12, 8, 4};

   for (i = 0; i < 256; i++)
      s_iso[i] = (uint8_t) (1 + (i % 160));   /* never the sentinel */
   build_dt1(blocks, 2);
   idx = load("Data\\Global\\Tiles\\Test\\a.dt1");
   TEST_ASSERT_TRUE(idx >= 0);
   TEST_ASSERT_EQUAL_INT(2, glb_dt1[idx].block_num);

   bh = (BLOCK_S *) glb_dt1[idx].bh_buffer;
   TEST_ASSERT_EQUAL_INT32(0, bh[0].orientation);
   TEST_ASSERT_EQUAL_INT32(5, bh[0].main_index);
   TEST_ASSERT_EQUAL_INT32(2, bh[0].sub_index);
   TEST_ASSERT_EQUAL_INT32(7, bh[0].rarity);
   TEST_ASSERT_EQUAL_INT32(3, bh[0].direction);
   TEST_ASSERT_EQUAL_INT32(1, bh[0].tiles_number);
   TEST_ASSERT_EQUAL_INT32(1, bh[1].orientation);
   TEST_ASSERT_EQUAL_INT32(9, bh[1].main_index);
   TEST_ASSERT_EQUAL_UINT16(40, bh[1].roof_y);
   TEST_ASSERT_EQUAL_UINT8(1, bh[1].animated);

   /* floor: 160x80 image, diamond sub-tile at (64, 0) */
   ct = tile(idx, 0);
   TEST_ASSERT_EQUAL_INT(160, ct->width);
   TEST_ASSERT_EQUAL_INT(80, ct->height);
   n = 0;
   for (y = 0; y < 15; y++)
      for (x = 0; x < nbpix[y]; x++, n++)
         TEST_ASSERT_EQUAL_UINT8(s_iso[n], ct->indices[y * 160 + 64 + xjump[y] + x]);
   TEST_ASSERT_EQUAL_UINT8(0, ct->indices[0 * 160 + 64 + 13]);  /* left of row 0 */
   TEST_ASSERT_EQUAL_UINT8(0, ct->indices[15 * 160 + 64 + 16]); /* below the diamond */
   assert_no_sentinel(ct);
   assert_bitmap_matches_indices(idx, 0);

   /* upper wall: height 96 - 32 = 64, zero line at the bottom, so y = -64 is
    * the top row */
   ct = tile(idx, 1);
   TEST_ASSERT_EQUAL_INT(160, ct->width);
   TEST_ASSERT_EQUAL_INT(64, ct->height);
   TEST_ASSERT_EQUAL_UINT8(11, ct->indices[0 * 160 + 12]);
   TEST_ASSERT_EQUAL_UINT8(13, ct->indices[0 * 160 + 14]);
   TEST_ASSERT_EQUAL_UINT8(0, ct->indices[0 * 160 + 15]);
   TEST_ASSERT_EQUAL_UINT8(14, ct->indices[0 * 160 + 16]);
   TEST_ASSERT_EQUAL_UINT8(21, ct->indices[1 * 160 + 10]);
   TEST_ASSERT_EQUAL_UINT8(22, ct->indices[1 * 160 + 11]);
   TEST_ASSERT_EQUAL_UINT8(0, ct->indices[1 * 160 + 12]);
   assert_no_sentinel(ct);
   assert_bitmap_matches_indices(idx, 1);
   dt1_del(idx);
}

void test_rle_run_longer_than_the_sub_tile_is_not_drawn_from_outside(void)
{
   AUTHORED_SUBTILE st[1] = { { 0, 0, 0, s_rle_overrun, 4, 4, 0 } };
   AUTHORED_BLOCK blocks[1] = { { 0, 1, 1, 0, 160, -80, 0, 0, 0, st, 1 } };
   int idx;
   CACHED_TILE *ct;

   build_dt1(blocks, 1);
   idx = load("Data\\Global\\Tiles\\Test\\overrun.dt1");
   TEST_ASSERT_TRUE(idx >= 0);
   ct = tile(idx, 0);
   TEST_ASSERT_EQUAL_UINT8(31, ct->indices[0]);
   TEST_ASSERT_EQUAL_UINT8(32, ct->indices[1]);
   assert_no_sentinel(ct);
   dt1_del(idx);
}

void test_sub_tile_data_outside_the_file_is_skipped(void)
{
   /* a header pointing far past the file must neither crash nor draw */
   AUTHORED_SUBTILE st[2] = {
      { 0, 0, 1, s_iso, 256, 256, 0x7FFF0000 },
      { 0, 0, 0, s_rle, (int) sizeof(s_rle), (int) sizeof(s_rle), 0 } };
   AUTHORED_BLOCK blocks[1] = { { 0, 1, 1, 0, 160, -80, 0, 0, 0, st, 2 } };
   int idx;
   CACHED_TILE *ct;

   build_dt1(blocks, 1);
   idx = load("Data\\Global\\Tiles\\Test\\far.dt1");
   TEST_ASSERT_TRUE(idx >= 0);
   ct = tile(idx, 0);
   TEST_ASSERT_EQUAL_UINT8(11, ct->indices[2]);    /* the valid sub-tile still lands */
   assert_no_sentinel(ct);
   dt1_del(idx);
}

void test_invalid_headers_are_rejected(void)
{
   AUTHORED_SUBTILE st[1] = { { 0, 0, 0, s_rle, (int) sizeof(s_rle), (int) sizeof(s_rle), 0 } };
   AUTHORED_BLOCK blocks[1] = { { 0, 1, 1, 0, 160, -80, 0, 0, 0, st, 1 } };

   build_dt1(blocks, 1);
   put32(s_file + 0, 6);
   TEST_ASSERT_EQUAL_INT(-1, load("Data\\Global\\Tiles\\Test\\v6.dt1"));
   build_dt1(blocks, 1);
   put32(s_file + 268, 1000);
   TEST_ASSERT_EQUAL_INT(-1, load("Data\\Global\\Tiles\\Test\\many.dt1"));
}

int main(void)
{
   setvbuf(stdout, NULL, _IONBF, 0);   /* keep results if a test crashes */
   UNITY_BEGIN();
   RUN_TEST(test_floor_and_wall_blocks_decode_with_their_metadata);
   RUN_TEST(test_rle_run_longer_than_the_sub_tile_is_not_drawn_from_outside);
   RUN_TEST(test_sub_tile_data_outside_the_file_is_skipped);
   RUN_TEST(test_invalid_headers_are_rejected);
   return UNITY_END();
}
