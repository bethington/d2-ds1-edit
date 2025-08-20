#include "structs.h"
#include "error.h"
#include "ds1misc.h"
#include "undo.h"


// ==========================================================================
// after each cell, reset the flags to prepare the next 1
void undo_reset_flags(int ds1_idx)
{
   UNDO_DAT_FLG_S * df = & glb_ds1[ds1_idx].undo.dat_flg;
   int            i;

   for (i=0; i < FLOOR_MAX_LAYER; i++)
      df->f[i] = 0;

   for (i=0; i < SHADOW_MAX_LAYER; i++)
      df->s[i] = 0;

   for (i=0; i < WALL_MAX_LAYER; i++)
      df->w[i] = 0;
}


// ==========================================================================
// del tmp files at the end of the prog
void undo_exit(void)
{
   UNDO_S * u;
   struct al_ffblk f;
   int    done, i;
   char   tmp[150];

   for (i=0; i<DS1_MAX; i++)
   {
      if (strlen(glb_ds1[i].name))
      {
         u = & glb_ds1[i].undo;
         if (u->fptr != NULL)
         {
            fclose(u->fptr);
            u->fptr = NULL;
            strcpy(u->name, "");
         }
      }
   }
      
   sprintf(tmp, "%s%sundo*.bin", glb_ds1edit_data_dir, glb_ds1edit_tmp_dir);
   printf("\nremoving tmp undo buffers (%s)...", tmp);
   fprintf(stderr, "      . removing tmp undo buffers (%s)...", tmp);
   fflush(stderr);
   fflush(stdout);
   done = al_findfirst(tmp, &f, -1);
   while ( ! done)
   {
      sprintf(tmp, "%s%s%s", glb_ds1edit_data_dir, glb_ds1edit_tmp_dir, f.name);
      remove(tmp);
      done = al_findnext(&f);
   }
   al_findclose(&f);
   printf("done\n");
   fprintf(stderr, "done\n");
   fflush(stderr);
   fflush(stdout);
}


// ==========================================================================
// create a new tmp file
void undo_new_tile_buffer(int ds1_idx)
{
   UNDO_S * u = & glb_ds1[ds1_idx].undo;
   long   fake = 0, i;
   char   tmp[150];

   // just in case
   if (u->fptr != NULL)
   {
      fclose(u->fptr);
      sprintf(tmp, "undo_new_buffer() : undo file pointer was not NULL, "
                   "tile buffer %s", u->name);
      ds1edit_error(tmp);
   }
      
   // some undo have been made ?
   if (u->cur_buf_num < u->buf_num - 1)
   {
      // delete "old" undo buffer
      for (i = u->cur_buf_num + 1; i < u->buf_num; i++)
      {
         sprintf(tmp, "%s%sundo%itile%li.bin", glb_ds1edit_data_dir,
            glb_ds1edit_tmp_dir, ds1_idx, i);
         remove(tmp);
      }

      // adjust the buffer counter
      u->buf_num = u->cur_buf_num + 1;
   }
   
   // writing the file
   sprintf(u->name, "%s%sundo%itile%li.bin", glb_ds1edit_data_dir,
      glb_ds1edit_tmp_dir, ds1_idx, u->buf_num);
   u->fptr = fopen(u->name, "wb");
   if (u->fptr == NULL)
   {
      sprintf(tmp, "undo_new_tile_buffer() : can't create %s", u->name);
      ds1edit_error(tmp);
   }

   // tag
   fwrite(u->tag, sizeof(u->tag), 1, u->fptr);

   // fake segment number (will be updated later)
   u->seg_offset = ftell(u->fptr);
   fwrite(&fake, sizeof(u->seg_num), 1, u->fptr);

   // init
   u->in_seg      = FALSE;
   u->seg_num     = 0;
   u->cell_offset = -1;
   u->cell_num    = 0;
   u->old_x       = (UBYTE) -1;
   u->old_y       = (UBYTE) -1;
   undo_reset_flags(ds1_idx);
}


