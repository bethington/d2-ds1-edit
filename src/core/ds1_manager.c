#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "structs.h"
#include "error.h"
#include "misc.h"
#include "core/ds1.h"
#include "core/txtread.h"
#include "core/area_browser.h"
#include "core/ds1_manager.h"

#ifdef WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#define LVLPREST_MAX_LINE   4096
#define LVLPREST_MAX_COLS   30

/* ---- LvlPrest.txt raw file manipulation ---- */

/* Read the raw LvlPrest.txt from mod_dir (or assets/excel/ fallback).
 * Returns malloc'd buffer with file contents, sets *out_len. NULL on error. */
static char * lvlprest_read_raw(long * out_len)
{
   char path[512];
   FILE * in;
   long len;
   char * buf;

   /* Try mod_dir first */
   if (glb_config.mod_dir[0] != NULL)
   {
      sprintf(path, "%s\\Global\\Excel\\LvlPrest.txt", glb_config.mod_dir[0]);
      in = fopen(path, "rb");
      if (in != NULL)
      {
         fseek(in, 0, SEEK_END);
         len = ftell(in);
         fseek(in, 0, SEEK_SET);
         buf = (char *)malloc(len + 1);
         if (buf != NULL)
         {
            fread(buf, 1, len, in);
            buf[len] = '\0';
         }
         fclose(in);
         *out_len = len;
         return buf;
      }
   }

   /* Fallback to local assets */
   sprintf(path, "assets/excel/LvlPrest.txt");
   in = fopen(path, "rb");
   if (in == NULL)
   {
      printf("ds1_manager: can't open LvlPrest.txt\n");
      return NULL;
   }
   fseek(in, 0, SEEK_END);
   len = ftell(in);
   fseek(in, 0, SEEK_SET);
   buf = (char *)malloc(len + 1);
   if (buf != NULL)
   {
      fread(buf, 1, len, in);
      buf[len] = '\0';
   }
   fclose(in);
   *out_len = len;
   return buf;
}

/* Write the raw LvlPrest.txt content to mod_dir. Creates directories if needed. */
static int lvlprest_write_raw(const char * buf, long len)
{
   char path[512], dir[512];
   FILE * out;

   if (glb_config.mod_dir[0] == NULL)
   {
      printf("ds1_manager: no mod_dir configured, can't write LvlPrest.txt\n");
      return -1;
   }

   /* Ensure directory exists */
   sprintf(dir, "%s\\Global", glb_config.mod_dir[0]);
   MKDIR(dir);
   sprintf(dir, "%s\\Global\\Excel", glb_config.mod_dir[0]);
   MKDIR(dir);

   sprintf(path, "%s\\Global\\Excel\\LvlPrest.txt", glb_config.mod_dir[0]);
   out = fopen(path, "wb");
   if (out == NULL)
   {
      printf("ds1_manager: can't write %s\n", path);
      return -1;
   }
   fwrite(buf, 1, len, out);
   fclose(out);
   printf("ds1_manager: wrote %s (%ld bytes)\n", path, len);
   return 0;
}

/* Find the column index of a named column in a tab-separated header line.
 * Returns 0-based index, or -1 if not found. */
static int lvlprest_find_col(const char * header_line, const char * col_name)
{
   int col = 0;
   const char * p = header_line;
   int name_len = (int)strlen(col_name);

   while (*p != '\0' && *p != '\n' && *p != '\r')
   {
      /* Check if current position matches col_name */
      if (strnicmp(p, col_name, name_len) == 0)
      {
         char next = p[name_len];
         if (next == '\t' || next == '\n' || next == '\r' || next == '\0')
            return col;
      }

      /* Skip to next tab */
      while (*p != '\0' && *p != '\t' && *p != '\n' && *p != '\r')
         p++;
      if (*p == '\t')
      {
         p++;
         col++;
      }
   }
   return -1;
}

/* Find the start of a specific row by Def value. Returns pointer into buf, or NULL.
 * Also sets *row_end to point past the row's newline. */
