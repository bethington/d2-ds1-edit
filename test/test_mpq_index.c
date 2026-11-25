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

void test_full_path_chooses_matching_preset_over_same_basename(void)
{
   PRESET_ENTRY_S arr[2];
   const PRESET_ENTRY_S *hit;

   seed(&arr[0], 836, "Act 4 - Lava", "Act4/Lava/LavaX.ds1");
   seed(&arr[1], 900, "Act 5 - Lava", "ACT4\\Expansion\\lavaX.ds1");

   hit = mpq_index_find_by_ds1_path_in(arr, 2,
      "data/global/tiles/act4/expansion/LAVAX.DS1", NULL);
   TEST_ASSERT_NOT_NULL(hit);
   TEST_ASSERT_EQUAL_INT(900, hit->def);
   hit = mpq_index_find_by_ds1_path_in(arr, 2,
      "D:\\mods\\data\\Global\\Tiles\\ACT4\\Expansion\\lavaX.ds1", NULL);
   TEST_ASSERT_NOT_NULL(hit);
   TEST_ASSERT_EQUAL_INT(900, hit->def);
   TEST_ASSERT_NULL(mpq_index_find_by_ds1_path_in(arr, 2,
      "Act1/Other/LavaX.ds1", NULL));
}


void test_full_path_of_a_project_copy_finds_its_preset(void)
{
   /* Copy-on-save writes <project>/Global/Tiles/<suffix> (no "Data" level);
    * reopening that copy must resolve the same preset as the original. */
   PRESET_ENTRY_S arr[2];
   const PRESET_ENTRY_S *hit;

   seed(&arr[0], 836, "Act 4 - Lava", "Act4/Lava/LavaX.ds1");
   seed(&arr[1], 900, "Act 5 - Lava", "ACT4\\Expansion\\lavaX.ds1");

   hit = mpq_index_find_by_ds1_path_in(arr, 2,
      "C:\\mods\\my proj\\Global\\Tiles\\ACT4\\Expansion\\lavaX.ds1", NULL);
   TEST_ASSERT_NOT_NULL(hit);
   TEST_ASSERT_EQUAL_INT(900, hit->def);
   hit = mpq_index_find_by_ds1_path_in(arr, 2, "/home/u/proj/global/tiles/act4/lava/lavax.ds1", NULL);
   TEST_ASSERT_NOT_NULL(hit);
   TEST_ASSERT_EQUAL_INT(836, hit->def);
}

void test_full_path_never_falls_back_to_a_namesake(void)
{
   PRESET_ENTRY_S arr[1];
   int slot = -1;

   seed(&arr[0], 10, "Act 1 - Cave Entrance", "data\\global\\tiles\\ACT1\\CAVES\\denent.ds1");
   TEST_ASSERT_NULL(mpq_index_find_by_ds1_path_in(arr, 1,
      "C:\\mods\\proj\\Global\\Tiles\\ACT2\\CAVES\\denent.ds1", &slot));
   TEST_ASSERT_EQUAL_INT(-1, slot);
   TEST_ASSERT_NULL(mpq_index_find_by_ds1_path_in(arr, 1, "denent.ds1", NULL));
   TEST_ASSERT_NULL(mpq_index_find_by_ds1_path_in(arr, 1, "", NULL));
}


void test_row_names_ds1_never_crosses_folders_for_a_full_path(void)
{
   TEST_ASSERT_TRUE(mpq_index_row_names_ds1("Act4/Lava/LavaE.ds1", "ACT4\\lava\\lavae.ds1"));
   TEST_ASSERT_TRUE(mpq_index_row_names_ds1("Act4/Lava/LavaE.ds1", "assets/tiles/Act4\\Lava\\LavaE.ds1"));
   TEST_ASSERT_FALSE(mpq_index_row_names_ds1("Act4/Lava/LavaE.ds1", "expansion\\lavaE.ds1"));
   TEST_ASSERT_FALSE(mpq_index_row_names_ds1("Act4/Lava/LavaX.ds1", "assets/tiles/expansion\\lavaX.ds1"));
}


void test_row_names_ds1_matches_a_bare_filename_in_any_folder(void)
{
   TEST_ASSERT_TRUE(mpq_index_row_names_ds1("Act4/Lava/LavaE.ds1", "lavae.ds1"));
   TEST_ASSERT_FALSE(mpq_index_row_names_ds1("Act4/Lava/LavaE.ds1", "lavaW.ds1"));
   TEST_ASSERT_FALSE(mpq_index_row_names_ds1("0", "lavaE.ds1"));
   TEST_ASSERT_FALSE(mpq_index_row_names_ds1(NULL, "lavaE.ds1"));
   TEST_ASSERT_FALSE(mpq_index_row_names_ds1("Act4/Lava/LavaE.ds1", NULL));
}


void test_sub_level_type_matches_a_whole_path_only(void)
{
   SUB_ENTRY_S subs[2];

   memset(subs, 0, sizeof(subs));
   strcpy(subs[0].file, "Act1/Outdoors/Waypoint.ds1");
   subs[0].level_type = 2;
   strcpy(subs[1].file, "Expansion/Siege/Landmark1.ds1");
   subs[1].level_type = 31;

   TEST_ASSERT_EQUAL_INT(2, mpq_index_sub_level_type_in(subs, 2, "ACT1\\OUTDOORS\\waypoint.ds1"));
   TEST_ASSERT_EQUAL_INT(31, mpq_index_sub_level_type_in(subs, 2, "expansion\\siege\\landmark1.ds1"));
   TEST_ASSERT_EQUAL_INT(-1, mpq_index_sub_level_type_in(subs, 2, "ACT2\\Outdoors\\Waypoint.ds1"));
   TEST_ASSERT_EQUAL_INT(-1, mpq_index_sub_level_type_in(subs, 2, NULL));
   TEST_ASSERT_EQUAL_INT(-1, mpq_index_sub_level_type_in(NULL, 0, "x.ds1"));
}


int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_sub_level_type_matches_a_whole_path_only);

   RUN_TEST(test_find_by_basename_exact);
   RUN_TEST(test_find_by_basename_case_insensitive);
   RUN_TEST(test_find_handles_forward_slashes);
   RUN_TEST(test_find_scans_all_file_slots);
   RUN_TEST(test_find_misses_return_null);
   RUN_TEST(test_find_respects_ds1_count);
   RUN_TEST(test_full_path_chooses_matching_preset_over_same_basename);
   RUN_TEST(test_full_path_of_a_project_copy_finds_its_preset);
   RUN_TEST(test_full_path_never_falls_back_to_a_namesake);

   RUN_TEST(test_row_names_ds1_never_crosses_folders_for_a_full_path);
   RUN_TEST(test_row_names_ds1_matches_a_bare_filename_in_any_folder);
   return UNITY_END();
}
