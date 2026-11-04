#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "misc.h"
#include "ui/compat.h"
#include "ui/input.h"
#include "ui/fields_dialog.h"

#define DLG_W        560
#define TITLE_H      20
#define ROW_H        26
#define LABEL_W      140
#define BTN_W        64
#define BTN_H        22
#define STATUS_LINES 3
#define STATUS_H     (STATUS_LINES * 12 + 6)
#define TEXT_CAP     520
#define HINT         "[Tab] next  [Enter] OK  [Esc] Cancel"

typedef struct DLG
{
   const char * title;
   FIELD_S * f;
   int  count;
   char work[FIELDS_DIALOG_MAX][TEXT_CAP];   /* what's being edited */
   char orig[FIELDS_DIALOG_MAX][TEXT_CAP];   /* the defaults, for cancel */
   int  cap[FIELDS_DIALOG_MAX];
   FIELDS_CHECK_CB check;
   void * ctx;
   int  focus;
   char status[256];
   int  x0, y0, h;
} DLG;

static int rect_hit(int mx, int my, int x, int y, int w, int h)
{
   return mx >= x && mx < x + w && my >= y && my < y + h;
}

static int buttons_y(const DLG * m)
{
   return m->y0 + m->h - STATUS_H - BTN_H - 6;
}

static int field_y(const DLG * m, int field)
{
   return m->y0 + TITLE_H + 8 + field * ROW_H;
}

/* Put the working values in the caller's fields, or the defaults back. */
static void store(DLG * m, int use_defaults)
{
   int i;

   for (i = 0; i < m->count; i++)
      snprintf(m->f[i].value, (size_t) m->f[i].cap, "%s",
               use_defaults ? m->orig[i] : m->work[i]);
}

/* 1 if the caller's check accepts these values; otherwise its reason is in the
   status row. */
static int try_accept(DLG * m)
{
   m->status[0] = '\0';
   store(m, 0);
   if (m->check == NULL)
      return 1;
   if (m->check(m->status, (int) sizeof(m->status), m->ctx) != 0)
   {
      if (m->status[0] == '\0')
         snprintf(m->status, sizeof(m->status), "These values can't be used.");
      return 0;
   }
   return 1;
}

static void draw_field(const DLG * m, int field)
{
   int y = field_y(m, field);
   int x = m->x0 + 12 + LABEL_W;
   int w = DLG_W - 24 - LABEL_W;
   int focused = m->focus == field;
   char shown[TEXT_CAP + 2];
   const char * p;

   /* A value too long for the field shows its end, where the typing is. */
   snprintf(shown, sizeof(shown), "%s%s", m->work[field], focused ? "_" : "");
   p = shown;
   while (*p != '\0' && al_get_text_width(a5_font, p) > w - 12)
      p++;

   al_draw_text(a5_font, al_map_rgb(180, 200, 220), (float)(m->x0 + 12), (float)(y + 6), 0,
                m->f[field].label);
   al_draw_filled_rectangle((float)x, (float)y, (float)(x + w), (float)(y + 20),
                            al_map_rgb(30, 32, 40));
   al_draw_rectangle((float)x + 0.5f, (float)y + 0.5f, (float)(x + w) - 0.5f, (float)(y + 20) - 0.5f,
                     focused ? al_map_rgb(255, 230, 80) : al_map_rgb(80, 100, 130), 1.0f);
   al_draw_text(a5_font, al_map_rgb(255, 255, 255), (float)(x + 6), (float)(y + 6), 0, p);
}

