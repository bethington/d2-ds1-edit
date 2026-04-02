/*
 * DT1 sub-tile decoding functions (Allegro-independent).
 * These operate on raw unsigned char index buffers and can be unit tested
 * without linking Allegro.
 */

#include <string.h>
#include "dt1_draw.h"


/* ========================================================================== */
/* helper : safely set a pixel in a raw index buffer */
static void buf_putpixel(unsigned char *buf, int buf_w, int buf_h, int x, int y, unsigned char val)
{
   if (x >= 0 && x < buf_w && y >= 0 && y < buf_h)
      buf[y * buf_w + x] = val;
}


/* ========================================================================== */
void decode_sub_tile_isometric(unsigned char *buf, int buf_w, int buf_h,
                               int x0, int y0,
                               const unsigned char *data, int length)
{
   const unsigned char * ptr = data;
   int   x, y=0, n,
         xjump[15] = {14, 12, 10, 8, 6, 4, 2, 0, 2, 4, 6, 8, 10, 12, 14},
         nbpix[15] = {4, 8, 12, 16, 20, 24, 28, 32, 28, 24, 20, 16, 12, 8, 4};

   if (length != 256)
      return;

   while (length > 0)
   {
      x = xjump[y];
      n = nbpix[y];
      length -= n;
      while (n)
      {
         buf_putpixel(buf, buf_w, buf_h, x0+x, y0+y, *ptr);
         ptr++;
         x++;
         n--;
      }
      y++;
   }
}


/* ========================================================================== */
void decode_sub_tile_normal(unsigned char *buf, int buf_w, int buf_h,
                            int x0, int y0,
                            const unsigned char *data, int length)
{
   const unsigned char * ptr = data;
   unsigned char b1, b2;
   int   x=0, y=0;

   while (length > 0)
   {
      b1 = * ptr;
      b2 = * (ptr + 1);
      ptr += 2;
      length -= 2;
      if (b1 || b2)
      {
         x += b1;
         length -= b2;
         while (b2)
         {
            buf_putpixel(buf, buf_w, buf_h, x0+x, y0+y, *ptr);
            ptr++;
            x++;
            b2--;
         }
      }
      else
      {
         x = 0;
         y++;
      }
   }
}


/* ========================================================================== */
void index_buf_scale_down(const unsigned char *src, int src_w, int src_h,
                          unsigned char *dst, int dst_w, int dst_h)
{
   int x, y, sx, sy;

   for (y = 0; y < dst_h; y++)
   {
      sy = y * src_h / dst_h;
      if (sy >= src_h) sy = src_h - 1;

      for (x = 0; x < dst_w; x++)
      {
         sx = x * src_w / dst_w;
         if (sx >= src_w) sx = src_w - 1;

         dst[y * dst_w + x] = src[sy * src_w + sx];
      }
   }
}
