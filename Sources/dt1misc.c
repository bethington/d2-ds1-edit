#include <string.h>
#include "structs.h"
#include "error.h"
#include "dt1_draw.h"
#include "misc.h"
#include "mpq/MpqView.h"
#include "dt1misc.h"


// ==========================================================================
// check if this dt1 is already loaded (multiple ds1 can use the same dt1)
int dt1_already_loaded(char * dt1name, int * idx)
{
   int i;

   for (i=0; i<DT1_MAX; i++)
   {
      if (stricmp(glb_dt1[i].name, dt1name) == 0)
      {
         * idx = i;
         return TRUE;
      }
   }
   * idx = -1;
   return FALSE;
}


// ==========================================================================
// memory free of a dt1
int dt1_free(int i)
{
   BITMAP  * bmp_ptr;
   int     size = glb_dt1[i].buff_len + glb_dt1[i].bh_buff_len;
   int     z, b;


   if (strlen(glb_dt1[i].name) == 0)
      return 0;
      
fprintf(stderr, "      . %s\n", glb_dt1[i].name);
fflush(stderr);

   // bitmaps in all zoom format
   for (z=0; z<ZM_MAX; z++)
   {
      if (glb_dt1[i].block_zoom[z] != NULL)
      {
         for (b=0; b < glb_dt1[i].block_num; b++)
         {
            bmp_ptr = * (glb_dt1[i].block_zoom[z] + b);
            if (bmp_ptr != NULL)
            {
               size += bmp_ptr->w * bmp_ptr->h + sizeof(BITMAP);
               destroy_bitmap(bmp_ptr);
            }
         }
         size += glb_dt1[i].bz_size[z];
         free(glb_dt1[i].block_zoom[z]);
      }
   }

   // block headers of dt1
   if (glb_dt1[i].bh_buffer != NULL)
   {
      free(glb_dt1[i].bh_buffer);
   }

   // dt1 buffer
   if (glb_dt1[i].buffer != NULL)
   {
      free(glb_dt1[i].buffer);
   }
   
   // end
   memset(& glb_dt1[i], 0, sizeof(DT1_S));
   return size;
}


// ==========================================================================
// a ds1 don't need to use a dt1 anymore
// if the dt1 is still use by another ds1, no change (except the usage count)
// else, free it
int dt1_del(int i)
{
   if ( (i < 0) || (i >= DT1_MAX))
      return 2;

   if (glb_dt1[i].ds1_usage <= 0)
      return 1;

   glb_dt1[i].ds1_usage--;
   if (glb_dt1[i].ds1_usage == 0)
      dt1_free(i);
   return 0;
}


// ==========================================================================
// fill the block header data of a dt1 with the data from the file
void dt1_bh_update(int i)
{
   BLOCK_S * bh_ptr = glb_dt1[i].bh_buffer;
   UBYTE   * ptr;
   int     b, t, idxtable[25] = {20, 21, 22, 23, 24,
                                 15, 16, 17, 18, 19,
                                 10, 11, 12, 13, 14,
                                  5,  6,  7,  8,  9,
                                  0,  1,  2,  3,  4};

   ptr = (UBYTE *) glb_dt1[i].buffer + glb_dt1[i].bh_start;
   for (b=0; b < glb_dt1[i].block_num; b++)
   {
      bh_ptr->direction    = * (long *)  ptr;
      bh_ptr->roof_y       = * (WORD *)  (ptr +  4);
      bh_ptr->sound        = * (UBYTE *) (ptr +  6);
      bh_ptr->animated     = * (UBYTE *) (ptr +  7);
      bh_ptr->size_y       = * (long *)  (ptr +  8);
      bh_ptr->size_x       = * (long *)  (ptr + 12);
      // skip 4 bytes : zeros1
      bh_ptr->orientation  = * (long *)  (ptr + 20);
      bh_ptr->main_index   = * (long *)  (ptr + 24);
      bh_ptr->sub_index    = * (long *)  (ptr + 28);
      bh_ptr->rarity       = * (long *)  (ptr + 32);
      // skip 4 bytes : unknown_a thru unknown_d
      for (t=0; t<25; t++)
         bh_ptr->sub_tiles_flags[idxtable[t]] = * (UBYTE *) (ptr + 40 + t);
      // skip 7 bytes : zeros2
      bh_ptr->tiles_ptr    = * (long *)  (ptr + 72);
      bh_ptr->tiles_length = * (long *)  (ptr + 76);
      bh_ptr->tiles_number = * (long *)  (ptr + 80);
      // skip 12 bytes : zeros3

      // next block header
      bh_ptr++;
      ptr += 96;
   }
}