// ==========================================================================
// start a new segment of continuous cells in the undo buffer file
void undo_start_seg_in_tile_buffer(int ds1_idx, UBYTE x, UBYTE y)
{
   UNDO_S * u = & glb_ds1[ds1_idx].undo;
   long   fake = 0;
   char   tmp[150];

   // just in case
   if (u->fptr == NULL)
   {
      fclose(u->fptr);
      sprintf(tmp, "undo_start_seg_in_tile_buffer() : undo file pointer"
                   "is NULL, tile buffer %li", u->buf_num);
      ds1edit_error(tmp);
   }

   // just in case (bis)
   if (u->in_seg != FALSE)
   {
      fclose(u->fptr);
      sprintf(tmp, "undo_start_seg_in_tile_buffer() : try to start a "
                   "segment of cells while current one not closed");
      ds1edit_error(tmp);
   }

   // start new seg
   u->in_seg = TRUE;
   u->seg_num++;
   
   // starting cell
   fwrite(&x, sizeof(x), 1, u->fptr);
   fwrite(&y, sizeof(y), 1, u->fptr);

   // fake cell number of this segment (will be updated later)
   u->cell_offset = ftell(u->fptr);
   fwrite(&fake, sizeof(u->cell_num), 1, u->fptr);

   // init
   u->cell_num = 0;
   u->old_x    = x - 1;
   u->old_y    = y;
}


// ==========================================================================
// close a segment
void undo_close_seg_in_tile_buffer(int ds1_idx)
{
   UNDO_S * u = & glb_ds1[ds1_idx].undo;
   long   old_ptr;
   char   tmp[150];

   // just in case
   if (u->fptr == NULL)
   {
      fclose(u->fptr);
      sprintf(tmp, "undo_close_seg_in_tile_buffer() : undo file pointer"
                   "is NULL, tile buffer %li", u->buf_num);
      ds1edit_error(tmp);
   }

   if (u->seg_num != 0)
   {
      // just in case
      if (u->in_seg == FALSE)
      {
         fclose(u->fptr);
         sprintf(tmp, "undo_close_seg_in_tile_buffer() : try to close a "
                      "segment of cells while none was started");
         ds1edit_error(tmp);
      }

      // end the segment, write the TRUE cell number
      old_ptr = ftell(u->fptr);
      fseek(u->fptr, u->cell_offset, SEEK_SET);
      fwrite(&u->cell_num, sizeof(u->cell_num), 1, u->fptr);
      fseek(u->fptr, old_ptr, SEEK_SET);
   }

   // init
   u->in_seg = FALSE;
}


