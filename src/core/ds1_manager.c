#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "platform.h"
#include "structs.h"
#include "error.h"
#include "misc.h"
#include "core/ds1.h"
#include "core/txtread.h"
#include "core/area_browser.h"
#include "core/ds1_manager.h"
#include "core/file_copy.h"
#include "core/lvlprest_text.h"
#include "core/map_ops.h"
#include "core/map_state.h"
#include "core/project.h"
#include "core/ds1_save_io.h"

#ifdef WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#define RMDIR(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#define RMDIR(path) rmdir(path)
#endif

/* ---- Helpers shared by the project map operations ---- */

/* What a project without its own table starts from: what the editor reads now,
 * overlays included. Caller frees. */
static char * manager_seed(void)
{
   char name[] = "Data\\Global\\Excel\\LvlPrest.txt";
   return (char *) txt_read_in_mem(name);
}

static void manager_tables_changed(void)
{
   if (glb_ds1edit.lvlprest_buff != NULL)
      glb_ds1edit.lvlprest_buff = txt_destroy(glb_ds1edit.lvlprest_buff);
}

static int manager_need_project(char * error, int cap)
{
   if (glb_project.is_open)
      return 1;
   snprintf(error, cap, "No project is open.");
   return 0;
}

/* The open and selected rows keep pointing at the same rows. If the open one goes,
 * nothing is open as far as the browser knows. */
static void manager_remove_row(int group_idx, int entry_idx)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g = &ab->groups[group_idx];

   memmove(&g->entries[entry_idx], &g->entries[entry_idx + 1],
           sizeof(AREA_DS1_ENTRY_S) * (g->entry_count - entry_idx - 1));
   g->entry_count--;

   if (ab->loaded_group == group_idx)
   {
      if (ab->loaded_entry == entry_idx)
      {
         ab->loaded_group = -1;
         ab->loaded_entry = -1;
      }
      else if (ab->loaded_entry > entry_idx)
         ab->loaded_entry--;
   }
   if (ab->selected_group == group_idx)
   {
      if (ab->selected_entry == entry_idx)
         ab->selected_entry = -1;
      else if (ab->selected_entry > entry_idx)
         ab->selected_entry--;
   }
}

/* Every regular row of this map: a row can list the same file twice. */
static void manager_set_slot_cleared(int def, const char * ds1_path, int cleared)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   size_t len = strlen(ds1_path);
   int i, j;

   for (i = 0; i < ab->group_count; i++)
   {
      if (ab->groups[i].is_backup)
         continue;
      for (j = 0; j < ab->groups[i].entry_count; j++)
      {
         AREA_DS1_ENTRY_S * e = &ab->groups[i].entries[j];

         if (e->lvlprest_def == def && strlen(e->ds1_path) == len &&
             map_state_path_matches(e->ds1_path, ds1_path))
            e->slot_cleared = cleared;
      }
   }
}

/* "<date>" and "Act1-Town" for a backup folder, and the metadata that goes
 * with it. */
static void manager_backup_details(const AREA_GROUP_S * g, const AREA_DS1_ENTRY_S * e,
                                   char * date, int date_cap, char * label, int label_cap,
                                   char * json, int json_cap)
{
   time_t now;
   char stamp[64];

   time(&now);
   strftime(date, date_cap, "%Y-%m-%d", localtime(&now));
   strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));
   if (g->act > 0)
      snprintf(label, label_cap, "Act%d-%s", g->act, g->name);
   else
      snprintf(label, label_cap, "%s", g->name);
   snprintf(json, json_cap,
            "{\n"
            "  \"backup_date\": \"%s\",\n"
            "  \"original_path\": \"%s\",\n"
            "  \"lvlprest\": {\n"
            "    \"def\": %d,\n"
            "    \"lvltype_id\": %d,\n"
            "    \"area\": \"%s\"\n"
            "  }\n"
            "}\n",
            stamp, e->ds1_path, e->lvlprest_def, e->lvltype_id, label);
}

/* The folder part of a table path, slash included, or "" for none. */
static void manager_dir_of(const char * path, char * out, int cap)
{
   const char * a = strrchr(path, '/');
   const char * b = strrchr(path, '\\');
   const char * end = (a != NULL && (b == NULL || a > b)) ? a : b;

   if (end == NULL)
      out[0] = '\0';
   else
      snprintf(out, cap, "%.*s", (int)(end - path) + 1, path);
}

