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

/* 1 if File column `slot` of this Def's row names this map, matched as above. */
int lvlprest_slot_names_file(char * buf, int def, int slot, const char * ds1_path);

/* 1 if a File column of this Def's row holds this whole path, without regard to
   case or slash style. Unlike the two above, another folder's map of the same
   name doesn't count. */
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
   `project` doesn't. A Def with no row in `project` is skipped. */
void lvlprest_each_cleared(char * game, char * project, LVLPREST_ROW_CB cb,
                           void * ctx);

#endif
