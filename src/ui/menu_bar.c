#include <stdio.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "ui/compat.h"
#include "ui/commands.h"
#include "ui/menu_bar.h"
#include "ui/ui_layout.h"

/* The open menu, or -1. Only the modal loop opens one. */
static MENU_STATE s_state = { -1, 0 };
static CMD_CTX    s_ctx;

static void draw_dropdown(int bar_h)
{
   ALLEGRO_COLOR col_bg     = al_map_rgb(24, 20, 16);
   ALLEGRO_COLOR col_border = al_map_rgb(120, 110, 90);
   ALLEGRO_COLOR col_hover  = al_map_rgb(60, 80, 130);
   ALLEGRO_COLOR col_text   = al_map_rgb(235, 225, 200);
   ALLEGRO_COLOR col_dim    = al_map_rgb(110, 105, 95);
   int count = commands_menu_item_count(s_state.menu);
   int x = menu_title_x(s_state.menu);
   int i;

   al_draw_filled_rectangle((float) x, (float) bar_h, (float) (x + MENU_DROP_W),
                            (float) (bar_h + count * MENU_ITEM_H), col_bg);
   al_draw_rectangle((float) x + 0.5f, (float) bar_h + 0.5f,
                     (float) (x + MENU_DROP_W) - 0.5f,
                     (float) (bar_h + count * MENU_ITEM_H) - 0.5f, col_border, 1.0f);

   for (i = 0; i < count; i++)
   {
      const CMD_DEF *def = commands_get(commands_menu_item(s_state.menu, i));
      int y = bar_h + i * MENU_ITEM_H;
      ALLEGRO_COLOR col = commands_is_available(def->id, &s_ctx) ? col_text : col_dim;

      if (i == s_state.item)
         al_draw_filled_rectangle((float) x + 1, (float) y, (float) (x + MENU_DROP_W) - 1,
                                  (float) (y + MENU_ITEM_H), col_hover);
      al_draw_text(a5_font, col, (float) (x + 6), (float) (y + 3), 0, def->label);
      al_draw_text(a5_font, col, (float) (x + MENU_DROP_W - 6), (float) (y + 3),
                   ALLEGRO_ALIGN_RIGHT, def->shortcut);
   }
}

void menu_bar_draw(void)
{
   int h = ui_menu_bar_height();
   int w, i;

   if (h == 0)
      return;
   w = al_get_display_width(a5_display);

   al_draw_filled_rectangle(0, 0, (float) w, (float) h, al_map_rgb(36, 30, 24));
   al_draw_line(0, (float) h - 0.5f, (float) w, (float) h - 0.5f, al_map_rgb(120, 110, 90), 1.0f);

   for (i = 0; i < MENU_COUNT; i++)
   {
      int x = menu_title_x(i);
      if (i == s_state.menu)
         al_draw_filled_rectangle((float) (x - 4), 0,
                                  (float) (x + (int) strlen(menu_title(i)) * 8 + 4),
                                  (float) (h - 1), al_map_rgb(60, 80, 130));
      al_draw_text(a5_font, al_map_rgb(235, 225, 200), (float) x, 3, 0, menu_title(i));
   }

   if (s_state.menu >= 0)
      draw_dropdown(h);
}