static char * lvlprest_find_row(char * buf, int def_col, int target_def,
                                 char ** row_end)
{
   char * line = buf;
   char * next;
   int is_header = 1;

   while (*line != '\0')
   {
      /* Find end of line */
      next = line;
      while (*next != '\0' && *next != '\n' && *next != '\r')
         next++;

      if (!is_header)
      {
         /* Get the Def column value */
         char * p = line;
         int col = 0;
         while (col < def_col && *p != '\0' && *p != '\n' && *p != '\r')
         {
            if (*p == '\t') col++;
            p++;
         }
         if (col == def_col)
         {
            int val = atoi(p);
            if (val == target_def)
            {
               /* Skip past newline(s) */
               char * end = next;
               while (*end == '\n' || *end == '\r') end++;
               if (row_end) *row_end = end;
               return line;
            }
         }
      }

      /* Skip past newline(s) */
      while (*next == '\n' || *next == '\r') next++;
      line = next;
      is_header = 0;
   }
   return NULL;
}

/* Set a File column value in LvlPrest.txt for a given Def.
 * file_slot: 1-6 (maps to File1-File6 columns).
 * Returns 0 on success. */
int ds1_manager_lvlprest_set_file(int def, int file_slot, const char * ds1_path)
{
   char * buf, * row, * row_end;
   long len;
   int def_col, file_col;
   char col_name[16];
   char * header_end;
   char new_buf[256 * 1024]; /* 256KB should be enough */
   int new_len;

   if (file_slot < 1 || file_slot > 6) return -1;

   buf = lvlprest_read_raw(&len);
   if (buf == NULL) return -1;

   /* Find column indices from header */
   def_col = lvlprest_find_col(buf, "Def");
   sprintf(col_name, "File%d", file_slot);
   file_col = lvlprest_find_col(buf, col_name);

   if (def_col < 0 || file_col < 0)
   {
      printf("ds1_manager: can't find Def or %s column in LvlPrest.txt\n", col_name);
      free(buf);
      return -1;
   }

   /* Find the header end */
   header_end = buf;
   while (*header_end != '\0' && *header_end != '\n' && *header_end != '\r')
      header_end++;
   while (*header_end == '\n' || *header_end == '\r') header_end++;

   /* Find the target row */
   row = lvlprest_find_row(buf, def_col, def, &row_end);
   if (row == NULL)
   {
      printf("ds1_manager: Def %d not found in LvlPrest.txt\n", def);
      free(buf);
      return -1;
   }

   /* Rebuild the row with the modified File column */
   {
      char new_row[LVLPREST_MAX_LINE];
      char * src = row;
      int col = 0, pos = 0;

      while (src < row_end && *src != '\n' && *src != '\r' && *src != '\0')
      {
         if (col == file_col)
         {
            /* Replace this column's value */
            int path_len = (int)strlen(ds1_path);
            memcpy(new_row + pos, ds1_path, path_len);
            pos += path_len;
            /* Skip old value */
            while (src < row_end && *src != '\t' && *src != '\n' && *src != '\r' && *src != '\0')
               src++;
         }
         else
         {
            /* Copy column as-is */
            while (src < row_end && *src != '\t' && *src != '\n' && *src != '\r' && *src != '\0')
            {
               new_row[pos++] = *src++;
            }
         }

         if (*src == '\t')
         {
            new_row[pos++] = '\t';
            src++;
            col++;
         }
      }
      new_row[pos] = '\0';

      /* Reconstruct the full file: before_row + new_row + newline + after_row */
      new_len = 0;
      memcpy(new_buf, buf, row - buf);
      new_len = (int)(row - buf);
      memcpy(new_buf + new_len, new_row, pos);
      new_len += pos;
      new_buf[new_len++] = '\r';
      new_buf[new_len++] = '\n';
      memcpy(new_buf + new_len, row_end, len - (row_end - buf));
      new_len += (int)(len - (row_end - buf));
   }

   free(buf);
   return lvlprest_write_raw(new_buf, new_len);
}

