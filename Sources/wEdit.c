#include "structs.h"
#include "wPreview.h"
#include "error.h"
#include "undo.h"
#include "misc.h"
#include "wEdit.h"


// ==========================================================================
// draw shadow layer
void wedit_draw_s(int ds1_idx, int x, int y)
{
   WIN_ELEMENT_S * w_elm = & glb_ds1edit.win_edit.w_preview;
   BITMAP        * sub;
   BLOCK_TABLE_S * bt_ptr;
   BITMAP        * tmp_bmp;
   CELL_S_S      * s_ptr;
   int           n, t, bt_idx, dt1_idx, block_idx, y1;
   int           ux1, ux2, ux3, ux4, uy1, uy2, uy3;

   
   t     = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
   s_ptr = glb_ds1[ds1_idx].shadow_buff + t;
   sub   = create_sub_bitmap(glb_ds1edit.screen_buff,
              w_elm->i_x0, w_elm->i_y0, w_elm->inside->w, w_elm->inside->h);

   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
   {
      bt_idx = s_ptr[n].bt_idx; // index in block table
      if (bt_idx == -1)
      {
         ux1 = 0;
         ux2 = ux1 + 79;
         ux3 = ux1 + 80;
         ux4 = ux1 + 159;

         uy1 = sub->h - 160;
         uy2 = uy1 + 39;
         uy3 = uy1 + 78;

         line(sub, ux1, uy2, ux2, uy1, 168);
         line(sub, ux3, uy1, ux4, uy2, 168);
         line(sub, ux3, uy3, ux4, uy2, 168);
         line(sub, ux1, uy2, ux2, uy3, 168);

         textprintf(sub, font, ux1+8, uy2-2, 0,
            "%u %u %u %u",
            s_ptr[n].prop1,
            s_ptr[n].prop2,
            s_ptr[n].prop3,
            s_ptr[n].prop4
         );
         textprintf(sub, font, ux1+7, uy2-3, 255,
            "%u %u %u %u",
            s_ptr[n].prop1,
            s_ptr[n].prop2,
            s_ptr[n].prop3,
            s_ptr[n].prop4
         );
      }
      if (bt_idx <= 0)
         continue;

      bt_ptr = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table
      if (bt_ptr->type != BT_SHADOW)
         continue; // only shadows
      
      dt1_idx   = bt_ptr->dt1_idx;
      block_idx = bt_ptr->block_idx;
      tmp_bmp   = * (glb_dt1[dt1_idx].block_zoom[ZM_11] + block_idx);

      if (tmp_bmp == NULL)
         continue;

      y1 = sub->h - 160 - bt_ptr->zero_line;
      y1 += 80; // shadows, like walls are lower than floors
                // (and than roofs) by 80 pixels

      switch(glb_ds1[ds1_idx].shadow_layer_mask[0])
      {
         case 1 :
            draw_sprite(sub, tmp_bmp, 0, y1);
            break;
            
         case 2 :
            if (glb_ds1edit.cmd_line.force_pal_num == -1)
               color_map = & glb_ds1edit.cmap[CM_SELECT][glb_ds1[ds1_idx].act - 1];
            else
               color_map = & glb_ds1edit.cmap[CM_SELECT][glb_ds1edit.cmd_line.force_pal_num - 1];
            draw_lit_sprite(sub, tmp_bmp, 0, y1, COL_SHADOW);
            break;
         
         case 3 :
            if (glb_ds1edit.cmd_line.force_pal_num == -1)
               color_map = & glb_ds1edit.cmap[CM_TRANS][glb_ds1[ds1_idx].act - 1];
            else
               color_map = & glb_ds1edit.cmap[CM_TRANS][glb_ds1edit.cmd_line.force_pal_num - 1];
            draw_trans_sprite(sub, tmp_bmp, 0, y1);
            break;
      }
   }
   destroy_bitmap(sub);
}


// ==========================================================================
// draw floor layers
void wedit_draw_f(int ds1_idx, int x, int y)
{
   WIN_ELEMENT_S * w_elm = & glb_ds1edit.win_edit.w_preview;
   BITMAP        * sub;
   BLOCK_TABLE_S * bt_ptr;
   BITMAP        * tmp_bmp;
   CELL_F_S      * f_ptr;
   int           n, t, bt_idx, dt1_idx, block_idx, y1;
   ORDER_DATA_S  order_data[4];
   int           ux1, ux2, ux3, ux4, uy1, uy2, uy3;

   
   t     = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
   f_ptr = glb_ds1[ds1_idx].floor_buff + t;
   sub   = create_sub_bitmap(glb_ds1edit.screen_buff,
              w_elm->i_x0, w_elm->i_y0, w_elm->inside->w, w_elm->inside->h);

   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
   {
      order_data[n].idx    = n;
      order_data[n].height = f_ptr[n].prop1;
   }
   qsort(order_data, glb_ds1[ds1_idx].floor_num, sizeof(ORDER_DATA_S),
      qsort_helper_order_data);

   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
   {
      bt_idx = f_ptr[order_data[n].idx].bt_idx; // index in block table
      if (bt_idx == -1)
      {
         ux1 = 0;
         ux2 = ux1 + 79;
         ux3 = ux1 + 80;
         ux4 = ux1 + 159;

         uy1 = sub->h - 160;
         uy2 = uy1 + 39;
         uy3 = uy1 + 78;

         line(sub, ux1, uy2, ux2, uy1, 168);
         line(sub, ux3, uy1, ux4, uy2, 168);
         line(sub, ux3, uy3, ux4, uy2, 168);
         line(sub, ux1, uy2, ux2, uy3, 168);

         textprintf(sub, font, ux1+8, uy2-2, 0,
            "%u %u %u %u",
            f_ptr[order_data[n].idx].prop1,
            f_ptr[order_data[n].idx].prop2,
            f_ptr[order_data[n].idx].prop3,
            f_ptr[order_data[n].idx].prop4
         );
         textprintf(sub, font, ux1+7, uy2-3, 255,
            "%u %u %u %u",
            f_ptr[order_data[n].idx].prop1,
            f_ptr[order_data[n].idx].prop2,
            f_ptr[order_data[n].idx].prop3,
            f_ptr[order_data[n].idx].prop4
         );
      }
      if (bt_idx <= 0)
         continue;

      bt_ptr = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table
      if ( (bt_ptr->type != BT_STATIC) && (bt_ptr->type != BT_ANIMATED) )
         continue; // only floors

      dt1_idx   = bt_ptr->dt1_idx;
      block_idx = bt_ptr->block_idx;
      tmp_bmp   = * (glb_dt1[dt1_idx].block_zoom[ZM_11] + block_idx);

      if (tmp_bmp == NULL)
         continue;

      y1 = sub->h - 160 - bt_ptr->zero_line;
      draw_sprite(sub, tmp_bmp, 0, y1);
   }
   destroy_bitmap(sub);
}


// ==========================================================================
// draw wall layers
void wedit_draw_w(int ds1_idx, int x, int y, int upper)
{
   WIN_ELEMENT_S * w_elm = & glb_ds1edit.win_edit.w_preview;
   BITMAP        * sub;
   BLOCK_TABLE_S * bt_ptr;
   BITMAP        * tmp_bmp;
   CELL_W_S      * w_ptr;
   int           n, t, bt_idx, dt1_idx, block_idx, m, s, y1;
   int           done, found, o;
   ORDER_DATA_S  order_data[4];
   int           ux1, ux2, ux3, ux4, uy1, uy2, uy3;
   
   
   t     = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
   w_ptr = glb_ds1[ds1_idx].wall_buff + t;
   sub   = create_sub_bitmap(glb_ds1edit.screen_buff,
              w_elm->i_x0, w_elm->i_y0, w_elm->inside->w, w_elm->inside->h);
   
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      order_data[n].idx    = n;
      order_data[n].height = w_ptr[n].prop1;
      if (w_ptr[n].orientation == 10)
         order_data[n].height = 255;
   }
   qsort(order_data, glb_ds1[ds1_idx].wall_num, sizeof(ORDER_DATA_S),
      qsort_helper_order_data);

   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      o = w_ptr[order_data[n].idx].orientation;
      if ((upper == TRUE) && (o >= 15))
         continue;
      if ((upper == FALSE) && (o <= 15))
         continue;

      if (glb_ds1[ds1_idx].special_layer_mask && ((o == 10) || (o == 11)))
      {
         // special tile asked to draw later
         continue;
      }
      
      bt_idx = w_ptr[order_data[n].idx].bt_idx; // index in block table

      if (bt_idx == 0)
         continue; // no tiles here
      
      if (w_ptr[order_data[n].idx].prop4 & 0x80) // binary : 1000-0000
      {
         // hidden
         if ((o != 10) && (o != 11))
            bt_idx = -1; // a hidden floor --> "unknown"
      }

      if (bt_idx == -1)
      {
         ux1 = 0;
         ux2 = ux1 + 79;
         ux3 = ux1 + 80;
         ux4 = ux1 + 159;

         uy1 = sub->h - 160;
         uy2 = uy1 + 39;
         uy3 = uy1 + 78;

         line(sub, ux1, uy2, ux2, uy1, 168);
         line(sub, ux3, uy1, ux4, uy2, 168);
         line(sub, ux3, uy3, ux4, uy2, 168);
         line(sub, ux1, uy2, ux2, uy3, 168);
         
         textprintf(sub, font, ux1+8, uy2-2, 0,
            "(%u) %u %u %u %u",
            w_ptr[order_data[n].idx].orientation,
            w_ptr[order_data[n].idx].prop1,
            w_ptr[order_data[n].idx].prop2,
            w_ptr[order_data[n].idx].prop3,
            w_ptr[order_data[n].idx].prop4
         );
         textprintf(sub, font, ux1+7, uy2-3, 255,
            "(%u) %u %u %u %u",
            w_ptr[order_data[n].idx].orientation,
            w_ptr[order_data[n].idx].prop1,
            w_ptr[order_data[n].idx].prop2,
            w_ptr[order_data[n].idx].prop3,
            w_ptr[order_data[n].idx].prop4
         );
         continue;
      }

      bt_ptr = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table
      if ( (bt_ptr->type != BT_WALL_UP)   &&
           (bt_ptr->type != BT_WALL_DOWN) &&
           (bt_ptr->type != BT_SPECIAL))
      {
         // only walls or special tiles, but no roof
         continue;
      }
      
      dt1_idx   = bt_ptr->dt1_idx;
      block_idx = bt_ptr->block_idx;

      tmp_bmp = * (glb_dt1[dt1_idx].block_zoom[ZM_11] + block_idx);

      if (tmp_bmp == NULL)
         continue;

      y1 = sub->h - 160 - bt_ptr->zero_line;
      y1 += 80; // walls are lower than floors (and than roofs) by 80 pixels
      draw_sprite(sub, tmp_bmp, 0, y1);
      
      if (bt_ptr->orientation == 3) // upper-left corner
      {
         // search the o=4 m=m s=s
         m = bt_ptr->main_index;
         s = bt_ptr->sub_index;
         done  = FALSE;
         found = FALSE;
         while( ! done)
         {
            if (bt_idx >= glb_ds1[ds1_idx].bt_num)
               done = TRUE;
            else
            {
               bt_ptr++;
               if (bt_ptr->orientation < 4)
                  bt_idx++;
               else
               {
                  if (bt_ptr->orientation == 4)
                     if ((bt_ptr->main_index == m) && (bt_ptr->sub_index == s))
                        done = found = TRUE;
               }
            }
         }
         if (found == TRUE)
         {
            dt1_idx   = bt_ptr->dt1_idx;
            block_idx = bt_ptr->block_idx;
            tmp_bmp   = * (glb_dt1[dt1_idx].block_zoom[ZM_11] + block_idx);

            if (tmp_bmp == NULL)
               continue;

            y1 = sub->h - 160 - bt_ptr->zero_line;
            y1 += 80;
            draw_sprite(sub, tmp_bmp, 0, y1);
         }
      }
   }
   destroy_bitmap(sub);
}

