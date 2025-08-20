#include "structs.h"
#include "misc.h"
#include "dc6info.h"


// ==========================================================================
void dc6_decomp_norm(void * src, BITMAP * dst, long size, int x0, int y0)
{
   UBYTE * ptr = (UBYTE *) src;
   long  i;
   int   i2, x=x0, y=y0, c, c2;
   

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
            putpixel(dst, x, y, c2);
            x++;
         }
      }
   }
}


// ==========================================================================
// cmap must point to a table of 256 bytes
void dc6_decomp_cmap(void * src, BITMAP * dst, long size, int x0, int y0,
                     UBYTE * cmap)
{
   UBYTE * ptr = (UBYTE *) src;
   long  i;
   int   i2, x=x0, y=y0, c, c2;
   

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
            putpixel(dst, x, y, cmap[c2]);
            x++;
         }
      }
   }
}


// ==========================================================================
int anim_load_dc6(char * name, COF_S * cof, int lay_idx, long user_dir,
                  UBYTE * palshift)
{
   LAY_INF_S  * lay;
   int        entry, i, size, w, h, x1, y1, x2, y2;
   char       * buff;
   long       dir = 0, * lptr, offset, len;
   long       dc6_ver, dc6_unk1, dc6_unk2, dc6_dir, dc6_fpd, * dc6_fptr,
              f_w, f_h, f_offx, f_offy, f_x1, f_x2, f_y1, f_y2,
              f_len;
   UBYTE      * f_data;
   

   if ((cof == NULL) || (lay_idx >= COMPOSIT_NB))
      return 1;
   lay = & cof->lay_inf[lay_idx];

   // load dc6 file
   entry = misc_load_mpq_file(name, & buff, & len, FALSE);
   if (entry == -1)
      return 1;

   // decode dc6 header datas
   lptr     = (long *) buff;
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
   x1 = y1 = 0x7FFFFFFFL;
   x2 = y2 = 0x80000000L;
   for (i=0; i < dc6_fpd; i++)
   {
      // get pointer to frame header
      offset = dc6_fptr[dir * dc6_fpd + i];
      if (offset >= len)
      {
         free(buff);
         return 1;
      }
      lptr = (long *) (buff + offset);

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
   size = dc6_fpd * sizeof(BITMAP *);
   lay->bmp_num = dc6_fpd;
   lay->bmp = (BITMAP **) malloc(size);
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
            destroy_bitmap(lay->bmp[i]);
         }
         free(buff);
         return 1;
      }
      lptr = (long *) (buff + offset);

      // get frame datas
      f_w    = lptr[1];
      f_h    = lptr[2];
      f_offx = lptr[3];
      f_offy = lptr[4];
      f_len  = lptr[7];
      f_data = (UBYTE *) (& lptr[8]);

      // make a BITMAP
      lay->bmp[i] = create_bitmap(w, h);
      if (lay->bmp[i] == NULL)
      {
         while (i)
         {
            i--;
            destroy_bitmap(lay->bmp[i]);
         }
         free(buff);
         return 1;
      }
      clear(lay->bmp[i]);

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
   free(buff);
   return 0;
}
