#include "structs.h"
#include "ds1misc.h"
#include "undo.h"
#include "edittile.h"


// ==========================================================================
// =                                                                        =
// =  COPY / PASTE MANAGMENT                                                =
// =                                                                        =
// ==========================================================================


// ==========================================================================
// search the middle of the selection (for centered paste purpose)
void edittile_middle_select(int ds1_idx, int * start_x, int * start_y)
{
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   CELL_W_S * w_ptr;
   int      x, y, t, i;
   int      minx, miny, maxx, maxy;


   maxx = 0;
   maxy = 0;
   minx = glb_ds1[ds1_idx].width - 1;
   miny = glb_ds1[ds1_idx].height - 1;
   
   for (y=0; y < glb_ds1[ds1_idx].height; y++)
   {
      for (x=0; x < glb_ds1[ds1_idx].width; x++)
      {
         // floors
         t = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
         for (i=0; i<glb_ds1[ds1_idx].floor_num; i++)
         {
            f_ptr = glb_ds1[ds1_idx].floor_buff + t + i;
            if (IS_SELECTED(f_ptr->flags))
            {
               if (x < minx) minx = x;
               if (x > maxx) maxx = x;
               if (y < miny) miny = y;
               if (y > maxy) maxy = y;
            }
         }

         // shadows
         t = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
         for (i=0; i<glb_ds1[ds1_idx].shadow_num; i++)
         {
            s_ptr = glb_ds1[ds1_idx].shadow_buff + t + i;
            if (IS_SELECTED(s_ptr->flags))
            {
               if (x < minx) minx = x;
               if (x > maxx) maxx = x;
               if (y < miny) miny = y;
               if (y > maxy) maxy = y;
            }
         }

         // walls
         t = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
         for (i=0; i<glb_ds1[ds1_idx].wall_num; i++)
         {
            w_ptr = glb_ds1[ds1_idx].wall_buff + t + i;
            if (IS_SELECTED(w_ptr->flags))
            {
               if (x < minx) minx = x;
               if (x > maxx) maxx = x;
               if (y < miny) miny = y;
               if (y > maxy) maxy = y;
            }
         }
      }
   }

   if (minx < maxx)
      * start_x = minx + ((maxx - minx) / 2);
   else
      * start_x = maxx + ((minx - maxx) / 2);

   if (miny < maxy)
      * start_y = miny + ((maxy - miny) / 2);
   else
      * start_y = maxy + ((miny - maxy) / 2);
}


// ==========================================================================
// just a full copy of the current ds1 state into another buffer
// (no hard-to-make algorythm for minimizing the memory usage)
void edittile_paste_prepare(int ds1_idx)
{
   CELL_F_S * src_f_ptr = glb_ds1[ds1_idx].floor_buff,
            * dst_f_ptr = glb_ds1[ds1_idx].floor_buff2;
   CELL_S_S * src_s_ptr = glb_ds1[ds1_idx].shadow_buff,
            * dst_s_ptr = glb_ds1[ds1_idx].shadow_buff2;
   CELL_W_S * src_w_ptr = glb_ds1[ds1_idx].wall_buff,
            * dst_w_ptr = glb_ds1[ds1_idx].wall_buff2;
   long     i;


   // floors
   for (i=0; i < glb_ds1[ds1_idx].floor_len; i++)
   {
      dst_f_ptr->prop1  = src_f_ptr->prop1;
      dst_f_ptr->prop2  = src_f_ptr->prop2;
      dst_f_ptr->prop3  = src_f_ptr->prop3;
      dst_f_ptr->prop4  = src_f_ptr->prop4;
      dst_f_ptr->bt_idx = src_f_ptr->bt_idx;
      dst_f_ptr->flags  = src_f_ptr->flags;
      dst_f_ptr++;
      src_f_ptr++;
   }

   // shadows
   for (i=0; i < glb_ds1[ds1_idx].shadow_len; i++)
   {
      dst_s_ptr->prop1  = src_s_ptr->prop1;
      dst_s_ptr->prop2  = src_s_ptr->prop2;
      dst_s_ptr->prop3  = src_s_ptr->prop3;
      dst_s_ptr->prop4  = src_s_ptr->prop4;
      dst_s_ptr->bt_idx = src_s_ptr->bt_idx;
      dst_s_ptr->flags  = src_s_ptr->flags;
      dst_s_ptr++;
      src_s_ptr++;
   }

   // walls
   for (i=0; i < glb_ds1[ds1_idx].wall_len; i++)
   {
      dst_w_ptr->prop1       = src_w_ptr->prop1;
      dst_w_ptr->prop2       = src_w_ptr->prop2;
      dst_w_ptr->prop3       = src_w_ptr->prop3;
      dst_w_ptr->prop4       = src_w_ptr->prop4;
      dst_w_ptr->orientation = src_w_ptr->orientation;
      dst_w_ptr->bt_idx      = src_w_ptr->bt_idx;
      dst_w_ptr->flags       = src_w_ptr->flags;
      dst_w_ptr++;
      src_w_ptr++;
   }
}


// ==========================================================================
// put back the ds1 in its original state
void edittile_paste_undo(int ds1_idx)
{
   CELL_F_S * src_f_ptr = glb_ds1[ds1_idx].floor_buff2,
            * dst_f_ptr = glb_ds1[ds1_idx].floor_buff;
   CELL_S_S * src_s_ptr = glb_ds1[ds1_idx].shadow_buff2,
            * dst_s_ptr = glb_ds1[ds1_idx].shadow_buff;
   CELL_W_S * src_w_ptr = glb_ds1[ds1_idx].wall_buff2,
            * dst_w_ptr = glb_ds1[ds1_idx].wall_buff;
   long     i;


   // floors
   for (i=0; i < glb_ds1[ds1_idx].floor_len; i++)
   {
      dst_f_ptr->prop1  = src_f_ptr->prop1;
      dst_f_ptr->prop2  = src_f_ptr->prop2;
      dst_f_ptr->prop3  = src_f_ptr->prop3;
      dst_f_ptr->prop4  = src_f_ptr->prop4;
      dst_f_ptr->bt_idx = src_f_ptr->bt_idx;
      dst_f_ptr->flags  = src_f_ptr->flags;
      dst_f_ptr++;
      src_f_ptr++;
   }

   // shadows
   for (i=0; i < glb_ds1[ds1_idx].shadow_len; i++)
   {
      dst_s_ptr->prop1  = src_s_ptr->prop1;
      dst_s_ptr->prop2  = src_s_ptr->prop2;
      dst_s_ptr->prop3  = src_s_ptr->prop3;
      dst_s_ptr->prop4  = src_s_ptr->prop4;
      dst_s_ptr->bt_idx = src_s_ptr->bt_idx;
      dst_s_ptr->flags  = src_s_ptr->flags;
      dst_s_ptr++;
      src_s_ptr++;
   }

   // walls
   for (i=0; i < glb_ds1[ds1_idx].wall_len; i++)
   {
      dst_w_ptr->prop1       = src_w_ptr->prop1;
      dst_w_ptr->prop2       = src_w_ptr->prop2;
      dst_w_ptr->prop3       = src_w_ptr->prop3;
      dst_w_ptr->prop4       = src_w_ptr->prop4;
      dst_w_ptr->orientation = src_w_ptr->orientation;
      dst_w_ptr->bt_idx      = src_w_ptr->bt_idx;
      dst_w_ptr->flags       = src_w_ptr->flags;
      dst_w_ptr++;
      src_w_ptr++;
   }
}


