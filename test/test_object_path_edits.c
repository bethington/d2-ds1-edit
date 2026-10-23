/*
 * Object and NPC-path editing on real editor code: editobj_insert_obj,
 * editobj_copy_obj, editobj_del_obj, the move helpers, the one-step object
 * undo (editobj_prepare_undo / editobj_undo), and path editing driven
 * through editpath_draw() with synthetic mouse clicks on its own buttons.
 *
 * editpath_draw() also draws its window, so the test gives it an offscreen
 * memory bitmap and Allegro's built-in font; no display is created. Edited
 * maps are saved with the checked writer and read back with the real reader.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "unity/unity.h"
#include "structs.h"
#include "ui/compat.h"
#include "core/ds1.h"
#include "core/ds1_save_io.h"
#include "editor/objects.h"
#include "editor/paths.h"

#include "support/editor_test_state.h"

static ALLEGRO_BITMAP *offscreen;
static ALLEGRO_FONT   *builtin_font;

typedef struct OBJ_DEF
{
   long type, id, x, y, flags;
   int  npts;
   long pts[4][3];
} OBJ_DEF;

static const OBJ_DEF map0_objs[3] = {
   { 1, 5,  2,  3, 0x10, 2, { { 4, 4, 2 }, { 6, 5, 3 } } },
   { 2, 9, 10,  7, 0,    0, { { 0 } } },
   { 1, 1, 18, 13, 1,    1, { { 17, 12, 1 } } },
};

static void put_object(int m, int o, const OBJ_DEF *d)
{
   OBJ_S *ob = &glb_ds1[m].obj[o];
   int p;
   memset(ob, 0, sizeof(*ob));
   ob->type = d->type;
   ob->id = d->id;
   ob->x = d->x;
   ob->y = d->y;
   ob->ds1_flags = d->flags;
   ob->desc_idx = -1;
   ob->path_num = d->npts;
   for (p = 0; p < d->npts; p++)
   {
      ob->path[p].x = d->pts[p][0];
      ob->path[p].y = d->pts[p][1];
      ob->path[p].action = d->pts[p][2];
   }
}

void setUp(void)
{
   int o;
   static const OBJ_DEF other = { 2, 3, 1, 1, 0, 1, { { 2, 2, 4 } } };

   TEST_ASSERT_TRUE(editor_test_state_init(2, 4, 3));
   glb_ds1edit.screen_buff = offscreen;
   a5_font = builtin_font;
   for (o = 0; o < 3; o++)
      put_object(0, o, &map0_objs[o]);
   glb_ds1[0].obj_num = 3;
   put_object(1, 0, &other);
   glb_ds1[1].obj_num = 1;
}

void tearDown(void)
{
   editor_test_state_destroy();
}

/* Semantic equality of one object with a definition. */
static void expect_obj(int m, int o, const OBJ_DEF *d)
{
   const OBJ_S *ob = &glb_ds1[m].obj[o];
   char msg[64];
   int p;
   snprintf(msg, sizeof(msg), "map %d object %d", m, o);
   TEST_ASSERT_EQUAL_INT_MESSAGE(d->type, ob->type, msg);
   TEST_ASSERT_EQUAL_INT_MESSAGE(d->id, ob->id, msg);
   TEST_ASSERT_EQUAL_INT_MESSAGE(d->x, ob->x, msg);
   TEST_ASSERT_EQUAL_INT_MESSAGE(d->y, ob->y, msg);
   TEST_ASSERT_EQUAL_INT_MESSAGE(d->flags, ob->ds1_flags, msg);
   TEST_ASSERT_EQUAL_INT_MESSAGE(d->npts, ob->path_num, msg);
   for (p = 0; p < d->npts; p++)
   {
      TEST_ASSERT_EQUAL_INT_MESSAGE(d->pts[p][0], ob->path[p].x, msg);
      TEST_ASSERT_EQUAL_INT_MESSAGE(d->pts[p][1], ob->path[p].y, msg);
      TEST_ASSERT_EQUAL_INT_MESSAGE(d->pts[p][2], ob->path[p].action, msg);
   }
}

