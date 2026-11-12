#include <stdio.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "misc.h"
#include "ui/compat.h"
#include "ui/commands.h"
#include "ui/input.h"
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
      ui_text(col, (float) (x + 6), (float) (y + 3), 0, def->label);
      ui_text(col, (float) (x + MENU_DROP_W - 6), (float) (y + 3),
              ALLEGRO_ALIGN_RIGHT, def->shortcut);
   }
}

/* Drawn in logical pixels, under the UI scale misc_draw_screen() sets. */
void menu_bar_draw(void)
{
   int h = ui_menu_bar_height();
   int w, i;

   if (h == 0)
      return;
   w = ui_w();

   al_draw_filled_rectangle(0, 0, (float) w, (float) h, al_map_rgb(36, 30, 24));
   al_draw_line(0, (float) h - 0.5f, (float) w, (float) h - 0.5f, al_map_rgb(120, 110, 90), 1.0f);

   for (i = 0; i < MENU_COUNT; i++)
   {
      int x = menu_title_x(i);
      if (i == s_state.menu)
         al_draw_filled_rectangle((float) (x - 4), 0,
                                  (float) (x + (int) strlen(menu_title(i)) * 8 + 4),
                                  (float) (h - 1), al_map_rgb(60, 80, 130));
      ui_text(al_map_rgb(235, 225, 200), (float) x, UI_BAR_TEXT_Y, 0, menu_title(i));
   }

   if (s_state.menu >= 0)
      draw_dropdown(h);
}

static int held_mods(void)
{
   int mods = 0;

   if (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
      mods |= CMD_MOD_CTRL;
   if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
      mods |= CMD_MOD_SHIFT;
   if (key_pressed(KEY_ALT))
      mods |= CMD_MOD_ALT;
   return mods;
}

/* Run an open menu until it closes. start_menu is -1 for a press on an empty
 * part of the bar, which only waits for the release. */
static CMD_ID run_menu(int start_menu, const CMD_CTX *ctx)
{
   static const int nav_keys[] = {
      ALLEGRO_KEY_ESCAPE, ALLEGRO_KEY_UP, ALLEGRO_KEY_DOWN, ALLEGRO_KEY_LEFT,
      ALLEGRO_KEY_RIGHT, ALLEGRO_KEY_ENTER, ALLEGRO_KEY_PAD_ENTER
   };
   int bar_h = ui_menu_bar_height();
   CMD_ID chosen = CMD_NONE;
   int last_x = ui_mouse_x(), last_y = ui_mouse_y();
   size_t k;

   s_ctx = *ctx;
   s_state.menu = -1;
   if (start_menu >= 0)
      menu_state_open(&s_state, start_menu);
   input_suppress_held();

   while (s_state.menu >= 0 && !a5_display_closed)
   {
      int title, item;

      input_pump();

      for (k = 0; k < sizeof(nav_keys) / sizeof(nav_keys[0]) && s_state.menu >= 0; k++)
         if (key_hit(nav_keys[k]))
            chosen = menu_state_key(&s_state, nav_keys[k], ctx);

      if (s_state.menu >= 0)
      {
         int mx = ui_mouse_x(), my = ui_mouse_y();
         int moved = (mx != last_x || my != last_y);

         last_x = mx;
         last_y = my;

         /* Hover only acts when the pointer moves, so a resting pointer
            doesn't override the arrow keys. */
         if (moved)
         {
            title = menu_title_at(mx, my, bar_h);
            if (title >= 0 && title != s_state.menu)
               menu_state_open(&s_state, title);
            else
            {
               item = menu_item_at(s_state.menu, mx, my, bar_h);
               if (item >= 0)
                  s_state.item = item;
            }
         }

         /* Judged against the menu that's open now, wherever the click lands. */
         item = menu_item_at(s_state.menu, mx, my, bar_h);

         if (mouse_hit(1))
         {
            if (item >= 0)
            {
               CMD_ID id = commands_menu_item(s_state.menu, item);
               if (commands_is_available(id, ctx))
               {
                  chosen = id;
                  s_state.menu = -1;
               }
            }
            else
            {
               /* The title that's open, or anywhere else, closes it. */
               s_state.menu = -1;
            }
         }
      }

      misc_draw_screen(a5_mouse_x, a5_mouse_y);
      al_rest(0.01);
   }

   s_state.menu = -1;

   /* The press that closed the menu mustn't land on the map behind it. */
   input_pump();
   /* Raw state, not key_pressed(): Enter is suppressed while held, and that is
      what this loop must outwait so it can't answer the dialog a row opens. */
   while (((a5_mouse_b & 1) || al_key_down(&a5_kb_state, ALLEGRO_KEY_ENTER) ||
           al_key_down(&a5_kb_state, ALLEGRO_KEY_PAD_ENTER)) && !a5_display_closed)
   {
      al_rest(0.01);
      input_pump();
   }
   input_suppress_held();
   return chosen;
}

CMD_ID menu_bar_poll(const CMD_CTX *ctx, int keys_enabled)
{
   static const struct { int key; int menu; } mnemonics[] = {
      { ALLEGRO_KEY_F, MENU_FILE }, { ALLEGRO_KEY_E, MENU_EDIT },
      { ALLEGRO_KEY_M, MENU_MAP },  { ALLEGRO_KEY_V, MENU_VIEW },
      { ALLEGRO_KEY_B, MENU_BUILD }
   };
   int bar_h = ui_menu_bar_height();
   size_t i;
   int key;

   if (bar_h == 0)
      return CMD_NONE;

   if (mouse_hit(1) && ui_mouse_y() < bar_h)
      return run_menu(menu_title_at(ui_mouse_x(), ui_mouse_y(), bar_h), ctx);

   /* While the panel is editing, mnemonics and plain keys belong to it. A key
      with a modifier that the table owns is still used up, so it doesn't reach
      polled code: Ctrl+N mustn't toggle night mode behind a text field. */
   if (!keys_enabled)
   {
      int mods = held_mods();

      if (mods != 0)
         for (key = 1; key < ALLEGRO_KEY_MAX; key++)
            if (a5_key_hit[key] && commands_find_key(key, mods) != CMD_NONE)
            {
               a5_key_hit[key] = 0;
               a5_key_suppressed[key] = 1;
            }
      return CMD_NONE;
   }

   if (held_mods() == CMD_MOD_ALT)
      for (i = 0; i < sizeof(mnemonics) / sizeof(mnemonics[0]); i++)
         if (key_hit(mnemonics[i].key))
            return run_menu(mnemonics[i].menu, ctx);

   for (key = 1; key < ALLEGRO_KEY_MAX; key++)
   {
      if (a5_key_hit[key])
      {
         CMD_ID id = commands_find_key(key, held_mods());
         if (id != CMD_NONE)
         {
            /* Used up: polled code mustn't see it, even as held, until it's
               pressed again. So Ctrl+N doesn't toggle night mode and Ctrl+S
               doesn't scroll. */
            a5_key_hit[key] = 0;
            a5_key_suppressed[key] = 1;
            if (id == CMD_MAP_DELETE)
               return commands_ctrl_del_target(ctx);
            return commands_is_available(id, ctx) ? id : CMD_NONE;
         }
      }
   }
   return CMD_NONE;
}
