#include <string.h>
#include "unity/unity.h"
#include "structs.h"
#include "editor/tiles.h"
#include "editor/undo.h"

void wprop_2_block(int i, CELL_W_S *cell) { (void)i; (void)cell; }
void fprop_2_block(int i, CELL_F_S *cell) { (void)i; (void)cell; }
void sprop_2_block(int i, CELL_S_S *cell) { (void)i; (void)cell; }
void undo_add_this_tile_to_buffer(int i, int x, int y,
                                  CELL_F_S *f, CELL_W_S *w, CELL_S_S *s)
{
   (void)i; (void)x; (void)y; (void)f; (void)w; (void)s;
}

static DS1_S map;
static CELL_F_S floors[6];
static CELL_S_S shadows[3];
static CELL_W_S walls[6];
static int undo_cells;
DS1_S *glb_ds1 = &map;

void undo_new_tile_buffer(int ds1_idx)
{
   (void)ds1_idx;
   undo_cells = 0;
}
void undo_add_to_tile_buffer(int ds1_idx, int x, int y)
{
   (void)ds1_idx; (void)x; (void)y;
   undo_cells++;
}
void undo_close_tile_buffer(int ds1_idx) { (void)ds1_idx; }

void setUp(void)
{
   memset(&map, 0, sizeof(map));
   memset(floors, 0, sizeof(floors));
   memset(shadows, 0, sizeof(shadows));
   memset(walls, 0, sizeof(walls));
   map.width = 3;
   map.height = 1;
   map.floor_num = 2;
   map.floor_line = 6;
   map.shadow_num = 1;
   map.shadow_line = 3;
   map.wall_num = 2;
   map.wall_line = 6;
   map.floor_buff = floors;
   map.shadow_buff = shadows;
   map.wall_buff = walls;
   map.floor_layer_mask[0] = map.floor_layer_mask[1] = 1;
   map.shadow_layer_mask[0] = 1;
   map.wall_layer_mask[0] = map.wall_layer_mask[1] = 1;

   /* Cell 0 is the reference. Cell 1 has the same tiles in a different
    * floor layer order. Cell 2 has an extra wall tile. */
   floors[0].bt_idx = 10;
   floors[1].bt_idx = 20;
   floors[2].bt_idx = 20;
   floors[3].bt_idx = 10;
   floors[4].bt_idx = 10;
   floors[5].bt_idx = 20;
   shadows[0].bt_idx = shadows[1].bt_idx = shadows[2].bt_idx = 5;
   walls[0].bt_idx = walls[2].bt_idx = walls[4].bt_idx = 7;
   walls[5].bt_idx = 8;
   undo_cells = 0;
}
void tearDown(void) {}

void test_strict_identical_selects_complete_cells_and_records_undo(void)
{
   SET_SELECTED(floors[4].flags);
   edittile_identical(0, IT_NEW, 0, 0, 1);
   TEST_ASSERT_TRUE(IS_SELECTED(floors[0].flags));
   TEST_ASSERT_TRUE(IS_SELECTED(floors[2].flags));
   TEST_ASSERT_FALSE(IS_SELECTED(floors[4].flags));
   TEST_ASSERT_TRUE(IS_SELECTED(shadows[1].flags));
   TEST_ASSERT_FALSE(IS_SELECTED(shadows[2].flags));
   TEST_ASSERT_TRUE(IS_SELECTED(walls[2].flags));
   TEST_ASSERT_EQUAL_INT(3, undo_cells);
}

void test_strict_identical_ignores_hidden_layers(void)
{
   map.wall_layer_mask[1] = 0;
   edittile_identical(0, IT_NEW, 0, 0, 1);
   TEST_ASSERT_TRUE(IS_SELECTED(floors[4].flags));
   TEST_ASSERT_TRUE(IS_SELECTED(walls[4].flags));
   TEST_ASSERT_FALSE(IS_SELECTED(walls[5].flags));
   TEST_ASSERT_EQUAL_INT(3, undo_cells);
}

void test_strict_identical_add_and_remove_preserve_other_cells(void)
{
   SET_SELECTED(floors[4].flags);
   edittile_identical(0, IT_ADD, 0, 0, 1);
   TEST_ASSERT_TRUE(IS_SELECTED(floors[4].flags));
   TEST_ASSERT_TRUE(IS_SELECTED(floors[0].flags));
   edittile_identical(0, IT_DEL, 0, 0, 1);
   TEST_ASSERT_FALSE(IS_SELECTED(floors[0].flags));
   TEST_ASSERT_FALSE(IS_SELECTED(floors[2].flags));
   TEST_ASSERT_TRUE(IS_SELECTED(floors[4].flags));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_strict_identical_selects_complete_cells_and_records_undo);
   RUN_TEST(test_strict_identical_ignores_hidden_layers);
   RUN_TEST(test_strict_identical_add_and_remove_preserve_other_cells);
   return UNITY_END();
}