// ==========================================================================
// paste the selection into the new area, showing where are conflicts
void edittile_paste_preview(int ds1_idx, int dx, int dy, PASTE_POS_S * p)
{
   CELL_F_S * src_f_ptr, * dst_f_ptr;
   CELL_S_S * src_s_ptr, * dst_s_ptr;
   CELL_W_S * src_w_ptr, * dst_w_ptr;
   int      x, y, ts, td, i, f;


   // if tiles are 'cut' instead of 'copy', then delete the source tiles
   if (p->is_cut == TRUE)
   {
      if (p->cut_saved == FALSE)
      {
         // with UNDO
         edittile_delete_selected_tiles(p->src_ds1_idx);
         p->cut_saved = TRUE;
      }
      else
      {
         // with NO UNDO
         edittile_delete_selected_tiles2(p->src_ds1_idx);
      }
   }

   // copy tiles
   for (y=0; y < glb_ds1[p->src_ds1_idx].height; y++)
   {
      if ( ((y+dy) < 0) || ((y+dy) >= glb_ds1[ds1_idx].height) )
         continue;
         
      for (x=0; x < glb_ds1[p->src_ds1_idx].width; x++)
      {
         if ( ((x+dx) < 0) || ((x+dx) >= glb_ds1[ds1_idx].width) )
            continue;

         // floors
         ts = (y * glb_ds1[p->src_ds1_idx].floor_line) +
              (x * glb_ds1[p->src_ds1_idx].floor_num);
         td = ((y+dy) * glb_ds1[ds1_idx].floor_line) +
              ((x+dx) * glb_ds1[ds1_idx].floor_num);
         for (i=0; (i<glb_ds1[ds1_idx].floor_num) &&
                   (i<glb_ds1[p->src_ds1_idx].floor_num); i++)
         {
            src_f_ptr = glb_ds1[p->src_ds1_idx].floor_buff2 + ts + i;
            dst_f_ptr = glb_ds1[ds1_idx].floor_buff + td + i;
            
            f = src_f_ptr->flags;
            if (IS_SELECTED(f) && (src_f_ptr->bt_idx != 0))
            {
               SET_COPY_INFO(f);
               if (dst_f_ptr->bt_idx != 0)
               {
                  if ( (dst_f_ptr->prop1 != src_f_ptr->prop1) ||
                       (dst_f_ptr->prop2 != src_f_ptr->prop2) ||
                       (dst_f_ptr->prop3 != src_f_ptr->prop3) ||
                       (dst_f_ptr->prop4 != src_f_ptr->prop4)
                     )
                  {
                     SET_COPY_STATE(f); // conflict
                  }
               }
               dst_f_ptr->prop1  = src_f_ptr->prop1;
               dst_f_ptr->prop2  = src_f_ptr->prop2;
               dst_f_ptr->prop3  = src_f_ptr->prop3;
               dst_f_ptr->prop4  = src_f_ptr->prop4;
               dst_f_ptr->flags  = f;
               fprop_2_block(ds1_idx, dst_f_ptr); // search dst->bt_idx
            }
         }

         // shadows
         ts = (y * glb_ds1[p->src_ds1_idx].shadow_line) +
              (x * glb_ds1[p->src_ds1_idx].shadow_num);
         td = ((y+dy) * glb_ds1[ds1_idx].shadow_line) +
              ((x+dx) * glb_ds1[ds1_idx].shadow_num);
         for (i=0; (i<glb_ds1[ds1_idx].shadow_num) &&
                   (i<glb_ds1[p->src_ds1_idx].shadow_num); i++)
         {
            src_s_ptr = glb_ds1[p->src_ds1_idx].shadow_buff2 + ts + i;
            dst_s_ptr = glb_ds1[ds1_idx].shadow_buff + td + i;
            
            f = src_s_ptr->flags;
            if (IS_SELECTED(f) && (src_s_ptr->bt_idx != 0))
            {
               SET_COPY_INFO(f);
               if (dst_s_ptr->bt_idx != 0)
               {
                  if ( (dst_s_ptr->prop1 != src_s_ptr->prop1) ||
                       (dst_s_ptr->prop2 != src_s_ptr->prop2) ||
                       (dst_s_ptr->prop3 != src_s_ptr->prop3) ||
                       (dst_s_ptr->prop4 != src_s_ptr->prop4)
                     )
                  {
                     SET_COPY_STATE(f); // conflict
                  }
               }
               dst_s_ptr->prop1  = src_s_ptr->prop1;
               dst_s_ptr->prop2  = src_s_ptr->prop2;
               dst_s_ptr->prop3  = src_s_ptr->prop3;
               dst_s_ptr->prop4  = src_s_ptr->prop4;
               dst_s_ptr->flags  = f;
               sprop_2_block(ds1_idx, dst_s_ptr); // search dst->bt_idx
            }
         }

         // walls
         ts = (y * glb_ds1[p->src_ds1_idx].wall_line) +
              (x * glb_ds1[p->src_ds1_idx].wall_num);
         td = ((y+dy) * glb_ds1[ds1_idx].wall_line) +
              ((x+dx) * glb_ds1[ds1_idx].wall_num);
         for (i=0; (i<glb_ds1[ds1_idx].wall_num) &&
                   (i<glb_ds1[p->src_ds1_idx].wall_num); i++)
         {
            src_w_ptr = glb_ds1[p->src_ds1_idx].wall_buff2 + ts + i;
            dst_w_ptr = glb_ds1[ds1_idx].wall_buff + td + i;
            
            f = src_w_ptr->flags;
            if (IS_SELECTED(f) && (src_w_ptr->bt_idx != 0) )
            {
               SET_COPY_INFO(f);
               if (dst_w_ptr->bt_idx != 0)
               {
                  if ( (dst_w_ptr->prop1 != src_w_ptr->prop1) ||
                       (dst_w_ptr->prop2 != src_w_ptr->prop2) ||
                       (dst_w_ptr->prop3 != src_w_ptr->prop3) ||
                       (dst_w_ptr->prop4 != src_w_ptr->prop4) ||
                       (dst_w_ptr->orientation != src_w_ptr->orientation)
                     )
                  {
                     SET_COPY_STATE(f); // conflict
                  }
               }
               dst_w_ptr->prop1       = src_w_ptr->prop1;
               dst_w_ptr->prop2       = src_w_ptr->prop2;
               dst_w_ptr->prop3       = src_w_ptr->prop3;
               dst_w_ptr->prop4       = src_w_ptr->prop4;
               dst_w_ptr->orientation = src_w_ptr->orientation;
               dst_w_ptr->flags       = f;
               wprop_2_block(ds1_idx, dst_w_ptr); // search dst->bt_idx
            }
         }
      }
   }
}


