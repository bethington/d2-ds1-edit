#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define COMPOSE_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define COMPOSE_MKDIR(p) mkdir((p), 0755)
#endif

#include "core/compose_iter.h"
#include "core/compose_naming.h"

/* Two pieces are factored out so the pure helpers below link cleanly
 * into unit tests:
 *   - compose_iter_use_full_folder_names is defined in
 *     compose_iter_config.c (production reads glb_config).
 *   - compose_iter_probe_direction_count is defined in
 *     compose_iter_probe.c (production calls misc_load_mpq_file +
 *     compose_cof_parse). */

/* ------------------------------------------------------------------ */
/* Default mode / weapon / class lists                                */
/* ------------------------------------------------------------------ */

static const char *s_default_modes[] = {
   "NU", "TW", "WL", "RN", "GH", "DT", "DD", "SC",
   "TH", "KK", "BL", "A1", "A2",
   "S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8"
};
#define DEFAULT_MODE_COUNT \
   ((int) (sizeof(s_default_modes) / sizeof(s_default_modes[0])))

static const char *s_default_weapons[] = {
   "HTH", "1HS", "1HT", "2HS", "2HT",
   "BOW", "XBW", "STF", "HT1", "HT2"
};
#define DEFAULT_WEAPON_COUNT \
   ((int) (sizeof(s_default_weapons) / sizeof(s_default_weapons[0])))

static const char *s_player_classes[] = {
   "AI", "AM", "BA", "DZ", "NE", "PA", "SO"
};
#define PLAYER_CLASS_COUNT \
   ((int) (sizeof(s_player_classes) / sizeof(s_player_classes[0])))

int compose_iter_default_mode_count  (void) { return DEFAULT_MODE_COUNT; }
int compose_iter_default_weapon_count(void) { return DEFAULT_WEAPON_COUNT; }
int compose_iter_player_class_count  (void) { return PLAYER_CLASS_COUNT; }

const char *compose_iter_default_mode_at(int idx)
{
   if (idx < 0 || idx >= DEFAULT_MODE_COUNT) return NULL;
   return s_default_modes[idx];
}
const char *compose_iter_default_weapon_at(int idx)
{
   if (idx < 0 || idx >= DEFAULT_WEAPON_COUNT) return NULL;
   return s_default_weapons[idx];
}
const char *compose_iter_player_class_at(int idx)
{
   if (idx < 0 || idx >= PLAYER_CLASS_COUNT) return NULL;
   return s_player_classes[idx];
}

/* ------------------------------------------------------------------ */
/* Per-category metadata                                              */
/* ------------------------------------------------------------------ */

const char *compose_iter_category_base(COMPOSE_CATEGORY_E category)
{
   switch (category)
   {
      case COMPOSE_CATEGORY_PLAYER_CHAR: return "data\\global\\chars";
      case COMPOSE_CATEGORY_MONSTER:     return "data\\global\\monsters";
      case COMPOSE_CATEGORY_NPC:         return "data\\global\\npc";
      case COMPOSE_CATEGORY_OBJECT:      return "data\\global\\objects";
      default: return NULL;
   }
}

const char *compose_iter_category_skin(COMPOSE_CATEGORY_E category)
{
   switch (category)
   {
      case COMPOSE_CATEGORY_PLAYER_CHAR: return "LIT";
      default: return "";
   }
}

const char *compose_iter_category_folder(COMPOSE_CATEGORY_E category)
{
   switch (category)
   {
      case COMPOSE_CATEGORY_PLAYER_CHAR: return "Player_Characters";
      case COMPOSE_CATEGORY_MONSTER:     return "Monsters";
      case COMPOSE_CATEGORY_NPC:         return "NPCs";
      case COMPOSE_CATEGORY_OBJECT:      return "Objects";
      default: return "Composed";
   }
}

/* ------------------------------------------------------------------ */
/* Output path builder                                                */
/* ------------------------------------------------------------------ */

/* Pick the per-token folder name based on the config flag and the
 * available token name. Falls back to the bare code if the full name
 * is missing or sanitization yields an empty string. */