/* Clear a File column in LvlPrest.txt (set to "0"). */
int ds1_manager_lvlprest_clear_file(int def, int file_slot)
{
   return ds1_manager_lvlprest_set_file(def, file_slot, "0");
}

/* Find the first empty File slot (1-6) for a given Def. Returns slot number or -1. */
int ds1_manager_lvlprest_find_empty_slot(int def)
{
   TXT_S * txt = glb_ds1edit.lvlprest_buff;
   int i, f;
   int def_col, file_cols[6];
   char col_name[16];

   if (txt == NULL) return -1;

   def_col = misc_get_txt_column_num(RQ_LVLPREST, "Def");
   for (f = 0; f < 6; f++)
   {
      sprintf(col_name, "File%d", f + 1);
      file_cols[f] = misc_get_txt_column_num(RQ_LVLPREST, col_name);
   }

   for (i = 0; i < txt->line_num; i++)
   {
      long * def_ptr = (long *)(txt->data + (i * txt->line_size) + txt->col[def_col].offset);
      if (*def_ptr == def)
      {
         for (f = 0; f < 6; f++)
         {
            char * path = txt->data + (i * txt->line_size) + txt->col[file_cols[f]].offset;
            if (path[0] == '\0' || path[0] == '0')
               return f + 1;
         }
         return -1; /* All slots full */
      }
   }
   return -1; /* Def not found */
}

/* Find which File slot (1-6) contains a specific DS1 path for a given Def.
 * Matches by filename (case-insensitive). Returns slot 1-6, or -1 if not found. */
int ds1_manager_lvlprest_find_file_slot(int def, const char * ds1_path)
{
   TXT_S * txt = glb_ds1edit.lvlprest_buff;
   int i, f;
   int def_col, file_cols[6];
   char col_name[16];
   const char * search_name;

   if (txt == NULL || ds1_path == NULL) return -1;

   /* Extract just the filename for matching */
   search_name = strrchr(ds1_path, '/');
   if (search_name == NULL) search_name = strrchr(ds1_path, '\\');
   if (search_name != NULL) search_name++; else search_name = ds1_path;

   def_col = misc_get_txt_column_num(RQ_LVLPREST, "Def");
   for (f = 0; f < 6; f++)
   {
      sprintf(col_name, "File%d", f + 1);
      file_cols[f] = misc_get_txt_column_num(RQ_LVLPREST, col_name);
   }

   for (i = 0; i < txt->line_num; i++)
   {
      long * def_ptr = (long *)(txt->data + (i * txt->line_size) + txt->col[def_col].offset);
      if (*def_ptr == def)
      {
         for (f = 0; f < 6; f++)
         {
            char * path = txt->data + (i * txt->line_size) + txt->col[file_cols[f]].offset;
            const char * entry_name;

            if (path[0] == '\0' || path[0] == '0')
               continue;

            /* Match by filename */
            entry_name = strrchr(path, '/');
            if (entry_name == NULL) entry_name = strrchr(path, '\\');
            if (entry_name != NULL) entry_name++; else entry_name = path;

            if (stricmp(entry_name, search_name) == 0)
               return f + 1;
         }
         return -1; /* Def found but file not in any slot */
      }
   }
   return -1;
}

