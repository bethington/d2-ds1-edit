#include <string.h>

#include "unity/unity.h"

#include "core/compose_presets.h"

void setUp(void)
{
   compose_mode_presets_reset();
   compose_weapon_presets_reset();
}

void tearDown(void)
{
   compose_mode_presets_reset();
   compose_weapon_presets_reset();
}

static void test_parse_single_code(void)
{
   COMPOSE_PRESET_S p;
   TEST_ASSERT_EQUAL_INT(1, compose_preset_parse("idle_only", "NU", &p));
   TEST_ASSERT_EQUAL_STRING("idle_only", p.name);
   TEST_ASSERT_EQUAL_INT(1, p.code_count);
   TEST_ASSERT_EQUAL_STRING("NU", p.codes[0]);
}

static void test_parse_multi_code(void)
{
   COMPOSE_PRESET_S p;
   TEST_ASSERT_EQUAL_INT(1, compose_preset_parse("standard_combat",
      "NU, WL, A1, DT", &p));
   TEST_ASSERT_EQUAL_INT(4, p.code_count);
   TEST_ASSERT_EQUAL_STRING("NU", p.codes[0]);
   TEST_ASSERT_EQUAL_STRING("WL", p.codes[1]);
   TEST_ASSERT_EQUAL_STRING("A1", p.codes[2]);
   TEST_ASSERT_EQUAL_STRING("DT", p.codes[3]);
}

static void test_parse_uppercases_codes(void)
{
   COMPOSE_PRESET_S p;
   TEST_ASSERT_EQUAL_INT(1, compose_preset_parse("mixed", "nu, Wl, a1", &p));
   TEST_ASSERT_EQUAL_STRING("NU", p.codes[0]);
   TEST_ASSERT_EQUAL_STRING("WL", p.codes[1]);
   TEST_ASSERT_EQUAL_STRING("A1", p.codes[2]);
}

static void test_parse_strips_extra_whitespace(void)
{
   COMPOSE_PRESET_S p;
   TEST_ASSERT_EQUAL_INT(1, compose_preset_parse("ws",
      "  NU  ,  WL  ,  A1  ", &p));
   TEST_ASSERT_EQUAL_STRING("NU", p.codes[0]);
   TEST_ASSERT_EQUAL_STRING("WL", p.codes[1]);
   TEST_ASSERT_EQUAL_STRING("A1", p.codes[2]);
}

static void test_parse_handles_trailing_comma(void)
{
   COMPOSE_PRESET_S p;
   TEST_ASSERT_EQUAL_INT(1, compose_preset_parse("trailing", "NU, WL,", &p));
   TEST_ASSERT_EQUAL_INT(2, p.code_count);
}

static void test_parse_rejects_empty_value(void)
{
   COMPOSE_PRESET_S p;
   TEST_ASSERT_EQUAL_INT(0, compose_preset_parse("empty", "", &p));
   TEST_ASSERT_EQUAL_INT(0, compose_preset_parse("empty", "   ", &p));
   TEST_ASSERT_EQUAL_INT(0, compose_preset_parse("empty", " , , ", &p));
}

static void test_parse_rejects_empty_name(void)
{
   COMPOSE_PRESET_S p;
   TEST_ASSERT_EQUAL_INT(0, compose_preset_parse("", "NU", &p));
}

static void test_parse_rejects_null_inputs(void)
{
   COMPOSE_PRESET_S p;
   TEST_ASSERT_EQUAL_INT(0, compose_preset_parse(NULL, "NU", &p));
   TEST_ASSERT_EQUAL_INT(0, compose_preset_parse("a", NULL, &p));
   TEST_ASSERT_EQUAL_INT(0, compose_preset_parse("a", "NU", NULL));
}

static void test_mode_registry_add_count_at(void)
{
   COMPOSE_PRESET_S a, b;
   compose_preset_parse("idle_only", "NU", &a);
   compose_preset_parse("idle_walk", "NU, WL", &b);

   TEST_ASSERT_EQUAL_INT(0, compose_mode_presets_count());
   TEST_ASSERT_EQUAL_INT(1, compose_mode_presets_add(&a));
   TEST_ASSERT_EQUAL_INT(1, compose_mode_presets_add(&b));
   TEST_ASSERT_EQUAL_INT(2, compose_mode_presets_count());

   TEST_ASSERT_EQUAL_STRING("idle_only", compose_mode_presets_at(0)->name);
   TEST_ASSERT_EQUAL_STRING("idle_walk", compose_mode_presets_at(1)->name);
   TEST_ASSERT_NULL(compose_mode_presets_at(2));
}

static void test_weapon_registry_separate_from_mode(void)
{
   COMPOSE_PRESET_S m, w;
   compose_preset_parse("idle_only",   "NU",  &m);
   compose_preset_parse("bare_hands",  "HTH", &w);

   compose_mode_presets_add(&m);
   compose_weapon_presets_add(&w);

   TEST_ASSERT_EQUAL_INT(1, compose_mode_presets_count());
   TEST_ASSERT_EQUAL_INT(1, compose_weapon_presets_count());
   TEST_ASSERT_EQUAL_STRING("idle_only",  compose_mode_presets_at(0)->name);
   TEST_ASSERT_EQUAL_STRING("bare_hands", compose_weapon_presets_at(0)->name);
}

static void test_reset_clears(void)
{
   COMPOSE_PRESET_S p;
   compose_preset_parse("x", "NU", &p);
   compose_mode_presets_add(&p);
   compose_weapon_presets_add(&p);
   TEST_ASSERT_EQUAL_INT(1, compose_mode_presets_count());
   TEST_ASSERT_EQUAL_INT(1, compose_weapon_presets_count());

   compose_mode_presets_reset();
   compose_weapon_presets_reset();
   TEST_ASSERT_EQUAL_INT(0, compose_mode_presets_count());
   TEST_ASSERT_EQUAL_INT(0, compose_weapon_presets_count());
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_parse_single_code);
   RUN_TEST(test_parse_multi_code);
   RUN_TEST(test_parse_uppercases_codes);
   RUN_TEST(test_parse_strips_extra_whitespace);
   RUN_TEST(test_parse_handles_trailing_comma);
   RUN_TEST(test_parse_rejects_empty_value);
   RUN_TEST(test_parse_rejects_empty_name);
   RUN_TEST(test_parse_rejects_null_inputs);
   RUN_TEST(test_mode_registry_add_count_at);
   RUN_TEST(test_weapon_registry_separate_from_mode);
   RUN_TEST(test_reset_clears);
   return UNITY_END();
}
