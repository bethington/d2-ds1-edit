#include <string.h>

#include "unity/unity.h"

#include "core/compose_cof_path.h"

void setUp(void) {}
void tearDown(void) {}

static void test_build_player_char_cof(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(1, compose_cof_path_build(buf, sizeof(buf),
      "data\\global\\chars", "NE", "WL", "HTH"));
   TEST_ASSERT_EQUAL_STRING(
      "data\\global\\chars\\NE\\COF\\NEWLHTH.cof", buf);
}

static void test_build_sorceress_idle_staff(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(1, compose_cof_path_build(buf, sizeof(buf),
      "data\\global\\chars", "SO", "NU", "STF"));
   TEST_ASSERT_EQUAL_STRING(
      "data\\global\\chars\\SO\\COF\\SONUSTF.cof", buf);
}

static void test_build_monster_with_wclass(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(1, compose_cof_path_build(buf, sizeof(buf),
      "data\\global\\monsters", "AN", "NU", "HTH"));
   TEST_ASSERT_EQUAL_STRING(
      "data\\global\\monsters\\AN\\COF\\ANNUHTH.cof", buf);
}

static void test_build_with_empty_wclass(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(1, compose_cof_path_build(buf, sizeof(buf),
      "data\\global\\monsters", "DI", "NU", ""));
   TEST_ASSERT_EQUAL_STRING(
      "data\\global\\monsters\\DI\\COF\\DINU.cof", buf);
}

static void test_build_with_null_wclass(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(1, compose_cof_path_build(buf, sizeof(buf),
      "data\\global\\monsters", "BL", "WL", NULL));
   TEST_ASSERT_EQUAL_STRING(
      "data\\global\\monsters\\BL\\COF\\BLWL.cof", buf);
}

static void test_build_skill_mode(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(1, compose_cof_path_build(buf, sizeof(buf),
      "data\\global\\chars", "DZ", "S1", "STF"));
   TEST_ASSERT_EQUAL_STRING(
      "data\\global\\chars\\DZ\\COF\\DZS1STF.cof", buf);
}

static void test_build_rejects_bad_args(void)
{
   char buf[256];
   TEST_ASSERT_EQUAL_INT(0, compose_cof_path_build(NULL, sizeof(buf),
      "data\\global\\chars", "NE", "WL", "HTH"));
   TEST_ASSERT_EQUAL_INT(0, compose_cof_path_build(buf, 0,
      "data\\global\\chars", "NE", "WL", "HTH"));
   TEST_ASSERT_EQUAL_INT(0, compose_cof_path_build(buf, sizeof(buf),
      NULL, "NE", "WL", "HTH"));
   TEST_ASSERT_EQUAL_INT(0, compose_cof_path_build(buf, sizeof(buf),
      "data\\global\\chars", NULL, "WL", "HTH"));
   TEST_ASSERT_EQUAL_INT(0, compose_cof_path_build(buf, sizeof(buf),
      "data\\global\\chars", "NE", NULL, "HTH"));
}

static void test_build_truncation(void)
{
   char buf[10];
   TEST_ASSERT_EQUAL_INT(0, compose_cof_path_build(buf, sizeof(buf),
      "data\\global\\chars", "NE", "WL", "HTH"));
   TEST_ASSERT_EQUAL_STRING("", buf);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_build_player_char_cof);
   RUN_TEST(test_build_sorceress_idle_staff);
   RUN_TEST(test_build_monster_with_wclass);
   RUN_TEST(test_build_with_empty_wclass);
   RUN_TEST(test_build_with_null_wclass);
   RUN_TEST(test_build_skill_mode);
   RUN_TEST(test_build_rejects_bad_args);
   RUN_TEST(test_build_truncation);
   return UNITY_END();
}