/* The table path for a new map in this Def: beside the Def's other files. */
static int manager_new_path(const AREA_GROUP_S * g, int def, const char * file_name,
                            char * out, int cap, char * error, int error_cap)
{
   char dir[256];
   int j;

   dir[0] = '\0';
   for (j = 0; j < g->entry_count; j++)
   {
      if (g->entries[j].lvlprest_def == def)
      {
         manager_dir_of(g->entries[j].ds1_path, dir, sizeof(dir));
         break;
      }
   }
   if (dir[0] == '\0' && g->entry_count > 0)
      manager_dir_of(g->entries[0].ds1_path, dir, sizeof(dir));
   if (dir[0] == '\0')
   {
      snprintf(error, error_cap, "Couldn't tell which folder %s goes in.", g->name);
      return -1;
   }
   if (snprintf(out, cap, "%s%s", dir, file_name) >= cap)
   {
      snprintf(error, error_cap, "The path for %s is too long.", file_name);
      return -1;
   }
   return 0;
}

int ds1_manager_new_row_path(int group_idx, int def, const char * file_name,
                             char * out, int cap, char * error, int error_cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;

   if (group_idx < 0 || group_idx >= ab->group_count)
   {
      snprintf(error, error_cap, "That area is gone. Pick another.");
      return -1;
   }
   if (manager_new_path(&ab->groups[group_idx], def, file_name, out, cap, error, error_cap) != 0)
      return -1;
   if ((int) strlen(out) > MAP_OPS_TABLE_PATH_MAX)
   {
      snprintf(error, error_cap, "That makes a %d-character path. LvlPrest.txt allows %d.",
               (int) strlen(out), MAP_OPS_TABLE_PATH_MAX);
      return -1;
   }
   return 0;
}

int ds1_manager_name_in_use(int group_idx, int def, const char * file_name)
{
   char rel[256], ignored[160];

   if (ds1_manager_new_row_path(group_idx, def, file_name, rel, sizeof(rel),
                                ignored, sizeof(ignored)) != 0)
      return 0;
   return area_browser_path_known(rel);
}

/* A New map: to the project's backup folder, and out of the table. */
int ds1_manager_delete(int group_idx, int entry_idx, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   AREA_DS1_ENTRY_S e;
   char date[32], label[128], json[1024], backup[512];
   char * table, * seed;
   long len;
   int rc;

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   g = &ab->groups[group_idx];
   if (entry_idx < 0 || entry_idx >= g->entry_count)
      return -1;
   if (ds1_manager_delete_check(group_idx, entry_idx, error, cap) != 0)
      return -1;
   e = g->entries[entry_idx];

   /* A table that can't be read stops this before the file moves. */
   table = map_ops_read_table(glb_project.path, &len);
   if (table == NULL)
   {
      snprintf(error, cap, "The project's LvlPrest.txt can't be read.");
      return -1;
   }
   rc = lvlprest_find_file_slot(table, e.lvlprest_def, e.ds1_path);
   free(table);
   if (rc == LVLPREST_TABLE_UNUSABLE)
   {
      snprintf(error, cap, "The project's LvlPrest.txt has no Def or File columns.");
      return -1;
   }

   /* Read before the file moves, since it can fail for the same reason a table
      write would. A backup row holds at most this much path. */
   seed = manager_seed();
   manager_backup_details(g, &e, date, sizeof(date), label, sizeof(label), json, sizeof(json));
   if (map_ops_backup_file(glb_project.path, e.ds1_path, label, date, json,
                           backup, (int) sizeof(e.ds1_path), error, cap) != 0)
   {
      free(seed);
      return -1;
   }

   rc = map_ops_unregister_new(glb_project.path, seed, e.lvlprest_def, e.ds1_path, error, cap);
   free(seed);
   if (rc != 0)
   {
      /* The table still names it, so the file goes back, and with it the backup
         is gone: no JSON is left to list a row with no data. */
      char ignored[8], table_error[256], jpath[576];
      int n = (int) strlen(backup);

      snprintf(table_error, sizeof(table_error), "%s", error);
      if (map_ops_restore_file(glb_project.path, backup, e.ds1_path, ignored,
                               sizeof(ignored)) == 0 &&
          n > 4 && stricmp(backup + n - 4, ".ds1") == 0)
      {
         char dir[576];

         snprintf(jpath, sizeof(jpath), "%.*s.json", n - 4, backup);
         remove(jpath);
         manager_dir_of(backup, dir, sizeof(dir));
         if (dir[0] != '\0')
            dir[strlen(dir) - 1] = '\0';
         RMDIR(dir);
      }
      else
      {
         snprintf(error, cap, "%s The map's file is kept as a backup at %s.",
                  table_error, backup);
      }
      return -1;
   }

   manager_tables_changed();

   /* Find the open map's row again by path, before this row goes. A deleted open
      map then has no row, and the rescan's lookup finds none. */
   {
      char open_path[256] = "";
      int open_def = 0, was_this = ab->loaded_group == group_idx && ab->loaded_entry == entry_idx;

      if (!was_this && ab->loaded_group >= 0 && ab->loaded_group < ab->group_count &&
          ab->loaded_entry >= 0 && ab->loaded_entry < ab->groups[ab->loaded_group].entry_count)
      {
         snprintf(open_path, sizeof(open_path), "%s",
                  ab->groups[ab->loaded_group].entries[ab->loaded_entry].ds1_path);
         open_def = ab->groups[ab->loaded_group].entries[ab->loaded_entry].lvlprest_def;
      }
      manager_remove_row(group_idx, entry_idx);
      ab->loaded_group = -1;
      ab->loaded_entry = -1;
      if (open_path[0] != '\0')
         area_browser_find_row(open_path, open_def, &ab->loaded_group, &ab->loaded_entry);
   }
   area_browser_rescan_backups();
   return 0;
}

