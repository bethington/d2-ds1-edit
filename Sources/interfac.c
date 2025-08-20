#include "structs.h"
#include "wPreview.h"
#include "editobj.h"
#include "misc.h"
#include "ds1save.h"
#include "msg_save.h"
#include "edittile.h"
#include "undo.h"
#include "animdata.h"
#include "anim.h"
#include "txtread.h"
#include "wBits.h"
#include "wEdit.h"
#include "msg_quit.h"
#include "interfac.h"


// ==========================================================================
// MAIN loop
void interfac_user_handler(int start_ds1_idx)
{
   int  ds1_idx, done, cx, cy, n, i, dx, dy, old_ds1_idx=0;
   int  old_mouse_x = mouse_x, old_mouse_y=mouse_y, old_mouse_b=0;
   int  cur_mouse_z = 0, old_mouse_z = 0;
   int  old_cell_x = -1, old_cell_y = -1;
   int  old_identical_x = -1, old_identical_y = -1;
   int  ticks_elapsed, ret;
   int  can_swich_mode, key_func_code[7] = {KEY_F1, KEY_F2,
                            KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F11};
   TMP_SEL_S   tmp_sel;
   PASTE_POS_S paste_pos;
   char        tmp[150];
   MODE_E      old_mode = 0;
   IT_ENUM     itype = IT_NULL;
   int         group_changed, old_group, found;
   BITMAP      * old_screen_buff = NULL;
 

   // init
   tmp_sel.x1            = tmp_sel.x2     = tmp_sel.y1 = tmp_sel.y2 = 0;
   tmp_sel.old_x2        = tmp_sel.old_y2 = 0;
   tmp_sel.type          = TMP_NULL;
   tmp_sel.start         = FALSE;
   paste_pos.old_ds1_idx = 0;
   paste_pos.old_x       = 0;
   paste_pos.old_y       = 0;
   paste_pos.src_ds1_idx = 0;
   paste_pos.start_x     = 0;
   paste_pos.start_y     = 0;
   paste_pos.start       = FALSE;
   paste_pos.is_cut      = FALSE;
   paste_pos.cut_saved   = FALSE;
   ds1_idx               = start_ds1_idx;
   done                  = FALSE;
   

   // main loop
   while (! done)
   {
      can_swich_mode = TRUE;
      if (glb_ds1edit.mode == MOD_P)
      {
         if (glb_ds1[ds1_idx].path_edit_win.mode != PEM_NONE)
            can_swich_mode = FALSE;
         else if (glb_ds1[ds1_idx].path_edit_win.obj_dirty == TRUE)
            can_swich_mode = FALSE;
      }

      old_mouse_x = mouse_x;
      old_mouse_y = mouse_y;
      old_mouse_b = mouse_b;

      cur_mouse_z = mouse_z;

      // keep the current mouse coordinates for the entire loop process
      
      // which tile (or sub-tile) is RIGHT NOW under the mouse ?
      mouse_to_tile(ds1_idx, &cx, &cy);
      if (glb_ds1edit.mode == MOD_T)
      {
         if (cx < 0)
            cx = 0;
         else if (cx >= glb_ds1[ds1_idx].width)
            cx = glb_ds1[ds1_idx].width - 1;
         if (cy < 0)
            cy = 0;
         else if (cy >= glb_ds1[ds1_idx].height)
            cy = glb_ds1[ds1_idx].height - 1;
      }
      else
      {
         cx -= 2;
         cy += 2;
         if (cx < 0)
            cx = 0;
         else if (cx >= glb_ds1[ds1_idx].width * 5 - 1)
            cx = glb_ds1[ds1_idx].width * 5 - 1;
         if (cy < 0)
            cy = 0;
         else if (cy >= glb_ds1[ds1_idx].height * 5 - 1)
            cy = glb_ds1[ds1_idx].height * 5 - 1;
      }
            
      if (glb_ds1edit.mode == MOD_O)
      {
         editobj_handler(
            ds1_idx, cx,
            cy,
            old_mouse_x,
            old_mouse_y,
            old_mouse_b
         );
      }
      
      if ((cx != old_cell_x) || (cy != old_cell_y))
      {
         old_cell_x = cx;
         old_cell_y = cy;
         old_identical_x = -1;
         old_identical_y = -1;
      }

      // check if need to redraw the screen because of floor animation
      ticks_elapsed = glb_ds1edit.ticks_elapsed;
      if ( ticks_elapsed && (glb_ds1[ds1_idx].animations_layer_mask == 1))
      {
         // animated floor rate = 10 fps
         // therefore it's at 2/5 of 25 fps
         // but internal unit is in 5th
         glb_ds1[ds1_idx].cur_anim_floor_frame += ticks_elapsed * 2;
      }
      else
         glb_ds1edit.ticks_elapsed = 0;

      // redraw the whole screen
      wpreview_draw_tiles(ds1_idx);
      glb_ds1edit.fps++;
      
      // scroll UP / DOWN / LEFT / RIGHT

      // if the Object Editing Window is display
      if (glb_ds1[ds1_idx].draw_edit_obj == TRUE)
      {
         if (glb_config.winobj_scroll_keyb == TRUE)
         {
            // can scroll by keyboard
            if (key[KEY_UP])
            {
               glb_ds1edit.win_preview.y0 -= glb_ds1[ds1_idx].cur_scroll.keyb.y;
            }
            if (key[KEY_DOWN])
            {
               glb_ds1edit.win_preview.y0 += glb_ds1[ds1_idx].cur_scroll.keyb.y;
            }
            if (key[KEY_LEFT])
            {
               glb_ds1edit.win_preview.x0 -= glb_ds1[ds1_idx].cur_scroll.keyb.x;
            }
            if (key[KEY_RIGHT])
            {
               glb_ds1edit.win_preview.x0 += glb_ds1[ds1_idx].cur_scroll.keyb.x;
            }
         }

         if (glb_config.winobj_scroll_mouse == TRUE)
         {
            // can scroll by mouse
            if (old_mouse_y == 0)
            {
               glb_ds1edit.win_preview.y0 -= glb_ds1[ds1_idx].cur_scroll.mouse.y;
            }
            if (old_mouse_y == glb_config.screen.height - 1)
            {
               glb_ds1edit.win_preview.y0 += glb_ds1[ds1_idx].cur_scroll.mouse.y;
            }
            if (old_mouse_x == 0)
            {
               glb_ds1edit.win_preview.x0 -= glb_ds1[ds1_idx].cur_scroll.mouse.x;
            }
            if (old_mouse_x == glb_config.screen.width  - 1)
            {
               glb_ds1edit.win_preview.x0 += glb_ds1[ds1_idx].cur_scroll.mouse.x;
            }
         }
      }
      else
      {
         if (key[KEY_UP])
         {
            glb_ds1edit.win_preview.y0 -= glb_ds1[ds1_idx].cur_scroll.keyb.y;
         }
         else if (old_mouse_y == 0)
         {
            glb_ds1edit.win_preview.y0 -= glb_ds1[ds1_idx].cur_scroll.mouse.y;
         }

         if (key[KEY_DOWN])
         {
            glb_ds1edit.win_preview.y0 += glb_ds1[ds1_idx].cur_scroll.keyb.y;
         }
         else if (old_mouse_y == glb_config.screen.height - 1)
         {
            glb_ds1edit.win_preview.y0 += glb_ds1[ds1_idx].cur_scroll.mouse.y;
         }

         if (key[KEY_LEFT])
         {
            glb_ds1edit.win_preview.x0 -= glb_ds1[ds1_idx].cur_scroll.keyb.x;
         }
         else if (old_mouse_x == 0)
         {
            glb_ds1edit.win_preview.x0 -= glb_ds1[ds1_idx].cur_scroll.mouse.x;
         }

         if (key[KEY_RIGHT])
         {
            glb_ds1edit.win_preview.x0 += glb_ds1[ds1_idx].cur_scroll.keyb.x;
         }
         else if (old_mouse_x == glb_config.screen.width  - 1)
         {
            glb_ds1edit.win_preview.x0 += glb_ds1[ds1_idx].cur_scroll.mouse.x;
         }
      }

      // zoom
      if ( (key[KEY_MINUS_PAD] || key[KEY_MINUS] || (cur_mouse_z < old_mouse_z) ) &&
           glb_ds1[ds1_idx].cur_zoom < ZM_116)
      {
         if (key[KEY_MINUS_PAD] || key[KEY_MINUS])
         {
            while(key[KEY_MINUS_PAD] || key[KEY_MINUS])
            {
               // wait until the MINUS key is released
            }
         }
         glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0;
         glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0;
         glb_ds1[ds1_idx].own_wpreview.w  = glb_ds1edit.win_preview.w;
         glb_ds1[ds1_idx].own_wpreview.h  = glb_ds1edit.win_preview.h;
         change_zoom(ds1_idx, glb_ds1[ds1_idx].cur_zoom + 1);
         glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
         glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
      }

      if ( (key[KEY_PLUS_PAD] || key[KEY_EQUALS] || (cur_mouse_z > old_mouse_z) ) &&
           glb_ds1[ds1_idx].cur_zoom > ZM_11)
      {
         if (key[KEY_PLUS_PAD] || key[KEY_EQUALS])
         {
            while(key[KEY_PLUS_PAD] || key[KEY_EQUALS])
            {
               // wait until the '+' and '=' key are released
            }
         }
         glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0;
         glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0;
         glb_ds1[ds1_idx].own_wpreview.w  = glb_ds1edit.win_preview.w;
         glb_ds1[ds1_idx].own_wpreview.h  = glb_ds1edit.win_preview.h;
         change_zoom(ds1_idx, glb_ds1[ds1_idx].cur_zoom - 1);
         glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
         glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
      }

      if (old_mouse_z != cur_mouse_z)
      {
         old_mouse_z = cur_mouse_z;
         if (mouse_b & 4)
         {
            if (glb_ds1edit.mode == MOD_T)
            {
               // Center to mouse in TILE mode
               cx++;
               dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
               dy = (cy *  glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);
               cx--;
               glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0 =
                  dx - glb_ds1edit.win_preview.w / 2;
               glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0 =
                  dy - glb_ds1edit.win_preview.h / 2;
               glb_ds1[ds1_idx].own_wpreview.w  = glb_ds1edit.win_preview.w;
               glb_ds1[ds1_idx].own_wpreview.h  = glb_ds1edit.win_preview.h;
               glb_ds1edit.win_preview.x0   = glb_ds1[ds1_idx].own_wpreview.x0;
               glb_ds1edit.win_preview.y0   = glb_ds1[ds1_idx].own_wpreview.y0;
               position_mouse(glb_ds1edit.win_preview.w / 2, glb_ds1edit.win_preview.h / 2);
            }
            else
            {
               // Center to mouse in OBJECT / PATH mode
               cx /= 5;
               cy /= 5;
               cx++;
               dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
               dy = (cy *  glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);
               cx--;
               cx *= 5;
               cy *= 5;
               glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0 =
                  dx - glb_ds1edit.win_preview.w / 2;
               glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0 =
                  dy - glb_ds1edit.win_preview.h / 2;
               glb_ds1[ds1_idx].own_wpreview.w  = glb_ds1edit.win_preview.w;
               glb_ds1[ds1_idx].own_wpreview.h  = glb_ds1edit.win_preview.h;
               glb_ds1edit.win_preview.x0   = glb_ds1[ds1_idx].own_wpreview.x0;
               glb_ds1edit.win_preview.y0   = glb_ds1[ds1_idx].own_wpreview.y0;
               position_mouse(glb_ds1edit.win_preview.w / 2, glb_ds1edit.win_preview.h / 2);
            }
         }
      }

      // layers toggle
      if (key[KEY_LSHIFT] || key[KEY_RSHIFT])
      {
         // if shift pressed, just 1 layer will be active
         for (n=0; n<7; n++)
         {
            if (key[key_func_code[n]])
            {
               for (i=0; i<FLOOR_MAX_LAYER; i++)
                  glb_ds1[ds1_idx].floor_layer_mask[i] = 0;

               if (key[KEY_F11])
               {
                  for (i=0; i<SHADOW_MAX_LAYER; i++)
                     glb_ds1[ds1_idx].shadow_layer_mask[i] = 3;
               }
               else
               {
                  for (i=0; i<SHADOW_MAX_LAYER; i++)
                     glb_ds1[ds1_idx].shadow_layer_mask[i] = 0;
               }

               for (i=0; i<WALL_MAX_LAYER; i++)
                  glb_ds1[ds1_idx].wall_layer_mask[i] = 0;

               break;
            }
         }
      }
      if (key[KEY_LCONTROL] || key[KEY_RCONTROL])
      {
         // if control pressed, just 1 layer will be inactive
         for (n=0; n<7; n++)
         {
            if (key[key_func_code[n]])
            {
               for (i=0; i<FLOOR_MAX_LAYER; i++)
                  glb_ds1[ds1_idx].floor_layer_mask[i] = 1;

               for (i=0; i<SHADOW_MAX_LAYER; i++)
                  glb_ds1[ds1_idx].shadow_layer_mask[i] = 3;

               for (i=0; i<WALL_MAX_LAYER; i++)
                  glb_ds1[ds1_idx].wall_layer_mask[i] = 1;

               break;
            }
         }
      }
      if (key[KEY_F1])
      {
         while(key[KEY_F1])
         {
            // wait until the 'F' key is released
         }
         glb_ds1[ds1_idx].floor_layer_mask[0] = 1 - glb_ds1[ds1_idx].floor_layer_mask[0];
      }
      if (key[KEY_F2])
      {
         while(key[KEY_F2])
         {
            // wait until the 'F2' key is released
         }
         glb_ds1[ds1_idx].floor_layer_mask[1] = 1 - glb_ds1[ds1_idx].floor_layer_mask[1];
      }
      if (key[KEY_F5])
      {
         while(key[KEY_F5])
         {
            // wait until the 'F5' key is released
         }
         glb_ds1[ds1_idx].wall_layer_mask[0] = 1 - glb_ds1[ds1_idx].wall_layer_mask[0];
      }
      if (key[KEY_F6])
      {
         while(key[KEY_F6])
         {
            // wait until the 'F6' key is released
         }
         glb_ds1[ds1_idx].wall_layer_mask[1] = 1 - glb_ds1[ds1_idx].wall_layer_mask[1];
      }
      if (key[KEY_F7])
      {
         while(key[KEY_F7])
         {
            // wait until the 'F7' key is released
         }
         glb_ds1[ds1_idx].wall_layer_mask[2] = 1 - glb_ds1[ds1_idx].wall_layer_mask[2];
      }
      if (key[KEY_F8])
      {
         while(key[KEY_F8])
         {
            // wait until the 'F8' key is released
         }
         glb_ds1[ds1_idx].wall_layer_mask[3] = 1 - glb_ds1[ds1_idx].wall_layer_mask[3];
      }

      // special tiles layer
      if (key[KEY_F9])
      {
         while(key[KEY_F9])
         {
            // wait until the 'F9' key is released
         }
         glb_ds1[ds1_idx].special_layer_mask = 1 - glb_ds1[ds1_idx].special_layer_mask;
      }
      
      // animation layer
      if (key[KEY_F3])
      {
         while(key[KEY_F3])
         {
            // wait until the 'F3' key is released
         }
         glb_ds1[ds1_idx].animations_layer_mask++;
         if (glb_ds1[ds1_idx].animations_layer_mask == 3)
            glb_ds1[ds1_idx].animations_layer_mask = 0;
      }
      
      // objects layer
      if (key[KEY_F4] && (glb_ds1edit.mode != MOD_O))
      {
         while(key[KEY_F4])
         {
            // wait until the 'F4' key is released
         }
         glb_ds1[ds1_idx].objects_layer_mask++;
         if (glb_ds1[ds1_idx].objects_layer_mask >= OL_MAX)
            glb_ds1[ds1_idx].objects_layer_mask = OL_NONE;
      }

      // paths layer
      if (key[KEY_F10] && (glb_ds1edit.mode != MOD_P))
      {
         while(key[KEY_F10])
         {
            // wait until the 'F10' key is released
         }
         glb_ds1[ds1_idx].paths_layer_mask = 1 - glb_ds1[ds1_idx].paths_layer_mask;
      }

      // shadow mode
      if (key[KEY_F11])
      {
         while(key[KEY_F11])
         {
            // wait until the 'F11' key is released
         }
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

      // walkable infos
      if (key[KEY_SPACE])
      {
         while(key[KEY_SPACE])
         {
            // wait until the SPACE key is released
         }
         glb_ds1[ds1_idx].walkable_layer_mask++;
         if (glb_ds1[ds1_idx].walkable_layer_mask >= 3)
            glb_ds1[ds1_idx].walkable_layer_mask = 0;
      }
      if (key[KEY_T])
      {
         while(key[KEY_T])
         {
            // wait until the 'T' key is released
         }
         glb_ds1[ds1_idx].subtile_help_display =
            1 - glb_ds1[ds1_idx].subtile_help_display;
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

      // Home (center the map)
      if (key[KEY_HOME])
      {
         while (key[KEY_HOME])
         {
            // wait until the HOME key is released
         }
         cx = glb_ds1[ds1_idx].width/2 + 1;
         cy = glb_ds1[ds1_idx].height/2;
         dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
         dy = (cy *  glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);
         glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0 = dx - glb_ds1edit.win_preview.w / 2;
         glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0 = dy - glb_ds1edit.win_preview.h / 2;
      }

      // Backspace (show all layers)
      if (key[KEY_BACKSPACE])
      {
         while (key[KEY_BACKSPACE])
         {
            // wait until the BACKSPACE key is released
         }
         for (i=0; i<FLOOR_MAX_LAYER; i++)
            glb_ds1[ds1_idx].floor_layer_mask[i]  = 1;
         for (i=0; i<WALL_MAX_LAYER; i++)
            glb_ds1[ds1_idx].wall_layer_mask[i]   = 1;
         for (i=0; i<SHADOW_MAX_LAYER; i++)
            glb_ds1[ds1_idx].shadow_layer_mask[i] = 3;
      }

      // P ('P'rintscreen = screenshot)
      if (key[KEY_P])
      {
         if ((key[KEY_LSHIFT] || key[KEY_RSHIFT]))
         {
            // BIG screenshot (complete map)
            old_screen_buff = glb_ds1edit.screen_buff;
            if (wpreview_draw_tiles_big_screenshot(ds1_idx) == 0)
            {
               // big screenshot is ready
               sprintf(tmp, "screenshot-%05i.bmp", glb_ds1edit.screenshot_num);
               while (file_exists(tmp, -1, NULL))
               {
                  glb_ds1edit.screenshot_num++;
                  sprintf(tmp, "screenshot-%05i.bmp", glb_ds1edit.screenshot_num);
               }

               // handle palette
               if (glb_ds1edit.cmd_line.force_pal_num == -1)
               {
                  // use .ds1 act value for palette
                  save_bmp(tmp, glb_ds1edit.screen_buff, glb_ds1edit.vga_pal[glb_ds1[ds1_idx].act - 1]);
               }
               else
               {
                  // use force_pal value for palette
                  save_bmp(tmp, glb_ds1edit.screen_buff, glb_ds1edit.vga_pal[glb_ds1edit.cmd_line.force_pal_num - 1]);
               }

               // free temp bitmap
               destroy_bitmap(glb_ds1edit.screen_buff);
            }
            while ((key[KEY_LSHIFT] || key[KEY_RSHIFT]))
            {
               // wait until Shift key is relased
            }
            glb_ds1edit.screen_buff = old_screen_buff;
         }
         else
         {
            // normal screenshot (visible screen only)
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
               old_mouse_x - 1,
               old_mouse_y - 1
            );

            // handle palette
            if (glb_ds1edit.cmd_line.force_pal_num == -1)
            {
               // use .ds1 act value for palette
               save_pcx(tmp, glb_ds1edit.screen_buff, glb_ds1edit.vga_pal[glb_ds1[ds1_idx].act - 1]);
               misc_pcx_put_d2_palette(tmp, glb_ds1[ds1_idx].act - 1);
            }
            else
            {
               // use force_pal value for palette
               save_pcx(tmp, glb_ds1edit.screen_buff, glb_ds1edit.vga_pal[glb_ds1edit.cmd_line.force_pal_num - 1]);
               misc_pcx_put_d2_palette(tmp, glb_ds1edit.cmd_line.force_pal_num - 1);
            }
         }
         while (key[KEY_P])
         {
            // wait until P is released
         }

         // the buffer was saved
         glb_ds1edit.screenshot_num++;
      }
      
      // S
      if (key[KEY_S])
      {
         if (key[KEY_LCONTROL] || key[KEY_RCONTROL])
         {
            // CTRL + S : save the ds1, in the current state, incremental save
            ds1_save(ds1_idx, FALSE);
            while (key[KEY_S])
            {
               // wait until the S key is not pressed anymore
            }
            ret = msg_save_main();
            switch (ret)
            {
               case -1 :
                  // error
                  done = TRUE;
                  break;

               case 0 :
                  // ok
                  break;
            }
         }
         else if (glb_ds1edit.mode == MOD_T)
         {
            // (Show all precedently hiden tiles)
            while (key[KEY_S])
            {
               // wait until the 'S' key is released
            }
            edittile_unhide_all(ds1_idx);
         }
      }
      
      // 'C' : either Copy or Center
      if (key[KEY_C])
      {
         if (glb_ds1edit.mode == MOD_T)
         {
            // TILE mode
            if (key[KEY_LCONTROL] || key[KEY_RCONTROL])
            {
               // CTRL + C : copy selected layers (copy / paste)
               if (paste_pos.start == FALSE)
               {
                  for (i=0; i<DS1_MAX; i++)
                  {
                    if (strlen(glb_ds1[i].name))
                        edittile_paste_prepare(i);
                  }
                  paste_pos.src_ds1_idx = ds1_idx;
                  paste_pos.old_ds1_idx = ds1_idx;
                  paste_pos.start       = TRUE;
                  paste_pos.is_cut      = FALSE; // just a 'COPY'
                  paste_pos.cut_saved   = FALSE;
                  paste_pos.old_x       = cx;
                  paste_pos.old_y       = cy;
                  edittile_middle_select(
                     ds1_idx,
                     & paste_pos.start_x,
                     & paste_pos.start_y
                  );
                  edittile_paste_preview(ds1_idx,
                     cx - paste_pos.start_x,
                     cy - paste_pos.start_y,
                     & paste_pos
                  );

                  while (key[KEY_C])
                  {
                     // wait until the 'C' key is released
                  }
               }
            }
            else
            {
               // Center to mouse in TILE mode
               while (key[KEY_C])
               {
                  // wait until 'C' is released
               }
               cx++;
               dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
               dy = (cy *  glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);
               cx--;
               glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0 =
                  dx - glb_ds1edit.win_preview.w / 2;
               glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0 =
                  dy - glb_ds1edit.win_preview.h / 2;
               glb_ds1[ds1_idx].own_wpreview.w  = glb_ds1edit.win_preview.w;
               glb_ds1[ds1_idx].own_wpreview.h  = glb_ds1edit.win_preview.h;
               if (glb_config.center_zoom != -1)
                  change_zoom(ds1_idx, glb_config.center_zoom);
               glb_ds1edit.win_preview.x0   = glb_ds1[ds1_idx].own_wpreview.x0;
               glb_ds1edit.win_preview.y0   = glb_ds1[ds1_idx].own_wpreview.y0;
               position_mouse(glb_ds1edit.win_preview.w / 2, glb_ds1edit.win_preview.h / 2);
            }
         }
         else
         {
            if ( ! key[KEY_LCONTROL] && ! key[KEY_RCONTROL])
            {
               // Center to mouse in OBJECT / PATH mode
               while (key[KEY_C])
               {
                  // wait until the 'C' key is released
               }
               cx /= 5;
               cy /= 5;
               cx++;
               dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
               dy = (cy *  glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);
               cx--;
               cx *= 5;
               cy *= 5;
               glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0 =
                  dx - glb_ds1edit.win_preview.w / 2;
               glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0 =
                  dy - glb_ds1edit.win_preview.h / 2;
               glb_ds1[ds1_idx].own_wpreview.w  = glb_ds1edit.win_preview.w;
               glb_ds1[ds1_idx].own_wpreview.h  = glb_ds1edit.win_preview.h;
               if (glb_config.center_zoom != -1)
                  change_zoom(ds1_idx, glb_config.center_zoom);
               glb_ds1edit.win_preview.x0   = glb_ds1[ds1_idx].own_wpreview.x0;
               glb_ds1edit.win_preview.y0   = glb_ds1[ds1_idx].own_wpreview.y0;
               position_mouse(glb_ds1edit.win_preview.w / 2, glb_ds1edit.win_preview.h / 2);
            }
         }
      }

      // CTRL + X : Copy selected tiles, WITH CUT (crop / paste)
      if (key[KEY_X] && (key[KEY_LCONTROL] || key[KEY_RCONTROL]))
      {
         if (glb_ds1edit.mode == MOD_T)
         {
            // TILE mode
            if (paste_pos.start == FALSE)
            {
               for (i=0; i<DS1_MAX; i++)
               {
                 if (strlen(glb_ds1[i].name))
                     edittile_paste_prepare(i);
               }
               paste_pos.src_ds1_idx = ds1_idx;
               paste_pos.old_ds1_idx = ds1_idx;
               paste_pos.start       = TRUE;
               paste_pos.is_cut      = TRUE; // copy with 'CUT'
               paste_pos.cut_saved   = FALSE;
               paste_pos.old_x       = cx;
               paste_pos.old_y       = cy;
               edittile_middle_select(
                  ds1_idx,
                  & paste_pos.start_x,
                  & paste_pos.start_y
               );
               edittile_paste_preview(ds1_idx,
                  cx - paste_pos.start_x,
                  cy - paste_pos.start_y,
                  & paste_pos
               );
               while (key[KEY_X])
               {
                  // wait until the X key is released
               }
            }
         }
      }

      // DEL key (regular or keypad) : delete all selected layers of all tiles
      if (key[KEY_DEL] || key[KEY_DEL_PAD])
      {
         if (glb_ds1edit.mode == MOD_T)
         {
            if (paste_pos.start == FALSE)
            {
               edittile_delete_selected_tiles(ds1_idx);
               while (key[KEY_DEL] || key[KEY_DEL_PAD])
               {
                  // wait until both DEL keys are released
               }
            }
         }
      }

      // CTRL + U : undo tiles modification
      if (key[KEY_U] && (key[KEY_LCONTROL] || key[KEY_RCONTROL]))
      {
         if (glb_ds1edit.mode == MOD_T)
         {
            if (paste_pos.start == FALSE)
            {
               undo_apply_tile_buffer(ds1_idx);
               while (key[KEY_U])
               {
                  // wait until the 'U' key is released
               }
            }
         }
      }

      // G : toggle tile grid
      if (key[KEY_G])
      {
         if (key[KEY_LSHIFT] || key[KEY_RSHIFT])
            glb_ds1edit.display_tile_grid --;
         else
            glb_ds1edit.display_tile_grid ++;
         if (glb_ds1edit.display_tile_grid < TG_OFF)
            glb_ds1edit.display_tile_grid = TG_MAX - 1;
         if (glb_ds1edit.display_tile_grid >= TG_MAX)
            glb_ds1edit.display_tile_grid = TG_OFF;
         while(key[KEY_G])
         {
            // wait
         }
      }

      // changing current ds1
      group_changed = FALSE;
      old_group     = glb_ds1edit.ds1_group_idx;
      if (can_swich_mode)
      {
         if (key[KEY_LCONTROL] || key[KEY_RCONTROL])
         {
            if (key[KEY_1])
            {
               while (key[KEY_1] || key[KEY_LCONTROL] || key[KEY_RCONTROL])
               {
                  // wait
               }
               glb_ds1edit.ds1_group_idx = 0;
               group_changed = TRUE;
            }
            else if (key[KEY_2])
            {
               while (key[KEY_2] || key[KEY_LCONTROL] || key[KEY_RCONTROL])
               {
                  // wait
               }
               glb_ds1edit.ds1_group_idx = 1;
               group_changed = TRUE;
            }
            else if (key[KEY_3])
            {
               while (key[KEY_3] || key[KEY_LCONTROL] || key[KEY_RCONTROL])
               {
                  // wait
               }
               glb_ds1edit.ds1_group_idx = 2;
               group_changed = TRUE;
            }
            else if (key[KEY_4])
            {
               while (key[KEY_4] || key[KEY_LCONTROL] || key[KEY_RCONTROL])
               {
                  // wait
               }
               glb_ds1edit.ds1_group_idx = 3;
               group_changed = TRUE;
            }
            else if (key[KEY_5])
            {
               while (key[KEY_5] || key[KEY_LCONTROL] || key[KEY_RCONTROL])
               {
                  // wait
               }
               glb_ds1edit.ds1_group_idx = 4;
               group_changed = TRUE;
            }
            else if (key[KEY_6])
            {
               while (key[KEY_6] || key[KEY_LCONTROL] || key[KEY_RCONTROL])
               {
                  // wait
               }
               glb_ds1edit.ds1_group_idx = 5;
               group_changed = TRUE;
            }
            else if (key[KEY_7])
            {
               while (key[KEY_7] || key[KEY_LCONTROL] || key[KEY_RCONTROL])
               {
                  // wait
               }
               glb_ds1edit.ds1_group_idx = 6;
               group_changed = TRUE;
            }
            else if (key[KEY_8])
            {
               while (key[KEY_8] || key[KEY_LCONTROL] || key[KEY_RCONTROL])
               {
                  // wait
               }
               glb_ds1edit.ds1_group_idx = 7;
               group_changed = TRUE;
            }
            else if (key[KEY_9])
            {
               while (key[KEY_9] || key[KEY_LCONTROL] || key[KEY_RCONTROL])
               {
                  // wait
               }
               glb_ds1edit.ds1_group_idx = 8;
               group_changed = TRUE;
            }
            else if (key[KEY_0])
            {
               while (key[KEY_0] || key[KEY_LCONTROL] || key[KEY_RCONTROL])
               {
                  // wait
               }
               glb_ds1edit.ds1_group_idx = 9;
               group_changed = TRUE;
            }
         }

         if (group_changed == TRUE)
         {
            // try to swap to 1st ds1 of this group set
            found = FALSE;
            for (i=0; i < 10; i++)
            {
               if (strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + i].name))
               {
                  // there's a ds1 open here
                  old_ds1_idx = ds1_idx;
                  ds1_idx = glb_ds1edit.ds1_group_idx * 10 + i;
                  found = TRUE;
                  break;
               }
            }
            if (found == FALSE)
            {
               // don't change to this group
               glb_ds1edit.ds1_group_idx = old_group;
               group_changed = FALSE;
            }
         }

         // swap to a different ds1 ?
         if (key[KEY_1] && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10;
            while (key[KEY_1])
            {
               // wait
            }
         }
         if (key[KEY_2] && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 1].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 1;
            while (key[KEY_2])
            {
               // wait
            }
         }
         if (key[KEY_3] && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 2].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 2;
            while (key[KEY_3])
            {
               // wait
            }
         }
         if (key[KEY_4] && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 3].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 3;
            while (key[KEY_4])
            {
               // wait
            }
         }
         if (key[KEY_5] && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 4].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 4;
            while (key[KEY_5])
            {
               // wait
            }
         }
         if (key[KEY_6] && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 5].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 5;
            while (key[KEY_6])
            {
               // wait
            }
         }
         if (key[KEY_7] && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 6].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 6;
            while (key[KEY_7])
            {
               // wait
            }
         }
         if (key[KEY_8] && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 7].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 7;
            while (key[KEY_8])
            {
               // wait
            }
         }
         if (key[KEY_9] && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 8].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 8;
            while (key[KEY_9])
            {
               // wait
            }
         }
         if (key[KEY_0] && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 9].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 9;
            while (key[KEY_0])
            {
               // wait
            }
         }
      }

      if (old_ds1_idx != ds1_idx)
      {
         // save current win preview state for the old ds1
         glb_ds1[old_ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0;
         glb_ds1[old_ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0;
         glb_ds1[old_ds1_idx].own_wpreview.w  = glb_ds1edit.win_preview.w;
         glb_ds1[old_ds1_idx].own_wpreview.h  = glb_ds1edit.win_preview.h;

         // put back old win preview state for the new ds1
         glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
         glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
         glb_ds1edit.win_preview.w  = glb_ds1[ds1_idx].own_wpreview.w;
         glb_ds1edit.win_preview.h  = glb_ds1[ds1_idx].own_wpreview.h;

         // ending swap
         old_ds1_idx = ds1_idx;
      }

      // toggle 2nd row
      if (key[KEY_TILDE])
      {
         while (key[KEY_TILDE])
         {
            // wait until TILDE is not pressed
         }
         if (glb_ds1edit.show_2nd_row == FALSE)
            glb_ds1edit.show_2nd_row = TRUE;
         else
            glb_ds1edit.show_2nd_row = FALSE;
      }
      
      // TAB : change edit mode
      if (key[KEY_TAB])
      {
         while (key[KEY_TAB])
         {
            // wait until TAB is not pressed
         }
         if (glb_ds1edit.mode == MOD_L)
            glb_ds1edit.mode = old_mode;
         else
         {
            if (can_swich_mode)
            {
               if ((key[KEY_LSHIFT]) || (key[KEY_RSHIFT]))
                  glb_ds1edit.mode--;
               else
                  glb_ds1edit.mode++;
            }
         }
         if (glb_ds1edit.mode < MOD_T)
            glb_ds1edit.mode = MOD_P;
         if ((glb_ds1edit.mode >= MOD_MAX) || (glb_ds1edit.mode == MOD_L))
            glb_ds1edit.mode = MOD_T;
         // show_mouse(NULL);
//         misc_set_mouse_cursor(glb_ds1edit.mouse_cursor[glb_ds1edit.mode]);
         // show_mouse(screen);
         old_cell_x = -1;
         old_cell_y = -1;
      }

      // N : Toggle Night mode
      if (key[KEY_N])
      {
         while (key[KEY_N])
         {
            // wait until the 'N' key is released
         }
         if (glb_ds1edit.mode == MOD_L)
         {
            glb_ds1edit.night_mode++;
            if (glb_ds1edit.night_mode >= 2)
            {
               glb_ds1edit.night_mode = 0;
               glb_ds1edit.mode = old_mode;
            }
         }
         else
         {
            old_mode = glb_ds1edit.mode;
            glb_ds1edit.mode = MOD_L;
         }

         old_cell_x = -1;
         old_cell_y = -1;
      }

      // R : Refresh obj.txt
      if (key[KEY_R])
      {
         while (key[KEY_R])
         {
            // wait until the 'R' key is released
         }
            
         // refresh animdata.d2
         animdata_load();

         // destroy all animations
         anim_exit();

         // destroy memory obj.txt and objects.txt
         glb_ds1edit.obj_buff     = txt_destroy(glb_ds1edit.obj_buff);
         glb_ds1edit.objects_buff = txt_destroy(glb_ds1edit.obj_buff);

         // read the current obj.txt and objects.txt
         read_objects_txt(); // objects.txt first !
         read_obj_txt();

         // load new animations
         anim_update_gfx(FALSE);

         // reset the ticks counter
         glb_ds1edit.ticks_elapsed = 0;
      }
      
      // left mouse button
      if (old_mouse_b & 1)
      {
         // mouse button 1 is pressed
         if (glb_ds1edit.mode == MOD_T)
         {
            if (paste_pos.start == TRUE)
            {
               // end a paste
               edittile_paste_final(ds1_idx);
               paste_pos.start = FALSE;
               while (mouse_b & 1) // NOT old_mouse_b else infinite loop
               {
                  // wait until the left mouse button is released
               }
            }
            else if (tmp_sel.start == FALSE)
            {
               if ( (key[KEY_I]) &&
                    (cx != old_identical_x) && (cy != old_identical_y)
                  )
               {
                  // for all the tiles Identical to the visible ones

                  if (key[KEY_LSHIFT] || key[KEY_RSHIFT])
                  {
                     // add to selection
                     itype = IT_ADD;
                  }
                  else if (key[KEY_LCONTROL] || key[KEY_RCONTROL])
                  {
                     // delete from previous selection
                     itype = IT_DEL;
                  }
                  else
                  {
                     // new selection (delete previous one)
                     itype = IT_NEW;
                  }
                  edittile_identical(ds1_idx, itype, cx, cy);
                  old_identical_x = cx;
                  old_identical_y = cy;
               }
               else if ( ! key[KEY_I])
               {
                  // starting a temp selection
                  old_identical_x = -1;
                  old_identical_y = -1;
                  
                  tmp_sel.start = TRUE;
                  tmp_sel.x1 = tmp_sel.x2 = tmp_sel.old_x2 = cx;
                  tmp_sel.y1 = tmp_sel.y2 = tmp_sel.old_y2 = cy;
                  edittile_delete_all_tmpsel(ds1_idx);
                  edittile_set_tmpsel(ds1_idx, & tmp_sel);
               }
            }
            else
            {
               if ((tmp_sel.old_x2 != cx) || (tmp_sel.old_y2 != cy))
               {
                  // update the temp selection
                  tmp_sel.x2 = tmp_sel.old_x2 = cx;
                  tmp_sel.y2 = tmp_sel.old_y2 = cy;
                  edittile_delete_all_tmpsel(ds1_idx);
                  edittile_set_tmpsel(ds1_idx, & tmp_sel);
               }
            }
         }
      }
      else
      {
         // mouse button 1 is not pressed
         if (glb_ds1edit.mode == MOD_T)
         {
            if (tmp_sel.start == TRUE)
            {
               // end of tmp sel, process it
               if (key[KEY_H])
                  tmp_sel.type = TMP_HIDE;
               else if (key[KEY_LSHIFT] || key[KEY_RSHIFT])
                  tmp_sel.type = TMP_ADD;
               else if (key[KEY_LCONTROL] || key[KEY_RCONTROL])
                  tmp_sel.type = TMP_DEL;
               else
                  tmp_sel.type = TMP_NEW;
               switch (tmp_sel.type)
               {
                  case TMP_NEW :
                     edittile_delete_all_tmpsel(ds1_idx);
                     edittile_change_to_new_permanent_sel(ds1_idx, & tmp_sel);
                     tmp_sel.start = FALSE;
                     tmp_sel.type = TMP_NULL;
                     tmp_sel.x1 = tmp_sel.x2 = tmp_sel.y1 = tmp_sel.y2 = 0;
                     tmp_sel.old_x2 = tmp_sel.old_y2 = 0;
                     break;

                  case TMP_ADD :
                     edittile_delete_all_tmpsel(ds1_idx);
                     edittile_change_to_add_permanent_sel(ds1_idx, & tmp_sel);
                     tmp_sel.start = FALSE;
                     tmp_sel.type = TMP_NULL;
                     tmp_sel.x1 = tmp_sel.x2 = tmp_sel.y1 = tmp_sel.y2 = 0;
                     tmp_sel.old_x2 = tmp_sel.old_y2 = 0;
                     break;

                  case TMP_HIDE :
                     edittile_delete_all_tmpsel(ds1_idx);
                     edittile_change_to_hide_sel(ds1_idx, & tmp_sel);
                     tmp_sel.start = FALSE;
                     tmp_sel.type = TMP_NULL;
                     tmp_sel.x1 = tmp_sel.x2 = tmp_sel.y1 = tmp_sel.y2 = 0;
                     tmp_sel.old_x2 = tmp_sel.old_y2 = 0;
                     break;

                  case TMP_DEL :
                     edittile_delete_all_tmpsel(ds1_idx);
                     edittile_change_to_del_sel(ds1_idx, & tmp_sel);
                     tmp_sel.start = FALSE;
                     tmp_sel.type = TMP_NULL;
                     tmp_sel.x1 = tmp_sel.x2 = tmp_sel.y1 = tmp_sel.y2 = 0;
                     tmp_sel.old_x2 = tmp_sel.old_y2 = 0;
                     break;
               }
            }
            else if (paste_pos.start == TRUE)
            {
               if ((paste_pos.old_x != cx) ||
                   (paste_pos.old_y != cy) ||
                   (paste_pos.old_ds1_idx != ds1_idx)
                  )
               {
                  edittile_paste_undo(paste_pos.old_ds1_idx);
                  edittile_paste_preview(ds1_idx,
                     cx - paste_pos.start_x,
                     cy - paste_pos.start_y,
                     & paste_pos
                  );
                  paste_pos.old_x = cx;
                  paste_pos.old_y = cy;
                  paste_pos.old_ds1_idx = ds1_idx;
               }
            }
         }
      }
      
      // right mouse button
      if (old_mouse_b & 2)
      {
         if (glb_ds1edit.mode == MOD_T)
         {
            while (mouse_b & 2) // NOT old_mouse_b else infinite loop
            {
               // wait until the right mouse button is released
            }
            if ( (key[KEY_LCONTROL] || key[KEY_RCONTROL]) &&
                 (key[KEY_LSHIFT]   || key[KEY_RSHIFT]) )
            {
               // advanced tile editing window (bits)
               wbits_main(ds1_idx, cx, cy);
               position_mouse(old_mouse_x, old_mouse_y);
            }
            else
            {
               wedit_test(ds1_idx, cx, cy);
               set_color_depth(8); // back to a 8bpp color depth for duture BITMAP creation
               position_mouse(old_mouse_x, old_mouse_y);
            }
         }
      }

      // quit
      if (key[KEY_ESC] && (glb_ds1[ds1_idx].draw_edit_obj == FALSE))
      {
         while (key[KEY_ESC])
         {
            // wait until ESC is not pressed anymore
         }
         ret = msg_quit_main();
         switch (ret)
         {
            case -1 :
               // error
               ds1_save(ds1_idx, TRUE); // save a .TMP map
               done = TRUE;
               break;

            case 0 :
               // save ALL & quit
               for (i=0; i < DS1_MAX; i++)
               {
                  if (strlen(glb_ds1[ds1_idx].name))
                     ds1_save(i, FALSE);
               }
               done = TRUE;
               break;

            case 1 :
               // quit
               ds1_save(ds1_idx, TRUE); // save a .TMP map
               done = TRUE;
               break;

            case 2  :
            default :
               // cancel
               break;
         }
      }
   }
}
