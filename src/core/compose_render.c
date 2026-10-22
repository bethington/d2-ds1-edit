#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "core/dcc.h"
#include "core/dc6.h"
#include "core/index_frame.h"
#include "core/compose_bbox.h"
#include "core/compose_blit.h"
#include "core/compose_cof.h"
#include "core/compose_cof_path.h"
#include "core/compose_dcc_path.h"
#include "core/compose_render.h"

extern RGBA_PALETTE *a5_current_palette;

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

/* Convert one palette-index canvas to RGBA through the current palette.
 * Index 0 is transparent. Without a palette, indices become a grey ramp. */
static void index_canvas_to_rgba(const unsigned char *idx, int w, int h,
                                 unsigned char *rgba)
{
   size_t i, n = (size_t) w * (size_t) h;

   for (i = 0; i < n; i++)
   {
      unsigned char v = idx[i];
      unsigned char *p = rgba + i * 4;

      if (v == 0)
      {
         p[0] = p[1] = p[2] = p[3] = 0;
      }
      else if (a5_current_palette != NULL)
      {
         p[0] = a5_current_palette->colors[v].r;
         p[1] = a5_current_palette->colors[v].g;
         p[2] = a5_current_palette->colors[v].b;
         p[3] = a5_current_palette->colors[v].a;
      }
      else
      {
         p[0] = p[1] = p[2] = v;
         p[3] = 255;
      }
   }
}

/* Fill `lay` from `count` index canvases of w x h. Returns 1 on success. */
static int layer_from_index_canvases(LAYER_DATA_S *lay,
                                     unsigned char *const *canvases,
                                     int count, int w, int h,
                                     int off_x, int off_y)
{
   int f;

   if (w <= 0 || h <= 0 || count <= 0)
      return 0;

   lay->frames = (unsigned char **) calloc((size_t) count,
                                            sizeof(unsigned char *));
   if (lay->frames == NULL)
      return 0;
   lay->frame_count = count;
   lay->loaded = 1;   /* so layer_data_free releases a partial fill */

   for (f = 0; f < count; f++)
   {
      lay->frames[f] = (unsigned char *) malloc((size_t) w * h * 4);
      if (lay->frames[f] == NULL)
      {
         layer_data_free(lay);
         return 0;
      }
      index_canvas_to_rgba(canvases[f], w, h, lay->frames[f]);
   }

   lay->width  = w;
   lay->height = h;
   lay->off_x  = off_x;
   lay->off_y  = off_y;
   return 1;
}

/* Load one layer's DC6 for the given direction. Returns 1 on success,
 * 0 on failure (file missing, decode error, allocation). The frames are
 * decoded by the shared raw decoder and laid out on the union of their
 * rectangles, exactly as the editor and the asset export do. */