/* Full delete operation: backup, update LvlPrest.txt, remove file. */
int ds1_manager_delete(int group_idx, int entry_idx)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   AREA_DS1_ENTRY_S * e;
   int file_slot;
   char src_path[512];

   if (group_idx < 0 || group_idx >= ab->group_count) return -1;
   g = &ab->groups[group_idx];
   if (entry_idx < 0 || entry_idx >= g->entry_count) return -1;
   e = &g->entries[entry_idx];

   /* 1. Create backup */
   if (ds1_manager_backup(group_idx, entry_idx) != 0)
   {
      printf("ds1_manager_delete: backup failed, aborting delete\n");
      return -1;
   }

   /* 2. Find the file slot in LvlPrest.txt */
   file_slot = ds1_manager_lvlprest_find_file_slot(e->lvlprest_def, e->ds1_path);
   if (file_slot > 0)
   {
      /* 3. Clear the File slot in LvlPrest.txt */
      if (ds1_manager_lvlprest_clear_file(e->lvlprest_def, file_slot) == 0)
         printf("ds1_manager_delete: cleared File%d for Def %d in LvlPrest.txt\n",
                file_slot, e->lvlprest_def);
      else
         printf("ds1_manager_delete: warning, failed to update LvlPrest.txt\n");
   }
   else
   {
      printf("ds1_manager_delete: warning, couldn't find file slot in LvlPrest.txt\n");
   }

   /* 4. Delete the DS1 file from disk */
   if (glb_config.mod_dir[0] != NULL)
      sprintf(src_path, "%s\\Global\\Tiles\\%s", glb_config.mod_dir[0], e->ds1_path);
   else
      sprintf(src_path, "assets/tiles/%s", e->ds1_path);

   if (remove(src_path) == 0)
      printf("ds1_manager_delete: removed %s\n", src_path);
   else
      printf("ds1_manager_delete: warning, couldn't remove %s (may not exist on disk)\n", src_path);

   /* 5. Remove entry from the in-memory area browser */
   {
      int j;
      for (j = entry_idx; j < g->entry_count - 1; j++)
         g->entries[j] = g->entries[j + 1];
      g->entry_count--;
   }

   printf("ds1_manager_delete: complete\n");
   fflush(stdout);
   return 0;
}


/* ---- Backup System ---- */

/* Write JSON metadata for a backed-up DS1 file. */
int ds1_manager_write_backup_json(const char * json_path, int group_idx,
                                   int entry_idx, int ds1_idx)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   AREA_DS1_ENTRY_S * e;
   FILE * out;
   time_t now;
   struct tm * t;
   char time_str[64];

   if (group_idx < 0 || group_idx >= ab->group_count) return -1;
   g = &ab->groups[group_idx];
   if (entry_idx < 0 || entry_idx >= g->entry_count) return -1;
   e = &g->entries[entry_idx];

   out = fopen(json_path, "wt");
   if (out == NULL) return -1;

   time(&now);
   t = localtime(&now);
   strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", t);

   fprintf(out, "{\n");
   fprintf(out, "  \"backup_date\": \"%s\",\n", time_str);
   fprintf(out, "  \"original_path\": \"%s\",\n", e->ds1_path);
   fprintf(out, "  \"lvlprest\": {\n");
   fprintf(out, "    \"def\": %d,\n", e->lvlprest_def);
   fprintf(out, "    \"lvltype_id\": %d,\n", e->lvltype_id);

   if (g->act > 0)
      fprintf(out, "    \"area\": \"Act %d - %s\"\n", g->act, g->name);
   else
      fprintf(out, "    \"area\": \"%s\"\n", g->name);

   fprintf(out, "  },\n");

   /* DS1 metadata if loaded */
   if (ds1_idx >= 0 && ds1_idx < DS1_MAX && glb_ds1[ds1_idx].name[0] != '\0')
   {
      fprintf(out, "  \"ds1_metadata\": {\n");
      fprintf(out, "    \"version\": %ld,\n", glb_ds1[ds1_idx].version);
      fprintf(out, "    \"width\": %ld,\n", glb_ds1[ds1_idx].width);
      fprintf(out, "    \"height\": %ld,\n", glb_ds1[ds1_idx].height);
      fprintf(out, "    \"act\": %ld,\n", glb_ds1[ds1_idx].act);
      fprintf(out, "    \"object_count\": %ld,\n", glb_ds1[ds1_idx].obj_num);
      fprintf(out, "    \"floor_layers\": %ld,\n", glb_ds1[ds1_idx].floor_num);
      fprintf(out, "    \"wall_layers\": %ld\n", glb_ds1[ds1_idx].wall_num);
      fprintf(out, "  }\n");
   }
   else
   {
      fprintf(out, "  \"ds1_metadata\": null\n");
   }

   fprintf(out, "}\n");
   fclose(out);
   return 0;
}

