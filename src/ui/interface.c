#include "structs.h"
#include "render/preview.h"
#include "editor/objects.h"
#include "misc.h"
#include "core/ds1.h"
#include "ui/dialogs.h"
#include "editor/tiles.h"
#include "editor/undo.h"
#include "core/animdata.h"
#include "core/cof.h"
#include "core/txtread.h"
#include "ui/bits_window.h"
#include "ui/edit_window.h"
#include "ui/dialogs.h"
#include "ui/interface.h"
#include "core/area_browser.h"
#include "core/ds1_manager.h"
#include "ui/props_panel.h"


typedef struct
{
   double event_ms_total;
   double input_ms_total;
   double mouse_to_tile_ms_total;
   double editobj_ms_total;
   double anim_ms_total;
   double render_ms_total;
   double ui_ms_total;
   double frame_ms_total;

   double event_ms_max;
   double input_ms_max;
   double mouse_to_tile_ms_max;
   double editobj_ms_max;
   double anim_ms_max;
   double render_ms_max;
   double ui_ms_max;
   double frame_ms_max;

   int frames;
} PERF_STATS_S;

static PERF_STATS_S glb_perf_stats;

static double perf_now_ms(void)
{
   return al_get_time() * 1000.0;
}

static void perf_accumulate(double * total, double * max, double dt_ms)
{
   (*total) += dt_ms;
   if (dt_ms > *max)
      *max = dt_ms;
}

static void perf_print_summary(void)
{
   double inv;

   if (glb_perf_stats.frames <= 0)
      return;

   inv = 1.0 / glb_perf_stats.frames;

   fprintf(stderr, "\n[perf] last %d frames\n", glb_perf_stats.frames);
   fprintf(stderr, "[perf] frame:       avg %7.2f ms  max %7.2f ms  (%.2f FPS)\n",
      glb_perf_stats.frame_ms_total * inv,
      glb_perf_stats.frame_ms_max,
      (glb_perf_stats.frame_ms_total > 0.0) ?
         (1000.0 * glb_perf_stats.frames / glb_perf_stats.frame_ms_total) : 0.0);
   fprintf(stderr, "[perf] events:      avg %7.2f ms  max %7.2f ms\n",
      glb_perf_stats.event_ms_total * inv, glb_perf_stats.event_ms_max);
   fprintf(stderr, "[perf] input:       avg %7.2f ms  max %7.2f ms\n",
      glb_perf_stats.input_ms_total * inv, glb_perf_stats.input_ms_max);
   fprintf(stderr, "[perf] mouse->tile: avg %7.2f ms  max %7.2f ms\n",
      glb_perf_stats.mouse_to_tile_ms_total * inv, glb_perf_stats.mouse_to_tile_ms_max);
   fprintf(stderr, "[perf] editobj:     avg %7.2f ms  max %7.2f ms\n",
      glb_perf_stats.editobj_ms_total * inv, glb_perf_stats.editobj_ms_max);
   fprintf(stderr, "[perf] anim:        avg %7.2f ms  max %7.2f ms\n",
      glb_perf_stats.anim_ms_total * inv, glb_perf_stats.anim_ms_max);
   fprintf(stderr, "[perf] render:      avg %7.2f ms  max %7.2f ms\n",
      glb_perf_stats.render_ms_total * inv, glb_perf_stats.render_ms_max);
   fprintf(stderr, "[perf] ui/other:    avg %7.2f ms  max %7.2f ms\n",
      glb_perf_stats.ui_ms_total * inv, glb_perf_stats.ui_ms_max);
   fflush(stderr);

   memset(&glb_perf_stats, 0, sizeof(glb_perf_stats));
}