// ==========================================================================
// the new copied tiles become the new selection
void edittile_paste_final(int ds1_idx)
{
   CELL_F_S * f_ptr = glb_ds1[ds1_idx].floor_buff,  * f2;
   CELL_S_S * s_ptr = glb_ds1[ds1_idx].shadow_buff, * s2;
   CELL_W_S * w_ptr = glb_ds1[ds1_idx].wall_buff,   * w2;
   int      i, f, tf, ts, tw, x, y, make_undo;
   UNDO_S   * u = & glb_ds1[ds1_idx].undo;


   undo_new_tile_buffer(ds1_idx);
   for (y=0; y<glb_ds1[ds1_idx].height; y++)
   {
      for (x=0; x<glb_ds1[ds1_idx].width; x++)
      {
         tf    = (y * glb_ds1[ds1_idx].floor_line)  + (x * glb_ds1[ds1_idx].floor_num);
         ts    = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
         tw    = (y * glb_ds1[ds1_idx].wall_line)   + (x * glb_ds1[ds1_idx].wall_num);
         f_ptr = glb_ds1[ds1_idx].floor_buff   + tf;
         s_ptr = glb_ds1[ds1_idx].shadow_buff  + ts;
         w_ptr = glb_ds1[ds1_idx].wall_buff    + tw;
         f2    = glb_ds1[ds1_idx].floor_buff2  + tf;
         s2    = glb_ds1[ds1_idx].shadow_buff2 + ts;
         w2    = glb_ds1[ds1_idx].wall_buff2   + tw;

         // undo managment
         make_undo = FALSE;
         for (i=0; i<glb_ds1[ds1_idx].floor_num; i++)
         {
            if (f_ptr[i].prop1 != f2[i].prop1)
               u->dat_flg.f[i] |= DF_P1;

            if (f_ptr[i].prop2 != f2[i].prop2)
               u->dat_flg.f[i] |= DF_P2;

            if (f_ptr[i].prop3 != f2[i].prop3)
               u->dat_flg.f[i] |= DF_P3;

            if (f_ptr[i].prop4 != f2[i].prop4)
               u->dat_flg.f[i] |= DF_P4;
                  
            if (f_ptr[i].bt_idx != f2[i].bt_idx)
               u->dat_flg.f[i] |= DF_BT;

            f = f_ptr[i].flags;
            if (IS_COPY_INFO(f))
               SET_SELECTED(f);
            else
               DEL_SELECTED(f);
            f_ptr[i].flags = IS_SELECTED(f) | IS_HIDE(f);

            if (f_ptr[i].flags != f2[i].flags)
               u->dat_flg.f[i] |= DF_FL;
                  
            if (u->dat_flg.f[i])
               make_undo = TRUE;
         }
         for (i=0; i<glb_ds1[ds1_idx].shadow_num; i++)
         {
            if (s_ptr[i].prop1 != s2[i].prop1)
               u->dat_flg.s[i] |= DF_P1;

            if (s_ptr[i].prop2 != s2[i].prop2)
               u->dat_flg.s[i] |= DF_P2;

            if (s_ptr[i].prop3 != s2[i].prop3)
               u->dat_flg.s[i] |= DF_P3;

            if (s_ptr[i].prop4 != s2[i].prop4)
               u->dat_flg.s[i] |= DF_P4;
                  
            if (s_ptr[i].bt_idx != s2[i].bt_idx)
               u->dat_flg.s[i] |= DF_BT;

            f = s_ptr[i].flags;
            if (IS_COPY_INFO(f))
               SET_SELECTED(f);
            else
               DEL_SELECTED(f);
            s_ptr[i].flags = IS_SELECTED(f) | IS_HIDE(f);

            if (s_ptr[i].flags != s2[i].flags)
               u->dat_flg.s[i] |= DF_FL;
                  
            if (u->dat_flg.s[i])
               make_undo = TRUE;
         }
         for (i=0; i<glb_ds1[ds1_idx].wall_num; i++)
         {
            if (w_ptr[i].prop1 != w2[i].prop1)
               u->dat_flg.w[i] |= DF_P1;

            if (w_ptr[i].prop2 != w2[i].prop2)
               u->dat_flg.w[i] |= DF_P2;

            if (w_ptr[i].prop3 != w2[i].prop3)
               u->dat_flg.w[i] |= DF_P3;

            if (w_ptr[i].prop4 != w2[i].prop4)
               u->dat_flg.w[i] |= DF_P4;
                  
            if (w_ptr[i].orientation != w2[i].orientation)
               u->dat_flg.w[i] |= DF_OR;
                  
            if (w_ptr[i].bt_idx != w2[i].bt_idx)
               u->dat_flg.w[i] |= DF_BT;

            f = w_ptr[i].flags;
            if (IS_COPY_INFO(f))
               SET_SELECTED(f);
            else
               DEL_SELECTED(f);
            w_ptr[i].flags = IS_SELECTED(f) | IS_HIDE(f);

            if (w_ptr[i].flags != w2[i].flags)
               u->dat_flg.w[i] |= DF_FL;
                  
            if (u->dat_flg.w[i])
               make_undo = TRUE;
         }
         if (make_undo)
            undo_add_this_tile_to_buffer(ds1_idx, x, y, f2, w2, s2);
      }
   }
   
   undo_close_tile_buffer(ds1_idx);
}




// ==========================================================================
// =                                                                        =
// =  DELETE TILES MANAGMENT                                                =
// =                                                                        =
// ==========================================================================


