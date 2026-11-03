#ifndef _DS1_MANAGER_H_
#define _DS1_MANAGER_H_

/* Map operations on the open project. Each returns 0, or -1 with a sentence in
 * `error`. area_browser_rescan_backups can re-sort groups, so a caller must not
 * hold a group_idx across ds1_manager_delete or ds1_manager_revert. */
int  ds1_manager_delete(int group_idx, int entry_idx, char * error, int cap);        /* New */
int  ds1_manager_revert(int group_idx, int entry_idx, char * error, int cap);        /* Modified */
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
int  ds1_manager_clone(int src_ds1_idx, int group_idx, int def, const char * file_name,
                       char * error, int cap);
/* The table path a new map in this area and Def would have. -1 with a sentence if
 * it can't be worked out or doesn't fit the table's File columns. */
int  ds1_manager_new_row_path(int group_idx, int def, const char * file_name,
                              char * out, int cap, char * error, int error_cap);
/* 1 if any regular row has the table path this new map would. Unlisted rows share
 * folders with the other areas, so a name free in this area can still shadow one. */
int  ds1_manager_name_in_use(int group_idx, int def, const char * file_name);
/* 1 if the project's table, or the editor's own if the project has none, has a
 * free File slot on this Def's row. 0 if its six are used or the Def isn't there. */
int  ds1_manager_def_has_free_slot(int def);

/* Smart filename generation — finds gap in numbering sequence */
void ds1_manager_suggest_name(const char * base_ds1_path,
                               const AREA_GROUP_S * group,
                               char * out_name, int out_size);

/* Generic TXT cell editor */
int  ds1_manager_txt_set_cell(RQ_ENUM txt_type, const char * key_col, int key_val,
                               const char * target_col, const char * new_value);

#endif
