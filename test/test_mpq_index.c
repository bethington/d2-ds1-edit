/*
 * Unit tests for mpq_index's reverse-lookup logic.
 *
 * The table-build path needs real MPQs and so is exercised only in the
 * integration smoke run. The basename-match scan is pure and easy to test
 * via the exposed mpq_index_find_by_ds1_name_in() helper.
 */

#include <stdio.h>
#include <string.h>

#include "unity/unity.h"

#include "core/mpq_index.h"

void setUp(void) {}
void tearDown(void) {}


// Populate a preset entry with the given ds1 path in slot 0.
static void seed(PRESET_ENTRY_S *e, int def, const char *name, const char *ds1)
{
   memset(e, 0, sizeof(*e));
   e->def = def;
   strncpy(e->name, name, MPQ_INDEX_NAME_LEN - 1);
   strncpy(e->ds1_files[0], ds1, MPQ_INDEX_PATH_LEN - 1);
   e->ds1_count = 1;
}


void test_find_by_basename_exact(void)
{
   PRESET_ENTRY_S arr[3];
   const PRESET_ENTRY_S *hit;
   int slot = -99;

   seed(&arr[0], 10, "Act 1 - Cave Entrance",
        "data\\global\\tiles\\ACT1\\CAVES\\denent.ds1");
   seed(&arr[1], 11, "Act 1 - Bivouac",
        "data\\global\\tiles\\ACT1\\WILD\\tree1.ds1");
   seed(&arr[2], 12, "Act 2 - Town",
        "data\\global\\tiles\\ACT2\\TOWN\\lutn1.ds1");

   hit = mpq_index_find_by_ds1_name_in(arr, 3, "denent.ds1", &slot);
   TEST_ASSERT_NOT_NULL(hit);
   TEST_ASSERT_EQUAL_INT(10, hit->def);
   TEST_ASSERT_EQUAL_INT(0, slot);
}


void test_find_by_basename_case_insensitive(void)
{
   PRESET_ENTRY_S arr[1];
   const PRESET_ENTRY_S *hit;

   seed(&arr[0], 42, "Test",
        "data\\global\\tiles\\ACT1\\CAVES\\DENENT.DS1");

   hit = mpq_index_find_by_ds1_name_in(arr, 1, "denent.ds1", NULL);
   TEST_ASSERT_NOT_NULL(hit);
   TEST_ASSERT_EQUAL_INT(42, hit->def);

   hit = mpq_index_find_by_ds1_name_in(arr, 1, "DeNeNt.Ds1", NULL);
   TEST_ASSERT_NOT_NULL(hit);
   TEST_ASSERT_EQUAL_INT(42, hit->def);
}


void test_find_handles_forward_slashes(void)
{
   PRESET_ENTRY_S arr[1];
   const PRESET_ENTRY_S *hit;

   seed(&arr[0], 7, "Forward slashes",
        "data/global/tiles/act5/baal/baalthrone.ds1");

   hit = mpq_index_find_by_ds1_name_in(arr, 1, "baalthrone.ds1", NULL);
   TEST_ASSERT_NOT_NULL(hit);
   TEST_ASSERT_EQUAL_INT(7, hit->def);
}


void test_find_scans_all_file_slots(void)
{
   PRESET_ENTRY_S arr[1];
   const PRESET_ENTRY_S *hit;
   int slot = -1;

   memset(&arr[0], 0, sizeof(arr[0]));
   arr[0].def = 99;
   strncpy(arr[0].ds1_files[0], "a\\first.ds1",  MPQ_INDEX_PATH_LEN - 1);
   strncpy(arr[0].ds1_files[1], "a\\second.ds1", MPQ_INDEX_PATH_LEN - 1);
   strncpy(arr[0].ds1_files[2], "a\\third.ds1",  MPQ_INDEX_PATH_LEN - 1);
   arr[0].ds1_count = 3;

   hit = mpq_index_find_by_ds1_name_in(arr, 1, "third.ds1", &slot);
   TEST_ASSERT_NOT_NULL(hit);
   TEST_ASSERT_EQUAL_INT(99, hit->def);
   TEST_ASSERT_EQUAL_INT(2, slot);
}


void test_find_misses_return_null(void)
{
   PRESET_ENTRY_S arr[1];

   seed(&arr[0], 1, "Single", "data\\global\\tiles\\x\\y.ds1");

   TEST_ASSERT_NULL(mpq_index_find_by_ds1_name_in(arr, 1, "nope.ds1", NULL));
   TEST_ASSERT_NULL(mpq_index_find_by_ds1_name_in(arr, 1, NULL,       NULL));
   TEST_ASSERT_NULL(mpq_index_find_by_ds1_name_in(NULL, 0, "y.ds1",   NULL));
}


// Ensure the scan doesn't drop past the declared ds1_count even if later
// slots happen to contain garbage bytes.
void test_find_respects_ds1_count(void)
{
   PRESET_ENTRY_S arr[1];

   memset(&arr[0], 0, sizeof(arr[0]));
   arr[0].def = 55;
   strncpy(arr[0].ds1_files[0], "a\\used.ds1",   MPQ_INDEX_PATH_LEN - 1);
   // Slot 1 never populated; ds1_count stays at 1.
   strncpy(arr[0].ds1_files[1], "a\\unused.ds1", MPQ_INDEX_PATH_LEN - 1);
   arr[0].ds1_count = 1;

   TEST_ASSERT_NOT_NULL(mpq_index_find_by_ds1_name_in(arr, 1, "used.ds1",   NULL));
   TEST_ASSERT_NULL    (mpq_index_find_by_ds1_name_in(arr, 1, "unused.ds1", NULL));
}


int main(void)
{
   UNITY_BEGIN();

   RUN_TEST(test_find_by_basename_exact);
   RUN_TEST(test_find_by_basename_case_insensitive);
   RUN_TEST(test_find_handles_forward_slashes);
   RUN_TEST(test_find_scans_all_file_slots);
   RUN_TEST(test_find_misses_return_null);
   RUN_TEST(test_find_respects_ds1_count);

   return UNITY_END();
}
