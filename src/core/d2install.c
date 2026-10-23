#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "platform.h"
#include "structs.h"
#include "core/d2install.h"

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
#else
   #include <dirent.h>
   #include <strings.h>
#endif

// Vanilla slot names, in chain order (patch_d2 first = highest precedence);
// slot index = MPQ_SLOT_PATCH_D2 + position.
static const char * const VANILLA_NAMES[MAX_VANILLA_MPQ] = {
   "patch_d2.mpq",
   "d2exp.mpq",
   "d2data.mpq",
   "d2char.mpq"
};

// Project Diablo 2 archives, in chain order; slot index = position. This
// order is the editor's assumption: PD2 ships no readable load list (the
// .cfg/.json/ddraw.ini carry none). It is the order the names sit in
// ProjectDiablo.dll's string table (data, maps, assets, monchars), with the
// folder's own patch_d2.mpq last, as the base game ranks its patch below
// the mod's data. D2MultiRes / SGD2FreeRes / d2gl / ver-IX86 are display and
// patch helpers, not data the editor reads, and are never opened.
static const char * const PD2_NAMES[MAX_PD2_MPQ] = {
   "pd2data.mpq",
   "pd2maps.mpq",
   "pd2assets.mpq",
   "pd2monchars.mpq",
   "patch_d2.mpq"
};

#define PD2_SUBDIR "ProjectD2"

static int           g_pd2_in_use = 0;

// Resolve <dir>/<want> to a real file, ignoring filename case, and write the
// path that actually exists into `out`. Returns 1 on success.
//
// A stock Blizzard install spells the patch archive "Patch_D2.mpq" while the
// other four are lowercase, and repacks vary. fopen() papers over that on
// Windows; on a case-sensitive filesystem it does not, and silently missing
// patch_d2.mpq costs the editor every table the patch updates -- Levels.txt
// included, which then looks like a version mismatch rather than a missing
// archive. So: try the exact name, then scan the directory case-insensitively.
static int resolve_mpq_path(const char *dir, const char *want,
                            char *out, size_t out_cap)
{
   FILE *fp;

   snprintf(out, out_cap, "%s%s%s", dir, DS1_SEP_STR, want);
   ds1_path_normalize(out);
   fp = fopen(out, "rb");
   if (fp != NULL) { fclose(fp); return 1; }

#ifndef WIN32
   {
      DIR *d = opendir(dir);
      struct dirent *ent;

      if (d != NULL)
      {
         while ((ent = readdir(d)) != NULL)
         {
            if (strcasecmp(ent->d_name, want) == 0)
            {
               snprintf(out, out_cap, "%s%s%s", dir, DS1_SEP_STR, ent->d_name);
               ds1_path_normalize(out);
               closedir(d);
               return 1;
            }
         }
         closedir(d);
      }
   }
#endif

   return 0;
}


// ---- path helpers -------------------------------------------------------

// Copy `src` into `dst` in this host's separator style with trailing
// separators removed ("C:\Diablo2\" -> "C:\Diablo2"; a bare drive root such
// as "C:\" is left alone). Returns 0 for an empty or overlong path.
static int copy_clean_dir(char *dst, size_t cap, const char *src)
{
   size_t n;

   if (src == NULL || src[0] == 0) return 0;
   n = strlen(src);
   if (n >= cap) return 0;
   memcpy(dst, src, n + 1);
   ds1_path_normalize(dst);

   n = strlen(dst);
   while (n > 1 && DS1_IS_SEP(dst[n - 1]) && !(n == 3 && dst[1] == ':'))
      dst[--n] = 0;
   return dst[0] != 0;
}

static int env_find(const D2INSTALL_ENV *env, const char *dir, const char *name,
                    char *out, size_t cap)
{
   if (env == NULL || env->find_file == NULL) return 0;
   return env->find_file(env->ctx, dir, name, out, cap);
}

// Valid install: holds d2data.mpq or d2exp.mpq.
static int env_dir_has_d2_mpq(const D2INSTALL_ENV *env, const char *dir)
{
   char path[D2INSTALL_PATH_MAX];
   return env_find(env, dir, "d2data.mpq", path, sizeof(path))
       || env_find(env, dir, "d2exp.mpq", path, sizeof(path));
}