/* Backup a DS1 file: move it to backup/ folder with JSON metadata. */
int ds1_manager_backup(int group_idx, int entry_idx)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   AREA_DS1_ENTRY_S * e;
   char backup_dir[512], backup_subdir[512];
   char src_path[512], dst_ds1[512], dst_json[512];
   const char * fname;
   time_t now;
   struct tm * t;
   char date_str[32];

   if (group_idx < 0 || group_idx >= ab->group_count) return -1;
   g = &ab->groups[group_idx];
   if (entry_idx < 0 || entry_idx >= g->entry_count) return -1;
   e = &g->entries[entry_idx];

   /* Extract filename */
   fname = strrchr(e->ds1_path, '/');
   if (fname == NULL) fname = strrchr(e->ds1_path, '\\');
   if (fname != NULL) fname++; else fname = e->ds1_path;

   /* Create backup directory */
   time(&now);
   t = localtime(&now);
   strftime(date_str, sizeof(date_str), "%Y-%m-%d", t);

   sprintf(backup_dir, "backup");
   MKDIR(backup_dir);

   if (g->act > 0)
      sprintf(backup_subdir, "backup/%s_Act%d-%s_%.*s",
              date_str, g->act, g->name,
              (int)(strlen(fname) - 4), fname); /* strip .ds1 */
   else
      sprintf(backup_subdir, "backup/%s_%s_%.*s",
              date_str, g->name,
              (int)(strlen(fname) - 4), fname);
   MKDIR(backup_subdir);

   /* Source path */
   if (glb_config.mod_dir[0] != NULL)
      sprintf(src_path, "%s\\Global\\Tiles\\%s", glb_config.mod_dir[0], e->ds1_path);
   else
      sprintf(src_path, "assets/tiles/%s", e->ds1_path);

   /* Copy DS1 to backup */
   sprintf(dst_ds1, "%s/%s", backup_subdir, fname);
   {
      FILE * in = fopen(src_path, "rb");
      if (in != NULL)
      {
         FILE * out = fopen(dst_ds1, "wb");
         if (out != NULL)
         {
            char copy_buf[8192];
            int n;
            while ((n = (int)fread(copy_buf, 1, sizeof(copy_buf), in)) > 0)
               fwrite(copy_buf, 1, n, out);
            fclose(out);
            printf("ds1_manager: backed up %s\n", dst_ds1);
         }
         fclose(in);
      }
      else
      {
         printf("ds1_manager: warning, can't read %s for backup\n", src_path);
      }
   }

   /* Write JSON metadata */
   {
      char json_name[256];
      sprintf(json_name, "%.*s.json", (int)(strlen(fname) - 4), fname);
      sprintf(dst_json, "%s/%s", backup_subdir, json_name);
      ds1_manager_write_backup_json(dst_json, group_idx, entry_idx,
                                     ab->loaded_group == group_idx ? entry_idx : -1);
   }

   return 0;
}

