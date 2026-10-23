#include "unity/unity.h"
#include "editor/selection_logic.h"

void setUp(void) {}
void tearDown(void) {}

void test_strict_match_ignores_layer_order_but_keeps_multiplicity(void)
{
   const int reference[] = {11, 22, 11};
   const int reordered[] = {11, 11, 22};
   const int missing_duplicate[] = {11, 22};
   TEST_ASSERT_TRUE(selection_tile_multiset_equal(reference, 3, reordered, 3));
   TEST_ASSERT_FALSE(selection_tile_multiset_equal(reference, 3, missing_duplicate, 2));
}

void test_strict_match_rejects_extra_or_different_tiles(void)
{
   const int reference[] = {11, 22};
   const int extra[] = {11, 22, 33};
   const int different[] = {11, 44};
   TEST_ASSERT_FALSE(selection_tile_multiset_equal(reference, 2, extra, 3));
   TEST_ASSERT_FALSE(selection_tile_multiset_equal(reference, 2, different, 2));
   TEST_ASSERT_TRUE(selection_tile_multiset_equal(NULL, 0, NULL, 0));
}

void test_object_box_includes_edges_in_either_drag_direction(void)
{
   TEST_ASSERT_TRUE(selection_point_in_box(12, 8, 4, 2, 4, 8));
   TEST_ASSERT_TRUE(selection_point_in_box(12, 8, 4, 2, 12, 2));
   TEST_ASSERT_TRUE(selection_point_in_box(12, 8, 4, 2, 7, 5));
   TEST_ASSERT_FALSE(selection_point_in_box(12, 8, 4, 2, 3, 5));
   TEST_ASSERT_FALSE(selection_point_in_box(12, 8, 4, 2, 7, 9));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_strict_match_ignores_layer_order_but_keeps_multiplicity);
   RUN_TEST(test_strict_match_rejects_extra_or_different_tiles);
   RUN_TEST(test_object_box_includes_edges_in_either_drag_direction);
   return UNITY_END();
}
