#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>

#include "structs.h"
#include "core/project.h"
#include "core/upscale.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

static void set_error_text(char *error, int error_cap, const char *text)
{
   if (error == NULL || error_cap <= 0)
      return;

   if (text == NULL)
      text = "Unknown upscale error.";

   strncpy(error, text, error_cap - 1);
   error[error_cap - 1] = 0;
}

static int ends_with_png(const char *path)
{
   const char *ext;

   if (path == NULL)
      return 0;

   ext = strrchr(path, '.');
   if (ext == NULL)
      return 0;

   return stricmp(ext, ".png") == 0;
}

static int create_memory_bitmap_context(int enable)
{
   static int saved_flags = 0;
   static int saved_format = 0;

   if (enable)
   {
      saved_flags = al_get_new_bitmap_flags();
      saved_format = al_get_new_bitmap_format();
      al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);
      al_set_new_bitmap_format(ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE);
   }
   else
   {
      al_set_new_bitmap_flags(saved_flags);
      al_set_new_bitmap_format(saved_format);
   }

   return 1;
}

static unsigned int read_pixel_u32(ALLEGRO_LOCKED_REGION *lock, int x, int y)
{
   unsigned char *row = (unsigned char *) lock->data + y * lock->pitch;
   return ((unsigned int *) row)[x];
}

static void write_pixel_u32(ALLEGRO_LOCKED_REGION *lock, int x, int y,
                            unsigned int value)
{
   unsigned char *row = (unsigned char *) lock->data + y * lock->pitch;
   ((unsigned int *) row)[x] = value;
}

static unsigned int sample_clamped(ALLEGRO_LOCKED_REGION *lock,
                                   int x, int y, int w, int h)
{
   if (x < 0) x = 0;
   if (y < 0) y = 0;
   if (x >= w) x = w - 1;
   if (y >= h) y = h - 1;
   return read_pixel_u32(lock, x, y);
}

static ALLEGRO_BITMAP *upscale_bitmap_scale2x(ALLEGRO_BITMAP *src)
{
   ALLEGRO_BITMAP *dst;
   ALLEGRO_LOCKED_REGION *src_lock;
   ALLEGRO_LOCKED_REGION *dst_lock;
   int w, h;
   int x, y;

   if (src == NULL)
      return NULL;

   w = al_get_bitmap_width(src);
   h = al_get_bitmap_height(src);
   if (w <= 0 || h <= 0)
      return NULL;

   create_memory_bitmap_context(1);
   dst = al_create_bitmap(w * 2, h * 2);
   create_memory_bitmap_context(0);
   if (dst == NULL)
      return NULL;

   src_lock = al_lock_bitmap(src, ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
                             ALLEGRO_LOCK_READONLY);
   dst_lock = al_lock_bitmap(dst, ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
                             ALLEGRO_LOCK_WRITEONLY);
   if (src_lock == NULL || dst_lock == NULL)
   {
      if (src_lock != NULL)
         al_unlock_bitmap(src);
      if (dst_lock != NULL)
         al_unlock_bitmap(dst);
      al_destroy_bitmap(dst);
      return NULL;
   }

   for (y = 0; y < h; y++)
   {
      for (x = 0; x < w; x++)
      {
         unsigned int b = sample_clamped(src_lock, x, y - 1, w, h);
         unsigned int d = sample_clamped(src_lock, x - 1, y, w, h);
         unsigned int e = sample_clamped(src_lock, x, y, w, h);
         unsigned int f = sample_clamped(src_lock, x + 1, y, w, h);
         unsigned int hpx = sample_clamped(src_lock, x, y + 1, w, h);
         unsigned int e0 = e, e1 = e, e2 = e, e3 = e;

         if (b != hpx && d != f)
         {
            if (d == b) e0 = d;
            if (b == f) e1 = f;
            if (d == hpx) e2 = d;
            if (hpx == f) e3 = f;
         }

         write_pixel_u32(dst_lock, x * 2,     y * 2,     e0);
         write_pixel_u32(dst_lock, x * 2 + 1, y * 2,     e1);
         write_pixel_u32(dst_lock, x * 2,     y * 2 + 1, e2);
         write_pixel_u32(dst_lock, x * 2 + 1, y * 2 + 1, e3);
      }
   }

   al_unlock_bitmap(src);
   al_unlock_bitmap(dst);
   return dst;
}

