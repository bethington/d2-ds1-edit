/*
 * Placeholder test to verify the Unity test framework and CMake test
 * infrastructure are working correctly. This will be replaced with
 * real tests in Phase 1 (palette) and Phase 2 (decoders).
 */

#include "unity/unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_sanity(void)
{
    TEST_ASSERT_EQUAL_INT(1, 1);
}

void test_true_is_true(void)
{
    TEST_ASSERT_TRUE(1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sanity);
    RUN_TEST(test_true_is_true);
    return UNITY_END();
}
