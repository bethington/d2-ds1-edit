#include <stddef.h>

#include "unity/unity.h"

#include "core/area_metadata.h"


void setUp(void)
{
}


void tearDown(void)
{
}


void test_area_name_parse_act_reads_standard_prefix(void)
{
   TEST_ASSERT_EQUAL_INT(3, area_name_parse_act("Act 3 - Jungle"));
   TEST_ASSERT_EQUAL_INT(5, area_name_parse_act("Act 5 - Town"));
}


void test_area_name_parse_act_rejects_nonstandard_names(void)
{
   TEST_ASSERT_EQUAL_INT(0, area_name_parse_act("Imperial Palace"));
   TEST_ASSERT_EQUAL_INT(0, area_name_parse_act("Act X - Broken"));
   TEST_ASSERT_EQUAL_INT(0, area_name_parse_act(NULL));
}


void test_area_name_has_act_mismatch_detects_inconsistent_lvltypes_rows(void)
{
   TEST_ASSERT_EQUAL_INT(1,
      area_name_has_act_mismatch(5, "Act 1 - Cathedral"));
   TEST_ASSERT_EQUAL_INT(1,
      area_name_has_act_mismatch(3, "Act 1 - Town"));
}


void test_area_name_has_act_mismatch_ignores_missing_or_matching_acts(void)
{
   TEST_ASSERT_EQUAL_INT(0,
      area_name_has_act_mismatch(3, "Act 3 - Jungle"));
   TEST_ASSERT_EQUAL_INT(0,
      area_name_has_act_mismatch(0, "Act 3 - Jungle"));
   TEST_ASSERT_EQUAL_INT(0,
      area_name_has_act_mismatch(5, "Imperial Palace"));
}


void test_area_group_resolve_act_prefers_lvltypes_column_over_name(void)
{
   TEST_ASSERT_EQUAL_INT(5, area_group_resolve_act(5, "Act 1 - Cathedral"));
   TEST_ASSERT_EQUAL_INT(3, area_group_resolve_act(3, "Act 1 - Town"));
}


void test_area_group_resolve_act_falls_back_to_name_when_txt_missing(void)
{
   TEST_ASSERT_EQUAL_INT(4, area_group_resolve_act(0, "Act 4 - Town"));
   TEST_ASSERT_EQUAL_INT(0, area_group_resolve_act(0, "Imperial Palace"));
}


void test_area_preset_resolve_act_uses_zero_based_levels_act(void)
{
   TEST_ASSERT_EQUAL_INT(1, area_preset_resolve_act(1, 0, "Act 5 - Wrong"));
   TEST_ASSERT_EQUAL_INT(3, area_preset_resolve_act(75, 2, "Act 1 - Wrong"));
   TEST_ASSERT_EQUAL_INT(5, area_preset_resolve_act(109, 4, "Town"));
}


void test_area_preset_resolve_act_uses_preset_name_for_null_level(void)
{
   TEST_ASSERT_EQUAL_INT(3, area_preset_resolve_act(0, 0,
      "Act 3 - Clearing Boggy E"));
   TEST_ASSERT_EQUAL_INT(0, area_preset_resolve_act(0, 0, "Guild"));
}


void test_area_name_strip_prefix_preserves_nonstandard_names(void)
{
   TEST_ASSERT_EQUAL_STRING("Jungle", area_name_strip_prefix("Act 3 - Jungle"));
   TEST_ASSERT_EQUAL_STRING("Imperial Palace", area_name_strip_prefix("Imperial Palace"));
}


void test_area_preset_area_name_files_rows_whose_names_differ_from_their_area(void)
{
   TEST_ASSERT_EQUAL_STRING("Act 3 - Kurast", area_preset_area_name("Act 3 - Burbs Border E"));
   TEST_ASSERT_EQUAL_STRING("Act 3 - Kurast", area_preset_area_name("Act 3 - MetroTemple"));
   TEST_ASSERT_EQUAL_STRING("Act 3 - Kurast", area_preset_area_name("Act 3 - Slums 08x08"));
   TEST_ASSERT_EQUAL_STRING("Act 3 - Kurast", area_preset_area_name("Act 3 - Mephisto Next W"));
   TEST_ASSERT_EQUAL_STRING("Act 3 - Jungle", area_preset_area_name("Act 3 - Clearing Boggy E"));
   TEST_ASSERT_EQUAL_STRING("Act 4 - Mesa", area_preset_area_name("Act 4 - Pits 1 08X08"));
   TEST_ASSERT_EQUAL_STRING("Act 4 - Lava", area_preset_area_name("Act 4 - Diablo Arm W"));
   TEST_ASSERT_EQUAL_STRING("Act 1 - Wilderness", area_preset_area_name("Act 1 - Cottages 1"));
   TEST_ASSERT_EQUAL_STRING("Act 1 - Wilderness", area_preset_area_name("Act 1 - DOE Entrance"));
   TEST_ASSERT_EQUAL_STRING("Act 2 - Harem", area_preset_area_name("Act 2 - Corrupt Harem NE"));
}