// ==========================================================================
// add 1 cell datas to the current undo buffer
void undo_add_to_tile_buffer(int ds1_idx, int ix, int iy)
{
   UNDO_S         * u  = & glb_ds1[ds1_idx].undo;
   UNDO_DAT_FLG_S * df = & u->dat_flg;
   int            tf   = (iy * glb_ds1[ds1_idx].floor_line) +
                         (ix * glb_ds1[ds1_idx].floor_num),
                  ts   = (iy * glb_ds1[ds1_idx].shadow_line) +
                         (ix * glb_ds1[ds1_idx].shadow_num),
                  tw   = (iy * glb_ds1[ds1_idx].wall_line)  +
                         (ix * glb_ds1[ds1_idx].wall_num),
                  i;
   CELL_F_S       * f  = glb_ds1[ds1_idx].floor_buff  + tf;
   CELL_S_S       * s  = glb_ds1[ds1_idx].shadow_buff + ts;
   CELL_W_S       * w  = glb_ds1[ds1_idx].wall_buff   + tw;
   UWORD          layer_flag = 0; // 4 walls + 4 floors + 1 shadow = 9 bits
   UBYTE          x = ix & 0xFF, y = iy & 0xFF;
   char           tmp[150];

   // just in case
   if (u->fptr == NULL)
   {
      sprintf(tmp, "undo_add_to_tile_buffer() : can't write in undo tile buffer %li",
         u->buf_num);
      ds1edit_error(tmp);
   }

   // check which datas are to be to saved
   for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
      if (df->f[i])
         layer_flag |= 1 << i;
   for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
      if (df->s[i])
         layer_flag |= 1 << (i + FLOOR_MAX_LAYER);
   for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
      if (df->w[i])
         layer_flag |= 1 << (i + FLOOR_MAX_LAYER + SHADOW_MAX_LAYER);

   // save them (if any)
   if (layer_flag)
   {
      // same segment ?
      if ( (u->seg_num == 0) || ((x != (u->old_x + 1)) || (y != u->old_y)) )
      {
         // another segment, close a precedent one ?
         if (u->in_seg != FALSE)
         {
            // close the precednet
            undo_close_seg_in_tile_buffer(ds1_idx);
         }

         // start a new one
         undo_start_seg_in_tile_buffer(ds1_idx, x, y);
      }

      // add this cell to the segment
      fwrite(&layer_flag, sizeof(layer_flag), 1, u->fptr);
      for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
      {
         if (layer_flag & (1 << i))
         {
            fwrite(&df->f[i], sizeof(df->f[i]), 1, u->fptr);
            
            if (df->f[i] & DF_P1)
               fwrite(&f[i].prop1, sizeof(f[i].prop1), 1, u->fptr);

            if (df->f[i] & DF_P2)
               fwrite(&f[i].prop2, sizeof(f[i].prop2), 1, u->fptr);

            if (df->f[i] & DF_P3)
               fwrite(&f[i].prop3, sizeof(f[i].prop3), 1, u->fptr);

            if (df->f[i] & DF_P4)
               fwrite(&f[i].prop4, sizeof(f[i].prop4), 1, u->fptr);

            if (df->f[i] & DF_BT)
               fwrite(&f[i].bt_idx, sizeof(f[i].bt_idx), 1, u->fptr);

            if (df->f[i] & DF_FL)
               fwrite(&f[i].flags, sizeof(f[i].flags), 1, u->fptr);
         }
      }
      for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
      {
         if (layer_flag & (1 << (i + FLOOR_MAX_LAYER)))
         {
            fwrite(&df->s[i], sizeof(df->s[i]), 1, u->fptr);
            
            if (df->s[i] & DF_P1)
               fwrite(&s[i].prop1, sizeof(s[i].prop1), 1, u->fptr);

            if (df->s[i] & DF_P2)
               fwrite(&s[i].prop2, sizeof(s[i].prop2), 1, u->fptr);

            if (df->s[i] & DF_P3)
               fwrite(&s[i].prop3, sizeof(s[i].prop3), 1, u->fptr);

            if (df->s[i] & DF_P4)
               fwrite(&s[i].prop4, sizeof(s[i].prop4), 1, u->fptr);

            if (df->s[i] & DF_BT)
               fwrite(&s[i].bt_idx, sizeof(s[i].bt_idx), 1, u->fptr);

            if (df->s[i] & DF_FL)
               fwrite(&s[i].flags, sizeof(s[i].flags), 1, u->fptr);
         }
      }
      for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
      {
         if (layer_flag & (1 << (i + FLOOR_MAX_LAYER + SHADOW_MAX_LAYER)))
         {
            fwrite(&df->w[i], sizeof(df->w[i]), 1, u->fptr);
            
            if (df->w[i] & DF_P1)
               fwrite(&w[i].prop1, sizeof(w[i].prop1), 1, u->fptr);

            if (df->w[i] & DF_P2)
               fwrite(&w[i].prop2, sizeof(w[i].prop2), 1, u->fptr);

            if (df->w[i] & DF_P3)
               fwrite(&w[i].prop3, sizeof(w[i].prop3), 1, u->fptr);

            if (df->w[i] & DF_P4)
               fwrite(&w[i].prop4, sizeof(w[i].prop4), 1, u->fptr);

            if (df->w[i] & DF_OR)
               fwrite(&w[i].orientation, sizeof(w[i].orientation), 1, u->fptr);

            if (df->w[i] & DF_BT)
               fwrite(&w[i].bt_idx, sizeof(w[i].bt_idx), 1, u->fptr);

            if (df->w[i] & DF_FL)
               fwrite(&w[i].flags, sizeof(w[i].flags), 1, u->fptr);
         }
      }

      // this cell was writen
      undo_reset_flags(ds1_idx);
      u->old_x++;
      u->cell_num++;
   }
}