static int load_dc6_layer_for_direction(const char *dc6_path,
                                        int direction,
                                        LAYER_DATA_S *lay)
{
   char *buf = NULL;
   long buf_len = 0;
   DS1EDIT_INDEX_FRAME *frames = NULL;
   size_t count = 0;
   unsigned char **canvases = NULL;
   int w, h, off_x, off_y, ok;

   if (lay == NULL) return 0;
   memset(lay, 0, sizeof(*lay));

   if (misc_load_mpq_file((char *) dc6_path, &buf, &buf_len, 0) == -1)
      return 0;
   if (buf == NULL || buf_len <= 0) { if (buf) free(buf); return 0; }

   ok = dc6_decode_indices(buf, (size_t) buf_len, direction,
                           &frames, &count, NULL, 0);
   free(buf);
   if (!ok)
      return 0;

   ok = (ds1edit_index_frames_to_canvases(frames, count, &canvases, &w, &h,
                                &off_x, &off_y) == 0)
        && layer_from_index_canvases(lay, canvases, (int) count, w, h,
                                     off_x, off_y);

   ds1edit_index_canvases_free(canvases, count);
   ds1edit_index_frames_free(frames, count);
   if (!ok)
      memset(lay, 0, sizeof(*lay));
   return ok;
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
   DS1EDIT_INDEX_FRAME *frames = NULL;
   size_t count = 0, f;
   unsigned char **canvases;
   int ok;

   if (lay == NULL)
      return 0;
   memset(lay, 0, sizeof(*lay));

   if (misc_load_mpq_file((char *) dcc_path, &dcc_buf, &dcc_len, 0) == -1)
      return 0;
   if (dcc_buf == NULL || dcc_len <= 0) { if (dcc_buf) free(dcc_buf); return 0; }

   ok = dcc_decode_indices(dcc_buf, (size_t) dcc_len, direction,
                           &frames, &count, NULL, 0);
   free(dcc_buf);
   if (!ok || count == 0)
   {
      ds1edit_index_frames_free(frames, count);
      return 0;
   }

   /* Every DCC frame of a direction already has the direction box size. */
   canvases = (unsigned char **) calloc(count, sizeof(unsigned char *));
   ok = (canvases != NULL);
   for (f = 0; ok && f < count; f++)
      canvases[f] = frames[f].pixels;
   if (ok)
      ok = layer_from_index_canvases(lay, canvases, (int) count,
                                     frames[0].width, frames[0].height,
                                     frames[0].origin_x, frames[0].origin_y);

   free(canvases);
   ds1edit_index_frames_free(frames, count);
   if (!ok)
      memset(lay, 0, sizeof(*lay));
   return ok;
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

   /* 2. For each composit slot 0..15, attempt to load its sprite for
    *    the requested direction. Slots without a sprite file (the
    *    common case for many mode/weapon combos) silently skip.
    *
    *    Layer wclass: when the COF declares an empty per-layer
    *    weapon_class, fall back to the COF filename's wclass
    *    (params->wclass). The actual file on disk uses the latter --
    *    e.g. Andariel's COF says layer wclass="" but the file is
    *    ANTRLITNUHTH.dcc with HTH from the COF filename.
    *
    *    DCC -> DC6 fallback: some bosses (Mephisto, ...) ship their
    *    layers as DC6 instead of DCC. The working anim_load_cof in
    *    core/cof.c does the same fallback. */
   for (slot = 0; slot < COMPOSE_COF_MAX_LAYERS; slot++)
   {
      const char *layer_skin = params->skin;
      const char *layer_wclass;
      char dc6_path[COMPOSE_PATH_BUF];
      int loaded;

      if (slot < COMPOSE_RENDER_LAYER_COUNT
          && params->skin_per_layer[slot][0] != 0)
         layer_skin = params->skin_per_layer[slot];

      layer_wclass = cof.layers[slot].weapon_class;
      if (layer_wclass == NULL || layer_wclass[0] == 0)
         layer_wclass = params->wclass != NULL ? params->wclass : "";

      if (!compose_dcc_path_build(dcc_path, sizeof(dcc_path),
                                  params->base, params->token, slot,
                                  layer_skin, params->mode,
                                  layer_wclass))
         continue;

      loaded = load_layer_for_direction(dcc_path, params->direction,
                                        &layers[slot]);
      if (!loaded)
      {
         /* Try .dc6 sibling. Some bosses (Mephisto, ...) ship their
          * layers as DC6. Replace the trailing 'c' of .dcc with '6'
          * so we don't recompute the whole path. */
         size_t n = strlen(dcc_path);
         if (n >= 4 && (dcc_path[n-3]=='d'||dcc_path[n-3]=='D')
             && (dcc_path[n-2]=='c'||dcc_path[n-2]=='C')
             && (dcc_path[n-1]=='c'||dcc_path[n-1]=='C'))
         {
            memcpy(dc6_path, dcc_path, n + 1);
            dc6_path[n-1] = '6';
            loaded = load_dc6_layer_for_direction(dc6_path, params->direction,
                                                  &layers[slot]);
         }
      }

      if (loaded)
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
