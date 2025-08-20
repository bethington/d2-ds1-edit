#include "structs.h"
#include "error.h"
#include "ds1misc.h"
#include "wBits.h"
#include "misc.h"


// ==========================================================================
void wbits_main_single_tile(int ds1_idx, int tx, int ty)
{
   CELL_F_S      * f_ptr;
   CELL_W_S      * w_ptr;
   CELL_S_S      * s_ptr;
   CELL_T_S      * t_ptr;
   int           t, n;
   int           w_x0, w_y0, w_x1, w_y1;
   int           ok_x0, ok_y0, cancel_x0;
   int           c_black      = makecol(0, 0, 0),
                 c_white      = makecol(255, 255, 255),
                 c_yellow     = makecol(255, 255, 0),
                 c_darkgreen  = makecol(0, 64, 0),
                 c_lightgreen = makecol(0, 255, 0),
                 c_red        = makecol(255, 0, 0),
                 c, c1, c2, c3, mx, my, mb;
   int           b, i, val[4], x, y, save = FALSE, done = FALSE;
   UBYTE         props[6][4], dir[4], shad[4];
   UDWORD        tag_dw;
//   char          tmp[256];
   int           w_width = 545, w_height= 270;


   text_mode(-1);

   w_x0 = (glb_config.screen.width  - w_width) / 2;
   w_y0 = (glb_config.screen.height - w_height) / 2;
   w_x1 = w_x0 + w_width;
   w_y1 = w_y0 + w_height;

   ok_x0 = w_x0 + (w_width / 2) - 64;
   ok_y0 = w_y0 + w_height - 24;
   cancel_x0 = ok_x0 + 70;

   rectfill(glb_ds1edit.screen_buff, w_x0, w_y0, w_x1, w_y1, c_black);
   rect(glb_ds1edit.screen_buff, w_x0, w_y0, w_x1, w_y1, c_white);

   if (glb_ds1[ds1_idx].floor_num >= 1)
      textout(glb_ds1edit.screen_buff, font, "F1", w_x0 + 2, w_y0 +  2, c_yellow);
   if (glb_ds1[ds1_idx].floor_num >= 2)
      textout(glb_ds1edit.screen_buff, font, "F2", w_x0 + 2, w_y0 + 12, c_yellow);

   if (glb_ds1[ds1_idx].shadow_num >= 1)
      textout(glb_ds1edit.screen_buff, font, "SH", w_x0 + 2, w_y0 + 32, c_yellow);

   if (glb_ds1[ds1_idx].wall_num >= 1)
      textout(glb_ds1edit.screen_buff, font, "W1", w_x0 + 2, w_y0 + 52, c_yellow);
   if (glb_ds1[ds1_idx].wall_num >= 2)
      textout(glb_ds1edit.screen_buff, font, "W2", w_x0 + 2, w_y0 + 62, c_yellow);
   if (glb_ds1[ds1_idx].wall_num >= 3)
      textout(glb_ds1edit.screen_buff, font, "W3", w_x0 + 2, w_y0 + 72, c_yellow);
   if (glb_ds1[ds1_idx].wall_num >= 4)
      textout(glb_ds1edit.screen_buff, font, "W4", w_x0 + 2, w_y0 + 82, c_yellow);

   if (glb_ds1[ds1_idx].tag_num >= 1)
      textout(glb_ds1edit.screen_buff, font, "SUB", w_x0 + 2, w_y0 + 200, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 237, w_y0 + 92, w_y0 + 105, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Unwalkable", w_x0 + 200, w_y0 + 110, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 27, w_y0 + 92, w_y0 + 105, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Hidden", w_x0 + 10, w_y0 + 110, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 87, w_y0 + 92, w_y0 + 105, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "?", w_x0 + 82, w_y0 + 110, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 97, w_y0 + 92, w_y0 + 125, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "IsShadow", w_x0 + 60, w_y0 + 130, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 162, w_y0 + 92, w_y0 + 140, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Main-index", w_x0 + 130, w_y0 + 145, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 333, w_y0 + 92, w_y0 + 140, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Sub-index", w_x0 + 300, w_y0 + 145, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 413, w_y0 + 92, w_y0 + 100, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Layers priority,", w_x0 + 360, w_y0 + 105, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Type of layers,",  w_x0 + 360, w_y0 + 115, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "   and unknown",   w_x0 + 360, w_y0 + 125, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 503, w_y0 + 92, w_y0 + 140, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Orientation", w_x0 + 455, w_y0 + 145, c_yellow);

   // save props
   t     = (ty * glb_ds1[ds1_idx].floor_line) + (tx * glb_ds1[ds1_idx].floor_num);
   f_ptr = glb_ds1[ds1_idx].floor_buff + t;

   for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
   {
      props[i][0] = f_ptr[i].prop1;
      props[i][1] = f_ptr[i].prop2;
      props[i][2] = f_ptr[i].prop3;
      props[i][3] = f_ptr[i].prop4;
   }

   t     = (ty * glb_ds1[ds1_idx].wall_line) + (tx * glb_ds1[ds1_idx].wall_num);
   w_ptr = glb_ds1[ds1_idx].wall_buff + t;
   for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
   {
      props[i+2][0] = w_ptr[i].prop1;
      props[i+2][1] = w_ptr[i].prop2;
      props[i+2][2] = w_ptr[i].prop3;
      props[i+2][3] = w_ptr[i].prop4;
      dir[i]        = w_ptr[i].orientation;
   }

   t     = (ty * glb_ds1[ds1_idx].shadow_line) + (tx * glb_ds1[ds1_idx].shadow_num);
   s_ptr = glb_ds1[ds1_idx].shadow_buff + t;
   for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
   {
      shad[0] = s_ptr[i].prop1;
      shad[1] = s_ptr[i].prop2;
      shad[2] = s_ptr[i].prop3;
      shad[3] = s_ptr[i].prop4;
   }

   t     = (ty * glb_ds1[ds1_idx].tag_line) + (tx * glb_ds1[ds1_idx].tag_num);
   t_ptr = glb_ds1[ds1_idx].tag_buff + t;
   tag_dw = t_ptr->num;

   // mouse background
   // show_mouse(NULL);
   mx = mouse_x;
   my = mouse_y;
   mb = mouse_b;

   // main loop
   while ( ! done)
   {
      text_mode(-1);

      // OK button
      c1 = c_darkgreen;
      c2 = c_lightgreen;
      c3 = c_yellow;
      if ((mx >= ok_x0) && (mx <= ok_x0 + 64) && (my >= ok_y0) && (my <= ok_y0 + 20))
      {
         c1 = c_lightgreen;
         c2 = c_white;
         c3 = c_black;
         if (mb & 1)
         {
            // OK button selected
            save = TRUE;
            done = TRUE;
            while (mouse_b & 1)
            {
               // wait until left mouse button is not pressed anymore
            }
         }
      }
      if (key[KEY_ENTER] || key[KEY_ENTER_PAD])
      {
         save = TRUE;
         done = TRUE;
      }
      rectfill(glb_ds1edit.screen_buff, ok_x0, ok_y0-3, ok_x0 + 64, ok_y0 + 20, c1);
      rect(glb_ds1edit.screen_buff,     ok_x0, ok_y0-3, ok_x0 + 64, ok_y0 + 20, c2);
      textout(glb_ds1edit.screen_buff, font, "OK", ok_x0+25, ok_y0+5, c3);

      // CANCEL button
      c1 = c_darkgreen;
      c2 = c_lightgreen;
      c3 = c_yellow;
      if ((mx >= cancel_x0) && (mx <= cancel_x0 + 64) && (my >= ok_y0) && (my <= ok_y0 + 20))
      {
         c1 = c_lightgreen;
         c2 = c_white;
         c3 = c_black;
         if (mb & 1)
         {
            // CANCEL button selected
            save = FALSE;
            done = TRUE;
            while (mouse_b & 1)
            {
               // wait until left mouse button is not pressed anymore
            }
         }
      }
      if (key[KEY_ESC])
      {
         save = FALSE;
         done = TRUE;
      }
      rectfill(glb_ds1edit.screen_buff, cancel_x0, ok_y0-3, cancel_x0 + 64, ok_y0 + 20, c1);
      rect(glb_ds1edit.screen_buff,     cancel_x0, ok_y0-3, cancel_x0 + 64, ok_y0 + 20, c2);
      textout(glb_ds1edit.screen_buff, font, "CANCEL", cancel_x0+10, ok_y0+5, c3);

      text_mode(0);

      // floors
      y = w_y0 + 2;
      for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
      {
         val[0] = f_ptr[n].prop1;
         val[1] = f_ptr[n].prop2;
         val[2] = f_ptr[n].prop3;
         val[3] = f_ptr[n].prop4;
         x = w_x0 + 24;
         for (b=3; b >= 0 ; b--)
         {
            for (i=7; i >= 0; i--)
            {
               c = c_white;
               if ( (mx >= x) && (mx <= x+9) &&
                    (my >= y) && (my <= y+9) )
               {
                  c  = c_red;
                  if (mb & 1)
                  {
                     if (val[b] & (1 << i))
                     {
                        // change to 0
                        val[b] = val[b] - (1 << i);
                     }
                     else
                     {
                        // change to 1
                        val[b] = val[b] + (1 << i);
                     }
                     while (mb & 1)
                     {
                        // wait until no mouse button pressed
                        mb = mouse_b;
                     }
                     switch(b)
                     {
                        case 0 : f_ptr[n].prop1 = val[b]; break;
                        case 1 : f_ptr[n].prop2 = val[b]; break;
                        case 2 : f_ptr[n].prop3 = val[b]; break;
                        case 3 : f_ptr[n].prop4 = val[b]; break;
                     }
                  }
               }
               if (val[b] & (1 << i))
                  textout(glb_ds1edit.screen_buff, font, "1", x, y, c);
               else
                  textout(glb_ds1edit.screen_buff, font, "0", x, y, c);
               x += 10;

               if ((b==3) && ((i==7) || (i==6) || (i==5) || (i==3) || (i==2)))
                  x+=10;
               else if ((b==2) && ((i==4) || (i==2) || (i==1) || (i==0)))
                  x+=10;
               else if ((b==1) && ((i==6) || (i==0)))
                  x+=10;
            }
         }
         y += 10;
      }

      // shadow layer
      y += 10 * (2 - glb_ds1[ds1_idx].floor_num);
      y += 10;
      for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
      {
         shad[0] = s_ptr[n].prop1;
         shad[1] = s_ptr[n].prop2;
         shad[2] = s_ptr[n].prop3;
         shad[3] = s_ptr[n].prop4;
         x = w_x0 + 24;
         for (b=3; b >= 0 ; b--)
         {
            for (i=7; i >= 0; i--)
            {
               c = c_white;
               if ( (mx >= x) && (mx <= x+9) &&
                    (my >= y) && (my <= y+9) )
               {
                  c  = c_red;
                  if (mb & 1)
                  {
                     if (shad[b] & (1 << i))
                     {
                        // change to 0
                        shad[b] = shad[b] - (1 << i);
                     }
                     else
                     {
                        // change to 1
                        shad[b] = shad[b] + (1 << i);
                     }
                     while (mb & 1)
                     {
                        // wait until no mouse button pressed
                        mb = mouse_b;
                     }
                     switch(b)
                     {
                        case 0 : s_ptr[n].prop1 = shad[b]; break;
                        case 1 : s_ptr[n].prop2 = shad[b]; break;
                        case 2 : s_ptr[n].prop3 = shad[b]; break;
                        case 3 : s_ptr[n].prop4 = shad[b]; break;
                     }
                  }
               }
               if (shad[b] & (1 << i))
                  textout(glb_ds1edit.screen_buff, font, "1", x, y, c);
               else
                  textout(glb_ds1edit.screen_buff, font, "0", x, y, c);
               x += 10;

               if ((b==3) && ((i==7) || (i==6) || (i==5) || (i==3) || (i==2)))
                  x+=10;
               else if ((b==2) && ((i==4) || (i==2) || (i==1) || (i==0)))
                  x+=10;
               else if ((b==1) && ((i==6) || (i==0)))
                  x+=10;
            }
         }
         y += 10;
      }

      // walls
      y += 10;
      for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
      {
         val[0] = w_ptr[n].prop1;
         val[1] = w_ptr[n].prop2;
         val[2] = w_ptr[n].prop3;
         val[3] = w_ptr[n].prop4;
         x = w_x0 + 24;

         // props
         for (b=3; b >= 0 ; b--)
         {
            for (i=7; i >= 0; i--)
            {
               c = c_white;
               if ( (mx >= x) && (mx <= x+9) &&
                    (my >= y) && (my <= y+9) )
               {
                  c  = c_red;
                  if (mb & 1)
                  {
                     if (val[b] & (1 << i))
                     {
                        // change to 0
                        val[b] = val[b] - (1 << i);
                     }
                     else
                     {
                        // change to 1
                        val[b] = val[b] + (1 << i);
                     }
                     while (mb & 1)
                     {
                        // wait until no mouse button pressed
                        mb = mouse_b;
                     }
                     switch(b)
                     {
                        case 0 : w_ptr[n].prop1 = val[b]; break;
                        case 1 : w_ptr[n].prop2 = val[b]; break;
                        case 2 : w_ptr[n].prop3 = val[b]; break;
                        case 3 : w_ptr[n].prop4 = val[b]; break;
                     }
                  }
               }
               if (val[b] & (1 << i))
                  textout(glb_ds1edit.screen_buff, font, "1", x, y, c);
               else
                  textout(glb_ds1edit.screen_buff, font, "0", x, y, c);
               x += 10;

               if ((b==3) && ((i==7) || (i==6) || (i==5) || (i==3) || (i==2)))
                  x+=10;
               else if ((b==2) && ((i==4) || (i==2) || (i==1) || (i==0)))
                  x+=10;
               else if ((b==1) && ((i==6) || (i==0)))
                  x+=10;
               else if ((b==0) && (i==0))
                  x+=10;
            }
         }

         // orientation
         for (i=7; i >= 0; i--)
         {
            c = c_white;
            if ( (mx >= x) && (mx <= x+9) &&
                 (my >= y) && (my <= y+9) )
            {
               c  = c_red;
               if (mb & 1)
               {
                  if (w_ptr[n].orientation & (1 << i))
                  {
                     // change to 0
                     w_ptr[n].orientation = w_ptr[n].orientation - (1 << i);
                  }
                  else
                  {
                     // change to 1
                     w_ptr[n].orientation = w_ptr[n].orientation + (1 << i);
                  }
                  while (mb & 1)
                  {
                     // wait until no mouse button pressed
                     mb = mouse_b;
                  }
               }
            }
            if (w_ptr[n].orientation & (1 << i))
               textout(glb_ds1edit.screen_buff, font, "1", x, y, c);
            else
               textout(glb_ds1edit.screen_buff, font, "0", x, y, c);
            x += 10;
         }

         y += 10;
      }

      // substitution layer
      y += 10 * (4 - glb_ds1[ds1_idx].wall_num);
      y += 108;
      for (n=0; n<glb_ds1[ds1_idx].tag_num; n++)
      {
         tag_dw = t_ptr->num;
         x = w_x0 + 34;
         for (i=31; i >= 0; i--)
         {
            c = c_white;
            if ( (mx >= x) && (mx <= x+9) &&
                 (my >= y) && (my <= y+9) )
            {
               c  = c_red;
               if (mb & 1)
               {
                  if (tag_dw & (1 << i))
                  {
                     // change to 0
                     tag_dw = tag_dw - (1 << i);
                  }
                  else
                  {
                     // change to 1
                     tag_dw = tag_dw + (1 << i);
                  }
                  while (mb & 1)
                  {
                     // wait until no mouse button pressed
                     mb = mouse_b;
                  }
                  t_ptr->num = tag_dw;
               }
            }
            if (tag_dw & (1 << i))
               textout(glb_ds1edit.screen_buff, font, "1", x, y, c);
            else
               textout(glb_ds1edit.screen_buff, font, "0", x, y, c);
            x += 10;

            if ((i==8) || (i==16) || (i==24))
               x+=10;
         }
         y += 10;
         x = w_x0 + 48;
         textprintf(glb_ds1edit.screen_buff, font, x,       y, c_lightgreen, "%8lu", (tag_dw & 0xFF000000LU) >> 24);
         textprintf(glb_ds1edit.screen_buff, font, x + 90,  y, c_lightgreen, "%8lu", (tag_dw & 0x00FF0000LU) >> 16);
         textprintf(glb_ds1edit.screen_buff, font, x + 180, y, c_lightgreen, "%8lu", (tag_dw & 0x0000FF00LU) >> 8);
         textprintf(glb_ds1edit.screen_buff, font, x + 270, y, c_lightgreen, "%8lu",  tag_dw & 0x000000FFLU);
      }

      misc_draw_screen(mx, my);

      // new mouse state
      mx = mouse_x;
      my = mouse_y;
      mb = mouse_b;
   }

   if (save == FALSE)
   {
      // restore original props
      for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
      {
         f_ptr[i].prop1 = props[i][0];
         f_ptr[i].prop2 = props[i][1];
         f_ptr[i].prop3 = props[i][2];
         f_ptr[i].prop4 = props[i][3];
      }
      for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
      {
         w_ptr[i].prop1 = props[i+2][0];
         w_ptr[i].prop2 = props[i+2][1];
         w_ptr[i].prop3 = props[i+2][2];
         w_ptr[i].prop4 = props[i+2][3];
         w_ptr[i].orientation = dir[i];
      }
      for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
      {
         s_ptr[i].prop1 = shad[0];
         s_ptr[i].prop2 = shad[1];
         s_ptr[i].prop3 = shad[2];
         s_ptr[i].prop4 = shad[3];
      }
      t_ptr->num = tag_dw;
   }
   else
   {
      // find again all tiles
      ds1_make_prop_2_block(ds1_idx);
   }

   if (key[KEY_ESC])
   {
      while (key[KEY_ESC])
      {
         // wait until Escape is released
      }
   }
   if (key[KEY_ENTER] || key[KEY_ENTER_PAD])
   {
      while (key[KEY_ENTER] || key[KEY_ENTER_PAD])
      {
         // wait until Enter is released
      }
   }

   // end
   text_mode(-1);
}


// ====================================================================================
void wbits_apply_modification(int ds1_idx, WBITSDATA_S * ptr_wbitsdata)
{
   CELL_F_S    * f_ptr;
   CELL_W_S    * w_ptr;
   CELL_S_S    * s_ptr;
   CELL_T_S    * t_ptr;
   int         x, y, t, i, b;
   UDWORD      bt, b1, b2;


   for (y=0; y < glb_ds1[ds1_idx].height; y++)
   {
      for (x=0; x < glb_ds1[ds1_idx].width; x++)
      {
         // floors
         t = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
         for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
         {
            f_ptr = glb_ds1[ds1_idx].floor_buff + t + i;
            if (IS_SELECTED(f_ptr->flags))
            {
               // set the new bits, if needeed

               // prop1
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_f[i][0][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_f[i].prop1 & (1 << b);
                     b2 = f_ptr->prop1 & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           f_ptr->prop1 |= (1 << b); // set the bit
                        else
                           f_ptr->prop1 &= ~(1 << b); // remove the bit
                     }
                  }
               }

               // prop2
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_f[i][1][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_f[i].prop2 & (1 << b);
                     b2 = f_ptr->prop2 & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           f_ptr->prop2 |= (1 << b); // set the bit
                        else
                           f_ptr->prop2 &= ~(1 << b); // remove the bit
                     }
                  }
               }

               // prop3
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_f[i][2][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_f[i].prop3 & (1 << b);
                     b2 = f_ptr->prop3 & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           f_ptr->prop3 |= (1 << b); // set the bit
                        else
                           f_ptr->prop3 &= ~(1 << b); // remove the bit
                     }
                  }
               }

               // prop4
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_f[i][3][b];
                  if ((bt == WBITSTYPE_SAME))
                  {
                     b1 = ptr_wbitsdata->mix_f[i].prop4 & (1 << b);
                     b2 = f_ptr->prop4 & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           f_ptr->prop4 |= (1 << b); // set the bit
                        else
                           f_ptr->prop4 &= ~(1 << b); // remove the bit
                     }
                  }
               }
            }
         }

         // shadows
         t = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
         for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
         {
            s_ptr = glb_ds1[ds1_idx].shadow_buff + t + i;
            if (IS_SELECTED(s_ptr->flags))
            {
               // set the new bits, if needeed

               // prop1
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_s[i][0][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_s[i].prop1 & (1 << b);
                     b2 = s_ptr->prop1 & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           s_ptr->prop1 |= (1 << b); // set the bit
                        else
                           s_ptr->prop1 &= ~(1 << b); // remove the bit
                     }
                  }
               }

               // prop2
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_s[i][1][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_s[i].prop2 & (1 << b);
                     b2 = s_ptr->prop2 & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           s_ptr->prop2 |= (1 << b); // set the bit
                        else
                           s_ptr->prop2 &= ~(1 << b); // remove the bit
                     }
                  }
               }

               // prop3
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_s[i][2][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_s[i].prop3 & (1 << b);
                     b2 = s_ptr->prop3 & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           s_ptr->prop3 |= (1 << b); // set the bit
                        else
                           s_ptr->prop3 &= ~(1 << b); // remove the bit
                     }
                  }
               }

               // prop4
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_s[i][3][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_s[i].prop4 & (1 << b);
                     b2 = s_ptr->prop4 & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           s_ptr->prop4 |= (1 << b); // set the bit
                        else
                           s_ptr->prop4 &= ~(1 << b); // remove the bit
                     }
                  }
               }
            }
         }

         // walls
         t = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
         for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
         {
            w_ptr = glb_ds1[ds1_idx].wall_buff + t + i;
            if (IS_SELECTED(w_ptr->flags))
            {
               // set the new bits, if needeed

               // prop1
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_w[i][0][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_w[i].prop1 & (1 << b);
                     b2 = w_ptr->prop1 & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           w_ptr->prop1 |= (1 << b); // set the bit
                        else
                           w_ptr->prop1 &= ~(1 << b); // remove the bit
                     }
                  }
               }

               // prop2
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_w[i][1][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_w[i].prop2 & (1 << b);
                     b2 = w_ptr->prop2 & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           w_ptr->prop2 |= (1 << b); // set the bit
                        else
                           w_ptr->prop2 &= ~(1 << b); // remove the bit
                     }
                  }
               }

               // prop3
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_w[i][2][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_w[i].prop3 & (1 << b);
                     b2 = w_ptr->prop3 & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           w_ptr->prop3 |= (1 << b); // set the bit
                        else
                           w_ptr->prop3 &= ~(1 << b); // remove the bit
                     }
                  }
               }

               // prop4
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_w[i][3][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_w[i].prop4 & (1 << b);
                     b2 = w_ptr->prop4 & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           w_ptr->prop4 |= (1 << b); // set the bit
                        else
                           w_ptr->prop4 &= ~(1 << b); // remove the bit
                     }
                  }
               }

               // orientation
               for (b=0; b < 8; b++)
               {
                  bt = ptr_wbitsdata->bit_type_w[i][4][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_w[i].orientation & (1 << b);
                     b2 = w_ptr->orientation & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           w_ptr->orientation |= (1 << b); // set the bit
                        else
                           w_ptr->orientation &= ~(1 << b); // remove the bit
                     }
                  }
               }
            }
         }

         // tags
         t = (y * glb_ds1[ds1_idx].tag_line) + (x * glb_ds1[ds1_idx].tag_num);
         for (i=0; i < glb_ds1[ds1_idx].tag_num; i++)
         {
            t_ptr = glb_ds1[ds1_idx].tag_buff + t + i;
            if (IS_SELECTED(t_ptr->flags))
            {
               // set the new bits, if needeed

               // tag num
               for (b=0; b < 32; b++)
               {
                  bt = ptr_wbitsdata->bit_type_t[i][b];
                  if (bt == WBITSTYPE_SAME)
                  {
                     b1 = ptr_wbitsdata->mix_t[i].num & (1 << b);
                     b2 = t_ptr->num & (1 << b);
                     if (b1 != b2)
                     {
                        if (b1)
                           t_ptr->num |= (1 << b); // set the bit
                        else
                           t_ptr->num &= ~(1 << b); // remove the bit
                     }
                  }
               }

            }
         }
      }
   }
}