// ==========================================================================
// close the undo buffer
void undo_close_tile_buffer(int ds1_idx)
{
   UNDO_S * u = & glb_ds1[ds1_idx].undo;
   long   old_ptr;
   char   tmp[150];

   // just in case
   if (u->fptr == NULL)
   {
      sprintf(tmp, "undo_close_tile_buffer() : no undo tile buffer pointer "
                   "to close (%li)", u->buf_num);
      ds1edit_error(tmp);
   }

   if (u->in_seg != FALSE)
      undo_close_seg_in_tile_buffer(ds1_idx);

   // write the True segment number
   old_ptr = ftell(u->fptr);
   fseek(u->fptr, u->seg_offset, SEEK_SET);
   fwrite(&u->seg_num, sizeof(u->seg_num), 1, u->fptr);
   fseek(u->fptr, old_ptr, SEEK_SET);
   
   // close
   if (fclose(u->fptr) != 0)
   {
      sprintf(tmp, "undo_close_tile_buffer() : can't close undo tile "
                   "buffer %li", u->buf_num);
      ds1edit_error(tmp);
   }

   // end
   u->cur_buf_num = u->buf_num;
   u->buf_num++;
   u->fptr = NULL;
   strcpy(u->name, "");
}


// ==========================================================================
// open and apply an undo buffer into the ds1
void undo_apply_tile_buffer(int ds1_idx)
{
   UNDO_S * u = & glb_ds1[ds1_idx].undo;
   FILE   * in;
   int    tf, ts, tw, i, c, s, seg_num, cell_num, x, y, layer_flag, data_flag;
   CELL_F_S * f;
   CELL_S_S * sh;
   CELL_W_S * w;
   char   undo_file[100], tmp[100];
   
   if ((u->buf_num <= 0) || (u->cur_buf_num < 0))
      return;

   sprintf(undo_file, "%s%sundo%itile%li.bin", glb_ds1edit_data_dir,
      glb_ds1edit_tmp_dir, ds1_idx, u->cur_buf_num);
   in = fopen(undo_file, "rb");
   if (in == NULL)
   {
      sprintf(tmp, "undo_apply_tile_buffer() : can't open %s", undo_file);
      ds1edit_error(tmp);
   }

   for (i=0; i < sizeof(u->tag); i++)
   {
      c = fgetc(in);
      if (c != u->tag[i])
      {
         fclose(in);
         sprintf(tmp, "undo_apply_tile_buffer() : wrong tag in %s", u->name);
         ds1edit_error(tmp);
      }
   }

   seg_num = fgetc(in);
   for (s=0; s < seg_num; s++)
   {
      x  = fgetc(in);
      y  = fgetc(in);
      cell_num = fgetc(in);
      for (c=0; c < cell_num; c++)
      {
         if ( (x < 0) || (y < 0) ||
              (x >= glb_ds1[ds1_idx].width) ||
              (y >= glb_ds1[ds1_idx].height)
            )
         {
            fclose(in);
            sprintf(tmp, "undo_apply_tile_buffer() : wrong cell (%i, %i)",
               x, y);
            ds1edit_error(tmp);
         }
         tf = (y * glb_ds1[ds1_idx].floor_line)  + (x * glb_ds1[ds1_idx].floor_num);
         ts = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
         tw = (y * glb_ds1[ds1_idx].wall_line)   + (x * glb_ds1[ds1_idx].wall_num);
         f  = glb_ds1[ds1_idx].floor_buff  + tf;
         sh = glb_ds1[ds1_idx].shadow_buff + ts;
         w  = glb_ds1[ds1_idx].wall_buff   + tw;
         fread(&layer_flag, sizeof(UWORD), 1, in);
         for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
         {
            if (layer_flag & (1 << i))
            {
               data_flag = fgetc(in);

               if (data_flag & DF_P1)
                  f[i].prop1 = fgetc(in);

               if (data_flag & DF_P2)
                  f[i].prop2 = fgetc(in);

               if (data_flag & DF_P3)
                  f[i].prop3 = fgetc(in);

               if (data_flag & DF_P4)
                  f[i].prop4 = fgetc(in);

               if (data_flag & DF_BT)
                  fread(&f[i].bt_idx, sizeof(f[i].bt_idx), 1, in);
               else
                  fprop_2_block(ds1_idx, &f[i]);

               if (data_flag & DF_FL)
                  f[i].flags = fgetc(in);
            }
         }
         for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
         {
            if (layer_flag & (1 << (i + FLOOR_MAX_LAYER)))
            {
               data_flag = fgetc(in);

               if (data_flag & DF_P1)
                  sh[i].prop1 = fgetc(in);

               if (data_flag & DF_P2)
                  sh[i].prop2 = fgetc(in);

               if (data_flag & DF_P3)
                  sh[i].prop3 = fgetc(in);

               if (data_flag & DF_P4)
                  sh[i].prop4 = fgetc(in);

               if (data_flag & DF_BT)
                  fread(&sh[i].bt_idx, sizeof(sh[i].bt_idx), 1, in);
               else
                  sprop_2_block(ds1_idx, &sh[i]);

               if (data_flag & DF_FL)
                  sh[i].flags = fgetc(in);
            }
         }
         for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
         {
            if (layer_flag & (1 << (i + FLOOR_MAX_LAYER + SHADOW_MAX_LAYER)))
            {
               data_flag = fgetc(in);

               if (data_flag & DF_P1)
                  w[i].prop1 = fgetc(in);

               if (data_flag & DF_P2)
                  w[i].prop2 = fgetc(in);

               if (data_flag & DF_P3)
                  w[i].prop3 = fgetc(in);

               if (data_flag & DF_P4)
                  w[i].prop4 = fgetc(in);

               if (data_flag & DF_OR)
                  w[i].orientation = fgetc(in);

               if (data_flag & DF_BT)
                  fread(&w[i].bt_idx, sizeof(w[i].bt_idx), 1, in);
               else
                  wprop_2_block(ds1_idx, &w[i]);

               if (data_flag & DF_FL)
                  w[i].flags = fgetc(in);
            }
         }
         x++;
      }
   }
   fclose(in);
   u->cur_buf_num--;
}


