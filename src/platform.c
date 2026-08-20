/*
 * platform.c -- Win32 / POSIX implementations of the shims in platform.h.
 *
 * See the header for the two path rules (MPQ virtual paths keep backslashes;
 * filesystem paths use DS1_SEP).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
#endif

/* ---- path separators -------------------------------------------------- */

void ds1_path_normalize(char *path)
{
   char *p;

   if (path == NULL) return;

   for (p = path; *p != '\0'; p++)
   {
      if (DS1_IS_SEP(*p)) *p = DS1_SEP;
   }
}

/* ---- directory enumeration -------------------------------------------- */

#ifdef WIN32

int ds1_dir_open(DS1_DIR *d, const char *path)
{
   char             search[1024];
   WIN32_FIND_DATAA *fd;
   HANDLE            h;

   if (d == NULL || path == NULL) return 0;
   memset(d, 0, sizeof(*d));

   fd = (WIN32_FIND_DATAA *) malloc(sizeof(WIN32_FIND_DATAA));
   if (fd == NULL) return 0;

   snprintf(search, sizeof(search), "%s\\*", path);

   h = FindFirstFileA(search, fd);
   if (h == INVALID_HANDLE_VALUE)
   {
      free(fd);
      return 0;
   }

   d->handle    = (void *) h;
   d->find_data = (void *) fd;
   d->pending   = 1;   /* FindFirstFile already produced entry #1 */
   return 1;
}

int ds1_dir_next(DS1_DIR *d)
{
   WIN32_FIND_DATAA *fd;

   if (d == NULL || d->handle == NULL) return 0;
   fd = (WIN32_FIND_DATAA *) d->find_data;

   for (;;)
   {
      if (d->pending)
      {
         d->pending = 0;
      }
      else if (!FindNextFileA((HANDLE) d->handle, fd))
      {
         return 0;
      }

      if (strcmp(fd->cFileName, ".") == 0 || strcmp(fd->cFileName, "..") == 0)
         continue;

      snprintf(d->name, sizeof(d->name), "%s", fd->cFileName);
      d->is_dir = (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
      return 1;
   }
}

void ds1_dir_close(DS1_DIR *d)
{
   if (d == NULL) return;
   if (d->handle != NULL)
   {
      FindClose((HANDLE) d->handle);
      d->handle = NULL;
   }
   if (d->find_data != NULL)
   {
      free(d->find_data);
      d->find_data = NULL;
   }
}

int ds1_dir_exists(const char *path)
{
   DWORD a;
   if (path == NULL || *path == '\0') return 0;
   a = GetFileAttributesA(path);
   return (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
}

int ds1_file_exists(const char *path)
{
   DWORD a;
   if (path == NULL || *path == '\0') return 0;
   a = GetFileAttributesA(path);
   return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
}

/* Windows filesystems are case-insensitive already; a plain join is correct. */
int ds1_path_resolve_nocase(const char *root, const char *relative,
                            char *out, size_t out_cap)
{
   if (root == NULL || relative == NULL || out == NULL || out_cap == 0) return 0;
   snprintf(out, out_cap, "%s\\%s", root, relative);
   ds1_path_normalize(out);
   return (ds1_file_exists(out) || ds1_dir_exists(out)) ? 1 : 0;
}

#else  /* ---------------------------- POSIX ---------------------------- */

int ds1_dir_open(DS1_DIR *d, const char *path)
{
   DIR *dp;

   if (d == NULL || path == NULL) return 0;
   memset(d, 0, sizeof(*d));

   dp = opendir(path);
   if (dp == NULL) return 0;

   d->handle = (void *) dp;
   snprintf(d->root, sizeof(d->root), "%s", path);
   return 1;
}

int ds1_dir_next(DS1_DIR *d)
{
   struct dirent *e;

   if (d == NULL || d->handle == NULL) return 0;

   while ((e = readdir((DIR *) d->handle)) != NULL)
   {
      struct stat st;
      char full[2048];

      if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
         continue;

      snprintf(d->name, sizeof(d->name), "%s", e->d_name);

      /* d_type is not populated on every filesystem (notably some network
         and older ext variants), so fall back to stat when it is unknown. */
#ifdef DT_DIR
      if (e->d_type == DT_DIR)      { d->is_dir = 1; return 1; }
      if (e->d_type != DT_UNKNOWN)  { d->is_dir = 0; return 1; }
#endif
      snprintf(full, sizeof(full), "%s/%s", d->root, e->d_name);
      d->is_dir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
      return 1;
   }
   return 0;
}

void ds1_dir_close(DS1_DIR *d)
{
   if (d == NULL) return;
   if (d->handle != NULL)
   {
      closedir((DIR *) d->handle);
      d->handle = NULL;
   }
}

int ds1_dir_exists(const char *path)
{
   struct stat st;
   if (path == NULL || *path == '\0') return 0;
   return (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}

int ds1_file_exists(const char *path)
{
   struct stat st;
   if (path == NULL || *path == '\0') return 0;
   return (stat(path, &st) == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
}

/*
 * Walk `relative` one component at a time under `root`. Each component is
 * tried verbatim first -- the common case, and the only one that costs
 * nothing -- and only if that misses do we scan the directory for a
 * case-insensitive match.
 */
int ds1_path_resolve_nocase(const char *root, const char *relative,
                            char *out, size_t out_cap)
{
   char  rel[1024];
   char  cur[2048];
   char *comp;
   char *save;

   if (root == NULL || relative == NULL || out == NULL || out_cap == 0)
      return 0;

   snprintf(rel, sizeof(rel), "%s", relative);
   ds1_path_normalize(rel);              /* accept '\' from D2 data files */
   snprintf(cur, sizeof(cur), "%s", root);

   for (comp = strtok_r(rel, "/", &save);
        comp != NULL;
        comp = strtok_r(NULL, "/", &save))
   {
      char    probe[2048];
      DS1_DIR d;
      int     matched = 0;

      if (*comp == '\0') continue;

      snprintf(probe, sizeof(probe), "%s/%s", cur, comp);
      if (ds1_file_exists(probe) || ds1_dir_exists(probe))
      {
         snprintf(cur, sizeof(cur), "%s", probe);
         continue;
      }

      if (!ds1_dir_open(&d, cur)) return 0;
      while (ds1_dir_next(&d))
      {
         if (strcasecmp(d.name, comp) == 0)
         {
            snprintf(probe, sizeof(probe), "%s/%s", cur, d.name);
            matched = 1;
            break;
         }
      }
      ds1_dir_close(&d);

      if (!matched) return 0;
      snprintf(cur, sizeof(cur), "%s", probe);
   }

   snprintf(out, out_cap, "%s", cur);
   return 1;
}

#endif /* WIN32 */
