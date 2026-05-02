#include "unity/unity.h"

#include "core/glob_match.h"

void setUp(void) {}
void tearDown(void) {}

static void test_exact_match(void)
{
   TEST_ASSERT_TRUE(glob_match("inv2ax.dc6", "inv2ax.dc6"));
   TEST_ASSERT_FALSE(glob_match("inv2ax.dc6", "inv3ax.dc6"));
}

static void test_case_insensitive(void)
{
   TEST_ASSERT_TRUE(glob_match("inv2ax.dc6", "INV2AX.DC6"));
   TEST_ASSERT_TRUE(glob_match("INV*.DC6", "inv2ax.dc6"));
}

static void test_star_within_segment(void)
{
   TEST_ASSERT_TRUE(glob_match("inv*.dc6", "inv2ax.dc6"));
   TEST_ASSERT_TRUE(glob_match("inv*.dc6", "invring.dc6"));
   TEST_ASSERT_TRUE(glob_match("*.dc6", "anything.dc6"));
   TEST_ASSERT_FALSE(glob_match("inv*.dc6", "flpaar.dc6"));
}

static void test_star_does_not_cross_separator(void)
{
   TEST_ASSERT_TRUE(glob_match("data\\*.dc6", "data\\foo.dc6"));
   TEST_ASSERT_FALSE(glob_match("data\\*.dc6", "data\\sub\\foo.dc6"));
}

static void test_question_mark(void)
{
   TEST_ASSERT_TRUE(glob_match("inv?ax.dc6", "inv2ax.dc6"));
   TEST_ASSERT_FALSE(glob_match("inv?ax.dc6", "inv12ax.dc6"));
   TEST_ASSERT_FALSE(glob_match("inv?ax.dc6", "invax.dc6"));
}

static void test_question_mark_does_not_cross_separator(void)
{
   TEST_ASSERT_FALSE(glob_match("data?file", "data\\file"));
}

static void test_double_star_recursive(void)
{
   TEST_ASSERT_TRUE(glob_match("data\\**\\*.dt1",
                               "data\\global\\tiles\\ACT1\\BARRACKS\\barr.dt1"));
   TEST_ASSERT_TRUE(glob_match("data\\**\\*.dc6", "data\\foo.dc6"));
   TEST_ASSERT_TRUE(glob_match("data\\**\\*.dc6", "data\\a\\b\\c\\foo.dc6"));
}

static void test_double_star_matches_zero_segments(void)
{
   TEST_ASSERT_TRUE(glob_match("data\\**\\foo.dc6", "data\\foo.dc6"));
   TEST_ASSERT_TRUE(glob_match("data\\**\\foo.dc6", "data\\sub\\foo.dc6"));
}

static void test_forward_slash_normalization(void)
{
   TEST_ASSERT_TRUE(glob_match("data/global/items/inv*.dc6",
                               "data\\global\\items\\inv2ax.dc6"));
   TEST_ASSERT_TRUE(glob_match("data\\global\\items\\inv*.dc6",
                               "data/global/items/inv2ax.dc6"));
}

static void test_double_star_only_as_full_segment(void)
{
   /* "inv**.dc6" is not a full-segment **; treat as "inv*.dc6". */
   TEST_ASSERT_TRUE(glob_match("inv**.dc6", "inv2ax.dc6"));
   TEST_ASSERT_FALSE(glob_match("inv**.dc6", "flpaar.dc6"));
   /* And it should NOT cross separators just because there are two stars. */
   TEST_ASSERT_FALSE(glob_match("data\\inv**.dc6",
                                "data\\sub\\inv2ax.dc6"));
}

static void test_null_inputs(void)
{
   TEST_ASSERT_FALSE(glob_match(NULL, "anything"));
   TEST_ASSERT_FALSE(glob_match("anything", NULL));
   TEST_ASSERT_FALSE(glob_match(NULL, NULL));
}

static void test_empty_pattern_against_non_empty(void)
{
   TEST_ASSERT_FALSE(glob_match("", "foo"));
}

static void test_empty_pattern_against_empty(void)
{
   TEST_ASSERT_TRUE(glob_match("", ""));
}

static void test_full_d2_path_examples(void)
{
   /* Real-world preset patterns from the planning doc. */
   TEST_ASSERT_TRUE(glob_match("data\\global\\items\\inv*.dc6",
                               "data\\global\\items\\inv2ax.dc6"));
   TEST_ASSERT_TRUE(glob_match("data\\global\\items\\inv*.dc6",
                               "data\\global\\items\\INVGLD.dc6"));
   TEST_ASSERT_FALSE(glob_match("data\\global\\items\\inv*.dc6",
                                "data\\global\\items\\flpaar.dc6"));

   TEST_ASSERT_TRUE(glob_match("data\\global\\items\\*.dc6",
                               "data\\global\\items\\anything.dc6"));
   TEST_ASSERT_FALSE(glob_match("data\\global\\items\\*.dc6",
                                "data\\global\\items\\sub\\anything.dc6"));

   TEST_ASSERT_TRUE(glob_match("data\\global\\tiles\\ACT1\\**\\*.dt1",
                               "data\\global\\tiles\\ACT1\\BARRACKS\\barr.dt1"));
   TEST_ASSERT_TRUE(glob_match("data\\global\\tiles\\**\\*.dt1",
                               "data\\global\\tiles\\ACT5\\baal\\baal.dt1"));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_exact_match);
   RUN_TEST(test_case_insensitive);
   RUN_TEST(test_star_within_segment);
   RUN_TEST(test_star_does_not_cross_separator);
   RUN_TEST(test_question_mark);
   RUN_TEST(test_question_mark_does_not_cross_separator);
   RUN_TEST(test_double_star_recursive);
   RUN_TEST(test_double_star_matches_zero_segments);
   RUN_TEST(test_forward_slash_normalization);
   RUN_TEST(test_double_star_only_as_full_segment);
   RUN_TEST(test_null_inputs);
   RUN_TEST(test_empty_pattern_against_non_empty);
   RUN_TEST(test_empty_pattern_against_empty);
   RUN_TEST(test_full_d2_path_examples);
   return UNITY_END();
}
