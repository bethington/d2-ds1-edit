#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "structs.h"
#include "core/preferences.h"

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
   #include <direct.h>
   #define SEP "\\"
#else
   #include <sys/stat.h>
   #include <sys/types.h>
   #define SEP "/"
#endif

PREFERENCES_S glb_prefs;

static void make_recent_key(int idx, char *out, int out_cap)
{
   snprintf(out, out_cap, "recent_project_%d", idx);
}

// mkdir on the parent dir of `path`. Idempotent; ignores "already exists."
static void ensure_parent_dir(const char *path)
{
   char buf[PREFS_PATH_MAX];
   char *slash;
   size_t n;

   n = strlen(path);
   if (n == 0 || n >= sizeof(buf)) return;
   memcpy(buf, path, n + 1);

   slash = strrchr(buf, '\\');
   if (slash == NULL) slash = strrchr(buf, '/');
   if (slash == NULL) return;
   *slash = 0;

#ifdef WIN32
   _mkdir(buf);
#else
   mkdir(buf, 0755);
#endif
}

int prefs_resolve_path(char *out, int out_cap)
{
   const char *base;

   if (out == NULL || out_cap < 2) return 0;
   out[0] = 0;

#ifdef WIN32
   base = getenv("APPDATA");
#else
   base = getenv("XDG_CONFIG_HOME");
   if (base == NULL || base[0] == 0)
      base = getenv("HOME");
#endif
   if (base == NULL || base[0] == 0) return 0;

   snprintf(out, out_cap, "%s%sds1edit%spreferences.ini", base, SEP, SEP);
   return 1;
}

int prefs_load_from(const char *path)
{
   ALLEGRO_CONFIG *cfg;
   const char *v;
   int i;
   char key[32];

   memset(&glb_prefs, 0, sizeof(glb_prefs));

   if (path == NULL || path[0] == 0) return 0;

   cfg = al_load_config_file(path);
   if (cfg == NULL) return 0;

   v = al_get_config_value(cfg, "", "last_d2_install");
   if (v != NULL)
   {
      strncpy(glb_prefs.last_d2_install, v,
              sizeof(glb_prefs.last_d2_install) - 1);
      glb_prefs.last_d2_install[sizeof(glb_prefs.last_d2_install) - 1] = 0;
   }

   for (i = 0; i < PREFS_RECENT_MAX; i++)
   {
      make_recent_key(i, key, sizeof(key));
      v = al_get_config_value(cfg, "", key);
      if (v == NULL || v[0] == 0) break;
      strncpy(glb_prefs.recent_projects[glb_prefs.recent_count], v,
              PREFS_PATH_MAX - 1);
      glb_prefs.recent_projects[glb_prefs.recent_count][PREFS_PATH_MAX - 1] = 0;
      glb_prefs.recent_count++;
   }

   al_destroy_config(cfg);
   glb_prefs.loaded = 1;
   return 1;
}

int prefs_save_to(const char *path)
{
   ALLEGRO_CONFIG *cfg;
   int i, rc;
   char key[32];

   if (path == NULL || path[0] == 0) return 0;

   ensure_parent_dir(path);

   cfg = al_create_config();
   if (cfg == NULL) return 0;

   if (glb_prefs.last_d2_install[0] != 0)
      al_set_config_value(cfg, "", "last_d2_install",
                          glb_prefs.last_d2_install);

   for (i = 0; i < glb_prefs.recent_count && i < PREFS_RECENT_MAX; i++)
   {
      make_recent_key(i, key, sizeof(key));
      al_set_config_value(cfg, "", key, glb_prefs.recent_projects[i]);
   }

   rc = al_save_config_file(path, cfg);
   al_destroy_config(cfg);
   return rc ? 1 : 0;
}

int prefs_load(void)
{
   char path[PREFS_PATH_MAX];
   if (!prefs_resolve_path(path, sizeof(path))) return 0;
   return prefs_load_from(path);
}

int prefs_save(void)
{
   char path[PREFS_PATH_MAX];
   if (!prefs_resolve_path(path, sizeof(path))) return 0;
   return prefs_save_to(path);
}

void prefs_record_recent_project(const char *project_path)
{
   int i, found_at = -1;
   int dst;

   if (project_path == NULL || project_path[0] == 0) return;

   for (i = 0; i < glb_prefs.recent_count; i++)
   {
      if (strcmp(glb_prefs.recent_projects[i], project_path) == 0)
      {
         found_at = i;
         break;
      }
   }

   // Shift everything down one slot from the top until we hit the old
   // occurrence (or PREFS_RECENT_MAX-1 if not present and list is full).
   if (found_at < 0)
   {
      if (glb_prefs.recent_count < PREFS_RECENT_MAX)
         glb_prefs.recent_count++;
      found_at = glb_prefs.recent_count - 1;
   }

   for (dst = found_at; dst > 0; dst--)
   {
      memcpy(glb_prefs.recent_projects[dst],
             glb_prefs.recent_projects[dst - 1],
             PREFS_PATH_MAX);
   }

   strncpy(glb_prefs.recent_projects[0], project_path, PREFS_PATH_MAX - 1);
   glb_prefs.recent_projects[0][PREFS_PATH_MAX - 1] = 0;
}
