/* Unsaved-edit rules, over the fixture's in-memory maps. Whole edit paths are
 * covered in test_editor_undo and test_ds1_save. */
#include <stdio.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "core/map_state.h"
#include "support/editor_test_state.h"

void setUp(void)
{
   TEST_ASSERT_TRUE(editor_test_state_init(2, 2, 2));
}

void tearDown(void)
{
   editor_test_state_destroy();
}

void test_a_fresh_map_is_clean(void)
{
   TEST_ASSERT_FALSE(map_is_dirty(0));
   TEST_ASSERT_FALSE(map_any_dirty());
}

void test_mark_and_save(void)
{
   map_dirty_mark(1);
   TEST_ASSERT_TRUE(map_is_dirty(1));
   TEST_ASSERT_FALSE(map_is_dirty(0));
   TEST_ASSERT_TRUE(map_any_dirty());
   map_dirty_mark_saved(1);
   TEST_ASSERT_FALSE(map_is_dirty(1));
   TEST_ASSERT_FALSE(map_any_dirty());
}

void test_object_undo_returns_to_the_state_before_the_edit(void)
{
   map_dirty_before_object_edit(0);
   TEST_ASSERT_TRUE(map_is_dirty(0));
   map_dirty_after_object_undo(0);
   TEST_ASSERT_FALSE(map_is_dirty(0));
}

void test_object_undo_after_a_save_is_an_unsaved_change(void)
{
   map_dirty_before_object_edit(0);
   map_dirty_mark_saved(0);
   TEST_ASSERT_FALSE(map_is_dirty(0));
   map_dirty_after_object_undo(0);
   TEST_ASSERT_TRUE(map_is_dirty(0));
}

void test_empty_slots_are_never_dirty(void)
{
   TEST_ASSERT_FALSE(map_is_dirty(-1));
   TEST_ASSERT_FALSE(map_is_dirty(DS1_MAX));
   TEST_ASSERT_FALSE(map_is_dirty(2));   /* a zeroed slot, which the fixture left unnamed */
}

void test_classify_covers_every_state(void)
{
   TEST_ASSERT_EQUAL_INT(MAP_STATE_GAME,     map_state_classify(1, 0, 0));
   TEST_ASSERT_EQUAL_INT(MAP_STATE_MODIFIED, map_state_classify(1, 1, 0));
   TEST_ASSERT_EQUAL_INT(MAP_STATE_NEW,      map_state_classify(0, 1, 0));
   TEST_ASSERT_EQUAL_INT(MAP_STATE_REMOVED,  map_state_classify(1, 0, 1));
   TEST_ASSERT_EQUAL_INT(MAP_STATE_REMOVED,  map_state_classify(1, 1, 1));
}

void test_marks(void)
{
   TEST_ASSERT_EQUAL_STRING("",  map_state_mark(MAP_STATE_GAME));
   TEST_ASSERT_EQUAL_STRING("M", map_state_mark(MAP_STATE_MODIFIED));
   TEST_ASSERT_EQUAL_STRING("N", map_state_mark(MAP_STATE_NEW));
}

void test_path_matching_ignores_case_and_slash_style_at_a_boundary(void)
{
   TEST_ASSERT_TRUE(map_state_path_matches("Act1/Town/TownN1.ds1", "act1\\town\\townn1.ds1"));
   TEST_ASSERT_TRUE(map_state_path_matches("C:\\p\\Global\\Tiles\\Act1\\Town\\TownN1.ds1",
                                           "Act1/Town/TownN1.ds1"));
   TEST_ASSERT_FALSE(map_state_path_matches("x/XAct1/Town/TownN1.ds1", "Act1/Town/TownN1.ds1"));
   TEST_ASSERT_FALSE(map_state_path_matches("Town.ds1", "Act1/Town.ds1"));
   TEST_ASSERT_FALSE(map_state_path_matches("", "a.ds1"));
}

void test_object_undo_does_not_clear_another_unsaved_edit(void)
{
   map_dirty_before_object_edit(0);
   map_dirty_mark(0);
   map_dirty_after_object_undo(0);
   TEST_ASSERT_TRUE(map_is_dirty(0));
}

int main(int argc, char **argv)
{
   pkgc_test_filter = argc > 1 ? argv[1] : NULL;
   UNITY_BEGIN();
   PKGC_RUN(test_a_fresh_map_is_clean);
   PKGC_RUN(test_mark_and_save);
   PKGC_RUN(test_object_undo_returns_to_the_state_before_the_edit);
   PKGC_RUN(test_object_undo_after_a_save_is_an_unsaved_change);
   PKGC_RUN(test_empty_slots_are_never_dirty);
   PKGC_RUN(test_classify_covers_every_state);
   PKGC_RUN(test_marks);
   PKGC_RUN(test_path_matching_ignores_case_and_slash_style_at_a_boundary);
   PKGC_RUN(test_object_undo_does_not_clear_another_unsaved_edit);
   return UNITY_END();
}
