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

#endif