// Valid Project Diablo 2 folder: holds pd2data.mpq.
static int env_dir_has_pd2(const D2INSTALL_ENV *env, const char *dir)
{
   char path[D2INSTALL_PATH_MAX];
   return env_find(env, dir, PD2_NAMES[0], path, sizeof(path));
}

static const D2REG_ROOT UNINSTALL_ROOTS[3] = {
   D2REG_HKCU, D2REG_HKLM_NATIVE, D2REG_HKLM_WOW64
};

const char *d2install_src_name(D2INSTALL_SRC src)
{
   switch (src)
   {
      case D2SRC_INI:       return "ini";
      case D2SRC_REG_HKCU:  return "HKCU registry";
      case D2SRC_REG_HKLM:  return "HKLM registry";
      case D2SRC_REG_WOW64: return "HKLM WOW6432Node registry";
      case D2SRC_UNINSTALL: return "Uninstall registry entry";
      case D2SRC_COMMON:    return "common install folder";
      case D2SRC_SIBLING:   return "folder next to Diablo II";
      default:              return "none";
   }
}

// ---- detection (pure: everything comes from the environment) ------------

static const char * const DEFAULT_COMMON_WIN[] = {
   "C:\\Diablo II",
   "C:\\Program Files\\Diablo II",
   "C:\\Program Files (x86)\\Diablo II",
   "C:\\Games\\Diablo II",
   NULL
};

// Common folders: the Windows ones always, plus a few Unix-style ones with
// $HOME expanded once.
static const char * const *default_common_paths(void)
{
   static const char *list[16];
   static int built;
#ifndef WIN32
   static char home_a[D2INSTALL_PATH_MAX], home_b[D2INSTALL_PATH_MAX],
               home_c[D2INSTALL_PATH_MAX];
#endif

   if (!built)
   {
      int n = 0, k;

      built = 1;
      for (k = 0; DEFAULT_COMMON_WIN[k] != NULL; k++)
         list[n++] = DEFAULT_COMMON_WIN[k];
#ifndef WIN32
      {
         const char *home = getenv("HOME");
         if (home != NULL && home[0] != 0 && strlen(home) < 400)
         {
            snprintf(home_a, sizeof(home_a), "%s/Diablo II", home);
            snprintf(home_b, sizeof(home_b), "%s/Games/Diablo II", home);
            snprintf(home_c, sizeof(home_c), "%s/diablo2", home);
            list[n++] = home_a;
            list[n++] = home_b;
            list[n++] = home_c;
         }
      }
      list[n++] = "/Applications/Diablo II";
      list[n++] = "/opt/diablo2";
#endif
      list[n] = NULL;
   }
   return list;
}

