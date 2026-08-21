// Modal "Open Preset" finder backed by the mpq_index.
//
// Drawn into glb_ds1edit.screen_buff and flipped via misc_draw_screen() so
// the modal sits over the last-rendered DS1 view. Self-contained event
// loop -- drains the main event queue directly, only the main render loop
// is paused while the modal is up.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "misc.h"
#include "ui/compat.h"
#include "ui/preset_picker.h"
#include "core/mpq_index.h"
#include "core/area_browser.h"
#include "ui/input.h"

#define FILTER_MAX       63
#define ROW_HEIGHT       13
#define TITLE_HEIGHT     18
#define FILTER_HEIGHT    20
#define STATUS_HEIGHT    18
#define BORDER           2
#define COLOR_PANEL_BG   al_map_rgba(10, 12, 16, 235)
#define COLOR_BORDER     al_map_rgb(120, 160, 200)
#define COLOR_TITLE_BG   al_map_rgb(20, 40, 60)
#define COLOR_TITLE_FG   al_map_rgb(200, 220, 255)
#define COLOR_FILTER_BG  al_map_rgb(15, 20, 30)
#define COLOR_FILTER_FG  al_map_rgb(220, 220, 220)
#define COLOR_PROMPT     al_map_rgb(120, 180, 100)
#define COLOR_ROW_FG     al_map_rgb(180, 200, 220)
#define COLOR_ROW_DIM    al_map_rgb(110, 120, 140)
#define COLOR_ROW_SEL_BG al_map_rgb(40, 70, 110)
#define COLOR_ROW_SEL_FG al_map_rgb(255, 255, 255)
#define COLOR_STATUS_FG  al_map_rgb(140, 160, 180)

typedef struct PICKER_S
{
   char  filter[FILTER_MAX + 1];
   int   filter_len;
   int  *filtered;           // indices into the global preset list
   int   filtered_count;
   int   filtered_cap;
   int   selected;           // index into filtered[]
   int   scroll;             // first visible filtered row
   int   x0, y0, x1, y1;     // modal rect
   int   list_y0, list_y1;   // list rect
   int   visible_rows;
} PICKER_S;

/* ---- filter matching ---- */

static int str_contains_ci(const char *hay, const char *needle)
{
   size_t nh, nn, i, j;
   if (hay == NULL || needle == NULL) return 0;
   if (needle[0] == 0) return 1;
   nh = strlen(hay);
   nn = strlen(needle);
   if (nn > nh) return 0;
   for (i = 0; i + nn <= nh; i++)
   {
      for (j = 0; j < nn; j++)
      {
         if (tolower((unsigned char) hay[i + j]) !=
             tolower((unsigned char) needle[j]))
            break;
      }
      if (j == nn) return 1;
   }
   return 0;
}

static int preset_matches(const PRESET_ENTRY_S *p, const char *needle)
{
   int f;
   if (str_contains_ci(p->name,      needle)) return 1;
   if (str_contains_ci(p->type_name, needle)) return 1;
   for (f = 0; f < p->ds1_count; f++)
      if (str_contains_ci(p->ds1_files[f], needle)) return 1;
   return 0;
}

static void rebuild_filtered(PICKER_S *s)
{
   int i, total;
   const PRESET_ENTRY_S *p;

   s->filtered_count = 0;
   total = mpq_index_preset_count();

   for (i = 0; i < total; i++)
   {
      p = mpq_index_preset_at(i);
      if (p == NULL) continue;
      if (!preset_matches(p, s->filter)) continue;
      if (s->filtered_count >= s->filtered_cap) break;
      s->filtered[s->filtered_count++] = i;
   }

   if (s->selected >= s->filtered_count) s->selected = s->filtered_count - 1;
   if (s->selected < 0)                  s->selected = 0;
   if (s->scroll > s->selected)          s->scroll = s->selected;
}

/* ---- layout + drawing ---- */

static void compute_layout(PICKER_S *s)
{
   int w = glb_config.screen.width;
   int h = glb_config.screen.height;
   int mw, mh;

   mw = (w * 80) / 100;
   if (mw < 480) mw = 480;
   if (mw > 1100) mw = 1100;

   mh = (h * 70) / 100;
   if (mh < 360) mh = 360;

   s->x0 = (w - mw) / 2;
   s->y0 = (h - mh) / 2;
   s->x1 = s->x0 + mw;
   s->y1 = s->y0 + mh;

   s->list_y0 = s->y0 + TITLE_HEIGHT + FILTER_HEIGHT + BORDER;
   s->list_y1 = s->y1 - STATUS_HEIGHT - BORDER;
   s->visible_rows = (s->list_y1 - s->list_y0) / ROW_HEIGHT;
   if (s->visible_rows < 1) s->visible_rows = 1;
}

