/* AnimData.d2 loading and lookup through the editor's real code
 * (animdata_load, animdata_hash_value, animdata_get_cof_info_quiet,
 * animdata_get_frame_delay_ms). Only the archive fetch is replaced: it serves
 * the authored test/equivalence/fixtures/animdata-authored.bin, written by
 * scripts/equivalence/visual_values.py from the documented layout (256
 * buckets of a count plus 160-byte records, bucket = uppercase byte sum). */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "core/animdata.h"
#include "visual_test_support.h"

#define ANIMDATA_PATH "Data\\Global\\AnimData.d2"

static unsigned char *s_fixture;
static size_t s_fixture_len;

static void load_bytes(const void *bytes, size_t len)
{
   visual_fetch_reset();
   visual_fetch_add(ANIMDATA_PATH, bytes, len);
   animdata_load();
}

void setUp(void)
{
   if (s_fixture == NULL)
      s_fixture = visual_read_file(DS1EDIT_VISUAL_FIXTURES "/animdata-authored.bin",
                                   &s_fixture_len);
   TEST_ASSERT_NOT_NULL_MESSAGE(s_fixture, "animdata-authored.bin");
   load_bytes(s_fixture, s_fixture_len);
}

void tearDown(void) {}

static void assert_record(const char *name, long fpd, long speed)
{
   long f = -1, s = -1;
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, animdata_get_cof_info_quiet(name, &f, &s), name);
   TEST_ASSERT_EQUAL_INT32_MESSAGE(fpd, f, name);
   TEST_ASSERT_EQUAL_INT32_MESSAGE(speed, s, name);
}

static void assert_miss(const char *name)
{
   long f = 7, s = 7;
   TEST_ASSERT_EQUAL_INT_MESSAGE(-1, animdata_get_cof_info_quiet(name, &f, &s), name);
   TEST_ASSERT_EQUAL_INT32(0, f);
   TEST_ASSERT_EQUAL_INT32(0, s);
}

/* ---- hash ----------------------------------------------------------------- */

void test_hash_is_uppercase_byte_sum_mod_256(void)
{
   /* animdata-d2.md: A+M+T+N+H+T+H = 532, 532 mod 256 = 20 */
   TEST_ASSERT_EQUAL_UINT8(20, animdata_hash_value("AMTNHTH"));
   TEST_ASSERT_EQUAL_UINT8(20, animdata_hash_value("amtnhth"));
   TEST_ASSERT_EQUAL_UINT8(20, animdata_hash_value("AmTnHtH"));
   TEST_ASSERT_EQUAL_UINT8(59, animdata_hash_value("NUZZHTH"));
   TEST_ASSERT_EQUAL_UINT8(0, animdata_hash_value(""));
}

/* ---- traversal and lookup ---------------------------------------------------- */

void test_buckets_are_traversed_with_160_byte_records(void)
{
   /* bucket 20 holds four colliding names; the last one is three records
    * (480 bytes) past the first */
   TEST_ASSERT_EQUAL_INT(4, glb_ds1edit.anim_data.nb_records[20]);
   TEST_ASSERT_EQUAL_INT(1, glb_ds1edit.anim_data.nb_records[59]);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1edit.anim_data.nb_records[0]);
   TEST_ASSERT_NULL(glb_ds1edit.anim_data.block_pointer[0]);
   assert_record("AMTNHTH", 16, 80);
   assert_record("AMNTHTH", 8, 256);
   assert_record("TNMAHTH", 12, 128);
   assert_record("OBOPHTH", 4, 512);
   assert_record("NUZZHTH", 20, 0);
}

void test_queries_are_case_insensitive(void)
{
   assert_record("amtnhth", 16, 80);
   assert_record("TnMaHtH", 12, 128);
}

void test_misses_and_null_arguments(void)
{
   long f = 3;

   assert_miss("HTHAMTN");      /* same bucket as AMTNHTH, not stored */
   assert_miss("AMTNHT");       /* empty bucket */
   assert_miss("ZZZZZZZ");
   TEST_ASSERT_EQUAL_INT(-1, animdata_get_cof_info_quiet(NULL, &f, &f));
   TEST_ASSERT_EQUAL_INT32(0, f);
   TEST_ASSERT_EQUAL_INT(-1, animdata_get_cof_info_quiet("AMTNHTH", NULL, &f));
   TEST_ASSERT_EQUAL_INT32(0, f);
}

void test_cof_extension_is_ignored_as_promised(void)
{
   /* animdata.c: "The .cof extension can be in the name, but it won't be
    * used". The hash already ignores it; the compare must too. */
   assert_record("AMTNHTH.cof", 16, 80);
   assert_record("tnmahth.COF", 12, 128);
}

/* ---- editor timing policy (not a game fixed-point claim) ---------------------- */

void test_frame_delay_policy(void)
{
   TEST_ASSERT_EQUAL_INT(128, animdata_get_frame_delay_ms("AMTNHTH", 40)); /* 10240/80 */
   TEST_ASSERT_EQUAL_INT(40, animdata_get_frame_delay_ms("AMNTHTH", 99));  /* 10240/256 */
   TEST_ASSERT_EQUAL_INT(20, animdata_get_frame_delay_ms("OBOPHTH", 99));
   TEST_ASSERT_EQUAL_INT(77, animdata_get_frame_delay_ms("NUZZHTH", 77));  /* speed 0 */
   TEST_ASSERT_EQUAL_INT(55, animdata_get_frame_delay_ms("HTHAMTN", 55));  /* miss */
   TEST_ASSERT_EQUAL_INT(55, animdata_get_frame_delay_ms(NULL, 55));
}

/* ---- malformed files ------------------------------------------------------------ */