static ALLEGRO_BITMAP *upscale_bitmap_local(ALLEGRO_BITMAP *src, int scale)
{
   ALLEGRO_BITMAP *cur;
   ALLEGRO_BITMAP *next;
   int passes;
   int i;

   if (src == NULL)
      return NULL;
   if (scale != 2 && scale != 4)
      return NULL;

   passes = (scale == 4) ? 2 : 1;
   cur = src;
   for (i = 0; i < passes; i++)
   {
      next = upscale_bitmap_scale2x(cur);
      if (next == NULL)
      {
         if (cur != src)
            al_destroy_bitmap(cur);
         return NULL;
      }
      if (cur != src)
         al_destroy_bitmap(cur);
      cur = next;
   }

   return cur;
}

#ifdef WIN32
static int directory_exists(const char *path)
{
   DWORD attrs;

   if (path == NULL || path[0] == 0)
      return 0;

   attrs = GetFileAttributesA(path);
   return (attrs != INVALID_FILE_ATTRIBUTES) &&
          ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

static int upscale_directory_local_recursive(const char *src_dir,
                                             const char *dst_dir,
                                             int scale,
                                             char *error,
                                             int error_cap)
{
   WIN32_FIND_DATAA fd;
   HANDLE hFind;
   char search_path[PATH_MAX];

   snprintf(search_path, sizeof(search_path), "%s\\*", src_dir);
   hFind = FindFirstFileA(search_path, &fd);
   if (hFind == INVALID_HANDLE_VALUE)
   {
      set_error_text(error, error_cap, "Unable to enumerate staged PNGs for local upscale.");
      return 0;
   }

   do
   {
      char src_path[PATH_MAX];
      char dst_path[PATH_MAX];

      if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
         continue;

      snprintf(src_path, sizeof(src_path), "%s\\%s", src_dir, fd.cFileName);
      snprintf(dst_path, sizeof(dst_path), "%s\\%s", dst_dir, fd.cFileName);

      if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
      {
         if (!CreateDirectoryA(dst_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
         {
            set_error_text(error, error_cap, "Unable to create output directory for local upscale.");
            FindClose(hFind);
            return 0;
         }
         if (!upscale_directory_local_recursive(src_path, dst_path, scale, error, error_cap))
         {
            FindClose(hFind);
            return 0;
         }
      }
      else if (ends_with_png(fd.cFileName))
      {
         ALLEGRO_BITMAP *src_bmp;
         ALLEGRO_BITMAP *scaled_bmp;

         create_memory_bitmap_context(1);
         src_bmp = al_load_bitmap(src_path);
         create_memory_bitmap_context(0);
         if (src_bmp == NULL)
         {
            set_error_text(error, error_cap, "Failed to load staged PNG for local upscale.");
            FindClose(hFind);
            return 0;
         }

         scaled_bmp = upscale_bitmap_local(src_bmp, scale);
         al_destroy_bitmap(src_bmp);
         if (scaled_bmp == NULL)
         {
            set_error_text(error, error_cap, "Local upscale failed while processing a PNG.");
            FindClose(hFind);
            return 0;
         }

         if (!project_ensure_parent_dirs(dst_path) || !al_save_bitmap(dst_path, scaled_bmp))
         {
            al_destroy_bitmap(scaled_bmp);
            set_error_text(error, error_cap, "Failed to save local upscaled PNG.");
            FindClose(hFind);
            return 0;
         }

         al_destroy_bitmap(scaled_bmp);
      }
   } while (FindNextFileA(hFind, &fd));

   FindClose(hFind);
   return 1;
}

static int run_command(const char *command)
{
   int rc;

   if (command == NULL || command[0] == 0)
      return 0;

   rc = system(command);
   return rc == 0;
}
#endif

int upscale_is_remote_configured(void)
{
   return glb_config.upscale_enabled &&
          glb_config.upscale_service_url != NULL &&
          glb_config.upscale_service_url[0] != 0;
}

int upscale_create_temp_dir(char *out, int out_cap)
{
#ifdef WIN32
   char temp_path[MAX_PATH];
   char temp_file[MAX_PATH];

   if (out == NULL || out_cap <= 0)
      return 0;

   if (GetTempPathA(sizeof(temp_path), temp_path) == 0)
      return 0;
   if (GetTempFileNameA(temp_path, "d2u", 0, temp_file) == 0)
      return 0;

   DeleteFileA(temp_file);
   if (!CreateDirectoryA(temp_file, NULL))
      return 0;

   strncpy(out, temp_file, out_cap - 1);
   out[out_cap - 1] = 0;
   return 1;
#else
   (void) out;
   (void) out_cap;
   return 0;
#endif
}

int upscale_remove_tree(const char *path)
{
#ifdef WIN32
   WIN32_FIND_DATAA fd;
   HANDLE hFind;
   char search_path[PATH_MAX];

   if (path == NULL || path[0] == 0 || !directory_exists(path))
      return 1;

   snprintf(search_path, sizeof(search_path), "%s\\*", path);
   hFind = FindFirstFileA(search_path, &fd);
   if (hFind == INVALID_HANDLE_VALUE)
      return RemoveDirectoryA(path);

   do
   {
      char child[PATH_MAX];

      if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
         continue;

      snprintf(child, sizeof(child), "%s\\%s", path, fd.cFileName);
      if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
         upscale_remove_tree(child);
      else
         DeleteFileA(child);
   } while (FindNextFileA(hFind, &fd));

   FindClose(hFind);
   return RemoveDirectoryA(path) || GetLastError() == ERROR_PATH_NOT_FOUND;
#else
   (void) path;
   return 0;
#endif
}

int upscale_directory_local(const char *src_dir, const char *dst_dir,
                            int scale, char *error, int error_cap)
{
#ifdef WIN32
   if (src_dir == NULL || dst_dir == NULL)
   {
      set_error_text(error, error_cap, "Local upscale paths were not provided.");
      return 0;
   }
   if (scale != 2 && scale != 4)
   {
      set_error_text(error, error_cap, "Local upscale only supports 2x or 4x.");
      return 0;
   }
   if (!CreateDirectoryA(dst_dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
   {
      set_error_text(error, error_cap, "Unable to create local upscale output directory.");
      return 0;
   }

   return upscale_directory_local_recursive(src_dir, dst_dir, scale, error, error_cap);
#else
   (void) src_dir;
   (void) dst_dir;
   (void) scale;
   set_error_text(error, error_cap, "Local upscale is only implemented on Windows builds.");
   return 0;
#endif
}

int upscale_directory_remote(const char *src_dir, const char *dst_dir,
                             int scale, const char *method,
                             char *error, int error_cap)
{
#ifdef WIN32
   char work_dir[PATH_MAX];
   char zip_in[PATH_MAX];
   char zip_out[PATH_MAX];
   char command[4096];

   if (!upscale_is_remote_configured())
   {
      set_error_text(error, error_cap, "Remote upscale is not configured.");
      return 0;
   }
   if (src_dir == NULL || dst_dir == NULL)
   {
      set_error_text(error, error_cap, "Remote upscale paths were not provided.");
      return 0;
   }
   if (method == NULL || method[0] == 0)
      method = "realesrgan";

   if (!upscale_create_temp_dir(work_dir, sizeof(work_dir)))
   {
      set_error_text(error, error_cap, "Unable to allocate a temporary working directory for remote upscale.");
      return 0;
   }

   snprintf(zip_in, sizeof(zip_in), "%s\\input.zip", work_dir);
   snprintf(zip_out, sizeof(zip_out), "%s\\output.zip", work_dir);

   snprintf(command, sizeof(command),
      "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Compress-Archive -Path '%s\\*' -DestinationPath '%s' -Force\"",
      src_dir,
      zip_in);
   if (!run_command(command))
   {
      upscale_remove_tree(work_dir);
      set_error_text(error, error_cap, "Failed to create the staged ZIP archive for remote upscale.");
      return 0;
   }

   snprintf(command, sizeof(command),
      "powershell -NoProfile -ExecutionPolicy Bypass -Command \"$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -UseBasicParsing -Uri '%s/upscale/archive?method=%s&scale=%d' -Method Post -InFile '%s' -ContentType 'application/zip' -OutFile '%s'\"",
      glb_config.upscale_service_url,
      method,
      scale,
      zip_in,
      zip_out);
   if (!run_command(command))
   {
      upscale_remove_tree(work_dir);
      set_error_text(error, error_cap, "Remote upscale request failed.");
      return 0;
   }

   if (!CreateDirectoryA(dst_dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
   {
      upscale_remove_tree(work_dir);
      set_error_text(error, error_cap, "Unable to create the output directory for remote upscale.");
      return 0;
   }

   snprintf(command, sizeof(command),
      "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"",
      zip_out,
      dst_dir);
   if (!run_command(command))
   {
      upscale_remove_tree(work_dir);
      set_error_text(error, error_cap, "Failed to extract the remote upscale result archive.");
      return 0;
   }

   upscale_remove_tree(work_dir);
   return 1;
#else
   (void) src_dir;
   (void) dst_dir;
   (void) scale;
   (void) method;
   set_error_text(error, error_cap, "Remote upscale client is currently implemented for Windows builds only.");
   return 0;
#endif
}