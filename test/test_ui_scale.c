/* How the UI scale is chosen: the setting when there is one, otherwise the
 * window height, to the nearest quarter and within the supported range. */

#include "unity/unity.h"
#include "ui/ui_scale.h"

void setUp(void) {}
void tearDown(void) {}

void test_auto_is_full_size_at_the_base_height(void)
{
   TEST_ASSERT_EQUAL_FLOAT(1.0f, ui_scale_choose(0, 1080));
}

void test_auto_doubles_on_a_4k_screen(void)
{
   TEST_ASSERT_EQUAL_FLOAT(2.0f, ui_scale_choose(0, 2160));
}

void test_auto_never_shrinks_on_a_small_screen(void)
{
   TEST_ASSERT_EQUAL_FLOAT(1.0f, ui_scale_choose(0, 720));
}

void test_auto_rounds_to_a_quarter(void)
{
   TEST_ASSERT_EQUAL_FLOAT(1.25f, ui_scale_choose(0, 1440));
}

void test_auto_stops_at_the_maximum(void)
{
   TEST_ASSERT_EQUAL_FLOAT(3.0f, ui_scale_choose(0, 8640));
}

void test_the_setting_wins_over_the_screen(void)
{
   TEST_ASSERT_EQUAL_FLOAT(1.5f, ui_scale_choose(150, 2160));
}

void test_the_setting_is_kept_in_range(void)
{
   TEST_ASSERT_EQUAL_FLOAT(1.0f, ui_scale_choose(50, 2160));
   TEST_ASSERT_EQUAL_FLOAT(3.0f, ui_scale_choose(900, 2160));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_auto_is_full_size_at_the_base_height);
   RUN_TEST(test_auto_doubles_on_a_4k_screen);
   RUN_TEST(test_auto_never_shrinks_on_a_small_screen);
   RUN_TEST(test_auto_rounds_to_a_quarter);
   RUN_TEST(test_auto_stops_at_the_maximum);
   RUN_TEST(test_the_setting_wins_over_the_screen);
   RUN_TEST(test_the_setting_is_kept_in_range);
   return UNITY_END();
}
