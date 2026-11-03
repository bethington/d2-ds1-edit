#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "core/file_copy.h"
#include "core/lvlprest_text.h"
#include "core/map_ops.h"
#include "core/project.h"

#define PATH_CAP 768

static int fail(char * error, int cap, const char * fmt, ...)
{
   va_list ap;

   if (error != NULL && cap > 0)
   {
      va_start(ap, fmt);
      vsnprintf(error, (size_t) cap, fmt, ap);
      va_end(ap);
   }
   return -1;
}

static int file_exists(const char * path)
{
   FILE * f = fopen(path, "rb");

   if (f == NULL)
      return 0;
   fclose(f);
   return 1;
}

static const char * base_name(const char * path)
{
   const char * base = path, * p;

   for (p = path; *p != '\0'; p++)
      if (*p == '/' || *p == '\\')
         base = p + 1;
   return base;
}

int map_ops_check_name(const char * name, char * out_file, int out_cap,
                       char * error, int cap)
{
   static const char * const reserved[] = {
      "con", "prn", "aux", "nul",
      "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
      "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"
   };
   size_t n = strlen(name), stem, i;
   const char * dot;

   if (n == 0)
      return fail(error, cap, "The map needs a name.");
   for (i = 0; i < n; i++)
   {
      unsigned char c = (unsigned char) name[i];
      if (!isalnum(c) && c != '_' && c != '-' && c != '.')
         return fail(error, cap, "Names can use letters, digits, _ - and . only.");
   }
   if (name[0] == '.' || name[n - 1] == '.')
      return fail(error, cap, "A name can't start or end with a dot.");

   dot = strchr(name, '.');
   stem = dot != NULL ? (size_t)(dot - name) : n;
   for (i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++)
      if (strlen(reserved[i]) == stem && strncasecmp(name, reserved[i], stem) == 0)
         return fail(error, cap, "%s is a reserved name on Windows.", reserved[i]);

   {
      int has_ext = n >= 4 && strcasecmp(name + n - 4, ".ds1") == 0;
      size_t need = n + (has_ext ? 0 : 4) + 1;

      if (out_cap <= 0 || need > (size_t) out_cap)
         return fail(error, cap, "That name is too long: %d characters with .ds1 at most.",
                     out_cap - 1);
      snprintf(out_file, (size_t) out_cap, has_ext ? "%s" : "%s.ds1", name);
   }
   return 0;
}

int map_ops_check_size(int width, int height, char * error, int cap)
{
   if (width < 1 || height < 1 || width > MAP_OPS_SIZE_MAX || height > MAP_OPS_SIZE_MAX)
      return fail(error, cap, "Width and height must be 1 to %d.", MAP_OPS_SIZE_MAX);
   return 0;
}

int map_ops_table_path(const char * project_dir, char * out, int cap)
{
   int n = snprintf(out, (size_t) cap, "%s/Global/Excel/LvlPrest.txt", project_dir);
   return n > 0 && n < cap ? 0 : -1;
}

int map_ops_tile_path(const char * project_dir, const char * ds1_path, char * out, int cap)
{
   int n = snprintf(out, (size_t) cap, "%s/Global/Tiles/%s", project_dir, ds1_path);
   char * p;

   if (n <= 0 || n >= cap)
      return -1;
   for (p = out; *p != '\0'; p++)
      if (*p == '\\')
         *p = '/';
   return 0;
}

char * map_ops_read_file(const char * path, long * out_len)
{
   FILE * f = fopen(path, "rb");
   char * buf;
   long len;

   if (f == NULL)
      return NULL;
   fseek(f, 0, SEEK_END);
   len = ftell(f);
   fseek(f, 0, SEEK_SET);
   buf = (char *) malloc(len + 1);
   if (buf != NULL && fread(buf, 1, (size_t) len, f) != (size_t) len)
   {
      free(buf);
      buf = NULL;
   }
   fclose(f);
   if (buf != NULL)
   {
      buf[len] = '\0';
      *out_len = len;
   }
   return buf;
}

