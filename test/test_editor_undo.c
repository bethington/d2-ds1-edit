/*
 * Real tile editing and undo: edittile_delete_selected_tiles() and the undo
 * buffer files written and replayed by src/editor/undo.c.
 *
 * Nothing in the edit/undo path is stubbed (unlike test_editor_selection,
 * which replaces the undo functions). Maps are small in-memory fixtures
 * (test/support/editor_test_state); undo buffers are written to the owned
 * temp directory the fixture installs as the editor's tmp dir.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "editor/tiles.h"
#include "editor/undo.h"
#include "core/ds1.h"
#include "core/ds1_save_io.h"

#include "support/editor_test_state.h"

void setUp(void) {}

void tearDown(void)
{
   editor_test_state_destroy();
}

/* ---- helpers ---- */

static CELL_F_S *F(int m, int x, int y, int l)
{
   return glb_ds1[m].floor_buff + y * glb_ds1[m].floor_line + x * glb_ds1[m].floor_num + l;
}
static CELL_W_S *W(int m, int x, int y, int l)
{
   return glb_ds1[m].wall_buff + y * glb_ds1[m].wall_line + x * glb_ds1[m].wall_num + l;
}
static CELL_S_S *S(int m, int x, int y)
{
   return glb_ds1[m].shadow_buff + y * glb_ds1[m].shadow_line + x * glb_ds1[m].shadow_num;
}

/* Distinct nonzero content in every layer of map m. */
static void fill_map(int m)
{
   int x, y, l, c;
   DS1_S *d = &glb_ds1[m];
   for (y = 0; y < d->height; y++)
      for (x = 0; x < d->width; x++)
      {
         c = y * d->width + x;
         for (l = 0; l < d->floor_num; l++)
         {
            CELL_F_S *f = F(m, x, y, l);
            f->prop1 = (UBYTE) (1 + m * 7 + c);
            f->prop2 = (UBYTE) (2 + l * 3 + c);
            f->prop3 = (UBYTE) (3 + m + l);
            f->prop4 = (UBYTE) (4 + c % 5);
            f->bt_idx = 1000 * (m + 1) + c * 10 + l;
            f->flags = 0;
         }
         for (l = 0; l < d->wall_num; l++)
         {
            CELL_W_S *w = W(m, x, y, l);
            w->prop1 = (UBYTE) (101 + m * 7 + c);
            w->prop2 = (UBYTE) (102 + l + c);
            w->prop3 = (UBYTE) (103 + m);
            w->prop4 = (UBYTE) (104 + l);
            w->orientation = (UBYTE) (1 + (c + l) % 20);
            w->bt_idx = 2000 * (m + 1) + c * 10 + l;
            w->flags = 0;
         }
         {
            CELL_S_S *s = S(m, x, y);
            s->prop1 = (UBYTE) (201 + c);
            s->prop2 = (UBYTE) (202 + m);
            s->prop3 = 203;
            s->prop4 = 204;
            s->bt_idx = 3000 * (m + 1) + c;
            s->flags = 0;
         }
      }
}

/* Full copy of the cell buffers of map m. */
typedef struct MAPSNAP
{
   CELL_F_S *f;
   CELL_W_S *w;
   CELL_S_S *s;
   int nf, nw, ns;
} MAPSNAP;

static void take(MAPSNAP *sn, int m)
{
   DS1_S *d = &glb_ds1[m];
   sn->nf = d->floor_len;
   sn->nw = d->wall_len;
   sn->ns = d->shadow_len;
   sn->f = (CELL_F_S *) malloc(sizeof(CELL_F_S) * sn->nf);
   sn->w = (CELL_W_S *) malloc(sizeof(CELL_W_S) * sn->nw);
   sn->s = (CELL_S_S *) malloc(sizeof(CELL_S_S) * sn->ns);
   memcpy(sn->f, d->floor_buff, sizeof(CELL_F_S) * sn->nf);
   memcpy(sn->w, d->wall_buff, sizeof(CELL_W_S) * sn->nw);
   memcpy(sn->s, d->shadow_buff, sizeof(CELL_S_S) * sn->ns);
}

static void drop(MAPSNAP *sn)
{
   free(sn->f);
   free(sn->w);
   free(sn->s);
}

