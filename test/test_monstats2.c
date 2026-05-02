#include <string.h>

#include "unity/unity.h"

#include "core/monstats2.h"

void setUp(void) {}
void tearDown(void) {}

/* Synthetic MonStats2.txt-like header + a row that mirrors what real
 * skeleton1 has. Tests that the parser pulls out:
 *   - Id  = "skeleton1"
 *   - BaseW = "1hs"
 *   - HD layer used + skin "lit"
 *   - LH layer not used
 *   - S2 layer used + skin "lit"
 */
static const char skeleton_fixture[] =
   "Id\tBaseW\tHD\tHDv\tTR\tTRv\tLG\tLGv\tRA\tRav\tLA\tLav"
     "\tRH\tRHv\tLH\tLHv\tSH\tSHv"
     "\tS1\tS1v\tS2\tS2v\tS3\tS3v\tS4\tS4v"
     "\tS5\tS5v\tS6\tS6v\tS7\tS7v\tS8\tS8v\n"
   "skeleton1\t1hs"
     "\t1\t\"lit,med,hvy\""
     "\t1\t\"lit,med,hvy\""
     "\t1\t\"lit,med,hvy\""
     "\t1\t\"lit,med,hvy\""
     "\t1\t\"lit,med,hvy\""
     "\t1\t\"axe,fla\""
     "\t\t"
     "\t1\t\"nil,buc,lrg\""
     "\t1\t\"lit,med,hvy\""
     "\t1\t\"lit,med,hvy\""
     "\t\t\t\t"
     "\t\t\t\t"
     "\t\t\t\t\n"
   /* Real D2 sentinel row -- some bookkeeping value with no real Id. */
   "*Expansion\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
     "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\n";

static void test_parse_skeleton1_basic(void)
{
   MONSTATS2_ENTRY_S out[8];
   int count = 0;
   int rc = monstats2_parse(skeleton_fixture, out, 8, &count);
   TEST_ASSERT_EQUAL_INT(1, rc);
   TEST_ASSERT_EQUAL_INT(1, count);  /* Expansion row skipped. */
   TEST_ASSERT_EQUAL_STRING("skeleton1", out[0].id);
   TEST_ASSERT_EQUAL_STRING("1hs",       out[0].basew);
}

static void test_parse_layers_and_skins(void)
{
   MONSTATS2_ENTRY_S out[8];
   int count = 0;
   monstats2_parse(skeleton_fixture, out, 8, &count);
   /* HD: used, first variant "lit". */
   TEST_ASSERT_EQUAL_INT(1, out[0].layers[0].used);
   TEST_ASSERT_EQUAL_STRING("lit", out[0].layers[0].skin);
   /* TR: used. */
   TEST_ASSERT_EQUAL_INT(1, out[0].layers[1].used);
   /* LH (index 6): not used. */
   TEST_ASSERT_EQUAL_INT(0, out[0].layers[6].used);
   /* SH (index 7): used, first variant "nil". */
   TEST_ASSERT_EQUAL_INT(1, out[0].layers[7].used);
   TEST_ASSERT_EQUAL_STRING("nil", out[0].layers[7].skin);
   /* S1, S2 (indices 8, 9): used. */
   TEST_ASSERT_EQUAL_INT(1, out[0].layers[8].used);
   TEST_ASSERT_EQUAL_INT(1, out[0].layers[9].used);
   /* S3-S8 (10..15): not used. */
   TEST_ASSERT_EQUAL_INT(0, out[0].layers[10].used);
   TEST_ASSERT_EQUAL_INT(0, out[0].layers[15].used);
}

static void test_parse_handles_empty_basew(void)
{
   /* Andariel-shaped fixture: BaseW empty, only HD layer used. */
   const char *fixture =
      "Id\tBaseW\tHD\tHDv\tTR\tTRv\n"
      "andariel\t\t1\t\t\t\n";
   MONSTATS2_ENTRY_S out[2];
   int count = 0;
   int rc = monstats2_parse(fixture, out, 2, &count);
   TEST_ASSERT_EQUAL_INT(1, rc);
   TEST_ASSERT_EQUAL_INT(1, count);
   TEST_ASSERT_EQUAL_STRING("andariel", out[0].id);
   TEST_ASSERT_EQUAL_STRING("",         out[0].basew);
   TEST_ASSERT_EQUAL_INT(1, out[0].layers[0].used);
   TEST_ASSERT_EQUAL_INT(0, out[0].layers[1].used);
}

static void test_parse_rejects_missing_id_column(void)
{
   const char *fixture =
      "BaseW\tHD\n"
      "1hs\t1\n";
   MONSTATS2_ENTRY_S out[2];
   int count = 99;
   int rc = monstats2_parse(fixture, out, 2, &count);
   TEST_ASSERT_EQUAL_INT(0, rc);
}

static void test_parse_null_inputs_safe(void)
{
   MONSTATS2_ENTRY_S out[2];
   int count = 99;
   TEST_ASSERT_EQUAL_INT(0, monstats2_parse(NULL, out, 2, &count));
   TEST_ASSERT_EQUAL_INT(0, monstats2_parse(skeleton_fixture, out, 2, NULL));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_parse_skeleton1_basic);
   RUN_TEST(test_parse_layers_and_skins);
   RUN_TEST(test_parse_handles_empty_basew);
   RUN_TEST(test_parse_rejects_missing_id_column);
   RUN_TEST(test_parse_null_inputs_safe);
   return UNITY_END();
}