// ==========================================================================
// delete the selected tiles of a ds1, WITH UNDO
void edittile_delete_selected_tiles(int ds1_idx)
{
   CELL_W_S * w_ptr;
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   int      i, tf, ts, tw, x, y, make_undo;
   UNDO_S   * u = & glb_ds1[ds1_idx].undo;


   undo_new_tile_buffer(ds1_idx);
   for (y=0; y<glb_ds1[ds1_idx].height; y++)
   {
      for (x=0; x<glb_ds1[ds1_idx].width; x++)
      {
         tf    = (y * glb_ds1[ds1_idx].floor_line)  + (x * glb_ds1[ds1_idx].floor_num);
         ts    = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
         tw    = (y * glb_ds1[ds1_idx].wall_line)   + (x * glb_ds1[ds1_idx].wall_num);
         f_ptr = glb_ds1[ds1_idx].floor_buff  + tf;
         s_ptr = glb_ds1[ds1_idx].shadow_buff + ts;
         w_ptr = glb_ds1[ds1_idx].wall_buff   + tw;

         // undo managment
         make_undo = FALSE;
         for (i=0; i<glb_ds1[ds1_idx].floor_num; i++)
         {
            if (IS_SELECTED(f_ptr[i].flags))
            {
               if (f_ptr[i].prop1)
                  u->dat_flg.f[i] |= DF_P1;

               if (f_ptr[i].prop2)
                  u->dat_flg.f[i] |= DF_P2;

               if (f_ptr[i].prop3)
                  u->dat_flg.f[i] |= DF_P3;

               if (f_ptr[i].prop4)
                  u->dat_flg.f[i] |= DF_P4;
                  
               if (f_ptr[i].bt_idx != 0)
                  u->dat_flg.f[i] |= DF_BT;

               if (f_ptr[i].flags)
                  u->dat_flg.f[i] |= DF_FL;
                  
               if (u->dat_flg.f[i])
                  make_undo = TRUE;
            }
         }
         for (i=0; i<glb_ds1[ds1_idx].shadow_num; i++)
         {
            if (IS_SELECTED(s_ptr[i].flags))
            {
               if (s_ptr[i].prop1)
                  u->dat_flg.s[i] |= DF_P1;

               if (s_ptr[i].prop2)
                  u->dat_flg.s[i] |= DF_P2;

               if (s_ptr[i].prop3)
                  u->dat_flg.s[i] |= DF_P3;

               if (s_ptr[i].prop4)
                  u->dat_flg.s[i] |= DF_P4;
                  
               if (s_ptr[i].bt_idx != 0)
                  u->dat_flg.s[i] |= DF_BT;

               if (s_ptr[i].flags)
                  u->dat_flg.s[i] |= DF_FL;
                  
               if (u->dat_flg.s[i])
                  make_undo = TRUE;
            }
         }
         for (i=0; i<glb_ds1[ds1_idx].wall_num; i++)
         {
            if (IS_SELECTED(w_ptr[i].flags))
            {
               if (w_ptr[i].prop1)
                  u->dat_flg.w[i] |= DF_P1;

               if (w_ptr[i].prop2)
                  u->dat_flg.w[i] |= DF_P2;

               if (w_ptr[i].prop3)
                  u->dat_flg.w[i] |= DF_P3;

               if (w_ptr[i].prop4)
                  u->dat_flg.w[i] |= DF_P4;
                  
               if (w_ptr[i].orientation)
                  u->dat_flg.w[i] |= DF_OR;
                  
               if (w_ptr[i].bt_idx != 0)
                  u->dat_flg.w[i] |= DF_BT;

               if (w_ptr[i].flags)
                  u->dat_flg.w[i] |= DF_FL;
                  
               if (u->dat_flg.w[i])
                  make_undo = TRUE;
            }
         }
         if (make_undo)
            undo_add_to_tile_buffer(ds1_idx, x, y);
         
         // floors
         for (i=0; i<glb_ds1[ds1_idx].floor_num; i++)
         {
            if (IS_SELECTED(f_ptr[i].flags))
            {
               f_ptr[i].prop1  = 0;
               f_ptr[i].prop2  = 0;
               f_ptr[i].prop3  = 0;
               f_ptr[i].prop4  = 0;
               f_ptr[i].bt_idx = 0;
               f_ptr[i].flags  = 0;
            }
         }

         // shadows
         for (i=0; i<glb_ds1[ds1_idx].shadow_num; i++)
         {
            if (IS_SELECTED(s_ptr[i].flags))
            {
               s_ptr[i].prop1  = 0;
               s_ptr[i].prop2  = 0;
               s_ptr[i].prop3  = 0;
               s_ptr[i].prop4  = 0;
               s_ptr[i].bt_idx = 0;
               s_ptr[i].flags  = 0;
            }
         }

         // walls
         for (i=0; i<glb_ds1[ds1_idx].wall_num; i++)
         {
            if (IS_SELECTED(w_ptr[i].flags))
            {
               w_ptr[i].prop1       = 0;
               w_ptr[i].prop2       = 0;
               w_ptr[i].prop3       = 0;
               w_ptr[i].prop4       = 0;
               w_ptr[i].orientation = 0;
               w_ptr[i].bt_idx      = 0;
               w_ptr[i].flags       = 0;
            }
         }
      }
   }
   undo_close_tile_buffer(ds1_idx);
}


// ==========================================================================
// delete selected tiles of a ds1 with NO UNDO
// function used only by edittile_paste_preview()
void edittile_delete_selected_tiles2(int ds1_idx)
{
   CELL_W_S * w_ptr;
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   int      i, tf, ts, tw, x, y;


   for (y=0; y<glb_ds1[ds1_idx].height; y++)
   {
      for (x=0; x<glb_ds1[ds1_idx].width; x++)
      {
         tf    = (y * glb_ds1[ds1_idx].floor_line)  + (x * glb_ds1[ds1_idx].floor_num);
         ts    = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
         tw    = (y * glb_ds1[ds1_idx].wall_line)   + (x * glb_ds1[ds1_idx].wall_num);
         f_ptr = glb_ds1[ds1_idx].floor_buff  + tf;
         s_ptr = glb_ds1[ds1_idx].shadow_buff + ts;
         w_ptr = glb_ds1[ds1_idx].wall_buff   + tw;

         // floors
         for (i=0; i<glb_ds1[ds1_idx].floor_num; i++)
         {
            if (IS_SELECTED(f_ptr[i].flags))
            {
               f_ptr[i].prop1  = 0;
               f_ptr[i].prop2  = 0;
               f_ptr[i].prop3  = 0;
               f_ptr[i].prop4  = 0;
               f_ptr[i].bt_idx = 0;
               f_ptr[i].flags  = 0;
            }
         }

         // shadows
         for (i=0; i<glb_ds1[ds1_idx].shadow_num; i++)
         {
            if (IS_SELECTED(s_ptr[i].flags))
            {
               s_ptr[i].prop1  = 0;
               s_ptr[i].prop2  = 0;
               s_ptr[i].prop3  = 0;
               s_ptr[i].prop4  = 0;
               s_ptr[i].bt_idx = 0;
               s_ptr[i].flags  = 0;
            }
         }

         // walls
         for (i=0; i<glb_ds1[ds1_idx].wall_num; i++)
         {
            if (IS_SELECTED(w_ptr[i].flags))
            {
               w_ptr[i].prop1       = 0;
               w_ptr[i].prop2       = 0;
               w_ptr[i].prop3       = 0;
               w_ptr[i].prop4       = 0;
               w_ptr[i].orientation = 0;
               w_ptr[i].bt_idx      = 0;
               w_ptr[i].flags       = 0;
            }
         }
      }
   }
}




// ==========================================================================
// =                                                                        =
// =  MODIFY SELECTION MANAGMENT                                            =
// =                                                                        =
// ==========================================================================