int d2install_find_d2(const D2INSTALL_ENV *env, const char *ini_dir,
                      char *out, int out_cap, D2INSTALL_SRC *src)
{
   static const struct { D2REG_ROOT root; D2INSTALL_SRC src; } reg_steps[3] = {
      { D2REG_HKCU,        D2SRC_REG_HKCU  },
      { D2REG_HKLM_NATIVE, D2SRC_REG_HKLM  },
      { D2REG_HKLM_WOW64,  D2SRC_REG_WOW64 }
   };
   char raw[D2INSTALL_PATH_MAX];
   const char * const *common;
   int i;

   if (src != NULL) *src = D2SRC_NONE;
   if (out == NULL || out_cap < 2 || env == NULL) return 0;
   out[0] = 0;

   // A configured path is authoritative: a typo stays visible instead of
   // being replaced by whatever else is installed.
   if (ini_dir != NULL && ini_dir[0] != 0)
   {
      if (!copy_clean_dir(out, (size_t) out_cap, ini_dir)) { out[0] = 0; return 0; }
      if (src != NULL) *src = D2SRC_INI;
      return 1;
   }

   if (env->reg_install_path != NULL)
      for (i = 0; i < 3; i++)
      {
         raw[0] = 0;
         if (env->reg_install_path(env->ctx, reg_steps[i].root, raw, (int) sizeof(raw))
             && copy_clean_dir(out, (size_t) out_cap, raw)
             && env_dir_has_d2_mpq(env, out))
         {
            if (src != NULL) *src = reg_steps[i].src;
            return 1;
         }
      }

   if (env->reg_uninstall_location != NULL)
      for (i = 0; i < 3; i++)
      {
         raw[0] = 0;
         if (env->reg_uninstall_location(env->ctx, UNINSTALL_ROOTS[i], "Diablo II",
                                         raw, (int) sizeof(raw))
             && copy_clean_dir(out, (size_t) out_cap, raw)
             && env_dir_has_d2_mpq(env, out))
         {
            if (src != NULL) *src = D2SRC_UNINSTALL;
            return 1;
         }
      }

   common = env->common_paths != NULL ? env->common_paths : default_common_paths();
   for (i = 0; common[i] != NULL; i++)
      if (copy_clean_dir(out, (size_t) out_cap, common[i])
          && env_dir_has_d2_mpq(env, out))
      {
         if (src != NULL) *src = D2SRC_COMMON;
         return 1;
      }

   out[0] = 0;
   return 0;
}

// "<base>\ProjectD2" into out; 1 if it fit.
static int pd2_subdir(char *out, int out_cap, const char *base)
{
   int n = snprintf(out, (size_t) out_cap, "%s%s%s", base, DS1_SEP_STR, PD2_SUBDIR);
   return n > 0 && n < out_cap;
}

int d2install_find_pd2(const D2INSTALL_ENV *env, const char *ini_dir,
                       const char *d2_dir, char *out, int out_cap,
                       D2INSTALL_SRC *src)
{
   char raw[D2INSTALL_PATH_MAX];
   char base[D2INSTALL_PATH_MAX];
   int i;

   if (src != NULL) *src = D2SRC_NONE;
   if (out == NULL || out_cap < 2 || env == NULL) return 0;
   out[0] = 0;

   if (ini_dir != NULL && ini_dir[0] != 0)
   {
      if (copy_clean_dir(out, (size_t) out_cap, ini_dir)
          && env_dir_has_pd2(env, out))
      {
         if (src != NULL) *src = D2SRC_INI;
         return 1;
      }
      out[0] = 0;   // configured but not a PD2 folder: keep looking
   }

   if (env->reg_uninstall_location != NULL)
      for (i = 0; i < 3; i++)
      {
         raw[0] = 0;
         if (env->reg_uninstall_location(env->ctx, UNINSTALL_ROOTS[i],
                                         "Project Diablo 2", raw, (int) sizeof(raw))
             && copy_clean_dir(base, sizeof(base), raw)
             && pd2_subdir(out, out_cap, base)
             && env_dir_has_pd2(env, out))
         {
            if (src != NULL) *src = D2SRC_UNINSTALL;
            return 1;
         }
      }

   if (d2_dir != NULL && d2_dir[0] != 0
       && copy_clean_dir(base, sizeof(base), d2_dir)
       && pd2_subdir(out, out_cap, base)
       && env_dir_has_pd2(env, out))
   {
      if (src != NULL) *src = D2SRC_SIBLING;
      return 1;
   }

   out[0] = 0;
   return 0;
}

static int path_eq(const char *a, const char *b)
{
#ifdef WIN32
   return _stricmp(a, b) == 0;
#else
   return strcmp(a, b) == 0;
#endif
}

// 1 if `pd2` is exactly "<d2_dir>\ProjectD2" (normalised, separators stripped).
static int pd2_belongs_to(const char *d2_dir, const char *pd2)
{
   char base[D2INSTALL_PATH_MAX], want[D2INSTALL_PATH_MAX], have[D2INSTALL_PATH_MAX];
   if (!copy_clean_dir(base, sizeof(base), d2_dir)
       || !pd2_subdir(want, (int) sizeof(want), base)
       || !copy_clean_dir(have, sizeof(have), pd2))
      return 0;
   return path_eq(want, have);
}

