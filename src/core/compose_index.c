#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/compose_index.h"

#ifdef _WIN32
#define cidx_stricmp _stricmp
#define cidx_strnicmp _strnicmp
#else
#include <strings.h>
#define cidx_stricmp strcasecmp
#define cidx_strnicmp strncasecmp
#endif

/* In-memory storage shared with compose_index_build.c. Not static
 * because the build glue refers to these by external name. */
COMPOSE_TOKEN_S compose_index_storage_monsters[COMPOSE_INDEX_MAX_PER_CATEGORY];
COMPOSE_TOKEN_S compose_index_storage_npcs    [COMPOSE_INDEX_MAX_PER_CATEGORY];
COMPOSE_TOKEN_S compose_index_storage_objects [COMPOSE_INDEX_MAX_PER_CATEGORY];
int compose_index_storage_monster_count = 0;
int compose_index_storage_npc_count     = 0;
int compose_index_storage_object_count  = 0;

void compose_index_reset(void)
{
   compose_index_storage_monster_count = 0;
   compose_index_storage_npc_count     = 0;
   compose_index_storage_object_count  = 0;
}

int compose_index_monster_count(void) { return compose_index_storage_monster_count; }
int compose_index_npc_count    (void) { return compose_index_storage_npc_count;     }
int compose_index_object_count (void) { return compose_index_storage_object_count;  }

const COMPOSE_TOKEN_S *compose_index_monster_at(int idx)
{
   if (idx < 0 || idx >= compose_index_storage_monster_count) return NULL;
   return &compose_index_storage_monsters[idx];
}
const COMPOSE_TOKEN_S *compose_index_npc_at(int idx)
{
   if (idx < 0 || idx >= compose_index_storage_npc_count) return NULL;
   return &compose_index_storage_npcs[idx];
}
const COMPOSE_TOKEN_S *compose_index_object_at(int idx)
{
   if (idx < 0 || idx >= compose_index_storage_object_count) return NULL;
   return &compose_index_storage_objects[idx];
}

/* ---------------------------------------------------------------- */
/* Minimal D2 TXT parser. The format is tab-separated, with a header */
/* row. We do not need to interpret any field types -- we just split */
/* lines on \t and take the columns we need by header name.          */
/* ---------------------------------------------------------------- */

typedef struct LINE_S
{
   const char *start;
   int length;        /* not including the line terminator */
} LINE_S;

static int next_line(const char *text, int len, int *pos, LINE_S *out)
{
   int p = *pos;
   int line_start;
   if (p >= len) return 0;
   line_start = p;
   while (p < len && text[p] != '\n' && text[p] != '\r')
      p++;
   out->start = text + line_start;
   out->length = p - line_start;
   while (p < len && (text[p] == '\n' || text[p] == '\r'))
      p++;
   *pos = p;
   return 1;
}

/* Find the column index whose header (case-insensitive) matches one
 * of the candidates. Returns -1 if not found. */
static int find_column(const char *header, int header_len,
                       const char *const *candidates)
{
   int col = 0;
   int p = 0;
   while (p < header_len)
   {
      int field_start = p;
      while (p < header_len && header[p] != '\t')
         p++;
      {
         int field_len = p - field_start;
         int i;
         for (i = 0; candidates[i] != NULL; i++)
         {
            int cand_len = (int) strlen(candidates[i]);
            if (cand_len == field_len
                && cidx_strnicmp(header + field_start, candidates[i], cand_len) == 0)
               return col;
         }
      }
      col++;
      if (p < header_len && header[p] == '\t')
         p++;
   }
   return -1;
}

/* Extract the n-th tab-separated field from `line` (length line_len)
 * into `out_buf` (capped at out_cap-1 chars). Returns 1 on success,
 * 0 if the field index is past the end. */
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
      while (p < line_len && line[p] != '\t')
         p++;
      if (col == field_idx)
      {
         int n = p - field_start;
         if (n > out_cap - 1) n = out_cap - 1;
         memcpy(out_buf, line + field_start, (size_t) n);
         out_buf[n] = 0;
         return 1;
      }
      col++;
      if (p < line_len && line[p] == '\t')
         p++;
   }
   return 0;
}

