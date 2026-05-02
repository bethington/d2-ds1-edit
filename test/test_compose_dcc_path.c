#include <string.h>

#include "unity/unity.h"

#include "core/compose_dcc_path.h"

void setUp(void) {}
void tearDown(void) {}

static void test_layer_codes(void)
{
   TEST_ASSERT_EQUAL_STRING("HD", compose_dcc_path_layer_code(0));
   TEST_ASSERT_EQUAL_STRING("TR", compose_dcc_path_layer_code(1));
   TEST_ASSERT_EQUAL_STRING("LG", compose_dcc_path_layer_code(2));
   TEST_ASSERT_EQUAL_STRING("RA", compose_dcc_path_layer_code(3));
   TEST_ASSERT_EQUAL_STRING("LA", compose_dcc_path_layer_code(4));
   TEST_ASSERT_EQUAL_STRING("RH", compose_dcc_path_layer_code(5));
   TEST_ASSERT_EQUAL_STRING("LH", compose_dcc_path_layer_code(6));
   TEST_ASSERT_EQUAL_STRING("SH", compose_dcc_path_layer_code(7));
   TEST_ASSERT_EQUAL_STRING("S1", compose_dcc_path_layer_code(8));
   TEST_ASSERT_EQUAL_STRING("S8", compose_dcc_path_layer_code(15));
}

static void test_layer_codes_out_of_range(void)
{
   TEST_ASSERT_NULL(compose_dcc_path_layer_code(-1));
   TEST_ASSERT_NULL(compose_dcc_path_layer_code(16));
   TEST_ASSERT_NULL(compose_dcc_path_layer_code(99));
}

static void test_build_necromancer_head_idle_hth(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(1, compose_dcc_path_build(buf, sizeof(buf),
      "data\\global\\chars", "NE", 0, "LIT", "NU", "HTH"));
   TEST_ASSERT_EQUAL_STRING(
      "data\\global\\chars\\NE\\HD\\NEHDLITNUHTH.dcc", buf);
}

static void test_build_sorceress_torso_walk_staff(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(1, compose_dcc_path_build(buf, sizeof(buf),
      "data\\global\\chars", "SO", 1, "MED", "WL", "STF"));
   TEST_ASSERT_EQUAL_STRING(
      "data\\global\\chars\\SO\\TR\\SOTRMEDWLSTF.dcc", buf);
}

static void test_build_paladin_shield_attack_1hs(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(1, compose_dcc_path_build(buf, sizeof(buf),
      "data\\global\\chars", "PA", 7, "HVY", "A1", "1HS"));
   TEST_ASSERT_EQUAL_STRING(
      "data\\global\\chars\\PA\\SH\\PASHHVYA11HS.dcc", buf);
}

static void test_build_monster_with_no_skin(void)
{
   char buf[256];
   /* Monsters typically have no skin variant; pass empty string. */
   TEST_ASSERT_EQUAL_INT(1, compose_dcc_path_build(buf, sizeof(buf),
      "data\\global\\monsters", "AN", 0, "", "NU", ""));
   TEST_ASSERT_EQUAL_STRING(
      "data\\global\\monsters\\AN\\HD\\ANHDNU.dcc", buf);
}

static void test_build_null_skin_treated_as_empty(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(1, compose_dcc_path_build(buf, sizeof(buf),
      "data\\global\\monsters", "DI", 0, NULL, "WL", NULL));
   TEST_ASSERT_EQUAL_STRING(
      "data\\global\\monsters\\DI\\HD\\DIHDWL.dcc", buf);
}

static void test_build_skill_layer(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(1, compose_dcc_path_build(buf, sizeof(buf),
      "data\\global\\chars", "NE", 8, "LIT", "S1", "HTH"));
   TEST_ASSERT_EQUAL_STRING(
      "data\\global\\chars\\NE\\S1\\NES1LITS1HTH.dcc", buf);
}

static void test_build_rejects_bad_args(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(0, compose_dcc_path_build(NULL, sizeof(buf),
      "data\\global\\chars", "NE", 0, "LIT", "NU", "HTH"));
   TEST_ASSERT_EQUAL_INT(0, compose_dcc_path_build(buf, 0,
      "data\\global\\chars", "NE", 0, "LIT", "NU", "HTH"));
   TEST_ASSERT_EQUAL_INT(0, compose_dcc_path_build(buf, sizeof(buf),
      NULL, "NE", 0, "LIT", "NU", "HTH"));
   TEST_ASSERT_EQUAL_INT(0, compose_dcc_path_build(buf, sizeof(buf),
      "data\\global\\chars", NULL, 0, "LIT", "NU", "HTH"));
   TEST_ASSERT_EQUAL_INT(0, compose_dcc_path_build(buf, sizeof(buf),
      "data\\global\\chars", "NE", 0, "LIT", NULL, "HTH"));
   TEST_ASSERT_EQUAL_INT(0, compose_dcc_path_build(buf, sizeof(buf),
      "data\\global\\chars", "NE", -1, "LIT", "NU", "HTH"));
   TEST_ASSERT_EQUAL_INT(0, compose_dcc_path_build(buf, sizeof(buf),
      "data\\global\\chars", "NE", 16, "LIT", "NU", "HTH"));
}

static void test_build_truncation(void)
{
   char buf[10];
   /* Buffer way too small; expect failure with empty out. */
   TEST_ASSERT_EQUAL_INT(0, compose_dcc_path_build(buf, sizeof(buf),
      "data\\global\\chars", "NE", 0, "LIT", "NU", "HTH"));
   TEST_ASSERT_EQUAL_STRING("", buf);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_layer_codes);
   RUN_TEST(test_layer_codes_out_of_range);
   RUN_TEST(test_build_necromancer_head_idle_hth);
   RUN_TEST(test_build_sorceress_torso_walk_staff);
   RUN_TEST(test_build_paladin_shield_attack_1hs);
   RUN_TEST(test_build_monster_with_no_skin);
   RUN_TEST(test_build_null_skin_treated_as_empty);
   RUN_TEST(test_build_skill_layer);
   RUN_TEST(test_build_rejects_bad_args);
   RUN_TEST(test_build_truncation);
   return UNITY_END();
}
