#include <stddef.h>

#include "core/compose_bbox.h"

int compose_bbox_union(const COMPOSE_LAYER_RECT_S *rects, int rect_count,
                       int *canvas_w, int *canvas_h,
                       int *layer_blit_x, int *layer_blit_y)
{
   int i;
   int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
   int have_any = 0;

   if (rects == NULL || rect_count <= 0
       || canvas_w == NULL || canvas_h == NULL
       || layer_blit_x == NULL || layer_blit_y == NULL)
      return 0;

   for (i = 0; i < rect_count; i++)
   {
      int rx0, ry0, rx1, ry1;

      if (rects[i].width <= 0 || rects[i].height <= 0)
      {
         layer_blit_x[i] = -1;
         layer_blit_y[i] = -1;
         continue;
      }

      rx0 = rects[i].offset_x;
      ry0 = rects[i].offset_y;
      rx1 = rx0 + rects[i].width;
      ry1 = ry0 + rects[i].height;

      if (!have_any)
      {
         min_x = rx0; min_y = ry0;
         max_x = rx1; max_y = ry1;
         have_any = 1;
      }
      else
      {
         if (rx0 < min_x) min_x = rx0;
         if (ry0 < min_y) min_y = ry0;
         if (rx1 > max_x) max_x = rx1;
         if (ry1 > max_y) max_y = ry1;
      }
   }

   if (!have_any)
   {
      *canvas_w = 0;
      *canvas_h = 0;
      return 0;
   }

   *canvas_w = max_x - min_x;
   *canvas_h = max_y - min_y;

   /* Second pass: now that we know min_x/min_y, fill in the
    * canvas-relative blit positions for non-skipped layers. */
   for (i = 0; i < rect_count; i++)
   {
      if (rects[i].width <= 0 || rects[i].height <= 0)
         continue;
      layer_blit_x[i] = rects[i].offset_x - min_x;
      layer_blit_y[i] = rects[i].offset_y - min_y;
   }

   return 1;
}