static void draw_modal(PICKER_S *s)
{
   ALLEGRO_BITMAP *prev;
   int i, row_i, y;
   int lh = al_get_font_line_height(a5_font);
   char tmp[256];

   prev = al_get_target_bitmap();
   al_set_target_bitmap(glb_ds1edit.screen_buff);

   /* Panel + border */
   al_draw_filled_rectangle((float) s->x0, (float) s->y0,
                            (float) s->x1, (float) s->y1, COLOR_PANEL_BG);
   al_draw_rectangle((float) s->x0 + 0.5f, (float) s->y0 + 0.5f,
                     (float) s->x1 - 0.5f, (float) s->y1 - 0.5f,
                     COLOR_BORDER, 1.0f);

   /* Title bar */
   al_draw_filled_rectangle((float) s->x0,          (float) s->y0,
                            (float) s->x1,          (float) (s->y0 + TITLE_HEIGHT),
                            COLOR_TITLE_BG);
   al_draw_text(a5_font, COLOR_TITLE_FG,
                (float) (s->x0 + 8), (float) (s->y0 + (TITLE_HEIGHT - lh) / 2),
                0, "Open Preset");

   /* Filter bar */
   {
      int fy0 = s->y0 + TITLE_HEIGHT;
      int fy1 = fy0 + FILTER_HEIGHT;
      al_draw_filled_rectangle((float) s->x0, (float) fy0,
                               (float) s->x1, (float) fy1,
                               COLOR_FILTER_BG);
      al_draw_textf(a5_font, COLOR_PROMPT,
                    (float) (s->x0 + 8), (float) (fy0 + (FILTER_HEIGHT - lh) / 2),
                    0, "Filter:");
      al_draw_textf(a5_font, COLOR_FILTER_FG,
                    (float) (s->x0 + 8 + 56),
                    (float) (fy0 + (FILTER_HEIGHT - lh) / 2),
                    0, "%s_", s->filter);
   }

   /* Rows */
   for (row_i = 0; row_i < s->visible_rows; row_i++)
   {
      int filt_idx = s->scroll + row_i;
      const PRESET_ENTRY_S *p;
      int selected_row;

      if (filt_idx >= s->filtered_count) break;

      p = mpq_index_preset_at(s->filtered[filt_idx]);
      if (p == NULL) continue;

      y = s->list_y0 + row_i * ROW_HEIGHT;
      selected_row = (filt_idx == s->selected);

      if (selected_row)
      {
         al_draw_filled_rectangle((float) (s->x0 + BORDER), (float) y,
                                  (float) (s->x1 - BORDER),
                                  (float) (y + ROW_HEIGHT),
                                  COLOR_ROW_SEL_BG);
      }

      /* Columns: name (left) | type (mid) | path (right).
       * Widths are proportional to the modal width; text truncates on
       * its own when the format limits are exceeded. */
      snprintf(tmp, sizeof(tmp), "%-36.36s", p->name[0] ? p->name : "<unnamed>");
      al_draw_text(a5_font, selected_row ? COLOR_ROW_SEL_FG : COLOR_ROW_FG,
                   (float) (s->x0 + 10),
                   (float) (y + (ROW_HEIGHT - lh) / 2),
                   0, tmp);

      snprintf(tmp, sizeof(tmp), "%-24.24s",
               p->type_name[0] ? p->type_name : "");
      al_draw_text(a5_font, selected_row ? COLOR_ROW_SEL_FG : COLOR_ROW_DIM,
                   (float) (s->x0 + 10 + 36 * 8),
                   (float) (y + (ROW_HEIGHT - lh) / 2),
                   0, tmp);

      {
         const char *path = p->ds1_count > 0 ? p->ds1_files[0] : "";
         al_draw_textf(a5_font, selected_row ? COLOR_ROW_SEL_FG : COLOR_ROW_DIM,
                       (float) (s->x0 + 10 + 36 * 8 + 24 * 8),
                       (float) (y + (ROW_HEIGHT - lh) / 2),
                       0, "%.40s", path);
      }
   }

   /* Status bar */
   {
      int sy = s->y1 - STATUS_HEIGHT;
      al_draw_filled_rectangle((float) s->x0, (float) sy,
                               (float) s->x1, (float) s->y1,
                               COLOR_TITLE_BG);
      al_draw_textf(a5_font, COLOR_STATUS_FG,
                    (float) (s->x0 + 8), (float) (sy + (STATUS_HEIGHT - lh) / 2),
                    0,
                    "%d / %d    [Up/Down] select    [Enter] open    [Esc] cancel",
                    s->filtered_count, mpq_index_preset_count());
   }

   if (prev) al_set_target_bitmap(prev);
}

/* ---- selection helpers ---- */

static void ensure_visible(PICKER_S *s)
{
   if (s->selected < s->scroll) s->scroll = s->selected;
   if (s->selected >= s->scroll + s->visible_rows)
      s->scroll = s->selected - s->visible_rows + 1;
   if (s->scroll < 0) s->scroll = 0;
}

static void move_selected(PICKER_S *s, int delta)
{
   if (s->filtered_count == 0) return;
   s->selected += delta;
   if (s->selected < 0) s->selected = 0;
   if (s->selected >= s->filtered_count) s->selected = s->filtered_count - 1;
   ensure_visible(s);
}