// ==========================================================================
// deselect this cell only for the visible layers
void edittile_deselect_one(int ds1_idx, int x, int y)
{
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   CELL_W_S * w_ptr;
   UNDO_S   * u  = & glb_ds1[ds1_idx].undo;
   int      n, tf, ts, tw, make_undo = FALSE;

   
   tf    = (y * glb_ds1[ds1_idx].floor_line)  + (x * glb_ds1[ds1_idx].floor_num);
   ts    = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
   tw    = (y * glb_ds1[ds1_idx].wall_line)   + (x * glb_ds1[ds1_idx].wall_num);
   f_ptr = glb_ds1[ds1_idx].floor_buff  + tf;
   s_ptr = glb_ds1[ds1_idx].shadow_buff + ts;
   w_ptr = glb_ds1[ds1_idx].wall_buff   + tw;
   
   // undo managment
   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
   {
      if (glb_ds1[ds1_idx].floor_layer_mask[n] != 0)
      {
         if (IS_SELECTED(f_ptr[n].flags))
         {
            u->dat_flg.f[n] |= DF_FL;
            make_undo = TRUE;
         }
      }
   }
   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
   {
      if (glb_ds1[ds1_idx].shadow_layer_mask[n] != 0)
      {
         if (IS_SELECTED(s_ptr[n].flags))
         {
            u->dat_flg.s[n] |= DF_FL;
            make_undo = TRUE;
         }
      }
   }
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      if (glb_ds1[ds1_idx].wall_layer_mask[n] != 0)
      {
         if (IS_SELECTED(w_ptr[n].flags))
         {
            u->dat_flg.w[n] |= DF_FL;
            make_undo = TRUE;
         }
      }
   }
   if (make_undo)
      undo_add_to_tile_buffer(ds1_idx, x, y);

   // floors
   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
      if (glb_ds1[ds1_idx].floor_layer_mask[n] != 0)
         DEL_SELECTED(f_ptr[n].flags);

   // shadows
   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
      if (glb_ds1[ds1_idx].shadow_layer_mask[n] != 0)
         DEL_SELECTED(s_ptr[n].flags);

   // walls
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
      if (glb_ds1[ds1_idx].wall_layer_mask[n] != 0)
         DEL_SELECTED(w_ptr[n].flags);
}


// ==========================================================================
// deselect this cell for ALL layers, whatever they're visible or not
void edittile_deselect_one_completly(int ds1_idx, int x, int y)
{
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   CELL_W_S * w_ptr;
   UNDO_S   * u  = & glb_ds1[ds1_idx].undo;
   int      n, tf, ts, tw, make_undo = FALSE;


   tf    = (y * glb_ds1[ds1_idx].floor_line)  + (x * glb_ds1[ds1_idx].floor_num);
   ts    = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
   tw    = (y * glb_ds1[ds1_idx].wall_line)   + (x * glb_ds1[ds1_idx].wall_num);
   f_ptr = glb_ds1[ds1_idx].floor_buff  + tf;
   s_ptr = glb_ds1[ds1_idx].shadow_buff + ts;
   w_ptr = glb_ds1[ds1_idx].wall_buff   + tw;
   
   // undo managment
   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
   {
      if (IS_SELECTED(f_ptr[n].flags))
      {
         u->dat_flg.f[n] |= DF_FL;
         make_undo = TRUE;
      }
   }
   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
   {
      if (IS_SELECTED(s_ptr[n].flags))
      {
         u->dat_flg.s[n] |= DF_FL;
         make_undo = TRUE;
      }
   }
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      if (IS_SELECTED(w_ptr[n].flags))
      {
         u->dat_flg.w[n] |= DF_FL;
         make_undo = TRUE;
      }
   }
   if (make_undo)
      undo_add_to_tile_buffer(ds1_idx, x, y);

   // floors
   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
      DEL_SELECTED(f_ptr[n].flags);

   // shadows
   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
      DEL_SELECTED(s_ptr[n].flags);

   // walls
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
      DEL_SELECTED(w_ptr[n].flags);
}


// ==========================================================================
// select only the visible layers of this cell (leave already selected)
void edittile_select_one(int ds1_idx, int x, int y)
{
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   CELL_W_S * w_ptr;
   UNDO_S   * u  = & glb_ds1[ds1_idx].undo;
   int      n, tf, ts, tw, make_undo = FALSE;


   tf    = (y * glb_ds1[ds1_idx].floor_line)  + (x * glb_ds1[ds1_idx].floor_num);
   ts    = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
   tw    = (y * glb_ds1[ds1_idx].wall_line)   + (x * glb_ds1[ds1_idx].wall_num);
   f_ptr = glb_ds1[ds1_idx].floor_buff  + tf;
   s_ptr = glb_ds1[ds1_idx].shadow_buff + ts;
   w_ptr = glb_ds1[ds1_idx].wall_buff   + tw;
   
   // undo managment
   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
   {
      if ((glb_ds1[ds1_idx].floor_layer_mask[n] != 0) && (f_ptr[n].prop1 != 0))
      {
         if ( ! IS_SELECTED(f_ptr[n].flags))
         {
            u->dat_flg.f[n] |= DF_FL;
            make_undo = TRUE;
         }
      }
   }
   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
   {
      if ((glb_ds1[ds1_idx].shadow_layer_mask[n] != 0) && (s_ptr[n].prop1 != 0))
      {
         if ( ! IS_SELECTED(s_ptr[n].flags))
         {
            u->dat_flg.s[n] |= DF_FL;
            make_undo = TRUE;
         }
      }
   }
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      if ((glb_ds1[ds1_idx].wall_layer_mask[n] != 0) && (w_ptr[n].prop1 != 0))
      {
         if ( ! IS_SELECTED(w_ptr[n].flags))
         {
            u->dat_flg.w[n] |= DF_FL;
            make_undo = TRUE;
         }
      }
   }
   if (make_undo)
      undo_add_to_tile_buffer(ds1_idx, x, y);

   // floors
   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
      if ((glb_ds1[ds1_idx].floor_layer_mask[n] != 0) && (f_ptr[n].prop1 != 0))
         SET_SELECTED(f_ptr[n].flags);

   // shadows
   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
      if ((glb_ds1[ds1_idx].shadow_layer_mask[n] != 0) && (s_ptr[n].prop1 != 0))
         SET_SELECTED(s_ptr[n].flags);

   // walls
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
      if ((glb_ds1[ds1_idx].wall_layer_mask[n] != 0) && (w_ptr[n].prop1 != 0))
         SET_SELECTED(w_ptr[n].flags);
}