/* The status, broken at spaces into the lines the row has room for. */
static void draw_status(const DLG * m)
{
   int y = m->y0 + m->h - STATUS_H;
   int max_w = DLG_W - 16;
   const char * p = m->status[0] ? m->status : HINT;
   ALLEGRO_COLOR col = m->status[0] ? al_map_rgb(255, 170, 120) : al_map_rgb(140, 160, 180);
   int line;

   al_draw_filled_rectangle((float)m->x0, (float)y, (float)(m->x0 + DLG_W),
                            (float)(m->y0 + m->h), al_map_rgb(20, 40, 60));
   for (line = 0; line < STATUS_LINES && *p != '\0'; line++)
   {
      char buf[256];
      int n = 0, last_space = -1, len = (int) strlen(p);

      while (n < len && n < (int) sizeof(buf) - 1)
      {
         memcpy(buf, p, (size_t) n + 1);
         buf[n + 1] = '\0';
         if (al_get_text_width(a5_font, buf) > max_w)
            break;
         if (p[n] == ' ')
            last_space = n;
         n++;
      }
      if (n < len && last_space > 0 && line < STATUS_LINES - 1)
         n = last_space;
      if (n == 0)
         n = 1;
      memcpy(buf, p, (size_t) n);
      buf[n] = '\0';
      al_draw_text(a5_font, col, (float)(m->x0 + 8), (float)(y + 4 + line * 12), 0, buf);
      p += n;
      while (*p == ' ')
         p++;
   }
}

static void draw_dialog(const DLG * m, int hover_ok, int hover_cancel)
{
   ALLEGRO_BITMAP * prev = al_get_target_bitmap();
   int by = buttons_y(m), i;
   int ok_x = m->x0 + DLG_W / 2 - BTN_W - 6, cancel_x = m->x0 + DLG_W / 2 + 6;

   al_set_target_bitmap(glb_ds1edit.screen_buff);
   al_draw_filled_rectangle((float)m->x0, (float)m->y0, (float)(m->x0 + DLG_W), (float)(m->y0 + m->h),
                            al_map_rgba(10, 12, 16, 235));
   al_draw_rectangle((float)m->x0 + 0.5f, (float)m->y0 + 0.5f, (float)(m->x0 + DLG_W) - 0.5f,
                     (float)(m->y0 + m->h) - 0.5f, al_map_rgb(120, 160, 200), 1.0f);
   al_draw_filled_rectangle((float)m->x0, (float)m->y0, (float)(m->x0 + DLG_W), (float)(m->y0 + TITLE_H),
                            al_map_rgb(20, 40, 60));
   al_draw_text(a5_font, al_map_rgb(200, 220, 255), (float)(m->x0 + 8), (float)(m->y0 + 6), 0,
                m->title);

   for (i = 0; i < m->count; i++)
      draw_field(m, i);

   al_draw_filled_rectangle((float)ok_x, (float)by, (float)(ok_x + BTN_W), (float)(by + BTN_H),
                            hover_ok ? al_map_rgb(60, 90, 130) : al_map_rgb(40, 60, 90));
   al_draw_text(a5_font, al_map_rgb(220, 230, 250), (float)(ok_x + BTN_W / 2), (float)(by + 7),
                ALLEGRO_ALIGN_CENTRE, "OK");
   al_draw_filled_rectangle((float)cancel_x, (float)by, (float)(cancel_x + BTN_W), (float)(by + BTN_H),
                            hover_cancel ? al_map_rgb(60, 90, 130) : al_map_rgb(40, 60, 90));
   al_draw_text(a5_font, al_map_rgb(220, 230, 250), (float)(cancel_x + BTN_W / 2), (float)(by + 7),
                ALLEGRO_ALIGN_CENTRE, "Cancel");

   draw_status(m);

   if (prev != NULL)
      al_set_target_bitmap(prev);
}

