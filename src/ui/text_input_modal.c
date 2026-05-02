#include <stdio.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "misc.h"
#include "ui/text_input_modal.h"

#define MODAL_WIDTH        480
#define MODAL_HEIGHT       150
#define BORDER             2
#define TITLE_HEIGHT       20
#define STATUS_HEIGHT      18
#define INPUT_HEIGHT       24
#define BUTTON_HEIGHT      22
#define BUTTON_WIDTH       64
#define INPUT_PADDING_X    8

#define COLOR_PANEL_BG     al_map_rgba(10, 12, 16, 235)
#define COLOR_BORDER       al_map_rgb(120, 160, 200)
#define COLOR_TITLE_BG     al_map_rgb(20, 40, 60)
#define COLOR_TITLE_FG     al_map_rgb(200, 220, 255)
#define COLOR_PROMPT_FG    al_map_rgb(180, 200, 220)
#define COLOR_INPUT_BG     al_map_rgb(30, 32, 40)
#define COLOR_INPUT_BORDER al_map_rgb(80, 100, 130)
#define COLOR_INPUT_FG     al_map_rgb(255, 255, 255)
#define COLOR_CURSOR       al_map_rgb(255, 230, 80)
#define COLOR_BUTTON_BG    al_map_rgb(40, 60, 90)
#define COLOR_BUTTON_HOVER al_map_rgb(60, 90, 130)
#define COLOR_BUTTON_FG    al_map_rgb(220, 230, 250)
#define COLOR_STATUS_FG    al_map_rgb(140, 160, 180)

typedef struct TEXT_INPUT_S
{
   char buffer[1024];
   int  length;
   int  cursor;
   int  capacity;

   const char *title;
   const char *prompt;

   int x0, y0, x1, y1;
   int input_x0, input_y0, input_x1, input_y1;
   int ok_x0, ok_y0, ok_x1, ok_y1;
   int cancel_x0, cancel_y0, cancel_x1, cancel_y1;
} TEXT_INPUT_S;

static int point_in_rect(int mx, int my, int x0, int y0, int x1, int y1)
{
   return mx >= x0 && mx < x1 && my >= y0 && my < y1;
}

static void compute_layout(TEXT_INPUT_S *m)
{
   int input_y;
   int button_y;
   int center_x;
   int spacing = 12;

   m->x0 = (glb_config.screen.width - MODAL_WIDTH) / 2;
   m->y0 = (glb_config.screen.height - MODAL_HEIGHT) / 2;
   m->x1 = m->x0 + MODAL_WIDTH;
   m->y1 = m->y0 + MODAL_HEIGHT;

   input_y = m->y0 + TITLE_HEIGHT + 28;
   m->input_x0 = m->x0 + 16;
   m->input_y0 = input_y;
   m->input_x1 = m->x1 - 16;
   m->input_y1 = input_y + INPUT_HEIGHT;

   button_y = m->input_y1 + 18;
   center_x = (m->x0 + m->x1) / 2;

   m->ok_x0 = center_x - BUTTON_WIDTH - spacing / 2;
   m->ok_y0 = button_y;
   m->ok_x1 = m->ok_x0 + BUTTON_WIDTH;
   m->ok_y1 = button_y + BUTTON_HEIGHT;

   m->cancel_x0 = center_x + spacing / 2;
   m->cancel_y0 = button_y;
   m->cancel_x1 = m->cancel_x0 + BUTTON_WIDTH;
   m->cancel_y1 = button_y + BUTTON_HEIGHT;
}

static void insert_char(TEXT_INPUT_S *m, char ch)
{
   if (m->length + 1 >= m->capacity)
      return;
   if (m->cursor < m->length)
      memmove(m->buffer + m->cursor + 1,
              m->buffer + m->cursor,
              m->length - m->cursor);
   m->buffer[m->cursor] = ch;
   m->cursor++;
   m->length++;
   m->buffer[m->length] = 0;
}

static void delete_at_cursor(TEXT_INPUT_S *m, int forward)
{
   int idx;
   if (forward)
   {
      if (m->cursor >= m->length)
         return;
      idx = m->cursor;
   }
   else
   {
      if (m->cursor <= 0)
         return;
      idx = m->cursor - 1;
   }

   memmove(m->buffer + idx,
           m->buffer + idx + 1,
           m->length - idx - 1);
   m->length--;
   m->buffer[m->length] = 0;
   if (!forward)
      m->cursor--;
}

static void paste_clipboard(TEXT_INPUT_S *m)
{
   ALLEGRO_DISPLAY *disp = al_get_current_display();
   char *text;

   if (disp == NULL)
      return;

   text = al_get_clipboard_text(disp);
   if (text == NULL)
      return;

   {
      const char *p = text;
      while (*p != 0)
      {
         /* Filter to printable ASCII only; reject newlines / control */
         unsigned char c = (unsigned char) *p;
         if (c >= 32 && c < 127)
            insert_char(m, (char) c);
         p++;
      }
   }
   al_free(text);
}