/* Replace a file only once the new bytes are safely beside it. */
static int write_whole_file(const char * path, const char * data, long len)
{
   char staging[PATH_CAP], old[PATH_CAP];
   FILE * f;
   int had_old;

   if (snprintf(staging, sizeof(staging), "%s.staging", path) >= (int) sizeof(staging) ||
       snprintf(old, sizeof(old), "%s.old", path) >= (int) sizeof(old))
      return -1;
   if (!project_ensure_parent_dirs(path))
      return -1;
   f = fopen(staging, "wb");
   if (f == NULL)
      return -1;
   if (fwrite(data, 1, (size_t) len, f) != (size_t) len)
   {
      fclose(f);
      remove(staging);
      return -1;
   }
   if (fclose(f) != 0)
   {
      remove(staging);
      return -1;
   }
   /* Keep the old file until the new one is in place. */
   had_old = file_exists(path);
   if (had_old)
   {
      remove(old);
      if (rename(path, old) != 0)
      {
         remove(staging);
         return -1;
      }
   }
   if (rename(staging, path) != 0)
   {
      if (had_old)
         rename(old, path);
      remove(staging);
      return -1;
   }
   if (had_old)
      remove(old);
   return 0;
}

/* Refuse a project folder that is inside the game install. */
static int check_not_in_game(const char * project_dir, char * error, int cap)
{
   if (project_path_in_game_install(project_dir))
      return fail(error, cap, "%s is inside the game install, which the editor never writes to.",
                  project_dir);
   return 0;
}

/* The project's table, or a copy of the seed in memory if there isn't one.
 * Nothing is written until save_table. */
static char * table_for_edit(const char * project_dir, const char * seed,
                             long * out_len, char * error, int cap)
{
   char path[PATH_CAP];
   char * text;

   if (map_ops_table_path(project_dir, path, sizeof(path)) != 0)
   {
      fail(error, cap, "The project's path is too long for LvlPrest.txt.");
      return NULL;
   }
   text = map_ops_read_file(path, out_len);
   if (text != NULL)
      return text;
   if (seed == NULL)
   {
      fail(error, cap, "The project has no LvlPrest.txt, and the game's couldn't be read.");
      return NULL;
   }
   *out_len = (long) strlen(seed);
   text = (char *) malloc((size_t) *out_len + 1);
   if (text == NULL)
   {
      fail(error, cap, "Out of memory reading LvlPrest.txt.");
      return NULL;
   }
   memcpy(text, seed, (size_t) *out_len + 1);
   return text;
}

static int save_table(const char * project_dir, const char * text, long len,
                      char * error, int cap)
{
   char path[PATH_CAP];

   if (map_ops_table_path(project_dir, path, sizeof(path)) != 0 ||
       write_whole_file(path, text, len) != 0)
      return fail(error, cap, "Couldn't write the project's LvlPrest.txt.");
   return 0;
}

static int set_cell_and_save(const char * project_dir, char * table, int def, int slot,
                             const char * value, char * error, int cap)
{
   long len = 0;
   char * edited = lvlprest_with_cell(table, def, slot, value, &len);
   int rc;

   if (edited == NULL)
      return fail(error, cap, "Couldn't edit Def %d's row in LvlPrest.txt.", def);
   rc = save_table(project_dir, edited, len, error, cap);
   free(edited);
   return rc;
}

int map_ops_register(const char * project_dir, const char * seed, int def,
                     const char * ds1_path, char * error, int cap)
{
   long len;
   char * table;
   int slot, rc, def_col;
   char * row_end;

   if (check_not_in_game(project_dir, error, cap) != 0)
      return -1;
   if ((int) strlen(ds1_path) > MAP_OPS_TABLE_PATH_MAX)
      return fail(error, cap, "%s is longer than the %d characters LvlPrest.txt allows.",
                  ds1_path, MAP_OPS_TABLE_PATH_MAX);

   table = table_for_edit(project_dir, seed, &len, error, cap);
   if (table == NULL)
      return -1;

   def_col = lvlprest_find_col(table, "Def");
   if (def_col < 0 || lvlprest_find_row(table, def_col, def, &row_end) == NULL)
   {
      free(table);
      return fail(error, cap, "Def %d isn't in LvlPrest.txt.", def);
   }
   slot = lvlprest_empty_slot(table, def);
   if (slot <= 0)
   {
      free(table);
      return fail(error, cap, "Def %d has no free File slot in LvlPrest.txt.", def);
   }
   rc = set_cell_and_save(project_dir, table, def, slot, ds1_path, error, cap);
   free(table);
   return rc;
}

