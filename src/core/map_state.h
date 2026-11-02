#ifndef _MAP_STATE_H_
#define _MAP_STATE_H_

/* Unsaved-edit tracking. A map is dirty when its tile undo position differs
 * from the position at the last save, or when another kind of edit is unsaved.
 * Undoing back to the saved position makes the map clean again. All fields
 * start at zero, which is the state of a freshly loaded map. */
void map_dirty_mark(int ds1_idx);               // an edit with no undo step
void map_dirty_before_tile_edit(int ds1_idx);   // a new tile undo buffer is starting
void map_dirty_before_object_edit(int ds1_idx); // the one-step object undo is being set
void map_dirty_after_object_undo(int ds1_idx);  // that undo was applied
void map_dirty_mark_saved(int ds1_idx);         // a real save reached the disk
int  map_is_dirty(int ds1_idx);
int  map_any_dirty(void);                       // any loaded map

#endif
