#include <ctype.h>
#include <string.h>

#include "core/compose_presets.h"

static COMPOSE_PRESET_S s_mode_presets[COMPOSE_PRESETS_MAX];
static int s_mode_count = 0;

static COMPOSE_PRESET_S s_weapon_presets[COMPOSE_PRESETS_MAX];
static int s_weapon_count = 0;

static int copy_with_trim_upper(char *dst, int cap,
                                const char *start, const char *end)
{
   int n;
   while (start < end && (*start == ' ' || *start == '\t'))
      start++;
   while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
      end--;
   n = (int) (end - start);
   if (n >= cap)
      n = cap - 1;
   memcpy(dst, start, (size_t) n);
   dst[n] = 0;
   /* uppercase */
   {
      int i;
      for (i = 0; dst[i] != 0; i++)
         dst[i] = (char) toupper((unsigned char) dst[i]);
   }
   return n > 0;
}

static int copy_with_trim(char *dst, int cap,
                          const char *start, const char *end)
{
   int n;
   while (start < end && (*start == ' ' || *start == '\t'))
      start++;
   while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
      end--;
   n = (int) (end - start);
   if (n >= cap)
      n = cap - 1;
   memcpy(dst, start, (size_t) n);
   dst[n] = 0;
   return n > 0;
}

int compose_preset_parse(const char *name, const char *value,
                         COMPOSE_PRESET_S *out)
{
   COMPOSE_PRESET_S tmp;
   const char *p;

   if (name == NULL || value == NULL || out == NULL)
      return 0;
   if (name[0] == 0)
      return 0;

   memset(&tmp, 0, sizeof(tmp));

   if (!copy_with_trim(tmp.name, sizeof(tmp.name),
                       name, name + strlen(name)))
      return 0;

   /* Split value on commas, trim + uppercase each token. */
   p = value;
   while (*p != 0 && tmp.code_count < COMPOSE_PRESET_CODES_MAX)
   {
      const char *start = p;
      while (*p != 0 && *p != ',')
         p++;
      {
         const char *end = p;
         char code_buf[COMPOSE_PRESET_CODE_MAX];
         if (copy_with_trim_upper(code_buf, sizeof(code_buf), start, end))
         {
            strncpy(tmp.codes[tmp.code_count],
                    code_buf,
                    COMPOSE_PRESET_CODE_MAX - 1);
            tmp.code_count++;
         }
      }
      if (*p == ',')
         p++;
   }

   if (tmp.code_count == 0)
      return 0;

   *out = tmp;
   return 1;
}

int compose_mode_presets_add(const COMPOSE_PRESET_S *preset)
{
   if (preset == NULL || preset->name[0] == 0 || preset->code_count == 0)
      return 0;
   if (s_mode_count >= COMPOSE_PRESETS_MAX)
      return 0;
   s_mode_presets[s_mode_count++] = *preset;
   return 1;
}

int compose_mode_presets_count(void) { return s_mode_count; }

const COMPOSE_PRESET_S *compose_mode_presets_at(int idx)
{
   if (idx < 0 || idx >= s_mode_count) return NULL;
   return &s_mode_presets[idx];
}

void compose_mode_presets_reset(void) { s_mode_count = 0; }

int compose_weapon_presets_add(const COMPOSE_PRESET_S *preset)
{
   if (preset == NULL || preset->name[0] == 0 || preset->code_count == 0)
      return 0;
   if (s_weapon_count >= COMPOSE_PRESETS_MAX)
      return 0;
   s_weapon_presets[s_weapon_count++] = *preset;
   return 1;
}

int compose_weapon_presets_count(void) { return s_weapon_count; }

const COMPOSE_PRESET_S *compose_weapon_presets_at(int idx)
{
   if (idx < 0 || idx >= s_weapon_count) return NULL;
   return &s_weapon_presets[idx];
}

void compose_weapon_presets_reset(void) { s_weapon_count = 0; }
