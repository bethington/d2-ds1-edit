#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <allegro5/allegro.h>

#include "structs.h"
#include "core/dcc.h"
#include "core/compose_bbox.h"
#include "core/compose_blit.h"
#include "core/compose_cof.h"
#include "core/compose_cof_path.h"
#include "core/compose_dcc_path.h"
#include "core/compose_render.h"

extern int misc_load_mpq_file(char *filename, char **buffer, long *buf_len, int output);

#define COMPOSE_PATH_BUF 512

typedef struct LAYER_DATA_S
{
   int loaded;
   int width;
   int height;
   int off_x;
   int off_y;
   int frame_count;
   unsigned char **frames;  /* each is width*height*4 bytes RGBA */
} LAYER_DATA_S;

static void layer_data_free(LAYER_DATA_S *lay)
{
   int i;
   if (lay == NULL || !lay->loaded)
      return;
   if (lay->frames != NULL)
   {
      for (i = 0; i < lay->frame_count; i++)
         free(lay->frames[i]);
      free(lay->frames);
   }
   memset(lay, 0, sizeof(*lay));
}

static int copy_bitmap_to_rgba(ALLEGRO_BITMAP *bmp, int w, int h,
                               unsigned char *out_buf)
{
   ALLEGRO_LOCKED_REGION *lock;
   int y;

   if (bmp == NULL || out_buf == NULL || w <= 0 || h <= 0)
      return 0;

   lock = al_lock_bitmap(bmp,
                         ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
                         ALLEGRO_LOCK_READONLY);
   if (lock == NULL)
      return 0;

   for (y = 0; y < h; y++)
   {
      unsigned char *src_row = (unsigned char *) lock->data + y * lock->pitch;
      unsigned char *dst_row = out_buf + (size_t) y * w * 4;
      memcpy(dst_row, src_row, (size_t) w * 4);
   }

   al_unlock_bitmap(bmp);
   return 1;
}

/* Load one layer's DCC for the given direction. On success, populates
 * lay with the per-frame RGBA buffers + per-direction box. On failure
 * (file missing, decode error, allocation), returns 0 and lay stays
 * marked unloaded. Failures here are EXPECTED for many (slot, mode,
 * weapon) combos -- callers silently skip. */
static int load_layer_for_direction(const char *dcc_path,
                                    int direction,
                                    LAYER_DATA_S *lay)
{
   char *dcc_buf = NULL;
   long dcc_len = 0;
   DCC_S *dcc = NULL;
   DCC_DIRECTION_S *dir;
   int w, h;
   int frames;
   int f;

   if (lay == NULL)
      return 0;
   memset(lay, 0, sizeof(*lay));

   if (misc_load_mpq_file((char *) dcc_path, &dcc_buf, &dcc_len, 0) == -1)
      return 0;

   dcc = dcc_mem_load(dcc_buf, (int) dcc_len);
   free(dcc_buf);
   if (dcc == NULL)
      return 0;

   /* dcc_mem_load only copies bytes; the header is parsed inside
    * dcc_decode. Decode FIRST, then validate the direction. */
   if (dcc_decode(dcc, 1L << direction))
   {
      dcc_destroy(dcc);
      return 0;
   }

   if (direction < 0 || direction >= dcc->header.directions)
   {
      dcc_destroy(dcc);
      return 0;
   }

   dir = &dcc->direction[direction];
   w = (int) dir->box.width;
   h = (int) dir->box.height;
   frames = (int) dcc->header.frames_per_dir;

   if (w <= 0 || h <= 0 || frames <= 0)
   {
      dcc_destroy(dcc);
      return 0;
   }

   lay->frames = (unsigned char **) calloc((size_t) frames,
                                            sizeof(unsigned char *));
   if (lay->frames == NULL)
   {
      dcc_destroy(dcc);
      return 0;
   }

   for (f = 0; f < frames; f++)
   {
      lay->frames[f] = (unsigned char *) calloc((size_t) w * h * 4, 1);
      if (lay->frames[f] == NULL)
      {
         /* On alloc failure mid-way, free what we have and bail. */
         while (f > 0)
            free(lay->frames[--f]);
         free(lay->frames);
         dcc_destroy(dcc);
         memset(lay, 0, sizeof(*lay));
         return 0;
      }
      copy_bitmap_to_rgba(dcc->frame[direction][f].bmp, w, h, lay->frames[f]);
   }

   lay->loaded = 1;
   lay->width = w;
   lay->height = h;
   lay->off_x = (int) dir->box.xmin;
   lay->off_y = (int) dir->box.ymin;
   lay->frame_count = frames;

   dcc_destroy(dcc);
   return 1;
}

void compose_render_free(COMPOSE_RENDER_RESULT_S *result)
{
   int i;
   if (result == NULL)
      return;
   if (result->frames != NULL)
   {
      for (i = 0; i < result->frame_count; i++)
         free(result->frames[i]);
      free(result->frames);
   }
   memset(result, 0, sizeof(*result));
}

