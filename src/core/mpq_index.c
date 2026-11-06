#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "structs.h"
#include "error.h"
#include "misc.h"
#include "core/txtread.h"
#include "core/mpq_index.h"
#include "core/area_metadata.h"

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static PRESET_ENTRY_S *s_presets    = NULL;
static int             s_preset_cap = 0;
static int             s_preset_num = 0;
static int             s_ready      = 0;
static SUB_ENTRY_S    *s_subs       = NULL;
static int             s_sub_num    = 0;

// How many levels of each tileset use each Levels.Pal value.
#define MPQ_INDEX_TYPE_MAX 256
static int             s_type_pal[MPQ_INDEX_TYPE_MAX][5];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Pointer to column `col_idx` of row `row` in `txt`. Returns NULL if out of
// range or if the column isn't present.
static char *row_col_ptr(TXT_S *txt, int row, int col_idx)
{
   if (txt == NULL || col_idx < 0 || col_idx >= txt->col_num) return NULL;
   return txt->data + (row * txt->line_size) + txt->col[col_idx].offset;
}

// Read a long from a numeric column, or `default_val` if col is missing.
static long read_long_col(TXT_S *txt, int row, const char *col_name,
                          RQ_ENUM rq, long default_val)
{
   int col_idx;
   char *p;

   col_idx = misc_get_txt_column_num(rq, (char *) col_name);
   if (col_idx < 0) return default_val;
   p = row_col_ptr(txt, row, col_idx);
   if (p == NULL) return default_val;
   if (txt->col[col_idx].type == CT_NUM) return *(long *) p;
   return atol(p);
}

// Copy a string column into `out`, truncating as needed. Missing -> empty.
static void read_str_col(TXT_S *txt, int row, const char *col_name,
                         RQ_ENUM rq, char *out, int out_cap)
{
   int col_idx;
   char *p;

   out[0] = 0;
   col_idx = misc_get_txt_column_num(rq, (char *) col_name);
   if (col_idx < 0) return;
   p = row_col_ptr(txt, row, col_idx);
   if (p == NULL) return;
   if (txt->col[col_idx].type == CT_STR)
   {
      strncpy(out, p, out_cap - 1);
      out[out_cap - 1] = 0;
   }
   else if (txt->col[col_idx].type == CT_NUM)
   {
      snprintf(out, out_cap, "%ld", *(long *) p);
   }
}

static void preset_array_grow(int min_cap)
{
   int new_cap = s_preset_cap ? s_preset_cap : 256;
   while (new_cap < min_cap) new_cap *= 2;
   if (new_cap == s_preset_cap) return;
   s_presets = (PRESET_ENTRY_S *) realloc(s_presets,
                                          new_cap * sizeof(PRESET_ENTRY_S));
   if (s_presets == NULL)
   {
      fprintf(stderr, "mpq_index: out of memory growing to %d entries\n", new_cap);
      s_preset_cap = 0;
      s_preset_num = 0;
      return;
   }
   s_preset_cap = new_cap;
}

// ---------------------------------------------------------------------------
// Levels / LvlTypes join: small lookup tables held just for the build pass
// ---------------------------------------------------------------------------

typedef struct LEVELS_ROW_S
{
   int id;
   int act;
   int pal;
   int level_type;
   int sub_type;
} LEVELS_ROW_S;

typedef struct LVLTYPES_ROW_S
{
   int  id;
   char name[MPQ_INDEX_TYPE_NAME_LEN];
} LVLTYPES_ROW_S;

static int find_level(LEVELS_ROW_S *rows, int n, int id)
{
   int i;
   for (i = 0; i < n; i++) if (rows[i].id == id) return i;
   return -1;
}

static int find_lvltype(LVLTYPES_ROW_S *rows, int n, int id)
{
   int i;
   for (i = 0; i < n; i++) if (rows[i].id == id) return i;
   return -1;
}

