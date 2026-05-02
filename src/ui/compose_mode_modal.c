#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "misc.h"
#include "ui/compose_mode_modal.h"

#define MODAL_W            420
#define MODAL_H            150
#define TITLE_HEIGHT        20
#define STATUS_HEIGHT       18
#define BORDER               2
#define BUTTON_W            96
#define BUTTON_H            26

#define COLOR_PANEL_BG     al_map_rgba(10, 12, 16, 235)
#define COLOR_BORDER       al_map_rgb(120, 160, 200)
#define COLOR_TITLE_BG     al_map_rgb(20, 40, 60)
#define COLOR_TITLE_FG     al_map_rgb(200, 220, 255)
#define COLOR_PROMPT_FG    al_map_rgb(200, 215, 230)
#define COLOR_DIM_FG       al_map_rgb(140, 160, 180)
#define COLOR_BTN_BG       al_map_rgb(40, 60, 90)
#define COLOR_BTN_HOVER    al_map_rgb(60, 90, 130)
#define COLOR_BTN_DEFAULT  al_map_rgb(60, 90, 130)
#define COLOR_BTN_FG       al_map_rgb(220, 230, 250)

static int point_in_rect(int mx, int my, int x0, int y0, int x1, int y1)
{
   return mx >= x0 && mx < x1 && my >= y0 && my < y1;
}

int compose_mode_modal_show(void)
{
   int x0, y0, x1, y1;
   int yes_x0, yes_y0, yes_x1, yes_y1;
   int no_x0,  no_y0,  no_x1,  no_y1;
   int line_h = al_get_font_line_height(a5_font);
   int done = 0;
   int result = 0;
   int prev_mb = 1;
   int prompt_y;

   x0 = (glb_config.screen.width  - MODAL_W) / 2;
   y0 = (glb_config.screen.height - MODAL_H) / 2;
   x1 = x0 + MODAL_W;
   y1 = y0 + MODAL_H;

   prompt_y = y0 + TITLE_HEIGHT + 14;

   yes_x0 = x0 + 60;
   yes_x1 = yes_x0 + BUTTON_W;
   yes_y0 = y1 - STATUS_HEIGHT - BUTTON_H - 14;
   yes_y1 = yes_y0 + BUTTON_H;

   no_x0  = x1 - 60 - BUTTON_W;
   no_x1  = no_x0 + BUTTON_W;
   no_y0  = yes_y0;
   no_y1  = yes_y1;

   al_flush_event_queue(a5_event_queue);

   while (!done)
   {
      ALLEGRO_EVENT ev;
      int mx, my, mb;
      int hover_yes, hover_no;
      ALLEGRO_BITMAP *prev;

      while (al_get_next_event(a5_event_queue, &ev))
      {
         if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
         {
            done = 1;
            break;
         }
         if (ev.type == ALLEGRO_EVENT_KEY_CHAR)
         {
            int kc = ev.keyboard.keycode;
            if (kc == ALLEGRO_KEY_ESCAPE)
            {
               done = 1;
               break;
            }
            if (kc == ALLEGRO_KEY_ENTER || kc == ALLEGRO_KEY_PAD_ENTER
                || kc == ALLEGRO_KEY_Y)
            {
               result = 1;
               done = 1;
               break;
            }
            if (kc == ALLEGRO_KEY_N)
            {
               result = 2;
               done = 1;
               break;
            }
         }
      }

      al_get_mouse_state(&a5_ms_state);
      mx = a5_mouse_x;
      my = a5_mouse_y;
      mb = a5_mouse_b;
      hover_yes = point_in_rect(mx, my, yes_x0, yes_y0, yes_x1, yes_y1);
      hover_no  = point_in_rect(mx, my, no_x0,  no_y0,  no_x1,  no_y1);
      if (mb && !prev_mb)
      {
         if (hover_yes) { result = 1; done = 1; }
         else if (hover_no)  { result = 2; done = 1; }
      }
      prev_mb = mb;

      prev = al_get_target_bitmap();
      al_set_target_bitmap(glb_ds1edit.screen_buff);

      /* panel + border */
      al_draw_filled_rectangle((float) x0, (float) y0,
                               (float) x1, (float) y1, COLOR_PANEL_BG);
      al_draw_rectangle((float) x0 + 0.5f, (float) y0 + 0.5f,
                        (float) x1 - 0.5f, (float) y1 - 0.5f,
                        COLOR_BORDER, 1.0f);

      /* title bar */
      al_draw_filled_rectangle((float) x0, (float) y0,
                               (float) x1, (float) (y0 + TITLE_HEIGHT),
                               COLOR_TITLE_BG);
      al_draw_text(a5_font, COLOR_TITLE_FG,
                   (float) (x0 + 8),
                   (float) (y0 + (TITLE_HEIGHT - line_h) / 2),
                   0, "Compose Mode");

      /* prompt text */
      al_draw_text(a5_font, COLOR_PROMPT_FG,
                   (float) (x0 + 16), (float) prompt_y, 0,
                   "Compose fully-animated character / monster /");
      al_draw_text(a5_font, COLOR_PROMPT_FG,
                   (float) (x0 + 16), (float) (prompt_y + line_h + 2), 0,
                   "NPC / object output (one APNG per direction)?");

      /* Yes button (default; outlined) */
      al_draw_filled_rectangle((float) yes_x0, (float) yes_y0,
                               (float) yes_x1, (float) yes_y1,
                               hover_yes ? COLOR_BTN_HOVER : COLOR_BTN_DEFAULT);
      al_draw_rectangle((float) yes_x0 + 0.5f, (float) yes_y0 + 0.5f,
                        (float) yes_x1 - 0.5f, (float) yes_y1 - 0.5f,
                        COLOR_BORDER, 2.0f);
      {
         const char *label = "Yes (default)";
         int tw = al_get_text_width(a5_font, label);
         int tx = yes_x0 + ((yes_x1 - yes_x0) - tw) / 2;
         int ty = yes_y0 + (BUTTON_H - line_h) / 2;
         al_draw_text(a5_font, COLOR_BTN_FG,
                      (float) tx, (float) ty, 0, label);
      }

      /* No button */
      al_draw_filled_rectangle((float) no_x0, (float) no_y0,
                               (float) no_x1, (float) no_y1,
                               hover_no ? COLOR_BTN_HOVER : COLOR_BTN_BG);
      al_draw_rectangle((float) no_x0 + 0.5f, (float) no_y0 + 0.5f,
                        (float) no_x1 - 0.5f, (float) no_y1 - 0.5f,
                        COLOR_BORDER, 1.0f);
      {
         const char *label = "No (raw export)";
         int tw = al_get_text_width(a5_font, label);
         int tx = no_x0 + ((no_x1 - no_x0) - tw) / 2;
         int ty = no_y0 + (BUTTON_H - line_h) / 2;
         al_draw_text(a5_font, COLOR_BTN_FG,
                      (float) tx, (float) ty, 0, label);
      }

      /* status hint */
      al_draw_filled_rectangle((float) x0, (float) (y1 - STATUS_HEIGHT),
                               (float) x1, (float) y1, COLOR_TITLE_BG);
      al_draw_text(a5_font, COLOR_DIM_FG,
                   (float) (x0 + 8),
                   (float) (y1 - STATUS_HEIGHT + (STATUS_HEIGHT - line_h) / 2),
                   0,
                   "[Y/Enter] yes  [N] no  [Esc] cancel");

      if (prev != NULL)
         al_set_target_bitmap(prev);

      misc_draw_screen(mx, my);
      al_rest(0.016);
   }

   return result;
}
