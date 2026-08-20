#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/monstats2.h"

#ifdef _WIN32
#define ms2_stricmp _stricmp
#define ms2_strnicmp _strnicmp
#else
#include <strings.h>
#define ms2_stricmp strcasecmp
#define ms2_strnicmp strncasecmp
#endif

/* Real D2/LOD MonStats2.txt has ~270 rows, but the parser also keeps
 * malformed/sentinel rows in the count when scanning -- and we want
 * headroom for mod content too. 1024 is plenty without being wasteful. */
#define MONSTATS2_MAX_ROWS  1024

/* ------------------------------------------------------------------ */
/* In-memory index. Identical pattern to compose_index_storage_*.     */
/* ------------------------------------------------------------------ */
static MONSTATS2_ENTRY_S s_storage[MONSTATS2_MAX_ROWS];
static int s_count = 0;

void monstats2_reset(void)
{
   s_count = 0;
}

const MONSTATS2_ENTRY_S *monstats2_find(const char *id)
{
   int i;
   if (id == NULL || id[0] == 0) return NULL;
   for (i = 0; i < s_count; i++)
   {
      if (ms2_stricmp(s_storage[i].id, id) == 0)
         return &s_storage[i];
   }
   return NULL;
}

/* ------------------------------------------------------------------ */
/* Pure parser                                                        */
/* ------------------------------------------------------------------ */

typedef struct LINE_S
{
   const char *start;
   int length;
} LINE_S;

static int next_line(const char *text, int len, int *pos, LINE_S *out)
{
   int p = *pos;
   int line_start;
   if (p >= len) return 0;
   line_start = p;
   while (p < len && text[p] != '\n' && text[p] != '\r') p++;
   out->start = text + line_start;
   out->length = p - line_start;
   while (p < len && (text[p] == '\n' || text[p] == '\r')) p++;
   *pos = p;
   return 1;
}

static int find_column(const char *header, int header_len,
                       const char *target)
{
   int col = 0;
   int p = 0;
   int target_len = (int) strlen(target);
   while (p < header_len)
   {
      int field_start = p;
      int field_len;
      while (p < header_len && header[p] != '\t') p++;
      field_len = p - field_start;
      if (field_len == target_len
          && ms2_strnicmp(header + field_start, target, (size_t) target_len) == 0)
         return col;
      col++;
      if (p < header_len && header[p] == '\t') p++;
   }
   return -1;
}

static int field_at(const char *line, int line_len, int field_idx,
                    char *out_buf, int out_cap)
{
   int col = 0;
   int p = 0;
   if (out_cap <= 0) return 0;
   out_buf[0] = 0;
   while (p < line_len)
   {
      int field_start = p;
      while (p < line_len && line[p] != '\t') p++;
      if (col == field_idx)
      {
         int n = p - field_start;
         if (n > out_cap - 1) n = out_cap - 1;
         memcpy(out_buf, line + field_start, (size_t) n);
         out_buf[n] = 0;
         return 1;
      }
      col++;
      if (p < line_len && line[p] == '\t') p++;
   }
   return 0;
}

/* Strip whitespace + matching surrounding quotes. The .txt files
 * sometimes wrap comma-separated lists in double-quotes (e.g.
 * "lit,med,hvy"). */
static void strip_quotes_ws(char *s)
{
   int n;
   char *start = s;
   while (*start == ' ' || *start == '\t' || *start == '"')
      start++;
   if (start != s)
      memmove(s, start, strlen(start) + 1);
   n = (int) strlen(s);
   while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r'
                    || s[n-1] == '"'))
      n--;
   s[n] = 0;
}

/* Take the first comma-separated entry from a string. So
 * "lit,med,hvy" -> "lit". */
static void first_csv_entry(char *s)
{
   char *comma = strchr(s, ',');
   if (comma != NULL) *comma = 0;
   strip_quotes_ws(s);
}

