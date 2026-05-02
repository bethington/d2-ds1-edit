#include "core/compose_scale.h"

int compose_scale_nn_rgba(const unsigned char *src, int src_w, int src_h,
                          unsigned char *dst, int scale)
{
   int dst_w;
   int sy, sx, oy, ox;

   if (src == NULL || dst == NULL) return 0;
   if (src_w <= 0 || src_h <= 0)   return 0;
   if (scale < 1)                  return 0;

   dst_w = src_w * scale;
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
   return 1;
}
