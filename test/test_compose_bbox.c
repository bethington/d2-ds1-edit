#include <string.h>

#include "unity/unity.h"

#include "core/compose_bbox.h"

void setUp(void) {}
void tearDown(void) {}

static void test_single_layer_canvas_matches(void)
{
   COMPOSE_LAYER_RECT_S rects[1] = {{ 0, 0, 32, 48 }};
   int cw, ch, bx[1], by[1];

   TEST_ASSERT_EQUAL_INT(1, compose_bbox_union(rects, 1, &cw, &ch, bx, by));
   TEST_ASSERT_EQUAL_INT(32, cw);
   TEST_ASSERT_EQUAL_INT(48, ch);
   TEST_ASSERT_EQUAL_INT(0, bx[0]);
   TEST_ASSERT_EQUAL_INT(0, by[0]);
}

static void test_single_layer_with_offset(void)
{
   COMPOSE_LAYER_RECT_S rects[1] = {{ 10, 20, 32, 48 }};
   int cw, ch, bx[1], by[1];

   TEST_ASSERT_EQUAL_INT(1, compose_bbox_union(rects, 1, &cw, &ch, bx, by));
   TEST_ASSERT_EQUAL_INT(32, cw);
   TEST_ASSERT_EQUAL_INT(48, ch);
   TEST_ASSERT_EQUAL_INT(0, bx[0]);  /* canvas-relative; layer is at top-left */
   TEST_ASSERT_EQUAL_INT(0, by[0]);
}

static void test_two_layers_stacked_vertically(void)
{
   /* Head at (0, 0) 32x16; Torso at (4, 16) 24x32. Canvas should
    * span 32 wide (head's width) by 48 tall (top of head to bottom
    * of torso). */
   COMPOSE_LAYER_RECT_S rects[2] = {
      { 0,  0, 32, 16 },
      { 4, 16, 24, 32 }
   };
   int cw, ch, bx[2], by[2];

   TEST_ASSERT_EQUAL_INT(1, compose_bbox_union(rects, 2, &cw, &ch, bx, by));
   TEST_ASSERT_EQUAL_INT(32, cw);
   TEST_ASSERT_EQUAL_INT(48, ch);
   TEST_ASSERT_EQUAL_INT(0, bx[0]);
   TEST_ASSERT_EQUAL_INT(0, by[0]);
   TEST_ASSERT_EQUAL_INT(4, bx[1]);
   TEST_ASSERT_EQUAL_INT(16, by[1]);
}

static void test_two_layers_with_negative_offset(void)
{
   /* Layer 0 at (-5, -10) 20x20; layer 1 at (5, 5) 20x20. Canvas
    * spans from min(-5,5)=-5 to max(15,25)=25 = 30 wide, and
    * from min(-10,5)=-10 to max(10,25)=25 = 35 tall.
    * Layer 0 blit at (0, 0); layer 1 blit at (10, 15). */
   COMPOSE_LAYER_RECT_S rects[2] = {
      { -5, -10, 20, 20 },
      {  5,   5, 20, 20 }
   };
   int cw, ch, bx[2], by[2];

   TEST_ASSERT_EQUAL_INT(1, compose_bbox_union(rects, 2, &cw, &ch, bx, by));
   TEST_ASSERT_EQUAL_INT(30, cw);
   TEST_ASSERT_EQUAL_INT(35, ch);
   TEST_ASSERT_EQUAL_INT(0, bx[0]);
   TEST_ASSERT_EQUAL_INT(0, by[0]);
   TEST_ASSERT_EQUAL_INT(10, bx[1]);
   TEST_ASSERT_EQUAL_INT(15, by[1]);
}