/* Compare field by field (struct padding is not part of the state). */
static void expect_same(const MAPSNAP *sn, int m)
{
   DS1_S *d = &glb_ds1[m];
   int i;
   char msg[96];
   for (i = 0; i < sn->nf; i++)
   {
      const CELL_F_S *a = &sn->f[i], *b = &d->floor_buff[i];
      snprintf(msg, sizeof(msg), "map %d floor cell-layer %d", m, i);
      TEST_ASSERT_TRUE_MESSAGE(a->prop1 == b->prop1 && a->prop2 == b->prop2 &&
                               a->prop3 == b->prop3 && a->prop4 == b->prop4 &&
                               a->bt_idx == b->bt_idx && a->flags == b->flags, msg);
   }
   for (i = 0; i < sn->nw; i++)
   {
      const CELL_W_S *a = &sn->w[i], *b = &d->wall_buff[i];
      snprintf(msg, sizeof(msg), "map %d wall cell-layer %d", m, i);
      TEST_ASSERT_TRUE_MESSAGE(a->prop1 == b->prop1 && a->prop2 == b->prop2 &&
                               a->prop3 == b->prop3 && a->prop4 == b->prop4 &&
                               a->orientation == b->orientation &&
                               a->bt_idx == b->bt_idx && a->flags == b->flags, msg);
   }
   for (i = 0; i < sn->ns; i++)
   {
      const CELL_S_S *a = &sn->s[i], *b = &d->shadow_buff[i];
      snprintf(msg, sizeof(msg), "map %d shadow cell %d", m, i);
      TEST_ASSERT_TRUE_MESSAGE(a->prop1 == b->prop1 && a->prop2 == b->prop2 &&
                               a->prop3 == b->prop3 && a->prop4 == b->prop4 &&
                               a->bt_idx == b->bt_idx && a->flags == b->flags, msg);
   }
}

static void select_cell(int m, int x, int y)
{
   int l;
   for (l = 0; l < glb_ds1[m].floor_num; l++) SET_SELECTED(F(m, x, y, l)->flags);
   for (l = 0; l < glb_ds1[m].wall_num; l++)  SET_SELECTED(W(m, x, y, l)->flags);
   SET_SELECTED(S(m, x, y)->flags);
}

static void expect_cell_cleared(int m, int x, int y)
{
   int l;
   for (l = 0; l < glb_ds1[m].floor_num; l++)
   {
      TEST_ASSERT_EQUAL_UINT8(0, F(m, x, y, l)->prop1);
      TEST_ASSERT_EQUAL_INT(0, F(m, x, y, l)->bt_idx);
      TEST_ASSERT_EQUAL_UINT8(0, F(m, x, y, l)->flags);
   }
   for (l = 0; l < glb_ds1[m].wall_num; l++)
   {
      TEST_ASSERT_EQUAL_UINT8(0, W(m, x, y, l)->prop1);
      TEST_ASSERT_EQUAL_UINT8(0, W(m, x, y, l)->orientation);
   }
   TEST_ASSERT_EQUAL_UINT8(0, S(m, x, y)->prop1);
}

static void undo(int m)
{
   int trapped = 0;
   EDITOR_TEST_TRAPS_ERROR(undo_apply_tile_buffer(m), trapped);
   TEST_ASSERT_FALSE_MESSAGE(trapped, editor_test_last_error);
}

static void delete_selected(int m)
{
   int trapped = 0;
   EDITOR_TEST_TRAPS_ERROR(edittile_delete_selected_tiles(m), trapped);
   TEST_ASSERT_FALSE_MESSAGE(trapped, editor_test_last_error);
}

/* ---- tests ---- */

void test_delete_then_undo_restores_the_cell_and_leaves_neighbours(void)
{
   MAPSNAP before, other;
   TEST_ASSERT_TRUE(editor_test_state_init(2, 3, 2));
   fill_map(0);
   fill_map(1);
   select_cell(0, 1, 0);
   take(&before, 0);
   take(&other, 1);

   delete_selected(0);
   expect_cell_cleared(0, 1, 0);
   TEST_ASSERT_EQUAL_UINT8(1 + 0 + 0, F(0, 0, 0, 0)->prop1);   /* neighbour kept */
   TEST_ASSERT_EQUAL_UINT8(1 + 0 + 2, F(0, 2, 0, 0)->prop1);
   TEST_ASSERT_EQUAL_UINT8(1 + 0 + 4, F(0, 1, 1, 0)->prop1);

   undo(0);
   expect_same(&before, 0);     /* props, orientation, bt_idx, selection */
   expect_same(&other, 1);
   drop(&before);
   drop(&other);
}