/* A Modified map: the project's copy goes to the backup folder. */
int ds1_manager_revert(int group_idx, int entry_idx, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   char date[32], label[128], json[1024], backup[512];

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   g = &ab->groups[group_idx];
   if (entry_idx < 0 || entry_idx >= g->entry_count)
      return -1;

   manager_backup_details(g, &g->entries[entry_idx], date, sizeof(date), label, sizeof(label),
                          json, sizeof(json));
   if (map_ops_backup_file(glb_project.path, g->entries[entry_idx].ds1_path, label, date, json,
                           backup, (int) sizeof(g->entries[entry_idx].ds1_path), error, cap) != 0)
      return -1;
   area_browser_rescan_backups();
   area_browser_refresh_states();
   return 0;
}

/* The table an edit would start from: the project's, or the one it would be seeded
 * from. NULL if neither can be read. Caller frees. */
static char * manager_current_table(void)
{
   char * table = NULL;
   long len;

   if (glb_project.is_open)
      table = map_ops_read_table(glb_project.path, &len);
   return table != NULL ? table : manager_seed();
}

/* 0 if Delete may go ahead, else -1 with a sentence. */
int ds1_manager_delete_check(int group_idx, int entry_idx, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_DS1_ENTRY_S * e;
   char * table;
   int rc;

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   if (entry_idx < 0 || entry_idx >= ab->groups[group_idx].entry_count)
      return -1;
   e = &ab->groups[group_idx].entries[entry_idx];

   table = manager_current_table();
   if (table == NULL)
   {
      snprintf(error, cap, "LvlPrest.txt can't be read.");
      return -1;
   }
   rc = map_ops_check_unregister_new(table, e->lvlprest_def, e->ds1_path, error, cap);
   free(table);
   return rc;
}

/* 0 if Remove from Project may go ahead, else -1 with a sentence. */
int ds1_manager_remove_check(int group_idx, int entry_idx, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_DS1_ENTRY_S * e;
   char name[] = "Data\\Global\\Excel\\LvlPrest.txt";
   char * game, * table;
   int listed, rc;

   if (group_idx < 0 || group_idx >= ab->group_count ||
       entry_idx < 0 || entry_idx >= ab->groups[group_idx].entry_count)
      return -1;
   e = &ab->groups[group_idx].entries[entry_idx];

   /* Restore Slot puts back what the game's table says. A row it doesn't list
      would have nothing changed and nothing to come back from. */
   game = (char *) txt_read_in_mem_mpq_only(name);
   if (game == NULL)
   {
      snprintf(error, cap, "The game's LvlPrest.txt can't be read.");
      return -1;
   }
   listed = e->lvlprest_def > 0 && lvlprest_lists_path(game, e->lvlprest_def, e->ds1_path);
   free(game);
   if (!listed)
   {
      snprintf(error, cap, "The game's LvlPrest.txt doesn't list %s here, so there's nothing "
               "to take out of the row.", e->ds1_path);
      return -1;
   }

   table = manager_current_table();
   if (table == NULL)
   {
      snprintf(error, cap, "LvlPrest.txt can't be read.");
      return -1;
   }
   rc = map_ops_check_unregister(table, e->lvlprest_def, e->ds1_path, error, cap);
   free(table);
   return rc;
}