// ==========================================================================
// draw roof layers
void wedit_draw_r(int ds1_idx, int x, int y)
{
   WIN_ELEMENT_S * w_elm = & glb_ds1edit.win_edit.w_preview;
   BITMAP        * sub;
   BLOCK_TABLE_S * bt_ptr;
   BITMAP        * tmp_bmp;
   CELL_W_S      * r_ptr;
   int           n, t, bt_idx, dt1_idx, block_idx, y1;
   ORDER_DATA_S  order_data[4];
   
   t     = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
   r_ptr = glb_ds1[ds1_idx].wall_buff + t;
   sub   = create_sub_bitmap(glb_ds1edit.screen_buff,
              w_elm->i_x0, w_elm->i_y0, w_elm->inside->w, w_elm->inside->h);
   
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      order_data[n].idx    = n;
      order_data[n].height = r_ptr[n].prop1;
   }
   qsort(order_data, glb_ds1[ds1_idx].wall_num, sizeof(ORDER_DATA_S),
      qsort_helper_order_data);
   
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      if (r_ptr[order_data[n].idx].prop4 & 0x80) // binary : 1000-0000
         continue; // don't draw this tile
      
      bt_idx = r_ptr[order_data[n].idx].bt_idx; // index in block table
      if (bt_idx <= 0)
         continue;

      bt_ptr = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table
      if (bt_ptr->type != BT_ROOF)
         continue; // only roof

      dt1_idx   = bt_ptr->dt1_idx;
      block_idx = bt_ptr->block_idx;
      tmp_bmp   = * (glb_dt1[dt1_idx].block_zoom[ZM_11] + block_idx);

      if (tmp_bmp == NULL)
         continue;

      y1 = sub->h - 160 - bt_ptr->zero_line;
      y1 -= bt_ptr->roof_y;
      draw_sprite(sub, tmp_bmp, 0, y1);
   }
   destroy_bitmap(sub);
}


// ==========================================================================
// draw special tiles
void wedit_draw_sp(int ds1_idx, int x, int y)
{
   WIN_ELEMENT_S * w_elm = & glb_ds1edit.win_edit.w_preview;
   BITMAP        * sub;
   BLOCK_TABLE_S * bt_ptr;
   BITMAP        * tmp_bmp;
   CELL_W_S      * w_ptr;
   int           n, t, bt_idx, dt1_idx, block_idx, y1, o;
   ORDER_DATA_S  order_data[4];
   int           ux1, ux2, ux3, ux4, uy1, uy2, uy3;
   
   
   t     = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
   w_ptr = glb_ds1[ds1_idx].wall_buff + t;
   sub   = create_sub_bitmap(glb_ds1edit.screen_buff,
              w_elm->i_x0, w_elm->i_y0, w_elm->inside->w, w_elm->inside->h);
   
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      order_data[n].idx    = n;
      order_data[n].height = w_ptr[n].prop1;
      if (w_ptr[n].orientation == 10)
         order_data[n].height = 255;
   }
   qsort(order_data, glb_ds1[ds1_idx].wall_num, sizeof(ORDER_DATA_S),
      qsort_helper_order_data);

   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      bt_idx = w_ptr[order_data[n].idx].bt_idx; // index in block table

      if (bt_idx == 0) // no tile here
         continue;

      o = w_ptr[order_data[n].idx].orientation;

      if ((o != 10) && (o != 11))
      {
         // not a special tile
         continue;
      }
      else
      {
         // special tile

         if (bt_idx <= 0)
         {
            // unknown special tile
            ux1 = 0;
            ux2 = ux1 + 79;
            ux3 = ux1 + 80;
            ux4 = ux1 + 159;

            uy1 = sub->h - 160;
            uy2 = uy1 + 39;
            uy3 = uy1 + 78;

            line(glb_ds1edit.screen_buff, ux1, uy2, ux2, uy1, 168);
            line(glb_ds1edit.screen_buff, ux3, uy1, ux4, uy2, 168);
            line(glb_ds1edit.screen_buff, ux3, uy3, ux4, uy2, 168);
            line(glb_ds1edit.screen_buff, ux1, uy2, ux2, uy3, 168);
         
            textprintf(glb_ds1edit.screen_buff, font, ux1+8, uy2-2, 0,
               "(%u) %u %u %u %u",
               w_ptr[order_data[n].idx].orientation,
               w_ptr[order_data[n].idx].prop1,
               w_ptr[order_data[n].idx].prop2,
               w_ptr[order_data[n].idx].prop3,
               w_ptr[order_data[n].idx].prop4
            );
            textprintf(glb_ds1edit.screen_buff, font, ux1+7, uy2-3, 255,
               "(%u) %u %u %u %u",
               w_ptr[order_data[n].idx].orientation,
               w_ptr[order_data[n].idx].prop1,
               w_ptr[order_data[n].idx].prop2,
               w_ptr[order_data[n].idx].prop3,
               w_ptr[order_data[n].idx].prop4
            );
            continue;
         }
      }
      
      // draw only special tiles
      bt_ptr = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table
      if (bt_ptr->type != BT_SPECIAL)
         continue; // only special tiles

      dt1_idx   = bt_ptr->dt1_idx;
      block_idx = bt_ptr->block_idx;

      tmp_bmp = * (glb_dt1[dt1_idx].block_zoom[ZM_11] + block_idx);

      if (tmp_bmp == NULL)
         continue;

      y1 = sub->h - 160 - bt_ptr->zero_line;
      y1 += 80; // walls are lower than floors (and than roofs) by 80 pixels
      draw_sprite(sub, tmp_bmp, 0, y1);
   }
   destroy_bitmap(sub);
}


// ==========================================================================
// make a button gfx at run-time, an activated button
void wedit_make_2nd_buttons(void)
{
   BUTTON_S * b;
   BITMAP   * tmp;
   int      i;

   for (i = BU_NULL+1; i<BU_MAX; i++)
   {
      b = & glb_ds1edit.win_edit.button[i];
      tmp = create_bitmap(b->w, b->h);
      if (tmp == NULL)
         ds1edit_error("not enough mem for 1 button");
      b->bmp[1] = tmp;
      clear_to_color(tmp, 172); // almost black (but NOT color of index 0)
      blit(b->bmp[0], tmp, 0, 0, 2, 2, b->w, b->h);
   }
}