void test_two_edits_undo_in_reverse_order_then_stop(void)
{
   MAPSNAP s0, s1;
   TEST_ASSERT_TRUE(editor_test_state_init(1, 3, 2));
   fill_map(0);
   take(&s0, 0);

   select_cell(0, 0, 0);
   delete_selected(0);
   select_cell(0, 2, 1);
   take(&s1, 0);
   delete_selected(0);
   expect_cell_cleared(0, 2, 1);

   undo(0);
   expect_same(&s1, 0);
   undo(0);
   /* (2,1) was selected before the second edit only; the first undo put
    * that selection back, the second undo restores the first edit. */
   TEST_ASSERT_EQUAL_UINT8(s0.f[0].prop1, F(0, 0, 0, 0)->prop1);
   TEST_ASSERT_EQUAL_INT(s0.f[0].bt_idx, F(0, 0, 0, 0)->bt_idx);
   TEST_ASSERT_TRUE(IS_SELECTED(F(0, 0, 0, 0)->flags));
   TEST_ASSERT_TRUE(IS_SELECTED(F(0, 2, 1, 0)->flags));

   /* nothing left to undo: a third undo is a no-op */
   take(&s1, 0);
   undo(0);
   expect_same(&s1, 0);
   TEST_ASSERT_EQUAL_INT(-1, glb_ds1[0].undo.cur_buf_num);
   drop(&s0);
   drop(&s1);
}

void test_undo_with_no_history_and_empty_delete_are_no_ops(void)
{
   MAPSNAP s0;
   TEST_ASSERT_TRUE(editor_test_state_init(1, 3, 2));
   fill_map(0);
   take(&s0, 0);
   undo(0);                    /* no buffer yet */
   expect_same(&s0, 0);
   delete_selected(0);         /* nothing selected */
   expect_same(&s0, 0);
   undo(0);
   expect_same(&s0, 0);
   drop(&s0);
}

void test_hidden_layers_are_deleted_and_restored_like_visible_ones(void)
{
   MAPSNAP s0;
   TEST_ASSERT_TRUE(editor_test_state_init(1, 3, 2));
   fill_map(0);
   glb_ds1[0].wall_layer_mask[1] = 0;
   glb_ds1[0].floor_layer_mask[1] = 0;
   SET_SELECTED(W(0, 1, 1, 1)->flags);      /* hidden wall only */
   SET_SELECTED(F(0, 0, 1, 0)->flags);      /* visible floor only */
   take(&s0, 0);
   delete_selected(0);
   TEST_ASSERT_EQUAL_UINT8(0, W(0, 1, 1, 1)->prop1);
   TEST_ASSERT_EQUAL_UINT8(s0.w[(1 * 3 + 1) * 2 + 0].prop1, W(0, 1, 1, 0)->prop1);
   TEST_ASSERT_EQUAL_UINT8(0, F(0, 0, 1, 0)->prop1);
   TEST_ASSERT_EQUAL_UINT8(s0.f[(1 * 3 + 0) * 2 + 1].prop1, F(0, 0, 1, 1)->prop1);
   undo(0);
   expect_same(&s0, 0);
   drop(&s0);
}

void test_segments_break_at_gaps_and_row_ends(void)
{
   MAPSNAP s0;
   TEST_ASSERT_TRUE(editor_test_state_init(1, 3, 2));
   fill_map(0);
   select_cell(0, 0, 0);
   select_cell(0, 2, 0);    /* gap at (1,0) */
   select_cell(0, 0, 1);    /* next row */
   select_cell(0, 1, 1);
   take(&s0, 0);
   delete_selected(0);
   expect_cell_cleared(0, 0, 0);
   expect_cell_cleared(0, 2, 0);
   expect_cell_cleared(0, 1, 1);
   TEST_ASSERT_EQUAL_UINT8(s0.f[(0 * 3 + 1) * 2].prop1, F(0, 1, 0, 0)->prop1);
   undo(0);
   expect_same(&s0, 0);
   drop(&s0);
}

