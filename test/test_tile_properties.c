/* DS1 cell -> DT1 block association through the real wprop_2_block,
 * fprop_2_block and sprop_2_block (src/core/ds1.c).
 *
 * Expected keys follow the game's extraction (ds1-map-format.md, D2Common
 * 6fd5c32a): main_index = (cell >> 20) & 0x3F, sub_index = (cell >> 8) & 0xFF;
 * walls match their orientation, floors orientation 0, shadows orientation 13.
 * Every main/sub combination is exercised for each layer kind. */

#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "core/ds1.h"
#include "visual_test_support.h"

static DS1_S s_map;
DS1_S *glb_ds1 = &s_map;

static BLOCK_TABLE_S *s_bt;

static void set_table(int n)
{
   free(s_bt);
   s_bt = (BLOCK_TABLE_S *) calloc((size_t) n, sizeof(BLOCK_TABLE_S));
   memset(&s_map, 0, sizeof(s_map));
   s_map.block_table = s_bt;
   s_map.bt_num = n;
}

static void set_block(int b, long orientation, long main_index, long sub_index)
{
   s_bt[b].orientation    = orientation;
   s_bt[b].main_index     = main_index;
   s_bt[b].sub_index      = sub_index;
   s_bt[b].used_by_editor = 1;
}

/* A table with every (main, sub) for one orientation: block m*256+s. */
static void full_table(long orientation)
{
   int m, s;
   set_table(64 * 256);
   for (m = 0; m < 64; m++)
      for (s = 0; s < 256; s++)
         set_block(m * 256 + s, orientation, m, s);
}

static void cell_props(int main_index, int sub, UBYTE *p1, UBYTE *p2, UBYTE *p3, UBYTE *p4)
{
   *p1 = 0x80;
   *p2 = (UBYTE) sub;
   *p3 = (UBYTE) ((main_index & 0x0F) << 4);
   *p4 = (UBYTE) ((main_index >> 4) & 0x03);
}

void setUp(void) {}
void tearDown(void) {}

/* ---- exhaustive keys ------------------------------------------------------- */

void test_every_floor_key_resolves(void)
{
   int m, s;
   full_table(0);
   for (m = 0; m < 64; m++)
      for (s = 0; s < 256; s++)
      {
         CELL_F_S c;
         memset(&c, 0, sizeof(c));
         cell_props(m, s, &c.prop1, &c.prop2, &c.prop3, &c.prop4);
         fprop_2_block(0, &c);
         TEST_ASSERT_EQUAL_INT(m * 256 + s, c.bt_idx);
      }
}

void test_every_shadow_key_resolves(void)
{
   int m, s;
   full_table(13);
   for (m = 0; m < 64; m++)
      for (s = 0; s < 256; s++)
      {
         CELL_S_S c;
         memset(&c, 0, sizeof(c));
         cell_props(m, s, &c.prop1, &c.prop2, &c.prop3, &c.prop4);
         c.prop4 |= 0x08;                       /* IsShadow, as on disk */
         sprop_2_block(0, &c);
         TEST_ASSERT_EQUAL_INT(m * 256 + s, c.bt_idx);
      }
}

void test_every_wall_key_resolves_for_its_orientation(void)
{
   static const long orientations[] = { 1, 5, 10, 15, 16 };
   int o, m, s;

   for (o = 0; o < 5; o++)
   {
      full_table(orientations[o]);
      for (m = 0; m < 64; m++)
         for (s = 0; s < 256; s += (o == 0) ? 1 : 17)
         {
            CELL_W_S c;
            memset(&c, 0, sizeof(c));
            cell_props(m, s, &c.prop1, &c.prop2, &c.prop3, &c.prop4);
            c.prop1 = 0x81;
            c.orientation = (UBYTE) orientations[o];
            wprop_2_block(0, &c);
            TEST_ASSERT_EQUAL_INT(m * 256 + s, c.bt_idx);
         }
   }
}

/* ---- independent flag bits ---------------------------------------------------- */

void test_non_key_bits_neither_change_the_key_nor_get_rewritten(void)
{
   /* Every bit outside main/sub, flipped one at a time: prop1 bits 0..6,
    * prop3 bits 0..3, prop4 bits 2..7. The association must not move and the
    * raw bytes (and the editor's own flags) must come back untouched. */
   static const int mains[] = { 0, 1, 15, 16, 47, 63 };
   static const int subs[] = { 0, 1, 63, 64, 255 };
   int mi, si, bit;

   full_table(0);
   for (mi = 0; mi < 6; mi++)
      for (si = 0; si < 5; si++)
         for (bit = 0; bit < 32; bit++)
         {
            UBYTE p[4];
            CELL_F_S c;
            int byte = bit / 8, mask = 1 << (bit % 8);
            int is_key = (byte == 1) || (byte == 2 && mask >= 0x10)
                      || (byte == 3 && mask <= 0x02) || (byte == 0 && mask == 0x80);
            if (is_key)
               continue;
            cell_props(mains[mi], subs[si], &p[0], &p[1], &p[2], &p[3]);
            p[byte] ^= (UBYTE) mask;
            memset(&c, 0, sizeof(c));
            c.prop1 = p[0]; c.prop2 = p[1]; c.prop3 = p[2]; c.prop4 = p[3];
            c.flags = 0x5A;
            fprop_2_block(0, &c);
            TEST_ASSERT_EQUAL_INT(mains[mi] * 256 + subs[si], c.bt_idx);
            TEST_ASSERT_EQUAL_HEX8(p[0], c.prop1);
            TEST_ASSERT_EQUAL_HEX8(p[1], c.prop2);
            TEST_ASSERT_EQUAL_HEX8(p[2], c.prop3);
            TEST_ASSERT_EQUAL_HEX8(p[3], c.prop4);
            TEST_ASSERT_EQUAL_HEX8(0x5A, c.flags);
         }
}

