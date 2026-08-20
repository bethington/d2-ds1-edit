/* Derived from win_ds1edit by Paul Siramy.
 * See NOTICE at the repository root for attribution and license status. */

#include "structs.h"
#include "core/dt1_draw.h"
#include "ui/compat.h"


// ==========================================================================
// Sub-tile drawing, used during DT1 tile loading to render sub-tiles into a
// temporary bitmap. The decode_sub_tile_* and index_buf_scale_down functions
// in dt1_decode.c provide the Allegro-independent, unit-testable equivalents.
//
// These write through al_put_pixel to the *current target bitmap*. The caller
// is responsible for selecting the target and, for any reasonable speed,
// holding an ALLEGRO_LOCK_WRITEONLY lock across the whole block -- see
// dt1_all_zoom_make. They used to switch the target per pixel via
// a5_putpixel, which cost ~350x what walking the same pixels into a plain
// array does.

void draw_sub_tile_isometric (ALLEGRO_BITMAP * dst, int x0, int y0, UBYTE * data, int length)
{
   (void) dst; /* written through the current target; see note above */
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
         al_put_pixel(x0+x, y0+y, pal_color(* ptr));
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
   (void) dst; /* written through the current target; see note above */
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
            al_put_pixel(x0+x, y0+y, pal_color(* ptr));
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
