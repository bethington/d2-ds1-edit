#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

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
   memset(&glb_project, 0, sizeof(glb_project));
}

void project_apply_to_config(void)
{
   size_t len;
   char *buf;

   if (!glb_project.is_open) return;

   // Point the overlay at the project folder. This replaces mod_dir[0];
   // previous value (if any) leaks -- acceptable given the lifecycle and
   // the existing INI loader does the same thing.
   len = strlen(glb_project.path);
   buf = (char *) malloc(len + 1);
   if (buf != NULL)
   {
      memcpy(buf, glb_project.path, len + 1);
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