/* A Game map: the project's table takes it out of the row. The file isn't touched. */
int ds1_manager_remove_slot(int group_idx, int entry_idx, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_DS1_ENTRY_S * e;
   char * seed;
   int rc;

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   if (entry_idx < 0 || entry_idx >= ab->groups[group_idx].entry_count)
      return -1;
   if (ds1_manager_remove_check(group_idx, entry_idx, error, cap) != 0)
      return -1;
   e = &ab->groups[group_idx].entries[entry_idx];

   seed = manager_seed();
   rc = map_ops_unregister(glb_project.path, seed, e->lvlprest_def, e->ds1_path, error, cap);
   free(seed);
   if (rc != 0)
      return -1;

   manager_tables_changed();
   manager_set_slot_cleared(e->lvlprest_def, e->ds1_path, 1);
   area_browser_refresh_states();
   return 0;
}

/* A Removed map: the game's value goes back in the project's table. */
int ds1_manager_restore_slot(int group_idx, int entry_idx, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_DS1_ENTRY_S * e;
   char name[] = "Data\\Global\\Excel\\LvlPrest.txt";
   char * game, * seed;
   int rc;

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   if (entry_idx < 0 || entry_idx >= ab->groups[group_idx].entry_count)
      return -1;
   e = &ab->groups[group_idx].entries[entry_idx];

   game = (char *) txt_read_in_mem_mpq_only(name);
   if (game == NULL)
   {
      snprintf(error, cap, "The game's LvlPrest.txt can't be read.");
      return -1;
   }
   seed = manager_seed();
   rc = map_ops_restore_slot(glb_project.path, seed, game, e->lvlprest_def, e->ds1_path,
                             error, cap);
   free(seed);
   free(game);
   if (rc != 0)
      return -1;

   manager_tables_changed();
   manager_set_slot_cleared(e->lvlprest_def, e->ds1_path, 0);
   area_browser_refresh_states();
   return 0;
}

int ds1_manager_def_can_take_map(int def, int empty_map, char * error, int cap)
{
   char * table = manager_current_table();
   int rc;

   if (table == NULL)
      return 1;   /* nothing to judge by: the create itself will say */
   rc = empty_map ? map_ops_check_new_empty(table, def, error, cap)
                  : map_ops_check_register(table, def, error, cap);
   if (rc != 0)
      map_ops_dialog_status(table, def, error, cap);
   free(table);
   return rc == 0;
}

void ds1_manager_def_default_size(int def, int * width, int * height)
{
   char * table = manager_current_table();

   if (table == NULL)
      return;
   map_ops_default_size(table, def, width, height);
   free(table);
}

int ds1_manager_def_takes_size(int def, int width, int height, char * error, int cap)
{
   char * table = manager_current_table();
   int rc;

   if (table == NULL)
      return 1;
   rc = map_ops_check_row_size(table, def, width, height, error, cap);
   free(table);
   return rc == 0;
}

/* An Unlisted group's id isn't a LvlType, so the row takes its Def's: otherwise the
 * map opens without its tileset, and comes back differently after a reload. */
static void manager_add_new_row(int group_idx, int def, const char * rel)
{
   AREA_GROUP_S * g = &glb_ds1edit.area_browser.groups[group_idx];
   int lvltype = area_browser_lvltype_for_def(def, g->lvltype_id > 0 ? g->lvltype_id : -1);

   area_group_add_entry(g, lvltype, def, rel);
   area_browser_refresh_states();
   area_browser_scroll_to_row(group_idx, g->entry_count - 1);
}

int ds1_manager_create_empty(int group_idx, int def, const char * file_name,
                             int width, int height, char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   char rel[256];
   char * seed;
   int rc;

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   g = &ab->groups[group_idx];
   if (manager_new_path(g, def, file_name, rel, sizeof(rel), error, cap) != 0)
      return -1;

   seed = manager_seed();
   rc = map_ops_create_empty(glb_project.path, seed, def, rel, width, height,
                             g->act > 0 ? g->act : 1, error, cap);
   free(seed);
   if (rc != 0)
      return -1;

   manager_tables_changed();
   manager_add_new_row(group_idx, def, rel);
   return 0;
}