static void expect_map1_untouched(void)
{
   static const OBJ_DEF other = { 2, 3, 1, 1, 0, 1, { { 2, 2, 4 } } };
   TEST_ASSERT_EQUAL_INT(1, glb_ds1[1].obj_num);
   expect_obj(1, 0, &other);
}

/* ---------------- objects ---------------- */

void test_insert_appends_one_selected_default_object(void)
{
   static const OBJ_DEF fresh = { 1, 0, 7, 9, 0, 0, { { 0 } } };
   int o;
   TEST_ASSERT_EQUAL_INT(0, editobj_insert_obj(0, 7, 9));
   TEST_ASSERT_EQUAL_INT(4, glb_ds1[0].obj_num);
   expect_obj(0, 3, &fresh);
   TEST_ASSERT_TRUE(IS_SELECTED(glb_ds1[0].obj[3].flags));
   for (o = 0; o < 3; o++)
      expect_obj(0, o, &map0_objs[o]);
   expect_map1_untouched();
}

void test_copy_duplicates_selected_objects_with_their_paths(void)
{
   SET_SELECTED(glb_ds1[0].obj[0].flags);
   editobj_copy_obj(0);
   TEST_ASSERT_EQUAL_INT(4, glb_ds1[0].obj_num);
   expect_obj(0, 3, &map0_objs[0]);
   TEST_ASSERT_TRUE(IS_SELECTED(glb_ds1[0].obj[3].flags));
   TEST_ASSERT_FALSE(IS_SELECTED(glb_ds1[0].obj[0].flags));
   expect_obj(0, 0, &map0_objs[0]);
   expect_obj(0, 1, &map0_objs[1]);
   expect_obj(0, 2, &map0_objs[2]);
   expect_map1_untouched();
}

void test_copy_past_the_object_capacity_grows_and_keeps_everything(void)
{
   int o;
   for (o = 0; o < 3; o++)
      SET_SELECTED(glb_ds1[0].obj[o].flags);
   /* 3 -> 6 -> 12 -> ... -> 192 objects: crosses OBJ_MAX_START (100) */
   for (o = 0; o < 6; o++)
   {
      int n;
      editobj_copy_obj(0);
      for (n = 0; n < glb_ds1[0].obj_num; n++)
         SET_SELECTED(glb_ds1[0].obj[n].flags);
   }
   TEST_ASSERT_EQUAL_INT(192, glb_ds1[0].obj_num);
   TEST_ASSERT_TRUE(glb_ds1[0].current_obj_max >= 192);
   for (o = 0; o < 192; o++)
      expect_obj(0, o, &map0_objs[o % 3]);
}

void test_delete_removes_selected_objects_and_keeps_order(void)
{
   SET_SELECTED(glb_ds1[0].obj[1].flags);
   editobj_del_obj(0);
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].obj_num);
   expect_obj(0, 0, &map0_objs[0]);
   expect_obj(0, 1, &map0_objs[2]);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].obj[2].type);
   expect_map1_untouched();
}

void test_delete_first_and_last_keeps_the_middle(void)
{
   SET_SELECTED(glb_ds1[0].obj[0].flags);
   SET_SELECTED(glb_ds1[0].obj[2].flags);
   editobj_del_obj(0);
   TEST_ASSERT_EQUAL_INT(1, glb_ds1[0].obj_num);
   expect_obj(0, 0, &map0_objs[1]);
}

void test_delete_everything_leaves_no_object(void)
{
   int o;
   for (o = 0; o < 3; o++)
      SET_SELECTED(glb_ds1[0].obj[o].flags);
   editobj_del_obj(0);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].obj_num);
   for (o = 0; o < 3; o++)
      TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].obj[o].path_num);
}

void test_delete_with_nothing_selected_changes_nothing(void)
{
   int o;
   editobj_del_obj(0);
   TEST_ASSERT_EQUAL_INT(3, glb_ds1[0].obj_num);
   for (o = 0; o < 3; o++)
      expect_obj(0, o, &map0_objs[o]);
}

