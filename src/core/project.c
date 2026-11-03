#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "structs.h"
#include "core/project.h"

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

#ifndef DS1EDIT_VERSION_STR
#define DS1EDIT_VERSION_STR "0.0.0"
#endif

PROJECT_S glb_project;

// The configured read chain, moved down one by the project and restored when it
// closes.
static char *s_configured_mod_dir[MAX_MOD_DIR];
static int   s_mod_dir_replaced;

static int file_exists(const char *path)
{
   FILE *fp = fopen(path, "rb");
   if (fp == NULL) return 0;
   fclose(fp);
   return 1;
}

static int ensure_dir(const char *path)
{
#ifdef WIN32
   int rc = _mkdir(path);
#else
   int rc = mkdir(path, 0755);
#endif
   // _mkdir / mkdir both set errno on failure; EEXIST is a success case.
   return (rc == 0 || errno == EEXIST) ? 1 : 0;
}

static void make_extra_key(int idx, char *out, int out_cap)
{
   snprintf(out, out_cap, "extra_mod_mpq_%d", idx);
}

int project_ini_path(const char *project_dir, char *out, int out_cap)
{
   if (project_dir == NULL || out == NULL || out_cap < 2) return 0;
   snprintf(out, out_cap, "%s%sproject.ini", project_dir, SEP);
   return 1;
}

int project_create(const char *path, const char *name,
                   const char *d2_install)
{
   char ini[PROJECT_PATH_MAX];
   ALLEGRO_CONFIG *cfg;
   int rc;

   if (path == NULL || path[0] == 0) return 0;
   if (name == NULL) name = "";
   if (d2_install == NULL) d2_install = "";

   if (!ensure_dir(path))
   {
      fprintf(stderr, "project_create: can't create dir <%s>\n", path);
      return 0;
   }

   if (!project_ini_path(path, ini, sizeof(ini))) return 0;

   if (file_exists(ini))
   {
      fprintf(stderr, "project_create: project.ini already exists at <%s>\n", ini);
      return 0;
   }

   cfg = al_create_config();
   if (cfg == NULL) return 0;

   al_set_config_value(cfg, "", "name",           name);
   al_set_config_value(cfg, "", "editor_version", DS1EDIT_VERSION_STR);
   al_set_config_value(cfg, "", "d2_install",     d2_install);

   rc = al_save_config_file(ini, cfg);
   al_destroy_config(cfg);

   if (!rc)
   {
      fprintf(stderr, "project_create: failed to write <%s>\n", ini);
      return 0;
   }

   fprintf(stdout, "project_create: created <%s>\n", ini);
   return 1;
}

int project_load(const char *path)
{
   char ini[PROJECT_PATH_MAX];
   ALLEGRO_CONFIG *cfg;
   const char *v;
   char key[32];
   int i;

   if (path == NULL || path[0] == 0) return 0;
   if (!project_ini_path(path, ini, sizeof(ini))) return 0;

   cfg = al_load_config_file(ini);
   if (cfg == NULL)
   {
      fprintf(stderr, "project_load: missing or unreadable <%s>\n", ini);
      return 0;
   }

   memset(&glb_project, 0, sizeof(glb_project));
   strncpy(glb_project.path, path, PROJECT_PATH_MAX - 1);
   glb_project.path[PROJECT_PATH_MAX - 1] = 0;

   v = al_get_config_value(cfg, "", "name");
   if (v != NULL)
   {
      strncpy(glb_project.name, v, PROJECT_NAME_MAX - 1);
      glb_project.name[PROJECT_NAME_MAX - 1] = 0;
   }

   v = al_get_config_value(cfg, "", "d2_install");
   if (v != NULL)
   {
      strncpy(glb_project.d2_install, v, PROJECT_PATH_MAX - 1);
      glb_project.d2_install[PROJECT_PATH_MAX - 1] = 0;
   }

   for (i = 0; i < PROJECT_EXTRA_MPQ_MAX; i++)
   {
      make_extra_key(i, key, sizeof(key));
      v = al_get_config_value(cfg, "", key);
      if (v == NULL || v[0] == 0) break;
      strncpy(glb_project.extra_mod_mpqs[glb_project.extra_count], v,
              PROJECT_PATH_MAX - 1);
      glb_project.extra_mod_mpqs[glb_project.extra_count][PROJECT_PATH_MAX - 1] = 0;
      glb_project.extra_count++;
   }

   al_destroy_config(cfg);
   glb_project.is_open = 1;
   fprintf(stdout, "project_load: opened <%s> (%s)\n",
           path, glb_project.name[0] ? glb_project.name : "<unnamed>");
   return 1;
}

