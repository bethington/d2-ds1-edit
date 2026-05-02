#include <ctype.h>
#include <string.h>

#include "core/export_presets.h"

#ifdef _WIN32
#define preset_stricmp _stricmp
#else
#include <strings.h>
#define preset_stricmp strcasecmp
#endif

static EXPORT_PRESET_S s_presets[EXPORT_PRESETS_MAX];
static int s_count = 0;

static int copy_with_trim(char *dst, int cap, const char *start, const char *end)
{
   int n;

   while (start < end && (*start == ' ' || *start == '\t'))
      start++;
   while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
      end--;

   n = (int) (end - start);
   if (n >= cap)
      n = cap - 1;
   memcpy(dst, start, n);
   dst[n] = 0;
   return n > 0;
}

static int valid_type(const char *t)
{
   return preset_stricmp(t, "all") == 0
       || preset_stricmp(t, "dt1") == 0
       || preset_stricmp(t, "dc6") == 0
       || preset_stricmp(t, "dcc") == 0;
}

static void lowercase_inplace(char *s)
{
   while (*s != 0)
   {
      *s = (char) tolower((unsigned char) *s);
      s++;
   }
}

int export_preset_parse(const char *name, const char *value,
                        EXPORT_PRESET_S *out)
{
   const char *bar;
   EXPORT_PRESET_S tmp;

   if (name == NULL || value == NULL || out == NULL)
      return 0;
   if (name[0] == 0)
      return 0;

   bar = strchr(value, '|');
   if (bar == NULL)
      return 0;

   memset(&tmp, 0, sizeof(tmp));

   if (!copy_with_trim(tmp.name, sizeof(tmp.name),
                       name, name + strlen(name)))
      return 0;

   if (!copy_with_trim(tmp.type, sizeof(tmp.type), value, bar))
      return 0;

   if (!valid_type(tmp.type))
      return 0;

   if (!copy_with_trim(tmp.pattern, sizeof(tmp.pattern),
                       bar + 1, bar + 1 + strlen(bar + 1)))
      return 0;

   lowercase_inplace(tmp.type);

   *out = tmp;
   return 1;
}

int export_presets_add(const EXPORT_PRESET_S *preset)
{
   if (preset == NULL)
      return 0;
   if (preset->name[0] == 0
       || preset->type[0] == 0
       || preset->pattern[0] == 0)
      return 0;
   if (s_count >= EXPORT_PRESETS_MAX)
      return 0;

   s_presets[s_count++] = *preset;
   return 1;
}

int export_presets_count(void)
{
   return s_count;
}

const EXPORT_PRESET_S *export_presets_at(int idx)
{
   if (idx < 0 || idx >= s_count)
      return NULL;
   return &s_presets[idx];
}

void export_presets_reset(void)
{
   s_count = 0;
}
