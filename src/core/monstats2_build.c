/* MPQ-side glue for monstats2. Kept in its own translation unit so the
 * pure parser in monstats2.c links cleanly into unit tests without
 * dragging in misc_load_mpq_file. */

#include <stdlib.h>

#include "core/monstats2.h"

extern int misc_load_mpq_file(char *filename, char **buffer,
                              long *buf_len, int output);

extern MONSTATS2_ENTRY_S * const monstats2_storage_ptr;
extern const int                 monstats2_storage_cap;
extern void monstats2_set_count(int n);

int monstats2_build(void)
{
   char *buf = NULL;
   long buf_len = 0;
   int ok;
   int n = 0;

   monstats2_reset();

   if (misc_load_mpq_file("Data\\Global\\Excel\\MonStats2.txt",
                          &buf, &buf_len, 0) == -1
       || buf == NULL)
      return 0;

   ok = monstats2_parse(buf, monstats2_storage_ptr,
                        monstats2_storage_cap, &n);
   monstats2_set_count(ok ? n : 0);
   free(buf);
   return ok ? 1 : 0;
}