// ==========================================================================
// fill the sub-tile structure with the right sub-header data from the file
void dt1_fill_subt(SUB_TILE_S * ptr, int i, long tiles_ptr, int s)
{
   UBYTE * st_ptr;

   st_ptr = (UBYTE *) glb_dt1[i].buffer + tiles_ptr + (20 * s);
   ptr->x_pos       = * (WORD *)  st_ptr;
   ptr->y_pos       = * (WORD *)  (st_ptr +  2);
   // skip 2 bytes : unknown1
   ptr->x_grid      = * (UBYTE *) (st_ptr +  6);
   ptr->y_grid      = * (UBYTE *) (st_ptr +  7);
   ptr->format      = * (WORD *)  (st_ptr +  8);
   ptr->length      = * (long *)  (st_ptr + 10);
   // skip 2 bytes : unknown2
   ptr->data_offset = * (long *)  ((UBYTE *)st_ptr + 16);
}


// ==========================================================================
// make the bitmap of 1 tile, for 1 zoom
void dt1_zoom(BITMAP * src, int i, int b, int z)
{
   BITMAP * dst;
   int    w = src->w, h = src->h, d=1;
   char   tmp_str[100];

   switch(z)
   {
      case ZM_11  : break;
      case ZM_12  : d =  2; break;
      case ZM_14  : d =  4; break;
      case ZM_18  : d =  8; break;
      case ZM_116 : d = 16; break;
   }
   w /= d;
   h /= d;
   dst = create_bitmap(w, h);
   if (dst == NULL)
   {
      sprintf(tmp_str, "dt1_zoom(%i, %i, %i), can't make a bitmap "
         "of %i * %i pixels\n", i, b, z, w, h);
      ds1edit_error(tmp_str);
   }
   stretch_blit(src, dst, 0, 0, src->w, src->h, 0, 0, w, h);

   * (glb_dt1[i].block_zoom[z] + b) = dst;
}


