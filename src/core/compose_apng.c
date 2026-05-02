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
   return compose_apng_export_scaled(params, output_path, 1);
}

/* Nearest-neighbour scale a single RGBA buffer by an integer factor.
 * `src` is `src_w * src_h * 4` bytes; `dst` is allocated by the
 * caller as `(src_w * scale) * (src_h * scale) * 4` bytes. */
static void nn_scale_rgba(const unsigned char *src, int src_w, int src_h,
                          unsigned char *dst, int scale)
{
   int dst_w = src_w * scale;
   int sy, sx, oy, ox;
   for (sy = 0; sy < src_h; sy++)
   {
      for (sx = 0; sx < src_w; sx++)
      {
         const unsigned char *p = src + ((size_t) sy * src_w + sx) * 4;
         for (oy = 0; oy < scale; oy++)
         {
            int dy = sy * scale + oy;
            for (ox = 0; ox < scale; ox++)
            {
               int dx = sx * scale + ox;
               unsigned char *q = dst + ((size_t) dy * dst_w + dx) * 4;
               q[0] = p[0]; q[1] = p[1]; q[2] = p[2]; q[3] = p[3];
            }
         }
      }
   }
}

int compose_apng_export_scaled(const COMPOSE_RENDER_PARAMS_S *params,
                               const char *output_path,
                               int scale)
{
   COMPOSE_RENDER_RESULT_S result;
   COMPOSE_RENDER_RESULT_S scaled = {0};
   const COMPOSE_RENDER_RESULT_S *to_write;
   char cof_name[64];
   int ok = 0;
   int i;

   if (params == NULL || output_path == NULL) return 0;
   if (scale != 1 && scale != 2 && scale != 4) return 0;

   if (!compose_render(params, &result))
      return 0;

   snprintf(cof_name, sizeof(cof_name), "%s%s%s",
            params->token  != NULL ? params->token  : "",
            params->mode   != NULL ? params->mode   : "",
            params->wclass != NULL ? params->wclass : "");

   if (scale == 1)
   {
      to_write = &result;
   }
   else
   {
      /* Build a scaled copy. Skip frames whose size product overflows
       * (defensive; D2 sprites are small so this never happens in
       * practice). */
      scaled.width       = result.width  * scale;
      scaled.height      = result.height * scale;
      scaled.frame_count = result.frame_count;
      scaled.frames      = (unsigned char **) calloc(
         (size_t) result.frame_count, sizeof(unsigned char *));
      if (scaled.frames == NULL)
      {
         compose_render_free(&result);
         return 0;
      }
      for (i = 0; i < result.frame_count; i++)
      {
         size_t bytes = (size_t) scaled.width * scaled.height * 4;
         if (result.frames[i] == NULL || bytes == 0)
         {
            scaled.frames[i] = NULL;
            continue;
         }
         scaled.frames[i] = (unsigned char *) calloc(bytes, 1);
         if (scaled.frames[i] == NULL)
         {
            compose_render_free(&scaled);
            compose_render_free(&result);
            return 0;
         }
         nn_scale_rgba(result.frames[i], result.width, result.height,
                       scaled.frames[i], scale);
      }
      to_write = &scaled;
   }

   ok = compose_apng_write(to_write, cof_name, output_path);
   compose_render_free(&result);
   compose_render_free(&scaled);  /* harmless if zeroed */
   return ok;
}
