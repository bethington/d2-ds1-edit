#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "misc.h"
#include "core/ds1_manager.h"
#include "core/map_ops.h"
#include "core/txtread.h"
#include "ui/compat.h"
#include "ui/input.h"
#include "ui/map_dialog.h"

#define DLG_W        520
#define DLG_H        218
#define TITLE_H      20
#define ROW_H        26
#define LABEL_W      84
#define BTN_W        64
#define BTN_H        22
#define STATUS_H     18
#define FIELD_COUNT  5

enum { F_NAME, F_AREA, F_TILESET, F_WIDTH, F_HEIGHT };

typedef struct DLG
{
   MAP_DIALOG_S * d;
   char name[64], w[8], h[8];
   int  focus;
   int  defs[64];
   int  def_count, def_index;
   char status[160];
   int  x0, y0;
} DLG;

static int rect_hit(int mx, int my, int x, int y, int w, int h)
{
   return mx >= x && mx < x + w && my >= y && my < y + h;
}

static int usable_group(int gi)
{
   AREA_GROUP_S * g = &glb_ds1edit.area_browser.groups[gi];
   return !g->is_backup && g->entry_count > 0;
}

/* The distinct Defs in the chosen area, keeping the chosen one if it's there. */
static void load_defs(DLG * m)
{
   AREA_GROUP_S * g = &glb_ds1edit.area_browser.groups[m->d->group_idx];
   int j, k, wanted = m->d->def;

   m->def_count = 0;
   m->def_index = 0;
   for (j = 0; j < g->entry_count && m->def_count < 64; j++)
   {
      int def = g->entries[j].lvlprest_def, seen = 0;

      for (k = 0; k < m->def_count; k++)
         seen = seen || m->defs[k] == def;
      if (seen || def <= 0)
         continue;
      if (def == wanted)
         m->def_index = m->def_count;
      m->defs[m->def_count++] = def;
   }
   m->d->def = m->def_count > 0 ? m->defs[m->def_index] : 0;
}

static void cycle_area(DLG * m, int dir)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int gi = m->d->group_idx, tries;

   for (tries = 0; tries < ab->group_count; tries++)
   {
      gi = (gi + dir + ab->group_count) % ab->group_count;
      if (usable_group(gi))
      {
         m->d->group_idx = gi;
         load_defs(m);
         return;
      }
   }
}

static void cycle_tileset(DLG * m, int dir)
{
   if (m->def_count == 0)
      return;
   m->def_index = (m->def_index + dir + m->def_count) % m->def_count;
   m->d->def = m->defs[m->def_index];
}

static char * focused_text(DLG * m, int * cap, int * digits_only)
{
   *digits_only = 0;
   switch (m->focus)
   {
   case F_NAME:   *cap = (int) sizeof(m->name); return m->name;
   case F_WIDTH:  *cap = (int) sizeof(m->w); *digits_only = 1; return m->w;
   case F_HEIGHT: *cap = (int) sizeof(m->h); *digits_only = 1; return m->h;
   default:       return NULL;
   }
}

static void next_focus(DLG * m, int dir)
{
   do
      m->focus = (m->focus + dir + FIELD_COUNT) % FIELD_COUNT;
   while (m->d->is_clone && (m->focus == F_WIDTH || m->focus == F_HEIGHT));
}

/* 1 if every rule holds; otherwise the first problem goes in the status row. */
static int try_accept(DLG * m)
{
   char file[64];

   if (map_ops_check_name(m->name, file, sizeof(file), m->status, sizeof(m->status)) != 0)
      return 0;
   if (ds1_manager_name_in_use(m->d->group_idx, file))
   {
      snprintf(m->status, sizeof(m->status), "This area already has a map named %s.", file);
      return 0;
   }
   if (m->def_count == 0)
   {
      snprintf(m->status, sizeof(m->status), "This area has no LvlPrest row to add a map to.");
      return 0;
   }
   if (!m->d->is_clone)
   {
      int w = atoi(m->w), h = atoi(m->h);
      if (map_ops_check_size(w, h, m->status, sizeof(m->status)) != 0)
         return 0;
      m->d->width = w;
      m->d->height = h;
   }
   snprintf(m->d->file_name, sizeof(m->d->file_name), "%s", file);
   return 1;
}

static void draw_field(const DLG * m, int field, const char * label, const char * value,
                       int chooser)
{
   int row = field;
   int y = m->y0 + TITLE_H + 8 + row * ROW_H;
   int x = m->x0 + 12 + LABEL_W;
   int w = DLG_W - 24 - LABEL_W;
   int focused = m->focus == field;

   al_draw_text(a5_font, al_map_rgb(180, 200, 220), (float)(m->x0 + 12), (float)(y + 6), 0, label);
   al_draw_filled_rectangle((float)x, (float)y, (float)(x + w), (float)(y + 20),
                            al_map_rgb(30, 32, 40));
   al_draw_rectangle((float)x + 0.5f, (float)y + 0.5f, (float)(x + w) - 0.5f, (float)(y + 20) - 0.5f,
                     focused ? al_map_rgb(255, 230, 80) : al_map_rgb(80, 100, 130), 1.0f);
   al_draw_textf(a5_font, al_map_rgb(255, 255, 255), (float)(x + 6), (float)(y + 6), 0,
                 chooser ? "< %s >" : "%s", value);
}