void test_delete_on_a_map_without_objects_creates_none(void)
{
   /* The Delete key reaches editobj_del_obj() whether or not the map has
    * objects. */
   editobj_del_obj(1);   /* map 1 has one, unselected */
   TEST_ASSERT_EQUAL_INT(1, glb_ds1[1].obj_num);
   glb_ds1[1].obj_num = 0;
   memset(&glb_ds1[1].obj[0], 0, sizeof(OBJ_S));
   editobj_del_obj(1);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[1].obj_num);
}

void test_move_offsets_selected_objects_and_clamps_to_the_map(void)
{
   OBJ_DEF moved0 = map0_objs[0], moved2 = map0_objs[2];
   SET_SELECTED(glb_ds1[0].obj[0].flags);
   SET_SELECTED(glb_ds1[0].obj[2].flags);
   editobj_prepare_moving(0);
   editobj_moving_obj_lab(0, 1, 1, 0, 0);
   editobj_moving_obj_lab(0, 3, -4, 0, 0);
   editobj_end_move_obj_lab(0);
   /* (2,3)+(3,-4) -> (5,0) after clamping y; (18,13)+(3,-4) -> clamp x at 19 */
   moved0.x = 5;  moved0.y = 0;
   moved2.x = 19; moved2.y = 9;
   expect_obj(0, 0, &moved0);   /* path points are absolute: not moved */
   expect_obj(0, 1, &map0_objs[1]);
   expect_obj(0, 2, &moved2);
   TEST_ASSERT_FALSE(IS_MOVING(glb_ds1[0].obj[0].flags));
}

static void save_reload_expect(const OBJ_DEF *defs, int n);

/* ---------- objects outside the map (the original game keeps them) ---------- */

/* map 0 is 4x3 cells = 20x15 sub-tiles; these sit outside it on purpose */
static const OBJ_DEF oob_objs[3] = {
   { 1, 21, -3, -7,   0x20, 1, { { -4, -8, 2 } } },
   { 2, 22, 40, 999,  0,    0, { { 0 } } },
   { 1, 23, 20, 4,    5,    0, { { 0 } } },   /* x == width * 5: one past */
};

static void add_oob_objects(void)
{
   int i;
   for (i = 0; i < 3; i++)
      put_object(0, 3 + i, &oob_objs[i]);
   glb_ds1[0].obj_num = 6;
}

void test_out_of_bounds_objects_are_counted(void)
{
   TEST_ASSERT_EQUAL_INT(0, editobj_count_out_of_bounds(0));
   TEST_ASSERT_FALSE(editobj_is_out_of_bounds(0, 0));
   add_oob_objects();
   TEST_ASSERT_EQUAL_INT(3, editobj_count_out_of_bounds(0));
   TEST_ASSERT_FALSE(editobj_is_out_of_bounds(0, 2));
   TEST_ASSERT_TRUE(editobj_is_out_of_bounds(0, 3));
   TEST_ASSERT_TRUE(editobj_is_out_of_bounds(0, 5));
   TEST_ASSERT_EQUAL_INT(0, editobj_count_out_of_bounds(1));
}

void test_drawing_order_keys_of_far_objects_stay_in_range(void)
{
   int o, seen[8] = { 0 };
   add_oob_objects();
   editobj_set_drawing_order(0);
   for (o = 0; o < 6; o++)
   {
      /* the sub-tile keys index obj_sub_tile_order[5][5] */
      TEST_ASSERT_TRUE_MESSAGE(glb_ds1[0].obj[o].sx >= 0 && glb_ds1[0].obj[o].sx < 5, "sx");
      TEST_ASSERT_TRUE_MESSAGE(glb_ds1[0].obj[o].sy >= 0 && glb_ds1[0].obj[o].sy < 5, "sy");
      seen[glb_ds1[0].drawing_order[o]]++;
   }
   for (o = 0; o < 6; o++)
      TEST_ASSERT_EQUAL_INT(1, seen[o]);
   /* the keys are monotonic with position: -3 sits in a tile before 0 */
   TEST_ASSERT_TRUE(glb_ds1[0].obj[3].tx < glb_ds1[0].obj[0].tx);
   TEST_ASSERT_TRUE(glb_ds1[0].obj[3].ty < glb_ds1[0].obj[0].ty);
}

