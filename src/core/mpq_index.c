#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "structs.h"
#include "error.h"
#include "misc.h"
#include "core/txtread.h"
#include "core/mpq_index.h"

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static PRESET_ENTRY_S *s_presets    = NULL;
static int             s_preset_cap = 0;
static int             s_preset_num = 0;
static int             s_ready      = 0;

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
   int level_type;
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
   if (glb_ds1edit.lvlprest_buff != NULL)
      lvlprest = glb_ds1edit.lvlprest_buff;
   else
   {
      buff = (char *) txt_read_in_mem("Data\\Global\\Excel\\LvlPrest.txt");
      if (buff == NULL) goto fail;
      lvlprest = txt_load(buff, RQ_LVLPREST, "Data\\Global\\Excel\\LvlPrest.txt");
      free(buff); buff = NULL;
      if (lvlprest == NULL) goto fail;
      glb_ds1edit.lvlprest_buff = lvlprest;
   }

   if (glb_ds1edit.lvltypes_buff != NULL)
      lvltypes = glb_ds1edit.lvltypes_buff;
   else
   {
      buff = (char *) txt_read_in_mem("Data\\Global\\Excel\\LvlTypes.txt");
      if (buff == NULL) goto fail;
      lvltypes = txt_load(buff, RQ_LVLTYPE, "Data\\Global\\Excel\\LvlTypes.txt");
      free(buff); buff = NULL;
      if (lvltypes == NULL) goto fail;
      glb_ds1edit.lvltypes_buff = lvltypes;
   }

   if (glb_ds1edit.levels_buff != NULL)
      levels = glb_ds1edit.levels_buff;
   else
   {
      buff = (char *) txt_read_in_mem("Data\\Global\\Excel\\Levels.txt");
      if (buff == NULL) goto fail;
      levels = txt_load(buff, RQ_LEVELS, "Data\\Global\\Excel\\Levels.txt");
      free(buff); buff = NULL;
      if (levels == NULL) goto fail;
      glb_ds1edit.levels_buff = levels;
   }

   // Build a tiny Levels.id -> (act, level_type) lookup.
   levels_rows = (LEVELS_ROW_S *) calloc(levels->line_num, sizeof(LEVELS_ROW_S));
   if (levels_rows == NULL) goto fail;
   for (i = 0; i < levels->line_num; i++)
   {
      levels_rows[i].id         = (int) read_long_col(levels, i, "Id",        RQ_LEVELS, -1);
      levels_rows[i].act        = (int) read_long_col(levels, i, "Act",       RQ_LEVELS,  0);
      levels_rows[i].level_type = (int) read_long_col(levels, i, "LevelType", RQ_LEVELS, -1);
      if (levels_rows[i].id >= 0) levels_n++;
   }

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

      // Join Levels -> Act + LevelType
      e->level_type = -1;
      e->act        = 0;
      file_idx = find_level(levels_rows, levels->line_num, level_id);
      if (file_idx >= 0)
      {
         e->act        = levels_rows[file_idx].act;
         e->level_type = levels_rows[file_idx].level_type;
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
