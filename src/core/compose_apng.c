#include <stdio.h>
#include <string.h>

#include "core/animdata.h"
#include "core/apng_writer.h"
#include "core/compose_apng.h"
#include "core/compose_render.h"

#define COMPOSE_APNG_DEFAULT_DELAY_MS  40

int compose_apng_write(const COMPOSE_RENDER_RESULT_S *result,
                       const char *cof_name,
                       const char *output_path)
{
   APNG_WRITER_S *w;
   int delay_ms;
   int delay_num, delay_den;
   int i;
   int ok = 1;

   if (result == NULL || output_path == NULL)
      return 0;
   if (result->frames == NULL || result->frame_count <= 0
       || result->width <= 0 || result->height <= 0)
      return 0;

   delay_ms = animdata_get_frame_delay_ms(cof_name,
                                          COMPOSE_APNG_DEFAULT_DELAY_MS);
   /* APNG stores delay as a rational num/den seconds. Use ms / 1000. */
   delay_num = delay_ms;
   delay_den = 1000;

   w = apng_writer_open(output_path,
                        result->width, result->height,
                        result->frame_count,
                        0 /* num_plays = 0 means infinite loop */);
   if (w == NULL)
      return 0;

   for (i = 0; i < result->frame_count; i++)
   {
      if (result->frames[i] == NULL)
      {
         ok = 0;
         break;
      }
      if (!apng_writer_write_frame(w, result->frames[i],
                                   delay_num, delay_den))
      {
         ok = 0;
         break;
      }
   }

   if (!apng_writer_close(w))
      ok = 0;

   return ok;
}

int compose_apng_export(const COMPOSE_RENDER_PARAMS_S *params,
                        const char *output_path)
{
   COMPOSE_RENDER_RESULT_S result;
   char cof_name[64];
   int ok;

   if (params == NULL || output_path == NULL)
      return 0;

   if (!compose_render(params, &result))
      return 0;

   /* Build the COF basename used as the animdata key:
    * <token><mode><wclass> -- e.g. "NEWLHTH" */
   snprintf(cof_name, sizeof(cof_name), "%s%s%s",
            params->token != NULL ? params->token : "",
            params->mode  != NULL ? params->mode  : "",
            params->wclass != NULL ? params->wclass : "");

   ok = compose_apng_write(&result, cof_name, output_path);
   compose_render_free(&result);
   return ok;
}
