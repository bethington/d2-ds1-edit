#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "core/compose_naming.h"

#ifdef _WIN32
#define compose_stricmp _stricmp
#else
#include <strings.h>
#define compose_stricmp strcasecmp
#endif

typedef struct CODE_NAME_PAIR_S
{
   const char *code;
   const char *name;
} CODE_NAME_PAIR_S;

static const CODE_NAME_PAIR_S s_class_names[] = {
   { "AM", "Amazon"      },
   { "AI", "Assassin"    },
   { "BA", "Barbarian"   },
   { "DZ", "Druid"       },
   { "NE", "Necromancer" },
   { "PA", "Paladin"     },
   { "SO", "Sorceress"   },
   { NULL, NULL }
};

const char *compose_naming_class_name(const char *code)
{
   int i;

   if (code == NULL)
      return NULL;

   for (i = 0; s_class_names[i].code != NULL; i++)
   {
      if (compose_stricmp(code, s_class_names[i].code) == 0)
         return s_class_names[i].name;
   }
   return NULL;
}

static int is_filename_char(char c)
{
   if (c >= 'A' && c <= 'Z') return 1;
   if (c >= 'a' && c <= 'z') return 1;
   if (c >= '0' && c <= '9') return 1;
   if (c == '_') return 1;
   return 0;
}

int compose_naming_sanitize(const char *src, char *out_buf, int out_cap)
{
   int n;
   int prev_was_underscore = 0;
   int start_idx = 0;
   int end_idx;

   if (out_buf == NULL || out_cap <= 1)
      return 0;
   if (src == NULL)
   {
      out_buf[0] = '_';
      out_buf[1] = 0;
      return 1;
   }

   n = 0;
   while (*src != 0 && n < out_cap - 1)
   {
      char c = *src;
      char to_emit;

      if (is_filename_char(c))
      {
         to_emit = c;
         prev_was_underscore = 0;
      }
      else
      {
         /* Collapse runs of non-filename chars into a single
          * underscore. */
         if (prev_was_underscore)
         {
            src++;
            continue;
         }
         to_emit = '_';
         prev_was_underscore = 1;
      }

      out_buf[n++] = to_emit;
      src++;
   }
   out_buf[n] = 0;

   /* Strip leading underscores. */
   while (start_idx < n && out_buf[start_idx] == '_')
      start_idx++;

   /* Strip trailing underscores. */
   end_idx = n;
   while (end_idx > start_idx && out_buf[end_idx - 1] == '_')
      end_idx--;

   if (start_idx > 0 && start_idx < end_idx)
      memmove(out_buf, out_buf + start_idx, (size_t) (end_idx - start_idx));
   if (start_idx >= end_idx)
   {
      out_buf[0] = '_';
      out_buf[1] = 0;
      return 1;
   }
   out_buf[end_idx - start_idx] = 0;

   return 1;
}