static void tileset_label(const DLG * m, char * out, int cap)
{
   TXT_S * txt = glb_ds1edit.lvlprest_buff;

   snprintf(out, cap, "Def %d", m->d->def);
   if (txt != NULL)
   {
      int def_col = misc_get_txt_column_num(RQ_LVLPREST, "Def");
      int name_col = misc_get_txt_column_num(RQ_LVLPREST, "Name");
      int i;

      for (i = 0; i < txt->line_num; i++)
      {
         long * def = (long *)(txt->data + i * txt->line_size + txt->col[def_col].offset);
         if (*def == m->d->def)
         {
            snprintf(out, cap, "%s (Def %d)",
                     txt->data + i * txt->line_size + txt->col[name_col].offset, m->d->def);
            return;
         }
      }
   }
}

static void draw_dialog(const DLG * m, int hover_ok, int hover_cancel)
{
   AREA_GROUP_S * g = &glb_ds1edit.area_browser.groups[m->d->group_idx];
   ALLEGRO_BITMAP * prev = al_get_target_bitmap();
   int by = m->y0 + DLG_H - STATUS_H - BTN_H - 6;
   int ok_x = m->x0 + DLG_W / 2 - BTN_W - 6, cancel_x = m->x0 + DLG_W / 2 + 6;
   char area[120], tileset[160];

   al_set_target_bitmap(glb_ds1edit.screen_buff);
   al_draw_filled_rectangle((float)m->x0, (float)m->y0, (float)(m->x0 + DLG_W), (float)(m->y0 + DLG_H),
                            al_map_rgba(10, 12, 16, 235));
   al_draw_rectangle((float)m->x0 + 0.5f, (float)m->y0 + 0.5f, (float)(m->x0 + DLG_W) - 0.5f,
                     (float)(m->y0 + DLG_H) - 0.5f, al_map_rgb(120, 160, 200), 1.0f);
   al_draw_filled_rectangle((float)m->x0, (float)m->y0, (float)(m->x0 + DLG_W), (float)(m->y0 + TITLE_H),
                            al_map_rgb(20, 40, 60));
   al_draw_text(a5_font, al_map_rgb(200, 220, 255), (float)(m->x0 + 8), (float)(m->y0 + 6), 0,
                m->d->is_clone ? "Clone Map" : "New Map");

   if (g->act > 0)
      snprintf(area, sizeof(area), "Act %d - %s", g->act, g->name);
   else
      snprintf(area, sizeof(area), "%s", g->name);
   tileset_label(m, tileset, sizeof(tileset));

   draw_field(m, F_NAME, "Name", m->name, 0);
   draw_field(m, F_AREA, "Area", area, 1);
   draw_field(m, F_TILESET, "Tileset", tileset, 1);
   if (!m->d->is_clone)
   {
      draw_field(m, F_WIDTH, "Width", m->w, 0);
      draw_field(m, F_HEIGHT, "Height", m->h, 0);
   }

   al_draw_filled_rectangle((float)ok_x, (float)by, (float)(ok_x + BTN_W), (float)(by + BTN_H),
                            hover_ok ? al_map_rgb(60, 90, 130) : al_map_rgb(40, 60, 90));
   al_draw_text(a5_font, al_map_rgb(220, 230, 250), (float)(ok_x + BTN_W / 2), (float)(by + 7),
                ALLEGRO_ALIGN_CENTRE, "OK");
   al_draw_filled_rectangle((float)cancel_x, (float)by, (float)(cancel_x + BTN_W), (float)(by + BTN_H),
                            hover_cancel ? al_map_rgb(60, 90, 130) : al_map_rgb(40, 60, 90));
   al_draw_text(a5_font, al_map_rgb(220, 230, 250), (float)(cancel_x + BTN_W / 2), (float)(by + 7),
                ALLEGRO_ALIGN_CENTRE, "Cancel");

   al_draw_filled_rectangle((float)m->x0, (float)(m->y0 + DLG_H - STATUS_H),
                            (float)(m->x0 + DLG_W), (float)(m->y0 + DLG_H), al_map_rgb(20, 40, 60));
   al_draw_text(a5_font, m->status[0] ? al_map_rgb(255, 170, 120) : al_map_rgb(140, 160, 180),
                (float)(m->x0 + 8), (float)(m->y0 + DLG_H - STATUS_H + 5), 0,
                m->status[0] ? m->status : "[Tab] next field   [Left/Right] choose   [Enter] OK   [Esc] Cancel");

   if (prev != NULL)
      al_set_target_bitmap(prev);
}

