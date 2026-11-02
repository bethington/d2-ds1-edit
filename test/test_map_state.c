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

int main(int argc, char **argv)
{
   pkgc_test_filter = argc > 1 ? argv[1] : NULL;
   UNITY_BEGIN();
   PKGC_RUN(test_a_fresh_map_is_clean);
   PKGC_RUN(test_mark_and_save);
   PKGC_RUN(test_object_undo_returns_to_the_state_before_the_edit);
   PKGC_RUN(test_object_undo_after_a_save_is_an_unsaved_change);
   PKGC_RUN(test_empty_slots_are_never_dirty);
   return UNITY_END();
}