void test_delete_removes_an_out_of_bounds_object_only(void)
{
   add_oob_objects();
   SET_SELECTED(glb_ds1[0].obj[4].flags);
   editobj_prepare_undo(0);
   editobj_del_obj(0);
   TEST_ASSERT_EQUAL_INT(5, glb_ds1[0].obj_num);
   expect_obj(0, 0, &map0_objs[0]);
   expect_obj(0, 1, &map0_objs[1]);
   expect_obj(0, 2, &map0_objs[2]);
   expect_obj(0, 3, &oob_objs[0]);
   expect_obj(0, 4, &oob_objs[2]);
   editobj_undo(0);
   TEST_ASSERT_EQUAL_INT(6, glb_ds1[0].obj_num);
   expect_obj(0, 4, &oob_objs[1]);
}

void test_move_of_an_out_of_bounds_object_is_not_snapped_to_the_edge(void)
{
   OBJ_DEF moved = oob_objs[0];
   add_oob_objects();
   SET_SELECTED(glb_ds1[0].obj[3].flags);
   editobj_prepare_moving(0);
   editobj_moving_obj_lab(0, 1, 2, 0, 0);
   editobj_end_move_obj_lab(0);
   moved.x = -2; moved.y = -5;   /* still outside, still where the user put it */
   expect_obj(0, 3, &moved);
}

void test_moving_an_out_of_bounds_object_back_inside_works(void)
{
   OBJ_DEF moved = oob_objs[2];
   add_oob_objects();
   SET_SELECTED(glb_ds1[0].obj[5].flags);
   editobj_prepare_moving(0);
   editobj_moving_obj_lab(0, -3, 0, 0, 0);   /* x 20 -> 17 */
   editobj_end_move_obj_lab(0);
   moved.x = 17;
   expect_obj(0, 5, &moved);
   TEST_ASSERT_FALSE(editobj_is_out_of_bounds(0, 5));
}

void test_move_of_an_inside_object_is_still_clamped_to_the_map(void)
{
   add_oob_objects();
   SET_SELECTED(glb_ds1[0].obj[0].flags);
   SET_SELECTED(glb_ds1[0].obj[4].flags);   /* outside: must follow, unclamped */
   editobj_prepare_moving(0);
   editobj_moving_obj_lab(0, 100, 100, 0, 0);
   editobj_end_move_obj_lab(0);
   TEST_ASSERT_EQUAL_INT(19, glb_ds1[0].obj[0].x);
   TEST_ASSERT_EQUAL_INT(14, glb_ds1[0].obj[0].y);
   TEST_ASSERT_EQUAL_INT(140, glb_ds1[0].obj[4].x);
   TEST_ASSERT_EQUAL_INT(1099, glb_ds1[0].obj[4].y);
}

void test_move_undo_restores_an_out_of_bounds_object(void)
{
   add_oob_objects();
   SET_SELECTED(glb_ds1[0].obj[3].flags);
   editobj_prepare_undo(0);
   editobj_prepare_moving(0);
   editobj_moving_obj_lab(0, 9, 9, 0, 0);
   editobj_end_move_obj_lab(0);
   editobj_undo(0);
   expect_obj(0, 3, &oob_objs[0]);
}

void test_the_foot_of_an_out_of_bounds_object_can_be_picked_with_the_mouse(void)
{
   int t = -1, o = -1, x, y, ox0, oy0, tw, th;
   add_oob_objects();
   editobj_set_drawing_order(0);
   tw = glb_ds1[0].tile_w = 160;
   th = glb_ds1[0].tile_h = 80;
   glb_ds1edit.win_preview.x0 = 0;
   glb_ds1edit.win_preview.y0 = 0;
   x = glb_ds1[0].obj[4].x;
   y = glb_ds1[0].obj[4].y;
   ox0 = ((y - 2) * -tw / 10) + ((x + 3) * tw / 10);
   oy0 = ((y - 2) * th / 10) + ((x + 3) * th / 10) - 16;
   /* the tile cursor is clamped inside the map, so cx/cy can never match */
   editobj_over_obj_lab(0, &t, &o, 0, 0, ox0, oy0 + 8);
   TEST_ASSERT_EQUAL_INT(2, t);
   TEST_ASSERT_EQUAL_INT(4, o);
   TEST_ASSERT_TRUE(IS_MOUSE_OVER(glb_ds1[0].obj[4].flags));
   /* and away from it nothing is picked */
   editobj_over_obj_lab(0, &t, &o, 0, 0, ox0 + 200, oy0 + 8);
   TEST_ASSERT_EQUAL_INT(-1, o);
}