// ==========================================================================
// select only the visible layers of this cell (deselect already selected)
void edittile_select_new_one(int ds1_idx, int x, int y)
{
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   CELL_W_S * w_ptr;
   UNDO_S   * u  = & glb_ds1[ds1_idx].undo;
   int      n, tf, ts, tw, make_undo = FALSE;


   tf    = (y * glb_ds1[ds1_idx].floor_line)  + (x * glb_ds1[ds1_idx].floor_num);
   ts    = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
   tw    = (y * glb_ds1[ds1_idx].wall_line)   + (x * glb_ds1[ds1_idx].wall_num);
   f_ptr = glb_ds1[ds1_idx].floor_buff  + tf;
   s_ptr = glb_ds1[ds1_idx].shadow_buff + ts;
   w_ptr = glb_ds1[ds1_idx].wall_buff   + tw;
   
   // undo managment
   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
   {
      if ((glb_ds1[ds1_idx].floor_layer_mask[n] != 0) && (f_ptr[n].prop1 != 0))
      {
         // this floor is visible
         if ( ! IS_SELECTED(f_ptr[n].flags))
         {
            // save the non-selected state of this cell layer
            u->dat_flg.f[n] |= DF_FL;
            make_undo = TRUE;
         }
      }
      else
      {
         // this floor is not visible
         if ( IS_SELECTED(f_ptr[n].flags))
         {
            // save the selected state of this cell layer
            u->dat_flg.f[n] |= DF_FL;
            make_undo = TRUE;
         }
      }
   }
   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
   {
      if ((glb_ds1[ds1_idx].shadow_layer_mask[n] != 0) && (s_ptr[n].prop1 != 0))
      {
         if ( ! IS_SELECTED(s_ptr[n].flags))
         {
            u->dat_flg.s[n] |= DF_FL;
            make_undo = TRUE;
         }
      }
      else
      {
         if ( IS_SELECTED(s_ptr[n].flags))
         {
            u->dat_flg.s[n] |= DF_FL;
            make_undo = TRUE;
         }
      }
   }
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      if ((glb_ds1[ds1_idx].wall_layer_mask[n] != 0) && (w_ptr[n].prop1 != 0))
      {
         if ( ! IS_SELECTED(w_ptr[n].flags))
         {
            u->dat_flg.w[n] |= DF_FL;
            make_undo = TRUE;
         }
      }
      else
      {
         if ( IS_SELECTED(w_ptr[n].flags))
         {
            u->dat_flg.w[n] |= DF_FL;
            make_undo = TRUE;
         }
      }
   }
   if (make_undo)
      undo_add_to_tile_buffer(ds1_idx, x, y);

   // floors
   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
      if ((glb_ds1[ds1_idx].floor_layer_mask[n] != 0) && (f_ptr[n].prop1 != 0))
         SET_SELECTED(f_ptr[n].flags);
      else
         DEL_SELECTED(f_ptr[n].flags);

   // shadows
   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
      if ((glb_ds1[ds1_idx].shadow_layer_mask[n] != 0) && (s_ptr[n].prop1 != 0))
         SET_SELECTED(s_ptr[n].flags);
      else
         DEL_SELECTED(s_ptr[n].flags);

   // walls
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
      if ((glb_ds1[ds1_idx].wall_layer_mask[n] != 0) && (w_ptr[n].prop1 != 0))
         SET_SELECTED(w_ptr[n].flags);
      else
         DEL_SELECTED(w_ptr[n].flags);
}