void test_bit_26_is_not_part_of_the_main_index(void)
{
   /* Control for a seven-bit mask: a decoy block keyed main|64 sits first.
    * Six bits (the game's AND 0x3F) skip it; seven bits would take it. */
   CELL_W_S c;

   set_table(2);
   set_block(0, 3, 5 | 64, 9);
   set_block(1, 3, 5, 9);
   memset(&c, 0, sizeof(c));
   cell_props(5, 9, &c.prop1, &c.prop2, &c.prop3, &c.prop4);
   c.prop4 |= 0x04;
   c.orientation = 3;
   wprop_2_block(0, &c);
   TEST_ASSERT_EQUAL_INT(1, c.bt_idx);
}

/* ---- association rules ----------------------------------------------------------- */

void test_empty_cell_and_missing_association(void)
{
   CELL_F_S f;
   CELL_W_S w;
   CELL_S_S s;

   set_table(1);
   set_block(0, 0, 1, 1);

   memset(&f, 0, sizeof(f));
   f.prop2 = 1; f.prop3 = 0x10;                 /* prop1 == 0: no tile */
   fprop_2_block(0, &f);
   TEST_ASSERT_EQUAL_INT(0, f.bt_idx);

   memset(&f, 0, sizeof(f));
   cell_props(2, 1, &f.prop1, &f.prop2, &f.prop3, &f.prop4);
   fprop_2_block(0, &f);
   TEST_ASSERT_EQUAL_INT(-1, f.bt_idx);

   /* a floor block does not answer for a wall or a shadow */
   memset(&w, 0, sizeof(w));
   cell_props(1, 1, &w.prop1, &w.prop2, &w.prop3, &w.prop4);
   w.orientation = 1;
   wprop_2_block(0, &w);
   TEST_ASSERT_EQUAL_INT(-1, w.bt_idx);
   memset(&s, 0, sizeof(s));
   cell_props(1, 1, &s.prop1, &s.prop2, &s.prop3, &s.prop4);
   sprop_2_block(0, &s);
   TEST_ASSERT_EQUAL_INT(-1, s.bt_idx);

   /* a block the editor disabled is not a match */
   s_bt[0].used_by_editor = 0;
   memset(&f, 0, sizeof(f));
   cell_props(1, 1, &f.prop1, &f.prop2, &f.prop3, &f.prop4);
   fprop_2_block(0, &f);
   TEST_ASSERT_EQUAL_INT(-1, f.bt_idx);
}

void test_first_matching_block_wins(void)
{
   CELL_F_S f;
   set_table(3);
   set_block(0, 0, 4, 2);
   set_block(1, 0, 7, 3);
   set_block(2, 0, 7, 3);
   memset(&f, 0, sizeof(f));
   cell_props(7, 3, &f.prop1, &f.prop2, &f.prop3, &f.prop4);
   fprop_2_block(0, &f);
   TEST_ASSERT_EQUAL_INT(1, f.bt_idx);
}

void test_orientation_18_19_editor_fallback(void)
{
   /* Editor policy (Siramy's "O=18 M=3 I=1 -> O=19 M=3 I=0" trick), not a
    * documented game rule: try the other door orientation, same sub, then
    * sub 0. */
   CELL_W_S w;

   set_table(2);
   set_block(0, 19, 3, 0);
   set_block(1, 5, 3, 1);
   memset(&w, 0, sizeof(w));
   cell_props(3, 1, &w.prop1, &w.prop2, &w.prop3, &w.prop4);
   w.orientation = 18;
   wprop_2_block(0, &w);
   TEST_ASSERT_EQUAL_INT(0, w.bt_idx);
   TEST_ASSERT_EQUAL_UINT8(18, w.orientation);   /* the cell keeps its own value */

   set_table(2);
   set_block(0, 18, 3, 0);
   set_block(1, 18, 3, 1);
   memset(&w, 0, sizeof(w));
   cell_props(3, 1, &w.prop1, &w.prop2, &w.prop3, &w.prop4);
   w.orientation = 19;
   wprop_2_block(0, &w);
   TEST_ASSERT_EQUAL_INT(1, w.bt_idx);            /* same sub before sub 0 */

   /* no fallback for other orientations */
   set_table(1);
   set_block(0, 6, 3, 0);
   memset(&w, 0, sizeof(w));
   cell_props(3, 1, &w.prop1, &w.prop2, &w.prop3, &w.prop4);
   w.orientation = 5;
   wprop_2_block(0, &w);
   TEST_ASSERT_EQUAL_INT(-1, w.bt_idx);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_every_floor_key_resolves);
   RUN_TEST(test_every_shadow_key_resolves);
   RUN_TEST(test_every_wall_key_resolves_for_its_orientation);
   RUN_TEST(test_non_key_bits_neither_change_the_key_nor_get_rewritten);
   RUN_TEST(test_bit_26_is_not_part_of_the_main_index);
   RUN_TEST(test_empty_cell_and_missing_association);
   RUN_TEST(test_first_matching_block_wins);
   RUN_TEST(test_orientation_18_19_editor_fallback);
   free(s_bt);
   return UNITY_END();
}