void test_area_preset_area_name_leaves_other_rows_alone(void)
{
   /* The same word in another act must not match. */
   TEST_ASSERT_NULL(area_preset_area_name("Act 2 - Bridge"));
   TEST_ASSERT_NULL(area_preset_area_name("Act 1 - Town 1"));
   TEST_ASSERT_NULL(area_preset_area_name("Ashen Plains Map"));
   TEST_ASSERT_NULL(area_preset_area_name(""));
   TEST_ASSERT_NULL(area_preset_area_name(NULL));
}


void test_area_pd2_folder_group_names_maps_no_table_mentions(void)
{
   TEST_ASSERT_EQUAL_STRING("Unnamed Maps", area_pd2_folder_group("expansion\\map\\darktomb.ds1"));
   TEST_ASSERT_EQUAL_STRING("Unnamed Maps", area_pd2_folder_group("Expansion/Map/lava2.ds1"));
   TEST_ASSERT_EQUAL_STRING("Arcane Variants", area_pd2_folder_group("ACT2\\Arcane\\sanctE-000.ds1"));
   TEST_ASSERT_EQUAL_STRING("Guilds", area_pd2_folder_group("Guild\\PD2\\guild4_original.ds1"));
   TEST_ASSERT_EQUAL_STRING("Other", area_pd2_folder_group("ACT4\\mesa\\fortress2.ds1"));
}


void test_area_map_destination_prefers_pd2_then_a_known_area(void)
{
   TEST_ASSERT_EQUAL_INT(AREA_DEST_PD2, area_map_destination(1, 37));
   TEST_ASSERT_EQUAL_INT(AREA_DEST_PD2, area_map_destination(1, -1));
   TEST_ASSERT_EQUAL_INT(AREA_DEST_AREA, area_map_destination(0, 22));
   TEST_ASSERT_EQUAL_INT(AREA_DEST_UNLISTED, area_map_destination(0, -1));
   TEST_ASSERT_EQUAL_INT(AREA_DEST_UNLISTED, area_map_destination(0, 0));
}


void test_area_section_key_keeps_pd2_after_the_acts_and_before_other(void)
{
   TEST_ASSERT_TRUE(area_section_key(0, 1) < area_section_key(0, 5));
   TEST_ASSERT_TRUE(area_section_key(0, 5) < area_section_key(1, 5));
   TEST_ASSERT_TRUE(area_section_key(1, 5) < area_section_key(0, 0));
   /* every PD2 group shares one section whatever act its level belongs to */
   TEST_ASSERT_EQUAL_INT(area_section_key(1, 1), area_section_key(1, 5));
}


void test_area_tileless_palette_act_trusts_a_map_that_names_a_later_act(void)
{
   /* expansion\\lavaX.ds1 holds Act 4 tiles in a folder filed under Act 5 */
   TEST_ASSERT_EQUAL_INT(4, area_tileless_palette_act(4, 5));
   TEST_ASSERT_EQUAL_INT(5, area_tileless_palette_act(5, 2));
}


void test_area_tileless_palette_act_treats_act_1_as_the_files_default(void)
{
   /* expansion\\Siege\\cliff2.ds1 says Act 1 only because nothing set it */
   TEST_ASSERT_EQUAL_INT(5, area_tileless_palette_act(1, 5));
   TEST_ASSERT_EQUAL_INT(1, area_tileless_palette_act(1, 1));
   /* no folder to go on: the map's word is all there is */
   TEST_ASSERT_EQUAL_INT(1, area_tileless_palette_act(1, 0));
   TEST_ASSERT_EQUAL_INT(0, area_tileless_palette_act(0, 0));
   TEST_ASSERT_EQUAL_INT(3, area_tileless_palette_act(9, 3));
}


void test_area_preset_area_name_files_rows_that_merely_sound_like_another_area(void)
{
   /* The cave's door stands in the wilderness and is drawn with its tiles. */
   TEST_ASSERT_EQUAL_STRING("Act 1 - Wilderness",
                            area_preset_area_name("Act 1 - Cave Entrance"));
   TEST_ASSERT_NULL(area_preset_area_name("Act 1 - Cave Treasure"));
   /* The transition is Mesa ground; the fortress itself stays Town. */
   TEST_ASSERT_EQUAL_STRING("Act 4 - Mesa",
                            area_preset_area_name("Act 4 - Fortress Transition"));
   TEST_ASSERT_EQUAL_STRING("Act 4 - Town", area_preset_area_name("Act 4 - Fortress"));
}


