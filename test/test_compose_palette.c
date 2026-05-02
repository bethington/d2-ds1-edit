#include "unity/unity.h"

#include "core/compose_palette.h"

void setUp(void) {}
void tearDown(void) {}

/* v1 tests: every category returns Act 1. The follow-up v2 will
 * extend monsters/objects with TXT-driven resolution; new tests
 * can land alongside that. */

static void test_player_char_returns_act_1(void)
{
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_resolve_act(COMPOSE_CATEGORY_PLAYER_CHAR, "NE"));
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_resolve_act(COMPOSE_CATEGORY_PLAYER_CHAR, "SO"));
}

static void test_npc_returns_act_1(void)
{
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_resolve_act(COMPOSE_CATEGORY_NPC, "Cain"));
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_resolve_act(COMPOSE_CATEGORY_NPC, "Charsi"));
}

static void test_monster_returns_act_1_v1(void)
{
   /* v2 will return the monster's natural act from monstats.txt. */
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_resolve_act(COMPOSE_CATEGORY_MONSTER, "AN"));
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_resolve_act(COMPOSE_CATEGORY_MONSTER, "DI"));
}

static void test_object_returns_act_1_v1(void)
{
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_resolve_act(COMPOSE_CATEGORY_OBJECT, "TownPortal"));
}

static void test_null_token_safe(void)
{
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_resolve_act(COMPOSE_CATEGORY_PLAYER_CHAR, NULL));
}

static void test_unknown_category_returns_act_1(void)
{
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_resolve_act(COMPOSE_CATEGORY_NONE, "x"));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_player_char_returns_act_1);
   RUN_TEST(test_npc_returns_act_1);
   RUN_TEST(test_monster_returns_act_1_v1);
   RUN_TEST(test_object_returns_act_1_v1);
   RUN_TEST(test_null_token_safe);
   RUN_TEST(test_unknown_category_returns_act_1);
   return UNITY_END();
}
