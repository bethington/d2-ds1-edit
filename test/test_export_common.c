#include "unity/unity.h"

#include "core/export_common.h"

void setUp(void) {}
void tearDown(void) {}

void test_screenshot_name_uses_png_extension(void)
{
    char out[64];

    export_make_screenshot_name(out, sizeof(out), 37);
    TEST_ASSERT_EQUAL_STRING("screenshot-00037.png", out);
}

void test_screenshot_name_zero_pads_number(void)
{
    char out[64];

    export_make_screenshot_name(out, sizeof(out), 1);
    TEST_ASSERT_EQUAL_STRING("screenshot-00001.png", out);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_screenshot_name_uses_png_extension);
    RUN_TEST(test_screenshot_name_zero_pads_number);
    return UNITY_END();
}