void test_area_row_accepts_name_match_trusts_the_level_over_the_name(void)
{
   /* Expansion/wildtemple/tempEnter.ds1: named Temple, its level is Barricade */
   TEST_ASSERT_FALSE(area_row_accepts_name_match(31, NULL, 32, "Act 5 - Temple"));
   TEST_ASSERT_TRUE(area_row_accepts_name_match(31, NULL, 31, "Act 5 - Barricade"));
}


void test_area_row_accepts_name_match_trusts_a_rule_when_no_level_says(void)
{
   TEST_ASSERT_FALSE(area_row_accepts_name_match(0, "Act 1 - Wilderness", 3, "Act 1 - Cave"));
   TEST_ASSERT_TRUE(area_row_accepts_name_match(-1, "Act 1 - Wilderness", 2, "Act 1 - Wilderness"));
   TEST_ASSERT_TRUE(area_row_accepts_name_match(0, NULL, 3, "Act 1 - Cave"));
}


void test_area_folder_area_name_files_folders_no_table_uses(void)
{
   TEST_ASSERT_EQUAL_STRING("Act 1 - Wilderness", area_folder_area_name("Guild\\cottages\\guild1.ds1"));
   TEST_ASSERT_EQUAL_STRING("Act 1 - Wilderness", area_folder_area_name("guild/house2/guild3.ds1"));
   TEST_ASSERT_EQUAL_STRING("Act 2 - Tomb", area_folder_area_name("Arena\\Tombs\\DM2.ds1"));
   /* Project Diablo 2's own guild maps have tilesets of their own */
   TEST_ASSERT_NULL(area_folder_area_name("Guild\\PD2\\guild4_original.ds1"));
   TEST_ASSERT_NULL(area_folder_area_name(NULL));
}


/* version 18, act 4 (stored 0-based), tag type 0, then the name table */
static const unsigned char DS1_V18[] =
{
   18, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   3, 0, 0, 0,   0, 0, 0, 0,
   2, 0, 0, 0,   'a', 0,   'b', 'c', 0,
};


void test_area_ds1_embedded_names_reads_the_header(void)
{
   const char * names = NULL;
   int act = 0, count = 0;

   TEST_ASSERT_TRUE(area_ds1_embedded_names(DS1_V18, sizeof(DS1_V18), &act, &names, &count));
   TEST_ASSERT_EQUAL_INT(4, act);
   TEST_ASSERT_EQUAL_INT(2, count);
   TEST_ASSERT_EQUAL_STRING("a", names);
   TEST_ASSERT_EQUAL_STRING("bc", names + 2);
}


void test_area_ds1_embedded_names_rejects_a_table_that_overruns_the_file(void)
{
   const char * names = NULL;
   int act = 0, count = 0;

   /* cut inside the last name, so its terminator is gone */
   TEST_ASSERT_FALSE(area_ds1_embedded_names(DS1_V18, sizeof(DS1_V18) - 1, &act, &names, &count));
   /* cut inside the header */
   TEST_ASSERT_FALSE(area_ds1_embedded_names(DS1_V18, 10, &act, &names, &count));
   TEST_ASSERT_FALSE(area_ds1_embedded_names(NULL, 0, &act, &names, &count));
}


void test_area_ds1_embedded_names_skips_versions_without_a_table(void)
{
   static const unsigned char v2[] = { 2, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0 };
   const char * names = NULL;
   int act = 0, count = 0;

   TEST_ASSERT_FALSE(area_ds1_embedded_names(v2, sizeof(v2), &act, &names, &count));
}


void test_area_dt1_names_match_compares_folder_and_name_only(void)
{
   TEST_ASSERT_TRUE(area_dt1_names_match(
      "C:\\d2\\Data\\GLOBAL\\TILES\\ACT1\\Caves\\cave.tg1", "ACT1\\CAVES\\Cave.dt1"));
   TEST_ASSERT_TRUE(area_dt1_names_match("\\d2\\expansion\\siege\\snow.dt1", "Expansion/Siege/Snow.dt1"));
   TEST_ASSERT_FALSE(area_dt1_names_match("\\d2\\act1\\town\\floor.tg1", "ACT1\\Outdoors\\Floor.dt1"));
   TEST_ASSERT_FALSE(area_dt1_names_match("cave.tg1", "ACT1\\Caves\\Cave.dt1"));
}