static void test_overlapping_layers(void)
{
   /* Two layers fully overlapping at (0, 0) 32x32. Canvas is 32x32. */
   COMPOSE_LAYER_RECT_S rects[2] = {
      { 0, 0, 32, 32 },
      { 0, 0, 32, 32 }
   };
   int cw, ch, bx[2], by[2];

   TEST_ASSERT_EQUAL_INT(1, compose_bbox_union(rects, 2, &cw, &ch, bx, by));
   TEST_ASSERT_EQUAL_INT(32, cw);
   TEST_ASSERT_EQUAL_INT(32, ch);
   TEST_ASSERT_EQUAL_INT(0, bx[0]);
   TEST_ASSERT_EQUAL_INT(0, by[0]);
   TEST_ASSERT_EQUAL_INT(0, bx[1]);
   TEST_ASSERT_EQUAL_INT(0, by[1]);
}

static void test_zero_sized_layer_skipped(void)
{
   COMPOSE_LAYER_RECT_S rects[3] = {
      { 0, 0, 32, 32 },
      { 5, 5,  0, 10 },   /* zero width - skipped */
      { 8, 8, 16, 16 }
   };
   int cw, ch, bx[3], by[3];

   TEST_ASSERT_EQUAL_INT(1, compose_bbox_union(rects, 3, &cw, &ch, bx, by));
   TEST_ASSERT_EQUAL_INT(32, cw);
   TEST_ASSERT_EQUAL_INT(32, ch);
   TEST_ASSERT_EQUAL_INT(0, bx[0]);
   TEST_ASSERT_EQUAL_INT(0, by[0]);
   TEST_ASSERT_EQUAL_INT(-1, bx[1]);  /* skipped */
   TEST_ASSERT_EQUAL_INT(-1, by[1]);
   TEST_ASSERT_EQUAL_INT(8, bx[2]);
   TEST_ASSERT_EQUAL_INT(8, by[2]);
}

static void test_all_zero_sized_returns_failure(void)
{
   COMPOSE_LAYER_RECT_S rects[2] = {
      { 0, 0, 0, 10 },
      { 5, 5, 10, 0 }
   };
   int cw = 999, ch = 999, bx[2] = {99, 99}, by[2] = {99, 99};

   TEST_ASSERT_EQUAL_INT(0, compose_bbox_union(rects, 2, &cw, &ch, bx, by));
   TEST_ASSERT_EQUAL_INT(0, cw);
   TEST_ASSERT_EQUAL_INT(0, ch);
}

static void test_rejects_zero_count(void)
{
   COMPOSE_LAYER_RECT_S rects[1] = {{ 0, 0, 32, 32 }};
   int cw, ch, bx[1], by[1];
   TEST_ASSERT_EQUAL_INT(0, compose_bbox_union(rects, 0, &cw, &ch, bx, by));
}

static void test_rejects_null_inputs(void)
{
   COMPOSE_LAYER_RECT_S rects[1] = {{ 0, 0, 32, 32 }};
   int cw, ch, bx[1], by[1];
   TEST_ASSERT_EQUAL_INT(0, compose_bbox_union(NULL, 1, &cw, &ch, bx, by));
   TEST_ASSERT_EQUAL_INT(0, compose_bbox_union(rects, 1, NULL, &ch, bx, by));
   TEST_ASSERT_EQUAL_INT(0, compose_bbox_union(rects, 1, &cw, NULL, bx, by));
   TEST_ASSERT_EQUAL_INT(0, compose_bbox_union(rects, 1, &cw, &ch, NULL, by));
   TEST_ASSERT_EQUAL_INT(0, compose_bbox_union(rects, 1, &cw, &ch, bx, NULL));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_single_layer_canvas_matches);
   RUN_TEST(test_single_layer_with_offset);
   RUN_TEST(test_two_layers_stacked_vertically);
   RUN_TEST(test_two_layers_with_negative_offset);
   RUN_TEST(test_overlapping_layers);
   RUN_TEST(test_zero_sized_layer_skipped);
   RUN_TEST(test_all_zero_sized_returns_failure);
   RUN_TEST(test_rejects_zero_count);
   RUN_TEST(test_rejects_null_inputs);
   return UNITY_END();
}
