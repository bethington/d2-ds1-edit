#include "unity/unity.h"

#include "core/area_metadata.h"


void setUp(void)
{
}


void tearDown(void)
{
}


void test_area_name_parse_act_reads_standard_prefix(void)
{
   TEST_ASSERT_EQUAL_INT(3, area_name_parse_act("Act 3 - Jungle"));
   TEST_ASSERT_EQUAL_INT(5, area_name_parse_act("Act 5 - Town"));
}


void test_area_name_parse_act_rejects_nonstandard_names(void)
{
   TEST_ASSERT_EQUAL_INT(0, area_name_parse_act("Imperial Palace"));
   TEST_ASSERT_EQUAL_INT(0, area_name_parse_act("Act X - Broken"));
   TEST_ASSERT_EQUAL_INT(0, area_name_parse_act(NULL));
}


void test_area_name_has_act_mismatch_detects_inconsistent_lvltypes_rows(void)
{
   TEST_ASSERT_EQUAL_INT(1,
      area_name_has_act_mismatch(5, "Act 1 - Cathedral"));
   TEST_ASSERT_EQUAL_INT(1,
      area_name_has_act_mismatch(3, "Act 1 - Town"));
}


void test_area_name_has_act_mismatch_ignores_missing_or_matching_acts(void)
{
   TEST_ASSERT_EQUAL_INT(0,
      area_name_has_act_mismatch(3, "Act 3 - Jungle"));
   TEST_ASSERT_EQUAL_INT(0,
      area_name_has_act_mismatch(0, "Act 3 - Jungle"));
   TEST_ASSERT_EQUAL_INT(0,
      area_name_has_act_mismatch(5, "Imperial Palace"));
}


void test_area_group_resolve_act_prefers_lvltypes_column_over_name(void)
{
   TEST_ASSERT_EQUAL_INT(5, area_group_resolve_act(5, "Act 1 - Cathedral"));
   TEST_ASSERT_EQUAL_INT(3, area_group_resolve_act(3, "Act 1 - Town"));
}


void test_area_group_resolve_act_falls_back_to_name_when_txt_missing(void)
{
   TEST_ASSERT_EQUAL_INT(4, area_group_resolve_act(0, "Act 4 - Town"));
   TEST_ASSERT_EQUAL_INT(0, area_group_resolve_act(0, "Imperial Palace"));
}


void test_area_name_strip_prefix_preserves_nonstandard_names(void)
{
   TEST_ASSERT_EQUAL_STRING("Jungle", area_name_strip_prefix("Act 3 - Jungle"));
   TEST_ASSERT_EQUAL_STRING("Imperial Palace", area_name_strip_prefix("Imperial Palace"));
}


int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_area_name_parse_act_reads_standard_prefix);
   RUN_TEST(test_area_name_parse_act_rejects_nonstandard_names);
   RUN_TEST(test_area_name_has_act_mismatch_detects_inconsistent_lvltypes_rows);
   RUN_TEST(test_area_name_has_act_mismatch_ignores_missing_or_matching_acts);
   RUN_TEST(test_area_group_resolve_act_prefers_lvltypes_column_over_name);
   RUN_TEST(test_area_group_resolve_act_falls_back_to_name_when_txt_missing);
   RUN_TEST(test_area_name_strip_prefix_preserves_nonstandard_names);
   return UNITY_END();
}