/* Strip leading/trailing whitespace in place. */
static void strip_ws(char *s)
{
   int n;
   char *start = s;
   while (*start == ' ' || *start == '\t')
      start++;
   if (start != s)
      memmove(s, start, strlen(start) + 1);
   n = (int) strlen(s);
   while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r'))
      n--;
   s[n] = 0;
}

/* ---------------------------------------------------------------- */
/* MonStats.txt parser                                              */
/* ---------------------------------------------------------------- */

int compose_index_parse_monstats(const char *txt_text,
                                 COMPOSE_TOKEN_S *monster_out, int monster_cap,
                                 int *monster_count_out,
                                 COMPOSE_TOKEN_S *npc_out, int npc_cap,
                                 int *npc_count_out)
{
   int len;
   int pos = 0;
   LINE_S header_line;
   LINE_S row;
   int col_code, col_id, col_npc, col_namestr, col_mon_stats_ex;
   int monster_n = 0, npc_n = 0;
   static const char *code_names[]    = { "Code", NULL };
   static const char *id_names[]      = { "Id", "ID", NULL };
   static const char *npc_names[]     = { "npc", "Npc", "NPC", NULL };
   static const char *namestr_names[] = { "NameStr", "namestr", NULL };
   static const char *mse_names[]     = { "MonStatsEx", "monstatsex", NULL };

   if (txt_text == NULL || monster_count_out == NULL || npc_count_out == NULL)
      return 0;
   *monster_count_out = 0;
   *npc_count_out     = 0;

   len = (int) strlen(txt_text);
   if (!next_line(txt_text, len, &pos, &header_line))
      return 0;

   col_code         = find_column(header_line.start, header_line.length, code_names);
   col_id           = find_column(header_line.start, header_line.length, id_names);
   col_npc          = find_column(header_line.start, header_line.length, npc_names);
   col_namestr      = find_column(header_line.start, header_line.length, namestr_names);
   col_mon_stats_ex = find_column(header_line.start, header_line.length, mse_names);

   if (col_code < 0 || col_id < 0)
      return 0;
   /* col_npc / col_namestr may be -1; we degrade gracefully. */

   while (next_line(txt_text, len, &pos, &row))
   {
      char code_buf[COMPOSE_TOKEN_CODE_MAX];
      char id_buf  [COMPOSE_TOKEN_NAME_MAX];
      char npc_buf [16];
      int is_npc = 0;
      COMPOSE_TOKEN_S tok;

      if (row.length <= 0) continue;
      if (!field_at(row.start, row.length, col_code, code_buf, sizeof(code_buf)))
         continue;
      strip_ws(code_buf);
      if (code_buf[0] == 0) continue;
      /* D2 TXTs commonly have an "Expansion" sentinel row that
       * starts with "Expansion" in some columns; skip if Code is
       * non-alphanumeric. */
      if (!isalnum((unsigned char) code_buf[0])) continue;
      /* Code "xx" (case-insensitive) is D2's placeholder for unused /
       * deleted MonStats rows -- they have no sprite and would just
       * spam the iterator with hundreds of failed COF probes. */
      if ((code_buf[0] == 'x' || code_buf[0] == 'X')
          && (code_buf[1] == 'x' || code_buf[1] == 'X')
          && code_buf[2] == 0)
         continue;

      if (!field_at(row.start, row.length, col_id, id_buf, sizeof(id_buf)))
         id_buf[0] = 0;
      strip_ws(id_buf);

      if (col_npc >= 0
          && field_at(row.start, row.length, col_npc, npc_buf, sizeof(npc_buf)))
      {
         strip_ws(npc_buf);
         if (npc_buf[0] == '1')
            is_npc = 1;
      }

      memset(&tok, 0, sizeof(tok));
      strncpy(tok.code, code_buf, sizeof(tok.code) - 1);
      if (id_buf[0] != 0)
         strncpy(tok.name, id_buf, sizeof(tok.name) - 1);
      else
         strncpy(tok.name, code_buf, sizeof(tok.name) - 1);

      /* MonStatsEx is the join key into MonStats2.txt; empty if the
       * column isn't present (graceful) or the row has nothing. */
      if (col_mon_stats_ex >= 0)
      {
         char mse_buf[COMPOSE_TOKEN_NAME_MAX];
         if (field_at(row.start, row.length, col_mon_stats_ex,
                      mse_buf, sizeof(mse_buf)))
         {
            strip_ws(mse_buf);
            strncpy(tok.mon_stats_ex, mse_buf, sizeof(tok.mon_stats_ex) - 1);
         }
      }

      /* Dedupe by Code. Many MonStats rows share the same Code
       * (skeleton1 / skeleton2 / skeleton3 all -> "SK") and they all
       * point at the same sprite folder, so iterating each one is
       * pure waste. Keep the first row that introduces a Code. */
      {
         COMPOSE_TOKEN_S *target_arr = is_npc ? npc_out : monster_out;
         int target_n = is_npc ? npc_n : monster_n;
         int dup;
         for (dup = 0; dup < target_n; dup++)
         {
            if (cidx_stricmp(target_arr[dup].code, tok.code) == 0)
               break;
         }
         if (dup < target_n) continue;  /* already have this Code */
      }

      if (is_npc)
      {
         if (npc_n < npc_cap)
            npc_out[npc_n++] = tok;
      }
      else
      {
         if (monster_n < monster_cap)
            monster_out[monster_n++] = tok;
      }
   }

   *monster_count_out = monster_n;
   *npc_count_out     = npc_n;
   return 1;
}

