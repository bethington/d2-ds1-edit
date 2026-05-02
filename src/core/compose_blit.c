#include <stddef.h>
#include <string.h>

#include "core/compose_blit.h"

void compose_blit_rgba(unsigned char *dst, int dst_w, int dst_h,
                       const unsigned char *src, int src_w, int src_h,
                       int dst_x, int dst_y)
{
   int sy0, sy1, sx0, sx1;
   int y;

   if (dst == NULL || src == NULL)
      return;
   if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0)
      return;

   /* Clip source rectangle to destination bounds. */
   sy0 = 0;
   sy1 = src_h;
   sx0 = 0;
   sx1 = src_w;

   if (dst_y < 0)            sy0 = -dst_y;
   if (dst_y + sy1 > dst_h)  sy1 = dst_h - dst_y;
   if (dst_x < 0)            sx0 = -dst_x;
   if (dst_x + sx1 > dst_w)  sx1 = dst_w - dst_x;

   if (sy0 >= sy1 || sx0 >= sx1)
      return;

   for (y = sy0; y < sy1; y++)
   {
      const unsigned char *src_row = src + (y * src_w + sx0) * 4;
      unsigned char *dst_row = dst + ((dst_y + y) * dst_w + (dst_x + sx0)) * 4;
      int x;
      for (x = sx0; x < sx1; x++)
      {
         unsigned char alpha = src_row[3];
         if (alpha > 0)
            memcpy(dst_row, src_row, 4);
         src_row += 4;
         dst_row += 4;
      }
   }
}
