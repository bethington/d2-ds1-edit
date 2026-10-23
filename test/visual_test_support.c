/* See visual_test_support.h. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "visual_test_support.h"

/* ---- editor globals the code under test reads -------------------------- */

GLB_DS1EDIT_S glb_ds1edit;
CONFIG_S      glb_config;
static DT1_S  s_dt1[DT1_MAX];
DT1_S        *glb_dt1 = s_dt1;

/* ---- archive fetch ------------------------------------------------------ */

#define VISUAL_FETCH_MAX 64

typedef struct
{
   char           name[260];
   unsigned char *bytes;
   size_t         len;
} VISUAL_FILE;

static VISUAL_FILE s_files[VISUAL_FETCH_MAX];
static int         s_file_count;
static int         s_hits, s_misses;

static void normalize(const char *in, char *out, size_t cap)
{
   size_t i;
   for (i = 0; in[i] != 0 && i + 1 < cap; i++)
   {
      char c = in[i];
      out[i] = (c == '/') ? '\\' : (char) tolower((unsigned char) c);
   }
   out[i] = 0;
}

void visual_fetch_reset(void)
{
   int i;
   for (i = 0; i < s_file_count; i++)
      free(s_files[i].bytes);
   memset(s_files, 0, sizeof(s_files));
   s_file_count = 0;
   s_hits = s_misses = 0;
}

int visual_fetch_add(const char *name, const void *bytes, size_t len)
{
   VISUAL_FILE *f;

   if (name == NULL || s_file_count >= VISUAL_FETCH_MAX)
      return 0;
   f = &s_files[s_file_count];
   normalize(name, f->name, sizeof(f->name));
   f->bytes = (unsigned char *) malloc(len > 0 ? len : 1);
   if (f->bytes == NULL)
      return 0;
   if (len > 0)
      memcpy(f->bytes, bytes, len);
   f->len = len;
   s_file_count++;
   return 1;
}

int visual_fetch_hits(void)   { return s_hits; }
int visual_fetch_misses(void) { return s_misses; }

int misc_load_mpq_file(char *filename, char **buffer, long *buf_len, int output)
{
   char key[260];
   int  i;

   (void) output;
   if (buffer != NULL)  *buffer = NULL;
   if (buf_len != NULL) *buf_len = 0;
   if (filename == NULL || buffer == NULL || buf_len == NULL)
      return -1;

   normalize(filename, key, sizeof(key));
   for (i = s_file_count - 1; i >= 0; i--)
   {
      if (strcmp(s_files[i].name, key) == 0)
      {
         *buffer = (char *) malloc(s_files[i].len > 0 ? s_files[i].len : 1);
         if (*buffer == NULL)
            return -1;
         memcpy(*buffer, s_files[i].bytes, s_files[i].len);
         *buf_len = (long) s_files[i].len;
         s_hits++;
         return i;
      }
   }
   s_misses++;
   return -1;
}

int mod_load_in_mem(char *moddir, char *filename, void **buffer, long *buf_len,
                    int output)
{
   (void) moddir;
   return misc_load_mpq_file(filename, (char **) buffer, buf_len, output);
}

/* ---- errors ------------------------------------------------------------- */

static int  s_error_count;
static char s_last_error[512];

void ds1edit_error(const char *text)
{
   s_error_count++;
   snprintf(s_last_error, sizeof(s_last_error), "%s", text ? text : "");
}

int visual_error_count(void)          { return s_error_count; }
const char *visual_last_error(void)   { return s_last_error; }

/* ---- misc --------------------------------------------------------------- */

unsigned char *visual_read_file(const char *path, size_t *len)
{
   FILE          *fp;
   long           size;
   unsigned char *buf;

   if (len != NULL)
      *len = 0;
   fp = fopen(path, "rb");
   if (fp == NULL)
      return NULL;
   if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0
       || fseek(fp, 0, SEEK_SET) != 0)
   {
      fclose(fp);
      return NULL;
   }
   buf = (unsigned char *) malloc((size_t) size + 1);
   if (buf != NULL && fread(buf, 1, (size_t) size, fp) != (size_t) size)
   {
      free(buf);
      buf = NULL;
   }
   fclose(fp);
   if (buf != NULL && len != NULL)
      *len = (size_t) size;
   return buf;
}

int visual_allegro_init(void)
{
   static int done = 0;

   if (!done)
   {
      if (!al_init())
         return 0;
      done = 1;
   }
   al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);
   al_set_new_bitmap_format(ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE);
   return 1;
}

/* The export progress dialog repaints through this; nothing to show here. */
void misc_draw_screen(int mx, int my)
{
   (void) mx;
   (void) my;
}
