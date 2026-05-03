#include "unity/unity.h"

#include "core/compose_palette.h"
#include "core/compose_palette_index.h"

/* compose_palette_index.c references misc_load_mpq_file via the
 * compose_palette_index_build entry point. We don't call build in
 * tests (we use the parser directly with synthetic input), but the
 * linker still wants the symbol. Stub it. */
int misc_load_mpq_file(char *filename, char **buffer, long *buf_len, int output)
{
   (void) filename; (void) buffer; (void) buf_len; (void) output;
   return -1;
}

void setUp(void) { compose_palette_index_reset(); }
void tearDown(void) {}

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

/* ---- v2 tests: Levels.txt parser ---- */

static int s_emit_count;
static struct { char id[32]; int act; } s_emits[16];

static void capturing_emit(const char *id, int act, void *ud)
{
   (void) ud;
   if (s_emit_count < 16)
   {
      strncpy(s_emits[s_emit_count].id, id, 31);
      s_emits[s_emit_count].id[31] = 0;
      s_emits[s_emit_count].act = act;
      s_emit_count++;
   }
}

static const char LEVELS_FIXTURE[] =
   /* Header includes Act + mon1 + mon2 + nmon1 + umon1 (bare minimum). */
   "Name\tId\tPal\tAct\tmon1\tmon2\tmon3\tmon4\tmon5\tmon6\tmon7"
       "\tmon8\tmon9\tmon10\tnmon1\tnmon2\tnmon3\tnmon4\tnmon5\tnmon6"
       "\tnmon7\tnmon8\tnmon9\tnmon10\tumon1\tumon2\n"
   /* Catacombs Level 4 = Act 0 (= Act 1) with zombie3 + bighead1. */
   "Catacombs 4\t37\t0\t0\tzombie3\tbighead1\t\t\t\t\t\t\t\t"
       "\tzombie3\tbighead1\t\t\t\t\t\t\t\t\tandariel\t\n"
   /* Some Act 3 level with mephisto. */
   "Travincal\t99\t2\t2\thierophant1\thierophant2\t\t\t\t\t\t\t\t"
       "\thierophant1\thierophant2\t\t\t\t\t\t\t\t\tmephisto\t\n"
   /* Sentinel row -- non-numeric Act, should be skipped. */
   "Expansion\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\n";

static void test_parse_levels_basic_emits(void)
{
   s_emit_count = 0;
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_index_parse_levels(LEVELS_FIXTURE,
                                          capturing_emit, NULL));
   /* Catacombs 4 emits 3 Ids: zombie3 (mon1), bighead1 (mon2),
    * andariel (umon1). Their nmon mirrors don't add new ids; first-
    * wins is the caller's job. Plus Travincal: hierophant1, hierophant2,
    * mephisto. = 6 unique emits over the two real rows. The capturing
    * emit doesn't dedupe so total is 9. */
   TEST_ASSERT_TRUE(s_emit_count >= 6);
}

static void test_empty_index_returns_act_1(void)
{
   /* When build hasn't been called (or it failed), act_for_monster_id
    * is the safe default of Act 1 for everything. The compose-mode
    * callers rely on this so an unbuilt index doesn't break compose
    * runs that don't care about the per-act palette. */
   compose_palette_index_reset();
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_index_act_for_monster_id("andariel"));
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_index_act_for_monster_id(""));
   TEST_ASSERT_EQUAL_INT(1,
      compose_palette_index_act_for_monster_id(NULL));
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
   RUN_TEST(test_parse_levels_basic_emits);
   RUN_TEST(test_empty_index_returns_act_1);
   return UNITY_END();
}
