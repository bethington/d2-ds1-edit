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
   fclose(stdout);

   // console output
   fprintf(
      stderr,
      "\n\nds1edit_error() :\n%s\n\n",
      text
   );
   fflush(stderr);

   // window GUI, if possible
   allegro_message(text);

   // exit
   exit(DS1ERR_OTHER);
}