static unsigned char *empty_buckets(size_t *len)
{
   unsigned char *b = (unsigned char *) calloc(256 * 4 + 160 * 4, 1);
   *len = 256 * 4;
   return b;
}

void test_bucket_index_is_the_position_not_the_first_name(void)
{
   /* The game indexes buckets by position (one pointer per bucket as it
    * walks them). A record filed in bucket 5 whose name hashes to 20 is
    * unreachable there; it must not be found, and must not hide bucket 20. */
   size_t len;
   unsigned char *b = empty_buckets(&len);
   unsigned char *out = (unsigned char *) calloc(len + 160, 1);
   int32_t one = 1;

   memcpy(out, b, 5 * 4);
   memcpy(out + 5 * 4, &one, 4);
   memcpy(out + 6 * 4, "AMTNHTH", 7);
   *(int32_t *) (out + 6 * 4 + 8)  = 16;
   *(int32_t *) (out + 6 * 4 + 12) = 80;
   memcpy(out + 6 * 4 + 160, b + 6 * 4, len - 6 * 4);
   load_bytes(out, len + 160);

   TEST_ASSERT_EQUAL_INT(1, glb_ds1edit.anim_data.nb_records[5]);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1edit.anim_data.nb_records[20]);
   assert_miss("AMTNHTH");
   free(out);
   free(b);
}

void test_a_bucket_running_past_the_file_is_not_used(void)
{
   /* bucket 0 claims 3 records but the file ends inside the first */
   unsigned char buf[4 + 100];
   int32_t three = 3;

   memset(buf, 0, sizeof(buf));
   memcpy(buf, &three, 4);
   memcpy(buf + 4, "AMTNHTH", 7);
   load_bytes(buf, sizeof(buf));
   TEST_ASSERT_NULL(glb_ds1edit.anim_data.block_pointer[0]);
   TEST_ASSERT_NULL(glb_ds1edit.anim_data.block_pointer[20]);
   assert_miss("AMTNHTH");
}

void test_bytes_after_256_buckets_are_ignored(void)
{
   size_t len = s_fixture_len + 4 + 160;
   unsigned char *buf = (unsigned char *) calloc(len, 1);
   int32_t one = 1;

   memcpy(buf, s_fixture, s_fixture_len);
   memcpy(buf + s_fixture_len, &one, 4);
   memcpy(buf + s_fixture_len + 4, "ZZZZZZZ", 7);   /* would be bucket 118 */
   load_bytes(buf, len);
   assert_miss("ZZZZZZZ");
   assert_record("AMTNHTH", 16, 80);
   free(buf);
}

/* One record, filed in bucket `bucket`, whose 8-byte name field is `name8`. */
static unsigned char *one_record_file(int bucket, const char name8[8], size_t *len)
{
   unsigned char *out;
   int32_t one = 1;
   int b, pos = 0;

   *len = 256 * 4 + 160;
   out = (unsigned char *) calloc(*len, 1);
   for (b = 0; b < 256; b++)
   {
      if (b == bucket)
      {
         memcpy(out + pos, &one, 4);
         memcpy(out + pos + 4, name8, 8);
         *(int32_t *) (out + pos + 4 + 8)  = 16;
         *(int32_t *) (out + pos + 4 + 12) = 80;
         pos += 4 + 160;
      }
      else
         pos += 4;
   }
   return out;
}

void test_stored_names_are_compared_exactly_after_uppercasing_the_query(void)
{
   /* ANIMDATA_NAME_COMPARE_CASE, refuted for the old case-insensitive
    * compare: D2Common 1.13c 6fd91dd0 uppercases the query, and 6fd91d00
    * compares the stored 8-byte field as two dwords. A lower-case stored
    * name can therefore never be found, whatever the query's case. */
   size_t len;
   unsigned char *buf = one_record_file(20, "amtnhth\0", &len);

   load_bytes(buf, len);
   TEST_ASSERT_EQUAL_INT(1, glb_ds1edit.anim_data.nb_records[20]);
   assert_miss("AMTNHTH");
   assert_miss("amtnhth");
   free(buf);

   /* the same record stored upper-case is found from any query case */
   buf = one_record_file(20, "AMTNHTH\0", &len);
   load_bytes(buf, len);
   assert_record("amtnhth", 16, 80);
   free(buf);
}

void test_bytes_after_the_stored_name_terminator_take_part(void)
{
   /* the dword compare covers all 8 bytes, so a stored field that is not
    * zero-padded after its terminator does not match the padded query */
   size_t len;
   unsigned char *buf = one_record_file(20, "AMTNHTH", &len);

   buf[4 * 20 + 4 + 7] = 'X';   /* "AMTNHTH" followed by 'X' instead of NUL */
   load_bytes(buf, len);
   assert_miss("AMTNHTH");
   free(buf);
}

void test_missing_file_leaves_an_empty_table(void)
{
   visual_fetch_reset();
   animdata_load();
   assert_miss("AMTNHTH");
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_hash_is_uppercase_byte_sum_mod_256);
   RUN_TEST(test_buckets_are_traversed_with_160_byte_records);
   RUN_TEST(test_queries_are_case_insensitive);
   RUN_TEST(test_misses_and_null_arguments);
   RUN_TEST(test_cof_extension_is_ignored_as_promised);
   RUN_TEST(test_frame_delay_policy);
   RUN_TEST(test_bucket_index_is_the_position_not_the_first_name);
   RUN_TEST(test_a_bucket_running_past_the_file_is_not_used);
   RUN_TEST(test_bytes_after_256_buckets_are_ignored);
   RUN_TEST(test_stored_names_are_compared_exactly_after_uppercasing_the_query);
   RUN_TEST(test_bytes_after_the_stored_name_terminator_take_part);
   RUN_TEST(test_missing_file_leaves_an_empty_table);
   free(s_fixture);
   return UNITY_END();
}
