#include <ctype.h>
#include <errno.h>
#include <limits.h>
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
#define TABLE_REL "Global/Excel/LvlPrest.txt"
#define BAD_TILE_PATH "The path for %s is too long or leads outside the project."
#define TABLE_OLD_SUFFIX ".old"

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

/* An existing project file, in any case, where the editor would read it from:
 * mod_load_in_mem tries <project>/Data/... first, then <project>/... . 1 if found,
 * 0 if not, -1 if it's there but its path doesn't fit. */
static int find_project_file(const char * project_dir, const char * rel, char * out, int cap)
{
   static const char * const roots[] = { "Data/", "" };
   char with_root[PATH_CAP], found[PATH_CAP * 2];
   size_t i;

   for (i = 0; i < sizeof(roots) / sizeof(roots[0]); i++)
   {
      int n = snprintf(with_root, sizeof(with_root), "%s%s", roots[i], rel);

      if (n <= 0 || n >= (int) sizeof(with_root) ||
          !ds1_path_resolve_nocase(project_dir, with_root, found, sizeof(found)))
         continue;
      if ((int) strlen(found) >= cap)
         return -1;
      strcpy(out, found);
      return 1;
   }
   return 0;
}

int map_ops_table_path(const char * project_dir, char * out, int cap)
{
   int n, rc = find_project_file(project_dir, TABLE_REL, out, cap);

   if (rc != 0)
      return rc > 0 ? 0 : -1;

   /* Only the old copy left: the table goes back beside it. */
   rc = find_project_file(project_dir, TABLE_REL TABLE_OLD_SUFFIX, out, cap);
   if (rc > 0)
   {
      out[strlen(out) - strlen(TABLE_OLD_SUFFIX)] = '\0';
      return 0;
   }
   if (rc < 0)
      return -1;

   n = snprintf(out, (size_t) cap, "%s/" TABLE_REL, project_dir);
   return n > 0 && n < cap ? 0 : -1;
}

int map_ops_tile_path(const char * project_dir, const char * ds1_path, char * out, int cap)
{
   int n;
   char * p;

   if (project_path_has_parent_ref(ds1_path) || project_path_is_rooted(ds1_path))
      return -1;
   n = snprintf(out, (size_t) cap, "%s/Global/Tiles/%s", project_dir, ds1_path);
   if (n <= 0 || n >= cap)
      return -1;
   for (p = out; *p != '\0'; p++)
      if (*p == '\\')
         *p = '/';
   return project_path_in_game_install(out) ? -1 : 0;
}

int map_ops_find_tile(const char * project_dir, const char * ds1_path, char * out, int cap)
{
   char rel[PATH_CAP];
   int n;

   if (project_path_has_parent_ref(ds1_path) || project_path_is_rooted(ds1_path))
      return 0;
   n = snprintf(rel, sizeof(rel), "Global/Tiles/%s", ds1_path);
   if (n <= 0 || n >= (int) sizeof(rel))
      return 0;
   return find_project_file(project_dir, rel, out, cap) > 0;
}