static const char * const s_layer_columns[MONSTATS2_LAYER_COUNT] = {
   "HD", "TR", "LG", "RA", "LA", "RH", "LH", "SH",
   "S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8"
};
static const char * const s_layer_v_columns[MONSTATS2_LAYER_COUNT] = {
   "HDv", "TRv", "LGv", "Rav", "Lav", "RHv", "LHv", "SHv",
   "S1v", "S2v", "S3v", "S4v", "S5v", "S6v", "S7v", "S8v"
};

int monstats2_parse(const char *txt_text,
                    MONSTATS2_ENTRY_S *out, int cap, int *count_out)
{
   int len;
   int pos = 0;
   LINE_S header_line, row;
   int col_id, col_basew;
   int col_layer[MONSTATS2_LAYER_COUNT];
   int col_layer_v[MONSTATS2_LAYER_COUNT];
   int n_out = 0;
   int i;

   if (txt_text == NULL || count_out == NULL) return 0;
   *count_out = 0;

   len = (int) strlen(txt_text);
   if (!next_line(txt_text, len, &pos, &header_line)) return 0;

   col_id    = find_column(header_line.start, header_line.length, "Id");
   col_basew = find_column(header_line.start, header_line.length, "BaseW");
   if (col_id < 0) return 0;
   /* col_basew may be -1 -- some rows don't have one; we degrade. */

   for (i = 0; i < MONSTATS2_LAYER_COUNT; i++)
   {
      col_layer  [i] = find_column(header_line.start, header_line.length,
                                   s_layer_columns[i]);
      col_layer_v[i] = find_column(header_line.start, header_line.length,
                                   s_layer_v_columns[i]);
   }

   while (next_line(txt_text, len, &pos, &row) && n_out < cap)
   {
      char id_buf[MONSTATS2_ID_MAX];
      MONSTATS2_ENTRY_S *e;

      if (row.length <= 0) continue;
      if (!field_at(row.start, row.length, col_id, id_buf, sizeof(id_buf)))
         continue;
      strip_quotes_ws(id_buf);
      if (id_buf[0] == 0) continue;
      /* Skip the "Expansion" sentinel row. */
      if (!isalnum((unsigned char) id_buf[0])) continue;

      e = &out[n_out];
      memset(e, 0, sizeof(*e));
      strncpy(e->id, id_buf, sizeof(e->id) - 1);

      if (col_basew >= 0)
      {
         char buf[32];
         if (field_at(row.start, row.length, col_basew, buf, sizeof(buf)))
         {
            strip_quotes_ws(buf);
            strncpy(e->basew, buf, sizeof(e->basew) - 1);
         }
      }

      for (i = 0; i < MONSTATS2_LAYER_COUNT; i++)
      {
         char buf[64];
         if (col_layer[i] >= 0
             && field_at(row.start, row.length, col_layer[i], buf, sizeof(buf)))
         {
            strip_quotes_ws(buf);
            e->layers[i].used = (buf[0] == '1') ? 1 : 0;
         }
         if (col_layer_v[i] >= 0
             && field_at(row.start, row.length, col_layer_v[i], buf, sizeof(buf)))
         {
            strip_quotes_ws(buf);
            first_csv_entry(buf);
            strncpy(e->layers[i].skin, buf, sizeof(e->layers[i].skin) - 1);
         }
      }

      n_out++;
   }

   *count_out = n_out;
   return 1;
}

/* monstats2_build lives in monstats2_build.c so the pure parser links
 * into unit tests without dragging in misc_load_mpq_file. The shared
 * storage + reset are non-static so the build glue can update them. */
MONSTATS2_ENTRY_S * const monstats2_storage_ptr = s_storage;
const int monstats2_storage_cap = MONSTATS2_MAX_ROWS;

void monstats2_set_count(int n)
{
   if (n < 0) n = 0;
   if (n > MONSTATS2_MAX_ROWS) n = MONSTATS2_MAX_ROWS;
   s_count = n;
}