int project_save(void)
{
   char ini[PROJECT_PATH_MAX];
   ALLEGRO_CONFIG *cfg;
   int i, rc;
   char key[32];

   if (!glb_project.is_open) return 0;
   if (!project_ini_path(glb_project.path, ini, sizeof(ini))) return 0;

   cfg = al_create_config();
   if (cfg == NULL) return 0;

   al_set_config_value(cfg, "", "name",           glb_project.name);
   al_set_config_value(cfg, "", "editor_version", DS1EDIT_VERSION_STR);
   al_set_config_value(cfg, "", "d2_install",     glb_project.d2_install);

   for (i = 0; i < glb_project.extra_count && i < PROJECT_EXTRA_MPQ_MAX; i++)
   {
      make_extra_key(i, key, sizeof(key));
      al_set_config_value(cfg, "", key, glb_project.extra_mod_mpqs[i]);
   }

   rc = al_save_config_file(ini, cfg);
   al_destroy_config(cfg);
   return rc ? 1 : 0;
}

void project_close(void)
{
   if (s_mod_dir_replaced)
   {
      int i;

      for (i = 0; i < MAX_MOD_DIR; i++)
         glb_config.mod_dir[i] = s_configured_mod_dir[i];
      s_mod_dir_replaced = 0;
   }
   memset(&glb_project, 0, sizeof(glb_project));
}

void project_apply_to_config(void)
{
   size_t len;
   char *buf;

   if (!glb_project.is_open) return;

   // Put the project folder first in the read chain. The configured overlays
   // stay behind it. Changing project replaces only entry 0; the old project's
   // string leaks, which is acceptable given the lifecycle, and the existing INI
   // loader does the same thing.
   len = strlen(glb_project.path);
   buf = (char *) malloc(len + 1);
   if (buf != NULL)
   {
      memcpy(buf, glb_project.path, len + 1);
      if (!s_mod_dir_replaced)
      {
         int i;

         for (i = 0; i < MAX_MOD_DIR; i++)
            s_configured_mod_dir[i] = glb_config.mod_dir[i];
         if (glb_config.mod_dir[MAX_MOD_DIR - 1] != NULL)
            fprintf(stderr,
               "project_apply_to_config: the read chain is full; <%s> is not "
               "read while the project is open\n",
               glb_config.mod_dir[MAX_MOD_DIR - 1]);
         for (i = MAX_MOD_DIR - 1; i > 0; i--)
            glb_config.mod_dir[i] = glb_config.mod_dir[i - 1];
         s_mod_dir_replaced = 1;
      }
      glb_config.mod_dir[0] = buf;
   }

   // Seed d2_install if the user hasn't explicitly configured one.
   if ((glb_config.d2_install == NULL || glb_config.d2_install[0] == 0) &&
       glb_project.d2_install[0] != 0)
   {
      len = strlen(glb_project.d2_install);
      buf = (char *) malloc(len + 1);
      if (buf != NULL)
      {
         memcpy(buf, glb_project.d2_install, len + 1);
         glb_config.d2_install = buf;
      }
   }
}

// ---------------------------------------------------------------------------
// Copy-on-save helpers
// ---------------------------------------------------------------------------

