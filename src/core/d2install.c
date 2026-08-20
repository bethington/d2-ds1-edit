#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "platform.h"
#include "structs.h"
#include "core/d2install.h"

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
#endif

// Slot layout mirrors the precedence assigned in config.c:
//   slot 0 = patch_d2  (highest precedence, searched first)
//   slot 1 = d2exp
//   slot 2 = d2data
//   slot 3 = d2char
static const char * const MPQ_SLOT_NAMES[MAX_MPQ_FILE] = {
   "patch_d2.mpq",
   "d2exp.mpq",
   "d2data.mpq",
   "d2char.mpq"
};

// Returns 1 if `dir` looks like a D2 install (contains d2data.mpq or d2exp.mpq).
// Name matching is case-sensitive on case-sensitive filesystems; on Windows it
// is case-insensitive automatically, which is what we need.
static int dir_has_d2_mpq(const char *dir)
{
   static const char * const probes[] = { "d2data.mpq", "d2exp.mpq" };
   char path[512];
   FILE *fp;
   int i;

   for (i = 0; i < (int)(sizeof(probes) / sizeof(probes[0])); i++)
   {
      snprintf(path, sizeof(path), "%s%s%s", dir, DS1_SEP_STR, probes[i]);
      fp = fopen(path, "rb");
      if (fp != NULL) { fclose(fp); return 1; }
   }
   return 0;
}

#ifdef WIN32
// Read the InstallPath string value from one registry view; validate it looks
// like a real D2 install before accepting it.
static int try_registry_key(HKEY root, const char *subkey, REGSAM flags,
                            char *out_path, int out_cap)
{
   HKEY  key;
   DWORD len, type;
   LONG  rc;

   rc = RegOpenKeyExA(root, subkey, 0, KEY_READ | flags, &key);
   if (rc != ERROR_SUCCESS) return 0;

   len = (DWORD) out_cap;
   rc = RegQueryValueExA(key, "InstallPath", NULL, &type,
                         (LPBYTE) out_path, &len);
   RegCloseKey(key);

   if (rc != ERROR_SUCCESS || type != REG_SZ) return 0;

   // RegQueryValueExA does not guarantee null-termination; force it.
   out_path[out_cap - 1] = 0;

   // Strip trailing slash if present.
   {
      size_t n = strlen(out_path);
      if (n > 0 && (out_path[n-1] == '\\' || out_path[n-1] == '/'))
         out_path[n-1] = 0;
   }

   return dir_has_d2_mpq(out_path);
}
#endif // WIN32

int d2install_detect(char *out_path, int out_cap)
{
   static const char * const common_paths[] = {
      "C:\\Diablo II",
      "C:\\Program Files\\Diablo II",
      "C:\\Program Files (x86)\\Diablo II",
      "C:\\Games\\Diablo II",
      NULL
   };
   int i;

   if (out_path == NULL || out_cap < 2) return 0;
   out_path[0] = 0;

#ifdef WIN32
   // Native view first (matches the editor's own bitness).
   if (try_registry_key(HKEY_LOCAL_MACHINE,
                        "SOFTWARE\\Blizzard Entertainment\\Diablo II",
                        0, out_path, out_cap))
      return 1;

   // 32-bit installer view (D2 is 32-bit, so on a 64-bit Windows host its key
   // lives under WOW6432Node).
   if (try_registry_key(HKEY_LOCAL_MACHINE,
                        "SOFTWARE\\Blizzard Entertainment\\Diablo II",
                        KEY_WOW64_32KEY, out_path, out_cap))
      return 1;
#endif

   for (i = 0; common_paths[i] != NULL; i++)
   {
      if (dir_has_d2_mpq(common_paths[i]))
      {
         strncpy(out_path, common_paths[i], out_cap - 1);
         out_path[out_cap - 1] = 0;
         return 1;
      }
   }

   out_path[0] = 0;
   return 0;
}

int d2install_resolve_mpqs(void)
{
   char detected[512];
   const char *install;
   char full[512];
   char *buf;
   size_t len;
   FILE *fp;
   int i, filled = 0;

   install = glb_config.d2_install;

   if (install == NULL || install[0] == 0)
   {
      if (!d2install_detect(detected, sizeof(detected)))
         return 0;
      install = detected;

      // Cache the detected path so downstream code (project save, UI, logs)
      // can see where the editor resolved from.
      len = strlen(install);
      buf = (char *) malloc(len + 1);
      if (buf != NULL)
      {
         memcpy(buf, install, len + 1);
         glb_config.d2_install = buf;
         install = buf;
      }

      fprintf(stdout, "d2install: auto-detected <%s>\n", install);
      fprintf(stderr, "d2install: auto-detected <%s>\n", install);
   }
   else
   {
      fprintf(stdout, "d2install: using configured path <%s>\n", install);
   }

   for (i = 0; i < MAX_MPQ_FILE; i++)
   {
      if (glb_config.mpq_file[i] != NULL && glb_config.mpq_file[i][0] != 0)
         continue; // explicit INI entry wins

      snprintf(full, sizeof(full), "%s\\%s", install, MPQ_SLOT_NAMES[i]);

      fp = fopen(full, "rb");
      if (fp == NULL) continue;
      fclose(fp);

      len = strlen(full);
      buf = (char *) malloc(len + 1);
      if (buf == NULL) continue;
      memcpy(buf, full, len + 1);

      glb_config.mpq_file[i] = buf;
      filled++;

      fprintf(stdout, "d2install: slot %d -> %s\n", i, buf);
   }

   return filled;
}
