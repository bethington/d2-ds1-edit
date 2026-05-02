#include <string.h>

#include "unity/unity.h"

#include "core/compose_naming.h"

void setUp(void) {}
void tearDown(void) {}

static void test_class_name_known_codes(void)
{
   TEST_ASSERT_EQUAL_STRING("Amazon",      compose_naming_class_name("AM"));
   TEST_ASSERT_EQUAL_STRING("Assassin",    compose_naming_class_name("AI"));
   TEST_ASSERT_EQUAL_STRING("Barbarian",   compose_naming_class_name("BA"));
   TEST_ASSERT_EQUAL_STRING("Druid",       compose_naming_class_name("DZ"));
   TEST_ASSERT_EQUAL_STRING("Necromancer", compose_naming_class_name("NE"));
   TEST_ASSERT_EQUAL_STRING("Paladin",     compose_naming_class_name("PA"));
   TEST_ASSERT_EQUAL_STRING("Sorceress",   compose_naming_class_name("SO"));
}

static void test_class_name_case_insensitive(void)
{
   TEST_ASSERT_EQUAL_STRING("Necromancer", compose_naming_class_name("ne"));
   TEST_ASSERT_EQUAL_STRING("Sorceress",   compose_naming_class_name("So"));
}

static void test_class_name_unknown_returns_null(void)
{
   TEST_ASSERT_NULL(compose_naming_class_name("ZZ"));
   TEST_ASSERT_NULL(compose_naming_class_name(""));
   TEST_ASSERT_NULL(compose_naming_class_name(NULL));
   TEST_ASSERT_NULL(compose_naming_class_name("Necromancer"));
}

static void test_sanitize_passes_valid_chars(void)
{
   char buf[64];
   TEST_ASSERT_EQUAL_INT(1, compose_naming_sanitize("Necromancer", buf, sizeof(buf)));
   TEST_ASSERT_EQUAL_STRING("Necromancer", buf);

   TEST_ASSERT_EQUAL_INT(1, compose_naming_sanitize("HellSpawn_42", buf, sizeof(buf)));
   TEST_ASSERT_EQUAL_STRING("HellSpawn_42", buf);
}

static void test_sanitize_replaces_spaces(void)
{
   char buf[64];
   TEST_ASSERT_EQUAL_INT(1, compose_naming_sanitize("Skeleton Archer", buf, sizeof(buf)));
   TEST_ASSERT_EQUAL_STRING("Skeleton_Archer", buf);
}

static void test_sanitize_collapses_runs(void)
{
   char buf[64];
   TEST_ASSERT_EQUAL_INT(1, compose_naming_sanitize("foo  bar    baz", buf, sizeof(buf)));
   TEST_ASSERT_EQUAL_STRING("foo_bar_baz", buf);

   TEST_ASSERT_EQUAL_INT(1, compose_naming_sanitize("a!@#b", buf, sizeof(buf)));
   TEST_ASSERT_EQUAL_STRING("a_b", buf);
}

static void test_sanitize_strips_leading_trailing_underscores(void)
{
   char buf[64];
   TEST_ASSERT_EQUAL_INT(1, compose_naming_sanitize("  hello  ", buf, sizeof(buf)));
   TEST_ASSERT_EQUAL_STRING("hello", buf);

   TEST_ASSERT_EQUAL_INT(1, compose_naming_sanitize("___hello___", buf, sizeof(buf)));
   TEST_ASSERT_EQUAL_STRING("hello", buf);
}

static void test_sanitize_empty_input_yields_underscore(void)
{
   char buf[64];
   TEST_ASSERT_EQUAL_INT(1, compose_naming_sanitize("", buf, sizeof(buf)));
   TEST_ASSERT_EQUAL_STRING("_", buf);

   TEST_ASSERT_EQUAL_INT(1, compose_naming_sanitize("!@#$", buf, sizeof(buf)));
   TEST_ASSERT_EQUAL_STRING("_", buf);
}

static void test_sanitize_null_input_yields_underscore(void)
{
   char buf[64];
   TEST_ASSERT_EQUAL_INT(1, compose_naming_sanitize(NULL, buf, sizeof(buf)));
   TEST_ASSERT_EQUAL_STRING("_", buf);
}

static void test_sanitize_truncates_to_fit(void)
{
   char buf[8];
   TEST_ASSERT_EQUAL_INT(1, compose_naming_sanitize("abcdefghijk", buf, sizeof(buf)));
   /* Result must fit in 7 chars + NUL. */
   TEST_ASSERT_TRUE(strlen(buf) < sizeof(buf));
}

static void test_sanitize_rejects_bad_args(void)
{
   char buf[8];
   TEST_ASSERT_EQUAL_INT(0, compose_naming_sanitize("hi", NULL, sizeof(buf)));
   TEST_ASSERT_EQUAL_INT(0, compose_naming_sanitize("hi", buf, 0));
   TEST_ASSERT_EQUAL_INT(0, compose_naming_sanitize("hi", buf, 1));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_class_name_known_codes);
   RUN_TEST(test_class_name_case_insensitive);
   RUN_TEST(test_class_name_unknown_returns_null);
   RUN_TEST(test_sanitize_passes_valid_chars);
   RUN_TEST(test_sanitize_replaces_spaces);
   RUN_TEST(test_sanitize_collapses_runs);
   RUN_TEST(test_sanitize_strips_leading_trailing_underscores);
   RUN_TEST(test_sanitize_empty_input_yields_underscore);
   RUN_TEST(test_sanitize_null_input_yields_underscore);
   RUN_TEST(test_sanitize_truncates_to_fit);
   RUN_TEST(test_sanitize_rejects_bad_args);
   return UNITY_END();
}