// ==========================================================================
// read all pcx and prepare some windows
void wedit_read_pcx(void)
{
   WIN_ELEMENT_S * w_elm;
   BITMAP        * tmp_bmp;
   BITMAP        * pcx[9];
   BUTTON_S      * b;
   static struct
   {
      char      file[30];
      BUT_TYP_E b_num;
      BLK_TYP_E t_num;
      int       x0;
      int       y0;
   } data[] = {
        {"but_ok",      BU_OK,        BT_NULL,        8,  8},
        {"but_cancel",  BU_CANCEL,    BT_NULL,       83,  8},
        {"but_floor1",  BU_FLOOR1,    BT_NULL,      186, 38},
        {"but_floor2",  BU_FLOOR2,    BT_NULL,      257, 38},
        {"but_shadow",  BU_SHADOW,    BT_NULL,      470, 38},
        {"but_wall1",   BU_WALL1,     BT_NULL,      186,  8},
        {"but_wall2",   BU_WALL2,     BT_NULL,      257,  8},
        {"but_wall3",   BU_WALL3,     BT_NULL,      328,  8},
        {"but_wall4",   BU_WALL4,     BT_NULL,      399,  8},
        {"view_rand",   BU_VIEW_RAND, BT_NULL,      470,  8},
        {"tab_static",  BU_NULL,      BT_STATIC,     -1, -1},
        {"tab_anim",    BU_NULL,      BT_ANIMATED,   -1, -1},
        {"tab_upper",   BU_NULL,      BT_WALL_UP,    -1, -1},
        {"tab_lower",   BU_NULL,      BT_WALL_DOWN,  -1, -1},
        {"tab_roofs",   BU_NULL,      BT_ROOF,       -1, -1},
        {"tab_special", BU_NULL,      BT_SPECIAL,    -1, -1},
        {"tab_shadow",  BU_NULL,      BT_SHADOW,     -1, -1},
        {"", BU_NULL, BT_NULL, -1, -1} // do not remove
     };
   char filename[80];
   char pcx_name[9][20] = {
           {"up-left"},   {"up"},   {"up-right"},
           {"left"},      {"fill"}, {"right"},
           {"down-left"}, {"down"}, {"down-right"}
        };
   int  i=0, n1, n2, x0, y0, w, h, my_border, x, y;

   fprintf(stderr, "loading pcx");
   fflush(stderr);
   while (strlen(data[i].file) != 0)
   {
      fprintf(stderr, ".");
      fflush(stderr);
      n1 = data[i].b_num;
      n2 = data[i].t_num;
      if (n1 != BU_NULL)
      {
         // button off
         b = & glb_ds1edit.win_edit.button[n1];
         sprintf(filename, "pcx/%s.pcx", data[i].file);
         b->bmp[0] = load_pcx(filename, glb_ds1edit.dummy_pal);
         b->w = b->bmp[0]->w;
         b->h = b->bmp[0]->h;
         b->x0 = data[i].x0;
         b->y0 = data[i].y0;
      }
      if (n2 != BT_NULL)
      {
         // tab off
         b = & glb_ds1edit.win_edit.tab[n2];
         sprintf(filename, "pcx/%s_off.pcx", data[i].file);
         b->bmp[0] = load_pcx(filename, glb_ds1edit.dummy_pal);
         b->w = b->bmp[0]->w;
         b->h = b->bmp[0]->h;

          // tab on
         sprintf(filename, "pcx/%s.pcx", data[i].file);
         b->bmp[1] = load_pcx(filename, glb_ds1edit.dummy_pal);
      }
      i++;
   }

   // make preview window
   for (i=0; i<9; i++)
   {
      fprintf(stderr, ".");
      fflush(stderr);
      sprintf(filename, "pcx/preview/%s.pcx", pcx_name[i]);
      pcx[i] = load_pcx(filename, glb_ds1edit.dummy_pal);
   }
   my_border = 8;
   x0 = my_border;
   y0 = 38;
   w = 160 + pcx[0]->w + pcx[2]->w;
   h = glb_config.screen.height - y0 - my_border;

   tmp_bmp = create_bitmap(w, h);

   clear(tmp_bmp);

   for (x=0; x<w; x += pcx[1]->w)
      blit(pcx[1], tmp_bmp, 0, 0, x, 0, pcx[1]->w, pcx[1]->h);
      
   for (x=0; x<w; x += pcx[7]->w)
      blit(pcx[7], tmp_bmp, 0, 0, x, h - pcx[7]->h, pcx[7]->w, pcx[7]->h);
      
   for (y=0; y<h; y += pcx[3]->h)
      blit(pcx[3], tmp_bmp, 0, 0, 0, y, pcx[3]->w, pcx[3]->h);
      
   for (y=0; y<h; y += pcx[5]->h)
      blit(pcx[5], tmp_bmp, 0, 0, w-pcx[5]->w, y, pcx[5]->w, pcx[5]->h);

   blit(pcx[0], tmp_bmp, 0, 0, 0,                     0, pcx[0]->w, pcx[0]->h);
   blit(pcx[2], tmp_bmp, 0, 0, w-pcx[2]->w,           0, pcx[2]->w, pcx[2]->h);
   blit(pcx[6], tmp_bmp, 0, 0, 0,           h-pcx[6]->h, pcx[6]->w, pcx[6]->h);
   blit(pcx[8], tmp_bmp, 0, 0, w-pcx[8]->w, h-pcx[8]->h, pcx[8]->w, pcx[8]->h);

   w_elm = & glb_ds1edit.win_edit.w_preview;
   w_elm->border = get_rle_sprite(tmp_bmp);
   w_elm->b_x0 = x0;
   w_elm->b_y0 = y0;

   for (y=0; y<h; y += pcx[4]->h)
      for (x=0; x<w; x += pcx[4]->w)
         blit(pcx[4], tmp_bmp, 0, 0, x, y, pcx[4]->w, pcx[4]->h);

   w_elm->inside = create_bitmap(w - pcx[3]->w * 2, h - pcx[1]->h * 2);
   blit(tmp_bmp, w_elm->inside, pcx[3]->w, pcx[1]->h, 0, 0, w - pcx[3]->w * 2, h - pcx[1]->h * 2);
   w_elm->i_x0 = w_elm->b_x0 + pcx[3]->w;
   w_elm->i_y0 = w_elm->b_y0 + pcx[1]->h;
   
   for (i=0; i<9; i++)
      destroy_bitmap(pcx[i]);
   destroy_bitmap(tmp_bmp);
   

   // make tiles window
   for (i=0; i<9; i++)
   {
      fprintf(stderr, ".");
      fflush(stderr);
      sprintf(filename, "pcx/tiles/%s.pcx", pcx_name[i]);
      pcx[i] = load_pcx(filename, glb_ds1edit.dummy_pal);
   }
   x0 = w_elm->b_x0 + w_elm->border->w + my_border;
   y0 = 119;
   w = glb_config.screen.width  - x0 - my_border;
   h = glb_config.screen.height - y0 - my_border;
   tmp_bmp = create_bitmap(w, h);
   clear(tmp_bmp);

   for (x=0; x<w; x += pcx[1]->w)
      blit(pcx[1], tmp_bmp, 0, 0, x, 0, pcx[1]->w, pcx[1]->h);
      
   for (x=0; x<w; x += pcx[7]->w)
      blit(pcx[7], tmp_bmp, 0, 0, x, h - pcx[7]->h, pcx[7]->w, pcx[7]->h);
      
   for (y=0; y<h; y += pcx[3]->h)
      blit(pcx[3], tmp_bmp, 0, 0, 0, y, pcx[3]->w, pcx[3]->h);
      
   for (y=0; y<h; y += pcx[5]->h)
      blit(pcx[5], tmp_bmp, 0, 0, w-pcx[5]->w, y, pcx[5]->w, pcx[5]->h);

   blit(pcx[0], tmp_bmp, 0, 0, 0,                     0, pcx[0]->w, pcx[0]->h);
   blit(pcx[2], tmp_bmp, 0, 0, w-pcx[2]->w,           0, pcx[2]->w, pcx[2]->h);
   blit(pcx[6], tmp_bmp, 0, 0, 0,           h-pcx[6]->h, pcx[6]->w, pcx[6]->h);
   blit(pcx[8], tmp_bmp, 0, 0, w-pcx[8]->w, h-pcx[8]->h, pcx[8]->w, pcx[8]->h);

   w_elm = & glb_ds1edit.win_edit.w_tiles;
   w_elm->border = get_rle_sprite(tmp_bmp);
   w_elm->b_x0 = x0;
   w_elm->b_y0 = y0;

   for (y=0; y<h; y += pcx[4]->h)
      for (x=0; x<w; x += pcx[4]->w)
         blit(pcx[4], tmp_bmp, 0, 0, x, y, pcx[4]->w, pcx[4]->h);

   w_elm->inside = create_bitmap(w - pcx[3]->w * 2, h - pcx[1]->h * 2);
   blit(tmp_bmp, w_elm->inside, pcx[3]->w, pcx[1]->h, 0, 0, w - pcx[3]->w * 2, h - pcx[1]->h * 2);
   w_elm->i_x0 = w_elm->b_x0 + pcx[3]->w;
   w_elm->i_y0 = w_elm->b_y0 + pcx[1]->h;
   
   for (i=0; i<9; i++)
      destroy_bitmap(pcx[i]);
   destroy_bitmap(tmp_bmp);

   fprintf(stderr, "\n");
   fflush(stderr);
}


// ==========================================================================
// end of 'tile edit window', free its mem
void wedit_free(void)
{
   WIN_ELEMENT_S * w_elm;
   BUTTON_S      * b, * t;
   int           i, n;

   // buttons
   for (i = BU_NULL+1; i<BU_MAX; i++)
   {
      b = & glb_ds1edit.win_edit.button[i];
      for (n=0; n<2; n++)
      {
         if (b->bmp[n] != NULL)
         {
            destroy_bitmap(b->bmp[n]);
            b->bmp[n] = NULL;
         }
      }
   }

   // tabs
   for (i = BT_NULL+1; i<BT_MAX; i++)
   {
      t = & glb_ds1edit.win_edit.tab[i];
      for (n=0; n<2; n++)
      {
         if (t->bmp[n] != NULL)
         {
            destroy_bitmap(t->bmp[n]);
            t->bmp[n] = NULL;
         }
      }
   }

   // preview & tiles windows
   for (i=0; i<2; i++)
   {
      // choose one
      if (i == 0)
         w_elm = & glb_ds1edit.win_edit.w_preview;
      else if (i == 1)
         w_elm = & glb_ds1edit.win_edit.w_tiles;
      else
         w_elm = NULL;

      // free
      if (w_elm->border != NULL)
      {
         destroy_rle_sprite(w_elm->border);
         w_elm->border = NULL;
      }
      if (w_elm->inside != NULL)
      {
         destroy_bitmap(w_elm->inside);
        w_elm->inside = NULL;
      }
   }
}


// ==========================================================================
// draw a button in the screen buffer
void wedit_draw_button(BUT_TYP_E b_num)
{
   BUTTON_S * b = & glb_ds1edit.win_edit.button[b_num];
   int x1, y1, x2, y2;
   
   if ( ! b->active)
      return;
   x1 = b->x0;
   x2 = x1 + b->w - 1;
   y1 = b->y0;
   y2 = y1 + b->h - 1;
   rectfill(glb_ds1edit.screen_buff, x1, y1, x2, y2, 0);
   draw_sprite(glb_ds1edit.screen_buff, b->bmp[b->state], b->x0, b->y0);
}