// "<dir>" with one trailing path component removed (the folder above ProjectD2).
static void parent_dir(char *dst, size_t cap, const char *dir)
{
   size_t n;
   snprintf(dst, cap, "%s", dir);
   n = strlen(dst);
   while (n > 0 && !DS1_IS_SEP(dst[n - 1])) n--;
   if (n > 1 && !(n == 3 && dst[1] == ':')) n--;
   dst[n] = 0;
}

void d2install_pd2_skipped_message(const D2INSTALL_RESULT *res, char *out, int cap)
{
   char owner[D2INSTALL_PATH_MAX];
   if (out == NULL || cap < 1) return;
   out[0] = 0;
   if (res == NULL || res->pd2_skipped_dir[0] == 0) return;
   parent_dir(owner, sizeof(owner), res->pd2_skipped_dir);
   snprintf(out, (size_t) cap,
            "d2install: Project Diablo 2 found at <%s> but it belongs to <%s>; "
            "not used (set pd2_dir to use it)\n", res->pd2_skipped_dir, owner);
}

void d2install_resolve(const D2INSTALL_ENV *env, const D2INSTALL_CONFIG *cfg,
                       D2INSTALL_RESULT *out)
{
   int i;

   memset(out, 0, sizeof(*out));
   if (env == NULL || cfg == NULL) return;

   d2install_find_d2(env, cfg->d2_install, out->d2_dir, sizeof(out->d2_dir),
                     &out->d2_source);

   // Vanilla chain: patch_d2, d2exp, d2data, d2char from the install.
   if (out->d2_dir[0] != 0)
      for (i = 0; i < MAX_VANILLA_MPQ; i++)
      {
         int slot = MPQ_SLOT_PATCH_D2 + i;
         if (cfg->explicit_mpq[slot] != NULL && cfg->explicit_mpq[slot][0] != 0)
            continue;   // the ini set it; never override
         if (env_find(env, out->d2_dir, VANILLA_NAMES[i],
                      out->mpq[slot], sizeof(out->mpq[slot])))
            out->filled++;
         else
            out->mpq[slot][0] = 0;
      }

   // Project Diablo 2 overlay, searched before everything above.
   if (cfg->use_pd2 != D2_USE_PD2_NO)
   {
      int found = d2install_find_pd2(env, cfg->pd2_dir, out->d2_dir, out->pd2_dir,
                                     sizeof(out->pd2_dir), &out->pd2_source);

      // Only an explicit pd2_dir may point outside the selected install.
      if (found && out->pd2_source != D2SRC_INI && out->d2_dir[0] != 0
          && !pd2_belongs_to(out->d2_dir, out->pd2_dir))
      {
         char base[D2INSTALL_PATH_MAX];
         snprintf(out->pd2_skipped_dir, sizeof(out->pd2_skipped_dir), "%s", out->pd2_dir);
         out->pd2_dir[0] = 0;
         out->pd2_source = D2SRC_NONE;
         found = 0;
         // The Uninstall entry was someone else's; the sibling may still be ours.
         if (copy_clean_dir(base, sizeof(base), out->d2_dir)
             && pd2_subdir(out->pd2_dir, (int) sizeof(out->pd2_dir), base)
             && env_dir_has_pd2(env, out->pd2_dir))
         {
            out->pd2_source = D2SRC_SIBLING;
            found = 1;
            out->pd2_skipped_dir[0] = 0;
         }
         else
            out->pd2_dir[0] = 0;
      }

      if (found)
      {
         out->pd2_active = 1;
         for (i = 0; i < MAX_PD2_MPQ; i++)
         {
            int slot = MPQ_SLOT_PD2_FIRST + i;
            if (env_find(env, out->pd2_dir, PD2_NAMES[i],
                         out->mpq[slot], sizeof(out->mpq[slot])))
               out->filled++;
            else
               out->mpq[slot][0] = 0;
         }
      }
      else if (cfg->use_pd2 == D2_USE_PD2_YES)
         out->pd2_required_missing = 1;
   }
}

// ---- real environment ---------------------------------------------------

