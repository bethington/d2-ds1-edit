/* Reading LvlPrest.txt as the raw tab-separated text that gets written back,
 * rather than the parsed table in glb_ds1edit.lvlprest_buff: that one can come
 * from an MPQ the editor can't update, and every edit drops it until the next
 * map load.
 */
#ifndef DS1EDIT_LVLPREST_TEXT_H
#define DS1EDIT_LVLPREST_TEXT_H

#define LVLPREST_TABLE_UNUSABLE  -1

/* The 0-based index of a named column in the header line, or -1. */
int lvlprest_find_col(const char * header_line, const char * col_name);

/* The start of the row with this Def, or NULL. *row_end is set past its
   newline. */
char * lvlprest_find_row(char * buf, int def_col, int target_def, char ** row_end);

/* Which File column (1-6) of this Def's row names this map, matched by file
   name without regard to case. 0 if the table doesn't list it there.
   LVLPREST_TABLE_UNUSABLE if the Def or File columns are missing. */
int lvlprest_find_file_slot(char * buf, int def, const char * ds1_path);

/* 1 if File column `slot` of this Def's row holds this whole path, without regard
   to case or slash style. Another folder's map of the same name doesn't count. */
int lvlprest_slot_holds_path(char * buf, int def, int slot, const char * ds1_path);

/* The first File column (1-6) of this Def's row holding this whole path. 0 if
   none. LVLPREST_TABLE_UNUSABLE as above. */
int lvlprest_find_path_slot(char * buf, int def, const char * ds1_path);

/* 1 if a File column of this Def's row holds this whole path. */
int lvlprest_lists_path(char * buf, int def, const char * ds1_path);

/* The first File column (1-6) of this Def's row that is blank or "0". 0 if all
   six are used or the Def isn't there. LVLPREST_TABLE_UNUSABLE as above. */
int lvlprest_empty_slot(char * buf, int def);

/* Copy File column `slot` of this Def's row. 1 if copied, 0 if there's no such
   cell or it doesn't fit. */
int lvlprest_get_cell(char * buf, int def, int slot, char * out, int cap);

/* The table with one File cell replaced, every other byte as it was. New
   malloc'd text with a NUL after *out_len bytes; NULL if there's no such cell. */
char * lvlprest_with_cell(char * buf, int def, int slot, const char * value,
                          long * out_len);

typedef void (* LVLPREST_ROW_CB)(int def, const char * ds1_path, void * ctx);

/* Calls cb for each file a row lists. */
void lvlprest_each_file(char * table, LVLPREST_ROW_CB cb, void * ctx);

/* Calls cb for each file a row of `game` lists that the same Def's row in
   `project` doesn't, by whole path. A Def with no row in `project` is skipped. */
void lvlprest_each_cleared(char * game, char * project, LVLPREST_ROW_CB cb,
                           void * ctx);

/* The row's Files column, the count the game's random pick uses. -1 if there's no
   such row or column. */
int lvlprest_files_value(char * buf, int def);

/* 1 if Files is 1 to 6, File1 to File<Files> are filled and the rest are empty.
   The game's random pick reads these cells. It indexes a cell blindly, so a blank
   below Files crashes it, and rows that aren't simple choose by their own rules. */
int lvlprest_row_is_simple(char * buf, int def);

/* The table with a map added to a simple row, at `slot` or at the end if that is
   past the last map, later cells moved along and Files raised. New malloc'd text
   with a NUL after *out_len bytes; NULL if the row isn't simple or is full. */
char * lvlprest_with_map_inserted(char * buf, int def, int slot, const char * value,
                                  long * out_len);

/* The table with File column `slot` taken out of a simple row, later cells moved
   back and Files lowered. NULL if the row isn't simple, the slot holds no map, or
   it is the row's only map. */
char * lvlprest_with_map_removed(char * buf, int def, int slot, long * out_len);

#endif