int compose_render(const COMPOSE_RENDER_PARAMS_S *params,
                   COMPOSE_RENDER_RESULT_S *out)
{
   char cof_path[COMPOSE_PATH_BUF];
   char dcc_path[COMPOSE_PATH_BUF];
   char *cof_buf = NULL;
   long cof_len = 0;
   COMPOSE_COF_S cof;
   LAYER_DATA_S layers[COMPOSE_COF_MAX_LAYERS];
   COMPOSE_LAYER_RECT_S rects[COMPOSE_COF_MAX_LAYERS];
   int blit_x[COMPOSE_COF_MAX_LAYERS];
   int blit_y[COMPOSE_COF_MAX_LAYERS];
   int canvas_w, canvas_h;
   int slot, f, order;
   int loaded_any = 0;

   if (params == NULL || out == NULL)
      return 0;
   memset(out, 0, sizeof(*out));
   memset(&cof, 0, sizeof(cof));
   memset(layers, 0, sizeof(layers));
   memset(rects, 0, sizeof(rects));

   /* 1. Load + parse the COF. */
   if (!compose_cof_path_build(cof_path, sizeof(cof_path),
                               params->base, params->token,
                               params->mode, params->wclass))
      return 0;

   if (misc_load_mpq_file(cof_path, &cof_buf, &cof_len, 0) == -1)
      return 0;

   if (!compose_cof_parse(cof_buf, cof_len, &cof))
   {
      free(cof_buf);
      return 0;
   }
   free(cof_buf);

   if (params->direction < 0 || params->direction >= cof.direction_count)
   {
      compose_cof_free(&cof);
      return 0;
   }

   /* 2. For each composit slot 0..15, attempt to load its DCC for the
    *    requested direction. Slots without a DCC file (the common case
    *    for many mode/weapon combos) silently skip. */
   for (slot = 0; slot < COMPOSE_COF_MAX_LAYERS; slot++)
   {
      if (!compose_dcc_path_build(dcc_path, sizeof(dcc_path),
                                  params->base, params->token, slot,
                                  params->skin, params->mode,
                                  cof.layers[slot].weapon_class))
         continue;

      if (load_layer_for_direction(dcc_path, params->direction, &layers[slot]))
      {
         loaded_any = 1;
         rects[slot].offset_x = layers[slot].off_x;
         rects[slot].offset_y = layers[slot].off_y;
         rects[slot].width    = layers[slot].width;
         rects[slot].height   = layers[slot].height;
      }
      else
      {
         /* Mark zero-sized so compose_bbox_union skips this slot. */
         rects[slot].width  = 0;
         rects[slot].height = 0;
      }
   }

   if (!loaded_any)
   {
      compose_cof_free(&cof);
      return 0;
   }

   /* 3. Compute canvas bbox. */
   if (!compose_bbox_union(rects, COMPOSE_COF_MAX_LAYERS,
                           &canvas_w, &canvas_h, blit_x, blit_y)
       || canvas_w <= 0 || canvas_h <= 0)
   {
      for (slot = 0; slot < COMPOSE_COF_MAX_LAYERS; slot++)
         layer_data_free(&layers[slot]);
      compose_cof_free(&cof);
      return 0;
   }

   /* 4. Allocate output frames + composite each one. */
   {
      unsigned char **out_frames = (unsigned char **)
         calloc((size_t) cof.frames_per_dir, sizeof(unsigned char *));
      if (out_frames == NULL)
      {
         for (slot = 0; slot < COMPOSE_COF_MAX_LAYERS; slot++)
            layer_data_free(&layers[slot]);
         compose_cof_free(&cof);
         return 0;
      }

      for (f = 0; f < cof.frames_per_dir; f++)
      {
         out_frames[f] = (unsigned char *) calloc(
            (size_t) canvas_w * canvas_h * 4, 1);
         if (out_frames[f] == NULL)
         {
            int g;
            for (g = 0; g < f; g++)
               free(out_frames[g]);
            free(out_frames);
            for (slot = 0; slot < COMPOSE_COF_MAX_LAYERS; slot++)
               layer_data_free(&layers[slot]);
            compose_cof_free(&cof);
            return 0;
         }

         /* Composite layers in COF-priority order. */
         for (order = 0; order < cof.layer_count; order++)
         {
            int composit_idx = compose_cof_priority_at(
               &cof, params->direction, f, order);
            if (composit_idx < 0
                || composit_idx >= COMPOSE_COF_MAX_LAYERS)
               continue;
            if (!layers[composit_idx].loaded)
               continue;
            if (f >= layers[composit_idx].frame_count)
               continue;
            if (blit_x[composit_idx] < 0 || blit_y[composit_idx] < 0)
               continue;

            compose_blit_rgba(out_frames[f], canvas_w, canvas_h,
                              layers[composit_idx].frames[f],
                              layers[composit_idx].width,
                              layers[composit_idx].height,
                              blit_x[composit_idx],
                              blit_y[composit_idx]);
         }
      }

      out->width       = canvas_w;
      out->height      = canvas_h;
      out->frame_count = cof.frames_per_dir;
      out->frames      = out_frames;
   }

   /* 5. Free per-layer scratch. */
   for (slot = 0; slot < COMPOSE_COF_MAX_LAYERS; slot++)
      layer_data_free(&layers[slot]);

   compose_cof_free(&cof);
   return 1;
}