// ==========================================================================
void wbits_main_multiple_tiles(int ds1_idx, WBITSDATA_S * ptr_wbitsdata)
{
   int           w_x0, w_y0, w_x1, w_y1;
   int           ok_x0, ok_y0, cancel_x0;
   int           c_black      = makecol(0, 0, 0),
                 c_white      = makecol(255, 255, 255),
                 c_yellow     = makecol(255, 255, 0),
                 c_darkgreen  = makecol(0, 64, 0),
                 c_lightgreen = makecol(0, 255, 0),
                 c_red        = makecol(255, 0, 0),
                 c_unkn       = makecol(52, 156, 28),
                 c_blue       = makecol(36, 96, 216),
                 c1, c2, c3, mx, my, mb;
   UDWORD        tag_dw;
//   char          tmp[256];
   int           w_width = 545, w_height= 270;
   int           save = FALSE, done = FALSE;
   int           x, y, i, b, c, n, val[4];
   UBYTE         shad[4];


   text_mode(-1);

   w_x0 = (glb_config.screen.width  - w_width) / 2;
   w_y0 = (glb_config.screen.height - w_height) / 2;
   w_x1 = w_x0 + w_width;
   w_y1 = w_y0 + w_height;

   ok_x0 = w_x0 + (w_width / 2) - 64;
   ok_y0 = w_y0 + w_height - 24;
   cancel_x0 = ok_x0 + 70;

   rectfill(glb_ds1edit.screen_buff, w_x0, w_y0, w_x1, w_y1, c_black);
   rect(glb_ds1edit.screen_buff, w_x0, w_y0, w_x1, w_y1, c_white);

   if (glb_ds1[ds1_idx].floor_num >= 1)
      textout(glb_ds1edit.screen_buff, font, "F1", w_x0 + 2, w_y0 +  2, c_yellow);
   if (glb_ds1[ds1_idx].floor_num >= 2)
      textout(glb_ds1edit.screen_buff, font, "F2", w_x0 + 2, w_y0 + 12, c_yellow);

   if (glb_ds1[ds1_idx].shadow_num >= 1)
      textout(glb_ds1edit.screen_buff, font, "SH", w_x0 + 2, w_y0 + 32, c_yellow);

   if (glb_ds1[ds1_idx].wall_num >= 1)
      textout(glb_ds1edit.screen_buff, font, "W1", w_x0 + 2, w_y0 + 52, c_yellow);
   if (glb_ds1[ds1_idx].wall_num >= 2)
      textout(glb_ds1edit.screen_buff, font, "W2", w_x0 + 2, w_y0 + 62, c_yellow);
   if (glb_ds1[ds1_idx].wall_num >= 3)
      textout(glb_ds1edit.screen_buff, font, "W3", w_x0 + 2, w_y0 + 72, c_yellow);
   if (glb_ds1[ds1_idx].wall_num >= 4)
      textout(glb_ds1edit.screen_buff, font, "W4", w_x0 + 2, w_y0 + 82, c_yellow);

   if (glb_ds1[ds1_idx].tag_num >= 1)
      textout(glb_ds1edit.screen_buff, font, "SUB", w_x0 + 2, w_y0 + 200, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 237, w_y0 + 92, w_y0 + 105, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Unwalkable", w_x0 + 200, w_y0 + 110, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 27, w_y0 + 92, w_y0 + 105, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Hidden", w_x0 + 10, w_y0 + 110, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 87, w_y0 + 92, w_y0 + 105, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "?", w_x0 + 82, w_y0 + 110, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 97, w_y0 + 92, w_y0 + 125, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "IsShadow", w_x0 + 60, w_y0 + 130, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 162, w_y0 + 92, w_y0 + 140, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Main-index", w_x0 + 130, w_y0 + 145, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 333, w_y0 + 92, w_y0 + 140, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Sub-index", w_x0 + 300, w_y0 + 145, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 413, w_y0 + 92, w_y0 + 100, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Layers priority,", w_x0 + 360, w_y0 + 105, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Type of layers,",  w_x0 + 360, w_y0 + 115, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "   and unknown",   w_x0 + 360, w_y0 + 125, c_yellow);

   vline(glb_ds1edit.screen_buff, w_x0 + 503, w_y0 + 92, w_y0 + 140, c_yellow);
   textout(glb_ds1edit.screen_buff, font, "Orientation", w_x0 + 455, w_y0 + 145, c_yellow);

   // mouse background
   // show_mouse(NULL);
   mx = mouse_x;
   my = mouse_y;
   mb = mouse_b;

   // main loop
   while ( ! done)
   {
      text_mode(-1);

      // OK button
      c1 = c_darkgreen;
      c2 = c_lightgreen;
      c3 = c_yellow;
      if ((mx >= ok_x0) && (mx <= ok_x0 + 64) && (my >= ok_y0) && (my <= ok_y0 + 20))
      {
         c1 = c_lightgreen;
         c2 = c_white;
         c3 = c_black;
         if (mb & 1)
         {
            // OK button selected
            save = TRUE;
            done = TRUE;
            while (mouse_b & 1)
            {
               // wait until left mouse button is not pressed anymore
            }
         }
      }
      if (key[KEY_ENTER] || key[KEY_ENTER_PAD])
      {
         save = TRUE;
         done = TRUE;
      }
      rectfill(glb_ds1edit.screen_buff, ok_x0, ok_y0-3, ok_x0 + 64, ok_y0 + 20, c1);
      rect(glb_ds1edit.screen_buff,     ok_x0, ok_y0-3, ok_x0 + 64, ok_y0 + 20, c2);
      textout(glb_ds1edit.screen_buff, font, "OK", ok_x0+25, ok_y0+5, c3);

      // CANCEL button
      c1 = c_darkgreen;
      c2 = c_lightgreen;
      c3 = c_yellow;
      if ((mx >= cancel_x0) && (mx <= cancel_x0 + 64) && (my >= ok_y0) && (my <= ok_y0 + 20))
      {
         c1 = c_lightgreen;
         c2 = c_white;
         c3 = c_black;
         if (mb & 1)
         {
            // CANCEL button selected
            save = FALSE;
            done = TRUE;
            while (mouse_b & 1)
            {
               // wait until left mouse button is not pressed anymore
            }
         }
      }
      if (key[KEY_ESC])
      {
         save = FALSE;
         done = TRUE;
      }
      rectfill(glb_ds1edit.screen_buff, cancel_x0, ok_y0-3, cancel_x0 + 64, ok_y0 + 20, c1);
      rect(glb_ds1edit.screen_buff,     cancel_x0, ok_y0-3, cancel_x0 + 64, ok_y0 + 20, c2);
      textout(glb_ds1edit.screen_buff, font, "CANCEL", cancel_x0+10, ok_y0+5, c3);

      text_mode(0);

      // floors
      y = w_y0 + 2;
      for (n=0; n < glb_ds1[ds1_idx].floor_num; n++)
      {
         val[0] = ptr_wbitsdata->mix_f[n].prop1;
         val[1] = ptr_wbitsdata->mix_f[n].prop2;
         val[2] = ptr_wbitsdata->mix_f[n].prop3;
         val[3] = ptr_wbitsdata->mix_f[n].prop4;
         x = w_x0 + 24;
         for (b=3; b >= 0 ; b--)
         {
            for (i=7; i >= 0; i--)
            {
               if (ptr_wbitsdata->bit_type_f[n][b][i] == WBITSTYPE_DIFFERENT)
                  c = c_blue;
               else if (ptr_wbitsdata->bit_type_f[n][b][i] == WBITSTYPE_UNKNOWN)
                  c = c_unkn;
               else
                  c = c_white;

               if ((mx >= x) && (mx <= x+9) && (my >= y) && (my <= y+9) &&
                   (ptr_wbitsdata->bit_type_f[n][b][i] != WBITSTYPE_UNKNOWN))
               {
                  c  = c_red;
                  if (mb & 1)
                  {
                     if (val[b] & (1 << i))
                     {
                        // change to 0
                        val[b] = val[b] - (1 << i);
                     }
                     else
                     {
                        // change to 1
                        val[b] = val[b] + (1 << i);
                     }
                     while (mb & 1)
                     {
                        // wait until no mouse left button pressed
                        mb = mouse_b;
                     }
                     switch(b)
                     {
                        case 0 : ptr_wbitsdata->mix_f[n].prop1 = val[0]; break;
                        case 1 : ptr_wbitsdata->mix_f[n].prop2 = val[1]; break;
                        case 2 : ptr_wbitsdata->mix_f[n].prop3 = val[2]; break;
                        case 3 : ptr_wbitsdata->mix_f[n].prop4 = val[3]; break;
                     }
                     if (ptr_wbitsdata->bit_type_f[n][b][i] == WBITSTYPE_DIFFERENT)
                        ptr_wbitsdata->bit_type_f[n][b][i] = WBITSTYPE_SAME;
                  }
               }
               if (ptr_wbitsdata->bit_type_f[n][b][i] == WBITSTYPE_UNKNOWN)
               {
                  textout(glb_ds1edit.screen_buff, font, "-", x, y, c);
               }
               else
               {
                  if (val[b] & (1 << i))
                     textout(glb_ds1edit.screen_buff, font, "1", x, y, c);
                  else
                     textout(glb_ds1edit.screen_buff, font, "0", x, y, c);
               }

               x += 10;
               if ((b==3) && ((i==7) || (i==6) || (i==5) || (i==3) || (i==2)))
                  x+=10;
               else if ((b==2) && ((i==4) || (i==2) || (i==1) || (i==0)))
                  x+=10;
               else if ((b==1) && ((i==6) || (i==0)))
                  x+=10;
            }
         }
         y += 10;
      }

      // shadow layer
      y += 10 * (2 - glb_ds1[ds1_idx].floor_num);
      y += 10;
      for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
      {
         shad[0] = ptr_wbitsdata->mix_s[n].prop1;
         shad[1] = ptr_wbitsdata->mix_s[n].prop2;
         shad[2] = ptr_wbitsdata->mix_s[n].prop3;
         shad[3] = ptr_wbitsdata->mix_s[n].prop4;
         x = w_x0 + 24;
         for (b=3; b >= 0 ; b--)
         {
            for (i=7; i >= 0; i--)
            {
               if (ptr_wbitsdata->bit_type_s[n][b][i] == WBITSTYPE_DIFFERENT)
                  c = c_blue;
               else if (ptr_wbitsdata->bit_type_s[n][b][i] == WBITSTYPE_UNKNOWN)
                  c = c_unkn;
               else
                  c = c_white;

               if ((mx >= x) && (mx <= x+9) && (my >= y) && (my <= y+9) &&
                   (ptr_wbitsdata->bit_type_s[n][b][i] != WBITSTYPE_UNKNOWN))
               {
                  c  = c_red;
                  if (mb & 1)
                  {
                     if (shad[b] & (1 << i))
                     {
                        // change to 0
                        shad[b] = shad[b] - (1 << i);
                     }
                     else
                     {
                        // change to 1
                        shad[b] = shad[b] + (1 << i);
                     }
                     while (mb & 1)
                     {
                        // wait until no mouse button pressed
                        mb = mouse_b;
                     }
                     switch(b)
                     {
                        case 0 : ptr_wbitsdata->mix_s[n].prop1 = shad[0]; break;
                        case 1 : ptr_wbitsdata->mix_s[n].prop2 = shad[1]; break;
                        case 2 : ptr_wbitsdata->mix_s[n].prop3 = shad[2]; break;
                        case 3 : ptr_wbitsdata->mix_s[n].prop4 = shad[3]; break;
                     }
                     if (ptr_wbitsdata->bit_type_s[n][b][i] == WBITSTYPE_DIFFERENT)
                        ptr_wbitsdata->bit_type_s[n][b][i] = WBITSTYPE_SAME;
                  }
               }
               if (ptr_wbitsdata->bit_type_s[n][b][i] == WBITSTYPE_UNKNOWN)
               {
                  textout(glb_ds1edit.screen_buff, font, "-", x, y, c);
               }
               else
               {
                  if (shad[b] & (1 << i))
                     textout(glb_ds1edit.screen_buff, font, "1", x, y, c);
                  else
                     textout(glb_ds1edit.screen_buff, font, "0", x, y, c);
               }

               x += 10;
               if ((b==3) && ((i==7) || (i==6) || (i==5) || (i==3) || (i==2)))
                  x+=10;
               else if ((b==2) && ((i==4) || (i==2) || (i==1) || (i==0)))
                  x+=10;
               else if ((b==1) && ((i==6) || (i==0)))
                  x+=10;
            }
         }
         y += 10;
      }

      // walls
      y += 10;
      for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
      {
         val[0] = ptr_wbitsdata->mix_w[n].prop1;
         val[1] = ptr_wbitsdata->mix_w[n].prop2;
         val[2] = ptr_wbitsdata->mix_w[n].prop3;
         val[3] = ptr_wbitsdata->mix_w[n].prop4;
         x = w_x0 + 24;

         // props
         for (b=3; b >= 0 ; b--)
         {
            for (i=7; i >= 0; i--)
            {
               if (ptr_wbitsdata->bit_type_w[n][b][i] == WBITSTYPE_DIFFERENT)
                  c = c_blue;
               else if (ptr_wbitsdata->bit_type_w[n][b][i] == WBITSTYPE_UNKNOWN)
                  c = c_unkn;
               else
                  c = c_white;

               if ((mx >= x) && (mx <= x+9) && (my >= y) && (my <= y+9) &&
                   (ptr_wbitsdata->bit_type_w[n][b][i] != WBITSTYPE_UNKNOWN))
               {
                  c  = c_red;
                  if (mb & 1)
                  {
                     if (val[b] & (1 << i))
                     {
                        // change to 0
                        val[b] = val[b] - (1 << i);
                     }
                     else
                     {
                        // change to 1
                        val[b] = val[b] + (1 << i);
                     }
                     while (mb & 1)
                     {
                        // wait until no mouse button pressed
                        mb = mouse_b;
                     }
                     switch(b)
                     {
                        case 0 : ptr_wbitsdata->mix_w[n].prop1 = val[0]; break;
                        case 1 : ptr_wbitsdata->mix_w[n].prop2 = val[1]; break;
                        case 2 : ptr_wbitsdata->mix_w[n].prop3 = val[2]; break;
                        case 3 : ptr_wbitsdata->mix_w[n].prop4 = val[3]; break;
                     }
                     if (ptr_wbitsdata->bit_type_w[n][b][i] == WBITSTYPE_DIFFERENT)
                        ptr_wbitsdata->bit_type_w[n][b][i] = WBITSTYPE_SAME;
                  }
               }
               if (ptr_wbitsdata->bit_type_w[n][b][i] == WBITSTYPE_UNKNOWN)
               {
                  textout(glb_ds1edit.screen_buff, font, "-", x, y, c);
               }
               else
               {
                  if (val[b] & (1 << i))
                     textout(glb_ds1edit.screen_buff, font, "1", x, y, c);
                  else
                     textout(glb_ds1edit.screen_buff, font, "0", x, y, c);
               }

               x += 10;
               if ((b==3) && ((i==7) || (i==6) || (i==5) || (i==3) || (i==2)))
                  x+=10;
               else if ((b==2) && ((i==4) || (i==2) || (i==1) || (i==0)))
                  x+=10;
               else if ((b==1) && ((i==6) || (i==0)))
                  x+=10;
               else if ((b==0) && (i==0))
                  x+=10;
            }
         }

         // orientation
         for (i=7; i >= 0; i--)
         {
            if (ptr_wbitsdata->bit_type_w[n][4][i] == WBITSTYPE_DIFFERENT)
               c = c_blue;
            else if (ptr_wbitsdata->bit_type_w[n][4][i] == WBITSTYPE_UNKNOWN)
               c = c_unkn;
            else
               c = c_white;

            if ((mx >= x) && (mx <= x+9) && (my >= y) && (my <= y+9) &&
                (ptr_wbitsdata->bit_type_w[n][4][i] != WBITSTYPE_UNKNOWN))
            {
               c  = c_red;
               if (mb & 1)
               {
                  if (ptr_wbitsdata->mix_w[n].orientation & (1 << i))
                  {
                     // change to 0
                     ptr_wbitsdata->mix_w[n].orientation =
                        ptr_wbitsdata->mix_w[n].orientation - (1 << i);
                  }
                  else
                  {
                     // change to 1
                     ptr_wbitsdata->mix_w[n].orientation =
                        ptr_wbitsdata->mix_w[n].orientation + (1 << i);
                  }
                  while (mb & 1)
                  {
                     // wait until no mouse button pressed
                     mb = mouse_b;
                  }

                  if (ptr_wbitsdata->bit_type_w[n][4][i] == WBITSTYPE_DIFFERENT)
                     ptr_wbitsdata->bit_type_w[n][4][i] = WBITSTYPE_SAME;
               }
            }
            if (ptr_wbitsdata->bit_type_w[n][4][i] == WBITSTYPE_UNKNOWN)
            {
               textout(glb_ds1edit.screen_buff, font, "-", x, y, c);
            }
            else
            {
               if (ptr_wbitsdata->mix_w[n].orientation & (1 << i))
                  textout(glb_ds1edit.screen_buff, font, "1", x, y, c);
               else
                  textout(glb_ds1edit.screen_buff, font, "0", x, y, c);
            }
            x += 10;
         }

         y += 10;
      }

      // tag num (substitution layer)
      y += 10 * (4 - glb_ds1[ds1_idx].wall_num);
      y += 108;
      for (n=0; n<glb_ds1[ds1_idx].tag_num; n++)
      {
         tag_dw = ptr_wbitsdata->mix_t[n].num;
         x = w_x0 + 34;
         for (i=31; i >= 0; i--)
         {
            if (ptr_wbitsdata->bit_type_t[n][i] == WBITSTYPE_DIFFERENT)
               c = c_blue;
            else if (ptr_wbitsdata->bit_type_t[n][i] == WBITSTYPE_UNKNOWN)
               c = c_unkn;
            else
               c = c_white;

            if ((mx >= x) && (mx <= x+9) && (my >= y) && (my <= y+9) &&
                (ptr_wbitsdata->bit_type_t[n][i] != WBITSTYPE_UNKNOWN))
            {
               c  = c_red;
               if (mb & 1)
               {
                  if (tag_dw & (1 << i))
                  {
                     // change to 0
                     tag_dw = tag_dw - (1 << i);
                  }
                  else
                  {
                     // change to 1
                     tag_dw = tag_dw + (1 << i);
                  }
                  while (mb & 1)
                  {
                     // wait until no mouse button pressed
                     mb = mouse_b;
                  }

                  ptr_wbitsdata->mix_t[n].num = tag_dw;

                  if (ptr_wbitsdata->bit_type_t[n][i] == WBITSTYPE_DIFFERENT)
                     ptr_wbitsdata->bit_type_t[n][i] = WBITSTYPE_SAME;
               }
            }
            if (ptr_wbitsdata->bit_type_t[n][i] == WBITSTYPE_UNKNOWN)
            {
               textout(glb_ds1edit.screen_buff, font, "-", x, y, c);
            }
            else
            {
               if (tag_dw & (1 << i))
                  textout(glb_ds1edit.screen_buff, font, "1", x, y, c);
               else
                  textout(glb_ds1edit.screen_buff, font, "0", x, y, c);
            }
            x += 10;

            if ((i==8) || (i==16) || (i==24))
               x+=10;
         }
         y += 10;
         x = w_x0 + 48;
         textprintf(glb_ds1edit.screen_buff, font, x,       y, c_lightgreen, "%8lu", (tag_dw & 0xFF000000LU) >> 24);
         textprintf(glb_ds1edit.screen_buff, font, x + 90,  y, c_lightgreen, "%8lu", (tag_dw & 0x00FF0000LU) >> 16);
         textprintf(glb_ds1edit.screen_buff, font, x + 180, y, c_lightgreen, "%8lu", (tag_dw & 0x0000FF00LU) >> 8);
         textprintf(glb_ds1edit.screen_buff, font, x + 270, y, c_lightgreen, "%8lu",  tag_dw & 0x000000FFLU);
      }

      misc_draw_screen(mx, my);

      // new mouse state
      mx = mouse_x;
      my = mouse_y;
      mb = mouse_b;
   }

   if (save == FALSE)
   {
      // no changes
   }
   else
   {
      // set all new bits values to the selection, where needed
      wbits_apply_modification(ds1_idx, ptr_wbitsdata);

      // find again all tiles
      ds1_make_prop_2_block(ds1_idx);
   }

   if (key[KEY_ESC])
   {
      while (key[KEY_ESC])
      {
         // wait until Escape is released
      }
   }
   if (key[KEY_ENTER] || key[KEY_ENTER_PAD])
   {
      while (key[KEY_ENTER] || key[KEY_ENTER_PAD])
      {
         // wait until Enter is released
      }
   }

   // end
   text_mode(-1);
}


