#include "unity/unity.h"

#include "core/animdata.h"

void setUp(void) {}
void tearDown(void) {}

/* Tests target the pure converter, animdata_speed_to_frame_delay_ms.
 * The COF lookup wrapper depends on glb_ds1edit.anim_data being
 * loaded, which is integration-tested at runtime against real assets;
 * unit-test coverage focuses on the math. */

static void test_default_speed_is_40ms(void)
{
   /* speed = 256 -> 256 frames per 256 ticks -> 1 frame per tick
    * = 40 ms. This is D2's "normal" rate. */
   TEST_ASSERT_EQUAL_INT(40, animdata_speed_to_frame_delay_ms(256, 40));
}

static void test_double_speed_halves_delay(void)
{
   TEST_ASSERT_EQUAL_INT(20, animdata_speed_to_frame_delay_ms(512, 40));
}

static void test_half_speed_doubles_delay(void)
{
   TEST_ASSERT_EQUAL_INT(80, animdata_speed_to_frame_delay_ms(128, 40));
}

static void test_quarter_speed_quadruples_delay(void)
{
   TEST_ASSERT_EQUAL_INT(160, animdata_speed_to_frame_delay_ms(64, 40));
}

static void test_zero_speed_returns_default(void)
{
   TEST_ASSERT_EQUAL_INT(40, animdata_speed_to_frame_delay_ms(0, 40));
   TEST_ASSERT_EQUAL_INT(100, animdata_speed_to_frame_delay_ms(0, 100));
}

static void test_negative_speed_returns_default(void)
{
   TEST_ASSERT_EQUAL_INT(40, animdata_speed_to_frame_delay_ms(-1, 40));
   TEST_ASSERT_EQUAL_INT(40, animdata_speed_to_frame_delay_ms(-1000, 40));
}

static void test_absurd_speed_clamps(void)
{
   TEST_ASSERT_EQUAL_INT(40, animdata_speed_to_frame_delay_ms(70000, 40));
}

static void test_extremely_slow_clamps_to_max(void)
{
   /* speed = 1 would give 10240 ms, which is clamped to 10000 ms. */
   TEST_ASSERT_EQUAL_INT(10000, animdata_speed_to_frame_delay_ms(1, 40));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_default_speed_is_40ms);
   RUN_TEST(test_double_speed_halves_delay);
   RUN_TEST(test_half_speed_doubles_delay);
   RUN_TEST(test_quarter_speed_quadruples_delay);
   RUN_TEST(test_zero_speed_returns_default);
   RUN_TEST(test_negative_speed_returns_default);
   RUN_TEST(test_absurd_speed_clamps);
   RUN_TEST(test_extremely_slow_clamps_to_max);
   return UNITY_END();
}