/* The map as it is in memory, so unsaved edits come along and a map that exists
 * only in an MPQ can be cloned. */
int ds1_manager_clone(int src_ds1_idx, int group_idx, int def, const char * file_name,
                      char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   char rel[256], dest[768], found[768];
   char * seed;
   int rc;

   if (group_idx < 0 || group_idx >= ab->group_count || !manager_need_project(error, cap))
      return -1;
   g = &ab->groups[group_idx];
   if (manager_new_path(g, def, file_name, rel, sizeof(rel), error, cap) != 0)
      return -1;
   if (!ds1_manager_def_takes_size(def, (int) glb_ds1[src_ds1_idx].width,
                                   (int) glb_ds1[src_ds1_idx].height, error, cap))
      return -1;
   if (map_ops_tile_path(glb_project.path, rel, dest, sizeof(dest)) != 0)
   {
      snprintf(error, cap, "The path for %s is too long.", file_name);
      return -1;
   }
   if (map_ops_find_tile(glb_project.path, rel, found, sizeof(found)))
   {
      snprintf(error, cap, "%s already exists in the project.", rel);
      return -1;
   }
   if (ds1_save_copy_checked(src_ds1_idx, dest, error, (size_t) cap) != 0)
      return -1;

   seed = manager_seed();
   rc = map_ops_register(glb_project.path, seed, def, rel, error, cap);
   free(seed);
   if (rc != 0)
   {
      remove(dest);
      return -1;
   }

   manager_tables_changed();
   manager_add_new_row(group_idx, def, rel);
   return 0;
}

/* ---- Generic TXT file cell editor ---- */

/* Read a raw TXT file by RQ_ENUM type. Returns malloc'd buffer. */
static char * txt_read_raw_by_type(RQ_ENUM txt_type, long * out_len)
{
   char path[512];
   FILE * in;
   long len;
   char * buf;
   const char * filenames[] = {
      "LvlTypes.txt", "LvlPrest.txt", "obj.txt", "Objects.txt", "Levels.txt"
   };
   const char * fname;

   if (txt_type >= RQ_MAX) return NULL;
   fname = filenames[txt_type];

   /* Try mod_dir first */
   if (glb_config.mod_dir[0] != NULL)
   {
      if (txt_type == RQ_OBJ)
         sprintf(path, "%s\\%s", glb_config.mod_dir[0], fname);
      else
         sprintf(path, "%s\\Global\\Excel\\%s", glb_config.mod_dir[0], fname);
      ds1_path_normalize(path);
      in = fopen(path, "rb");
      if (in != NULL)
      {
         fseek(in, 0, SEEK_END);
         len = ftell(in);
         fseek(in, 0, SEEK_SET);
         buf = (char *)malloc(len + 1);
         if (buf != NULL) { fread(buf, 1, len, in); buf[len] = '\0'; }
         fclose(in);
         *out_len = len;
         return buf;
      }
   }

   /* Fallback to local assets */
   if (txt_type == RQ_OBJ)
      sprintf(path, "assets/data/%s", fname);
   else
      sprintf(path, "assets/excel/%s", fname);
   in = fopen(path, "rb");
   if (in == NULL) return NULL;
   fseek(in, 0, SEEK_END);
   len = ftell(in);
   fseek(in, 0, SEEK_SET);
   buf = (char *)malloc(len + 1);
   if (buf != NULL) { fread(buf, 1, len, in); buf[len] = '\0'; }
   fclose(in);
   *out_len = len;
   return buf;
}

/* Write a raw TXT file by type to mod_dir. */
static int txt_write_raw_by_type(RQ_ENUM txt_type, const char * buf, long len)
{
   char path[512], dir[512];
   FILE * out;
   const char * filenames[] = {
      "LvlTypes.txt", "LvlPrest.txt", "obj.txt", "Objects.txt", "Levels.txt"
   };

   if (glb_config.mod_dir[0] == NULL) return -1;
   if (txt_type >= RQ_MAX) return -1;

   sprintf(dir, "%s\\Global", glb_config.mod_dir[0]);
   ds1_path_normalize(dir);
   MKDIR(dir);
   sprintf(dir, "%s\\Global\\Excel", glb_config.mod_dir[0]);
   ds1_path_normalize(dir);
   MKDIR(dir);

   if (txt_type == RQ_OBJ)
      sprintf(path, "%s\\%s", glb_config.mod_dir[0], filenames[txt_type]);
   else
      sprintf(path, "%s\\Global\\Excel\\%s", glb_config.mod_dir[0], filenames[txt_type]);
   ds1_path_normalize(path);

   out = fopen(path, "wb");
   if (out == NULL) return -1;
   fwrite(buf, 1, len, out);
   fclose(out);
   printf("ds1_manager_txt_set_cell: wrote %s (%ld bytes)\n", path, len);
   return 0;
}