int map_ops_unregister(const char * project_dir, const char * seed, int def,
                       const char * ds1_path, char * error, int cap)
{
   long len;
   char * table;
   int slot, rc = 0;

   if (check_not_in_game(project_dir, error, cap) != 0)
      return -1;
   table = table_for_edit(project_dir, seed, &len, error, cap);
   if (table == NULL)
      return -1;
   slot = lvlprest_find_file_slot(table, def, ds1_path);
   if (slot == LVLPREST_TABLE_UNUSABLE)
      rc = fail(error, cap, "The project's LvlPrest.txt has no Def or File columns.");
   else if (slot > 0)
      rc = set_cell_and_save(project_dir, table, def, slot, "0", error, cap);
   free(table);
   return rc;
}

int map_ops_restore_slot(const char * project_dir, const char * seed, char * game_table,
                         int def, const char * ds1_path, char * error, int cap)
{
   long len;
   char * table;
   char cell[256], current[256];
   int game_slot, slot, rc;

   if (check_not_in_game(project_dir, error, cap) != 0)
      return -1;
   game_slot = lvlprest_find_file_slot(game_table, def, ds1_path);
   if (game_slot <= 0 || !lvlprest_get_cell(game_table, def, game_slot, cell, sizeof(cell)))
      return fail(error, cap, "The game's LvlPrest.txt doesn't list %s under Def %d.",
                  ds1_path, def);

   table = table_for_edit(project_dir, seed, &len, error, cap);
   if (table == NULL)
      return -1;

   /* Already listed: nothing to put back, and a second copy would be ambiguous. */
   if (lvlprest_find_file_slot(table, def, ds1_path) > 0)
   {
      free(table);
      return 0;
   }

   slot = game_slot;
   if (!lvlprest_get_cell(table, def, slot, current, sizeof(current)) ||
       (current[0] != '\0' && strcmp(current, "0") != 0))
      slot = lvlprest_empty_slot(table, def);
   if (slot <= 0)
   {
      free(table);
      return fail(error, cap, "Def %d has no free File slot in LvlPrest.txt.", def);
   }
   rc = set_cell_and_save(project_dir, table, def, slot, cell, error, cap);
   free(table);
   return rc;
}

static int write_le32(FILE * f, int v)
{
   unsigned char b[4];

   b[0] = (unsigned char) (v & 0xFF);
   b[1] = (unsigned char) ((v >> 8) & 0xFF);
   b[2] = (unsigned char) ((v >> 16) & 0xFF);
   b[3] = (unsigned char) ((v >> 24) & 0xFF);
   return fwrite(b, 1, 4, f) == 4;
}

int map_ops_write_empty_ds1(const char * path, int width, int height, int act)
{
   FILE * f;
   int ok, i, cells = width * height;

   if (!project_ensure_parent_dirs(path))
      return -1;
   f = fopen(path, "wb");
   if (f == NULL)
      return -1;

   ok = write_le32(f, 18) &&          /* version */
        write_le32(f, width - 1) &&
        write_le32(f, height - 1) &&
        write_le32(f, act - 1) &&
        write_le32(f, 0) &&           /* tag type: none */
        write_le32(f, 0) &&           /* no embedded file names */
        write_le32(f, 1) &&           /* wall layers */
        write_le32(f, 1);             /* floor layers */

   /* wall properties, wall orientations, floor properties, shadow properties */
   for (i = 0; ok && i < cells * 4; i++)
      ok = write_le32(f, 0);

   ok = ok && write_le32(f, 0) &&     /* objects */
        write_le32(f, 0);             /* NPC paths */

   if (fclose(f) != 0)
      ok = 0;
   if (!ok)
   {
      remove(path);
      return -1;
   }
   return 0;
}

int map_ops_create_empty(const char * project_dir, const char * seed, int def,
                         const char * ds1_path, int width, int height, int act,
                         char * error, int cap)
{
   char path[PATH_CAP];

   if (check_not_in_game(project_dir, error, cap) != 0)
      return -1;
   if (map_ops_check_size(width, height, error, cap) != 0)
      return -1;
   if (map_ops_tile_path(project_dir, ds1_path, path, sizeof(path)) != 0)
      return fail(error, cap, "The path for %s is too long.", ds1_path);
   if (file_exists(path))
      return fail(error, cap, "%s already exists in the project.", ds1_path);
   if (map_ops_write_empty_ds1(path, width, height, act) != 0)
      return fail(error, cap, "Couldn't write %s.", path);
   if (map_ops_register(project_dir, seed, def, ds1_path, error, cap) != 0)
   {
      remove(path);
      return -1;
   }
   return 0;
}