// ==========================================================================
// MAIN loop
void interfac_user_handler(int start_ds1_idx)
{
   int  ds1_idx, done, cx, cy, n, i, dx, dy, old_ds1_idx=0;
   int  old_mouse_x = a5_mouse_x, old_mouse_y=a5_mouse_y, old_mouse_b=0;
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
   ALLEGRO_BITMAP * old_screen_buff = NULL;
   double      frame_start_ms, section_start_ms;
 

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
      frame_start_ms = perf_now_ms();

      // drain Allegro 5 event queue
      section_start_ms = perf_now_ms();
      {
          ALLEGRO_EVENT event;
          while (al_get_next_event(a5_event_queue, &event)) {
              if (event.type == ALLEGRO_EVENT_TIMER) {
                  if (event.timer.source == a5_tick_timer)
                      glb_ds1edit.ticks_elapsed++;
                  else if (event.timer.source == a5_fps_timer) {
                      glb_ds1edit.old_fps = glb_ds1edit.fps;
                      glb_ds1edit.fps = 0;
                  }
              }
              else if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
                  done = TRUE;
              }
              else if (event.type == ALLEGRO_EVENT_KEY_CHAR) {
                  /* Ctrl+Enter = Apply, Ctrl+Z = Discard */
                  if (glb_ds1edit.props_panel_visible &&
                      (event.keyboard.modifiers & ALLEGRO_KEYMOD_CTRL))
                  {
                      if (event.keyboard.keycode == ALLEGRO_KEY_ENTER ||
                          event.keyboard.keycode == ALLEGRO_KEY_PAD_ENTER)
                      {
                          if (glb_ds1edit.props_panel.pending_count > 0)
                              props_panel_apply();
                      }
                      else if (event.keyboard.keycode == ALLEGRO_KEY_Z)
                      {
                          glb_ds1edit.props_panel.pending_count = 0;
                          glb_ds1edit.props_panel.editing = FALSE;
                      }
                  }
                  else if (glb_ds1edit.props_panel.editing ||
                           glb_ds1edit.props_panel.has_focus)
                  {
                      props_panel_handle_keychar(
                          event.keyboard.unichar,
                          event.keyboard.keycode);
                  }
              }
          }
      }
      perf_accumulate(
         &glb_perf_stats.event_ms_total,
         &glb_perf_stats.event_ms_max,
         perf_now_ms() - section_start_ms
      );

      // poll input state
      section_start_ms = perf_now_ms();
      al_get_keyboard_state(&a5_kb_state);
      al_get_mouse_state(&a5_ms_state);

      perf_accumulate(
         &glb_perf_stats.input_ms_total,
         &glb_perf_stats.input_ms_max,
         perf_now_ms() - section_start_ms
      );

      cur_mouse_z = a5_mouse_z;

      /* Sidebar toggle: backtick key (`) */
      if (key_pressed(KEY_TILDE))
      {
         glb_ds1edit.sidebar_visible = !glb_ds1edit.sidebar_visible;
         while (key_pressed(KEY_TILDE))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }
      }

      /* Properties panel: Backspace opens+focuses.
       * If closed: open and focus. If open but unfocused: focus. If focused: close. */
      if (key_pressed(KEY_BACKSPACE) && !glb_ds1edit.props_panel.editing)
      {
         if (!glb_ds1edit.props_panel_visible)
         {
            /* Closed -> open + focused */
            glb_ds1edit.props_panel_visible = TRUE;
            glb_ds1edit.props_panel.has_focus = TRUE;
         }
         else if (!glb_ds1edit.props_panel.has_focus)
         {
            /* Open but unfocused -> focused */
            glb_ds1edit.props_panel.has_focus = TRUE;
         }
         else
         {
            /* Focused -> close */
            glb_ds1edit.props_panel_visible = FALSE;
            glb_ds1edit.props_panel.has_focus = FALSE;
         }
         while (key_pressed(KEY_BACKSPACE))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }
      }

      /* Sidebar: handle click on collapsed tab to expand */
      if (!glb_ds1edit.sidebar_visible && a5_mouse_b &&
          a5_mouse_x < 16) /* collapsed tab width */
      {
         glb_ds1edit.sidebar_visible = TRUE;
         while (a5_mouse_b)
         { al_rest(0.01); al_get_mouse_state(&a5_ms_state); }
      }

      /* Properties panel: handle click on collapsed tab to expand */
      {
         int pp_disp_w = al_get_display_width(a5_display);
         if (!glb_ds1edit.props_panel_visible && a5_mouse_b &&
             a5_mouse_x > pp_disp_w - 16)
         {
            glb_ds1edit.props_panel_visible = TRUE;
            while (a5_mouse_b)
            { al_rest(0.01); al_get_mouse_state(&a5_ms_state); }
         }
      }

      /* Sidebar: handle mouse interactions */
      if (glb_ds1edit.sidebar_visible &&
          a5_mouse_x < glb_ds1edit.sidebar_width)
      {
         /* Mouse wheel scrolls the sidebar (consume delta so zoom doesn't fire) */
         if (cur_mouse_z != old_mouse_z)
         {
            area_browser_sidebar_scroll(cur_mouse_z - old_mouse_z);
            old_mouse_z = cur_mouse_z;
         }

         /* Mouse click in sidebar */
         if (a5_mouse_b & 1)
         {
            int click_result = area_browser_sidebar_click(
               a5_mouse_x, a5_mouse_y,
               glb_ds1edit.sidebar_width,
               al_get_display_height(a5_display)
            );

            if (click_result == -2)
            {
               /* Close button */
               glb_ds1edit.sidebar_visible = FALSE;
            }
            else if (click_result == -3)
            {
               /* Individual DS1 entry clicked — switch to it (instant if preloaded) */
               {
                  int new_idx = area_browser_switch_single(
                     glb_ds1edit.area_browser.selected_group,
                     glb_ds1edit.area_browser.selected_entry);
                  if (new_idx >= 0)
                     ds1_idx = new_idx;
               }
            }
            else if (click_result >= 0)
            {
               /* Group clicked — expand/collapse + preload all DS1s */
               if (glb_ds1edit.area_browser.groups[click_result].is_expanded &&
                   glb_ds1edit.area_browser.loaded_group != click_result)
               {
                  /* Just expanded — preload all DS1s for this group */
                  if (area_browser_switch_area(click_result) >= 0)
                     ds1_idx = 0;
               }
            }

            while (a5_mouse_b & 1)
            { al_rest(0.01); al_get_mouse_state(&a5_ms_state); }
         }

         /* Right-click in sidebar — context menu for backup entries */
         if (a5_mouse_b & 2)
         {
            AREA_BROWSER_S * rc_ab = &glb_ds1edit.area_browser;
            int rc_gi = rc_ab->selected_group;
            int rc_ei = rc_ab->selected_entry;

            /* Wait for button release */
            while (a5_mouse_b & 2)
            { al_rest(0.01); al_get_mouse_state(&a5_ms_state); }

            /* Show context menu if right-clicked on a backup entry */
            if (rc_gi >= 0 && rc_gi < rc_ab->group_count &&
                rc_ab->groups[rc_gi].is_backup && rc_ei >= 0 &&
                rc_ei < rc_ab->groups[rc_gi].entry_count)
            {
               /* Simple 2-item popup menu */
               int menu_x = a5_mouse_x;
               int menu_y = a5_mouse_y;
               int menu_w = 140;
               int menu_h = 28;
               int menu_sel = -1;
               int menu_done = 0;
               int disp_w = al_get_display_width(a5_display);
               int disp_h = al_get_display_height(a5_display);
               ALLEGRO_COLOR mc_bg     = al_map_rgba(40, 36, 30, 240);
               ALLEGRO_COLOR mc_hover  = al_map_rgba(60, 80, 120, 220);
               ALLEGRO_COLOR mc_text   = al_map_rgb(220, 220, 220);
               ALLEGRO_COLOR mc_border = al_map_rgb(80, 70, 50);

               /* Clamp to display bounds */
               if (menu_x + menu_w > disp_w) menu_x = disp_w - menu_w;
               if (menu_y + menu_h > disp_h) menu_y = disp_h - menu_h;

               while (!menu_done)
               {
                  int mx, my;

                  al_get_keyboard_state(&a5_kb_state);
                  al_get_mouse_state(&a5_ms_state);
                  mx = al_get_mouse_state_axis(&a5_ms_state, 0);
                  my = al_get_mouse_state_axis(&a5_ms_state, 1);

                  /* Determine which menu item is hovered */
                  menu_sel = -1;
                  if (mx >= menu_x && mx < menu_x + menu_w)
                  {
                     if (my >= menu_y && my < menu_y + 14)
                        menu_sel = 0;  /* Restore */
                     else if (my >= menu_y + 14 && my < menu_y + 28)
                        menu_sel = 1;  /* Delete permanently */
                  }

                  /* Draw the popup over current frame */
                  al_set_target_backbuffer(a5_display);
                  al_draw_filled_rectangle((float)menu_x, (float)menu_y,
                     (float)(menu_x + menu_w), (float)(menu_y + menu_h), mc_bg);
                  al_draw_rectangle((float)menu_x + 0.5f, (float)menu_y + 0.5f,
                     (float)(menu_x + menu_w) - 0.5f, (float)(menu_y + menu_h) - 0.5f,
                     mc_border, 1.0f);

                  /* Item 0: Restore */
                  if (menu_sel == 0)
                     al_draw_filled_rectangle((float)menu_x, (float)menu_y,
                        (float)(menu_x + menu_w), (float)(menu_y + 14), mc_hover);
                  al_draw_textf(a5_font, mc_text, (float)(menu_x + 6), (float)(menu_y + 3), 0,
                     "Restore");

                  /* Item 1: Delete permanently */
                  if (menu_sel == 1)
                     al_draw_filled_rectangle((float)menu_x, (float)(menu_y + 14),
                        (float)(menu_x + menu_w), (float)(menu_y + 28), mc_hover);
                  al_draw_textf(a5_font, mc_text, (float)(menu_x + 6), (float)(menu_y + 17), 0,
                     "Delete permanently");

                  al_flip_display();
                  al_rest(0.02);

                  /* Click selects item */
                  if (al_mouse_button_down(&a5_ms_state, 1))
                  {
                     if (menu_sel >= 0)
                     {
                        if (menu_sel == 0)
                        {
                           if (ds1_manager_restore(rc_gi, rc_ei) == 0)
                              printf("Backup restored successfully\n");
                        }
                        else
                        {
                           if (ds1_manager_delete_permanent(rc_gi, rc_ei) == 0)
                              printf("Backup deleted permanently\n");
                        }
                        fflush(stdout);
                     }
                     menu_done = 1;
                     /* Wait for release */
                     while (al_mouse_button_down(&a5_ms_state, 1))
                     { al_rest(0.01); al_get_mouse_state(&a5_ms_state); }
                  }
                  /* Right-click or Escape cancels */
                  if (al_mouse_button_down(&a5_ms_state, 2) ||
                      al_key_down(&a5_kb_state, ALLEGRO_KEY_ESCAPE))
                  {
                     menu_done = 1;
                     while (al_mouse_button_down(&a5_ms_state, 2))
                     { al_rest(0.01); al_get_mouse_state(&a5_ms_state); }
                  }
               }
            }
         }

         /* Update hover selection */
         {
            AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
            int sy, si, s_last_act = -1, s_draw_row = 0;
            int s_top = glb_ds1edit.show_2nd_row ? 20 : 9;
            int found = 0;

            ab->selected_entry = -1;
            sy = s_top + 22 + 4; /* panel_top + SB_HEADER_H + 4 */
            for (si = 0; si < ab->group_count && !found; si++)
            {
               if (ab->groups[si].act != s_last_act)
               {
                  if (s_draw_row >= ab->scroll_offset)
                     sy += 14;
                  s_draw_row++;
                  s_last_act = ab->groups[si].act;
               }
               /* Group row */
               if (s_draw_row >= ab->scroll_offset)
               {
                  if (a5_mouse_y >= sy && a5_mouse_y < sy + 14)
                  {
                     ab->selected_group = si;
                     found = 1;
                  }
                  sy += 14;
               }
               s_draw_row++;

               /* Expanded entry rows */
               if (ab->groups[si].is_expanded && !found)
               {
                  int sj;
                  for (sj = 0; sj < ab->groups[si].entry_count; sj++)
                  {
                     if (s_draw_row >= ab->scroll_offset)
                     {
                        if (a5_mouse_y >= sy && a5_mouse_y < sy + 14)
                        {
                           ab->selected_group = si;
                           ab->selected_entry = sj;
                           found = 1;
                           break;
                        }
                        sy += 14;
                     }
                     s_draw_row++;
                  }
               }
            }
         }
      }

      /* Properties panel: handle mouse interactions (right side) */
      {
         int pp_disp_w = al_get_display_width(a5_display);
         if (glb_ds1edit.props_panel_visible &&
             a5_mouse_x > pp_disp_w - glb_ds1edit.props_panel_width)
         {
            /* Mouse wheel scrolls the panel */
            if (cur_mouse_z != old_mouse_z)
            {
               props_panel_scroll(cur_mouse_z - old_mouse_z);
               old_mouse_z = cur_mouse_z;
            }

            /* Mouse click in panel */
            if (a5_mouse_b & 1)
            {
               int pp_result = props_panel_click(
                  a5_mouse_x, a5_mouse_y,
                  glb_ds1edit.props_panel_width,
                  al_get_display_height(a5_display)
               );

               if (pp_result == -2)
                  glb_ds1edit.props_panel_visible = FALSE;
               else
                  glb_ds1edit.props_panel.has_focus = TRUE;

               while (a5_mouse_b & 1)
               { al_rest(0.01); al_get_mouse_state(&a5_ms_state); }
            }

            /* Consume right-click in panel */
            if (a5_mouse_b & 2)
            {
               while (a5_mouse_b & 2)
               { al_rest(0.01); al_get_mouse_state(&a5_ms_state); }
            }
         }
      }

      /* Click outside properties panel removes its focus */
      if (glb_ds1edit.props_panel.has_focus && (a5_mouse_b & 1))
      {
         int pp_dw = al_get_display_width(a5_display);
         if (!glb_ds1edit.props_panel_visible ||
             a5_mouse_x <= pp_dw - glb_ds1edit.props_panel_width)
         {
            glb_ds1edit.props_panel.has_focus = FALSE;
         }
      }

      can_swich_mode = TRUE;
      if (glb_ds1edit.mode == MOD_P)
      {
         if (glb_ds1[ds1_idx].path_edit_win.mode != PEM_NONE)
            can_swich_mode = FALSE;
         else if (glb_ds1[ds1_idx].path_edit_win.obj_dirty == TRUE)
            can_swich_mode = FALSE;
      }

      old_mouse_x = a5_mouse_x;
      old_mouse_y = a5_mouse_y;
      old_mouse_b = a5_mouse_b;

      cur_mouse_z = a5_mouse_z;

      // keep the current mouse coordinates for the entire loop process
      
      // which tile (or sub-tile) is RIGHT NOW under the mouse ?
      section_start_ms = perf_now_ms();
      if (glb_ds1edit.has_loaded_ds1 && glb_ds1[ds1_idx].tile_w > 0)
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
      perf_accumulate(
         &glb_perf_stats.mouse_to_tile_ms_total,
         &glb_perf_stats.mouse_to_tile_ms_max,
         perf_now_ms() - section_start_ms
      );
            
      if (glb_ds1edit.mode == MOD_O)
      {
         section_start_ms = perf_now_ms();
         editobj_handler(
            ds1_idx, cx,
            cy,
            old_mouse_x,
            old_mouse_y,
            old_mouse_b
         );
         perf_accumulate(
            &glb_perf_stats.editobj_ms_total,
            &glb_perf_stats.editobj_ms_max,
            perf_now_ms() - section_start_ms
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
      section_start_ms = perf_now_ms();
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
      perf_accumulate(
         &glb_perf_stats.anim_ms_total,
         &glb_perf_stats.anim_ms_max,
         perf_now_ms() - section_start_ms
      );

      // redraw the whole screen
      section_start_ms = perf_now_ms();
      if (glb_ds1edit.has_loaded_ds1)
         wpreview_draw_tiles(ds1_idx);
      else
         misc_draw_screen(a5_mouse_x, a5_mouse_y);
      glb_ds1edit.fps++;
      perf_accumulate(
         &glb_perf_stats.render_ms_total,
         &glb_perf_stats.render_ms_max,
         perf_now_ms() - section_start_ms
      );

      // scroll UP / DOWN / LEFT / RIGHT
      section_start_ms = perf_now_ms();

      // if the Object Editing Window is display
      if (glb_ds1[ds1_idx].draw_edit_obj == TRUE)
      {
         if (glb_config.winobj_scroll_keyb == TRUE)
         {
            // can scroll by keyboard (WASD)
            if (key_pressed(KEY_W))
            {
               glb_ds1edit.win_preview.y0 -= glb_ds1[ds1_idx].cur_scroll.keyb.y;
            }
            if (key_pressed(KEY_S))
            {
               glb_ds1edit.win_preview.y0 += glb_ds1[ds1_idx].cur_scroll.keyb.y;
            }
            if (key_pressed(KEY_A))
            {
               glb_ds1edit.win_preview.x0 -= glb_ds1[ds1_idx].cur_scroll.keyb.x;
            }
            if (key_pressed(KEY_D))
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
         if (key_pressed(KEY_W))
         {
            glb_ds1edit.win_preview.y0 -= glb_ds1[ds1_idx].cur_scroll.keyb.y;
         }
         else if (old_mouse_y == 0)
         {
            glb_ds1edit.win_preview.y0 -= glb_ds1[ds1_idx].cur_scroll.mouse.y;
         }

         if (key_pressed(KEY_S))
         {
            glb_ds1edit.win_preview.y0 += glb_ds1[ds1_idx].cur_scroll.keyb.y;
         }
         else if (old_mouse_y == glb_config.screen.height - 1)
         {
            glb_ds1edit.win_preview.y0 += glb_ds1[ds1_idx].cur_scroll.mouse.y;
         }

         if (key_pressed(KEY_A))
         {
            glb_ds1edit.win_preview.x0 -= glb_ds1[ds1_idx].cur_scroll.keyb.x;
         }
         else if (old_mouse_x == 0)
         {
            glb_ds1edit.win_preview.x0 -= glb_ds1[ds1_idx].cur_scroll.mouse.x;
         }

         if (key_pressed(KEY_D))
         {
            glb_ds1edit.win_preview.x0 += glb_ds1[ds1_idx].cur_scroll.keyb.x;
         }
         else if (old_mouse_x == glb_config.screen.width  - 1)
         {
            glb_ds1edit.win_preview.x0 += glb_ds1[ds1_idx].cur_scroll.mouse.x;
         }
      }

      // zoom — keyboard (+/- keys): zoom around screen center
      if ( (key_pressed(KEY_MINUS_PAD) || key_pressed(KEY_MINUS)) &&
           glb_ds1[ds1_idx].cur_zoom < ZM_116)
      {
         while(key_pressed(KEY_MINUS_PAD) || key_pressed(KEY_MINUS))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }
         glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0;
         glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0;
         glb_ds1[ds1_idx].own_wpreview.w  = glb_ds1edit.win_preview.w;
         glb_ds1[ds1_idx].own_wpreview.h  = glb_ds1edit.win_preview.h;
         change_zoom(ds1_idx, glb_ds1[ds1_idx].cur_zoom + 1);
         glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
         glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
      }
      if ( (key_pressed(KEY_PLUS_PAD) || key_pressed(KEY_EQUALS)) &&
           glb_ds1[ds1_idx].cur_zoom > ZM_21)
      {
         while(key_pressed(KEY_PLUS_PAD) || key_pressed(KEY_EQUALS))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }
         glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0;
         glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0;
         glb_ds1[ds1_idx].own_wpreview.w  = glb_ds1edit.win_preview.w;
         glb_ds1[ds1_idx].own_wpreview.h  = glb_ds1edit.win_preview.h;
         change_zoom(ds1_idx, glb_ds1[ds1_idx].cur_zoom - 1);
         glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
         glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
      }

      // zoom — mouse wheel: zoom toward/away from cursor (Google Maps style)
      if (cur_mouse_z != old_mouse_z && glb_ds1edit.has_loaded_ds1 &&
          !(a5_mouse_b & 4)) /* don't zoom if middle button held (center-to-cursor) */
      {
         int zoom_dir = (cur_mouse_z < old_mouse_z) ? 1 : -1; /* 1=out, -1=in */
         int new_zoom = glb_ds1[ds1_idx].cur_zoom + zoom_dir;

         if (new_zoom >= ZM_21 && new_zoom <= ZM_116)
         {
            /* World position under the cursor before zoom */
            int world_x = glb_ds1edit.win_preview.x0 + a5_mouse_x;
            int world_y = glb_ds1edit.win_preview.y0 + a5_mouse_y;

            /* Ratio of cursor position relative to viewport */
            float rx = (float)a5_mouse_x / (float)glb_ds1edit.win_preview.w;
            float ry = (float)a5_mouse_y / (float)glb_ds1edit.win_preview.h;

            /* Save and zoom */
            glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0;
            glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0;
            glb_ds1[ds1_idx].own_wpreview.w  = glb_ds1edit.win_preview.w;
            glb_ds1[ds1_idx].own_wpreview.h  = glb_ds1edit.win_preview.h;
            change_zoom(ds1_idx, new_zoom);

            /* After zoom, the world coordinates are rescaled.
             * Adjust viewport so the cursor world position stays fixed.
             * new_world = new_viewport_origin + cursor_screen_pos
             * We want: new_viewport_x0 + mouse_x = world_x * new_scale / old_scale */
            {
               int old_mul = (zoom_dir == 1) ?
                  (new_zoom == ZM_11 ? 1 : (new_zoom == ZM_21 ? 2 : 1)) :
                  (glb_ds1[ds1_idx].cur_zoom == ZM_21 ? 2 : 1);

               /* Simpler: just set viewport so cursor world position is preserved */
               float scale_ratio;
               int old_tw = (zoom_dir == 1) ?
                  glb_ds1[ds1_idx].tile_w * 2 : /* was bigger before zoom out */
                  glb_ds1[ds1_idx].tile_w / 2;  /* was smaller before zoom in */

               if (old_tw == 0) old_tw = 1;
               scale_ratio = (float)glb_ds1[ds1_idx].tile_w / (float)old_tw;

               glb_ds1edit.win_preview.x0 = (int)(world_x * scale_ratio) - a5_mouse_x;
               glb_ds1edit.win_preview.y0 = (int)(world_y * scale_ratio) - a5_mouse_y;
            }

            glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0;
            glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0;
         }
      }

      if (old_mouse_z != cur_mouse_z)
      {
         old_mouse_z = cur_mouse_z;
         if (a5_mouse_b & 4)
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
               al_set_mouse_xy(a5_display, glb_ds1edit.win_preview.w / 2, glb_ds1edit.win_preview.h / 2);
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
               al_set_mouse_xy(a5_display, glb_ds1edit.win_preview.w / 2, glb_ds1edit.win_preview.h / 2);
            }
         }
      }

      // DS1 file navigation (arrow keys, PgUp/PgDn, Home/End)
      if (key_pressed(KEY_UP))
      {
         int nav_idx = area_browser_nav_up();
         if (nav_idx >= 0 && glb_ds1edit.has_loaded_ds1)
            ds1_idx = nav_idx;
         while (key_pressed(KEY_UP))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }
      }
      if (key_pressed(KEY_DOWN))
      {
         int nav_idx = area_browser_nav_down();
         if (nav_idx >= 0 && glb_ds1edit.has_loaded_ds1)
            ds1_idx = nav_idx;
         while (key_pressed(KEY_DOWN))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }
      }
      if (key_pressed(KEY_LEFT))
      {
         area_browser_nav_left();
         while (key_pressed(KEY_LEFT))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }
      }
      if (key_pressed(KEY_RIGHT))
      {
         int nav_idx = area_browser_nav_right();
         if (nav_idx >= 0 && glb_ds1edit.has_loaded_ds1)
            ds1_idx = nav_idx;
         while (key_pressed(KEY_RIGHT))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }
      }
      if (key_pressed(KEY_PGUP) && !(key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL)))
      {
         area_browser_nav_pgup();
         while (key_pressed(KEY_PGUP))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }
      }
      if (key_pressed(KEY_PGDN) && !(key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL)))
      {
         area_browser_nav_pgdn();
         while (key_pressed(KEY_PGDN))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }
      }
      if (key_pressed(KEY_HOME) && !(key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT)))
      {
         int nav_idx = area_browser_nav_home();
         if (nav_idx >= 0 && glb_ds1edit.has_loaded_ds1)
            ds1_idx = nav_idx;
         while (key_pressed(KEY_HOME))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }
      }
      if (key_pressed(KEY_END))
      {
         int nav_idx = area_browser_nav_end();
         if (nav_idx >= 0 && glb_ds1edit.has_loaded_ds1)
            ds1_idx = nav_idx;
         while (key_pressed(KEY_END))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }
      }

      // Delete key — backup and remove current DS1 file
      if (key_pressed(KEY_DEL) && glb_ds1edit.has_loaded_ds1 &&
          glb_ds1edit.area_browser.selected_group >= 0 &&
          glb_ds1edit.area_browser.selected_entry >= 0)
      {
         while (key_pressed(KEY_DEL))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }

         {
            int gi = glb_ds1edit.area_browser.selected_group;
            int ei = glb_ds1edit.area_browser.selected_entry;

            if (ds1_manager_delete(gi, ei) == 0)
            {
               /* Adjust selection after deletion */
               if (ei >= glb_ds1edit.area_browser.groups[gi].entry_count)
                  glb_ds1edit.area_browser.selected_entry = glb_ds1edit.area_browser.groups[gi].entry_count - 1;
               if (glb_ds1edit.area_browser.selected_entry < 0)
                  glb_ds1edit.area_browser.selected_entry = -1;
            }
         }
      }

      // Insert key — create new DS1 or clone current
      if (key_pressed(KEY_INSERT) &&
          glb_ds1edit.area_browser.selected_group >= 0)
      {
         int gi = glb_ds1edit.area_browser.selected_group;
         AREA_GROUP_S * ins_g = &glb_ds1edit.area_browser.groups[gi];

         while (key_pressed(KEY_INSERT))
         { al_rest(0.01); al_get_keyboard_state(&a5_kb_state); }

         if (ins_g->entry_count > 0)
         {
            int ei = glb_ds1edit.area_browser.selected_entry;

            /* Shift+Insert = clone current, Insert alone = new empty */
            if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
            {
               /* Clone current DS1 (full copy including tiles/objects) */
               if (glb_ds1edit.has_loaded_ds1)
               {
                  if (ds1_manager_clone(ds1_idx, gi, ei) == 0)
                     printf("Insert: cloned DS1 successfully\n");
               }
            }
            else
            {
               /* Create new empty DS1 (same size as selected, same act) */
               int new_act = ins_g->act > 0 ? ins_g->act : 1;
               int new_w = 20, new_h = 20;

               /* Copy dimensions from selected DS1 if loaded */
               if (glb_ds1edit.has_loaded_ds1 && ds1_idx >= 0 && ds1_idx < DS1_MAX &&
                   glb_ds1[ds1_idx].name[0] != '\0')
               {
                  new_w = (int)glb_ds1[ds1_idx].width;
                  new_h = (int)glb_ds1[ds1_idx].height;
                  new_act = (int)glb_ds1[ds1_idx].act;
               }

               if (ds1_manager_create_empty(gi, ei, new_w, new_h, new_act) == 0)
                  printf("Insert: created new empty DS1 (%dx%d)\n", new_w, new_h);
            }
            fflush(stdout);
         }
      }

      // layers toggle
      if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
      {
         // if shift pressed, just 1 layer will be active
         for (n=0; n<7; n++)
         {
            if (key_pressed(key_func_code[n]))
            {
               for (i=0; i<FLOOR_MAX_LAYER; i++)
                  glb_ds1[ds1_idx].floor_layer_mask[i] = 0;

               if (key_pressed(KEY_F11))
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
      if (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
      {
         // if control pressed, just 1 layer will be inactive
         for (n=0; n<7; n++)
         {
            if (key_pressed(key_func_code[n]))
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
      if (key_pressed(KEY_F1))
      {
         while(key_pressed(KEY_F1))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         glb_ds1[ds1_idx].floor_layer_mask[0] = 1 - glb_ds1[ds1_idx].floor_layer_mask[0];
      }
      if (key_pressed(KEY_F2))
      {
         while(key_pressed(KEY_F2))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         glb_ds1[ds1_idx].floor_layer_mask[1] = 1 - glb_ds1[ds1_idx].floor_layer_mask[1];
      }
      if (key_pressed(KEY_F5))
      {
         while(key_pressed(KEY_F5))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         glb_ds1[ds1_idx].wall_layer_mask[0] = 1 - glb_ds1[ds1_idx].wall_layer_mask[0];
      }
      if (key_pressed(KEY_F6))
      {
         while(key_pressed(KEY_F6))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         glb_ds1[ds1_idx].wall_layer_mask[1] = 1 - glb_ds1[ds1_idx].wall_layer_mask[1];
      }
      if (key_pressed(KEY_F7))
      {
         while(key_pressed(KEY_F7))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         glb_ds1[ds1_idx].wall_layer_mask[2] = 1 - glb_ds1[ds1_idx].wall_layer_mask[2];
      }
      if (key_pressed(KEY_F8))
      {
         while(key_pressed(KEY_F8))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         glb_ds1[ds1_idx].wall_layer_mask[3] = 1 - glb_ds1[ds1_idx].wall_layer_mask[3];
      }

      // special tiles layer
      if (key_pressed(KEY_F9))
      {
         while(key_pressed(KEY_F9))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         glb_ds1[ds1_idx].special_layer_mask = 1 - glb_ds1[ds1_idx].special_layer_mask;
      }
      
      // animation layer
      if (key_pressed(KEY_F3))
      {
         while(key_pressed(KEY_F3))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         glb_ds1[ds1_idx].animations_layer_mask++;
         if (glb_ds1[ds1_idx].animations_layer_mask == 3)
            glb_ds1[ds1_idx].animations_layer_mask = 0;
      }
      
      // objects layer
      if (key_pressed(KEY_F4) && (glb_ds1edit.mode != MOD_O))
      {
         while(key_pressed(KEY_F4))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         glb_ds1[ds1_idx].objects_layer_mask++;
         if (glb_ds1[ds1_idx].objects_layer_mask >= OL_MAX)
            glb_ds1[ds1_idx].objects_layer_mask = OL_NONE;
      }

      // paths layer
      if (key_pressed(KEY_F10) && (glb_ds1edit.mode != MOD_P))
      {
         while(key_pressed(KEY_F10))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         glb_ds1[ds1_idx].paths_layer_mask = 1 - glb_ds1[ds1_idx].paths_layer_mask;
      }

      // shadow mode
      if (key_pressed(KEY_F11))
      {
         while(key_pressed(KEY_F11))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
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
      if (key_pressed(KEY_SPACE))
      {
         while(key_pressed(KEY_SPACE))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         glb_ds1[ds1_idx].walkable_layer_mask++;
         if (glb_ds1[ds1_idx].walkable_layer_mask >= 3)
            glb_ds1[ds1_idx].walkable_layer_mask = 0;
      }
      if (key_pressed(KEY_T))
      {
         while(key_pressed(KEY_T))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         glb_ds1[ds1_idx].subtile_help_display =
            1 - glb_ds1[ds1_idx].subtile_help_display;
      }

      // gamma correction
      if (key_pressed(KEY_F12))
      {
         if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
         {
            if (glb_ds1edit.cur_gamma > GC_060)
            {
               rest(80);
               glb_ds1edit.cur_gamma--;
               misc_update_pal_with_gamma();
               a5_current_palette = &glb_ds1edit.vga_pal[glb_ds1[ds1_idx].act - 1];
            }
         }
         else
         {
            if (glb_ds1edit.cur_gamma < GC_300)
            {
               rest(80);
               glb_ds1edit.cur_gamma++;
               misc_update_pal_with_gamma();
               a5_current_palette = &glb_ds1edit.vga_pal[glb_ds1[ds1_idx].act - 1];
            }
         }
      }

      // Home (center the map)
      if (key_pressed(KEY_HOME) && (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT)))
      {
         // Shift+Home: center map view
         while (key_pressed(KEY_HOME))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         cx = glb_ds1[ds1_idx].width/2 + 1;
         cy = glb_ds1[ds1_idx].height/2;
         dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
         dy = (cy *  glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);
         glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0 = dx - glb_ds1edit.win_preview.w / 2;
         glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0 = dy - glb_ds1edit.win_preview.h / 2;
      }

      // Backspace (show all layers)
      if (key_pressed(KEY_BACKSPACE))
      {
         while (key_pressed(KEY_BACKSPACE))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         for (i=0; i<FLOOR_MAX_LAYER; i++)
            glb_ds1[ds1_idx].floor_layer_mask[i]  = 1;
         for (i=0; i<WALL_MAX_LAYER; i++)
            glb_ds1[ds1_idx].wall_layer_mask[i]   = 1;
         for (i=0; i<SHADOW_MAX_LAYER; i++)
            glb_ds1[ds1_idx].shadow_layer_mask[i] = 3;
      }

      // P ('P'rintscreen = screenshot)
      if (key_pressed(KEY_P))
      {
         if ((key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT)))
         {
            // BIG screenshot (complete map)
            old_screen_buff = glb_ds1edit.screen_buff;
            if (wpreview_draw_tiles_big_screenshot(ds1_idx) == 0)
            {
               // big screenshot is ready
               sprintf(tmp, "screenshot-%05i.bmp", glb_ds1edit.screenshot_num);
               while (a5_file_exists(tmp))
               {
                  glb_ds1edit.screenshot_num++;
                  sprintf(tmp, "screenshot-%05i.bmp", glb_ds1edit.screenshot_num);
               }

               // handle palette
               if (glb_ds1edit.cmd_line.force_pal_num == -1)
               {
                  // use .ds1 act value for palette
                  al_save_bitmap(tmp, glb_ds1edit.screen_buff);
               }
               else
               {
                  // use force_pal value for palette
                  al_save_bitmap(tmp, glb_ds1edit.screen_buff);
               }

               // free temp bitmap
               al_destroy_bitmap(glb_ds1edit.screen_buff);
            }
            while ((key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT)))
            {
               al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
            }
            glb_ds1edit.screen_buff = old_screen_buff;
         }
         else
         {
            // normal screenshot (visible screen only)
            sprintf(tmp, "screenshot-%05i.png", glb_ds1edit.screenshot_num);
            while (a5_file_exists(tmp))
            {
               glb_ds1edit.screenshot_num++;
               sprintf(tmp, "screenshot-%05i.png", glb_ds1edit.screenshot_num);
            }

            // draw the mouse cursor onto the buffer
            a5_draw_sprite(
               glb_ds1edit.screen_buff,
               glb_ds1edit.mouse_cursor[glb_ds1edit.mode],
               old_mouse_x - 1,
               old_mouse_y - 1
            );

            // handle palette
            if (glb_ds1edit.cmd_line.force_pal_num == -1)
            {
               // use .ds1 act value for palette
               al_save_bitmap(tmp, glb_ds1edit.screen_buff);
            }
            else
            {
               // use force_pal value for palette
               al_save_bitmap(tmp, glb_ds1edit.screen_buff);
            }
         }
         while (key_pressed(KEY_P))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }

         // the buffer was saved
         glb_ds1edit.screenshot_num++;
      }
      
      // S
      if (key_pressed(KEY_S) && (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL)))
      {
         // CTRL + S : save the ds1, in the current state, incremental save
         ds1_save(ds1_idx, FALSE);
         while (key_pressed(KEY_S))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
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
      if (key_pressed(KEY_S) && (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
          && glb_ds1edit.mode == MOD_T)
      {
         // Shift+S : show all previously hidden tiles
         while (key_pressed(KEY_S))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         edittile_unhide_all(ds1_idx);
      }
      
      // 'C' : either Copy or Center
      if (key_pressed(KEY_C))
      {
         if (glb_ds1edit.mode == MOD_T)
         {
            // TILE mode
            if (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
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

                  while (key_pressed(KEY_C))
                  {
                     al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
                  }
               }
            }
            else
            {
               // Center to mouse in TILE mode
               while (key_pressed(KEY_C))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
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
               al_set_mouse_xy(a5_display, glb_ds1edit.win_preview.w / 2, glb_ds1edit.win_preview.h / 2);
            }
         }
         else
         {
            if ( ! key_pressed(KEY_LCONTROL) && ! key_pressed(KEY_RCONTROL))
            {
               // Center to mouse in OBJECT / PATH mode
               while (key_pressed(KEY_C))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
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
               al_set_mouse_xy(a5_display, glb_ds1edit.win_preview.w / 2, glb_ds1edit.win_preview.h / 2);
            }
         }
      }

      // CTRL + X : Copy selected tiles, WITH CUT (crop / paste)
      if (key_pressed(KEY_X) && (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL)))
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
               while (key_pressed(KEY_X))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
               }
            }
         }
      }

      // DEL key (regular or keypad) : delete all selected layers of all tiles
      if (key_pressed(KEY_DEL) || key_pressed(KEY_DEL_PAD))
      {
         if (glb_ds1edit.mode == MOD_T)
         {
            if (paste_pos.start == FALSE)
            {
               edittile_delete_selected_tiles(ds1_idx);
               while (key_pressed(KEY_DEL) || key_pressed(KEY_DEL_PAD))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
               }
            }
         }
      }

      // CTRL + U : undo tiles modification
      if (key_pressed(KEY_U) && (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL)))
      {
         if (glb_ds1edit.mode == MOD_T)
         {
            if (paste_pos.start == FALSE)
            {
               undo_apply_tile_buffer(ds1_idx);
               while (key_pressed(KEY_U))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
               }
            }
         }
      }

      // G : toggle tile grid
      if (key_pressed(KEY_G))
      {
         if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
            glb_ds1edit.display_tile_grid --;
         else
            glb_ds1edit.display_tile_grid ++;
         if (glb_ds1edit.display_tile_grid < TG_OFF)
            glb_ds1edit.display_tile_grid = TG_MAX - 1;
         if (glb_ds1edit.display_tile_grid >= TG_MAX)
            glb_ds1edit.display_tile_grid = TG_OFF;
         while(key_pressed(KEY_G))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
      }

      // changing current ds1
      group_changed = FALSE;
      old_group     = glb_ds1edit.ds1_group_idx;
      if (can_swich_mode)
      {
         if (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
         {
            if (key_pressed(KEY_1))
            {
               while (key_pressed(KEY_1) || key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
               }
               glb_ds1edit.ds1_group_idx = 0;
               group_changed = TRUE;
            }
            else if (key_pressed(KEY_2))
            {
               while (key_pressed(KEY_2) || key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
               }
               glb_ds1edit.ds1_group_idx = 1;
               group_changed = TRUE;
            }
            else if (key_pressed(KEY_3))
            {
               while (key_pressed(KEY_3) || key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
               }
               glb_ds1edit.ds1_group_idx = 2;
               group_changed = TRUE;
            }
            else if (key_pressed(KEY_4))
            {
               while (key_pressed(KEY_4) || key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
               }
               glb_ds1edit.ds1_group_idx = 3;
               group_changed = TRUE;
            }
            else if (key_pressed(KEY_5))
            {
               while (key_pressed(KEY_5) || key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
               }
               glb_ds1edit.ds1_group_idx = 4;
               group_changed = TRUE;
            }
            else if (key_pressed(KEY_6))
            {
               while (key_pressed(KEY_6) || key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
               }
               glb_ds1edit.ds1_group_idx = 5;
               group_changed = TRUE;
            }
            else if (key_pressed(KEY_7))
            {
               while (key_pressed(KEY_7) || key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
               }
               glb_ds1edit.ds1_group_idx = 6;
               group_changed = TRUE;
            }
            else if (key_pressed(KEY_8))
            {
               while (key_pressed(KEY_8) || key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
               }
               glb_ds1edit.ds1_group_idx = 7;
               group_changed = TRUE;
            }
            else if (key_pressed(KEY_9))
            {
               while (key_pressed(KEY_9) || key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
               }
               glb_ds1edit.ds1_group_idx = 8;
               group_changed = TRUE;
            }
            else if (key_pressed(KEY_0))
            {
               while (key_pressed(KEY_0) || key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
               {
                  al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
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
         if (key_pressed(KEY_1) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10;
            while (key_pressed(KEY_1))
            {
               al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
            }
         }
         if (key_pressed(KEY_2) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 1].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 1;
            while (key_pressed(KEY_2))
            {
               al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
            }
         }
         if (key_pressed(KEY_3) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 2].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 2;
            while (key_pressed(KEY_3))
            {
               al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
            }
         }
         if (key_pressed(KEY_4) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 3].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 3;
            while (key_pressed(KEY_4))
            {
               al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
            }
         }
         if (key_pressed(KEY_5) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 4].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 4;
            while (key_pressed(KEY_5))
            {
               al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
            }
         }
         if (key_pressed(KEY_6) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 5].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 5;
            while (key_pressed(KEY_6))
            {
               al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
            }
         }
         if (key_pressed(KEY_7) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 6].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 6;
            while (key_pressed(KEY_7))
            {
               al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
            }
         }
         if (key_pressed(KEY_8) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 7].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 7;
            while (key_pressed(KEY_8))
            {
               al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
            }
         }
         if (key_pressed(KEY_9) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 8].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 8;
            while (key_pressed(KEY_9))
            {
               al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
            }
         }
         if (key_pressed(KEY_0) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 9].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 9;
            while (key_pressed(KEY_0))
            {
               al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
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
      if (key_pressed(KEY_TILDE))
      {
         while (key_pressed(KEY_TILDE))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         if (glb_ds1edit.show_2nd_row == FALSE)
            glb_ds1edit.show_2nd_row = TRUE;
         else
            glb_ds1edit.show_2nd_row = FALSE;
      }
      
      // TAB : change edit mode
      if (key_pressed(KEY_TAB))
      {
         while (key_pressed(KEY_TAB))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
         }
         if (glb_ds1edit.mode == MOD_L)
            glb_ds1edit.mode = old_mode;
         else
         {
            if (can_swich_mode)
            {
               if ((key_pressed(KEY_LSHIFT)) || (key_pressed(KEY_RSHIFT)))
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
      if (key_pressed(KEY_N))
      {
         while (key_pressed(KEY_N))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
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
      if (key_pressed(KEY_R))
      {
         while (key_pressed(KEY_R))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
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
               while (a5_mouse_b & 1) // NOT old_mouse_b else infinite loop
               {
                  al_rest(0.01); al_get_mouse_state(&a5_ms_state);
               }
            }
            else if (tmp_sel.start == FALSE)
            {
               if ( (key_pressed(KEY_I)) &&
                    (cx != old_identical_x) && (cy != old_identical_y)
                  )
               {
                  // for all the tiles Identical to the visible ones

                  if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
                  {
                     // add to selection
                     itype = IT_ADD;
                  }
                  else if (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
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
               else if ( ! key_pressed(KEY_I))
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
               if (key_pressed(KEY_H))
                  tmp_sel.type = TMP_HIDE;
               else if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
                  tmp_sel.type = TMP_ADD;
               else if (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
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
            while (a5_mouse_b & 2) // NOT old_mouse_b else infinite loop
            {
               al_rest(0.01); al_get_mouse_state(&a5_ms_state);
            }
            if ( (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL)) &&
                 (key_pressed(KEY_LSHIFT)   || key_pressed(KEY_RSHIFT)) )
            {
               // advanced tile editing window (bits)
               wbits_main(ds1_idx, cx, cy);
               al_set_mouse_xy(a5_display, old_mouse_x, old_mouse_y);
            }
            else
            {
               wedit_test(ds1_idx, cx, cy);
               al_set_mouse_xy(a5_display, old_mouse_x, old_mouse_y);
            }
         }
      }

      // quit
      if (key_pressed(KEY_ESC) && (glb_ds1[ds1_idx].draw_edit_obj == FALSE))
      {
         while (key_pressed(KEY_ESC))
         {
            al_rest(0.01); al_get_keyboard_state(&a5_kb_state);
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

      perf_accumulate(
         &glb_perf_stats.ui_ms_total,
         &glb_perf_stats.ui_ms_max,
         perf_now_ms() - section_start_ms
      );
      perf_accumulate(
         &glb_perf_stats.frame_ms_total,
         &glb_perf_stats.frame_ms_max,
         perf_now_ms() - frame_start_ms
      );
      glb_perf_stats.frames++;
      if (glb_perf_stats.frames >= 30)
         perf_print_summary();
   }

   perf_print_summary();
}
