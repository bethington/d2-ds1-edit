/* Creating, registering and backing up maps in a project folder. Everything
 * takes explicit paths and reports problems as a sentence in `error`, so it can
 * run without a display or a loaded game. Functions return 0, or -1 with
 * nothing changed unless a comment says otherwise.
 *
 * `seed` is the LvlPrest.txt text to start from if the project has no copy yet.
 */
#ifndef DS1EDIT_MAP_OPS_H
#define DS1EDIT_MAP_OPS_H

/* The longest table path the game takes. It builds "DATA\GLOBAL\TILES\<cell>" with
 * wsprintf in a 60-byte stack buffer. */
#define MAP_OPS_TABLE_PATH_MAX 41
#define MAP_OPS_SIZE_MAX 1000

int map_ops_check_name(const char * name, char * out_file, int out_cap,
                       char * error, int cap);
int map_ops_check_size(int width, int height, char * error, int cap);

/* Where the project's table is: found in any case, under Data/Global/Excel then
 * Global/Excel as the editor reads it. With none, where a new one goes,
 * Global/Excel. */
int map_ops_table_path(const char * project_dir, char * out, int cap);

/* Where a new map goes, always under Global/Tiles. -1 if ds1_path has a ".." or is
 * rooted, if it's too long, or if that's in the game's folder. */
int map_ops_tile_path(const char * project_dir, const char * ds1_path,
                      char * out, int cap);

/* 1 if the project has this map, in any case, under Data/Global/Tiles or
 * Global/Tiles, with its real path in out. 0 if not, or if ds1_path is unsafe as
 * above. */
int map_ops_find_tile(const char * project_dir, const char * ds1_path,
                      char * out, int cap);

/* A whole file as malloc'd text with a NUL after *out_len bytes, or NULL with
 * *out_len 0 and errno saying why: ENOENT only if it isn't there. */
char * map_ops_read_file(const char * path, long * out_len);

/* If a crash left the table only as <table>.old, move it back. 0 if there's
 * nothing to do or it worked. */
int map_ops_recover_table(const char * project_dir);

/* The project's table, recovered first if needed, as map_ops_read_file reads it. */
char * map_ops_read_table(const char * project_dir, long * out_len);

int map_ops_write_empty_ds1(const char * path, int width, int height, int act);

/* May a map be added to, or taken from, this row of this table? 0, or -1 with a
 * sentence. Rows whose Files doesn't match their cells choose maps by their own rules,
 * and the game crashes on some edits to them. */
int map_ops_check_register(char * table, int def, char * error, int cap);
int map_ops_check_unregister(char * table, int def, const char * ds1_path, char * error, int cap);

/* Add ds1_path to this Def's row as its last map and raise Files. */
int map_ops_register(const char * project_dir, const char * seed, int def,
                     const char * ds1_path, char * error, int cap);
/* Take out every copy of ds1_path's whole path from the row, not another folder's map
 * of the same name, closing the gap and lowering Files. A map the row doesn't list is
 * not an error. */
int map_ops_unregister(const char * project_dir, const char * seed, int def,
                       const char * ds1_path, char * error, int cap);
/* Put back the copies of ds1_path's whole path the game lists, each at its own File
 * column or at the end if the row is shorter, raising Files. */
int map_ops_restore_slot(const char * project_dir, const char * seed, char * game_table,
                         int def, const char * ds1_path, char * error, int cap);

/* Write an empty map and register it. If registering fails, the map is removed. */
int map_ops_create_empty(const char * project_dir, const char * seed, int def,
                         const char * ds1_path, int width, int height, int act,
                         char * error, int cap);

/* Move the project's copy of ds1_path to backup/<date>_<label>_<name>[_N]/, with
 * <name>.json holding `json`. Sets out_ds1 to the backup's path. The original is
 * removed last, after the copy and the JSON are in place. */
int map_ops_backup_file(const char * project_dir, const char * ds1_path,
                        const char * label, const char * date, const char * json,
                        char * out_ds1, int out_cap, char * error, int cap);

/* Move a backup to the project's copy of ds1_path. Refuses to replace a file. If
 * the map is back but the backup can't be removed, that's a warning: it returns 0
 * with a sentence in `error`, which is left alone otherwise. */
int map_ops_restore_file(const char * project_dir, const char * backup_ds1,
                         const char * ds1_path, char * error, int cap);

#endif