void test_area_dt1_names_score_counts_the_names_a_tileset_holds(void)
{
   static const char names[] = "x\\act1\\caves\\cave.tg1\0x\\act1\\town\\floor.tg1\0";
   const char * const both[] = { "ACT1\\Town\\Floor.dt1", "ACT1\\Caves\\Cave.dt1" };
   const char * const one[]  = { "ACT1\\Caves\\Cave.dt1", "0" };

   TEST_ASSERT_EQUAL_INT(100, area_dt1_names_score(names, 2, both, 2));
   TEST_ASSERT_EQUAL_INT(50, area_dt1_names_score(names, 2, one, 2));
}


void test_area_dt1_names_min_score_lets_a_pd2_map_take_its_nearest_tileset(void)
{
   /* expansion\\map\\darktomb.ds1 names mostly PD2's own DT1s, yet only 37% of
    * its names are enough to find Act 2 Tomb, and it stays in the PD2 section. */
   TEST_ASSERT_TRUE(37 >= area_dt1_names_min_score(1));
   /* A wrong guess would put a vanilla map under the wrong area. */
   TEST_ASSERT_FALSE(37 >= area_dt1_names_min_score(0));
   TEST_ASSERT_EQUAL_INT(AREA_DT1_NAMES_MIN_SCORE, area_dt1_names_min_score(0));
   /* A score of 0 is no fit at all. */
   TEST_ASSERT_TRUE(area_dt1_names_min_score(1) > 0);
}


void test_area_dt1_names_score_ignores_the_warp_tiles_every_map_carries(void)
{
   static const char names[] = "x\\act1\\caves\\cave.tg1\0x\\act1\\barracks\\warp.tg1\0";
   static const char only_warp[] = "x\\act1\\barracks\\warp.tg1\0";
   const char * const caves[] = { "ACT1\\Caves\\Cave.dt1" };

   TEST_ASSERT_EQUAL_INT(100, area_dt1_names_score(names, 2, caves, 1));
   /* nothing left to judge by */
   TEST_ASSERT_EQUAL_INT(-1, area_dt1_names_score(only_warp, 1, caves, 1));
   TEST_ASSERT_EQUAL_INT(-1, area_dt1_names_score(NULL, 0, caves, 1));
}


int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_area_name_parse_act_reads_standard_prefix);
   RUN_TEST(test_area_name_parse_act_rejects_nonstandard_names);
   RUN_TEST(test_area_name_has_act_mismatch_detects_inconsistent_lvltypes_rows);
   RUN_TEST(test_area_name_has_act_mismatch_ignores_missing_or_matching_acts);
   RUN_TEST(test_area_group_resolve_act_prefers_lvltypes_column_over_name);
   RUN_TEST(test_area_group_resolve_act_falls_back_to_name_when_txt_missing);
   RUN_TEST(test_area_preset_resolve_act_uses_zero_based_levels_act);
   RUN_TEST(test_area_preset_resolve_act_uses_preset_name_for_null_level);
   RUN_TEST(test_area_name_strip_prefix_preserves_nonstandard_names);
   RUN_TEST(test_area_preset_area_name_files_rows_whose_names_differ_from_their_area);
   RUN_TEST(test_area_preset_area_name_leaves_other_rows_alone);
   RUN_TEST(test_area_pd2_folder_group_names_maps_no_table_mentions);
   RUN_TEST(test_area_map_destination_prefers_pd2_then_a_known_area);
   RUN_TEST(test_area_section_key_keeps_pd2_after_the_acts_and_before_other);
   RUN_TEST(test_area_tileless_palette_act_trusts_a_map_that_names_a_later_act);
   RUN_TEST(test_area_tileless_palette_act_treats_act_1_as_the_files_default);
   RUN_TEST(test_area_preset_area_name_files_rows_that_merely_sound_like_another_area);
   RUN_TEST(test_area_row_accepts_name_match_trusts_the_level_over_the_name);
   RUN_TEST(test_area_row_accepts_name_match_trusts_a_rule_when_no_level_says);
   RUN_TEST(test_area_folder_area_name_files_folders_no_table_uses);
   RUN_TEST(test_area_ds1_embedded_names_reads_the_header);
   RUN_TEST(test_area_ds1_embedded_names_rejects_a_table_that_overruns_the_file);
   RUN_TEST(test_area_ds1_embedded_names_skips_versions_without_a_table);
   RUN_TEST(test_area_dt1_names_match_compares_folder_and_name_only);
   RUN_TEST(test_area_dt1_names_score_counts_the_names_a_tileset_holds);
   RUN_TEST(test_area_dt1_names_score_ignores_the_warp_tiles_every_map_carries);
   RUN_TEST(test_area_dt1_names_min_score_lets_a_pd2_map_take_its_nearest_tileset);
   return UNITY_END();
}
