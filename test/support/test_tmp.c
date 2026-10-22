/* Owned scratch directories for package C tests. See test_tmp.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
   #include <direct.h>
   #include <process.h>
   #define TT_GETPID() _getpid()
   #define TT_MKDIR(p) _mkdir(p)
   #define TT_RMDIR(p) _rmdir(p)
   #define TT_SEP "\\"
#else
   #include <unistd.h>
   #define TT_GETPID() getpid()
   #define TT_MKDIR(p) mkdir((p), 0755)
   #define TT_RMDIR(p) rmdir(p)
   #define TT_SEP "/"
#endif

#include <allegro5/allegro.h>

#include "test_tmp.h"

int tt_mkdir(const char *path)
{
   struct stat st;
   if (TT_MKDIR(path) == 0)
      return 1;
   return stat(path, &st) == 0 && (st.st_mode & S_IFDIR);
}

void tt_remove_tree(const char *path)
{
   ALLEGRO_FS_ENTRY *dir = al_create_fs_entry(path);
   if (dir == NULL)
      return;
   if ((al_get_fs_entry_mode(dir) & ALLEGRO_FILEMODE_ISDIR) && al_open_directory(dir))
   {
      ALLEGRO_FS_ENTRY *e;
      while ((e = al_read_directory(dir)) != NULL)
      {
         const char *name = al_get_fs_entry_name(e);
         if (al_get_fs_entry_mode(e) & ALLEGRO_FILEMODE_ISDIR)
            tt_remove_tree(name);
         else
            remove(name);
         al_destroy_fs_entry(e);
      }
      al_close_directory(dir);
      al_destroy_fs_entry(dir);
      TT_RMDIR(path);
      return;
   }
   al_destroy_fs_entry(dir);
   remove(path);
}

int tt_root_create(const char *tag, char *out, size_t cap)
{
   char dir[512];
   int n;

   if (!al_is_system_installed())
      al_init();
   n = snprintf(dir, sizeof(dir), "pkgc_%s_%d", tag, (int) TT_GETPID());
   if (n <= 0 || (size_t) n >= sizeof(dir))
      return 0;
   tt_remove_tree(dir);
   if (!tt_mkdir(dir))
      return 0;
   n = snprintf(out, cap, "%s%s", dir, TT_SEP);
   return n > 0 && (size_t) n < cap;
}

int tt_write_file(const char *path, const void *data, size_t len)
{
   FILE *f = fopen(path, "wb");
   size_t w;
   if (f == NULL)
      return 0;
   w = len ? fwrite(data, 1, len, f) : 0;
   if (fclose(f) != 0)
      return 0;
   return w == len;
}

unsigned char *tt_read_file(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb");
   unsigned char *buf;
   long sz;

   *len = 0;
   if (f == NULL)
      return NULL;
   if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0)
   {
      fclose(f);
      return NULL;
   }
   buf = (unsigned char *) malloc((size_t) sz + 1);
   if (buf != NULL && sz > 0 && fread(buf, 1, (size_t) sz, f) != (size_t) sz)
   {
      free(buf);
      buf = NULL;
   }
   fclose(f);
   if (buf != NULL)
      *len = (size_t) sz;
   return buf;
}

int tt_file_exists(const char *path)
{
   struct stat st;
   return stat(path, &st) == 0;
}

int tt_files_equal(const char *a, const char *b)
{
   size_t la, lb;
   unsigned char *ba = tt_read_file(a, &la), *bb = tt_read_file(b, &lb);
   int eq = ba != NULL && bb != NULL && la == lb && (la == 0 || memcmp(ba, bb, la) == 0);
   free(ba);
   free(bb);
   return eq;
}

int tt_count_entries(const char *path)
{
   ALLEGRO_FS_ENTRY *dir = al_create_fs_entry(path), *e;
   int n = 0;
   if (dir == NULL)
      return -1;
   if (!al_open_directory(dir))
   {
      al_destroy_fs_entry(dir);
      return -1;
   }
   while ((e = al_read_directory(dir)) != NULL)
   {
      n++;
      al_destroy_fs_entry(e);
   }
   al_close_directory(dir);
   al_destroy_fs_entry(dir);
   return n;
}