// ==========================================================================
// select a new "Identical tile" selection, search in ALL layers
void edittile_identical(int ds1_idx, IT_ENUM itype, int tx, int ty)
{
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   CELL_W_S * w_ptr;
   UNDO_S   * u  = & glb_ds1[ds1_idx].undo;
   int      x, y, n, t, i, make_undo, is_same;
   int      fl_ref[FLOOR_MAX_LAYER],
            sh_ref[SHADOW_MAX_LAYER],
            wa_ref[WALL_MAX_LAYER];

   
   // prepare undo
   undo_new_tile_buffer(ds1_idx);
   
   // inits
   for (n=0; n < FLOOR_MAX_LAYER;  n++)  fl_ref[n] = 0;
   for (n=0; n < SHADOW_MAX_LAYER; n++)  sh_ref[n] = 0;
   for (n=0; n < WALL_MAX_LAYER;   n++)  wa_ref[n] = 0;

   // first, search the reference Identical visible tiles

   // floors
   y = 0;
   t = (ty * glb_ds1[ds1_idx].floor_line) + (tx * glb_ds1[ds1_idx].floor_num);
   for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
   {
      if (glb_ds1[ds1_idx].floor_layer_mask[i] != 0)
      {
         f_ptr = glb_ds1[ds1_idx].floor_buff + t + i;
         if (f_ptr->bt_idx >= 1)
         {
            fl_ref[y] = f_ptr->bt_idx;
            y++;
         }
      }
   }
   
   // shadows
   y = 0;
   t = (ty * glb_ds1[ds1_idx].shadow_line) + (tx * glb_ds1[ds1_idx].shadow_num);
   for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
   {
      if (glb_ds1[ds1_idx].shadow_layer_mask[i] != 0)
      {
         s_ptr = glb_ds1[ds1_idx].shadow_buff + t + i;
         if (s_ptr->bt_idx >= 1)
         {
            sh_ref[y] = s_ptr->bt_idx;
            y++;
         }
      }
   }
   
   // walls
   y = 0;
   t = (ty * glb_ds1[ds1_idx].wall_line) + (tx * glb_ds1[ds1_idx].wall_num);
   for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
   {
      if (glb_ds1[ds1_idx].wall_layer_mask[i] != 0)
      {
         w_ptr = glb_ds1[ds1_idx].wall_buff + t + i;
         if (w_ptr->bt_idx >= 1)
         {
            wa_ref[y] = w_ptr->bt_idx;
            y++;
         }
      }
   }

   // now, for ALL tiles of this map (visible or not)
   for (y=0; y < glb_ds1[ds1_idx].height; y++)
   {
      for (x=0; x < glb_ds1[ds1_idx].width; x++)
      {
         // set undo to default "don't save this tile flags"
         make_undo = FALSE;

         // check all layers to see if same as any ref
         
         // floor layers
         t = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
         for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
         {
            f_ptr = glb_ds1[ds1_idx].floor_buff + t + i;
            u->dat_flg.f[i] = 0; // don't save floor[].flag
            
            // is same as one of the ref ?
            is_same = FALSE;
            for (n=0; n < glb_ds1[ds1_idx].floor_num; n++)
            {
               if (fl_ref[n])
               {
                  if (fl_ref[n] == f_ptr->bt_idx)
                  {
                     // same
                     is_same = TRUE;

                     if ((itype == IT_NEW) || (itype == IT_ADD))
                     {
                        // add it to the selection
                        
                        // already selected ?
                        if (IS_SELECTED(f_ptr->flags))
                        {
                           // nothing to do
                        }
                        else
                        {
                           // flag it for the undo
                           u->dat_flg.f[i] |= DF_FL;
                           make_undo = TRUE;

                           // select it
                           SET_SELECTED(f_ptr->flags);
                        }
                     }
                     else if (itype == IT_DEL)
                     {
                        // delete it from selection

                        // already NOT selected ?
                        if ( ! IS_SELECTED(f_ptr->flags))
                        {
                           // nothing to do
                        }
                        else
                        {
                           // flag it for the undo
                           u->dat_flg.f[i] |= DF_FL;
                           make_undo = TRUE;

                           // DEselect it
                           DEL_SELECTED(f_ptr->flags);
                        }
                     }
                  }
               }
            }
            if (is_same == FALSE)
            {
               // different
               
               if (itype == IT_NEW)
               {
                  // delete it from the selection
                        
                  // already NOT selected ?
                  if ( ! IS_SELECTED(f_ptr->flags))
                  {
                     // nothing to do
                  }
                  else
                  {
                     // flag it for the undo
                     u->dat_flg.f[i] |= DF_FL;
                     make_undo = TRUE;

                     // DEselect it
                     DEL_SELECTED(f_ptr->flags);
                  }
               }
            }
         }

         // shadow layers
         t = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
         for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
         {
            s_ptr = glb_ds1[ds1_idx].shadow_buff + t + i;
            u->dat_flg.s[i] = 0; // don't save shadow[].flag
            
            // is same as one of the ref ?
            is_same = FALSE;
            for (n=0; n < glb_ds1[ds1_idx].shadow_num; n++)
            {
               if (sh_ref[n])
               {
                  if (sh_ref[n] == s_ptr->bt_idx)
                  {
                     // same
                     is_same = TRUE;

                     if ((itype == IT_NEW) || (itype == IT_ADD))
                     {
                        // add it to the selection
                        
                        // already selected ?
                        if (IS_SELECTED(s_ptr->flags))
                        {
                           // nothing to do
                        }
                        else
                        {
                           // flag it for the undo
                           u->dat_flg.s[i] |= DF_FL;
                           make_undo = TRUE;

                           // select it
                           SET_SELECTED(s_ptr->flags);
                        }
                     }
                     else if (itype == IT_DEL)
                     {
                        // delete it from selection

                        // already NOT selected ?
                        if ( ! IS_SELECTED(s_ptr->flags))
                        {
                           // nothing to do
                        }
                        else
                        {
                           // flag it for the undo
                           u->dat_flg.s[i] |= DF_FL;
                           make_undo = TRUE;

                           // DEselect it
                           DEL_SELECTED(s_ptr->flags);
                        }
                     }
                  }
               }
            }
            if (is_same == FALSE)
            {
               // different
               
               if (itype == IT_NEW)
               {
                  // delete it from the selection
                        
                  // already NOT selected ?
                  if ( ! IS_SELECTED(s_ptr->flags))
                  {
                     // nothing to do
                  }
                  else
                  {
                     // flag it for the undo
                     u->dat_flg.s[i] |= DF_FL;
                     make_undo = TRUE;

                     // DEselect it
                     DEL_SELECTED(s_ptr->flags);
                  }
               }
            }
         }
         
         // wall layers
         t = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
         for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
         {
            w_ptr = glb_ds1[ds1_idx].wall_buff + t + i;
            u->dat_flg.w[i] = 0; // don't save wall[].flag
            
            // is same as one of the ref ?
            is_same = FALSE;
            for (n=0; n < glb_ds1[ds1_idx].wall_num; n++)
            {
               if (wa_ref[n])
               {
                  if (wa_ref[n] == w_ptr->bt_idx)
                  {
                     // same
                     is_same = TRUE;

                     if ((itype == IT_NEW) || (itype == IT_ADD))
                     {
                        // add it to the selection
                        
                        // already selected ?
                        if (IS_SELECTED(w_ptr->flags))
                        {
                           // nothing to do
                        }
                        else
                        {
                           // flag it for the undo
                           u->dat_flg.w[i] |= DF_FL;
                           make_undo = TRUE;

                           // select it
                           SET_SELECTED(w_ptr->flags);
                        }
                     }
                     else if (itype == IT_DEL)
                     {
                        // delete it from selection

                        // already NOT selected ?
                        if ( ! IS_SELECTED(w_ptr->flags))
                        {
                           // nothing to do
                        }
                        else
                        {
                           // flag it for the undo
                           u->dat_flg.w[i] |= DF_FL;
                           make_undo = TRUE;

                           // DEselect it
                           DEL_SELECTED(w_ptr->flags);
                        }
                     }
                  }
               }
            }
            if (is_same == FALSE)
            {
               // different
               
               if (itype == IT_NEW)
               {
                  // delete it from the selection
                        
                  // already NOT selected ?
                  if ( ! IS_SELECTED(w_ptr->flags))
                  {
                     // nothing to do
                  }
                  else
                  {
                     // flag it for the undo
                     u->dat_flg.w[i] |= DF_FL;
                     make_undo = TRUE;

                     // DEselect it
                     DEL_SELECTED(w_ptr->flags);
                  }
               }
            }
         }
         
         // save this tile for undo ?
         if (make_undo)
            undo_add_to_tile_buffer(ds1_idx, x, y);
      }
   }

   // end
   undo_close_tile_buffer(ds1_idx);
}




// ==========================================================================
// =                                                                        =
// =  HIDE / SHOW (UN-HIDE) MANAGMENT                                       =
// =                                                                        =
// ==========================================================================


// ==========================================================================
// clear the Hide flag of all cells of all layers
void edittile_unhide_all(int ds1_idx)
{
   CELL_F_S * f_ptr = glb_ds1[ds1_idx].floor_buff;
   CELL_S_S * s_ptr = glb_ds1[ds1_idx].shadow_buff;
   CELL_W_S * w_ptr = glb_ds1[ds1_idx].wall_buff;
   int      i;


   // floors
   for (i=0; i<glb_ds1[ds1_idx].floor_len; i++)
   {
      DEL_HIDE(f_ptr->flags);
      f_ptr++;
   }

   // shadows
   for (i=0; i<glb_ds1[ds1_idx].shadow_len; i++)
   {
      DEL_HIDE(s_ptr->flags);
      s_ptr++;
   }

   // walls
   for (i=0; i<glb_ds1[ds1_idx].wall_len; i++)
   {
      DEL_HIDE(w_ptr->flags);
      w_ptr++;
   }
}


// ==========================================================================
// hide only the visible layers of 1 cell
void edittile_hide_add_one(int ds1_idx, int x, int y)
{
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   CELL_W_S * w_ptr;
   int      n, tf, ts, tw;


   tf    = (y * glb_ds1[ds1_idx].floor_line)  + (x * glb_ds1[ds1_idx].floor_num);
   ts    = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
   tw    = (y * glb_ds1[ds1_idx].wall_line)   + (x * glb_ds1[ds1_idx].wall_num);
   f_ptr = glb_ds1[ds1_idx].floor_buff  + tf;
   s_ptr = glb_ds1[ds1_idx].shadow_buff + ts;
   w_ptr = glb_ds1[ds1_idx].wall_buff   + tw;
   
   // floors
   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
   {
      if (glb_ds1[ds1_idx].floor_layer_mask[n] != 0)
         SET_HIDE(f_ptr->flags);
      f_ptr++;
   }

   // shadows
   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
   {
      if (glb_ds1[ds1_idx].shadow_layer_mask[n] != 0)
         SET_HIDE(s_ptr->flags);
      s_ptr++;
   }

   // walls
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      if (glb_ds1[ds1_idx].wall_layer_mask[n] != 0)
         SET_HIDE(w_ptr->flags);
      w_ptr++;
   }
}