void test_undo_of_more_than_255_segments_restores_every_cell(void)
{
   /* A scattered selection (every other cell) makes one undo segment per
    * cell: 32 x 32 / 2 = 512 segments in a single edit. */
   MAPSNAP s0;
   int x, y;
   TEST_ASSERT_TRUE(editor_test_state_init(1, 32, 32));
   fill_map(0);
   for (y = 0; y < 32; y++)
      for (x = (y & 1); x < 32; x += 2)
         select_cell(0, x, y);
   take(&s0, 0);
   delete_selected(0);
   expect_cell_cleared(0, 31, 31);
   undo(0);
   expect_same(&s0, 0);
   drop(&s0);
}

void test_undo_on_a_map_wider_than_256_cells(void)
{
   MAPSNAP s0;
   TEST_ASSERT_TRUE(editor_test_state_init(1, 300, 2));
   fill_map(0);
   select_cell(0, 280, 1);
   select_cell(0, 24, 1);   /* 280 & 0xFF */
   take(&s0, 0);
   delete_selected(0);
   expect_cell_cleared(0, 280, 1);
   undo(0);
   expect_same(&s0, 0);
   drop(&s0);
}

void test_a_run_longer_than_255_cells_is_restored(void)
{
   MAPSNAP s0;
   int x;
   TEST_ASSERT_TRUE(editor_test_state_init(1, 300, 1));
   fill_map(0);
   for (x = 0; x < 300; x++)
      select_cell(0, x, 0);
   take(&s0, 0);
   delete_selected(0);
   undo(0);
   expect_same(&s0, 0);
   drop(&s0);
}

void test_map_slots_keep_independent_histories(void)
{
   MAPSNAP a0, b0, a1;
   TEST_ASSERT_TRUE(editor_test_state_init(2, 3, 2));
   fill_map(0);
   fill_map(1);
   select_cell(0, 0, 0);
   select_cell(1, 2, 1);
   take(&a0, 0);
   take(&b0, 1);

   delete_selected(0);
   delete_selected(1);
   take(&a1, 0);

   undo(1);                 /* only map 1 changes */
   expect_same(&a1, 0);
   expect_same(&b0, 1);
   TEST_ASSERT_EQUAL_INT(0, glb_ds1[0].undo.cur_buf_num);

   undo(0);
   expect_same(&a0, 0);
   expect_same(&b0, 1);
   drop(&a0);
   drop(&b0);
   drop(&a1);
}

void test_a_new_edit_after_undo_discards_the_undone_branch(void)
{
   MAPSNAP s0, s1, s2;
   TEST_ASSERT_TRUE(editor_test_state_init(1, 3, 2));
   fill_map(0);
   select_cell(0, 0, 0);
   take(&s0, 0);
   delete_selected(0);            /* buffer 0 */
   select_cell(0, 1, 0);
   take(&s1, 0);
   delete_selected(0);            /* buffer 1 */
   undo(0);                       /* back to s1 */
   expect_same(&s1, 0);
   select_cell(0, 2, 1);
   take(&s2, 0);
   delete_selected(0);            /* replaces buffer 1 */
   TEST_ASSERT_EQUAL_INT(2, glb_ds1[0].undo.buf_num);
   undo(0);
   expect_same(&s2, 0);
   undo(0);
   /* buffer 0 restores (0,0); (1,0) and (2,1) keep their s2 state */
   TEST_ASSERT_EQUAL_UINT8(s0.f[0].prop1, F(0, 0, 0, 0)->prop1);
   TEST_ASSERT_TRUE(IS_SELECTED(F(0, 0, 0, 0)->flags));
   TEST_ASSERT_EQUAL_UINT8(s2.f[2].prop1, F(0, 1, 0, 0)->prop1);
   TEST_ASSERT_EQUAL_UINT8(s2.f[10].prop1, F(0, 2, 1, 0)->prop1);
   drop(&s0);
   drop(&s1);
   drop(&s2);
}

