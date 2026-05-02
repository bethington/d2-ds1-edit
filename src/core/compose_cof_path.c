#include <stdio.h>
#include <string.h>

#include "core/compose_cof_path.h"

int compose_cof_path_build(char *out_buf, int out_cap,
                           const char *base,
                           const char *token,
                           const char *mode,
                           const char *wclass)
{
   int written;

   if (out_buf == NULL || out_cap <= 0
       || base == NULL || token == NULL || mode == NULL)
      return 0;
   if (wclass == NULL)
      wclass = "";

   /* <base>\<token>\COF\<token><mode><wclass>.cof */
   written = snprintf(out_buf, (size_t) out_cap,
                      "%s\\%s\\COF\\%s%s%s.cof",
                      base, token, token, mode, wclass);

   if (written < 0 || written >= out_cap)
   {
      out_buf[0] = 0;
      return 0;
   }
   return 1;
}