// ==========================================================================
void wbits_main(int ds1_idx, int tx, int ty)
{
   WBITSDATA_S * ptr_wbitsdata;
   CELL_F_S    * f_ptr;
   CELL_W_S    * w_ptr;
   CELL_S_S    * s_ptr;
   CELL_T_S    * t_ptr;
   int         x, y, t, i, size, b, b1, b2;
   int         has_selection;
   char        tmp[256];


   // init
   size = sizeof(WBITSDATA_S);
   ptr_wbitsdata = (WBITSDATA_S *) malloc(size);
   if (ptr_wbitsdata == NULL)
   {
      sprintf(tmp, "wbits_main() : can't alocate %i bytes for 'ptr_wbitsdata'\n", size);
      ds1edit_error(tmp);
   }
   memset(ptr_wbitsdata, 0, size);

   // fill ptr_wbitsdata, by analysing all tiles

   // first, search if there's a selection
   has_selection = FALSE;
   for (y=0; y < glb_ds1[ds1_idx].height; y++)
   {
      for (x=0; x < glb_ds1[ds1_idx].width; x++)
      {
         // floors
         t = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
         for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
         {
            f_ptr = glb_ds1[ds1_idx].floor_buff + t + i;
            if (IS_SELECTED(f_ptr->flags))
               has_selection = TRUE;
         }

         // shadows
         t = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
         for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
         {
            s_ptr = glb_ds1[ds1_idx].shadow_buff + t + i;
            if (IS_SELECTED(s_ptr->flags))
               has_selection = TRUE;
         }

         // walls
         t = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
         for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
         {
            w_ptr = glb_ds1[ds1_idx].wall_buff + t + i;
            if (IS_SELECTED(w_ptr->flags))
               has_selection = TRUE;
         }

         // tags
         t = (y * glb_ds1[ds1_idx].tag_line) + (x * glb_ds1[ds1_idx].tag_num);
         for (i=0; i < glb_ds1[ds1_idx].tag_num; i++)
         {
            t_ptr = glb_ds1[ds1_idx].tag_buff + t + i;
            if (IS_SELECTED(t_ptr->flags))
               has_selection = TRUE;
         }
      }
   }

   if (has_selection == FALSE)
   {
      // only the tile under the mouse was 'selected'
      // --> use the normal bitfield edition window
      wbits_main_single_tile(ds1_idx, tx, ty);
   }
   else
   {
      // there were at least 1 layer of 1 tile selected
      // --> use the advanced bitfield edition window, that works
      //     ONLY on the selection (not necessary on the tile where
      //     the mouse is)

      // but first, fill up ptr_wbitsdata
      for (y=0; y < glb_ds1[ds1_idx].height; y++)
      {
         for (x=0; x < glb_ds1[ds1_idx].width; x++)
         {
            // floors
            t = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
            for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
            {
               f_ptr = glb_ds1[ds1_idx].floor_buff + t + i;
               if (IS_SELECTED(f_ptr->flags))
               {
                  // add this layer into the bitfield window data

                  if (IS_SELECTED(ptr_wbitsdata->mix_f[i].flags))
                  {
                     // we can compare with precedent layer

                     // for all bits of prop1
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_f[i].prop1 & (1 << b);
                        b2 = f_ptr->prop1 & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_f[i][0][b] = WBITSTYPE_DIFFERENT;
                     }

                     // for all bits of prop2
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_f[i].prop2 & (1 << b);
                        b2 = f_ptr->prop2 & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_f[i][1][b] = WBITSTYPE_DIFFERENT;
                     }

                     // for all bits of prop3
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_f[i].prop3 & (1 << b);
                        b2 = f_ptr->prop3 & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_f[i][2][b] = WBITSTYPE_DIFFERENT;
                     }

                     // for all bits of prop4
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_f[i].prop4 & (1 << b);
                        b2 = f_ptr->prop4 & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_f[i][3][b] = WBITSTYPE_DIFFERENT;
                     }
                  }
                  else
                  {
                     SET_SELECTED(ptr_wbitsdata->mix_f[i].flags);

                     // this is the 1st time we meet this layer
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_f[i][0][b] = WBITSTYPE_SAME;
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_f[i][1][b] = WBITSTYPE_SAME;
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_f[i][2][b] = WBITSTYPE_SAME;
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_f[i][3][b] = WBITSTYPE_SAME;
                  }

                  // mix the bits values
                  ptr_wbitsdata->mix_f[i].prop1 |= f_ptr->prop1;
                  ptr_wbitsdata->mix_f[i].prop2 |= f_ptr->prop2;
                  ptr_wbitsdata->mix_f[i].prop3 |= f_ptr->prop3;
                  ptr_wbitsdata->mix_f[i].prop4 |= f_ptr->prop4;
               }
            }

            // shadows
            t = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
            for (i=0; i < glb_ds1[ds1_idx].shadow_num; i++)
            {
               s_ptr = glb_ds1[ds1_idx].shadow_buff + t + i;
               if (IS_SELECTED(s_ptr->flags))
               {
                  // add this layer into the bitfield window data

                  if (IS_SELECTED(ptr_wbitsdata->mix_s[i].flags))
                  {
                     // we can compare with precedent layer

                     // for all bits of prop1
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_s[i].prop1 & (1 << b);
                        b2 = s_ptr->prop1 & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_s[i][0][b] = WBITSTYPE_DIFFERENT;
                     }

                     // for all bits of prop2
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_s[i].prop2 & (1 << b);
                        b2 = s_ptr->prop2 & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_s[i][1][b] = WBITSTYPE_DIFFERENT;
                     }

                     // for all bits of prop3
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_s[i].prop3 & (1 << b);
                        b2 = s_ptr->prop3 & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_s[i][2][b] = WBITSTYPE_DIFFERENT;
                     }

                     // for all bits of prop4
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_s[i].prop4 & (1 << b);
                        b2 = s_ptr->prop4 & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_s[i][3][b] = WBITSTYPE_DIFFERENT;
                     }
                  }
                  else
                  {
                     SET_SELECTED(ptr_wbitsdata->mix_s[i].flags);

                     // this is the 1st time we meet this layer
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_s[i][0][b] = WBITSTYPE_SAME;
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_s[i][1][b] = WBITSTYPE_SAME;
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_s[i][2][b] = WBITSTYPE_SAME;
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_s[i][3][b] = WBITSTYPE_SAME;
                  }

                  // mix the bits values
                  ptr_wbitsdata->mix_s[i].prop1 |= s_ptr->prop1;
                  ptr_wbitsdata->mix_s[i].prop2 |= s_ptr->prop2;
                  ptr_wbitsdata->mix_s[i].prop3 |= s_ptr->prop3;
                  ptr_wbitsdata->mix_s[i].prop4 |= s_ptr->prop4;
               }
            }

            // walls
            t = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
            for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
            {
               w_ptr = glb_ds1[ds1_idx].wall_buff + t + i;
               if (IS_SELECTED(w_ptr->flags))
               {
                  // add this layer into the bitfield window data

                  if (IS_SELECTED(ptr_wbitsdata->mix_w[i].flags))
                  {
                     // we can compare with precedent layer

                     // for all bits of prop1
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_w[i].prop1 & (1 << b);
                        b2 = w_ptr->prop1 & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_w[i][0][b] = WBITSTYPE_DIFFERENT;
                     }

                     // for all bits of prop2
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_w[i].prop2 & (1 << b);
                        b2 = w_ptr->prop2 & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_w[i][1][b] = WBITSTYPE_DIFFERENT;
                     }

                     // for all bits of prop3
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_w[i].prop3 & (1 << b);
                        b2 = w_ptr->prop3 & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_w[i][2][b] = WBITSTYPE_DIFFERENT;
                     }

                     // for all bits of prop4
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_w[i].prop4 & (1 << b);
                        b2 = w_ptr->prop4 & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_w[i][3][b] = WBITSTYPE_DIFFERENT;
                     }

                     // for all bits of orientation
                     for (b=0; b < 8; b++)
                     {
                        b1 = ptr_wbitsdata->mix_w[i].orientation  & (1 << b);
                        b2 = w_ptr->orientation & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_w[i][4][b] = WBITSTYPE_DIFFERENT;
                     }
                  }
                  else
                  {
                     SET_SELECTED(ptr_wbitsdata->mix_w[i].flags);

                     // this is the 1st time we meet this layer
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_w[i][0][b] = WBITSTYPE_SAME;
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_w[i][1][b] = WBITSTYPE_SAME;
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_w[i][2][b] = WBITSTYPE_SAME;
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_w[i][3][b] = WBITSTYPE_SAME;
                     for (b=0; b < 8; b++)
                        ptr_wbitsdata->bit_type_w[i][4][b] = WBITSTYPE_SAME;
                  }

                  // mix the bits values
                  ptr_wbitsdata->mix_w[i].prop1 |= w_ptr->prop1;
                  ptr_wbitsdata->mix_w[i].prop2 |= w_ptr->prop2;
                  ptr_wbitsdata->mix_w[i].prop3 |= w_ptr->prop3;
                  ptr_wbitsdata->mix_w[i].prop4 |= w_ptr->prop4;
                  ptr_wbitsdata->mix_w[i].orientation |= w_ptr->orientation;
               }
            }

            // tags
            t = (y * glb_ds1[ds1_idx].tag_line) + (x * glb_ds1[ds1_idx].tag_num);
            for (i=0; i < glb_ds1[ds1_idx].tag_num; i++)
            {
               t_ptr = glb_ds1[ds1_idx].tag_buff + t + i;
               if (IS_SELECTED(t_ptr->flags))
               {
                  // add this layer into the bitfield window data

                  if (IS_SELECTED(ptr_wbitsdata->mix_t[i].flags))
                  {
                     // we can compare with precedent layer

                     // for all bits of tag num
                     for (b=0; b < 32; b++)
                     {
                        b1 = ptr_wbitsdata->mix_t[i].num & (1 << b);
                        b2 = t_ptr->num & (1 << b);
                        if (b1 != b2)
                           ptr_wbitsdata->bit_type_t[i][b] = WBITSTYPE_DIFFERENT;
                     }
                  }
                  else
                  {
                     SET_SELECTED(ptr_wbitsdata->mix_t[i].flags);

                     // this is the 1st time we meet this layer
                     for (b=0; b < 32; b++)
                        ptr_wbitsdata->bit_type_t[i][b] = WBITSTYPE_SAME;
                  }

                  // mix the bits values
                  ptr_wbitsdata->mix_t[i].num |= t_ptr->num;
               }
            }
         }
      }

      // let the user edit the bits
      wbits_main_multiple_tiles(ds1_idx, ptr_wbitsdata);
   }

   // end
   free(ptr_wbitsdata);
}