static int real_find_file(void *ctx, const char *dir, const char *name,
                          char *out, size_t cap)
{
   (void) ctx;
   return resolve_mpq_path(dir, name, out, cap);
}

#ifdef WIN32
static HKEY reg_root_key(D2REG_ROOT root)
{
   return root == D2REG_HKCU ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
}

static REGSAM reg_view(D2REG_ROOT root)
{
   // The editor is a 32-bit build, so "native" must be asked for explicitly;
   // on a 32-bit Windows both flags are ignored.
   if (root == D2REG_HKLM_NATIVE) return KEY_WOW64_64KEY;
   if (root == D2REG_HKLM_WOW64)  return KEY_WOW64_32KEY;
   return 0;
}

// Read a REG_SZ value; force termination (the API does not guarantee it).
static int reg_read_sz(HKEY key, const char *name, char *out, int cap)
{
   DWORD len = (DWORD) (cap - 1), type = 0;
   LONG rc = RegQueryValueExA(key, name, NULL, &type, (LPBYTE) out, &len);

   if (rc != ERROR_SUCCESS || type != REG_SZ) return 0;
   out[cap - 1] = 0;
   if (len < (DWORD) cap) out[len] = 0;
   return out[0] != 0;
}

static int real_reg_install_path(void *ctx, D2REG_ROOT root, char *out, int cap)
{
   HKEY key;
   int ok;
   (void) ctx;

   if (RegOpenKeyExA(reg_root_key(root),
                     "SOFTWARE\\Blizzard Entertainment\\Diablo II",
                     0, KEY_READ | reg_view(root), &key) != ERROR_SUCCESS)
      return 0;
   ok = reg_read_sz(key, "InstallPath", out, cap);
   RegCloseKey(key);
   return ok;
}

