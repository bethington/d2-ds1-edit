#include "structs.h"
#include "core/map_state.h"

static int slot_has_map(int ds1_idx)
{
   return ds1_idx >= 0 && ds1_idx < DS1_MAX && glb_ds1[ds1_idx].name[0] != 0;
}

void map_dirty_mark(int ds1_idx)
{
   if (slot_has_map(ds1_idx))
      glb_ds1[ds1_idx].other_dirty = 1;
}

void map_dirty_before_tile_edit(int ds1_idx)
{
   DS1_S *d;

   if (!slot_has_map(ds1_idx)) return;
   d = &glb_ds1[ds1_idx];

   // The saved state was undone and this edit discards it for good, so a
   // later edit reaching the same depth must not count as clean.
   if (d->saved_undo_depth > d->undo.cur_buf_num + 1)
      d->saved_undo_depth = -1;
}

void map_dirty_before_object_edit(int ds1_idx)
{
   DS1_S *d;

   if (!slot_has_map(ds1_idx)) return;
   d = &glb_ds1[ds1_idx];
   d->undo_other_dirty = d->other_dirty;
   d->other_dirty = 1;
}

void map_dirty_after_object_undo(int ds1_idx)
{
   if (slot_has_map(ds1_idx))
      glb_ds1[ds1_idx].other_dirty = glb_ds1[ds1_idx].undo_other_dirty;
}

void map_dirty_mark_saved(int ds1_idx)
{
   DS1_S *d;

   if (!slot_has_map(ds1_idx)) return;
   d = &glb_ds1[ds1_idx];
   d->other_dirty = 0;
   d->undo_other_dirty = 1;   // the object undo backup now differs from disk
   d->saved_undo_depth = d->undo.cur_buf_num + 1;
}

int map_is_dirty(int ds1_idx)
{
   const DS1_S *d;

   if (!slot_has_map(ds1_idx)) return 0;
   d = &glb_ds1[ds1_idx];
   return d->other_dirty || d->saved_undo_depth != d->undo.cur_buf_num + 1;
}

int map_any_dirty(void)
{
   int i;

   for (i = 0; i < DS1_MAX; i++)
      if (map_is_dirty(i))
         return 1;
   return 0;
}