int project_ensure_parent_dirs(const char *path)
{
   char buf[PROJECT_PATH_MAX * 2];
   size_t n, i;

   if (path == NULL) return 0;
   n = strlen(path);
   if (n == 0 || n >= sizeof(buf)) return 0;
   memcpy(buf, path, n + 1);

   // Walk the path, mkdir'ing every separator we cross (except the drive
   // root like "C:"). Final segment is left alone -- it's the filename.
   for (i = 1; i < n; i++)
   {
      if (buf[i] == '/' || buf[i] == '\\')
      {
         char save = buf[i];
         buf[i] = 0;
         // Skip the drive-letter segment on Windows ("C:").
         if (!(i == 2 && buf[1] == ':'))
         {
            if (!ensure_dir(buf))
            {
               // Log but keep trying deeper levels; some intermediates may
               // already exist with different ACLs on the parent.
               fprintf(stderr,
                  "project_ensure_parent_dirs: mkdir %s failed (errno=%d)\n",
                  buf, errno);
            }
         }
         buf[i] = save;
      }
   }

   // What matters is the result: the file's own directory must now exist
   // as a directory. (EEXIST from mkdir also covers a *file* of that name,
   // so the loop alone used to report success with a file in the way.)
   while (n > 0 && buf[n - 1] != '/' && buf[n - 1] != '\\')
      n--;
   if (n == 0)
      return 1;                 // bare filename: the current directory
   if (n == 1 || (n == 3 && buf[1] == ':'))
      return 1;                 // a root: "/" or "C:\"
   buf[n - 1] = 0;
   {
      struct stat st;
      if (stat(buf, &st) != 0 || !(st.st_mode & S_IFDIR))
         return 0;
   }
   return 1;
}

// Find the tail after the last "/tiles/" or "\tiles\" segment in `path`,
// case-insensitive. Returns NULL if no such segment exists.
const char *find_tiles_suffix(const char *path)
{
   int len, i;
   if (path == NULL) return NULL;
   len = (int) strlen(path);
   // need sep + "tiles" + sep + at-least-one-char = 8
   if (len < 8) return NULL;

   for (i = len - 8; i >= 0; i--)
   {
      char s0 = path[i];
      char s6 = path[i + 6];
      if ((s0 != '/' && s0 != '\\') || (s6 != '/' && s6 != '\\'))
         continue;
      if (tolower((unsigned char) path[i + 1]) == 't' &&
          tolower((unsigned char) path[i + 2]) == 'i' &&
          tolower((unsigned char) path[i + 3]) == 'l' &&
          tolower((unsigned char) path[i + 4]) == 'e' &&
          tolower((unsigned char) path[i + 5]) == 's')
         return path + i + 7;
   }
   return NULL;
}

// Is `path` inside directory `dir`? Case-insensitive, both slash styles
// equivalent, and only at a path-component boundary: "C:\proj2\x" is not
// inside "C:\proj". (A plain prefix test said it was, and copy-on-save then
// saved over the sibling folder's original map in place.)
static int path_starts_with(const char *path, const char *dir)
{
   int i;
   if (path == NULL || dir == NULL || dir[0] == 0) return 0;
   for (i = 0; dir[i] != 0; i++)
   {
      char a = path[i];
      char b = dir[i];
      if (a == 0) return 0;
      if (a == '/')  a = '\\';
      if (b == '/')  b = '\\';
      if (tolower((unsigned char) a) != tolower((unsigned char) b)) return 0;
   }
   if (dir[i - 1] == '/' || dir[i - 1] == '\\')
      return 1;                  // dir given with its trailing separator
   return path[i] == 0 || path[i] == '/' || path[i] == '\\';
}