static int real_reg_uninstall_location(void *ctx, D2REG_ROOT root,
                                       const char *display_name,
                                       char *out, int cap)
{
   HKEY uninst;
   DWORD idx;
   int found = 0;
   (void) ctx;

   if (RegOpenKeyExA(reg_root_key(root),
                     "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                     0, KEY_READ | reg_view(root), &uninst) != ERROR_SUCCESS)
      return 0;

   for (idx = 0; !found; idx++)
   {
      char sub[256], name[256];
      DWORD sub_len = (DWORD) sizeof(sub);
      HKEY app;
      LONG rc = RegEnumKeyExA(uninst, idx, sub, &sub_len, NULL, NULL, NULL, NULL);

      if (rc == ERROR_NO_MORE_ITEMS) break;
      if (rc != ERROR_SUCCESS) continue;
      if (RegOpenKeyExA(uninst, sub, 0, KEY_READ | reg_view(root), &app) != ERROR_SUCCESS)
         continue;
      if (reg_read_sz(app, "DisplayName", name, (int) sizeof(name))
          && _stricmp(name, display_name) == 0
          && reg_read_sz(app, "InstallLocation", out, cap))
         found = 1;
      RegCloseKey(app);
   }
   RegCloseKey(uninst);
   return found;
}
#endif // WIN32

const D2INSTALL_ENV *d2install_real_env(void)
{
   static D2INSTALL_ENV env;
   static int ready;

   if (!ready)
   {
      memset(&env, 0, sizeof(env));
      env.find_file = real_find_file;
#ifdef WIN32
      env.reg_install_path = real_reg_install_path;
      env.reg_uninstall_location = real_reg_uninstall_location;
#endif
      ready = 1;
   }
   return &env;
}

static const D2INSTALL_ENV *g_env_override;

void d2install_set_env(const D2INSTALL_ENV *env)
{
   g_env_override = env;
}

static const D2INSTALL_ENV *active_env(void)
{
   return g_env_override != NULL ? g_env_override : d2install_real_env();
}

int d2install_is_install_dir(const char *dir)
{
   char clean[D2INSTALL_PATH_MAX];

   if (!copy_clean_dir(clean, sizeof(clean), dir)) return 0;
   return env_dir_has_d2_mpq(d2install_real_env(), clean);
}

int d2install_detect(char *out_path, int out_cap)
{
   return d2install_find_d2(active_env(), NULL, out_path, out_cap, NULL);
}

const char *d2install_data_set_name(void)
{
   return g_pd2_in_use ? "Project Diablo 2" : "vanilla";
}

static const char *use_pd2_name(int v)
{
   return v == D2_USE_PD2_YES ? "yes" : v == D2_USE_PD2_NO ? "no" : "auto";
}

// Print to both streams, like the rest of the startup diagnostics.
static void say(const char *fmt, ...)
{
   char line[1200];
   va_list ap;

   va_start(ap, fmt);
   vsnprintf(line, sizeof(line), fmt, ap);
   va_end(ap);
   fputs(line, stdout);
   fputs(line, stderr);
}

int d2install_resolve_mpqs(void)
{
   D2INSTALL_CONFIG cfg;
   D2INSTALL_RESULT *res;
   int i, filled = 0;
   int d2_was_configured;
   size_t len;
   char *buf;

   res = (D2INSTALL_RESULT *) malloc(sizeof(*res));
   if (res == NULL) return 0;

   memset(&cfg, 0, sizeof(cfg));
   cfg.d2_install = glb_config.d2_install;
   cfg.pd2_dir = glb_config.pd2_dir;
   cfg.use_pd2 = glb_config.use_pd2;
   for (i = 0; i < MAX_MPQ_FILE; i++)
      cfg.explicit_mpq[i] = glb_config.mpq_file[i];

   d2_was_configured = glb_config.d2_install != NULL && glb_config.d2_install[0] != 0;
   d2install_resolve(active_env(), &cfg, res);
   g_pd2_in_use = res->pd2_active;

   if (d2_was_configured)
      say("d2install: using configured path <%s>\n", glb_config.d2_install);
   else if (res->d2_dir[0] != 0)
   {
      // Cache the detected path so downstream code (project save, UI, logs)
      // can see where the editor resolved from.
      len = strlen(res->d2_dir);
      buf = (char *) malloc(len + 1);
      if (buf != NULL)
      {
         memcpy(buf, res->d2_dir, len + 1);
         glb_config.d2_install = buf;
      }
      say("d2install: auto-detected <%s> (%s)\n", res->d2_dir,
          d2install_src_name(res->d2_source));
   }
   else
      say("d2install: no Diablo II install found (set d2_install in the ini)\n");

   for (i = 0; i < MAX_MPQ_FILE; i++)
   {
      if (res->mpq[i][0] == 0) continue;
      if (glb_config.mpq_file[i] != NULL && glb_config.mpq_file[i][0] != 0)
         continue; // explicit INI entry wins

      len = strlen(res->mpq[i]);
      buf = (char *) malloc(len + 1);
      if (buf == NULL) continue;
      memcpy(buf, res->mpq[i], len + 1);

      glb_config.mpq_file[i] = buf;
      filled++;
      fprintf(stdout, "d2install: slot %d -> %s\n", i, buf);
   }

   {
      char skipped[1400];
      d2install_pd2_skipped_message(res, skipped, sizeof(skipped));
      if (skipped[0] != 0) say("%s", skipped);
   }

   if (res->pd2_active)
      say("d2install: Project Diablo 2 data from <%s> (use_pd2=%s, found via %s)\n",
          res->pd2_dir, use_pd2_name(cfg.use_pd2), d2install_src_name(res->pd2_source));
   else if (res->pd2_required_missing)
      say("d2install: ERROR use_pd2=yes but no Project Diablo 2 folder (one holding "
          "pd2data.mpq) was found; using vanilla Diablo II data. Set pd2_dir in the ini.\n");
   else if (cfg.use_pd2 == D2_USE_PD2_NO)
      say("d2install: vanilla Diablo II data from <%s> (use_pd2=no)\n", res->d2_dir);
   else
      say("d2install: vanilla Diablo II data from <%s> (use_pd2=%s, no Project Diablo 2 found)\n",
          res->d2_dir, use_pd2_name(cfg.use_pd2));

   free(res);
   return filled;
}
