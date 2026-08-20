#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/compose_palette_index.h"

#ifdef _WIN32
#define cpi_stricmp _stricmp
#define cpi_strnicmp _strnicmp
#else
#include <strings.h>
#define cpi_stricmp strcasecmp
#define cpi_strnicmp strncasecmp
#endif

#define CPI_MONSTER_ID_MAX  48
#define CPI_MAX_ENTRIES   1024

typedef struct CPI_ENTRY_S
{
   char id[CPI_MONSTER_ID_MAX];
   int  act;          /* 1..5 (file's Act column 0..4 + 1) */
} CPI_ENTRY_S;

static CPI_ENTRY_S s_table[CPI_MAX_ENTRIES];
static int         s_count = 0;

void compose_palette_index_reset(void)
{
   s_count = 0;
}

int compose_palette_index_act_for_monster_id(const char *monstats_id)
{
   int i;
   if (monstats_id == NULL || monstats_id[0] == 0) return 1;
   for (i = 0; i < s_count; i++)
   {
      if (cpi_stricmp(s_table[i].id, monstats_id) == 0)
         return s_table[i].act;
   }
   return 1;  /* default Act 1 */
}

/* Insert with first-wins semantics: only add if id not already there. */
static void emit_first_wins(const char *id, int act, void *userdata)
{
   int i;
   (void) userdata;
   if (id == NULL || id[0] == 0) return;
   if (act < 1 || act > 5) return;
   if (s_count >= CPI_MAX_ENTRIES) return;
   for (i = 0; i < s_count; i++)
   {
      if (cpi_stricmp(s_table[i].id, id) == 0)
         return;  /* already known */
   }
   strncpy(s_table[s_count].id, id, CPI_MONSTER_ID_MAX - 1);
   s_table[s_count].id[CPI_MONSTER_ID_MAX - 1] = 0;
   s_table[s_count].act = act;
   s_count++;
}

/* ------------------------------------------------------------------ */
/* Parser                                                             */
/* ------------------------------------------------------------------ */

typedef struct CPI_LINE_S
{
   const char *start;
   int length;
} CPI_LINE_S;

static int cpi_next_line(const char *text, int len, int *pos, CPI_LINE_S *out)
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

static int cpi_find_column(const char *header, int header_len,
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
          && cpi_strnicmp(header + field_start, target, (size_t) target_len) == 0)
         return col;
      col++;
      if (p < header_len && header[p] == '\t') p++;
   }
   return -1;
}

static int cpi_field_at(const char *line, int line_len, int field_idx,
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

static void cpi_strip_ws(char *s)
{
   int n;
   char *start = s;
   while (*start == ' ' || *start == '\t') start++;
   if (start != s) memmove(s, start, strlen(start) + 1);
   n = (int) strlen(s);
   while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r'))
      n--;
   s[n] = 0;
}

int compose_palette_index_parse_levels(
   const char *txt_text,
   void (*emit)(const char *mon_id, int act, void *userdata),
   void *userdata)
{
   int len;
   int pos = 0;
   CPI_LINE_S header, row;
   int col_act;
   int col_monNN[10];
   int col_nmonNN[10];
   int col_umonNN[10];
   int i;
   /* Levels.txt' Act column is 0-indexed (0..4 for acts 1..5). */
   if (txt_text == NULL || emit == NULL) return 0;

   len = (int) strlen(txt_text);
   if (!cpi_next_line(txt_text, len, &pos, &header)) return 0;

   col_act = cpi_find_column(header.start, header.length, "Act");
   if (col_act < 0) return 0;

   /* mon1..mon10 are required (normal-mode spawns). The nightmare
    * (nmon*) and unique (umon*) variants are best-effort. */
   for (i = 0; i < 10; i++)
   {
      char name[16];
      snprintf(name, sizeof(name), "mon%d", i + 1);
      col_monNN[i] = cpi_find_column(header.start, header.length, name);
      snprintf(name, sizeof(name), "nmon%d", i + 1);
      col_nmonNN[i] = cpi_find_column(header.start, header.length, name);
      snprintf(name, sizeof(name), "umon%d", i + 1);
      col_umonNN[i] = cpi_find_column(header.start, header.length, name);
   }
   if (col_monNN[0] < 0) return 0;

   while (cpi_next_line(txt_text, len, &pos, &row))
   {
      char act_buf[16];
      int act;

      if (row.length <= 0) continue;
      if (!cpi_field_at(row.start, row.length, col_act, act_buf, sizeof(act_buf)))
         continue;
      cpi_strip_ws(act_buf);
      if (!isdigit((unsigned char) act_buf[0])) continue;  /* sentinel rows etc. */
      act = atoi(act_buf) + 1;  /* 0..4 -> 1..5 */
      if (act < 1 || act > 5) continue;

      for (i = 0; i < 10; i++)
      {
         char id_buf[CPI_MONSTER_ID_MAX];
         int colsets[3];
         int j;
         colsets[0] = col_monNN[i];
         colsets[1] = col_nmonNN[i];
         colsets[2] = col_umonNN[i];
         for (j = 0; j < 3; j++)
         {
            int c = colsets[j];
            if (c < 0) continue;
            if (!cpi_field_at(row.start, row.length, c, id_buf, sizeof(id_buf)))
               continue;
            cpi_strip_ws(id_buf);
            if (id_buf[0] == 0) continue;
            emit(id_buf, act, userdata);
         }
      }
   }
   return 1;
}

/* ------------------------------------------------------------------ */
/* MPQ glue                                                           */
/* ------------------------------------------------------------------ */

extern int misc_load_mpq_file(char *filename, char **buffer,
                              long *buf_len, int output);

int compose_palette_index_build(void)
{
   char *buf = NULL;
   long buf_len = 0;
   int ok;

   compose_palette_index_reset();

   if (misc_load_mpq_file("Data\\Global\\Excel\\Levels.txt",
                          &buf, &buf_len, 0) == -1
       || buf == NULL)
      return 0;

   ok = compose_palette_index_parse_levels(buf, emit_first_wins, NULL);
   free(buf);
   return ok;
}