// ==========================================================================
// make all bitmaps in all zoom of all tiles of 1 dt1
void dt1_all_zoom_make(int i)
{
   BLOCK_S       * b_ptr, * my_b_ptr; // pointers to current block header
   SUB_TILE_S    st_ptr;  // current sub-tile header
   BITMAP        * tmp_bmp, * sprite;
   int           b, w, h, s, x0, y0, length, y_add, z, mem_size;
   UBYTE         * data;
   WORD          format;
   long          orientation;
   char          tmp_str[100];
   int           t_mi, t_si, my_idx;

   b_ptr    = (BLOCK_S *) glb_dt1[i].bh_buffer;

   fprintf(stderr, "making zoom for glb_dt1[%i] (%s) ", i, glb_dt1[i].name);
   fflush(stderr);
   // get mem for table of pointers
   for (z=0; z<ZM_MAX; z++)
   {
      mem_size = sizeof(BITMAP *) * glb_dt1[i].block_num;
      glb_dt1[i].block_zoom[z] = (BITMAP **) malloc(mem_size);
      if (glb_dt1[i].block_zoom[z] == NULL)
      {
         sprintf(tmp_str, "dt1_all_zoom_make(%i), zoom %i, not enough mem "
            "for %i bytes\n", i, z, mem_size);
         ds1edit_error(tmp_str);
      }
      memset(glb_dt1[i].block_zoom[z], 0, mem_size);
      glb_dt1[i].bz_size[z] = mem_size;
   }

   // make the bitmaps
   for (b=0; b < glb_dt1[i].block_num; b++) // for each blocks of a dt1
   {
      // get infos
      orientation = b_ptr->orientation;

      // prepare tmp bitmap size
      w = b_ptr->size_x;
      if ((orientation == 10) || (orientation == 11))
         w = 160; // set it to 160 because we'll draw infos over it later
      h = - b_ptr->size_y;

      // adjustment (which y line in the bitmap is the zero line ?)

      // by default, when orientation > 15 : lower wall
      y_add = 96;
      if ((orientation == 0) || (orientation == 15)) // floor or roof
      {
         if (b_ptr->size_y)
         {
            b_ptr->size_y = - 80;
            h = 80;
            y_add = 0;
         }
      }
      else if (orientation < 15) // upper wall, shadow, special
      {
         if (b_ptr->size_y)
         {
            b_ptr->size_y += 32;
            h -= 32;
            y_add = h;
         }
      }

      // anti-bug (for empty block)
      if ((w == 0) || (h == 0))
      {
         fprintf(stderr, "0");
         fflush(stderr);
         b_ptr ++;
         continue;
      }
      
      // normal block (non-empty)
      tmp_bmp = create_bitmap(w, h);
      if (tmp_bmp == NULL)
      {
         sprintf(tmp_str, "dt1_all_zoom_make(%i), can't make a bitmap "
            "of %i * %i pixels\n", i, w, h);
         ds1edit_error(tmp_str);
      }
      clear(tmp_bmp);

      // draw sub-tiles in this bitmap
      for (s=0; s < b_ptr->tiles_number; s++) // for each sub-tiles
      {
         // get the sub-tile info
         dt1_fill_subt(& st_ptr, i, b_ptr->tiles_ptr, s);
         
         // get infos
         x0     = st_ptr.x_pos;
         y0     = y_add + st_ptr.y_pos;
         data   = (UBYTE *) ((UBYTE *)glb_dt1[i].buffer + b_ptr->tiles_ptr + st_ptr.data_offset);
         length = st_ptr.length;
         format = st_ptr.format;
         
         // draw the sub-tile
         if (format == 0x0001)
            draw_sub_tile_isometric(tmp_bmp, x0, y0, data, length);
         else
            draw_sub_tile_normal(tmp_bmp, x0, y0, data, length);
      }

      // if a game's special tile, draw my own info over it
      if ( (glb_ds1edit.cmd_line.no_vis_debug == FALSE) &&
           (i != 0) && ((orientation == 10) || (orientation == 11))
         )
      {
         // get info
         t_mi     = b_ptr->main_index;
         t_si     = b_ptr->sub_index;
         my_b_ptr = (BLOCK_S *) glb_dt1[0].bh_buffer;

         // search same info in my dt1
         for (my_idx=0; my_idx < glb_dt1[0].block_num; my_idx++)
         {
            if ( (my_b_ptr[my_idx].orientation == orientation) &&
                 (my_b_ptr[my_idx].main_index  == t_mi) &&
                 (my_b_ptr[my_idx].sub_index   == t_si)
               )
            {
               // found it, draw that tile over the game's gfx
               sprite = * (glb_dt1[0].block_zoom[ZM_11] + my_idx);
               if (sprite != NULL)
                  draw_sprite(tmp_bmp, sprite, 0, tmp_bmp->h - sprite->h);

               // stop the search
               my_idx = glb_dt1[0].block_num;
            }
         }
      }
      
      // make zoom from the bitmap, for each zoom
      for (z=0; z<ZM_MAX; z++)
         dt1_zoom(tmp_bmp, i, b, z);

      // destroy tmp bitmap
      destroy_bitmap(tmp_bmp);

      // next block header
      b_ptr ++;
      fprintf(stderr, ".");
      fflush(stderr);
   }
   fprintf(stderr, "\n");
   fflush(stderr);
}