/* The first backup folder not already there. */
static int free_backup_dir(const char * project_dir, const char * date, const char * label,
                           const char * stem, const char * file, char * out, int out_cap)
{
   int i, n;

   for (i = 1; i < 1000; i++)
   {
      char probe[PATH_CAP + 16];

      if (i == 1)
         n = snprintf(out, (size_t) out_cap, "%s/backup/%s_%s_%s",
                      project_dir, date, label, stem);
      else
         n = snprintf(out, (size_t) out_cap, "%s/backup/%s_%s_%s_%d",
                      project_dir, date, label, stem, i);
      if (n <= 0 || n >= out_cap)
         return -1;
      snprintf(probe, sizeof(probe), "%s/%s", out, file);
      if (!file_exists(probe))
         return 0;
   }
   return -1;
}

int map_ops_backup_file(const char * project_dir, const char * ds1_path,
                        const char * label, const char * date, const char * json,
                        char * out_ds1, int out_cap, char * error, int cap)
{
   char src[PATH_CAP], dir[PATH_CAP], json_path[PATH_CAP + 16], stem[128], dst[PATH_CAP + 16];
   const char * file = base_name(ds1_path);
   const char * dot = strrchr(file, '.');
   size_t stem_len = dot != NULL ? (size_t)(dot - file) : strlen(file);

   if (check_not_in_game(project_dir, error, cap) != 0)
      return -1;
   if (stem_len >= sizeof(stem))
      return fail(error, cap, "The name %s is too long to back up.", file);
   memcpy(stem, file, stem_len);
   stem[stem_len] = '\0';

   if (map_ops_tile_path(project_dir, ds1_path, src, sizeof(src)) != 0)
      return fail(error, cap, "The path for %s is too long.", ds1_path);
   if (!file_exists(src))
      return fail(error, cap, "The project has no file for %s, so there's nothing to back up.",
                  ds1_path);

   if (free_backup_dir(project_dir, date, label, stem, file, dir, sizeof(dir)) != 0)
      return fail(error, cap, "Couldn't pick a backup folder for %s.", ds1_path);
   snprintf(dst, sizeof(dst), "%s/%s", dir, file);
   snprintf(json_path, sizeof(json_path), "%s/%s.json", dir, stem);
   if ((int) strlen(dst) >= out_cap)
      return fail(error, cap, "The backup path for %s is too long.", ds1_path);
   if (!project_ensure_parent_dirs(dst))
      return fail(error, cap, "Couldn't create %s.", dir);

   if (file_copy_checked(src, dst) != FILE_COPY_OK)
      return fail(error, cap, "Couldn't copy %s to %s.", src, dst);
   if (write_whole_file(json_path, json, (long) strlen(json)) != 0)
   {
      remove(dst);
      return fail(error, cap, "Couldn't write %s.", json_path);
   }
   if (remove(src) != 0)
   {
      remove(dst);
      remove(json_path);
      return fail(error, cap, "Couldn't remove %s, so it wasn't moved to the backup.", src);
   }
   snprintf(out_ds1, (size_t) out_cap, "%s", dst);
   return 0;
}

int map_ops_restore_file(const char * project_dir, const char * backup_ds1,
                         const char * ds1_path, char * error, int cap)
{
   char dst[PATH_CAP];

   if (check_not_in_game(project_dir, error, cap) != 0)
      return -1;
   if (map_ops_tile_path(project_dir, ds1_path, dst, sizeof(dst)) != 0)
      return fail(error, cap, "The path for %s is too long.", ds1_path);
   if (file_exists(dst))
      return fail(error, cap, "The project already has a file for %s.", ds1_path);
   if (!project_ensure_parent_dirs(dst) || file_copy_checked(backup_ds1, dst) != FILE_COPY_OK)
      return fail(error, cap, "Couldn't copy %s to %s.", backup_ds1, dst);
   if (remove(backup_ds1) != 0)
      return fail(error, cap, "%s was restored, but its backup couldn't be removed.", ds1_path);
   return 0;
}