int project_path_is_inside(const char *path, const char *dir)
{
   char   trimmed[PROJECT_PATH_MAX];
   size_t n;

   if (path == NULL || dir == NULL || dir[0] == 0) return 0;
   if (path_starts_with(path, dir)) return 1;

   // `dir` spelled with a trailing separator still matches `dir` itself.
   n = strlen(dir);
   if (n >= sizeof(trimmed)) return 0;
   memcpy(trimmed, dir, n + 1);
   while (n > 1 && (trimmed[n - 1] == '/' || trimmed[n - 1] == '\\'))
      trimmed[--n] = 0;
   return path_starts_with(path, trimmed);
}

// "/x", "\x" and "C:\x" name a real place on disk. Anything else is a name
// relative to Tiles, which is how the area browser names maps.
int project_path_is_rooted(const char *path)
{
   if (path == NULL) return 0;
   if (path[0] == '/' || path[0] == '\\') return 1;
   return isalpha((unsigned char) path[0]) && path[1] == ':';
}

int project_path_has_parent_ref(const char *path)
{
   const char *p;

   if (path == NULL) return 0;
   for (p = path; *p != 0; p++)
   {
      if (p[0] == '.' && p[1] == '.' &&
          (p == path || p[-1] == '/' || p[-1] == '\\') &&
          (p[2] == 0 || p[2] == '/' || p[2] == '\\'))
         return 1;
   }
   return 0;
}

static int path_in_install(const char *path, const char *install)
{
   if (install == NULL || install[0] == 0) return 0;
   if (project_path_is_inside(path, install)) return 1;
   return project_path_is_rooted(path) && project_path_has_parent_ref(path);
}

int project_path_in_game_install(const char *path)
{
   if (path == NULL) return 0;
   return path_in_install(path, glb_config.d2_install) ||
          path_in_install(path, glb_project.d2_install);
}

int project_redirect_ds1_save_path(const char *src, char *dst, int dst_cap)
{
   const char *suffix;
   int n;

   if (src == NULL || dst == NULL || dst_cap < 2) return 0;

   // Default: no change.
   strncpy(dst, src, dst_cap - 1);
   dst[dst_cap - 1] = 0;

   if (!glb_project.is_open)               return 0;
   if (glb_project.path[0] == 0)           return 0;

   // Already inside the project folder -> save in place, unless a ".." could
   // take it back out.
   if (path_starts_with(src, glb_project.path))
      return project_path_has_parent_ref(src) ? -1 : 0;

   // Only redirect if we can pull a meaningful in-game suffix out.
   suffix = find_tiles_suffix(src);
   if (suffix == NULL && !project_path_is_rooted(src))
      suffix = src;
   if (suffix == NULL || suffix[0] == 0)   return 0;

   // A ".." in what gets appended would climb back out of the project.
   if (project_path_has_parent_ref(suffix)) return -1;

   {
      size_t plen = strlen(glb_project.path);
      int    has_sep = (glb_project.path[plen - 1] == '/' ||
                        glb_project.path[plen - 1] == '\\');

      n = snprintf(dst, dst_cap, "%s%sGlobal%sTiles%s%s",
                   glb_project.path, has_sep ? "" : SEP, SEP, SEP, suffix);
   }
   if (n < 0 || n >= dst_cap)
   {
      // The copy does not fit. Answering "not redirected" here sent the save
      // back to the source map it was meant to protect; report it instead.
      strncpy(dst, src, dst_cap - 1);
      dst[dst_cap - 1] = 0;
      return -1;
   }
#ifndef WIN32
   // A table can spell a relative name with backslashes.
   {
      char *c;
      for (c = dst + strlen(glb_project.path); *c != 0; c++)
         if (*c == '\\') *c = '/';
   }
#endif
   return 1;
}

int project_save_would_replace(const char *src)
{
   char target[sizeof(glb_ds1[0].name)];
   struct stat st;

   if (src == NULL) return 0;
   if (project_redirect_ds1_save_path(src, target, (int) sizeof(target)) != 1)
      return 0;
   return stat(target, &st) == 0;
}
