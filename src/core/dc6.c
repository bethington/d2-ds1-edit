/* Derived from win_ds1edit by Paul Siramy (originally dc6info.c).
 * See NOTICE at the repository root for attribution and license status. */

#include "structs.h"
#include <limits.h>
#include <stdint.h>
#include "misc.h"
#include "core/dc6.h"
#include "core/cof.h"


// ==========================================================================
// Select `dst` and hold a write lock across a whole frame decode.
//
// These decoders used a5_putpixel, which switches the target bitmap twice per
// pixel. Against a memory bitmap that is merely wasteful; against a video
// bitmap -- which is what al_create_bitmap hands out once a display exists --
// every pixel takes its own texture lock and a frame costs hundreds of
// milliseconds. dt1_all_zoom_make() measured the same mistake at ~280 ms per
// block versus ~0.4 ms locked, so do here what it does there.
//
// Re-entrant by design: a caller that already holds a lock on `dst` keeps it,
// and we neither take nor release a second one.
typedef struct DC6_PIXEL_TARGET_S
{
   ALLEGRO_BITMAP * prev_target;
   int              locked_here;
} DC6_PIXEL_TARGET_S;

static void dc6_pixel_target_begin(DC6_PIXEL_TARGET_S * pt, ALLEGRO_BITMAP * dst)
{
   pt->prev_target = al_get_target_bitmap();
   pt->locked_here = 0;

   al_set_target_bitmap(dst);

   if (!al_is_bitmap_locked(dst))
      pt->locked_here = (al_lock_bitmap(dst, ALLEGRO_PIXEL_FORMAT_ANY,
                                        ALLEGRO_LOCK_READWRITE) != NULL);
}

static void dc6_pixel_target_end(DC6_PIXEL_TARGET_S * pt, ALLEGRO_BITMAP * dst)
{
   if (pt->locked_here)
      al_unlock_bitmap(dst);

   al_set_target_bitmap(pt->prev_target);
}


// ==========================================================================
void dc6_decomp_norm(void * src, ALLEGRO_BITMAP * dst, long size, int x0, int y0)
{
   UBYTE * ptr = (UBYTE *) src;
   long  i;
   int   i2, x=x0, y=y0, c, c2;
   DC6_PIXEL_TARGET_S pt;

   if (dst == NULL)
      return;

   dc6_pixel_target_begin(& pt, dst);

   for (i=0; i<size; i++)
   {
      c = * (ptr ++);

      if (c == 0x80)
      {
         x = x0;
         y--;
      }
      else if (c & 0x80)
         x += c & 0x7F;
      else
      {
         for (i2=0; i2<c; i2++)
         {
            c2 = * (ptr ++);
            i++;
            al_put_pixel(x, y, pal_color(c2));
            x++;
         }
      }
   }

   dc6_pixel_target_end(& pt, dst);
}


// ==========================================================================
// cmap must point to a table of 256 bytes
void dc6_decomp_cmap(void * src, ALLEGRO_BITMAP * dst, long size, int x0, int y0,
                     UBYTE * cmap)
{
   UBYTE * ptr = (UBYTE *) src;
   long  i;
   int   i2, x=x0, y=y0, c, c2;
   DC6_PIXEL_TARGET_S pt;

   if (dst == NULL)
      return;

   dc6_pixel_target_begin(& pt, dst);

   for (i=0; i<size; i++)
   {
      c = * (ptr ++);

      if (c == 0x80)
      {
         x = x0;
         y--;
      }
      else if (c & 0x80)
         x += c & 0x7F;
      else
      {
         for (i2=0; i2<c; i2++)
         {
            c2 = * (ptr ++);
            i++;
            al_put_pixel(x, y, pal_color(cmap[c2]));
            x++;
         }
      }
   }

   dc6_pixel_target_end(& pt, dst);
}


