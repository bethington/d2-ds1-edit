#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "misc.h"
#include "ui/compose_category_picker.h"

#define ENTRY_COUNT        5
#define ROW_HEIGHT         24
#define TITLE_HEIGHT       20
#define STATUS_HEIGHT      18
#define BORDER             2
#define PICKER_WIDTH       400
#define PICKER_HEIGHT      (TITLE_HEIGHT + ENTRY_COUNT * ROW_HEIGHT \
                            + STATUS_HEIGHT + 2 * BORDER)

#define COLOR_PANEL_BG     al_map_rgba(10, 12, 16, 235)
#define COLOR_BORDER       al_map_rgb(120, 160, 200)
#define COLOR_TITLE_BG     al_map_rgb(20, 40, 60)
#define COLOR_TITLE_FG     al_map_rgb(200, 220, 255)
#define COLOR_ROW_FG       al_map_rgb(180, 200, 220)
#define COLOR_ROW_SEL_BG   al_map_rgb(40, 70, 110)
#define COLOR_ROW_SEL_FG   al_map_rgb(255, 255, 255)
#define COLOR_STATUS_FG    al_map_rgb(140, 160, 180)

static const char *g_labels[ENTRY_COUNT] = {
   "All composed",
   "Player characters",
   "Monsters",
   "NPCs",
   "Objects"
};

static const COMPOSE_CATEGORY_E g_values[ENTRY_COUNT] = {
   COMPOSE_CATEGORY_NONE,        /* "All composed" */
   COMPOSE_CATEGORY_PLAYER_CHAR,
   COMPOSE_CATEGORY_MONSTER,
   COMPOSE_CATEGORY_NPC,
   COMPOSE_CATEGORY_OBJECT
};

int compose_category_picker_show(COMPOSE_CATEGORY_E *out)
{
   int x0, y0, x1, y1, list_y0;
   int selected = 0;
   int done = 0;
   int prev_mb = 1;
   int confirmed = 0;
   int line_h = al_get_font_line_height(a5_font);

   if (out == NULL)
      return 0;

   x0 = (glb_config.screen.width  - PICKER_WIDTH) / 2;
   y0 = (glb_config.screen.height - PICKER_HEIGHT) / 2;
   x1 = x0 + PICKER_WIDTH;
   y1 = y0 + PICKER_HEIGHT;
   list_y0 = y0 + TITLE_HEIGHT + BORDER;

   al_flush_event_queue(a5_event_queue);

   while (!done)
   {
      ALLEGRO_EVENT ev;
      int mx, my, mb;
      ALLEGRO_BITMAP *prev;
      int row;

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
            if (kc == ALLEGRO_KEY_ENTER || kc == ALLEGRO_KEY_PAD_ENTER)
            {
               confirmed = 1;
               done = 1;
               break;
            }
            if (kc == ALLEGRO_KEY_UP)
            {
               selected--;
               if (selected < 0) selected = ENTRY_COUNT - 1;
            }
            else if (kc == ALLEGRO_KEY_DOWN)
            {
               selected++;
               if (selected >= ENTRY_COUNT) selected = 0;
            }
         }
      }

      al_get_mouse_state(&a5_ms_state);
      mx = a5_mouse_x;
      my = a5_mouse_y;
      mb = a5_mouse_b;
      if (mb && !prev_mb && mx >= x0 && mx < x1
          && my >= list_y0 && my < list_y0 + ENTRY_COUNT * ROW_HEIGHT)
      {
         selected = (my - list_y0) / ROW_HEIGHT;
         if (selected < 0) selected = 0;
         if (selected >= ENTRY_COUNT) selected = ENTRY_COUNT - 1;
         confirmed = 1;
         done = 1;
      }
      prev_mb = mb;

      prev = al_get_target_bitmap();
      al_set_target_bitmap(glb_ds1edit.screen_buff);

      al_draw_filled_rectangle((float) x0, (float) y0,
                               (float) x1, (float) y1, COLOR_PANEL_BG);
      al_draw_rectangle((float) x0 + 0.5f, (float) y0 + 0.5f,
                        (float) x1 - 0.5f, (float) y1 - 0.5f,
                        COLOR_BORDER, 1.0f);

      al_draw_filled_rectangle((float) x0, (float) y0,
                               (float) x1, (float) (y0 + TITLE_HEIGHT),
                               COLOR_TITLE_BG);
      al_draw_text(a5_font, COLOR_TITLE_FG,
                   (float) (x0 + 8),
                   (float) (y0 + (TITLE_HEIGHT - line_h) / 2),
                   0, "Compose - Choose Category");

      for (row = 0; row < ENTRY_COUNT; row++)
      {
         int y = list_y0 + row * ROW_HEIGHT;
         int is_sel = (row == selected);
         if (is_sel)
         {
            al_draw_filled_rectangle((float) (x0 + BORDER), (float) y,
                                     (float) (x1 - BORDER),
                                     (float) (y + ROW_HEIGHT),
                                     COLOR_ROW_SEL_BG);
         }
         al_draw_text(a5_font,
                      is_sel ? COLOR_ROW_SEL_FG : COLOR_ROW_FG,
                      (float) (x0 + 12),
                      (float) (y + (ROW_HEIGHT - line_h) / 2),
                      0, g_labels[row]);
      }

      al_draw_filled_rectangle((float) x0,
                               (float) (y1 - STATUS_HEIGHT),
                               (float) x1, (float) y1,
                               COLOR_TITLE_BG);
      al_draw_text(a5_font, COLOR_STATUS_FG,
                   (float) (x0 + 8),
                   (float) (y1 - STATUS_HEIGHT + (STATUS_HEIGHT - line_h) / 2),
                   0,
                   "[Up/Down] select  [Enter] confirm  [Esc] cancel");

      if (prev != NULL)
         al_set_target_bitmap(prev);

      misc_draw_screen(mx, my);
      al_rest(0.016);
   }

   if (!confirmed)
      return 0;

   *out = g_values[selected];
   return 1;
}
