#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <allegro5/allegro.h>

#include "structs.h"
#include "core/animdata.h"
#include "core/apng_writer.h"
#include "core/compose_apng.h"
#include "core/compose_render.h"
#include "core/compose_scale.h"
#include "core/upscale.h"

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

/* Save one RGBA buffer as a PNG via Allegro. The compose pipeline is
 * already running on the main Allegro thread (DCC decoding requires
 * it), so al_create_bitmap + al_save_bitmap are safe. */
static int save_rgba_as_png(const unsigned char *rgba, int w, int h,
                            const char *path)
{
   ALLEGRO_BITMAP *bmp;
   ALLEGRO_LOCKED_REGION *lock;
   int y;
   int ok = 0;

   if (rgba == NULL || w <= 0 || h <= 0 || path == NULL) return 0;

   bmp = al_create_bitmap(w, h);
   if (bmp == NULL) return 0;

   lock = al_lock_bitmap(bmp,
                         ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
                         ALLEGRO_LOCK_WRITEONLY);
   if (lock != NULL)
   {
      for (y = 0; y < h; y++)
      {
         unsigned char *dst = (unsigned char *) lock->data + y * lock->pitch;
         const unsigned char *src = rgba + (size_t) y * w * 4;
         memcpy(dst, src, (size_t) w * 4);
      }
      al_unlock_bitmap(bmp);
      ok = al_save_bitmap(path, bmp) ? 1 : 0;
   }
   al_destroy_bitmap(bmp);
   return ok;
}

/* Load a PNG and copy its pixels into a freshly-allocated RGBA
 * buffer. *out_w / *out_h receive the dimensions. Caller frees the
 * buffer. Returns NULL on failure. */
static unsigned char *load_png_as_rgba(const char *path,
                                       int *out_w, int *out_h)
{
   ALLEGRO_BITMAP *bmp;
   ALLEGRO_LOCKED_REGION *lock;
   unsigned char *out = NULL;
   int w, h, y;

   if (path == NULL) return NULL;
   bmp = al_load_bitmap(path);
   if (bmp == NULL) return NULL;

   w = al_get_bitmap_width(bmp);
   h = al_get_bitmap_height(bmp);
   if (w <= 0 || h <= 0) { al_destroy_bitmap(bmp); return NULL; }

   out = (unsigned char *) calloc((size_t) w * h * 4, 1);
   if (out == NULL) { al_destroy_bitmap(bmp); return NULL; }

   lock = al_lock_bitmap(bmp,
                         ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
                         ALLEGRO_LOCK_READONLY);
   if (lock != NULL)
   {
      for (y = 0; y < h; y++)
      {
         const unsigned char *src = (unsigned char *) lock->data + y * lock->pitch;
         unsigned char *dst = out + (size_t) y * w * 4;
         memcpy(dst, src, (size_t) w * 4);
      }
      al_unlock_bitmap(bmp);
   }
   else
   {
      free(out); out = NULL;
   }
   al_destroy_bitmap(bmp);

   if (out != NULL) { *out_w = w; *out_h = h; }
   return out;
}

/* Upscale every frame of `result` via the remote ML service (the same
 * Real-ESRGAN pipeline raw export uses). Writes per-frame PNGs into a
 * temp staging dir, packs+POSTs them, unpacks the upscaled response,
 * loads the upscaled frames back into a freshly-allocated
 * COMPOSE_RENDER_RESULT_S in `out`. Returns 1 on success, 0 on any
 * failure (caller falls back to local NN). */
static int try_remote_upscale(const COMPOSE_RENDER_RESULT_S *src,
                              int scale,
                              COMPOSE_RENDER_RESULT_S *out)
{
   char in_dir[512];
   char out_dir[512];
   char err[256];
   int i;
   int ok_so_far = 0;

   if (src == NULL || out == NULL) return 0;
   if (!upscale_is_remote_configured()) return 0;
   if (src->frame_count <= 0) return 0;
   memset(out, 0, sizeof(*out));
   err[0] = 0;

   if (!upscale_create_temp_dir(in_dir, sizeof(in_dir))) return 0;
   if (!upscale_create_temp_dir(out_dir, sizeof(out_dir)))
   {
      upscale_remove_tree(in_dir);
      return 0;
   }

   /* Stage every frame as a numbered PNG. The remote service preserves
    * filenames, so frame_N.png in -> frame_N.png out. */
   for (i = 0; i < src->frame_count; i++)
   {
      char path[768];
      snprintf(path, sizeof(path), "%s\\frame_%03d.png", in_dir, i);
      if (src->frames[i] == NULL) goto cleanup;
      if (!save_rgba_as_png(src->frames[i], src->width, src->height, path))
         goto cleanup;
   }

   /* One POST upscales the whole batch. method="realesrgan" matches
    * the raw-export default and what the docker server expects. */
   if (!upscale_directory_remote(in_dir, out_dir, scale,
                                 "realesrgan", err, sizeof(err)))
      goto cleanup;

   /* Read the upscaled frames back. Dimensions come from the first
    * loaded frame and are asserted to be src*scale. */
   out->frames = (unsigned char **) calloc((size_t) src->frame_count,
                                            sizeof(unsigned char *));
   if (out->frames == NULL) goto cleanup;
   out->frame_count = src->frame_count;

   for (i = 0; i < src->frame_count; i++)
   {
      char path[768];
      int w, h;
      snprintf(path, sizeof(path), "%s\\frame_%03d.png", out_dir, i);
      out->frames[i] = load_png_as_rgba(path, &w, &h);
      if (out->frames[i] == NULL) goto cleanup;
      if (out->width == 0)  { out->width  = w; out->height = h; }
      else if (w != out->width || h != out->height) goto cleanup;
   }

   ok_so_far = 1;

cleanup:
   upscale_remove_tree(in_dir);
   upscale_remove_tree(out_dir);
   if (!ok_so_far)
   {
      compose_render_free(out);
      return 0;
   }
   return 1;
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
   else if (try_remote_upscale(&result, scale, &scaled))
   {
      /* Remote ML pipeline (Real-ESRGAN by default) succeeded. Same
       * service raw export uses; produces much higher quality output
       * than local nearest-neighbour, especially for the antialiased
       * boss portraits. */
      to_write = &scaled;
   }
   else
   {
      /* Remote upscale not configured or failed -- fall back to
       * pixel-perfect integer NN. */
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
         compose_scale_nn_rgba(result.frames[i],
                               result.width, result.height,
                               scaled.frames[i], scale);
      }
      to_write = &scaled;
   }

   ok = compose_apng_write(to_write, cof_name, output_path);
   compose_render_free(&result);
   compose_render_free(&scaled);  /* harmless if zeroed */
   return ok;
}