void test_out_of_bounds_objects_survive_edits_and_reload(void)
{
   OBJ_DEF expect[6];
   int i;
   add_oob_objects();
   /* delete one inside object, leave the three outside ones alone */
   SET_SELECTED(glb_ds1[0].obj[1].flags);
   editobj_del_obj(0);
   expect[0] = map0_objs[0];
   expect[1] = map0_objs[2];
   for (i = 0; i < 3; i++)
      expect[2 + i] = oob_objs[i];
   save_reload_expect(expect, 5);
}

void test_object_undo_restores_a_delete(void)
{
   int o;
   SET_SELECTED(glb_ds1[0].obj[0].flags);
   editobj_prepare_undo(0);
   editobj_del_obj(0);
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].obj_num);
   editobj_undo(0);
   TEST_ASSERT_EQUAL_INT(3, glb_ds1[0].obj_num);
   for (o = 0; o < 3; o++)
      expect_obj(0, o, &map0_objs[o]);
   TEST_ASSERT_TRUE(IS_SELECTED(glb_ds1[0].obj[0].flags));
   expect_map1_untouched();
}

void test_object_undo_restores_a_copy_and_move(void)
{
   int o;
   SET_SELECTED(glb_ds1[0].obj[1].flags);
   editobj_prepare_undo(0);
   editobj_copy_obj(0);
   editobj_prepare_moving(0);
   editobj_moving_obj_lab(0, 2, 2, 0, 0);
   editobj_end_move_obj_lab(0);
   TEST_ASSERT_EQUAL_INT(4, glb_ds1[0].obj_num);
   editobj_undo(0);
   TEST_ASSERT_EQUAL_INT(3, glb_ds1[0].obj_num);
   for (o = 0; o < 3; o++)
      expect_obj(0, o, &map0_objs[o]);
}

void test_object_undo_without_a_snapshot_does_nothing(void)
{
   int o;
   SET_SELECTED(glb_ds1[0].obj[0].flags);
   editobj_del_obj(0);
   editobj_undo(0);
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].obj_num);
   for (o = 0; o < 2; o++)
      expect_obj(0, o, &map0_objs[o + 1]);
}

void test_object_undo_is_per_map(void)
{
   SET_SELECTED(glb_ds1[0].obj[0].flags);
   SET_SELECTED(glb_ds1[1].obj[0].flags);
   editobj_prepare_undo(0);
   editobj_prepare_undo(1);
   editobj_del_obj(0);
   editobj_del_obj(1);
   editobj_undo(1);
   expect_map1_untouched();
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].obj_num);
}

/* ---------------- paths through editpath_draw ---------------- */

/* Button centres, from editpath_init(): command window at (10,20). */
#define BTN_OK_X      25
#define BTN_OK_Y      35
#define BTN_CANCEL_X 100
#define BTN_CANCEL_Y  35
#define BTN_ALLNEW_X  25   /* ALL NEW and STOP share a place */
#define BTN_ALLNEW_Y  75
#define MAP_X        600   /* outside both path windows */
#define MAP_Y        500

static void click(int mx, int my, long tx, long ty)
{
   editpath_draw(0, mx, my, 1, tx, ty);
}

static void begin_path_edit_of(int o)
{
   SET_SELECTED(glb_ds1[0].obj[o].flags);
   editpath_search_selected_obj(0);
   TEST_ASSERT_EQUAL_INT(o, glb_ds1[0].path_edit_win.obj_idx);
   editpath_draw(0, 0, 0, 0, 0, 0);   /* first frame: window init */
}

