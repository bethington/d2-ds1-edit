#ifndef _DS1_MANAGER_H_
#define _DS1_MANAGER_H_

/* Map operations on the open project. Each returns 0, or -1 with a sentence in
 * `error`. area_browser_rescan_backups can re-sort groups, so a caller must not
 * hold a group_idx across ds1_manager_delete or ds1_manager_revert. */
int  ds1_manager_delete(int group_idx, int entry_idx, char * error, int cap);        /* New */
int  ds1_manager_revert(int group_idx, int entry_idx, char * error, int cap);        /* Modified */
/* May this row be deleted, or removed? 0, or -1 with a sentence, with nothing changed,
 * so the caller can say why before asking to confirm. The operations check again. */
int  ds1_manager_delete_check(int group_idx, int entry_idx, char * error, int cap);
int  ds1_manager_remove_check(int group_idx, int entry_idx, char * error, int cap);
int  ds1_manager_remove_slot(int group_idx, int entry_idx, char * error, int cap);   /* Game */
int  ds1_manager_restore_slot(int group_idx, int entry_idx, char * error, int cap);  /* Removed */

/* Backup system */
/* A backup row. *row_group and *row_entry say which regular row the map has now,
 * or -1 if none could be made. */
int  ds1_manager_restore(int group_idx, int entry_idx, int * row_group, int * row_entry,
                         char * error, int cap);
int  ds1_manager_delete_permanent(int group_idx, int entry_idx);

/* DS1 creation */
int  ds1_manager_create_empty(int group_idx, int def, const char * file_name,
                              int width, int height, char * error, int cap);
/* Refuses with a sentence a map with a warp cell (map_ops_wall_cell_is_row_warp) cloned
 * to a Def other than its own row's, or whose own row isn't known. */
int  ds1_manager_clone(int src_ds1_idx, int group_idx, int def, const char * file_name,
                       char * error, int cap);
/* The table path a new map in this area and Def would have. -1 with a sentence if
 * it can't be worked out or doesn't fit the table's File columns. */
int  ds1_manager_new_row_path(int group_idx, int def, const char * file_name,
                              char * out, int cap, char * error, int error_cap);
/* 1 if any regular row has the table path this new map would. Unlisted rows share
 * folders with the other areas, so a name free in this area can still shadow one. */
int  ds1_manager_name_in_use(int group_idx, int def, const char * file_name);
/* 1 if this Def's row in the project's table, or the editor's own if the project has
 * none, may take a map: map_ops_check_new_empty for an empty map, or else
 * map_ops_check_register. 0 if not, with a sentence short enough for the dialog's status
 * row. 1 if neither table can be read, since creating the map will then say why. */
int  ds1_manager_def_can_take_map(int def, int empty_map, char * error, int cap);
/* Sets *width and *height to a new map's size for this Def where its row says. See
 * map_ops_default_size. */
void ds1_manager_def_default_size(int def, int * width, int * height);
/* 1 if a map of this size may go in this Def's row, else 0 with a sentence. 1 if the
 * table can't be read, since creating the map will then say why. See
 * map_ops_check_row_size. */
int  ds1_manager_def_takes_size(int def, int width, int height, char * error, int cap);

/* Smart filename generation — finds gap in numbering sequence */
void ds1_manager_suggest_name(const char * base_ds1_path,
                               const AREA_GROUP_S * group,
                               char * out_name, int out_size);

/* Generic TXT cell editor */
int  ds1_manager_txt_set_cell(RQ_ENUM txt_type, const char * key_col, int key_val,
                               const char * target_col, const char * new_value);

#endif
