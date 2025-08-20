#include "structs.h"
#include "gfx_custom.h"


// ==========================================================================
// draw a sprite at the zoom 1:div
// dst & sprite MUST be 8bpp color depth both
// dst & sprite MUST be linear bitmaps both (not plannar)
// if color_map point to NULL, regular sprite, else transparent one
void stretch_trans_sprite_8bpp(BITMAP * dst, BITMAP * sprite, int x0, int y0, int div)
{
   int dx1, dy1, dx2, dy2, sx1, sy1, dw, dh, sw, sh;


   // ridiculous cases
   if ((bitmap_color_depth(dst) != 8) || (bitmap_color_depth(sprite) != 8))
      return;
   if (( ! is_linear_bitmap(dst)) || ( ! is_linear_bitmap(sprite)))
      return;

   // clip

   dw = dst->w;
   dh = dst->h;
   if ((x0 >= dw) || (y0 >= dh))
      return;

   sw = sprite->w;
   sh = sprite->h;

   // start
   if (x0 < 0)
   {
      dx1 = 0;
      sx1 = (-x0) * div;
      if (sx1 >= sw)
         return;
   }
   else
   {
      dx1 = x0;
      sx1 = 0;
   }

   if (y0 < 0)
   {
      dy1 = 0;
      sy1 = (-y0) * div;
      if (sy1 >= sh)
         return;
   }
   else
   {
      dy1 = y0;
      sy1 = 0;
   }

   // end
   sw /= div;
   sh /= div;

   dx2 = dx1 + sw - 1 - (sx1 / div);
   if (dx2 >= dw)
      dx2 = dw - 1;

   dy2 = dy1 + sh - 1 - (sy1 / div);
   if (dy2 >= dh)
      dy2 = dh - 1;

   if ((dx2 < 0) || (dy2 < 0))
      return;

   // draw
   {
      int dx, dy, sx, sy;

      dx = dx1;
      dy = dy1;
      sx = sx1;
      sy = sy1;
      if (color_map == NULL)
      {
         // regular sprite
         for(;;)
         {
            if (sprite->line[sy][sx])
               dst->line[dy][dx] = sprite->line[sy][sx];
            dx++;
            if (dx > dx2)
            {
               dx = dx1;
               sx = sx1;
               dy++;
               if (dy > dy2)
                  return;
               else
                  sy += div;
            }
            else
               sx += div;
         }
      }
      else
      {
         // transparent sprite
         for(;;)
         {
            if (sprite->line[sy][sx])
            {
               dst->line[dy][dx] = color_map->data
                                       [ dst->line[dy][dx]    ]
                                       [ sprite->line[sy][sx] ];
            }
            dx++;
            if (dx > dx2)
            {
               dx = dx1;
               sx = sx1;
               dy++;
               if (dy > dy2)
                  return;
               else
                  sy += div;
            }
            else
               sx += div;
         }
      }
   }
}


// ==========================================================================
// draw the shadow projection of a sprite at the zoom 1:div
// dst & sprite MUST be 8bpp color depth both
// dst & sprite MUST be linear bitmaps both (not plannar)
// cmap must point on an array of 256 bytes
// offx and offy are the coordinates of the pivot point of the sprite
void stretch_trans_shadow_8bpp(BITMAP * dst, BITMAP * sprite, int x0, int y0, int div,
                               UBYTE * cmap, int offy)
{
   int dx1, dy1, dx2, dy2, sx1, sy1, sw, sh;


   // ridiculous cases
   if ((bitmap_color_depth(dst) != 8) || (bitmap_color_depth(sprite) != 8))
      return;
   if (( ! is_linear_bitmap(dst)) || ( ! is_linear_bitmap(sprite)))
      return;

   sx1 = 0;
   sy1 = 0;
   sw  = sprite->w / div;
   sh  = sprite->h / (div * 2);

   dy1 = y0 + ((offy - y0) / 2);
   dx1 = x0 - ((offy - y0) / 2);

   dx2 = dx1 + sw - 1;
   dy2 = dy1 + sh - 1;

   // draw
   {
      int dx, dy, sx, sy;

      dx = dx1;
      dy = dy1;
      sx = sx1;
      sy = sy1;
      for(;;)
      {
         if ((dx >= 0) && (dy >= 0) && (dx < dst->w) && (dy < dst->h))
         {
            if (sprite->line[sy][sx])
               dst->line[dy][dx] = cmap[ dst->line[dy][dx] ];
         }
         dx++;
         if (dx > dx2)
         {
            dx1++;
            dx2++;
            dx = dx1;
            sx = sx1;
            dy++;
            if (dy > dy2)
               return;
            else
               sy += div * 2;
         }
         else
            sx += div;
      }
   }
}