void test_path_all_new_then_ok_replaces_the_points(void)
{
   OBJ_DEF expect = map0_objs[0];
   begin_path_edit_of(0);
   click(BTN_ALLNEW_X, BTN_ALLNEW_Y, 0, 0);
   TEST_ASSERT_EQUAL_INT(PEM_ALLNEW, glb_ds1[0].path_edit_win.mode);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].obj[0].path_num);
   click(MAP_X, MAP_Y, 3, 4);
   click(MAP_X, MAP_Y, 5, 6);
   click(BTN_OK_X, BTN_OK_Y, 0, 0);
   TEST_ASSERT_EQUAL_INT(PEM_NONE, glb_ds1[0].path_edit_win.mode);
   TEST_ASSERT_FALSE(glb_ds1[0].path_edit_win.obj_dirty);
   expect.npts = 2;
   expect.pts[0][0] = 3; expect.pts[0][1] = 4; expect.pts[0][2] = 1;
   expect.pts[1][0] = 5; expect.pts[1][1] = 6; expect.pts[1][2] = 1;
   expect_obj(0, 0, &expect);
   expect_obj(0, 1, &map0_objs[1]);
   expect_obj(0, 2, &map0_objs[2]);
   expect_map1_untouched();
}

void test_path_all_new_then_cancel_restores_the_points(void)
{
   begin_path_edit_of(2);
   click(BTN_ALLNEW_X, BTN_ALLNEW_Y, 0, 0);
   click(MAP_X, MAP_Y, 1, 1);
   click(BTN_CANCEL_X, BTN_CANCEL_Y, 0, 0);
   expect_obj(0, 2, &map0_objs[2]);
   TEST_ASSERT_FALSE(glb_ds1[0].path_edit_win.obj_dirty);
}

void test_path_points_stop_at_the_editor_maximum(void)
{
   int i;
   begin_path_edit_of(1);
   click(BTN_ALLNEW_X, BTN_ALLNEW_Y, 0, 0);
   for (i = 0; i < WINDS1EDIT_PATH_MAX + 5; i++)
      click(MAP_X, MAP_Y, i % 20, i % 15);
   TEST_ASSERT_EQUAL_INT(WINDS1EDIT_PATH_MAX, glb_ds1[0].obj[1].path_num);
   click(BTN_OK_X, BTN_OK_Y, 0, 0);
   TEST_ASSERT_EQUAL_INT(WINDS1EDIT_PATH_MAX, glb_ds1[0].path_edit_win.path_copy_num);
}

void test_clicks_on_the_path_windows_do_not_add_points(void)
{
   begin_path_edit_of(0);
   click(BTN_ALLNEW_X, BTN_ALLNEW_Y, 0, 0);
   click(50, 300, 7, 7);    /* inside the path list window */
   click(300, 40, 7, 7);    /* inside the command window, no button */
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].obj[0].path_num);
}

/* ---------------- save and reload ---------------- */

static void save_reload_expect(const OBJ_DEF *defs, int n)
{
   char path[512], err[512];
   int o;
   editor_test_state_path(path, sizeof(path), "edited.ds1");
   snprintf(glb_ds1[0].name, sizeof(glb_ds1[0].name), "%s", path);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_save_checked(0, FALSE, err, sizeof(err)), err);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_read_checked(path, 2, 0, 0, err, sizeof(err)), err);
   TEST_ASSERT_EQUAL_INT(n, glb_ds1[2].obj_num);
   for (o = 0; o < n; o++)
      expect_obj(2, o, &defs[o]);
}

void test_saved_object_edits_survive_reload(void)
{
   OBJ_DEF expect[4];
   static const OBJ_DEF fresh = { 1, 0, 11, 2, 0, 0, { { 0 } } };
   /* delete obj 1, insert one, move obj 0 */
   SET_SELECTED(glb_ds1[0].obj[1].flags);
   editobj_del_obj(0);
   TEST_ASSERT_EQUAL_INT(0, editobj_insert_obj(0, 11, 2));
   editobj_clear_obj_lab_sel(0);
   SET_SELECTED(glb_ds1[0].obj[0].flags);
   editobj_prepare_moving(0);
   editobj_moving_obj_lab(0, 1, 2, 0, 0);
   editobj_end_move_obj_lab(0);
   /* the moved object's path anchor moves with it on save */
   expect[0] = map0_objs[0]; expect[0].x = 3; expect[0].y = 5;
   expect[1] = map0_objs[2];
   expect[2] = fresh;
   save_reload_expect(expect, 3);
}