/* Write a minimal empty DS1 file to disk. Returns 0 on success. */
static int write_empty_ds1(const char * path, int width, int height, int act)
{
   FILE * out;
   long val;
   int x, y;

   out = fopen(path, "wb");
   if (out == NULL) return -1;

   /* Version 18 */
   val = 18; fwrite(&val, 4, 1, out);

   /* Width - 1 */
   val = width - 1; fwrite(&val, 4, 1, out);

   /* Height - 1 */
   val = height - 1; fwrite(&val, 4, 1, out);

   /* Act - 1 */
   val = act - 1; fwrite(&val, 4, 1, out);

   /* Tag type = 0 */
   val = 0; fwrite(&val, 4, 1, out);

   /* File count = 0 (no embedded filenames) */
   val = 0; fwrite(&val, 4, 1, out);

   /* Wall layers = 1, Floor layers = 1 */
   val = 1; fwrite(&val, 4, 1, out); /* wall_num */
   val = 1; fwrite(&val, 4, 1, out); /* floor_num */

   /* Walls: props layer (4 bytes per cell) + orientation layer (4 bytes per cell) */
   for (y = 0; y < height; y++)
      for (x = 0; x < width; x++)
      { val = 0; fwrite(&val, 4, 1, out); }
   for (y = 0; y < height; y++)
      for (x = 0; x < width; x++)
      { val = 0; fwrite(&val, 4, 1, out); }

   /* Floors: props layer */
   for (y = 0; y < height; y++)
      for (x = 0; x < width; x++)
      { val = 0; fwrite(&val, 4, 1, out); }

   /* Shadows: props layer */
   for (y = 0; y < height; y++)
      for (x = 0; x < width; x++)
      { val = 0; fwrite(&val, 4, 1, out); }

   /* Objects: count = 0 */
   val = 0; fwrite(&val, 4, 1, out);

   /* NPC paths: count = 0 */
   val = 0; fwrite(&val, 4, 1, out);

   fclose(out);
   return 0;
}

/* Create an empty DS1 file and add it to the given area group.
 * Uses smart name suggestion based on the selected entry.
 * Saves to mod_dir and updates LvlPrest.txt. Returns 0 on success. */
int ds1_manager_create_empty(int group_idx, int entry_idx, int width, int height, int act)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   char filename[128], rel_path[256], full_path[512], dir_path[512];
   int def, file_slot;
   int base_entry;

   if (group_idx < 0 || group_idx >= ab->group_count) return -1;
   g = &ab->groups[group_idx];
   if (g->entry_count == 0) return -1;
   if (glb_config.mod_dir[0] == NULL)
   {
      printf("ds1_manager_create_empty: no mod_dir configured\n");
      return -1;
   }

   /* Use selected entry (or first entry) as template for naming and Def */
   base_entry = (entry_idx >= 0 && entry_idx < g->entry_count) ? entry_idx : 0;
   def = g->entries[base_entry].lvlprest_def;

   /* Find an empty file slot */
   file_slot = ds1_manager_lvlprest_find_empty_slot(def);
   if (file_slot < 0)
   {
      printf("ds1_manager_create_empty: no empty File slot for Def %d\n", def);
      return -1;
   }

   /* Generate smart filename based on selected entry */
   ds1_manager_suggest_name(g->entries[base_entry].ds1_path, g, filename, sizeof(filename));

   /* Build full path */
   {
      const char * first_path = g->entries[base_entry].ds1_path;
      const char * last_slash = strrchr(first_path, '/');
      int dir_len;

      if (last_slash == NULL) last_slash = strrchr(first_path, '\\');
      if (last_slash == NULL)
      {
         printf("ds1_manager_create_empty: can't determine directory from %s\n", first_path);
         return -1;
      }
      dir_len = (int)(last_slash - first_path);
      sprintf(rel_path, "%.*s/%s", dir_len, first_path, filename);
      sprintf(full_path, "%s\\Global\\Tiles\\%.*s\\%s",
              glb_config.mod_dir[0], dir_len, first_path, filename);
   }

   /* Ensure directory exists */
   {
      const char * last_slash = strrchr(full_path, '\\');
      if (last_slash == NULL) last_slash = strrchr(full_path, '/');
      if (last_slash != NULL)
      {
         int dlen = (int)(last_slash - full_path);
         strncpy(dir_path, full_path, dlen);
         dir_path[dlen] = '\0';
         MKDIR(dir_path);
      }
   }

   /* Write the empty DS1 */
   if (write_empty_ds1(full_path, width, height, act) != 0)
   {
      printf("ds1_manager_create_empty: failed to write %s\n", full_path);
      return -1;
   }
   printf("ds1_manager_create_empty: created %s (%dx%d, act %d)\n",
          full_path, width, height, act);

   /* Update LvlPrest.txt */
   if (ds1_manager_lvlprest_set_file(def, file_slot, rel_path) == 0)
      printf("ds1_manager_create_empty: added to LvlPrest.txt File%d for Def %d\n",
             file_slot, def);

   /* Add to in-memory area browser */
   {
      AREA_DS1_ENTRY_S new_entry;
      new_entry.lvltype_id = g->lvltype_id;
      new_entry.lvlprest_def = def;
      strncpy(new_entry.ds1_path, rel_path, sizeof(new_entry.ds1_path) - 1);
      new_entry.ds1_path[sizeof(new_entry.ds1_path) - 1] = '\0';
      area_group_add_entry(g, g->lvltype_id, def, rel_path);
   }

   printf("ds1_manager_create_empty: complete\n");
   fflush(stdout);
   return 0;
}