// ==========================================================================
int anim_load_dc6(char * name, COF_S * cof, int lay_idx, long user_dir,
                  UBYTE * palshift)
{
   LAY_INF_S  * lay;
   int        entry, i, size, w, h, x1, y1, x2, y2;
   char       * buff;
   long       dir = 0, offset, len;
   long       dc6_ver, dc6_unk1, dc6_unk2, dc6_dir, dc6_fpd,
              f_w, f_h, f_offx, f_offy, f_x1, f_x2, f_y1, f_y2,
              f_len;
   /* DC6 stores 32-bit fields; `long` is 64-bit under LP64, which would
      make lptr[n] straddle two of them. */
   const int32_t * lptr;
   const int32_t * dc6_fptr;
   UBYTE      * f_data;
   

   if ((cof == NULL) || (lay_idx >= COMPOSIT_NB))
      return 1;
   lay = & cof->lay_inf[lay_idx];

   // load dc6 file
   entry = misc_load_mpq_file(name, & buff, & len, FALSE);
   if (entry == -1)
      return 1;

   // decode dc6 header datas
   lptr     = (const int32_t *) buff;
   dc6_ver  = lptr[0];
   dc6_unk1 = lptr[1];
   dc6_unk2 = lptr[2];
   dc6_dir  = lptr[4];
   dc6_fpd  = lptr[5];
   dc6_fptr = & lptr[6];
   if ((dc6_ver != 6) || (dc6_unk1 != 1) || (dc6_unk2 != 0))
   {
      free(buff);
      return 1;
   }

   // valid direction ?
   if (dc6_dir != cof->dir)
   {
      free(buff);
      return 1;
   }
   
   // choose direction
   switch (cof->dir)
   {
      case  1 : dir = glb_ds1edit.new_dir1[user_dir];  break;
      case  4 : dir = glb_ds1edit.new_dir4[user_dir];  break;
      case  8 : dir = glb_ds1edit.new_dir8[user_dir];  break;
      case 16 : dir = glb_ds1edit.new_dir16[user_dir]; break;
      case 32 : dir = glb_ds1edit.new_dir32[user_dir]; break;
   }

   // decode dc6 direction

   // search the direction "box"
   x1 = y1 = LONG_MAX;
   x2 = y2 = LONG_MIN;
   for (i=0; i < dc6_fpd; i++)
   {
      // get pointer to frame header
      offset = dc6_fptr[dir * dc6_fpd + i];
      if (offset >= len)
      {
         free(buff);
         return 1;
      }
      lptr = (const int32_t *) (buff + offset);

      // update the box limits
      f_w    = lptr[1];
      f_h    = lptr[2];
      f_offx = lptr[3];
      f_offy = lptr[4];

      f_x1 = f_offx;
      f_x2 = f_x1 + f_w - 1;
      f_y2 = f_offy;
      f_y1 = f_y2 - f_h + 1;
      
      if (f_x1 < x1)
         x1 = f_x1;
      if (f_x2 > x2)
         x2 = f_x2;
      if (f_y1 < y1)
         y1 = f_y1;
      if (f_y2 > y2)
         y2 = f_y2;
   }
   w = x2 - x1 + 1;
   h = y2 - y1 + 1;

   if ((w <= 0) || (h <= 0))
   {
      free(buff);
      return 1;
   }
      
   lay->off_x = x1;
   lay->off_y = y1;
   
   // allocate the bitmaps
   size = dc6_fpd * sizeof(ALLEGRO_BITMAP *);
   lay->bmp_num = dc6_fpd;
   lay->bmp = (ALLEGRO_BITMAP **) malloc(size);
   if (lay->bmp == NULL)
   {
      free(buff);
      return 1;
   }
   memset(lay->bmp, 0, size);

   // make the bitmaps
   for (i=0; i < dc6_fpd; i++)
   {
      // get pointer to frame header
      offset = dc6_fptr[dir * dc6_fpd + i];
      if (offset >= len)
      {
         while (i)
         {
            i--;
            al_destroy_bitmap(lay->bmp[i]);
         }
         free(buff);
         return 1;
      }
      lptr = (const int32_t *) (buff + offset);

      // get frame datas
      f_w    = lptr[1];
      f_h    = lptr[2];
      f_offx = lptr[3];
      f_offy = lptr[4];
      f_len  = lptr[7];
      f_data = (UBYTE *) (& lptr[8]);

      // make a BITMAP
      lay->bmp[i] = al_create_bitmap(w, h);
      if (lay->bmp[i] == NULL)
      {
         while (i)
         {
            i--;
            al_destroy_bitmap(lay->bmp[i]);
         }
         free(buff);
         return 1;
      }
      a5_clear(lay->bmp[i]);

      // decode dc6 datas
      if (palshift == NULL)
      {
         dc6_decomp_norm(
            f_data,              // src
            lay->bmp[i],         // dst
            f_len,               // length
            f_offx - x1,         // x0
            h - 1 + f_offy - y2  // y0
         );
      }
      else
      {
         dc6_decomp_cmap(
            f_data,              // src
            lay->bmp[i],         // dst
            f_len,               // length
            f_offx - x1,         // x0
            h - 1 + f_offy - y2, // y0
            palshift             // cmap
         );
      }
   }

   // build CACHED_TILE array from the BITMAP frames
   anim_build_layer_cache(lay);

   free(buff);
   return 0;
}