// LvlSub.txt names its maps by sub-type, and Levels.SubType says who places
// them. Only some sub-types have a level; the rest stay unlinked. The table is
// a nicety, so a mod without it costs nothing.
static void build_sub_entries(const LEVELS_ROW_S *levels_rows, int levels_num)
{
   TXT_S *lvlsub = txt_load_lvlsub();
   char file[MPQ_INDEX_PATH_LEN];
   int i, l;

   if (lvlsub == NULL) return;

   s_subs = (SUB_ENTRY_S *) calloc(lvlsub->line_num > 0 ? lvlsub->line_num : 1,
                                   sizeof(SUB_ENTRY_S));
   if (s_subs != NULL)
   {
      for (i = 0; i < lvlsub->line_num; i++)
      {
         const int sub_type = (int) read_long_col(lvlsub, i, "Type", RQ_LVLSUB, -1);
         int level_type = 0;

         read_str_col(lvlsub, i, "File", RQ_LVLSUB, file, MPQ_INDEX_PATH_LEN);
         if (file[0] == 0 || (file[0] == '0' && file[1] == 0)) continue;

         for (l = 0; l < levels_num; l++)
         {
            if (levels_rows[l].id > 0 && levels_rows[l].level_type > 0
                && levels_rows[l].sub_type == sub_type)
               level_type = area_merge_level_type(level_type, levels_rows[l].level_type);
         }

         strcpy(s_subs[s_sub_num].file, file);
         s_subs[s_sub_num].level_type = level_type;
         s_sub_num++;
      }
   }
   txt_destroy(lvlsub);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void mpq_index_destroy(void)
{
   free(s_presets);
   s_presets    = NULL;
   s_preset_cap = 0;
   s_preset_num = 0;
   s_ready      = 0;
   free(s_subs);
   s_subs       = NULL;
   s_sub_num    = 0;
   memset(s_type_pal, 0, sizeof(s_type_pal));
}

int mpq_index_sub_level_type(const char *ds1_path)
{
   if (!s_ready) return -1;
   return mpq_index_sub_level_type_in(s_subs, s_sub_num, ds1_path);
}

int mpq_index_lvltype_pal(int level_type)
{
   if (!s_ready || level_type <= 0 || level_type >= MPQ_INDEX_TYPE_MAX) return -1;
   return area_most_common_pal(s_type_pal[level_type]);
}

int mpq_index_is_ready(void) { return s_ready; }
int mpq_index_preset_count(void) { return s_ready ? s_preset_num : 0; }

const PRESET_ENTRY_S *mpq_index_preset_at(int idx)
{
   if (!s_ready || idx < 0 || idx >= s_preset_num) return NULL;
   return &s_presets[idx];
}

const PRESET_ENTRY_S *mpq_index_find_by_ds1_name(const char *ds1_basename,
                                                 int *out_file_slot)
{
   if (!s_ready) return NULL;
   return mpq_index_find_by_ds1_name_in(s_presets, s_preset_num,
                                        ds1_basename, out_file_slot);
}

const PRESET_ENTRY_S *mpq_index_find_by_ds1_path(const char *ds1_path,
                                                 int *out_file_slot)
{
   if (!s_ready) return NULL;
   return mpq_index_find_by_ds1_path_in(s_presets, s_preset_num,
                                       ds1_path, out_file_slot);
}

int mpq_index_build(void)
{
   char *buff = NULL;
   TXT_S *lvlprest = NULL, *lvltypes = NULL, *levels = NULL;
   LEVELS_ROW_S   *levels_rows   = NULL;
   LVLTYPES_ROW_S *lvltypes_rows = NULL;
   int levels_n = 0, lvltypes_n = 0;
   int i, f, file_col, file_idx;
   char tmp_path[MPQ_INDEX_PATH_LEN];
   PRESET_ENTRY_S *e;

   mpq_index_destroy();

   // Prefer the globally-cached TXT_S if one is already loaded (e.g. if a
   // DS1 was opened first and warmed the cache). Otherwise load from MPQ.
   /* Route through the shared loaders rather than repeating the
    * read-then-parse dance three times. They cache into the same globals and
    * carry the archive fallback, so a mod shipping a trimmed table no longer
    * takes the preset index down with it. */
   if (txt_ensure_lvlprest() != 0) goto fail;
   lvlprest = glb_ds1edit.lvlprest_buff;

   if (txt_ensure_lvltypes() != 0) goto fail;
   lvltypes = glb_ds1edit.lvltypes_buff;

   if (read_levels_txt() != 0) goto fail;
   levels = glb_ds1edit.levels_buff;

   // Build a tiny Levels.id -> (act, level_type) lookup.
   levels_rows = (LEVELS_ROW_S *) calloc(levels->line_num, sizeof(LEVELS_ROW_S));
   if (levels_rows == NULL) goto fail;
   for (i = 0; i < levels->line_num; i++)
   {
      levels_rows[i].id         = (int) read_long_col(levels, i, "Id",        RQ_LEVELS, -1);
      levels_rows[i].act        = (int) read_long_col(levels, i, "Act",       RQ_LEVELS,  0);
      levels_rows[i].pal        = (int) read_long_col(levels, i, "Pal",       RQ_LEVELS, -1);
      levels_rows[i].level_type = (int) read_long_col(levels, i, "LevelType", RQ_LEVELS, -1);
      levels_rows[i].sub_type   = (int) read_long_col(levels, i, "SubType",   RQ_LEVELS, -1);
      if (levels_rows[i].id >= 0) levels_n++;
      if (levels_rows[i].id > 0
          && levels_rows[i].level_type > 0 && levels_rows[i].level_type < MPQ_INDEX_TYPE_MAX
          && levels_rows[i].pal >= 0 && levels_rows[i].pal <= 4)
         s_type_pal[levels_rows[i].level_type][levels_rows[i].pal]++;
   }

   build_sub_entries(levels_rows, levels->line_num);

   // LvlTypes.id -> name.
   lvltypes_rows = (LVLTYPES_ROW_S *) calloc(lvltypes->line_num,
                                             sizeof(LVLTYPES_ROW_S));
   if (lvltypes_rows == NULL) goto fail;
   for (i = 0; i < lvltypes->line_num; i++)
   {
      lvltypes_rows[i].id = (int) read_long_col(lvltypes, i, "Id", RQ_LVLTYPE, -1);
      read_str_col(lvltypes, i, "Name", RQ_LVLTYPE,
                   lvltypes_rows[i].name, MPQ_INDEX_TYPE_NAME_LEN);
      if (lvltypes_rows[i].id >= 0) lvltypes_n++;
   }

   // Walk LvlPrest and build PRESET_ENTRY_S rows.
   preset_array_grow(lvlprest->line_num);
   if (s_preset_cap == 0) goto fail;

   for (i = 0; i < lvlprest->line_num; i++)
   {
      int def      = (int) read_long_col(lvlprest, i, "Def",     RQ_LVLPREST, -1);
      int level_id = (int) read_long_col(lvlprest, i, "LevelId", RQ_LVLPREST, -1);
      if (def < 0) continue; // blank / padding row

      e = &s_presets[s_preset_num];
      memset(e, 0, sizeof(*e));
      e->def      = def;
      e->level_id = level_id;
      read_str_col(lvlprest, i, "Name", RQ_LVLPREST,
                   e->name, MPQ_INDEX_NAME_LEN);

      // File1..File6
      for (f = 0; f < MPQ_INDEX_PRESET_FILES_MAX; f++)
      {
         char col_name[8];
         snprintf(col_name, sizeof(col_name), "File%d", f + 1);
         read_str_col(lvlprest, i, col_name, RQ_LVLPREST,
                      tmp_path, MPQ_INDEX_PATH_LEN);
         if (tmp_path[0] == 0 || (tmp_path[0] == '0' && tmp_path[1] == 0))
            continue;
         strncpy(e->ds1_files[e->ds1_count], tmp_path, MPQ_INDEX_PATH_LEN - 1);
         e->ds1_files[e->ds1_count][MPQ_INDEX_PATH_LEN - 1] = 0;
         e->ds1_count++;
      }

      // Levels.Act is zero-based. LevelId 0 is the Null placeholder, so use
      // the Act prefix in LvlPrest.Name when a preset has no real level row.
      e->level_type = -1;
      e->pal        = -1;
      e->act        = area_preset_resolve_act(level_id, -1, e->name);
      file_idx = level_id > 0
               ? find_level(levels_rows, levels->line_num, level_id) : -1;
      if (file_idx >= 0)
      {
         e->act = area_preset_resolve_act(level_id,
                                          levels_rows[file_idx].act, e->name);
         e->level_type = levels_rows[file_idx].level_type;
         e->pal        = levels_rows[file_idx].pal;
      }

      // Join LvlTypes -> type_name
      if (e->level_type >= 0)
      {
         file_col = find_lvltype(lvltypes_rows, lvltypes->line_num,
                                 e->level_type);
         if (file_col >= 0)
         {
            strncpy(e->type_name, lvltypes_rows[file_col].name,
                    MPQ_INDEX_TYPE_NAME_LEN - 1);
            e->type_name[MPQ_INDEX_TYPE_NAME_LEN - 1] = 0;
         }
      }

      s_preset_num++;
   }

   free(levels_rows);
   free(lvltypes_rows);

   s_ready = 1;
   fprintf(stdout, "mpq_index: built %d presets\n", s_preset_num);
   fprintf(stderr, "mpq_index: built %d presets\n", s_preset_num);
   return s_preset_num;

fail:
   free(buff);
   free(levels_rows);
   free(lvltypes_rows);
   mpq_index_destroy();
   fprintf(stderr, "mpq_index: build failed\n");
   return -1;
}