/* Clone the current DS1 and add it to the given area group.
 * Uses smart name suggestion based on the selected entry. */
int ds1_manager_clone(int src_ds1_idx, int group_idx, int entry_idx)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   char src_path[512], dst_path[512], rel_path[256], filename[128];
   int def, file_slot, counter = 1;
   const char * first_path;
   const char * last_slash;
   int dir_len;

   if (group_idx < 0 || group_idx >= ab->group_count) return -1;
   g = &ab->groups[group_idx];
   if (g->entry_count == 0) return -1;
   if (glb_config.mod_dir[0] == NULL) return -1;
   if (src_ds1_idx < 0 || src_ds1_idx >= DS1_MAX) return -1;
   if (glb_ds1[src_ds1_idx].name[0] == '\0') return -1;

   {
      int base_entry = (entry_idx >= 0 && entry_idx < g->entry_count) ? entry_idx : 0;
      def = g->entries[base_entry].lvlprest_def;
   }
   file_slot = ds1_manager_lvlprest_find_empty_slot(def);
   if (file_slot < 0)
   {
      printf("ds1_manager_clone: no empty File slot for Def %d\n", def);
      return -1;
   }

   /* Build source path */
   strcpy(src_path, glb_ds1[src_ds1_idx].name);

   /* Generate smart destination filename */
   {
      int base_entry = (entry_idx >= 0 && entry_idx < g->entry_count) ? entry_idx : 0;
      ds1_manager_suggest_name(g->entries[base_entry].ds1_path, g, filename, sizeof(filename));

      first_path = g->entries[base_entry].ds1_path;
      last_slash = strrchr(first_path, '/');
      if (last_slash == NULL) last_slash = strrchr(first_path, '\\');
      if (last_slash == NULL) return -1;
      dir_len = (int)(last_slash - first_path);

      sprintf(rel_path, "%.*s/%s", dir_len, first_path, filename);
      sprintf(dst_path, "%s\\Global\\Tiles\\%.*s\\%s",
              glb_config.mod_dir[0], dir_len, first_path, filename);
   }

   /* Copy the file */
   {
      FILE * in = fopen(src_path, "rb");
      if (in == NULL)
      {
         printf("ds1_manager_clone: can't read %s\n", src_path);
         return -1;
      }
      {
         FILE * out = fopen(dst_path, "wb");
         if (out != NULL)
         {
            char copy_buf[8192];
            int n;
            while ((n = (int)fread(copy_buf, 1, sizeof(copy_buf), in)) > 0)
               fwrite(copy_buf, 1, n, out);
            fclose(out);
         }
         else
         {
            fclose(in);
            printf("ds1_manager_clone: can't write %s\n", dst_path);
            return -1;
         }
      }
      fclose(in);
   }
   printf("ds1_manager_clone: copied %s -> %s\n", src_path, dst_path);

   /* Update LvlPrest.txt */
   if (ds1_manager_lvlprest_set_file(def, file_slot, rel_path) == 0)
      printf("ds1_manager_clone: added to LvlPrest.txt File%d for Def %d\n",
             file_slot, def);

   /* Add to in-memory area browser */
   area_group_add_entry(g, g->lvltype_id, def, rel_path);

   printf("ds1_manager_clone: complete\n");
   fflush(stdout);
   return 0;
}


