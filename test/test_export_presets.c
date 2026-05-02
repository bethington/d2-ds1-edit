#include <string.h>

#include "unity/unity.h"

#include "core/export_presets.h"

void setUp(void)
{
   export_presets_reset();
}

void tearDown(void)
{
   export_presets_reset();
}

static void test_parse_simple(void)
{
   EXPORT_PRESET_S p;
   TEST_ASSERT_TRUE(export_preset_parse("inv", "dc6 | data\\global\\items\\inv*.dc6", &p));
   TEST_ASSERT_EQUAL_STRING("inv", p.name);
   TEST_ASSERT_EQUAL_STRING("dc6", p.type);
   TEST_ASSERT_EQUAL_STRING("data\\global\\items\\inv*.dc6", p.pattern);
}

static void test_parse_lowercases_type(void)
{
   EXPORT_PRESET_S p;
   TEST_ASSERT_TRUE(export_preset_parse("inv", "DC6 | foo", &p));
   TEST_ASSERT_EQUAL_STRING("dc6", p.type);
}

static void test_parse_trims_whitespace(void)
{
   EXPORT_PRESET_S p;
   TEST_ASSERT_TRUE(export_preset_parse("  inv  ",
                                        "  dc6  |  pattern  ", &p));
   TEST_ASSERT_EQUAL_STRING("inv", p.name);
   TEST_ASSERT_EQUAL_STRING("dc6", p.type);
   TEST_ASSERT_EQUAL_STRING("pattern", p.pattern);
}

static void test_parse_accepts_all_valid_types(void)
{
   EXPORT_PRESET_S p;
   TEST_ASSERT_TRUE(export_preset_parse("a", "all | x", &p));
   TEST_ASSERT_TRUE(export_preset_parse("b", "dt1 | x", &p));
   TEST_ASSERT_TRUE(export_preset_parse("c", "dc6 | x", &p));
   TEST_ASSERT_TRUE(export_preset_parse("d", "dcc | x", &p));
}

static void test_parse_rejects_invalid_type(void)
{
   EXPORT_PRESET_S p;
   TEST_ASSERT_FALSE(export_preset_parse("inv", "png | foo", &p));
   TEST_ASSERT_FALSE(export_preset_parse("inv", "bmp | foo", &p));
   TEST_ASSERT_FALSE(export_preset_parse("inv", " | foo", &p));
}

static void test_parse_rejects_missing_pipe(void)
{
   EXPORT_PRESET_S p;
   TEST_ASSERT_FALSE(export_preset_parse("inv", "dc6 foo", &p));
   TEST_ASSERT_FALSE(export_preset_parse("inv", "dc6", &p));
}

static void test_parse_rejects_empty_name(void)
{
   EXPORT_PRESET_S p;
   TEST_ASSERT_FALSE(export_preset_parse("", "dc6 | foo", &p));
}

static void test_parse_rejects_empty_pattern(void)
{
   EXPORT_PRESET_S p;
   TEST_ASSERT_FALSE(export_preset_parse("inv", "dc6 |", &p));
   TEST_ASSERT_FALSE(export_preset_parse("inv", "dc6 |   ", &p));
}

static void test_parse_rejects_null_inputs(void)
{
   EXPORT_PRESET_S p;
   TEST_ASSERT_FALSE(export_preset_parse(NULL, "dc6 | foo", &p));
   TEST_ASSERT_FALSE(export_preset_parse("inv", NULL, &p));
   TEST_ASSERT_FALSE(export_preset_parse("inv", "dc6 | foo", NULL));
}

static void test_add_count_at(void)
{
   EXPORT_PRESET_S p1, p2;
   TEST_ASSERT_EQUAL_INT(0, export_presets_count());

   export_preset_parse("first", "dc6 | a", &p1);
   export_preset_parse("second", "dt1 | b", &p2);

   TEST_ASSERT_EQUAL_INT(1, export_presets_add(&p1));
   TEST_ASSERT_EQUAL_INT(1, export_presets_add(&p2));
   TEST_ASSERT_EQUAL_INT(2, export_presets_count());

   TEST_ASSERT_EQUAL_STRING("first", export_presets_at(0)->name);
   TEST_ASSERT_EQUAL_STRING("second", export_presets_at(1)->name);
   TEST_ASSERT_NULL(export_presets_at(2));
   TEST_ASSERT_NULL(export_presets_at(-1));
}

static void test_add_preserves_ini_order(void)
{
   EXPORT_PRESET_S p;
   const char *names[] = { "z_last", "a_first", "m_middle" };
   int i;
   for (i = 0; i < 3; i++)
   {
      export_preset_parse(names[i], "dc6 | x", &p);
      export_presets_add(&p);
   }
   TEST_ASSERT_EQUAL_STRING("z_last", export_presets_at(0)->name);
   TEST_ASSERT_EQUAL_STRING("a_first", export_presets_at(1)->name);
   TEST_ASSERT_EQUAL_STRING("m_middle", export_presets_at(2)->name);
}

static void test_add_rejects_malformed(void)
{
   EXPORT_PRESET_S empty;
   memset(&empty, 0, sizeof(empty));
   TEST_ASSERT_EQUAL_INT(0, export_presets_add(&empty));
   TEST_ASSERT_EQUAL_INT(0, export_presets_add(NULL));
   TEST_ASSERT_EQUAL_INT(0, export_presets_count());
}

static void test_reset_clears(void)
{
   EXPORT_PRESET_S p;
   export_preset_parse("x", "dc6 | y", &p);
   export_presets_add(&p);
   TEST_ASSERT_EQUAL_INT(1, export_presets_count());

   export_presets_reset();
   TEST_ASSERT_EQUAL_INT(0, export_presets_count());
   TEST_ASSERT_NULL(export_presets_at(0));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_parse_simple);
   RUN_TEST(test_parse_lowercases_type);
   RUN_TEST(test_parse_trims_whitespace);
   RUN_TEST(test_parse_accepts_all_valid_types);
   RUN_TEST(test_parse_rejects_invalid_type);
   RUN_TEST(test_parse_rejects_missing_pipe);
   RUN_TEST(test_parse_rejects_empty_name);
   RUN_TEST(test_parse_rejects_empty_pattern);
   RUN_TEST(test_parse_rejects_null_inputs);
   RUN_TEST(test_add_count_at);
   RUN_TEST(test_add_preserves_ini_order);
   RUN_TEST(test_add_rejects_malformed);
   RUN_TEST(test_reset_clears);
   return UNITY_END();
}
