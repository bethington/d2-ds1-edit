#ifndef _MAP_SAVE_UI_H_
#define _MAP_SAVE_UI_H_

// Save one map. A real save with no project open asks for one first, and a
// failure shows why. Returns 0 if saved, -1 if not. Edits stay in memory.
int map_save_ui_save(int ds1_idx, int is_tmp_file);

// Save every map with unsaved edits. Returns 0 if all saved, -1 otherwise.
int map_save_ui_save_dirty(void);

// Ask what to do about unsaved maps before `what` happens, for example
// "\nOpening this map closes the open maps. What do you want to do ?".
// Returns 1 to go ahead (nothing unsaved, saved, or discarded) and 0 to stop.
int map_save_ui_resolve_unsaved(const char *what);

// Stops the area browser from unloading maps with unsaved edits.
int map_save_ui_unload_guard(void);

#endif
