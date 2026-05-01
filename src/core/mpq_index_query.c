// Pure query helpers for the preset index.
//
// Split into its own translation unit so unit tests can link just this file
// without needing stubs for glb_ds1edit / misc_get_txt_column_num / txt_load
// and the other heavy dependencies the build-side of mpq_index.c pulls in.

#include <string.h>
#include <ctype.h>

#include "core/mpq_index.h"

static const char *basename_of(const char *path)
{
   const char *p, *last = path;
   if (path == NULL) return NULL;
   for (p = path; *p != 0; p++)
   {
      if (*p == '\\' || *p == '/') last = p + 1;
   }
   return last;
}

static int streq_ci(const char *a, const char *b)
{
   if (a == NULL || b == NULL) return 0;
   while (*a && *b)
   {
      if (tolower((unsigned char) *a) != tolower((unsigned char) *b)) return 0;
      a++; b++;
   }
   return *a == 0 && *b == 0;
}

const PRESET_ENTRY_S *mpq_index_find_by_ds1_name_in(const PRESET_ENTRY_S *arr,
                                                    int count,
                                                    const char *ds1_basename,
                                                    int *out_file_slot)
{
   int i, f;
   const char *base;
   if (arr == NULL || count <= 0 || ds1_basename == NULL) return NULL;

   for (i = 0; i < count; i++)
   {
      for (f = 0; f < arr[i].ds1_count; f++)
      {
         base = basename_of(arr[i].ds1_files[f]);
         if (base != NULL && streq_ci(base, ds1_basename))
         {
            if (out_file_slot != NULL) *out_file_slot = f;
            return &arr[i];
         }
      }
   }
   return NULL;
}