/* Set a cell value in a TXT file by key column lookup.
 * key_col/key_val identify the row (e.g. "Def"/167 for LvlPrest).
 * target_col is the column to modify. new_value is the new cell text.
 * Returns 0 on success. */
int ds1_manager_txt_set_cell(RQ_ENUM txt_type, const char * key_col, int key_val,
                              const char * target_col, const char * new_value)
{
   char * buf;
   long len;
   char * header_end, * row_start, * row_end, * cell_start, * cell_end;
   int key_col_idx, target_col_idx;
   int cur_col;
   char * p;
   char * new_buf;
   long new_len;

   buf = txt_read_raw_by_type(txt_type, &len);
   if (buf == NULL) return -1;

   /* Find header line end */
   header_end = strchr(buf, '\n');
   if (header_end == NULL) { free(buf); return -1; }

   /* Find column indices in header */
   key_col_idx = lvlprest_find_col(buf, key_col);
   target_col_idx = lvlprest_find_col(buf, target_col);
   if (key_col_idx < 0 || target_col_idx < 0)
   {
      printf("ds1_manager_txt_set_cell: column '%s' or '%s' not found\n",
             key_col, target_col);
      free(buf);
      return -1;
   }

   /* Search rows for matching key value */
   row_start = header_end + 1;
   while (row_start < buf + len && *row_start != '\0')
   {
      char key_buf[64];

      row_end = strchr(row_start, '\n');
      if (row_end == NULL) row_end = buf + len;

      /* Extract key column value from this row */
      p = row_start;
      for (cur_col = 0; cur_col < key_col_idx && p < row_end; cur_col++)
      {
         p = strchr(p, '\t');
         if (p == NULL || p >= row_end) break;
         p++;
      }

      if (cur_col == key_col_idx && p < row_end)
      {
         char * tab = strchr(p, '\t');
         int vlen = (tab && tab < row_end) ? (int)(tab - p) : (int)(row_end - p);
         if (vlen > 0 && vlen < 63)
         {
            strncpy(key_buf, p, vlen);
            key_buf[vlen] = '\0';
            /* Strip \r */
            if (vlen > 0 && key_buf[vlen-1] == '\r') key_buf[vlen-1] = '\0';

            if (atoi(key_buf) == key_val)
            {
               /* Found the row — now find the target column cell */
               cell_start = row_start;
               for (cur_col = 0; cur_col < target_col_idx && cell_start < row_end; cur_col++)
               {
                  cell_start = strchr(cell_start, '\t');
                  if (cell_start == NULL || cell_start >= row_end) break;
                  cell_start++;
               }

               if (cur_col == target_col_idx && cell_start < row_end)
               {
                  char * tab2 = strchr(cell_start, '\t');
                  cell_end = (tab2 && tab2 < row_end) ? tab2 : row_end;
                  /* Skip \r at end of cell value (before \r\n or \t).
                   * We DON'T consume it — the \r stays in the output
                   * as part of the line ending. Only strip it from
                   * the cell value boundary so we replace just the text. */
                  if (cell_end == row_end && cell_end > cell_start &&
                      *(cell_end - 1) == '\r')
                     cell_end--;

                  /* Build new file: before_cell + new_value + after_cell */
                  {
                     int before_len = (int)(cell_start - buf);
                     int after_start = (int)(cell_end - buf);
                     int new_val_len = (int)strlen(new_value);
                     new_len = before_len + new_val_len + (len - after_start);
                     new_buf = (char *)malloc(new_len + 1);
                     if (new_buf != NULL)
                     {
                        memcpy(new_buf, buf, before_len);
                        memcpy(new_buf + before_len, new_value, new_val_len);
                        memcpy(new_buf + before_len + new_val_len,
                               buf + after_start, len - after_start);
                        new_buf[new_len] = '\0';

                        txt_write_raw_by_type(txt_type, new_buf, new_len);
                        free(new_buf);
                     }
                  }
                  free(buf);
                  return 0;
               }
            }
         }
      }

      row_start = (*row_end == '\n') ? row_end + 1 : row_end;
   }

   printf("ds1_manager_txt_set_cell: key %s=%d not found\n", key_col, key_val);
   free(buf);
   return -1;
}


