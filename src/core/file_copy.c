#include <stdio.h>
#include "core/file_copy.h"

int file_copy_checked(const char * src_path, const char * dst_path)
{
   FILE * in;
   FILE * out;
   char   buf[8192];
   size_t n;
   int    ok = 1;

   in = fopen(src_path, "rb");
   if (in == NULL)
      return FILE_COPY_NO_SOURCE;

   out = fopen(dst_path, "wb");
   if (out == NULL)
   {
      fclose(in);
      return FILE_COPY_FAILED;
   }

   while (ok && (n = fread(buf, 1, sizeof(buf), in)) > 0)
   {
      if (fwrite(buf, 1, n, out) != n)
         ok = 0;
   }
   if (ferror(in))
      ok = 0;
   fclose(in);

   /* fclose flushes, so a full disk can first show up here. */
   if (fclose(out) != 0)
      ok = 0;

   if (!ok)
   {
      remove(dst_path);
      return FILE_COPY_FAILED;
   }
   return FILE_COPY_OK;
}