/* ---- edited maps through the checked writer and the real reader ---- */

static void save_and_reload_into(int m, int slot)
{
   char err[512];
   int trapped = 0, rc = -1;
   EDITOR_TEST_TRAPS_ERROR(rc = ds1_save_checked(m, FALSE, err, sizeof(err)), trapped);
   TEST_ASSERT_FALSE(trapped);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, err);
   TEST_ASSERT_EQUAL_INT_MESSAGE(0, ds1_read_checked(glb_ds1[m].name, slot, 0, 0, err, sizeof(err)), err);
}

/* Stored fields only: bt_idx and flags are editor state, not file content. */
static void expect_same_file_content(int a, int b)
{
   int x, y, l;
   DS1_S *A = &glb_ds1[a], *B = &glb_ds1[b];
   TEST_ASSERT_EQUAL_INT(A->width, B->width);
   TEST_ASSERT_EQUAL_INT(A->height, B->height);
   TEST_ASSERT_EQUAL_INT(A->wall_num, B->wall_num);
   TEST_ASSERT_EQUAL_INT(A->floor_num, B->floor_num);
   for (y = 0; y < A->height; y++)
      for (x = 0; x < A->width; x++)
      {
         for (l = 0; l < A->floor_num; l++)
         {
            CELL_F_S *p = F(a, x, y, l), *q = F(b, x, y, l);
            TEST_ASSERT_TRUE(p->prop1 == q->prop1 && p->prop2 == q->prop2 &&
                             p->prop3 == q->prop3 && p->prop4 == q->prop4);
         }
         for (l = 0; l < A->wall_num; l++)
         {
            CELL_W_S *p = W(a, x, y, l), *q = W(b, x, y, l);
            TEST_ASSERT_TRUE(p->prop1 == q->prop1 && p->prop2 == q->prop2 &&
                             p->prop3 == q->prop3 && p->prop4 == q->prop4 &&
                             p->orientation == q->orientation);
         }
         {
            CELL_S_S *p = S(a, x, y), *q = S(b, x, y);
            TEST_ASSERT_TRUE(p->prop1 == q->prop1 && p->prop2 == q->prop2 &&
                             p->prop3 == q->prop3 && p->prop4 == q->prop4);
         }
      }
}

void test_deleted_and_undone_edits_survive_save_and_reload(void)
{
   TEST_ASSERT_TRUE(editor_test_state_init(1, 3, 2));
   fill_map(0);
   select_cell(0, 1, 1);
   delete_selected(0);
   save_and_reload_into(0, 1);          /* the deletion is on disk */
   expect_same_file_content(0, 1);
   TEST_ASSERT_EQUAL_UINT8(0, F(1, 1, 1, 0)->prop1);
   TEST_ASSERT_EQUAL_UINT8(0, W(1, 1, 1, 1)->orientation);

   undo(0);
   editor_test_state_free_slot(1);
   save_and_reload_into(0, 1);          /* and so is the undo */
   expect_same_file_content(0, 1);
   TEST_ASSERT_NOT_EQUAL(0, F(1, 1, 1, 0)->prop1);
}

int main(int argc, char **argv)
{
   pkgc_test_filter = argc > 1 ? argv[1] : NULL;
   UNITY_BEGIN();
   PKGC_RUN(test_delete_then_undo_restores_the_cell_and_leaves_neighbours);
   PKGC_RUN(test_two_edits_undo_in_reverse_order_then_stop);
   PKGC_RUN(test_undo_with_no_history_and_empty_delete_are_no_ops);
   PKGC_RUN(test_hidden_layers_are_deleted_and_restored_like_visible_ones);
   PKGC_RUN(test_segments_break_at_gaps_and_row_ends);
   PKGC_RUN(test_undo_of_more_than_255_segments_restores_every_cell);
   PKGC_RUN(test_undo_on_a_map_wider_than_256_cells);
   PKGC_RUN(test_a_run_longer_than_255_cells_is_restored);
   PKGC_RUN(test_map_slots_keep_independent_histories);
   PKGC_RUN(test_a_new_edit_after_undo_discards_the_undone_branch);
   PKGC_RUN(test_deleted_and_undone_edits_survive_save_and_reload);
   return UNITY_END();
}