static void handle_key(DLG * m, const ALLEGRO_EVENT * ev)
{
   int kc = ev->keyboard.keycode;
   unsigned int uch = ev->keyboard.unichar;
   int cap = 0, digits = 0;
   char * text = focused_text(m, &cap, &digits);

   m->status[0] = '\0';
   if (kc == ALLEGRO_KEY_TAB)
   {
      next_focus(m, (ev->keyboard.modifiers & ALLEGRO_KEYMOD_SHIFT) ? -1 : 1);
      return;
   }
   if (kc == ALLEGRO_KEY_LEFT || kc == ALLEGRO_KEY_RIGHT)
   {
      int dir = kc == ALLEGRO_KEY_LEFT ? -1 : 1;

      if (m->focus == F_AREA)
         cycle_area(m, dir);
      else if (m->focus == F_TILESET)
         cycle_tileset(m, dir);
      return;
   }
   if (text == NULL)
      return;
   if (kc == ALLEGRO_KEY_BACKSPACE)
   {
      size_t n = strlen(text);
      if (n > 0)
         text[n - 1] = '\0';
      return;
   }
   if (uch >= 32 && uch < 127 && (!digits || (uch >= '0' && uch <= '9')))
   {
      size_t n = strlen(text);
      if ((int) n + 1 < cap)
      {
         text[n] = (char) uch;
         text[n + 1] = '\0';
      }
   }
}

int map_dialog_show(MAP_DIALOG_S * d)
{
   DLG m;
   int done = 0, accepted = 0, prev_mb = 1;

   memset(&m, 0, sizeof(m));
   m.d = d;
   snprintf(m.name, sizeof(m.name), "%s", d->file_name);
   snprintf(m.w, sizeof(m.w), "%d", d->width);
   snprintf(m.h, sizeof(m.h), "%d", d->height);
   m.x0 = (glb_config.screen.width - DLG_W) / 2;
   m.y0 = (glb_config.screen.height - DLG_H) / 2;
   txt_ensure_lvlprest();
   load_defs(&m);

   input_suppress_held();
   input_pump();
   while (!done)
   {
      ALLEGRO_EVENT ev;
      int mx, my, mb, by, ok_x, cancel_x, hover_ok, hover_cancel, f;

      /* Key events go to the fields, so this loop reads them itself rather than
         through input_pump(), as text_input_modal does. */
      while (al_get_next_event(a5_event_queue, &ev))
      {
         if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
         {
            /* Cancel, and leave the close pending for the main loop's quit check. */
            a5_display_closed = 1;
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
               if (try_accept(&m))
               {
                  accepted = 1;
                  done = 1;
                  break;
               }
               continue;
            }
            handle_key(&m, &ev);
         }
      }

      al_get_mouse_state(&a5_ms_state);
      mx = a5_mouse_x;
      my = a5_mouse_y;
      mb = a5_mouse_b;
      by = m.y0 + DLG_H - STATUS_H - BTN_H - 6;
      ok_x = m.x0 + DLG_W / 2 - BTN_W - 6;
      cancel_x = m.x0 + DLG_W / 2 + 6;
      hover_ok = rect_hit(mx, my, ok_x, by, BTN_W, BTN_H);
      hover_cancel = rect_hit(mx, my, cancel_x, by, BTN_W, BTN_H);

      if (mb && !prev_mb)
      {
         if (hover_ok)
         {
            if (try_accept(&m))
            {
               accepted = 1;
               done = 1;
            }
         }
         else if (hover_cancel)
            done = 1;
         else
         {
            for (f = 0; f < FIELD_COUNT; f++)
            {
               if ((f == F_WIDTH || f == F_HEIGHT) && d->is_clone)
                  continue;
               if (rect_hit(mx, my, m.x0 + 12, m.y0 + TITLE_H + 8 + f * ROW_H, DLG_W - 24, 20))
               {
                  m.focus = f;
                  /* A click on the left or right half of a chooser steps it. */
                  if (f == F_AREA || f == F_TILESET)
                  {
                     int dir = mx < m.x0 + DLG_W / 2 ? -1 : 1;
                     if (f == F_AREA) cycle_area(&m, dir); else cycle_tileset(&m, dir);
                  }
               }
            }
         }
      }
      prev_mb = mb;

      draw_dialog(&m, hover_ok, hover_cancel);
      misc_draw_screen(mx, my);
      al_rest(0.016);
   }

   /* Keys and the click that closed this mustn't reach the map. */
   input_pump();
   while ((a5_mouse_b & 1) && !a5_display_closed)
   {
      al_rest(0.01);
      input_pump();
   }
   input_suppress_held();
   return accepted;
}
