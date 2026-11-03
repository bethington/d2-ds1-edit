#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "core/lvlprest_text.h"

/* Find the column index of a named column in a tab-separated header line.
 * Returns 0-based index, or -1 if not found. */
int lvlprest_find_col(const char * header_line, const char * col_name)
{
   int col = 0;
   const char * p = header_line;
   int name_len = (int)strlen(col_name);

   while (*p != '\0' && *p != '\n' && *p != '\r')
   {
      /* Check if current position matches col_name */
      if (strncasecmp(p, col_name, name_len) == 0)
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
char * lvlprest_find_row(char * buf, int def_col, int target_def, char ** row_end)
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

/* The file name part of a path, with either separator. */
static const char * lvlprest_base_name(const char * path)
{
   const char * base = path;
   const char * p;

   for (p = path; *p != '\0'; p++)
   {
      if (*p == '/' || *p == '\\')
         base = p + 1;
   }
   return base;
}

/* The bounds of a named column in this Def's row. 1 if found. */
static int find_named_cell(char * buf, int def, const char * col_name, char ** start, char ** end)
{
   int def_col, col_at, col = 0;
   char * row, * row_end, * cell;

   def_col = lvlprest_find_col(buf, "Def");
   col_at = lvlprest_find_col(buf, col_name);
   if (def_col < 0 || col_at < 0)
      return 0;
   row = lvlprest_find_row(buf, def_col, def, &row_end);
   if (row == NULL)
      return 0;

   cell = row;
   while (col < col_at && *cell != '\0' && *cell != '\n' && *cell != '\r')
   {
      if (*cell == '\t') col++;
      cell++;
   }
   if (col != col_at)
      return 0;

   *start = cell;
   while (*cell != '\0' && *cell != '\t' && *cell != '\n' && *cell != '\r')
      cell++;
   *end = cell;
   return 1;
}

/* The bounds of File column `slot` in this Def's row. 1 if found. */
static int find_cell(char * buf, int def, int slot, char ** start, char ** end)
{
   char col_name[16];

   if (slot < 1 || slot > 6)
      return 0;
   sprintf(col_name, "File%d", slot);
   return find_named_cell(buf, def, col_name, start, end);
}

static int cell_is_empty(const char * start, const char * end)
{
   return start == end || (end - start == 1 && *start == '0');
}

/* LVLPREST_TABLE_UNUSABLE if the Def or a File column is missing, else 0. */
static int check_columns(char * buf)
{
   char col_name[16];
   int f;

   if (lvlprest_find_col(buf, "Def") < 0)
      return LVLPREST_TABLE_UNUSABLE;
   for (f = 0; f < 6; f++)
   {
      sprintf(col_name, "File%d", f + 1);
      if (lvlprest_find_col(buf, col_name) < 0)
         return LVLPREST_TABLE_UNUSABLE;
   }
   return 0;
}

static int slot_names_file(char * buf, int def, int slot, const char * ds1_path)
{
   const char * name = lvlprest_base_name(ds1_path);
   int name_len = (int)strlen(name);
   char * cell, * cell_end;
   const char * base;

   if (!find_cell(buf, def, slot, &cell, &cell_end))
      return 0;
   base = cell_end;
   while (base > cell && base[-1] != '/' && base[-1] != '\\')
      base--;
   return (int)(cell_end - base) == name_len && strncasecmp(base, name, name_len) == 0;
}

int lvlprest_find_file_slot(char * buf, int def, const char * ds1_path)
{
   int f;

   if (check_columns(buf) != 0)
      return LVLPREST_TABLE_UNUSABLE;
   for (f = 1; f <= 6; f++)
      if (slot_names_file(buf, def, f, ds1_path))
         return f;
   return 0;
}

int lvlprest_slot_holds_path(char * buf, int def, int slot, const char * ds1_path)
{
   size_t len = strlen(ds1_path), i;
   char * cell, * cell_end;

   if (!find_cell(buf, def, slot, &cell, &cell_end) || (size_t)(cell_end - cell) != len)
      return 0;
   for (i = 0; i < len; i++)
   {
      char a = cell[i] == '\\' ? '/' : cell[i];
      char b = ds1_path[i] == '\\' ? '/' : ds1_path[i];

      if (tolower((unsigned char) a) != tolower((unsigned char) b))
         return 0;
   }
   return 1;
}

int lvlprest_find_path_slot(char * buf, int def, const char * ds1_path)
{
   int f;

   if (check_columns(buf) != 0)
      return LVLPREST_TABLE_UNUSABLE;
   for (f = 1; f <= 6; f++)
      if (lvlprest_slot_holds_path(buf, def, f, ds1_path))
         return f;
   return 0;
}

int lvlprest_lists_path(char * buf, int def, const char * ds1_path)
{
   return lvlprest_find_path_slot(buf, def, ds1_path) > 0;
}

int lvlprest_empty_slot(char * buf, int def)
{
   char * start, * end;
   int f;

   if (lvlprest_find_col(buf, "Def") < 0 || lvlprest_find_col(buf, "File1") < 0)
      return LVLPREST_TABLE_UNUSABLE;
   for (f = 1; f <= 6; f++)
      if (find_cell(buf, def, f, &start, &end) && cell_is_empty(start, end))
         return f;
   return 0;
}

int lvlprest_get_cell(char * buf, int def, int slot, char * out, int cap)
{
   char * start, * end;
   int n;

   if (!find_cell(buf, def, slot, &start, &end))
      return 0;
   n = (int)(end - start);
   if (n >= cap)
      return 0;
   memcpy(out, start, n);
   out[n] = '\0';
   return 1;
}

char * lvlprest_with_cell(char * buf, int def, int slot, const char * value,
                          long * out_len)
{
   char * start, * end, * out;
   long before, after, val = (long) strlen(value);

   if (!find_cell(buf, def, slot, &start, &end))
      return NULL;
   before = (long)(start - buf);
   after = (long) strlen(end);
   out = (char *) malloc(before + val + after + 1);
   if (out == NULL)
      return NULL;
   memcpy(out, buf, before);
   memcpy(out + before, value, val);
   memcpy(out + before + val, end, after + 1);
   *out_len = before + val + after;
   return out;
}

void lvlprest_each_file(char * table, LVLPREST_ROW_CB cb, void * ctx)
{
   int def_col = lvlprest_find_col(table, "Def");
   char * line = table;
   int is_header = 1;

   if (def_col < 0)
      return;

   while (*line != '\0')
   {
      char * next = line;
      while (*next != '\0' && *next != '\n' && *next != '\r')
         next++;

      if (!is_header)
      {
         char * p = line;
         int col = 0;

         while (col < def_col && p < next)
         {
            if (*p == '\t') col++;
            p++;
         }
         if (col == def_col && atoi(p) > 0)
         {
            int def = atoi(p), f;

            for (f = 1; f <= 6; f++)
            {
               char path[256];

               if (lvlprest_get_cell(table, def, f, path, sizeof(path)) &&
                   path[0] != '\0' && strcmp(path, "0") != 0)
                  cb(def, path, ctx);
            }
         }
      }

      while (*next == '\n' || *next == '\r') next++;
      line = next;
      is_header = 0;
   }
}

void lvlprest_each_cleared(char * game, char * project, LVLPREST_ROW_CB cb,
                           void * ctx)
{
   int def_col = lvlprest_find_col(game, "Def");
   int proj_def_col = lvlprest_find_col(project, "Def");
   char * line = game;
   int is_header = 1;

   if (def_col < 0 || proj_def_col < 0)
      return;

   while (*line != '\0')
   {
      char * next = line;
      while (*next != '\0' && *next != '\n' && *next != '\r')
         next++;

      if (!is_header)
      {
         char * p = line;
         char * row_end;
         int col = 0;

         while (col < def_col && p < next)
         {
            if (*p == '\t') col++;
            p++;
         }
         if (col == def_col)
         {
            int def = atoi(p);
            int f;

            if (def > 0 && lvlprest_find_row(project, proj_def_col, def, &row_end) != NULL)
            {
               for (f = 1; f <= 6; f++)
               {
                  char path[256];

                  if (!lvlprest_get_cell(game, def, f, path, sizeof(path)) ||
                      path[0] == '\0' || strcmp(path, "0") == 0)
                     continue;
                  if (lvlprest_find_path_slot(project, def, path) == 0)
                     cb(def, path, ctx);
               }
            }
         }
      }

      while (*next == '\n' || *next == '\r') next++;
      line = next;
      is_header = 0;
   }
}

#define ROW_CELL_CAP 256

int lvlprest_files_value(char * buf, int def)
{
   char * start, * end;
   char num[16];
   int n;

   if (!find_named_cell(buf, def, "Files", &start, &end))
      return -1;
   n = (int)(end - start);
   if (n >= (int) sizeof(num))
      return -1;
   memcpy(num, start, n);
   num[n] = '\0';
   return atoi(num);
}

int lvlprest_row_is_simple(char * buf, int def)
{
   int files = lvlprest_files_value(buf, def);
   char * start, * end;
   int f;

   if (files < 1 || files > 6)
      return 0;
   for (f = 1; f <= 6; f++)
   {
      if (!find_cell(buf, def, f, &start, &end))
         return 0;
      if (cell_is_empty(start, end) != (f > files))
         return 0;
   }
   return 1;
}

static int read_row_cells(char * buf, int def, char cells[6][ROW_CELL_CAP])
{
   int f;

   for (f = 1; f <= 6; f++)
      if (!lvlprest_get_cell(buf, def, f, cells[f - 1], ROW_CELL_CAP))
         return 0;
   return 1;
}

/* A new table with this row's Files and six File columns replaced. */
static char * table_with_row(char * buf, int def, char cells[6][ROW_CELL_CAP], int files,
                             long * out_len)
{
   char * start, * end, * text, * next;
   char num[16];
   long before, after, val;
   int f;

   if (!find_named_cell(buf, def, "Files", &start, &end))
      return NULL;
   sprintf(num, "%d", files);
   val = (long) strlen(num);
   before = (long)(start - buf);
   after = (long) strlen(end);
   text = (char *) malloc(before + val + after + 1);
   if (text == NULL)
      return NULL;
   memcpy(text, buf, before);
   memcpy(text + before, num, val);
   memcpy(text + before + val, end, after + 1);
   *out_len = before + val + after;

   for (f = 1; f <= 6; f++)
   {
      next = lvlprest_with_cell(text, def, f, cells[f - 1], out_len);
      free(text);
      if (next == NULL)
         return NULL;
      text = next;
   }
   return text;
}

char * lvlprest_with_map_inserted(char * buf, int def, int slot, const char * value,
                                  long * out_len)
{
   char cells[6][ROW_CELL_CAP];
   int files, f;

   if (!lvlprest_row_is_simple(buf, def) || strlen(value) >= ROW_CELL_CAP)
      return NULL;
   /* A blank or "0" cell below Files crashes the game's pick, and a tab or line
      break would change the row's layout. */
   if (value[0] == '\0' || (value[0] == '0' && value[1] == '\0')
       || strpbrk(value, "\t\r\n") != NULL)
      return NULL;
   files = lvlprest_files_value(buf, def);
   if (files >= 6 || slot < 1 || !read_row_cells(buf, def, cells))
      return NULL;
   if (slot > files + 1)
      slot = files + 1;
   for (f = files; f >= slot; f--)
      strcpy(cells[f], cells[f - 1]);
   strcpy(cells[slot - 1], value);
   return table_with_row(buf, def, cells, files + 1, out_len);
}

char * lvlprest_with_map_removed(char * buf, int def, int slot, long * out_len)
{
   char cells[6][ROW_CELL_CAP];
   int files, f;

   if (!lvlprest_row_is_simple(buf, def))
      return NULL;
   files = lvlprest_files_value(buf, def);
   if (files < 2 || slot < 1 || slot > files || !read_row_cells(buf, def, cells))
      return NULL;
   for (f = slot; f < files; f++)
      strcpy(cells[f - 1], cells[f]);
   strcpy(cells[files - 1], "0");
   return table_with_row(buf, def, cells, files - 1, out_len);
}