// ==========================================================================
// =                                                                        =
// =  TEMP SELECTION MANAGMENT                                              =
// =                                                                        =
// =  user is making a selection with the mouse, it's a temp one because    =
// =  the mouse button is still hold, and this is the key pressed at the    =
// =  end that will tell what to do with it, either add it to the current   =
// =  selection / delete it / hide it ...                                   =
// =                                                                        =
// =  theses functions are using the above ones to do their job             =
// =                                                                        =
// ==========================================================================


// ==========================================================================
// clear the TmpSelection flag of all cells
void edittile_delete_all_tmpsel(int ds1_idx)
{
   CELL_F_S * f_ptr = glb_ds1[ds1_idx].floor_buff;
   CELL_S_S * s_ptr = glb_ds1[ds1_idx].shadow_buff;
   CELL_W_S * w_ptr = glb_ds1[ds1_idx].wall_buff;
   int      i;


   // floors
   for (i=0; i<glb_ds1[ds1_idx].floor_len; i++)
   {
      DEL_TMP_SELECT(f_ptr->flags);
      f_ptr++;
   }

   // shadows
   for (i=0; i<glb_ds1[ds1_idx].shadow_len; i++)
   {
      DEL_TMP_SELECT(s_ptr->flags);
      s_ptr++;
   }

   // walls
   for (i=0; i<glb_ds1[ds1_idx].wall_len; i++)
   {
      DEL_TMP_SELECT(w_ptr->flags);
      w_ptr++;
   }
}


// ==========================================================================
// set the TmpSelection flag of 1 cell, only for the visible layers
void edittile_add_tmpsel_one(int ds1_idx, int x, int y)
{
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   CELL_W_S * w_ptr;
   int      n, tf, ts, tw;
   

   tf    = (y * glb_ds1[ds1_idx].floor_line)  + (x * glb_ds1[ds1_idx].floor_num);
   ts    = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
   tw    = (y * glb_ds1[ds1_idx].wall_line)   + (x * glb_ds1[ds1_idx].wall_num);
   f_ptr = glb_ds1[ds1_idx].floor_buff  + tf;
   s_ptr = glb_ds1[ds1_idx].shadow_buff + ts;
   w_ptr = glb_ds1[ds1_idx].wall_buff   + tw;
   
   // floors
   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
   {
      if (glb_ds1[ds1_idx].floor_layer_mask[n] != 0)
         SET_TMP_SELECT(f_ptr->flags);
      f_ptr++;
   }

   // shadows
   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
   {
      if (glb_ds1[ds1_idx].shadow_layer_mask[n] != 0)
         SET_TMP_SELECT(s_ptr->flags);
      s_ptr++;
   }

   // walls
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      if (glb_ds1[ds1_idx].wall_layer_mask[n] != 0)
         SET_TMP_SELECT(w_ptr->flags);
      w_ptr++;
   }
}


// ==========================================================================
// make the current temp selection
void edittile_set_tmpsel(int ds1_idx, TMP_SEL_S * s)
{
   int x, y, x1, y1, x2, y2;


   x1 = s->x1;
   x2 = s->x2;
   if (s->x2 < s->x1)
   {
      x1 = s->x2;
      x2 = s->x1;
   }
   y1 = s->y1;
   y2 = s->y2;
   if (s->y2 < s->y1)
   {
      y1 = s->y2;
      y2 = s->y1;
   }

   for (y = y1; y <= y2; y++)
      for (x = x1; x <= x2; x++)
         edittile_add_tmpsel_one(ds1_idx, x, y);
}


// ==========================================================================
// change the Temp selection to be a New selected selection
void edittile_change_to_new_permanent_sel(int ds1_idx, TMP_SEL_S * s)
{
   int x, y, x1, y1, x2, y2;


   x1 = s->x1;
   x2 = s->x2;
   if (s->x2 < s->x1)
   {
      x1 = s->x2;
      x2 = s->x1;
   }
   y1 = s->y1;
   y2 = s->y2;
   if (s->y2 < s->y1)
   {
      y1 = s->y2;
      y2 = s->y1;
   }

   undo_new_tile_buffer(ds1_idx);
   for (y=0; y < glb_ds1[ds1_idx].height; y++)
   {
      for (x=0; x < glb_ds1[ds1_idx].width; x++)
      {
         if ((x >= x1) && (x <= x2) && (y >= y1) && (y <= y2))
            edittile_select_new_one(ds1_idx, x, y);
         else
            edittile_deselect_one_completly(ds1_idx, x, y);
      }
   }
   undo_close_tile_buffer(ds1_idx);
}


// ==========================================================================
// change the Temp selection to be an addition of the current selected tiles
void edittile_change_to_add_permanent_sel(int ds1_idx, TMP_SEL_S * s)
{
   int x, y, x1, y1, x2, y2;


   x1 = s->x1;
   x2 = s->x2;
   if (s->x2 < s->x1)
   {
      x1 = s->x2;
      x2 = s->x1;
   }
   y1 = s->y1;
   y2 = s->y2;
   if (s->y2 < s->y1)
   {
      y1 = s->y2;
      y2 = s->y1;
   }

   undo_new_tile_buffer(ds1_idx);
   for (y=0; y < glb_ds1[ds1_idx].height; y++)
      for (x=0; x < glb_ds1[ds1_idx].width; x++)
         if ((x >= x1) && (x <= x2) && (y >= y1) && (y <= y2))
            edittile_select_one(ds1_idx, x, y);
   undo_close_tile_buffer(ds1_idx);
}


// ==========================================================================
// hide all tiles of the Temp selection
void edittile_change_to_hide_sel(int ds1_idx, TMP_SEL_S * s)
{
   int x, y, x1, y1, x2, y2;


   x1 = s->x1;
   x2 = s->x2;
   if (s->x2 < s->x1)
   {
      x1 = s->x2;
      x2 = s->x1;
   }
   y1 = s->y1;
   y2 = s->y2;
   if (s->y2 < s->y1)
   {
      y1 = s->y2;
      y2 = s->y1;
   }

   for (y = y1; y <= y2; y++)
      for (x = x1; x <= x2; x++)
         edittile_hide_add_one(ds1_idx, x, y);
}


// ==========================================================================
// deselect the tiles of the Temp selection
void edittile_change_to_del_sel(int ds1_idx, TMP_SEL_S * s)
{
   int x, y, x1, y1, x2, y2;


   x1 = s->x1;
   x2 = s->x2;
   if (s->x2 < s->x1)
   {
      x1 = s->x2;
      x2 = s->x1;
   }
   y1 = s->y1;
   y2 = s->y2;
   if (s->y2 < s->y1)
   {
      y1 = s->y2;
      y2 = s->y1;
   }

   undo_new_tile_buffer(ds1_idx);
   for (y = y1; y <= y2; y++)
      for (x = x1; x <= x2; x++)
         edittile_deselect_one(ds1_idx, x, y);
   undo_close_tile_buffer(ds1_idx);
}