static void choose_token_folder(const char *token,
                                const char *token_full_name,
                                char *out_buf, int out_cap)
{
   int use_full;
   if (out_cap <= 0) return;
   out_buf[0] = 0;

   use_full = compose_iter_use_full_folder_names() ? 1 : 0;

   if (use_full && token_full_name != NULL && token_full_name[0] != 0)
   {
      char tmp[128];
      if (compose_naming_sanitize(token_full_name, tmp, (int) sizeof(tmp))
          && tmp[0] != 0 && !(tmp[0] == '_' && tmp[1] == 0))
      {
         strncpy(out_buf, tmp, (size_t) (out_cap - 1));
         out_buf[out_cap - 1] = 0;
         return;
      }
   }

   if (token != NULL)
   {
      strncpy(out_buf, token, (size_t) (out_cap - 1));
      out_buf[out_cap - 1] = 0;
   }
}

int compose_iter_build_output_dir(char *out_buf, int out_cap,
                                  const char *root,
                                  COMPOSE_CATEGORY_E category,
                                  const char *token,
                                  const char *token_full_name)
{
   char token_folder[128];
   const char *cat_folder;
   int written;

   if (out_buf == NULL || out_cap <= 0) return 0;
   out_buf[0] = 0;
   if (root == NULL || root[0] == 0) return 0;
   if (token == NULL || token[0] == 0) return 0;

   cat_folder = compose_iter_category_folder(category);
   choose_token_folder(token, token_full_name, token_folder,
                       (int) sizeof(token_folder));
   if (token_folder[0] == 0) return 0;

   written = snprintf(out_buf, (size_t) out_cap, "%s\\%s\\%s",
                      root, cat_folder, token_folder);
   if (written < 0 || written >= out_cap)
   {
      out_buf[0] = 0;
      return 0;
   }
   return 1;
}

int compose_iter_build_output_path(char *out_buf, int out_cap,
                                   const char *root,
                                   COMPOSE_CATEGORY_E category,
                                   const char *token,
                                   const char *token_full_name,
                                   const char *mode,
                                   const char *wclass,
                                   int direction)
{
   char dir_buf[1024];
   const char *mode_safe;
   const char *wclass_safe;
   int written;

   if (out_buf == NULL || out_cap <= 0) return 0;
   out_buf[0] = 0;
   if (mode == NULL) return 0;
   if (direction < 0) return 0;

   if (!compose_iter_build_output_dir(dir_buf, (int) sizeof(dir_buf),
                                      root, category, token,
                                      token_full_name))
      return 0;

   mode_safe   = mode;
   wclass_safe = (wclass != NULL && wclass[0] != 0) ? wclass : "";

   if (wclass_safe[0] != 0)
      written = snprintf(out_buf, (size_t) out_cap,
                         "%s\\%s%s%s_dir%d.png",
                         dir_buf, token, mode_safe, wclass_safe,
                         direction);
   else
      written = snprintf(out_buf, (size_t) out_cap,
                         "%s\\%s%s_dir%d.png",
                         dir_buf, token, mode_safe, direction);

   if (written < 0 || written >= out_cap)
   {
      out_buf[0] = 0;
      return 0;
   }
   return 1;
}

/* ------------------------------------------------------------------ */
/* Recursive mkdir                                                    */
/* ------------------------------------------------------------------ */

int compose_iter_ensure_dir(const char *path)
{
   char buf[2048];
   size_t n, i;

   if (path == NULL) return 0;
   n = strlen(path);
   if (n == 0 || n + 1 > sizeof(buf)) return 0;
   memcpy(buf, path, n + 1);

   /* Walk path, mkdir at every separator. */
   for (i = 1; i <= n; i++)
   {
      char c = buf[i];
      int at_sep = (c == '/' || c == '\\' || c == 0);
      if (!at_sep) continue;

      /* Skip drive root "C:". */
      if (i == 2 && buf[1] == ':' && c != 0)
         continue;

      buf[i] = 0;
      if (buf[0] != 0)
      {
         errno = 0;
         if (COMPOSE_MKDIR(buf) != 0 && errno != EEXIST)
         {
            /* Tolerate "already exists with different metadata", but
             * give up if we hit something genuinely broken. */
            if (errno != 0 && errno != EEXIST)
            {
               /* Fall through; deeper levels may still succeed if an
                * intermediate is read-only but already present. */
            }
         }
      }
      buf[i] = c;
   }
   return 1;
}