/* ---- Smart filename generation ---- */

/* Extract the base name root and trailing number from a DS1 filename.
 * "BarE.ds1" → root="BarE", num=0 (no number)
 * "BarE2.ds1" → root="BarE", num=2
 * "BarE3.ds1" → root="BarE", num=3 */
static void parse_ds1_name(const char * fname, char * root, int root_size, int * num)
{
   int len, i, num_start;
   const char * dot;

   *num = 0;
   root[0] = '\0';

   /* Strip .ds1 extension */
   dot = strrchr(fname, '.');
   len = dot ? (int)(dot - fname) : (int)strlen(fname);

   /* Find where trailing digits start */
   num_start = len;
   while (num_start > 0 && fname[num_start - 1] >= '0' && fname[num_start - 1] <= '9')
      num_start--;

   /* If the entire name is digits, treat it all as root */
   if (num_start == 0)
   {
      strncpy(root, fname, len < root_size ? len : root_size - 1);
      root[len < root_size ? len : root_size - 1] = '\0';
      return;
   }

   /* Extract root */
   i = num_start < root_size ? num_start : root_size - 1;
   strncpy(root, fname, i);
   root[i] = '\0';

   /* Extract number */
   if (num_start < len)
      *num = atoi(fname + num_start);
}

/* Generate a suggested filename for a new DS1, finding the first gap
 * in the numbering sequence. Examines existing entries in the group.
 *
 * Example: if BarE.ds1, BarE2.ds1, BarE4.ds1 exist:
 *   base="BarE3.ds1" → suggests "BarE3.ds1" (fills gap)
 *   base="BarE4.ds1" → suggests "BarE3.ds1" (fills gap from BarE root)
 *   base="BarE.ds1"  → suggests "BarE3.ds1" (fills gap from BarE root) */
void ds1_manager_suggest_name(const char * base_ds1_path,
                               const AREA_GROUP_S * group,
                               char * out_name, int out_size)
{
   char base_root[128];
   int base_num, j, num;
   int used[256];
   int max_num = 0;
   const char * base_fname;

   /* Extract just the filename from the path */
   base_fname = strrchr(base_ds1_path, '/');
   if (base_fname == NULL) base_fname = strrchr(base_ds1_path, '\\');
   if (base_fname != NULL) base_fname++; else base_fname = base_ds1_path;

   parse_ds1_name(base_fname, base_root, sizeof(base_root), &base_num);

   /* Scan all entries in the group with the same root name */
   memset(used, 0, sizeof(used));
   for (j = 0; j < group->entry_count; j++)
   {
      const char * entry_fname;
      char entry_root[128];
      int entry_num;

      entry_fname = strrchr(group->entries[j].ds1_path, '/');
      if (entry_fname == NULL) entry_fname = strrchr(group->entries[j].ds1_path, '\\');
      if (entry_fname != NULL) entry_fname++; else entry_fname = group->entries[j].ds1_path;

      parse_ds1_name(entry_fname, entry_root, sizeof(entry_root), &entry_num);

      if (stricmp(entry_root, base_root) == 0)
      {
         /* Mark this number as used. No-number files count as "1" */
         num = (entry_num == 0) ? 1 : entry_num;
         if (num > 0 && num < 256)
            used[num] = 1;
         if (num > max_num)
            max_num = num;
      }
   }

   /* Find the first gap (starting from 2, since "no number" = 1) */
   for (num = 2; num <= max_num + 1 && num < 256; num++)
   {
      if (!used[num])
      {
         sprintf(out_name, "%s%d.ds1", base_root, num);
         return;
      }
   }

   /* No gap found — use next after max */
   sprintf(out_name, "%s%d.ds1", base_root, max_num + 1);
}