// ==========================================================================
// draw a tab in the screen buffer
void wedit_draw_tab(BLK_TYP_E t_num)
{
   BUTTON_S * t = & glb_ds1edit.win_edit.tab[t_num];
   int x1, y1, x2, y2;

   if ( ! t->active)
      return;
   x1 = t->x0;
   x2 = x1 + t->w - 1;
   y1 = t->y0;
   y2 = y1 + t->h - 1;
   rectfill(glb_ds1edit.screen_buff, x1, y1, x2, y2, 0);
   draw_sprite(glb_ds1edit.screen_buff, t->bmp[t->state], t->x0, t->y0);
}


// ==========================================================================
// does a tab have at least 1 tile of its type ?
int wedit_tab_have(int ds1_idx, BLK_TYP_E t)
{
   BLOCK_TABLE_S * bt_ptr = glb_ds1[ds1_idx].block_table;
   int           i;

   for (i=0; i<glb_ds1[ds1_idx].bt_num; i++)
   {
      if (bt_ptr->type == t)
         return TRUE;
      bt_ptr++;
   }

   return FALSE;
}


// ==========================================================================
// free the tiles tables for each tab
void wedit_tiles_free(void)
{
   WIN_EDIT_S  * w = & glb_ds1edit.win_edit;
   MAIN_LINE_S * m_ptr;
   int         t, m;

   for (t=BT_NULL+1; t<BT_MAX; t++)
   {
      if (w->main_line_tab[t] != NULL)
      {
         for (m=0; m < w->main_line_num[t]; m++)
         {
            m_ptr = w->main_line_tab[t] + m;
            if (m_ptr->sub_elm != NULL)
            {
               free(m_ptr->sub_elm);
               m_ptr->sub_elm = NULL;
            }
         }
         free(w->main_line_tab[t]);
         w->main_line_tab[t] = NULL;
      }
   }
}


// ==========================================================================
// make the tiles tables for each tab
void wedit_tiles_make(int ds1_idx)
{
   BITMAP        * tmp_bmp;
   BLOCK_TABLE_S * bt_ptr = glb_ds1[ds1_idx].block_table;
   WIN_EDIT_S    * w = & glb_ds1edit.win_edit;
   MAIN_LINE_S   * m_ptr;
   SUB_ELM_S     * s_ptr;
   int           numb, size, i, b, line, t, n, done, col, y, max_height, x;
   int           old_o, old_m, old_s, o, m, s, dx, bx, y0;
   char          ube;
   char          * is_keep, tmp[150];

   numb = glb_ds1[ds1_idx].bt_num;
   size = numb * sizeof(char);
   is_keep = (char *) malloc(size);
   if (is_keep == NULL)
   {
      sprintf(tmp, "(is_keep) : not enough mem (%i bytes)", size);
      ds1edit_error(tmp);
   }

   // for each type
   for (t = BT_NULL + 1; t < BT_MAX; t++)
   {
      for (i=0; i<numb; i++)
         * (is_keep + i) = FALSE;
         
      line = 0;
      old_o = old_m = -1;
      for (b=1; b < numb; b++)
      {
         if (bt_ptr[b].type == t)
         {
            o   = bt_ptr[b].orientation;
            m   = bt_ptr[b].main_index;
            dx  = bt_ptr[b].dt1_idx;
            bx  = bt_ptr[b].block_idx;
            ube = bt_ptr[b].used_by_editor;
            tmp_bmp = * (glb_dt1[dx].block_zoom[ZM_11] + bx);
            if ((tmp_bmp != NULL) && ube)
            {
               if ( (old_o != o) || (old_m != m) )
               {
                  * (is_keep + b) = TRUE;
                  line++;
                  old_o = o;
                  old_m = m;
               }
            }
         }
      }

      w->main_line_tab[t] = NULL;
      if (line == 0)
         continue;
         
      // malloc of w->main_line_tab[t]
      size = line * sizeof(MAIN_LINE_S);
      w->main_line_tab[t] = (MAIN_LINE_S *) malloc(size);
      if (w->main_line_tab[t] == NULL)
      {
         sprintf(tmp, "(main_line_tab) : not enough mem (%i bytes), "
                      "type %i, line %i", size, t, line);
         ds1edit_error(tmp);
      }
      for (i=0; i<line; i++)
      {
         w->main_line_tab[t][i].sub_elm    = NULL;
         w->main_line_tab[t][i].bt_idx_num = 0;
         w->main_line_tab[t][i].max_height = 0;
      }
      w->main_line_num[t] = line;

      b=1;
      y0=0;

      // for each line
      for (n=0; n<line; n++)
      {
         while ( * (is_keep + b) != TRUE)
            b++;
         old_o = bt_ptr[b].orientation;
         old_m = bt_ptr[b].main_index;
         old_s = bt_ptr[b].sub_index;
         col   = 1;
         y     = b;
         done  = FALSE;

         // # of different sub (but only 1st tile of the same set of random)
         do
         {
            y++;
            o   = bt_ptr[y].orientation;
            m   = bt_ptr[y].main_index;
            s   = bt_ptr[y].sub_index;
            if ((old_o == o) && (old_m == m))
            {
               // ok, same line
               dx  = bt_ptr[y].dt1_idx;
               bx  = bt_ptr[y].block_idx;
               ube = bt_ptr[y].used_by_editor;
               tmp_bmp = * (glb_dt1[dx].block_zoom[ZM_11] + bx);
               if ((tmp_bmp != NULL) && ube)
               {
                  if (old_s != s)
                  {
                     col++;
                     old_s = s;
                  }
                  else
                  {
                     // part of random set
                  }
               }
            }
            else
               done = TRUE;
         } while ( ! done);

         // col have the number of columns for this line

         // malloc of m_ptr->bt_idx_tab + n
         m_ptr = w->main_line_tab[t] + n;
         size = col * sizeof(SUB_ELM_S);
         m_ptr->sub_elm = (SUB_ELM_S *) malloc(size);
         if (m_ptr->sub_elm == NULL)
         {
            sprintf(tmp, "(sub_line) : not enough mem (%i bytes), "
                         "type %i, line %i, col %i", size, t, n, col);
            ds1edit_error(tmp);
         }
         for (i=0; i<col; i++)
         {
            m_ptr->sub_elm[i].bt_idx_tab = 0;
            m_ptr->sub_elm[i].x1         = 0;
            m_ptr->sub_elm[i].y1         = 0;
            m_ptr->sub_elm[i].x2         = 0;
            m_ptr->sub_elm[i].y2         = 0;
            m_ptr->sub_elm[i].is_draw    = FALSE;
         }
         m_ptr->bt_idx_num = col;

         max_height = 0;

         // each col, first time
         old_o = bt_ptr[b].orientation;
         old_m = bt_ptr[b].main_index;
         old_s = -1;
         y     = b - 1;
         x     = 0;
         done  = FALSE;
         
         // for each col of this line
         do
         {
            y++;
            o = bt_ptr[y].orientation;
            m = bt_ptr[y].main_index;
            s = bt_ptr[y].sub_index;
            if ((old_o == o) && (old_m == m))
            {
               // ok, same line
               dx  = bt_ptr[y].dt1_idx;
               bx  = bt_ptr[y].block_idx;
               ube = bt_ptr[y].used_by_editor;
               tmp_bmp = * (glb_dt1[dx].block_zoom[ZM_11] + bx);
               if ((tmp_bmp != NULL) && ube)
               {
                  if (old_s != s)
                  {
                     // fill it
                     s_ptr = m_ptr->sub_elm + x;
                     s_ptr->bt_idx_tab = y;
                     x++;
                     old_s = s;

                     // compare height
                     if (tmp_bmp->h > max_height)
                        max_height = tmp_bmp->h;
                  }
                  else
                  {
                     // part of random set
                  }
               }
            }
            else
               done = TRUE;
         } while ( ! done);

         // each col, second time (we know the max height this time)
         old_o = bt_ptr[b].orientation;
         old_m = bt_ptr[b].main_index;
         old_s = -1;
         y     = b - 1;
         x     = 0;
         done  = FALSE;
         
         // for each col of this line
         do
         {
            y++;
            o = bt_ptr[y].orientation;
            m = bt_ptr[y].main_index;
            s = bt_ptr[y].sub_index;
            if ((old_o == o) && (old_m == m))
            {
               // ok, same line
               dx  = bt_ptr[y].dt1_idx;
               bx  = bt_ptr[y].block_idx;
               ube = bt_ptr[y].used_by_editor;
               tmp_bmp = * (glb_dt1[dx].block_zoom[ZM_11] + bx);
               if ((tmp_bmp != NULL) && ube)
               {
                  if (old_s != s)
                  {
                     // fill it
                     s_ptr = m_ptr->sub_elm + x;
                     s_ptr->x1 = 161 * x;
                     s_ptr->x2 = s_ptr->x1 + 159;
                     s_ptr->y1 = y0;
                     s_ptr->y2 = s_ptr->y1 + max_height + 12;
                     x++;
                     old_s = s;
                  }
                  else
                  {
                     // part of random set
                  }
               }
            }
            else
               done = TRUE;
         } while ( ! done);

         // fill max_height of this line
         m_ptr->max_height = max_height;
         y0 += max_height + 12;

         // this line is done
         b++;
      }
   }
   free(is_keep);
}