// ==========================================================================
// fill / make all the datas of 1 dt1
void dt1_struct_update(int i)
{
   void * ptr = glb_dt1[i].buffer;
   int  size;
   char tmp[100];

   if (ptr == NULL)
     return;
   glb_dt1[i].x1        = * (long *) ptr;
   glb_dt1[i].x2        = * (long *) ((UBYTE *)ptr + 4);
   glb_dt1[i].block_num = * (long *) ((UBYTE *)ptr + 268);
   glb_dt1[i].bh_start  = * (long *) ((UBYTE *)ptr + 272);

   // debug
   printf("struct data of glb_dt1[%i] :\n", i);
   printf("   x1        = %li\n",    glb_dt1[i].x1);
   printf("   x2        = %li\n",    glb_dt1[i].x2);
   printf("   block_num = %li\n",    glb_dt1[i].block_num);
   printf("   bh_start  = 0x%0lX\n", glb_dt1[i].bh_start);
   
   // blocks
   size = sizeof(BLOCK_S) * glb_dt1[i].block_num;
   glb_dt1[i].bh_buffer = (void *) malloc(size);
   if (glb_dt1[i].bh_buffer == NULL)
   {
      sprintf(tmp, "dt1_struct_update(%i), not enough memory for %i bytes\n",
         i, size);
      ds1edit_error(tmp);
   }
   glb_dt1[i].bh_buff_len = size;
   dt1_bh_update(i);
   dt1_all_zoom_make(i);
}


// ==========================================================================
// ask to load a dt1 from a mpq
// if the dt1 was already loaded by another ds1, just use the same dt1
//    (and update the usage counter)
// else load it
int dt1_add(char * dt1name)
{
   int  i, idx, entry;
   char tmp[256];

   if (dt1_already_loaded(dt1name, & idx) == TRUE)
   {
      glb_dt1[idx].ds1_usage++;
      return idx;
   }
   else
   {
      // search the first available dt1
      for (i=0; i<DT1_MAX; i++)
      {
         if (glb_dt1[i].ds1_usage == 0)
         {
            // this one is emtpy, now load it
            entry = misc_load_mpq_file(
                       dt1name,
                       (char **) & glb_dt1[i].buffer,
                       & glb_dt1[i].buff_len,
                       TRUE
                    );
            if (entry == -1)
            {
               sprintf(tmp, "dt1_add() : file %s not found", dt1name);
               ds1edit_error(tmp);
            }

            // dt1 update
            strcpy(glb_dt1[i].name, dt1name);
            glb_dt1[i].ds1_usage = 1;
            dt1_struct_update(i);

            // free the copy of the dt1 in mem
            free(glb_dt1[i].buffer);
            glb_dt1[i].buffer = NULL;
            glb_dt1[i].buff_len = 0;

            // tell to the ds1 which idx in the glb_dt1[] have been choosen
            return i;
         }
      }
      sprintf(tmp, "can't load %s from mpq,\ntoo much dt1 already loaded (%i)",
         dt1name, DT1_MAX);
      ds1edit_error(tmp);
      return -1; // useless, just for not having a vc6 warning
   }
}


// ==========================================================================
// to load ds1edit.dt1 from the data\ directory (not the mod_dir directory)
int dt1_add_special(char * dt1name)
{
   int  i, idx, entry;
   char tmp[256];

   if (dt1_already_loaded(dt1name, & idx) == TRUE)
   {
      glb_dt1[idx].ds1_usage++;
      return idx;
   }
   else
   {
      // search the first available dt1
      for (i=0; i<DT1_MAX; i++)
      {
         if (glb_dt1[i].ds1_usage == 0)
         {
            // load from ds1edit directory
            entry = mod_load_in_mem(".", dt1name,
                                         & glb_dt1[i].buffer, & glb_dt1[i].buff_len);
            if (entry == -1)
            {
               sprintf(tmp, "(dt1_add_special) file %s not found", dt1name);
               ds1edit_error(tmp);
            }
            else
               printf(", found\n");

            // dt1 update
            strcpy(glb_dt1[i].name, dt1name);
            glb_dt1[i].ds1_usage = 1;
            dt1_struct_update(i);

            // free the copy of the dt1 in mem
            free(glb_dt1[i].buffer);
            glb_dt1[i].buffer = NULL;
            glb_dt1[i].buff_len = 0;

            // tell to the ds1 which idx in the glb_dt1[] have been choosen
            return i;
         }
      }
      sprintf(tmp, "can't load %s from mpq,\ntoo much dt1 already loaded (%i)",
         dt1name, DT1_MAX);
      ds1edit_error(tmp);
      return -1; // useless, just for not having a vc6 warning
   }
}
