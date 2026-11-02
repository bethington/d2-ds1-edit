#ifndef _MAP_STATE_H_
#define _MAP_STATE_H_

/* Unsaved-edit tracking. A map is dirty when its saved tile content (the
 * fields the DS1 writer emits, not selection flags) differs from the content
 * at the last save, or when another kind of edit is unsaved. Undoing back to
 * the saved content makes the map clean again. All fields
 * start at zero, which is the state of a freshly loaded map. */
void map_dirty_mark(int ds1_idx);               // an edit with no undo step
void map_dirty_before_tile_edit(int ds1_idx);   // a new tile undo buffer is starting
void map_dirty_after_tile_change(int ds1_idx);  // an undo buffer was closed or applied
void map_dirty_before_object_edit(int ds1_idx); // the one-step object undo is being set
void map_dirty_after_object_undo(int ds1_idx);  // that undo was applied
void map_dirty_mark_saved(int ds1_idx);         // a real save reached the disk
int  map_is_dirty(int ds1_idx);
int  map_any_dirty(void);                       // any loaded map

/* Where a map stands relative to the game. Derived, never stored on disk. */
typedef enum
{
   MAP_STATE_GAME,      // in the game's MPQs, with no project copy
   MAP_STATE_MODIFIED,  // in the game's MPQs, with a project copy at that path
   MAP_STATE_NEW,       // only in the project
   MAP_STATE_REMOVED    // in the game, but its slot is cleared in the project
} MAP_STATE_E;

MAP_STATE_E map_state_classify(int in_game, int in_project, int slot_cleared);

// The browser's letter for a state; "" for one that needs none.
const char *map_state_mark(MAP_STATE_E state);

// Does an open map's name refer to this table path? A saved map's name can have
// the project folder in front. Compares case-insensitively, either slash style.
int map_state_path_matches(const char *map_name, const char *ds1_path);

#endif