// ==========================================================================
// return the block table index of the cell (x, y) of 1 layer of the ds1
int wedit_search_tile(int ds1_idx, int x, int y, BUT_TYP_E but)
{
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   CELL_W_S * w_ptr;
   int      t, n;
   
   if ((but == BU_FLOOR1) || (but == BU_FLOOR2))
   {
      t     = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
      f_ptr = glb_ds1[ds1_idx].floor_buff + t;

      if ((but == BU_FLOOR1) && (glb_ds1[ds1_idx].floor_num >= 1))
         n = 0;
      else if ((but == BU_FLOOR2) && (glb_ds1[ds1_idx].floor_num >= 2))
         n = 1;
      else
         return -1;
         
      return f_ptr[n].bt_idx; // index in block table
   }
   else if (but == BU_SHADOW)
   {
      t     = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
      s_ptr = glb_ds1[ds1_idx].shadow_buff + t;

      if ((but == BU_SHADOW) && (glb_ds1[ds1_idx].shadow_num >= 1))
         n = 0;
      else
         return -1;
         
      return s_ptr[n].bt_idx; // index in block table
   }
   else if ((but == BU_WALL1) || (but == BU_WALL2) ||
            (but == BU_WALL3) || (but == BU_WALL4))
   {
      t      = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
      w_ptr  = glb_ds1[ds1_idx].wall_buff + t;
      if ((but == BU_WALL1) && (glb_ds1[ds1_idx].wall_num >= 1))
         n = 0;
      else if ((but == BU_WALL2) && (glb_ds1[ds1_idx].wall_num >= 2))
         n = 1;
      else if ((but == BU_WALL3) && (glb_ds1[ds1_idx].wall_num >= 3))
         n = 2;
      else if ((but == BU_WALL4) && (glb_ds1[ds1_idx].wall_num >= 4))
         n = 3;
      else
         return -1;
         
      return w_ptr[n].bt_idx; // index in block table
   }
   return -1;
}


// ==========================================================================
/*
void wedit_debug_tab_tiles(int ds1_idx, BLK_TYP_E t, int x0, int y0, int bt_sel)
{
   WIN_EDIT_S    * w  = & glb_ds1edit.win_edit;
   WIN_ELEMENT_S * wt = & w->w_tiles;
   MAIN_LINE_S   * m_ptr;
   SUB_ELM_S     * s_ptr;
   BLOCK_TABLE_S * bt_ptr = glb_ds1[ds1_idx].block_table;
   BITMAP        * tmp_bmp;
   int           m, i, bt, dx, bx, x1, y1, x2, y2, c;

   // draw the tiles
printf("\nTYPE = %3i\n", t);
   for (m=0; m < w->main_line_num[t]; m++)
   {
      printf("\nmain_line = %i\n", m);
      m_ptr = w->main_line_tab[t] + m;
      for (i=0; i < m_ptr->bt_idx_num; i++)
      {
         printf("i=%3i ", i);
         s_ptr = m_ptr->sub_elm + i;
         x1 = s_ptr->x1 - x0;
         x2 = s_ptr->x2 - x0;
         printf("(x1, x2)=(%+5i, %+5i) ", x1, x2);
         if ((x1 > -160) && (x1 < w->tmp_edit->w))
         {
            printf("OK ", m);
            bt = s_ptr->bt_idx_tab;
            dx = bt_ptr[bt].dt1_idx;
            bx = bt_ptr[bt].block_idx;
            tmp_bmp = * (glb_dt1[dx].block_zoom[ZM_11] + bx);
            y1 = s_ptr->y1 - y0;
            y2 = y1 + tmp_bmp->h - 1;
            printf("(bt dx bx tmp_bmp y1 y2)=(%3i %3i %3i %p %+5i %+5i) ",
               bt, dx, bx, tmp_bmp, y1, y2);
            if (((y1 + m_ptr->max_height - 1) < 0) || (y1 >= w->tmp_edit->h))
            {
               // nothing to draw
               printf("[NO draw] ", m);
            }
            else
            {
               printf("[   draw] ", m);
            }
         }
         printf("\n");
      }
   }
}
*/

// ==========================================================================
// draw the tiles of 1 tab
void wedit_tab_tiles(int ds1_idx, BLK_TYP_E t, int x0, int y0, int bt_sel,
                     int m_idx, int s_idx)
{
   WIN_EDIT_S    * w  = & glb_ds1edit.win_edit;
   WIN_ELEMENT_S * wt = & w->w_tiles;
   MAIN_LINE_S   * m_ptr;
   SUB_ELM_S     * s_ptr;
   BLOCK_TABLE_S * bt_ptr = glb_ds1[ds1_idx].block_table;
   BITMAP        * tmp_bmp;
   int           m, i, bt, dx, bx, x1, y1, x2, y2, c, len, dt1_idx;
   char          * dt1name;


// wedit_debug_tab_tiles(ds1_idx, t, x0, y0, bt_sel);

   // clear the background
   blit(wt->inside, w->tmp_edit, 0, 0, 0, 0, wt->inside->w, wt->inside->h);

   // draw the tiles
   text_mode(-1);
   for (m=0; m < w->main_line_num[t]; m++)
   {
      m_ptr = w->main_line_tab[t] + m;

      // name of the dt1 for this line
      s_ptr = m_ptr->sub_elm;
      x1 = s_ptr->x1 - x0;
      y1 = s_ptr->y1 - y0 + (m_ptr->max_height / 2);
      if ((y1 >= -8) && (y1 < w->tmp_edit->h) && (x1 > 0))
      {
         dt1_idx = bt_ptr[s_ptr->bt_idx_tab].dt1_idx;
         if (dt1_idx != -1)
         {
            dt1name = glb_dt1[dt1_idx].name;
            len = strlen(dt1name) + 2;
            x1 -= 8 * len;
            textprintf(w->tmp_edit, font, x1+1, y1+1,   0, "%s", dt1name);
            textprintf(w->tmp_edit, font, x1,   y1,   255, "%s", dt1name);
         }
      }
      
      // for each col of this line
      for (i=0; i < m_ptr->bt_idx_num; i++)
      {
         s_ptr = m_ptr->sub_elm + i;
         x1 = s_ptr->x1 - x0;
         x2 = s_ptr->x2 - x0;
         if ((x1 > -160) && (x1 < w->tmp_edit->w))
         {
            bt = s_ptr->bt_idx_tab;
            dx = bt_ptr[bt].dt1_idx;
            bx = bt_ptr[bt].block_idx;
            tmp_bmp = * (glb_dt1[dx].block_zoom[ZM_11] + bx);
            if (tmp_bmp != NULL)
            {
               y1 = s_ptr->y1 - y0;
               y2 = y1 + tmp_bmp->h - 1;
               if (((y1 + m_ptr->max_height - 1) < 0) || (y1 >= w->tmp_edit->h))
               {
                  // nothing to draw
                  s_ptr->is_draw = FALSE;
               }
               else
               {
                  s_ptr->is_draw = TRUE;
                  rect(w->tmp_edit, x1-1, y1-1, x2+1, y1+m_ptr->max_height+1+10, 255);
                  c = 22;
                  if (bt_sel == bt)
                     c = 119;
                  if ((m == m_idx) && (i == s_idx))
                     c = 255;
                  if (t == BT_WALL_DOWN)
                  {
                     rectfill(w->tmp_edit, x1, y1, x2, y2+9, c);
                     draw_sprite(w->tmp_edit, tmp_bmp, x1, y1);
                  }
                  else
                  {
                     rectfill(w->tmp_edit,
                        x1, y1 + m_ptr->max_height - tmp_bmp->h,
                        x2, y1 + m_ptr->max_height + 10, c);
                     draw_sprite(w->tmp_edit, tmp_bmp, x1,
                        y1 + m_ptr->max_height - tmp_bmp->h);
                  }
                  if (t == BT_WALL_DOWN)
                  {
                     textprintf(w->tmp_edit, font, x1 +   2, y1 + 2,  96, "%2i", bt_ptr[bt].main_index);
                     textprintf(w->tmp_edit, font, x1 +  26, y1 + 2,  96, "%2i", bt_ptr[bt].orientation);
                     textprintf(w->tmp_edit, font, x1 +  82, y1 + 2, 158, "%2i", bt_ptr[bt].sub_index);
                     textprintf(w->tmp_edit, font, x1 + 138, y1 + 2, 134, "%2i", bt_ptr[bt].rarity);
                  }
                  else
                  {
                     textprintf(w->tmp_edit, font, x1 +   2, y1 + m_ptr->max_height + 2,  96, "%2i", bt_ptr[bt].main_index);
                     textprintf(w->tmp_edit, font, x1 +  26, y1 + m_ptr->max_height + 2,  96, "%2i", bt_ptr[bt].orientation);
                     textprintf(w->tmp_edit, font, x1 +  82, y1 + m_ptr->max_height + 2, 158, "%2i", bt_ptr[bt].sub_index);
                     textprintf(w->tmp_edit, font, x1 + 138, y1 + m_ptr->max_height + 2, 134, "%2i", bt_ptr[bt].rarity);
                  }
               }
            }
            else
            {
               // nothing to draw (tmp_bmp == NULL)
               s_ptr->is_draw = FALSE;
            }
         }
      }
   }
}


// ==========================================================================
// scroll the tiles window of 1 tab to see the current tile
void wedit_jump(int ds1_idx, int bt_idx, int * xn, int * yn)
{
   WIN_EDIT_S    * w      = & glb_ds1edit.win_edit;
   WIN_ELEMENT_S * wt     = & w->w_tiles;
   MAIN_LINE_S   * m_ptr;
   SUB_ELM_S     * s_ptr;
   BLOCK_TABLE_S * bt_ptr = glb_ds1[ds1_idx].block_table;
   int           m, s;
   BLK_TYP_E     t;

   * xn = 0;
   * yn = 0;
   if (bt_idx <= 0)
      return;
      
   t = bt_ptr[bt_idx].type;
   for (m=0; m < w->main_line_num[t]; m++)
   {
      m_ptr = w->main_line_tab[t] + m;
      for (s=0; s<m_ptr->bt_idx_num; s++)
      {
         s_ptr = m_ptr->sub_elm + s;
         if (s_ptr->bt_idx_tab == bt_idx)
         {
            * xn = s_ptr->x1 - glb_config.scroll.edit.x;
            if (t == BT_WALL_DOWN)
               * yn = s_ptr->y1 - glb_config.scroll.edit.y;
            else
               * yn = s_ptr->y2 + glb_config.scroll.edit.y - wt->inside->h;
            return;
         }
      }
   }
   return;
}


