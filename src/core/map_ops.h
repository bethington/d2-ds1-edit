/* Creating, registering and backing up maps in a project folder. Everything
 * takes explicit paths and reports problems as a sentence in `error`, so it can
 * run without a display or a loaded game. Functions return 0, or -1 with
 * nothing changed unless a comment says otherwise.
 *
 * `seed` is the LvlPrest.txt text to start from if the project has no copy yet.
 */
#ifndef DS1EDIT_MAP_OPS_H
#define DS1EDIT_MAP_OPS_H

/* The longest table path that fits the game's File columns. */
#define MAP_OPS_TABLE_PATH_MAX 59
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

/* Put ds1_path in this Def's first empty File column. */
int map_ops_register(const char * project_dir, const char * seed, int def,
                     const char * ds1_path, char * error, int cap);
/* Clear every column of the row holding ds1_path's whole path, not another folder's
 * map of the same name. A map the row doesn't list is not an error. */
int map_ops_unregister(const char * project_dir, const char * seed, int def,
                       const char * ds1_path, char * error, int cap);
/* Put the game's cells holding ds1_path's whole path back: each in its own column,
 * or the first empty one if that was taken. */
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
