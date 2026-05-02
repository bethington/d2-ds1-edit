#include <string.h>

#include "unity/unity.h"

#include "core/compose_index.h"

void setUp(void) {}
void tearDown(void) {}

/* Tiny synthetic MonStats.txt: header row + 4 data rows.
 * Two real monsters (FA + AN), one NPC (CN), one row with no Code
 * (should be silently ignored). */
static const char *MONSTATS_FIXTURE =
   "Id\tCode\tnpc\tNameStr\n"
   "FallenWarrior\tFA\t0\tmonster_fa\n"
   "Andariel\tAN\t0\tmonster_an\n"
   "Cain\tCN\t1\tnpc_cain\n"
   "PlaceholderRow\t\t0\t\n";

/* Synthetic Objects.txt: header row + 3 data rows. */
static const char *OBJECTS_FIXTURE =
   "Name\tToken\tDescription\n"
   "Town Portal\tTownPortal\twarp_pad\n"
   "Wooden Chest\tWoodChest\tloot_container\n"
   "Permanent Stone\tStone\tdecoration\n";

static void test_parse_monstats_separates_monsters_and_npcs(void)
{
   COMPOSE_TOKEN_S monsters[16];
   COMPOSE_TOKEN_S npcs[16];
   int mc = 0, nc = 0;

   TEST_ASSERT_EQUAL_INT(1, compose_index_parse_monstats(MONSTATS_FIXTURE,
      monsters, 16, &mc, npcs, 16, &nc));
   TEST_ASSERT_EQUAL_INT(2, mc);
   TEST_ASSERT_EQUAL_INT(1, nc);
}

static void test_parse_monstats_records_code_and_name(void)
{
   COMPOSE_TOKEN_S monsters[16];
   COMPOSE_TOKEN_S npcs[16];
   int mc = 0, nc = 0;

   compose_index_parse_monstats(MONSTATS_FIXTURE,
      monsters, 16, &mc, npcs, 16, &nc);

   TEST_ASSERT_EQUAL_STRING("FA", monsters[0].code);
   TEST_ASSERT_EQUAL_STRING("FallenWarrior", monsters[0].name);

   TEST_ASSERT_EQUAL_STRING("AN", monsters[1].code);
   TEST_ASSERT_EQUAL_STRING("Andariel", monsters[1].name);

   TEST_ASSERT_EQUAL_STRING("CN", npcs[0].code);
   TEST_ASSERT_EQUAL_STRING("Cain", npcs[0].name);
}

static void test_parse_monstats_ignores_blank_code(void)
{
   COMPOSE_TOKEN_S monsters[16];
   COMPOSE_TOKEN_S npcs[16];
   int mc = 0, nc = 0;
   int i;
   int found_blank = 0;

   compose_index_parse_monstats(MONSTATS_FIXTURE,
      monsters, 16, &mc, npcs, 16, &nc);

   for (i = 0; i < mc; i++)
      if (monsters[i].code[0] == 0)
         found_blank = 1;
   TEST_ASSERT_EQUAL_INT(0, found_blank);
}

static void test_parse_monstats_handles_empty_buffer(void)
{
   COMPOSE_TOKEN_S monsters[16];
   COMPOSE_TOKEN_S npcs[16];
   int mc = 999, nc = 999;

   TEST_ASSERT_EQUAL_INT(0, compose_index_parse_monstats("",
      monsters, 16, &mc, npcs, 16, &nc));
   TEST_ASSERT_EQUAL_INT(0, mc);
   TEST_ASSERT_EQUAL_INT(0, nc);
}

static void test_parse_monstats_rejects_missing_required_columns(void)
{
   /* No Code column. */
   const char *bad = "Id\tnpc\nFallen\t0\n";
   COMPOSE_TOKEN_S monsters[16];
   COMPOSE_TOKEN_S npcs[16];
   int mc = 0, nc = 0;

   TEST_ASSERT_EQUAL_INT(0, compose_index_parse_monstats(bad,
      monsters, 16, &mc, npcs, 16, &nc));
}

static void test_parse_monstats_respects_caps(void)
{
   COMPOSE_TOKEN_S monsters[1];  /* only room for 1 monster */
   COMPOSE_TOKEN_S npcs[16];
   int mc = 0, nc = 0;

   compose_index_parse_monstats(MONSTATS_FIXTURE,
      monsters, 1, &mc, npcs, 16, &nc);
   TEST_ASSERT_EQUAL_INT(1, mc);  /* second monster dropped */
   TEST_ASSERT_EQUAL_INT(1, nc);  /* NPC fits */
}

static void test_parse_objects_emits_all_rows(void)
{
   COMPOSE_TOKEN_S out[16];
   int n = 0;

   TEST_ASSERT_EQUAL_INT(1, compose_index_parse_objects(OBJECTS_FIXTURE,
      out, 16, &n));
   TEST_ASSERT_EQUAL_INT(3, n);
}

static void test_parse_objects_records_token_and_name(void)
{
   COMPOSE_TOKEN_S out[16];
   int n = 0;

   compose_index_parse_objects(OBJECTS_FIXTURE, out, 16, &n);

   TEST_ASSERT_EQUAL_STRING("TownPortal", out[0].code);
   TEST_ASSERT_EQUAL_STRING("Town Portal", out[0].name);

   TEST_ASSERT_EQUAL_STRING("WoodChest", out[1].code);
   TEST_ASSERT_EQUAL_STRING("Wooden Chest", out[1].name);

   TEST_ASSERT_EQUAL_STRING("Stone", out[2].code);
   TEST_ASSERT_EQUAL_STRING("Permanent Stone", out[2].name);
}

static void test_parse_objects_rejects_missing_token_column(void)
{
   const char *bad = "Name\tDescription\nTownPortal\twarp\n";
   COMPOSE_TOKEN_S out[16];
   int n = 0;

   TEST_ASSERT_EQUAL_INT(0, compose_index_parse_objects(bad, out, 16, &n));
}

static void test_parse_objects_falls_back_to_token_when_name_blank(void)
{
   const char *fixture =
      "Token\tName\n"
      "Foo\t\n"
      "Bar\tBee\n";
   COMPOSE_TOKEN_S out[8];
   int n = 0;

   compose_index_parse_objects(fixture, out, 8, &n);
   TEST_ASSERT_EQUAL_INT(2, n);
   TEST_ASSERT_EQUAL_STRING("Foo", out[0].code);
   TEST_ASSERT_EQUAL_STRING("Foo", out[0].name);
   TEST_ASSERT_EQUAL_STRING("Bar", out[1].code);
   TEST_ASSERT_EQUAL_STRING("Bee", out[1].name);
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_parse_monstats_separates_monsters_and_npcs);
   RUN_TEST(test_parse_monstats_records_code_and_name);
   RUN_TEST(test_parse_monstats_ignores_blank_code);
   RUN_TEST(test_parse_monstats_handles_empty_buffer);
   RUN_TEST(test_parse_monstats_rejects_missing_required_columns);
   RUN_TEST(test_parse_monstats_respects_caps);
   RUN_TEST(test_parse_objects_emits_all_rows);
   RUN_TEST(test_parse_objects_records_token_and_name);
   RUN_TEST(test_parse_objects_rejects_missing_token_column);
   RUN_TEST(test_parse_objects_falls_back_to_token_when_name_blank);
   return UNITY_END();
}