void test_saved_path_edits_survive_reload(void)
{
   OBJ_DEF expect[3];
   begin_path_edit_of(1);
   click(BTN_ALLNEW_X, BTN_ALLNEW_Y, 0, 0);
   click(MAP_X, MAP_Y, 9, 8);
   click(MAP_X, MAP_Y, 11, 6);
   click(BTN_OK_X, BTN_OK_Y, 0, 0);
   expect[0] = map0_objs[0];
   expect[1] = map0_objs[1];
   expect[1].npts = 2;
   expect[1].pts[0][0] = 9;  expect[1].pts[0][1] = 8; expect[1].pts[0][2] = 1;
   expect[1].pts[1][0] = 11; expect[1].pts[1][1] = 6; expect[1].pts[1][2] = 1;
   expect[2] = map0_objs[2];
   save_reload_expect(expect, 3);
}

int main(int argc, char **argv)
{
   pkgc_test_filter = argc > 1 ? argv[1] : NULL;
   al_init();
   al_init_font_addon();
   al_init_primitives_addon();
   al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);
   offscreen = al_create_bitmap(800, 600);
   builtin_font = al_create_builtin_font();
   if (offscreen == NULL || builtin_font == NULL)
   {
      fprintf(stderr, "cannot create the offscreen bitmap or font\n");
      return 1;
   }

   UNITY_BEGIN();
   PKGC_RUN(test_insert_appends_one_selected_default_object);
   PKGC_RUN(test_copy_duplicates_selected_objects_with_their_paths);
   PKGC_RUN(test_copy_past_the_object_capacity_grows_and_keeps_everything);
   PKGC_RUN(test_delete_removes_selected_objects_and_keeps_order);
   PKGC_RUN(test_delete_first_and_last_keeps_the_middle);
   PKGC_RUN(test_delete_everything_leaves_no_object);
   PKGC_RUN(test_delete_with_nothing_selected_changes_nothing);
   PKGC_RUN(test_delete_on_a_map_without_objects_creates_none);
   PKGC_RUN(test_move_offsets_selected_objects_and_clamps_to_the_map);
   PKGC_RUN(test_out_of_bounds_objects_are_counted);
   PKGC_RUN(test_drawing_order_keys_of_far_objects_stay_in_range);
   PKGC_RUN(test_delete_removes_an_out_of_bounds_object_only);
   PKGC_RUN(test_move_of_an_out_of_bounds_object_is_not_snapped_to_the_edge);
   PKGC_RUN(test_moving_an_out_of_bounds_object_back_inside_works);
   PKGC_RUN(test_move_of_an_inside_object_is_still_clamped_to_the_map);
   PKGC_RUN(test_move_undo_restores_an_out_of_bounds_object);
   PKGC_RUN(test_the_foot_of_an_out_of_bounds_object_can_be_picked_with_the_mouse);
   PKGC_RUN(test_out_of_bounds_objects_survive_edits_and_reload);
   PKGC_RUN(test_object_undo_restores_a_delete);
   PKGC_RUN(test_object_undo_restores_a_copy_and_move);
   PKGC_RUN(test_object_undo_without_a_snapshot_does_nothing);
   PKGC_RUN(test_object_undo_is_per_map);
   PKGC_RUN(test_path_all_new_then_ok_replaces_the_points);
   PKGC_RUN(test_path_all_new_then_cancel_restores_the_points);
   PKGC_RUN(test_path_points_stop_at_the_editor_maximum);
   PKGC_RUN(test_clicks_on_the_path_windows_do_not_add_points);
   PKGC_RUN(test_saved_object_edits_survive_reload);
   PKGC_RUN(test_saved_path_edits_survive_reload);
   {
      int rc = UNITY_END();
      al_destroy_font(builtin_font);
      al_destroy_bitmap(offscreen);
      return rc;
   }
}