/* ---------------------------------------------------------------- */
/* Objects.txt parser                                               */
/* ---------------------------------------------------------------- */

int compose_index_parse_objects(const char *txt_text,
                                COMPOSE_TOKEN_S *out, int cap,
                                int *count_out)
{
   int len;
   int pos = 0;
   LINE_S header_line, row;
   int col_token, col_name;
   int n = 0;
   static const char *token_names[] = { "Token", "token", NULL };
   static const char *name_names[]  = { "Name", "name", NULL };

   if (txt_text == NULL || count_out == NULL)
      return 0;
   *count_out = 0;

   len = (int) strlen(txt_text);
   if (!next_line(txt_text, len, &pos, &header_line))
      return 0;

   col_token = find_column(header_line.start, header_line.length, token_names);
   col_name  = find_column(header_line.start, header_line.length, name_names);
   if (col_token < 0)
      return 0;

   while (next_line(txt_text, len, &pos, &row))
   {
      char token_buf[COMPOSE_TOKEN_CODE_MAX];
      char name_buf [COMPOSE_TOKEN_NAME_MAX];
      COMPOSE_TOKEN_S tok;

      if (row.length <= 0) continue;
      if (!field_at(row.start, row.length, col_token, token_buf, sizeof(token_buf)))
         continue;
      strip_ws(token_buf);
      if (token_buf[0] == 0) continue;
      if (!isalnum((unsigned char) token_buf[0])) continue;

      name_buf[0] = 0;
      if (col_name >= 0)
         field_at(row.start, row.length, col_name, name_buf, sizeof(name_buf));
      strip_ws(name_buf);

      memset(&tok, 0, sizeof(tok));
      strncpy(tok.code, token_buf, sizeof(tok.code) - 1);
      if (name_buf[0] != 0)
         strncpy(tok.name, name_buf, sizeof(tok.name) - 1);
      else
         strncpy(tok.name, token_buf, sizeof(tok.name) - 1);

      if (n < cap)
         out[n++] = tok;
   }

   *count_out = n;
   return 1;
}

/* compose_index_build lives in compose_index_build.c so unit tests
 * can link the pure parsers without dragging in misc_load_mpq_file. */
