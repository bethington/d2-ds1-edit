/* Derived from win_ds1edit by Paul Siramy.
 * See NOTICE at the repository root for attribution and license status. */

#include <stdio.h>
#include <stdlib.h>
#include "structs.h"
#include "error.h"


// ==========================================================================
// fatal error
void ds1edit_error(const char * text)
{
   // log
   printf("\nds1edit_error() :\n%s\n\n", text);
   fflush(stdout);

   // console output
   fprintf(
      stderr,
      "\n\nds1edit_error() :\n%s\n\n",
      text
   );
   fflush(stderr);

   // exit — do NOT fclose(stdout) before exit, the CRT handles stream
   // cleanup during exit(). Closing stdout early causes the CRT debug
   // invalid-parameter handler to fire (0xC0000409).
   exit(DS1ERR_OTHER);
}