static void handle_key(DLG * m, const ALLEGRO_EVENT * ev)
{
   int kc = ev->keyboard.keycode;
   unsigned int uch = ev->keyboard.unichar;
   char * text = m->work[m->focus];

   m->status[0] = '\0';
   if (kc == ALLEGRO_KEY_TAB || kc == ALLEGRO_KEY_UP || kc == ALLEGRO_KEY_DOWN)
   {
      int back = kc == ALLEGRO_KEY_UP ||
                 (kc == ALLEGRO_KEY_TAB && (ev->keyboard.modifiers & ALLEGRO_KEYMOD_SHIFT));

      m->focus = (m->focus + (back ? -1 : 1) + m->count) % m->count;
      return;
   }
   if (kc == ALLEGRO_KEY_BACKSPACE)
   {
      size_t n = strlen(text);
      if (n > 0)
         text[n - 1] = '\0';
      return;
   }
   if (uch >= 32 && uch < 127)
   {
      size_t n = strlen(text);
      if ((int) n + 1 < m->cap[m->focus])
      {
         text[n] = (char) uch;
         text[n + 1] = '\0';
      }
   }
}

int fields_dialog_show(const char * title, FIELD_S * fields, int count,
                       FIELDS_CHECK_CB check, void * ctx)
{
   DLG m;
   int i, done = 0, accepted = 0, prev_mb = 1;

   if (count < 1 || count > FIELDS_DIALOG_MAX)
      return 0;
   memset(&m, 0, sizeof(m));
   m.title = title;
   m.f = fields;
   m.count = count;
   m.check = check;
   m.ctx = ctx;
   m.h = TITLE_H + 8 + count * ROW_H + 6 + BTN_H + 6 + STATUS_H;
   for (i = 0; i < count; i++)
   {
      m.cap[i] = fields[i].cap < TEXT_CAP ? fields[i].cap : TEXT_CAP;
      snprintf(m.orig[i], sizeof(m.orig[i]), "%s", fields[i].value);
      snprintf(m.work[i], (size_t) m.cap[i], "%s", fields[i].value);
   }
   m.x0 = (glb_config.screen.width - DLG_W) / 2;
   m.y0 = (glb_config.screen.height - m.h) / 2;

   input_suppress_held();
   input_pump();
   while (!done && !a5_display_closed)
   {
      ALLEGRO_EVENT ev;
      int mx, my, mb, by, ok_x, cancel_x, hover_ok, hover_cancel;

      /* Key events go to the fields, so this loop reads them itself rather than
         through input_pump(), as map_dialog does. */
      while (al_get_next_event(a5_event_queue, &ev))
      {
         if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
         {
            /* Cancel, and leave the close pending for the main loop's quit check. */
            a5_display_closed = 1;
            done = 1;
            break;
         }
         if (ev.type != ALLEGRO_EVENT_KEY_CHAR)
         {
            input_note_event(&ev);
            continue;
         }
         if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
         {
            done = 1;
            break;
         }
         if (ev.keyboard.keycode == ALLEGRO_KEY_ENTER || ev.keyboard.keycode == ALLEGRO_KEY_PAD_ENTER)
         {
            /* The Enter that opened this, still held, mustn't accept it. */
            if (ev.keyboard.repeat)
               continue;
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

      /* A resize moves the screen's centre. */
      m.x0 = (glb_config.screen.width - DLG_W) / 2;
      m.y0 = (glb_config.screen.height - m.h) / 2;

      al_get_mouse_state(&a5_ms_state);
      mx = a5_mouse_x;
      my = a5_mouse_y;
      mb = a5_mouse_b & 1;
      by = buttons_y(&m);
      ok_x = m.x0 + DLG_W / 2 - BTN_W - 6;
      cancel_x = m.x0 + DLG_W / 2 + 6;
      hover_ok = rect_hit(mx, my, ok_x, by, BTN_W, BTN_H);
      hover_cancel = rect_hit(mx, my, cancel_x, by, BTN_W, BTN_H);

      if (!done && mb && !prev_mb)
      {
         m.status[0] = '\0';
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
            for (i = 0; i < m.count; i++)
               if (rect_hit(mx, my, m.x0 + 12, field_y(&m, i), DLG_W - 24, 20))
                  m.focus = i;
         }
      }
      prev_mb = mb;

      draw_dialog(&m, hover_ok, hover_cancel);
      misc_draw_screen(mx, my);
      al_rest(0.016);
   }

   if (!accepted)
      store(&m, 1);

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