/* ---- Restore and permanent delete ---- */
/* Restore a backup DS1 to its original place in the project, and list it in the
 * project's LvlPrest.txt if the table doesn't already. Returns 0, or -1 with a
 * sentence in error. */
int ds1_manager_restore(int group_idx, int entry_idx, int * row_group, int * row_entry,
                        char * error, int cap)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   AREA_DS1_ENTRY_S * e;
   char backup_ds1_path[512], backup_json_path[512];
   char original_path[256];
   int def, lvltype_id;
   FILE * jf;
   char backup_dir[512];
   char * seed;

   *row_group = -1;
   *row_entry = -1;
   if (group_idx < 0 || group_idx >= ab->group_count)
      return -1;
   g = &ab->groups[group_idx];
   if (!g->is_backup)
      return -1;
   if (entry_idx < 0 || entry_idx >= g->entry_count)
      return -1;
   e = &g->entries[entry_idx];

   /* The entry ds1_path is like "backup/2026-04-06_Act1-Town_TownN1/TownN1.ds1" */
   strncpy(backup_ds1_path, e->ds1_path, sizeof(backup_ds1_path) - 1);
   backup_ds1_path[sizeof(backup_ds1_path) - 1] = '\0';

   /* Build JSON path: replace .ds1 with .json */
   {
      int plen = (int)strlen(backup_ds1_path);
      if (plen > 4 && stricmp(backup_ds1_path + plen - 4, ".ds1") == 0)
      {
         strncpy(backup_json_path, backup_ds1_path, plen - 4);
         strcpy(backup_json_path + plen - 4, ".json");
      }
      else
      {
         snprintf(error, cap, "%s isn't a map backup.", backup_ds1_path);
         return -1;
      }
   }

   /* Extract the backup subdirectory for later cleanup */
   {
      const char * last_slash = strrchr(backup_ds1_path, '/');
      if (last_slash == NULL) last_slash = strrchr(backup_ds1_path, '\\');
      if (last_slash != NULL)
      {
         int dlen = (int)(last_slash - backup_ds1_path);
         strncpy(backup_dir, backup_ds1_path, dlen);
         backup_dir[dlen] = '\0';
      }
      else
      {
         strcpy(backup_dir, ".");
      }
   }

   /* Parse JSON to get original_path, def, lvltype_id */
   original_path[0] = '\0';
   def = e->lvlprest_def;
   lvltype_id = e->lvltype_id;

   jf = fopen(backup_json_path, "rt");
   if (jf != NULL)
   {
      char line[512];
      while (fgets(line, sizeof(line), jf) != NULL)
      {
         if (strstr(line, "\"original_path\"") != NULL)
         {
            char * p = strchr(line, ':');
            if (p != NULL)
            {
               p++; while (*p == ' ' || *p == '"') p++;
               {
                  char * end = strchr(p, '"');
                  if (end) *end = '\0';
                  strncpy(original_path, p, sizeof(original_path) - 1);
                  original_path[sizeof(original_path) - 1] = '\0';
               }
            }
         }
         if (strstr(line, "\"def\"") != NULL)
         {
            char * p = strchr(line, ':');
            if (p != NULL)
               def = atoi(p + 1);
         }
         if (strstr(line, "\"lvltype_id\"") != NULL)
         {
            char * p = strchr(line, ':');
            if (p != NULL)
               lvltype_id = atoi(p + 1);
         }
      }
      fclose(jf);
   }
   else
   {
      snprintf(error, cap, "The backup's details file %s can't be read.", backup_json_path);
      return -1;
   }

   if (original_path[0] == '\0')
   {
      snprintf(error, cap, "The backup's details file doesn't say where the map came from.");
      return -1;
   }

   if (!manager_need_project(error, cap))
      return -1;

   /* Read before the file moves, as ds1_manager_delete does. */
   seed = manager_seed();
   error[0] = '\0';
   if (map_ops_restore_file(glb_project.path, backup_ds1_path, original_path, error, cap) != 0)
   {
      free(seed);
      return -1;
   }
   if (error[0] != '\0')
   {
      /* The map is back, and a backup left behind does no harm. */
      printf("ds1_manager_restore: %s\n", error);
      error[0] = '\0';
   }

   /* Deleting a New map took it out of the row. A Modified map never left, and
      map_ops_register would then list it twice or refuse its row. A project with
      no table of its own lists what the seed does. */
   if (def > 0 && !map_ops_lists_map(glb_project.path, seed, def, original_path) &&
       map_ops_register(glb_project.path, seed, def, original_path, error, cap) != 0)
   {
      /* The file has moved but the table doesn't list it. Put it back in the
         backup so the row, the JSON and a retry all still work. */
      char moved[768], table_error[256];

      free(seed);
      snprintf(table_error, sizeof(table_error), "%s", error);
      if (map_ops_tile_path(glb_project.path, original_path, moved, sizeof(moved)) != 0 ||
          file_copy_checked(moved, backup_ds1_path) != FILE_COPY_OK ||
          remove(moved) != 0)
         snprintf(error, cap, "%s The map's file is in the project at %s, not listed "
                  "in LvlPrest.txt.", table_error, moved);
      return -1;
   }
   free(seed);

   manager_tables_changed();

   /* Add a row, unless the map already has one. A backup from a Def with no LvlType
      group names no usable LvlType. This can move the groups, so `g` and `e` are
      done with here. */
   {
      int gi, ei;

      if (area_browser_find_row(original_path, def, &gi, &ei))
      {
         /* A Removed row: the table lists the map again now. */
         manager_set_slot_cleared(def, original_path, 0);
         *row_group = gi;
         *row_entry = ei;
      }
      else
      {
         if (lvltype_id <= 0)
            lvltype_id = area_browser_lvltype_for_def(def, -1);
         if (area_browser_add_map_row(def, lvltype_id, original_path, &gi, &ei))
         {
            *row_group = gi;
            *row_entry = ei;
         }
      }
   }

   /* Remove the backup entry from the backup group. map_ops_restore_file moved
      the .ds1, so only the JSON and the folder are left. */
   manager_remove_row(group_idx, entry_idx);
   remove(backup_json_path);
   RMDIR(backup_dir);  /* Will only succeed if empty */

   area_browser_refresh_states();
   area_browser_scroll_to_row(*row_group, *row_entry);

   printf("ds1_manager_restore: complete\n");
   fflush(stdout);
   return 0;
}