char * map_ops_read_file(const char * path, long * out_len)
{
   FILE * f;
   char * buf = NULL;
   long len = -1;

   *out_len = 0;
   f = fopen(path, "rb");
   if (f == NULL)
      return NULL;   /* errno says why */
   if (fseek(f, 0, SEEK_END) == 0)
      len = ftell(f);
   /* On Linux a folder opens, and its end is at LONG_MAX. */
   if (len >= 0 && len < LONG_MAX && fseek(f, 0, SEEK_SET) == 0)
      buf = (char *) malloc((size_t) len + 1);
   if (buf != NULL)
   {
      /* The extra read catches a folder, which can report a size of 0 and open. */
      if (fread(buf, 1, (size_t) len, f) != (size_t) len ||
          (fgetc(f) == EOF && ferror(f)))
      {
         free(buf);
         buf = NULL;
      }
   }
   fclose(f);
   if (buf == NULL)
   {
      errno = EIO;
      return NULL;
   }
   buf[len] = '\0';
   *out_len = len;
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

int map_ops_recover_table(const char * project_dir)
{
   char path[PATH_CAP], old[PATH_CAP];
   FILE * f;

   if (map_ops_table_path(project_dir, path, sizeof(path)) != 0 ||
       snprintf(old, sizeof(old), "%s" TABLE_OLD_SUFFIX, path) >= (int) sizeof(old))
      return -1;
   f = fopen(path, "rb");
   if (f != NULL)
   {
      fclose(f);
      return 0;
   }
   if (errno != ENOENT || !file_exists(old))
      return 0;
   if (project_path_in_game_install(path) || rename(old, path) != 0)
      return -1;
   return 0;
}

char * map_ops_read_table(const char * project_dir, long * out_len)
{
   char path[PATH_CAP];

   *out_len = 0;
   if (map_ops_recover_table(project_dir) != 0 ||
       map_ops_table_path(project_dir, path, sizeof(path)) != 0)
   {
      errno = EIO;
      return NULL;
   }
   return map_ops_read_file(path, out_len);
}

/* The project's table, or a copy of the seed in memory only if the project has
 * none. A table that's there but can't be read stops this: the seed would replace
 * it. Nothing is written until save_table. */
static char * table_for_edit(const char * project_dir, const char * seed,
                             long * out_len, char * error, int cap)
{
   char * text;

   text = map_ops_read_table(project_dir, out_len);
   if (text != NULL)
      return text;
   if (errno != ENOENT)
   {
      fail(error, cap, "The project's LvlPrest.txt can't be read.");
      return NULL;
   }
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

/* Replace *table with an edited copy. `edited` is NULL if the edit couldn't be made. */
static int take_edit(char ** table, char * edited, int def, char * error, int cap)
{
   if (edited == NULL)
      return fail(error, cap, "Couldn't edit Def %d's row in LvlPrest.txt.", def);
   free(*table);
   *table = edited;
   return 0;
}

/* The first of Files and File1-6 the header doesn't have, or NULL. */
static const char * missing_map_column(char * table)
{
   static const char * const cols[] = {
      "Files", "File1", "File2", "File3", "File4", "File5", "File6"
   };
   size_t i;

   for (i = 0; i < sizeof(cols) / sizeof(cols[0]); i++)
      if (lvlprest_find_col(table, cols[i]) < 0)
         return cols[i];
   return NULL;
}

static int check_map_columns(char * table, char * error, int cap)
{
   const char * col = missing_map_column(table);

   if (col != NULL)
      return fail(error, cap, "The project's LvlPrest.txt has no %s column, so maps can't be "
                  "added to or removed from it.", col);
   return 0;
}

static int check_no_hole(char * table, int def, char * error, int cap)
{
   int hole = lvlprest_files_hole(table, def);

   if (hole > 0)
      return fail(error, cap, "Def %d's File%d is empty, but Files counts it, which can crash "
                  "the game. Fix that row in LvlPrest.txt by hand.", def, hole);
   return 0;
}

int map_ops_check_register(char * table, int def, char * error, int cap)
{
   int def_col = lvlprest_find_col(table, "Def");
   char * row_end;

   if (check_map_columns(table, error, cap) != 0)
      return -1;
   if (def_col < 0 || lvlprest_find_row(table, def_col, def, &row_end) == NULL)
      return fail(error, cap, "Def %d isn't in LvlPrest.txt.", def);
   if (check_no_hole(table, def, error, cap) != 0)
      return -1;
   if (!lvlprest_row_is_simple(table, def))
      return fail(error, cap, "Def %d's row chooses its maps by its own rules, so the game "
                  "would never use a map added to it.", def);
   if (lvlprest_files_value(table, def) >= 6)
      return fail(error, cap, "Def %d has no free File slot in LvlPrest.txt.", def);
   return 0;
}

int map_ops_check_new_empty(char * table, int def, char * error, int cap)
{
   if (map_ops_check_register(table, def, error, cap) != 0)
      return -1;
   if (!lvlprest_row_has_size(table, def))
      return fail(error, cap, "Def %d's row is a whole level, and an empty map would join the "
                  "game's random pick for it. Clone a working map instead.", def);
   return 0;
}

#define DIALOG_STATUS_MAX 62

void map_ops_dialog_status(char * table, int def, char * sentence, int cap)
{
   const char * col = missing_map_column(table);
   int hole;

   if ((int) strlen(sentence) <= DIALOG_STATUS_MAX)
      return;
   hole = lvlprest_files_hole(table, def);
   if (col != NULL)
      snprintf(sentence, (size_t) cap, "LvlPrest.txt has no %s column, so it can't take maps.", col);
   else if (hole > 0)
      snprintf(sentence, (size_t) cap, "Def %d's File%d is empty but counted. Fix it by hand.",
               def, hole);
   else if (lvlprest_row_is_simple(table, def) && !lvlprest_row_has_size(table, def))
      snprintf(sentence, (size_t) cap, "Def %d is a whole level. Use Clone, not New.", def);
   else
      snprintf(sentence, (size_t) cap, "Def %d can't take another map. Pick another tileset.", def);
}

void map_ops_default_size(char * table, int def, int * width, int * height)
{
   int x, y;

   if (!lvlprest_row_size(table, def, &x, &y))
      return;
   if (x > 0 && x < MAP_OPS_SIZE_MAX)
      *width = x + 1;
   if (y > 0 && y < MAP_OPS_SIZE_MAX)
      *height = y + 1;
}

int map_ops_wall_cell_is_row_warp(unsigned char prop3, unsigned char prop4,
                                  unsigned char orientation)
{
   int main_index = (prop3 >> 4) + ((prop4 & 0x03) << 4);

   return (orientation == 10 || orientation == 11) && main_index < 8 && (prop4 & 0x80) == 0;
}

int map_ops_check_row_size(char * table, int def, int width, int height, char * error, int cap)
{
   int want_w = 0, want_h = 0;

   map_ops_default_size(table, def, &want_w, &want_h);
   if ((want_w == 0 || width == want_w) && (want_h == 0 || height == want_h))
      return 0;
   if (want_w != 0 && want_h != 0)
      return fail(error, cap, "This row's maps are %d by %d. Use that size.", want_w, want_h);
   if (want_w != 0)
      return fail(error, cap, "This row's maps are %d wide. Use that width.", want_w);
   return fail(error, cap, "This row's maps are %d high. Use that height.", want_h);
}

/* How many of the row's cells hold this whole path. */
static int count_copies(char * table, int def, const char * ds1_path)
{
   int copies = 0, f;

   for (f = 1; f <= 6; f++)
      if (lvlprest_slot_holds_path(table, def, f, ds1_path))
         copies++;
   return copies;
}

/* What both unregister checks ask first. 0 with *copies set if they can go on. */
static int check_unregister_common(char * table, int def, const char * ds1_path, int * copies,
                                   char * error, int cap)
{
   if (check_map_columns(table, error, cap) != 0)
      return -1;
   if (lvlprest_find_path_slot(table, def, ds1_path) == LVLPREST_TABLE_UNUSABLE)
      return fail(error, cap, "The project's LvlPrest.txt has no Def column.");
   *copies = count_copies(table, def, ds1_path);
   if (*copies > 0)
      return check_no_hole(table, def, error, cap);
   return 0;
}

int map_ops_check_unregister(char * table, int def, const char * ds1_path, char * error, int cap)
{
   int copies;

   if (check_unregister_common(table, def, ds1_path, &copies, error, cap) != 0)
      return -1;
   if (copies == 0)
      return 0;
   if (!lvlprest_row_is_simple(table, def))
      return fail(error, cap, "Def %d's row chooses its maps by its own rules, and taking "
                  "one out can crash the game. Edit the map instead.", def);
   if (copies >= lvlprest_files_value(table, def))
      return fail(error, cap, "%s is the only map in Def %d's row, so its level would have "
                  "nothing to load. Edit the map instead.", ds1_path, def);
   return 0;
}

int map_ops_check_unregister_new(char * table, int def, const char * ds1_path, char * error, int cap)
{
   int copies, counted = 0, files, f;

   if (check_unregister_common(table, def, ds1_path, &copies, error, cap) != 0)
      return -1;
   if (copies == 0)
      return 0;
   files = lvlprest_files_value(table, def);
   for (f = 1; f <= 6 && f <= files; f++)
      if (lvlprest_slot_holds_path(table, def, f, ds1_path))
         counted++;

   /* With Files 0 the generator reads the cells itself, so past Files isn't safe. */
   if (files >= 1 && counted == 0)
      return 0;
   if (!lvlprest_row_is_simple(table, def))
      return fail(error, cap, "Def %d's row chooses its maps by its own rules, and taking "
                  "%s out can crash the game. Edit the row in LvlPrest.txt by hand.",
                  def, ds1_path);
   if (counted >= files)
      return fail(error, cap, "%s is the only map in Def %d's row that the game counts, so "
                  "its level would have nothing to load. If you took another map out of this "
                  "row, use Restore Slot on it first. Otherwise edit the row by hand.",
                  ds1_path, def);
   return 0;
}

int map_ops_register(const char * project_dir, const char * seed, int def,
                     const char * ds1_path, char * error, int cap)
{
   long len;
   char * table;
   int rc;

   if (check_not_in_game(project_dir, error, cap) != 0)
      return -1;
   if ((int) strlen(ds1_path) > MAP_OPS_TABLE_PATH_MAX)
      return fail(error, cap, "%s is longer than the %d characters LvlPrest.txt allows, so "
                  "the file must be renamed by hand.", ds1_path, MAP_OPS_TABLE_PATH_MAX);

   table = table_for_edit(project_dir, seed, &len, error, cap);
   if (table == NULL)
      return -1;

   rc = map_ops_check_register(table, def, error, cap);
   if (rc == 0)
   {
      char * edited = lvlprest_with_map_inserted(table, def, 6, ds1_path, &len);

      if (edited == NULL)
         rc = fail(error, cap, "Couldn't edit Def %d's row in LvlPrest.txt.", def);
      else
      {
         rc = save_table(project_dir, edited, len, error, cap);
         free(edited);
      }
   }
   free(table);
   return rc;
}

/* Clear the copies past Files, then take out the rest. The caller's check has said
 * the row allows whichever of these it holds. */
static int take_out_copies(const char * project_dir, const char * seed, int def,
                           const char * ds1_path, int past_files_too,
                           char * error, int cap)
{
   long len;
   char * table;
   int slot, rc, changed = 0;

   if (check_not_in_game(project_dir, error, cap) != 0)
      return -1;
   table = table_for_edit(project_dir, seed, &len, error, cap);
   if (table == NULL)
      return -1;

   rc = past_files_too ? map_ops_check_unregister_new(table, def, ds1_path, error, cap)
                       : map_ops_check_unregister(table, def, ds1_path, error, cap);
   if (rc == 0 && past_files_too)
   {
      int files = lvlprest_files_value(table, def);

      for (slot = files + 1; rc == 0 && slot <= 6; slot++)
         if (slot >= 1 && lvlprest_slot_holds_path(table, def, slot, ds1_path))
         {
            rc = take_edit(&table, lvlprest_with_cell(table, def, slot, "0", &len), def, error, cap);
            changed = 1;
         }
   }
   slot = rc == 0 ? lvlprest_find_path_slot(table, def, ds1_path) : 0;
   while (rc == 0 && slot > 0)
   {
      rc = take_edit(&table, lvlprest_with_map_removed(table, def, slot, &len), def, error, cap);
      changed = 1;
      if (rc == 0)
         slot = lvlprest_find_path_slot(table, def, ds1_path);
   }
   if (rc == 0 && changed)
      rc = save_table(project_dir, table, len, error, cap);
   free(table);
   return rc;
}

int map_ops_unregister(const char * project_dir, const char * seed, int def,
                       const char * ds1_path, char * error, int cap)
{
   return take_out_copies(project_dir, seed, def, ds1_path, 0, error, cap);
}

int map_ops_unregister_new(const char * project_dir, const char * seed, int def,
                           const char * ds1_path, char * error, int cap)
{
   return take_out_copies(project_dir, seed, def, ds1_path, 1, error, cap);
}

int map_ops_lists_map(const char * project_dir, const char * seed, int def,
                      const char * ds1_path)
{
   long len;
   char ignored[8];
   char * table = table_for_edit(project_dir, seed, &len, ignored, sizeof(ignored));
   int listed;

   if (table == NULL)
      return 0;
   listed = lvlprest_lists_path(table, def, ds1_path);
   free(table);
   return listed;
}

int map_ops_restore_slot(const char * project_dir, const char * seed, char * game_table,
                         int def, const char * ds1_path, char * error, int cap)
{
   long len;
   char * table;
   char cell[256];
   int game_slots[6];
   int game_count = 0, have = 0, f, i, rc = 0;

   if (check_not_in_game(project_dir, error, cap) != 0)
      return -1;
   for (f = 1; f <= 6; f++)
      if (lvlprest_slot_holds_path(game_table, def, f, ds1_path))
         game_slots[game_count++] = f;
   if (game_count == 0)
      return fail(error, cap, "The game's LvlPrest.txt doesn't list %s under Def %d.",
                  ds1_path, def);

   table = table_for_edit(project_dir, seed, &len, error, cap);
   if (table == NULL)
      return -1;

   /* Listed as often as the game lists it: there's nothing to put back, and
      another copy would be ambiguous. */
   for (f = 1; f <= 6; f++)
      if (lvlprest_slot_holds_path(table, def, f, ds1_path))
         have++;
   if (have >= game_count)
   {
      free(table);
      return 0;
   }

   for (i = 0; i < game_count && have < game_count && rc == 0; i++)
   {
      int slot = game_slots[i];

      if (lvlprest_slot_holds_path(table, def, slot, ds1_path))
         continue;
      if (!lvlprest_get_cell(game_table, def, slot, cell, sizeof(cell)))
      {
         rc = fail(error, cap, "The game's LvlPrest.txt doesn't list %s under Def %d.",
                   ds1_path, def);
         break;
      }
      rc = map_ops_check_register(table, def, error, cap);
      if (rc == 0)
         rc = take_edit(&table, lvlprest_with_map_inserted(table, def, slot, cell, &len),
                        def, error, cap);
      have++;
   }
   if (rc == 0)
      rc = save_table(project_dir, table, len, error, cap);
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
   char path[PATH_CAP], found[PATH_CAP];

   if (check_not_in_game(project_dir, error, cap) != 0)
      return -1;
   if (map_ops_check_size(width, height, error, cap) != 0)
      return -1;
   if (map_ops_tile_path(project_dir, ds1_path, path, sizeof(path)) != 0)
      return fail(error, cap, BAD_TILE_PATH, ds1_path);
   if (map_ops_find_tile(project_dir, ds1_path, found, sizeof(found)))
      return fail(error, cap, "%s already exists in the project.", ds1_path);
   {
      long len;
      char * table = table_for_edit(project_dir, seed, &len, error, cap);
      int rc;

      if (table == NULL)
         return -1;
      rc = map_ops_check_new_empty(table, def, error, cap);
      free(table);
      if (rc != 0)
         return -1;
   }
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
      return fail(error, cap, BAD_TILE_PATH, ds1_path);
   if (!map_ops_find_tile(project_dir, ds1_path, src, sizeof(src)))
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
   char dst[PATH_CAP], found[PATH_CAP];

   if (check_not_in_game(project_dir, error, cap) != 0)
      return -1;
   if (map_ops_tile_path(project_dir, ds1_path, dst, sizeof(dst)) != 0)
      return fail(error, cap, BAD_TILE_PATH, ds1_path);
   if (map_ops_find_tile(project_dir, ds1_path, found, sizeof(found)))
      return fail(error, cap, "The project already has a file for %s.", ds1_path);
   if (!project_ensure_parent_dirs(dst) || file_copy_checked(backup_ds1, dst) != FILE_COPY_OK)
      return fail(error, cap, "Couldn't copy %s to %s.", backup_ds1, dst);
   if (remove(backup_ds1) != 0)
      fail(error, cap, "%s was restored, but its backup %s couldn't be removed.",
           ds1_path, backup_ds1);
   return 0;
}