// ==========================================================================
// what tile of 1 tab is under the mouse ?
void wedit_mouse_tile(int mx, int my, int x0, int y0, int t, int * m_idx, int * s_idx)
{
   WIN_EDIT_S    * w  = & glb_ds1edit.win_edit;
   WIN_ELEMENT_S * wt = & w->w_tiles;
   MAIN_LINE_S   * m_ptr;
   SUB_ELM_S     * s_ptr;
   int           m, i, x, y;

   // mouse inside the tile window ?
   x = mx - wt->i_x0;
   y = my - wt->i_y0;
   if ((x >= 0) && (x < wt->inside->w) && (y >= 0) && (y < wt->inside->h))
   {
      // yes, so search the tile which is under the mouse
      x += x0;
      y += y0;
      for (m=0; m < w->main_line_num[t]; m++)
      {
         m_ptr = w->main_line_tab[t] + m;
         for (i=0; i < m_ptr->bt_idx_num; i++)
         {
            s_ptr = m_ptr->sub_elm + i;
            if (s_ptr->is_draw)
            {
               if ((x >= s_ptr->x1) && (x <= s_ptr->x2) &&
                   (y >= s_ptr->y1) && (y <= s_ptr->y2))
               {
                  * m_idx = m;
                  * s_idx = i;
                  return;
               }
            }
         }
      }
   }
   // outside, or no pointing any tile
   * m_idx = * s_idx = -1;
}


// ==========================================================================
// tile chosen in the list, update the ds1 with it
void wedit_update_tile(int ds1_idx, int x, int y, BUT_TYP_E button,
                       BLK_TYP_E type, int m_idx, int s_idx)
{
   BLOCK_TABLE_S * bt_ptr = glb_ds1[ds1_idx].block_table;
   WIN_EDIT_S    * e_ptr  = & glb_ds1edit.win_edit;
   MAIN_LINE_S   * m_ptr;
   SUB_ELM_S     * s_ptr;
   CELL_W_S      * w_ptr;
   CELL_F_S      * f_ptr;
   CELL_S_S      * sh_ptr;
   int           f = -1, s = -1, w = -1, t, bt;
   
   switch (button)
   {
      case BU_FLOOR1 : if (glb_ds1[ds1_idx].floor_num  >= 1) f = 0; break;
      case BU_FLOOR2 : if (glb_ds1[ds1_idx].floor_num  >= 2) f = 1; break;
      case BU_SHADOW : if (glb_ds1[ds1_idx].shadow_num >= 1) s = 0; break;
      case BU_WALL1  : if (glb_ds1[ds1_idx].wall_num   >= 1) w = 0; break;
      case BU_WALL2  : if (glb_ds1[ds1_idx].wall_num   >= 2) w = 1; break;
      case BU_WALL3  : if (glb_ds1[ds1_idx].wall_num   >= 3) w = 2; break;
      case BU_WALL4  : if (glb_ds1[ds1_idx].wall_num   >= 4) w = 3; break;
   }
   if ((f == -1) && (w == -1) && (s == -1))
      return;
   m_ptr = e_ptr->main_line_tab[type] + m_idx;
   s_ptr = m_ptr->sub_elm + s_idx;
   bt    = s_ptr->bt_idx_tab;
   if (f != -1)
   {
      t = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
      f_ptr = glb_ds1[ds1_idx].floor_buff + t + f;
      if (f_ptr->prop1 == 0)
         f_ptr->prop1 = 194; // normal floor priority
      f_ptr->prop2  = bt_ptr[bt].sub_index;
      f_ptr->prop3  = (bt_ptr[bt].main_index & 0x0F) << 4;
      f_ptr->prop4  = (bt_ptr[bt].main_index & 0x30) >> 4;
      f_ptr->bt_idx = bt;
   }
   else if (s != -1)
   {
      t = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
      sh_ptr = glb_ds1[ds1_idx].shadow_buff + t + s;
      if (sh_ptr->prop1 == 0)
         sh_ptr->prop1 = 128; // normal shadow prop1
      sh_ptr->prop2  = bt_ptr[bt].sub_index;
      sh_ptr->prop3  = (bt_ptr[bt].main_index & 0x0F) << 4;
      sh_ptr->prop4  = ((bt_ptr[bt].main_index & 0x30) >> 4) | 0x08;
      sh_ptr->bt_idx = bt;
   }
   else if (w != -1)
   {
      t = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
      w_ptr = glb_ds1[ds1_idx].wall_buff + t + w;
      w_ptr->orientation = bt_ptr[bt].orientation;
      if (w_ptr->prop1 == 0)
         w_ptr->prop1 = 129; // normal wall priority
      w_ptr->prop2  = bt_ptr[bt].sub_index;
      w_ptr->prop3  = (bt_ptr[bt].main_index & 0x0F) << 4;
      w_ptr->prop4  = (bt_ptr[bt].main_index & 0x30) >> 4;
      w_ptr->bt_idx = bt;
   }
}


// ==========================================================================
// entering the tile window : save the tile infos (preparing cancel button)
void wedit_save_tile(int ds1_idx, int x, int y,
                     CELL_F_S * dst_f_ptr, CELL_W_S * dst_w_ptr,
                     CELL_S_S * dst_s_ptr)
{
   CELL_W_S * w_ptr;
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   int      i, t;

   
   // save floors
   t = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
   for (i=0; i<glb_ds1[ds1_idx].floor_num; i++)
   {
      f_ptr = glb_ds1[ds1_idx].floor_buff + t + i;
      dst_f_ptr[i].prop1  = f_ptr->prop1;
      dst_f_ptr[i].prop2  = f_ptr->prop2;
      dst_f_ptr[i].prop3  = f_ptr->prop3;
      dst_f_ptr[i].prop4  = f_ptr->prop4;
      dst_f_ptr[i].bt_idx = f_ptr->bt_idx;
      dst_f_ptr[i].flags  = f_ptr->flags;
   }

   // save shadows
   t = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
   for (i=0; i<glb_ds1[ds1_idx].shadow_num; i++)
   {
      s_ptr = glb_ds1[ds1_idx].shadow_buff + t + i;
      dst_s_ptr[i].prop1  = s_ptr->prop1;
      dst_s_ptr[i].prop2  = s_ptr->prop2;
      dst_s_ptr[i].prop3  = s_ptr->prop3;
      dst_s_ptr[i].prop4  = s_ptr->prop4;
      dst_s_ptr[i].bt_idx = s_ptr->bt_idx;
      dst_s_ptr[i].flags  = s_ptr->flags;
   }

   // save walls
   t = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
   for (i=0; i<glb_ds1[ds1_idx].wall_num; i++)
   {
      w_ptr = glb_ds1[ds1_idx].wall_buff + t + i;
      dst_w_ptr[i].prop1       = w_ptr->prop1;
      dst_w_ptr[i].prop2       = w_ptr->prop2;
      dst_w_ptr[i].prop3       = w_ptr->prop3;
      dst_w_ptr[i].prop4       = w_ptr->prop4;
      dst_w_ptr[i].orientation = w_ptr->orientation;
      dst_w_ptr[i].bt_idx      = w_ptr->bt_idx;
      dst_w_ptr[i].flags       = w_ptr->flags;
   }
}


// ==========================================================================
// cancel button : put back the precedently save tile infos
void wedit_cancel_tile(int ds1_idx, int x, int y,
                       CELL_F_S * src_f_ptr, CELL_W_S * src_w_ptr,
                       CELL_S_S * src_s_ptr)
{
   CELL_W_S * w_ptr;
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   int      i, t;

   
   // put previous floors
   t = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
   for (i=0; i<glb_ds1[ds1_idx].floor_num; i++)
   {
      f_ptr = glb_ds1[ds1_idx].floor_buff + t;
      f_ptr[i].prop1  = src_f_ptr[i].prop1;
      f_ptr[i].prop2  = src_f_ptr[i].prop2;
      f_ptr[i].prop3  = src_f_ptr[i].prop3;
      f_ptr[i].prop4  = src_f_ptr[i].prop4;
      f_ptr[i].bt_idx = src_f_ptr[i].bt_idx;
      f_ptr[i].flags  = src_f_ptr[i].flags;
   }

   // put previous shadows
   t = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
   for (i=0; i<glb_ds1[ds1_idx].shadow_num; i++)
   {
      s_ptr = glb_ds1[ds1_idx].shadow_buff + t;
      s_ptr[i].prop1  = src_s_ptr[i].prop1;
      s_ptr[i].prop2  = src_s_ptr[i].prop2;
      s_ptr[i].prop3  = src_s_ptr[i].prop3;
      s_ptr[i].prop4  = src_s_ptr[i].prop4;
      s_ptr[i].bt_idx = src_s_ptr[i].bt_idx;
      s_ptr[i].flags  = src_s_ptr[i].flags;
   }

   // put previous walls
   t = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
   for (i=0; i<glb_ds1[ds1_idx].wall_num; i++)
   {
      w_ptr = glb_ds1[ds1_idx].wall_buff + t;
      w_ptr[i].prop1       = src_w_ptr[i].prop1;
      w_ptr[i].prop2       = src_w_ptr[i].prop2;
      w_ptr[i].prop3       = src_w_ptr[i].prop3;
      w_ptr[i].prop4       = src_w_ptr[i].prop4;
      w_ptr[i].orientation = src_w_ptr[i].orientation;
      w_ptr[i].bt_idx      = src_w_ptr[i].bt_idx;
      w_ptr[i].flags       = src_w_ptr[i].flags;
   }
}


// ==========================================================================
// ok button, put the old datas in undo buffer
void wedit_keep_tile(int ds1_idx, int x, int y,
                     CELL_F_S * src_f_ptr, CELL_W_S * src_w_ptr,
                     CELL_S_S * src_s_ptr)
{
   UNDO_S * u = & glb_ds1[ds1_idx].undo;
   int    n;
   
   undo_new_tile_buffer(ds1_idx);

   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
      u->dat_flg.f[n] = DF_P1 | DF_P2 | DF_P3 | DF_P4;

   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
      u->dat_flg.s[n] = DF_P1 | DF_P2 | DF_P3 | DF_P4;

   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
      u->dat_flg.w[n] = DF_P1 | DF_P2 | DF_P3 | DF_P4 | DF_OR;

   undo_add_this_tile_to_buffer(ds1_idx, x, y, src_f_ptr, src_w_ptr, src_s_ptr);
   undo_close_tile_buffer(ds1_idx);
}


