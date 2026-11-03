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

int lvlprest_find_file_slot(char * buf, int def, const char * ds1_path)
{
   const char * name = lvlprest_base_name(ds1_path);
   int name_len = (int)strlen(name);
   int def_col = lvlprest_find_col(buf, "Def");
   int file_cols[6];
   char * row, * row_end;
   char col_name[16];
   int f;

   if (def_col < 0)
      return LVLPREST_TABLE_UNUSABLE;
   for (f = 0; f < 6; f++)
   {
      sprintf(col_name, "File%d", f + 1);
      file_cols[f] = lvlprest_find_col(buf, col_name);
      if (file_cols[f] < 0)
         return LVLPREST_TABLE_UNUSABLE;
   }

   row = lvlprest_find_row(buf, def_col, def, &row_end);
   if (row == NULL)
      return 0;

   for (f = 0; f < 6; f++)
   {
      const char * cell = row;
      const char * cell_end;
      const char * base;
      int col = 0;

      while (col < file_cols[f] && *cell != '\0' && *cell != '\n' && *cell != '\r')
      {
         if (*cell == '\t') col++;
         cell++;
      }
      if (col != file_cols[f])
         continue;

      cell_end = cell;
      while (*cell_end != '\0' && *cell_end != '\t' && *cell_end != '\n' && *cell_end != '\r')
         cell_end++;

      base = cell_end;
      while (base > cell && base[-1] != '/' && base[-1] != '\\')
         base--;

      if ((int)(cell_end - base) == name_len && strncasecmp(base, name, name_len) == 0)
         return f + 1;
   }
   return 0;
}
