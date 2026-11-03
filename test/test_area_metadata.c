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
   TEST_ASSERT_EQUAL_STRING("Act 4 - Town", area_preset_area_name("Act 4 - Fortress Transition"));
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
   return UNITY_END();
}
