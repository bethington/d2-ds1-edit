#include "structs.h"
#include "dt1_draw.h"


// ==========================================================================
// Legacy functions: draw to Allegro 4 BITMAP via putpixel (used by rendering
// code until it migrates to Allegro 5 in Phase 4).
// The new decode_sub_tile_* and index_buf_scale_down functions are in
// dt1_decode.c (Allegro-independent, unit-testable).

void draw_sub_tile_isometric (ALLEGRO_BITMAP * dst, int x0, int y0, UBYTE * data, int length)
{
   UBYTE * ptr = data;
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
         a5_putpixel(dst, x0+x, y0+y, * ptr);
         ptr++;
         x++;
         n--;
      }
      y++;
   }
}

void draw_sub_tile_normal (ALLEGRO_BITMAP * dst, int x0, int y0, UBYTE * data,
                           int length)
{
   UBYTE * ptr = data, b1, b2;
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
            a5_putpixel(dst, x0+x, y0+y, * ptr);
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