// ==========================================================================
// add a cell to the undo buffer
// data aren't in the current ds1 cells
void undo_add_this_tile_to_buffer(int ds1_idx, int ix, int iy,
                                  CELL_F_S * f, CELL_W_S * w, CELL_S_S * s)
{
   UNDO_S         * u  = & glb_ds1[ds1_idx].undo;
   UNDO_DAT_FLG_S * df = & u->dat_flg;
   int            i;
   UWORD          layer_flag = 0; // 4 walls + 4 floors + 1 shadow = 9 bits
   UBYTE          x = ix & 0xFF, y = iy & 0xFF;
   char           tmp[150];


   // just in case
   if (u->fptr == NULL)
   {
      sprintf(tmp, "undo_add_to_tile_buffer() : can't write in undo tile buffer %li",
         u->buf_num);
      ds1edit_error(tmp);
   }

   // check which datas are to be to saved
   for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
      if (df->f[i])
         layer_flag |= 1 << i;

   for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
      if (df->s[i])
         layer_flag |= 1 << (i + FLOOR_MAX_LAYER);

   for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
      if (df->w[i])
         layer_flag |= 1 << (i + FLOOR_MAX_LAYER + SHADOW_MAX_LAYER);


   // save them (if any)
   if (layer_flag)
   {
      // same segment ?
      if ( (u->seg_num == 0) || ((x != (u->old_x + 1)) || (y != u->old_y)) )
      {
         // another segment, close a precedent one ?
         if (u->in_seg != FALSE)
         {
            // close the precednet
            undo_close_seg_in_tile_buffer(ds1_idx);
         }

         // start a new one
         undo_start_seg_in_tile_buffer(ds1_idx, x, y);
      }

      // add this cell to the segment
      fwrite(&layer_flag, sizeof(layer_flag), 1, u->fptr);
      for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
      {
         if (layer_flag & (1 << i))
         {
            fwrite(&df->f[i], sizeof(df->f[i]), 1, u->fptr);
            
            if (df->f[i] & DF_P1)
               fwrite(&f[i].prop1, sizeof(f[i].prop1), 1, u->fptr);

            if (df->f[i] & DF_P2)
               fwrite(&f[i].prop2, sizeof(f[i].prop2), 1, u->fptr);

            if (df->f[i] & DF_P3)
               fwrite(&f[i].prop3, sizeof(f[i].prop3), 1, u->fptr);

            if (df->f[i] & DF_P4)
               fwrite(&f[i].prop4, sizeof(f[i].prop4), 1, u->fptr);

            if (df->f[i] & DF_BT)
               fwrite(&f[i].bt_idx, sizeof(f[i].bt_idx), 1, u->fptr);

            if (df->f[i] & DF_FL)
               fwrite(&f[i].flags, sizeof(f[i].flags), 1, u->fptr);
         }
      }
      for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
      {
         if (layer_flag & (1 << (i + FLOOR_MAX_LAYER) ))
         {
            fwrite(&df->s[i], sizeof(df->s[i]), 1, u->fptr);
            
            if (df->s[i] & DF_P1)
               fwrite(&s[i].prop1, sizeof(s[i].prop1), 1, u->fptr);

            if (df->s[i] & DF_P2)
               fwrite(&s[i].prop2, sizeof(s[i].prop2), 1, u->fptr);

            if (df->s[i] & DF_P3)
               fwrite(&s[i].prop3, sizeof(s[i].prop3), 1, u->fptr);

            if (df->s[i] & DF_P4)
               fwrite(&s[i].prop4, sizeof(s[i].prop4), 1, u->fptr);

            if (df->s[i] & DF_BT)
               fwrite(&s[i].bt_idx, sizeof(s[i].bt_idx), 1, u->fptr);

            if (df->s[i] & DF_FL)
               fwrite(&s[i].flags, sizeof(s[i].flags), 1, u->fptr);
         }
      }
      for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
      {
         if (layer_flag & (1 << (i + FLOOR_MAX_LAYER + SHADOW_MAX_LAYER)))
         {
            fwrite(&df->w[i], sizeof(df->w[i]), 1, u->fptr);
            
            if (df->w[i] & DF_P1)
               fwrite(&w[i].prop1, sizeof(w[i].prop1), 1, u->fptr);

            if (df->w[i] & DF_P2)
               fwrite(&w[i].prop2, sizeof(w[i].prop2), 1, u->fptr);

            if (df->w[i] & DF_P3)
               fwrite(&w[i].prop3, sizeof(w[i].prop3), 1, u->fptr);

            if (df->w[i] & DF_P4)
               fwrite(&w[i].prop4, sizeof(w[i].prop4), 1, u->fptr);

            if (df->w[i] & DF_OR)
               fwrite(&w[i].orientation, sizeof(w[i].orientation), 1, u->fptr);

            if (df->w[i] & DF_BT)
               fwrite(&w[i].bt_idx, sizeof(w[i].bt_idx), 1, u->fptr);

            if (df->w[i] & DF_FL)
               fwrite(&w[i].flags, sizeof(w[i].flags), 1, u->fptr);
         }
      }

      // this cell was writen
      undo_reset_flags(ds1_idx);
      u->old_x++;
      u->cell_num++;
   }
}