// ==========================================================================
// MAIN loop of the 'tile edit window'
//    (certainly too big)
void wedit_test(int ds1_idx, int tx, int ty)
{
   WIN_ELEMENT_S * w_elm;
   WIN_EDIT_S    * w = & glb_ds1edit.win_edit;
   DS1_S         * d = & glb_ds1[ds1_idx];
   BUTTON_S      * b;
   BLOCK_TABLE_S * bt_ptr = glb_ds1[ds1_idx].block_table;
   int           i, n, done=FALSE, mx, my, mb;
   int           focus=0, focus_can_change=TRUE, bt_sel;
   int           my_border = 1, xn=0, yn=0, old_xn=0, old_yn=0;
   BUT_TYP_E     current_button = BU_NULL;
   BLK_TYP_E     current_type = BT_NULL;
   int           m_idx, s_idx;
   CELL_W_S      save_wall   [WALL_MAX_LAYER];
   CELL_F_S      save_floor  [FLOOR_MAX_LAYER];
   CELL_S_S      save_shadow [SHADOW_MAX_LAYER];
   char          tmp[80];
   

   wedit_save_tile(ds1_idx, tx, ty, save_floor, save_wall, save_shadow);
   wedit_tiles_make(ds1_idx);
   position_mouse(200, 70);
   
   w->button[BU_WALL1].active  = 0 < d->wall_num ? TRUE : FALSE;
   w->button[BU_WALL2].active  = 1 < d->wall_num ? TRUE : FALSE;
   w->button[BU_WALL3].active  = 2 < d->wall_num ? TRUE : FALSE;
   w->button[BU_WALL4].active  = 3 < d->wall_num ? TRUE : FALSE;
   
   w->button[BU_FLOOR1].active = 0 < d->floor_num ? TRUE : FALSE;
   w->button[BU_FLOOR2].active = 1 < d->floor_num ? TRUE : FALSE;

   w->button[BU_OK].active     = TRUE;
   w->button[BU_CANCEL].active = TRUE;

   w->button[BU_VIEW_RAND].active = FALSE;
//   w->button[BU_VIEW_RAND].active = TRUE;
   
   for (i=BU_NULL + 1; i<BU_MAX; i++)
   {
      if (w->button[i].active)
      {
         w->button[i].mouse_over  = FALSE;
         w->button[i].state       = BS_OFF;
      }
   }

   if (w->button[BU_WALL1].active == TRUE)
      current_button = BU_WALL1;
   else if (w->button[BU_FLOOR1].active == TRUE)
      current_button = BU_FLOOR1;

   // bt_idx of the current button (current layer)
   bt_sel = wedit_search_tile(ds1_idx, tx, ty, current_button);
   wedit_jump(ds1_idx, bt_sel, & xn, & yn);

   w_elm = & glb_ds1edit.win_edit.w_tiles;

   b = & w->tab[BT_STATIC];
   b->x0             = w_elm->i_x0;
   b->y0             = 88;
   b->active         = FALSE;
   b->state          = BS_OFF;
   b->tab_have_tiles = wedit_tab_have(ds1_idx, BT_STATIC);

   b = & w->tab[BT_ANIMATED];
   b->x0             = w->tab[BT_STATIC].x0 + w->tab[BT_STATIC].w + my_border;
   b->y0             = 88;
   b->active         = FALSE;
   b->state          = BS_OFF;
   b->tab_have_tiles = wedit_tab_have(ds1_idx, BT_ANIMATED);
   
   b = & w->tab[BT_SHADOW];
   b->x0             = w_elm->i_x0;
   b->y0             = 88;
   b->active         = FALSE;
   b->state          = BS_OFF;
   b->tab_have_tiles = wedit_tab_have(ds1_idx, BT_SHADOW);
   if (b->tab_have_tiles)
      w->button[BU_SHADOW].active = TRUE;
   else
      w->button[BU_SHADOW].active = FALSE;

   b = & w->tab[BT_WALL_UP];
   b->x0             = w_elm->i_x0;
   b->y0             = 88;
   b->state          = BS_OFF;
   b->tab_have_tiles = b->active = wedit_tab_have(ds1_idx, BT_WALL_UP);

   b = & w->tab[BT_WALL_DOWN];
   b->x0             = w->tab[BT_WALL_UP].x0 + w->tab[BT_WALL_UP].w + my_border;
   b->y0             = 88;
   b->state          = BS_OFF;
   b->tab_have_tiles = b->active = wedit_tab_have(ds1_idx, BT_WALL_DOWN);

   b = & w->tab[BT_ROOF];
   b->x0             = w->tab[BT_WALL_DOWN].x0 + w->tab[BT_WALL_DOWN].w + my_border;
   b->y0             = 88;
   b->state          = BS_OFF;
   b->tab_have_tiles = b->active = wedit_tab_have(ds1_idx, BT_ROOF);

   b = & w->tab[BT_SPECIAL];
   b->x0             = w->tab[BT_ROOF].x0 + w->tab[BT_ROOF].w + my_border;
   b->y0             = 88;
   b->state          = BS_OFF;
   b->tab_have_tiles = b->active = wedit_tab_have(ds1_idx, BT_SPECIAL);

   set_color_depth(8);

   w->tmp_edit = create_bitmap(w->w_tiles.inside->w, w->w_tiles.inside->h);

   set_color_depth(glb_config.screen.depth);

   if (bt_sel >= 1)
   {
      current_type = bt_ptr[bt_sel].type;
      w->tab[current_type].state = BS_ON;
   }
   else
   {
      if (w->tab[BT_WALL_UP].active)
      {
         w->tab[BT_WALL_UP].state = BS_ON;
         current_type = BT_WALL_UP;
      }
      else if (w->tab[BT_WALL_DOWN].active)
      {
         w->tab[BT_WALL_DOWN].state = BS_ON;
         current_type = BT_WALL_DOWN;
      }
      else if (w->tab[BT_ROOF].active)
      {
         w->tab[BT_ROOF].state = BS_ON;
        current_type = BT_ROOF;
      }
      else if (w->tab[BT_SPECIAL].active)
      {
         w->tab[BT_SPECIAL].state = BS_ON;
        current_type = BT_SPECIAL;
      }
   }

   wedit_tab_tiles(ds1_idx, current_type, xn, yn, bt_sel, -1, -1);

   // main loop
   while ( ! done)
   {
      mx = mouse_x;
      my = mouse_y;
      mb = mouse_b;

      // always redraw whole screen

      clear_to_color(glb_ds1edit.screen_buff, 27);
      for (i = BU_NULL+1; i<BU_MAX; i++)
         wedit_draw_button(i);

      if (current_button != BU_NULL)
      {
         b = & glb_ds1edit.win_edit.button[current_button];
         rect(glb_ds1edit.screen_buff,
            b->x0-2, b->y0-2, b->x0+b->w+1, b->y0+b->h+1, 133); // medium green
      }

      w_elm = & glb_ds1edit.win_edit.w_preview;
      draw_rle_sprite(glb_ds1edit.screen_buff, w_elm->border, w_elm->b_x0, w_elm->b_y0);
      blit(w_elm->inside, glb_ds1edit.screen_buff, 0, 0, w_elm->i_x0, w_elm->i_y0, w_elm->inside->w, w_elm->inside->h);

      w_elm = & glb_ds1edit.win_edit.w_tiles;
      draw_rle_sprite(glb_ds1edit.screen_buff, w_elm->border, w_elm->b_x0, w_elm->b_y0);

      wedit_draw_w (ds1_idx, tx, ty, FALSE); // lower walls
      wedit_draw_f (ds1_idx, tx, ty);        // floors
      wedit_draw_s (ds1_idx, tx, ty);        // shadows
      wedit_draw_w (ds1_idx, tx, ty, TRUE);  // upper walls
      wedit_draw_r (ds1_idx, tx, ty);        // roofs
      wedit_draw_sp(ds1_idx, tx, ty);        // special tiles

      for (i=BT_NULL + 1; i<BT_MAX; i++)
      {
         b = & w->tab[i];
         if (b->active)
            wedit_draw_tab(i);
      }

      blit(w->tmp_edit, glb_ds1edit.screen_buff, 0, 0,
            w_elm->i_x0, w_elm->i_y0, w->tmp_edit->w, w->tmp_edit->h);

      misc_draw_screen(mx, my);

      // which tile is under the mouse ?
      wedit_mouse_tile(mx, my, xn, yn, current_type, & m_idx, & s_idx);

      // scroll
      if (key[KEY_UP]    || my == 0)                        yn -= glb_config.scroll.edit.y;
      if (key[KEY_DOWN]  || my == glb_config.screen.height - 1) yn += glb_config.scroll.edit.y;
      if (key[KEY_LEFT]  || mx == 0)                        xn -= glb_config.scroll.edit.x;
      if (key[KEY_RIGHT] || mx == glb_config.screen.width - 1)  xn += glb_config.scroll.edit.x;
      if (key[KEY_HOME])  xn = 0;
      if (key[KEY_PGUP])
      {
         if (key[KEY_LCONTROL] || key[KEY_RCONTROL])
            xn -= w->tmp_edit->w;
         else
            yn -= w->tmp_edit->h;
         rest(80);
      }
      if (key[KEY_PGDN])
      {
         if (key[KEY_LCONTROL] || key[KEY_RCONTROL])
            xn += w->tmp_edit->w;
         else
            yn += w->tmp_edit->h;
         rest(80);
      }

      // special tiles layer
      if (key[KEY_F9])
      {
         while(key[KEY_F9])
            ;
         glb_ds1[ds1_idx].special_layer_mask = 1 - glb_ds1[ds1_idx].special_layer_mask;
      }
      
      // shadow mode
      if (key[KEY_F11])
      {
         while(key[KEY_F11])
            ;
         if (key[KEY_LSHIFT] || key[KEY_RSHIFT])
         {
            glb_ds1[ds1_idx].shadow_layer_mask[0]--;
            if (glb_ds1[ds1_idx].shadow_layer_mask[0] < 0)
               glb_ds1[ds1_idx].shadow_layer_mask[0] = 3;
         }
         else
         {
            glb_ds1[ds1_idx].shadow_layer_mask[0]++;
            if (glb_ds1[ds1_idx].shadow_layer_mask[0] >= 4)
               glb_ds1[ds1_idx].shadow_layer_mask[0] = 0;
         }
      }

      // gamma correction
      if (key[KEY_F12])
      {
         if (key[KEY_LSHIFT] || key[KEY_RSHIFT])
         {
            if (glb_ds1edit.cur_gamma > GC_060)
            {
               rest(80);
               glb_ds1edit.cur_gamma--;
               misc_update_pal_with_gamma();
               set_palette(glb_ds1edit.vga_pal[glb_ds1[ds1_idx].act - 1]);
            }
         }
         else
         {
            if (glb_ds1edit.cur_gamma < GC_300)
            {
               rest(80);
               glb_ds1edit.cur_gamma++;
               misc_update_pal_with_gamma();
               set_palette(glb_ds1edit.vga_pal[glb_ds1[ds1_idx].act - 1]);
            }
         }
      }

      // screenshot
      if (key[KEY_P])
      {
         sprintf(tmp, "screenshot-%05i.pcx", glb_ds1edit.screenshot_num);
         while (file_exists(tmp, -1, NULL))
         {
            glb_ds1edit.screenshot_num++;
            sprintf(tmp, "screenshot-%05i.pcx", glb_ds1edit.screenshot_num);
         }

         // draw the mouse cursor onto the buffer
         draw_sprite(
            glb_ds1edit.screen_buff,
            glb_ds1edit.mouse_cursor[glb_ds1edit.mode],
            mouse_x - 1,
            mouse_y - 1
         );

         // save the buffer
         save_pcx(tmp, glb_ds1edit.screen_buff, glb_ds1edit.vga_pal[glb_ds1[ds1_idx].act-1]);
         misc_pcx_put_d2_palette(tmp, glb_ds1[ds1_idx].act-1);
         glb_ds1edit.screenshot_num++;
         while (key[KEY_P])
            ;
      }

      // focus for buttons
      if (focus_can_change)
         focus = 0;

      for (i=BU_NULL + 1; i<BU_MAX; i++)
      {
         b = & w->button[i];
         if (b->active)
         {
            if ((mx >= b->x0) &&
                (my >= b->y0) &&
                (mx <  b->x0 + b->w) &&
                (my <  b->y0 + b->h))
            {
               // mouse is over
               b->mouse_over = TRUE;
               if (mb)
               {
                  // mouse button is pressed
                  if ((focus_can_change) || (focus == i))
                  {
                     // change state if needed
                     if (b->state == BS_OFF)
                     {
                        b->state       = BS_ON;
                     }
                     if (focus_can_change)
                     {
                        focus = i;
                        focus_can_change = FALSE;
                     }
                  }
               }
               else // no mouse button pressed
               {
                  // change state if needed
                  if (b->state == BS_ON)
                  {
                     b->state = BS_OFF;
                  }
               }
            }
            else // mouse NOT over
            {
               b->mouse_over = FALSE;
               if (mb)
               {
                  // change state if needed
                  if (b->state == BS_ON)
                  {
                     b->state = BS_OFF;
                  }
               }
            }
         }
      }

      // focus for tabs
      for (i=BT_NULL + 1; i<BT_MAX; i++)
      {
         b = & w->tab[i];
         if (b->active)
         {
            if ((mx >= b->x0) &&
                (my >= b->y0) &&
                (mx <  b->x0 + b->w) &&
                (my <  b->y0 + b->h))
            {
               // mouse is over
               b->mouse_over = TRUE;
               if (mb)
               {
                  // mouse button is pressed
                  if (focus_can_change)
                  {
                     // all tabs are off
                     for (n=BT_NULL + 1; n<BT_MAX; n++)
                     {
                        if (w->tab[n].active)
                           w->tab[n].state = BS_OFF;
                     }
                     // this tab is on
                     b->state     = BS_ON;
                     current_type = i;
                     xn = yn = 0;
                  }
               }
            }
            else // mouse NOT over
               b->mouse_over = FALSE;
         }
      }
      
      // anti-bug
      if ((focus == 0) && (mb))
         focus_can_change = FALSE;
         
      // when no mouse button
      if (mb == 0)
      {
         if (key[KEY_ENTER] || key[KEY_ENTER_PAD])
         {
            // keep changes
            done = TRUE;
            wedit_keep_tile(ds1_idx, tx, ty, save_floor, save_wall, save_shadow);
            while (key[KEY_ENTER] || key[KEY_ENTER_PAD])
            {}
            focus_can_change = TRUE;
         }

         // releasing the focus button ?
         if (focus_can_change == FALSE)
         {
            if (w->button[focus].mouse_over == TRUE)
            {
               // action
               switch(focus)
               {
                  case BU_OK :
                     // keep changes
                     done = TRUE;
                     wedit_keep_tile(ds1_idx, tx, ty, save_floor, save_wall, save_shadow);
                     break;

                  case BU_CANCEL :
                     // restore original state
                     wedit_cancel_tile(ds1_idx, tx, ty, save_floor, save_wall, save_shadow);
                     done = TRUE;
                     break;

                  case BU_FLOOR1 :
                  case BU_FLOOR2 :
                     for (n=BT_NULL + 1; n<BT_MAX; n++)
                     {
                        w->tab[n].active = FALSE;
                        w->tab[n].state  = BS_OFF;
                     }
                     w->tab[BT_STATIC].active   = w->tab[BT_STATIC].tab_have_tiles;
                     w->tab[BT_ANIMATED].active = w->tab[BT_ANIMATED].tab_have_tiles;
                     current_button = focus;
                     bt_sel = wedit_search_tile(ds1_idx, tx, ty, current_button);
                     wedit_jump(ds1_idx, bt_sel, & xn, & yn);
                     if (bt_sel >= 1)
                     {
                        current_type = bt_ptr[bt_sel].type;
                        w->tab[current_type].state = BS_ON;
                     }
                     else
                     {
                        if (w->tab[BT_STATIC].active)
                        {
                           w->tab[BT_STATIC].state = BS_ON;
                           current_type = BT_STATIC;
                        }
                        else if (w->tab[BT_ANIMATED].active)
                        {
                           w->tab[BT_ANIMATED].state = BS_ON;
                           current_type = BT_ANIMATED;
                        }
                     }
                     break;

                  case BU_SHADOW :
                     for (n=BT_NULL + 1; n<BT_MAX; n++)
                     {
                        w->tab[n].active = FALSE;
                        w->tab[n].state  = BS_OFF;
                     }
                     w->tab[BT_SHADOW].active = w->tab[BT_SHADOW].tab_have_tiles;
                     current_button = focus;
                     bt_sel = wedit_search_tile(ds1_idx, tx, ty, current_button);
                     wedit_jump(ds1_idx, bt_sel, & xn, & yn);
                     if (bt_sel >= 1)
                     {
                        current_type = bt_ptr[bt_sel].type;
                        w->tab[current_type].state = BS_ON;
                     }
                     else
                     {
                        if (w->tab[BT_SHADOW].active)
                        {
                           w->tab[BT_SHADOW].state = BS_ON;
                           current_type = BT_SHADOW;
                        }
                     }
                     break;

                  case BU_WALL1 :
                  case BU_WALL2 :
                  case BU_WALL3 :
                  case BU_WALL4 :
                     for (n=BT_NULL + 1; n<BT_MAX; n++)
                     {
                        w->tab[n].active = FALSE;
                        w->tab[n].state  = BS_OFF;
                     }
                     w->tab[BT_WALL_UP].active   = w->tab[BT_WALL_UP].tab_have_tiles;
                     w->tab[BT_WALL_DOWN].active = w->tab[BT_WALL_DOWN].tab_have_tiles;
                     w->tab[BT_ROOF].active      = w->tab[BT_ROOF].tab_have_tiles;
                     w->tab[BT_SPECIAL].active   = w->tab[BT_SPECIAL].tab_have_tiles;
                     current_button = focus;
                     bt_sel = wedit_search_tile(ds1_idx, tx, ty, current_button);
                     wedit_jump(ds1_idx, bt_sel, & xn, & yn);
                     if (bt_sel >= 1)
                     {
                        current_type = bt_ptr[bt_sel].type;
                        w->tab[current_type].state = BS_ON;
                     }
                     else
                     {
                        if (w->tab[BT_WALL_UP].active)
                        {
                           w->tab[BT_WALL_UP].state = BS_ON;
                           current_type = BT_WALL_UP;
                        }
                        else if (w->tab[BT_WALL_DOWN].active)
                        {
                           w->tab[BT_WALL_DOWN].state = BS_ON;
                           current_type = BT_WALL_DOWN;
                        }
                        else if (w->tab[BT_ROOF].active)
                        {
                           w->tab[BT_ROOF].state = BS_ON;
                           current_type = BT_ROOF;
                        }
                        else if (w->tab[BT_SPECIAL].active)
                        {
                           w->tab[BT_SPECIAL].state = BS_ON;
                           current_type = BT_SPECIAL;
                        }
                     }
                     break;
               }
            }
            focus_can_change = TRUE;

            // over a tile ?
            if ((m_idx != -1) && (s_idx != -1))
            {
               // yes, change to this one
               wedit_update_tile(ds1_idx, tx, ty, current_button,
                                 current_type, m_idx, s_idx);
               bt_sel = wedit_search_tile(ds1_idx, tx, ty, current_button);
            }
         }
      }

      // redraw tiles
      wedit_tab_tiles(ds1_idx, current_type, xn, yn, bt_sel, m_idx, s_idx);
      old_xn = xn;
      old_yn = yn;
   }

   destroy_bitmap(w->tmp_edit);
   w->tmp_edit = NULL;
   wedit_tiles_free();
}