static void draw_modal(const TEXT_INPUT_S *m, int show_cursor,
                       int hover_ok, int hover_cancel)
{
   ALLEGRO_BITMAP *prev;
   int line_h = al_get_font_line_height(a5_font);

   prev = al_get_target_bitmap();
   al_set_target_bitmap(glb_ds1edit.screen_buff);

   /* panel + border */
   al_draw_filled_rectangle((float) m->x0, (float) m->y0,
                            (float) m->x1, (float) m->y1,
                            COLOR_PANEL_BG);
   al_draw_rectangle((float) m->x0 + 0.5f, (float) m->y0 + 0.5f,
                     (float) m->x1 - 0.5f, (float) m->y1 - 0.5f,
                     COLOR_BORDER, 1.0f);

   /* title bar */
   al_draw_filled_rectangle((float) m->x0, (float) m->y0,
                            (float) m->x1, (float) (m->y0 + TITLE_HEIGHT),
                            COLOR_TITLE_BG);
   al_draw_text(a5_font, COLOR_TITLE_FG,
                (float) (m->x0 + 8),
                (float) (m->y0 + (TITLE_HEIGHT - line_h) / 2),
                0,
                m->title != NULL ? m->title : "Enter text");

   /* prompt */
   if (m->prompt != NULL)
   {
      al_draw_text(a5_font, COLOR_PROMPT_FG,
                   (float) (m->x0 + 16),
                   (float) (m->y0 + TITLE_HEIGHT + 6),
                   0,
                   m->prompt);
   }

   /* input field */
   al_draw_filled_rectangle((float) m->input_x0, (float) m->input_y0,
                            (float) m->input_x1, (float) m->input_y1,
                            COLOR_INPUT_BG);
   al_draw_rectangle((float) m->input_x0 + 0.5f, (float) m->input_y0 + 0.5f,
                     (float) m->input_x1 - 0.5f, (float) m->input_y1 - 0.5f,
                     COLOR_INPUT_BORDER, 1.0f);

   {
      int text_y = m->input_y0 + (INPUT_HEIGHT - line_h) / 2;
      int text_x = m->input_x0 + INPUT_PADDING_X;
      al_draw_text(a5_font, COLOR_INPUT_FG,
                   (float) text_x, (float) text_y, 0, m->buffer);

      if (show_cursor)
      {
         char left[1024];
         int cursor_px;
         int copy_len = m->cursor;
         if (copy_len >= (int) sizeof(left))
            copy_len = (int) sizeof(left) - 1;
         memcpy(left, m->buffer, copy_len);
         left[copy_len] = 0;
         cursor_px = al_get_text_width(a5_font, left);
         al_draw_line((float) (text_x + cursor_px), (float) (text_y - 1),
                      (float) (text_x + cursor_px), (float) (text_y + line_h + 1),
                      COLOR_CURSOR, 1.0f);
      }
   }

   /* OK button */
   al_draw_filled_rectangle((float) m->ok_x0, (float) m->ok_y0,
                            (float) m->ok_x1, (float) m->ok_y1,
                            hover_ok ? COLOR_BUTTON_HOVER : COLOR_BUTTON_BG);
   al_draw_rectangle((float) m->ok_x0 + 0.5f, (float) m->ok_y0 + 0.5f,
                     (float) m->ok_x1 - 0.5f, (float) m->ok_y1 - 0.5f,
                     COLOR_BORDER, 1.0f);
   {
      const char *label = "OK";
      int tw = al_get_text_width(a5_font, label);
      int tx = m->ok_x0 + ((m->ok_x1 - m->ok_x0) - tw) / 2;
      int ty = m->ok_y0 + (BUTTON_HEIGHT - line_h) / 2;
      al_draw_text(a5_font, COLOR_BUTTON_FG, (float) tx, (float) ty, 0, label);
   }

   /* Cancel button */
   al_draw_filled_rectangle((float) m->cancel_x0, (float) m->cancel_y0,
                            (float) m->cancel_x1, (float) m->cancel_y1,
                            hover_cancel ? COLOR_BUTTON_HOVER : COLOR_BUTTON_BG);
   al_draw_rectangle((float) m->cancel_x0 + 0.5f, (float) m->cancel_y0 + 0.5f,
                     (float) m->cancel_x1 - 0.5f, (float) m->cancel_y1 - 0.5f,
                     COLOR_BORDER, 1.0f);
   {
      const char *label = "Cancel";
      int tw = al_get_text_width(a5_font, label);
      int tx = m->cancel_x0 + ((m->cancel_x1 - m->cancel_x0) - tw) / 2;
      int ty = m->cancel_y0 + (BUTTON_HEIGHT - line_h) / 2;
      al_draw_text(a5_font, COLOR_BUTTON_FG, (float) tx, (float) ty, 0, label);
   }

   /* status hint row */
   al_draw_filled_rectangle((float) m->x0,
                            (float) (m->y1 - STATUS_HEIGHT),
                            (float) m->x1,
                            (float) m->y1,
                            COLOR_TITLE_BG);
   al_draw_text(a5_font, COLOR_STATUS_FG,
                (float) (m->x0 + 8),
                (float) (m->y1 - STATUS_HEIGHT + (STATUS_HEIGHT - line_h) / 2),
                0,
                "[Enter] OK   [Esc] Cancel   [Ctrl+V] paste");

   if (prev != NULL)
      al_set_target_bitmap(prev);
}