/* Permanently delete a backup DS1 (remove from backup/ without restoring).
 * Returns 0 on success, -1 on error. */
int ds1_manager_delete_permanent(int group_idx, int entry_idx)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   AREA_DS1_ENTRY_S * e;
   char backup_ds1_path[512], backup_json_path[512], backup_dir[512];

   if (group_idx < 0 || group_idx >= ab->group_count) return -1;
   g = &ab->groups[group_idx];
   if (!g->is_backup) return -1;
   if (entry_idx < 0 || entry_idx >= g->entry_count) return -1;
   e = &g->entries[entry_idx];

   strncpy(backup_ds1_path, e->ds1_path, sizeof(backup_ds1_path) - 1);
   backup_ds1_path[sizeof(backup_ds1_path) - 1] = '\0';

   /* Build JSON path */
   {
      int plen = (int)strlen(backup_ds1_path);
      if (plen > 4 && stricmp(backup_ds1_path + plen - 4, ".ds1") == 0)
      {
         strncpy(backup_json_path, backup_ds1_path, plen - 4);
         strcpy(backup_json_path + plen - 4, ".json");
      }
      else
      {
         sprintf(backup_json_path, "%s.json", backup_ds1_path);
      }
   }

   /* Extract backup subdirectory */
   {
      const char * last_slash = strrchr(backup_ds1_path, '/');
      if (last_slash == NULL) last_slash = strrchr(backup_ds1_path, '\\');
      if (last_slash != NULL)
      {
         int dlen = (int)(last_slash - backup_ds1_path);
         strncpy(backup_dir, backup_ds1_path, dlen);
         backup_dir[dlen] = '\0';
      }
      else
      {
         strcpy(backup_dir, ".");
      }
   }

   /* Delete files */
   remove(backup_ds1_path);
   remove(backup_json_path);
   RMDIR(backup_dir);  /* Only succeeds if empty */

   printf("ds1_manager_delete_permanent: removed %s\n", backup_ds1_path);

   /* Remove entry from backup group */
   manager_remove_row(group_idx, entry_idx);

   printf("ds1_manager_delete_permanent: complete\n");
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