/* ---- open the chosen preset via the area browser ---- */

static void open_chosen(int preset_idx)
{
   const PRESET_ENTRY_S *p = mpq_index_preset_at(preset_idx);
   int rc;

   if (p == NULL || p->ds1_count == 0) return;

   rc = area_browser_open_by_file(p->ds1_files[0]);
   if (rc != 0)
   {
      fprintf(stderr,
         "preset_picker: area_browser_open_by_file(%s) failed (rc=%d)\n",
         p->ds1_files[0], rc);
   }
   else
   {
      fprintf(stderr,
         "preset_picker: opened preset \"%s\" (def=%d, type=%d) via %s\n",
         p->name, p->def, p->level_type, p->ds1_files[0]);
   }
}

/* ---- modal event loop ---- */

static int run_modal(PICKER_S *s)
{
   int done = 0;
   int result = -1;
   int mx, my, mb, prev_mb = 1; // start with "pressed" so first frame doesn't double-click
   double last_click_time = 0;
   int    last_click_row  = -1;

   /* Flush any pending input so the trigger keys don't leak in. */
   al_flush_event_queue(a5_event_queue);

   while (!done)
   {
      ALLEGRO_EVENT ev;

      compute_layout(s);

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
            int uc = ev.keyboard.unichar;

            if (kc == ALLEGRO_KEY_ESCAPE)            { done = 1; break; }
            if (kc == ALLEGRO_KEY_ENTER ||
                kc == ALLEGRO_KEY_PAD_ENTER)
            {
               if (s->filtered_count > 0)
                  result = s->filtered[s->selected];
               done = 1; break;
            }
            if (kc == ALLEGRO_KEY_UP)      { move_selected(s, -1); continue; }
            if (kc == ALLEGRO_KEY_DOWN)    { move_selected(s, +1); continue; }
            if (kc == ALLEGRO_KEY_PGUP)    { move_selected(s, -s->visible_rows); continue; }
            if (kc == ALLEGRO_KEY_PGDN)    { move_selected(s, +s->visible_rows); continue; }
            if (kc == ALLEGRO_KEY_HOME)    { s->selected = 0; ensure_visible(s); continue; }
            if (kc == ALLEGRO_KEY_END)     { s->selected = s->filtered_count - 1; ensure_visible(s); continue; }
            if (kc == ALLEGRO_KEY_BACKSPACE)
            {
               if (s->filter_len > 0)
               {
                  s->filter[--s->filter_len] = 0;
                  rebuild_filtered(s);
                  ensure_visible(s);
               }
               continue;
            }
            if (uc >= 32 && uc < 127 && s->filter_len < FILTER_MAX)
            {
               s->filter[s->filter_len++] = (char) uc;
               s->filter[s->filter_len]   = 0;
               s->selected = 0;
               s->scroll   = 0;
               rebuild_filtered(s);
            }
         }
         /* Ignore timer events during the modal -- tick / fps counters
          * freeze cleanly until the modal closes. */
      }

      /* Mouse: click on a row selects; double-click opens. */
      al_get_mouse_state(&a5_ms_state);
      mx = a5_mouse_x; my = a5_mouse_y; mb = a5_mouse_b;

      if (mb && !prev_mb &&
          mx >= s->x0 && mx <= s->x1 &&
          my >= s->list_y0 && my < s->list_y1)
      {
         int row = (my - s->list_y0) / ROW_HEIGHT;
         int filt_idx = s->scroll + row;
         double now = al_get_time();

         if (filt_idx >= 0 && filt_idx < s->filtered_count)
         {
            s->selected = filt_idx;
            ensure_visible(s);

            if (last_click_row == filt_idx && (now - last_click_time) < 0.35)
            {
               result = s->filtered[filt_idx];
               done = 1;
            }
            last_click_row  = filt_idx;
            last_click_time = now;
         }
      }
      prev_mb = mb;

      /* Render: overlay on the last DS1 frame that was in screen_buff. */
      draw_modal(s);
      misc_draw_screen(mx, my);

      al_rest(0.016);
   }

   return result;
}

/* ---- public entry ---- */

void preset_picker_handle_shortcut(void)
{
   int ctrl, shift;
   PICKER_S s;
   int preset_idx;

   ctrl  = key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL);
   shift = key_pressed(KEY_LSHIFT)   || key_pressed(KEY_RSHIFT);

   if (!ctrl || !shift || !key_hit(KEY_P)) return;
   if (!mpq_index_is_ready()) return;

   /* debounce */

   memset(&s, 0, sizeof(s));
   s.filtered_cap = mpq_index_preset_count();
   if (s.filtered_cap <= 0) return;
   s.filtered = (int *) malloc(s.filtered_cap * sizeof(int));
   if (s.filtered == NULL) return;

   rebuild_filtered(&s);

   preset_idx = run_modal(&s);

   free(s.filtered);

   if (preset_idx >= 0)
      open_chosen(preset_idx);
}