int text_input_modal_show(const char *title, const char *prompt,
                          const char *default_text,
                          char *out_buf, int out_cap)
{
   TEXT_INPUT_S m;
   int done = 0;
   int result_ok = 0;
   int prev_mb = 1;
   double cursor_t = 0.0;
   int cap;

   if (out_buf == NULL || out_cap <= 0)
      return 0;

   memset(&m, 0, sizeof(m));
   m.title = title;
   m.prompt = prompt;

   cap = (int) sizeof(m.buffer);
   if (out_cap < cap)
      cap = out_cap;
   m.capacity = cap;

   if (default_text != NULL)
   {
      int n = (int) strlen(default_text);
      if (n >= cap)
         n = cap - 1;
      memcpy(m.buffer, default_text, n);
      m.buffer[n] = 0;
      m.length = n;
      m.cursor = n;
   }

   compute_layout(&m);
   al_flush_event_queue(a5_event_queue);

   while (!done)
   {
      ALLEGRO_EVENT ev;
      int mx, my, mb;
      int hover_ok, hover_cancel;
      int show_cursor;

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
            unsigned int uch = ev.keyboard.unichar;
            int mods = ev.keyboard.modifiers;

            if (kc == ALLEGRO_KEY_ESCAPE)
            {
               done = 1;
               break;
            }
            if (kc == ALLEGRO_KEY_ENTER || kc == ALLEGRO_KEY_PAD_ENTER)
            {
               result_ok = (m.length > 0);
               done = 1;
               break;
            }
            if (kc == ALLEGRO_KEY_BACKSPACE)
            {
               delete_at_cursor(&m, 0);
               continue;
            }
            if (kc == ALLEGRO_KEY_DELETE)
            {
               delete_at_cursor(&m, 1);
               continue;
            }
            if (kc == ALLEGRO_KEY_LEFT)
            {
               if (m.cursor > 0)
                  m.cursor--;
               continue;
            }
            if (kc == ALLEGRO_KEY_RIGHT)
            {
               if (m.cursor < m.length)
                  m.cursor++;
               continue;
            }
            if (kc == ALLEGRO_KEY_HOME)
            {
               m.cursor = 0;
               continue;
            }
            if (kc == ALLEGRO_KEY_END)
            {
               m.cursor = m.length;
               continue;
            }
            if (kc == ALLEGRO_KEY_V && (mods & ALLEGRO_KEYMOD_CTRL))
            {
               paste_clipboard(&m);
               continue;
            }

            /* Printable ASCII only (paths in this app are ASCII). */
            if (uch >= 32 && uch < 127)
               insert_char(&m, (char) uch);
         }
      }

      al_get_mouse_state(&a5_ms_state);
      mx = a5_mouse_x;
      my = a5_mouse_y;
      mb = a5_mouse_b;

      hover_ok = point_in_rect(mx, my, m.ok_x0, m.ok_y0, m.ok_x1, m.ok_y1);
      hover_cancel = point_in_rect(mx, my, m.cancel_x0, m.cancel_y0,
                                   m.cancel_x1, m.cancel_y1);

      if (mb && !prev_mb)
      {
         if (hover_ok)
         {
            result_ok = (m.length > 0);
            done = 1;
         }
         else if (hover_cancel)
         {
            done = 1;
         }
      }
      prev_mb = mb;

      cursor_t += 0.016;
      show_cursor = ((int) (cursor_t * 2.0)) & 1 ? 0 : 1;

      draw_modal(&m, show_cursor, hover_ok, hover_cancel);
      misc_draw_screen(mx, my);
      al_rest(0.016);
   }

   if (result_ok)
   {
      int n = m.length;
      if (n >= out_cap)
         n = out_cap - 1;
      memcpy(out_buf, m.buffer, n);
      out_buf[n] = 0;
      return 1;
   }
   return 0;
}
