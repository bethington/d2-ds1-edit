#include <stdio.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "misc.h"
#include "core/export_progress.h"

static EXPORT_PROGRESS_S s_state;
static int    s_show_remote_stages = 0;  /* set by begin/wire-up */
static double s_last_paint_time    = 0.0;

#define DIALOG_W              520
#define DIALOG_H              160
#define DIALOG_BORDER         2
#define DIALOG_TITLE_H        20
#define DIALOG_STATUS_H       18
#define DIALOG_BAR_H          18
#define DIALOG_BUTTON_H       22
#define DIALOG_BUTTON_W       72
#define DIALOG_PAINT_INTERVAL 0.05  /* throttle to ~20 Hz */

#define COLOR_PANEL_BG        al_map_rgba(10, 12, 16, 235)
#define COLOR_BORDER          al_map_rgb(120, 160, 200)
#define COLOR_TITLE_BG        al_map_rgb(20, 40, 60)
#define COLOR_TITLE_FG        al_map_rgb(200, 220, 255)
#define COLOR_LABEL_FG        al_map_rgb(200, 215, 230)
#define COLOR_DIM_FG          al_map_rgb(140, 160, 180)
#define COLOR_BAR_BG          al_map_rgb(30, 35, 45)
#define COLOR_BAR_FG          al_map_rgb(80, 160, 255)
#define COLOR_BAR_FG_CANCEL   al_map_rgb(180, 90, 90)
#define COLOR_BUTTON_BG       al_map_rgb(40, 60, 90)
#define COLOR_BUTTON_HOVER    al_map_rgb(60, 90, 130)
#define COLOR_BUTTON_FG       al_map_rgb(220, 230, 250)

// Tier-1 stage weights. Indexed by EXPORT_STAGE_E.
static const int s_stage_weights[] = {
   0,    /* NONE */
   5,    /* PREPARE */
   35,   /* NATIVE_EXPORT */
   35,   /* LOCAL_UPSCALE (replaces remote stages on local fallback) */
   10,   /* PACKAGE_UPLOAD */
   25,   /* REMOTE_PROCESSING */
   15,   /* DOWNLOAD */
   10    /* EXTRACT */
};

static int s_stage_count = (int) (sizeof(s_stage_weights) / sizeof(s_stage_weights[0]));

void export_progress_begin(const char *title)
{
   memset(&s_state, 0, sizeof(s_state));
   s_state.active = 1;
   s_state.stage = EXPORT_STAGE_NONE;
   s_state.result = EXPORT_RESULT_PENDING;
   if (title != NULL)
   {
      strncpy(s_state.title, title, sizeof(s_state.title) - 1);
      s_state.title[sizeof(s_state.title) - 1] = 0;
   }
   /* Force the very next pump call to repaint instead of being skipped
    * by the throttle window. */
   s_last_paint_time = 0.0;
   s_show_remote_stages = 0;
}

void export_progress_end(void)
{
   memset(&s_state, 0, sizeof(s_state));
}

int export_task_is_active(void)
{
   return s_state.active;
}

const EXPORT_PROGRESS_S *export_progress_state(void)
{
   return &s_state;
}

void export_progress_set_stage(EXPORT_STAGE_E stage,
                               const char *stage_label,
                               int items_total)
{
   if (!s_state.active)
      return;
   s_state.stage = stage;
   s_state.items_done = 0;
   s_state.items_total = items_total > 0 ? items_total : 0;
   s_state.current_item[0] = 0;
   if (stage_label != NULL)
   {
      strncpy(s_state.stage_label, stage_label,
              sizeof(s_state.stage_label) - 1);
      s_state.stage_label[sizeof(s_state.stage_label) - 1] = 0;
   }
   else
   {
      s_state.stage_label[0] = 0;
   }
   /* Stage transitions are rare; force the next pump to repaint
    * immediately rather than be skipped by the throttle window. */
   s_last_paint_time = 0.0;
}

void export_progress_advance(int delta)
{
   if (!s_state.active || delta <= 0)
      return;
   s_state.items_done += delta;
   if (s_state.items_total > 0 && s_state.items_done > s_state.items_total)
      s_state.items_done = s_state.items_total;
}

void export_progress_set_current_item(const char *path)
{
   if (!s_state.active)
      return;
   if (path == NULL)
   {
      s_state.current_item[0] = 0;
      return;
   }
   strncpy(s_state.current_item, path, sizeof(s_state.current_item) - 1);
   s_state.current_item[sizeof(s_state.current_item) - 1] = 0;
}

void export_progress_request_cancel(void)
{
   if (!s_state.active)
      return;
   s_state.cancel_requested = 1;
}

void export_progress_force_repaint(void)
{
   /* Bypass the next pump's throttle window. The pump itself does the
    * actual paint; this just forces it to happen on the very next call. */
   s_last_paint_time = 0.0;
   export_progress_pump();
}

int export_progress_cancel_requested(void)
{
   return s_state.cancel_requested;
}

static int stage_weight(EXPORT_STAGE_E stage)
{
   int idx = (int) stage;
   if (idx < 0 || idx >= s_stage_count)
      return 0;
   return s_stage_weights[idx];
}

int export_progress_percent(int include_remote_stages)
{
   /* Sum of stage weights for stages strictly before the current stage,
    * plus the within-stage fraction. Renormalize so the total equals 100. */
   int total_weight = stage_weight(EXPORT_STAGE_PREPARE);
   int passed = 0;
   int within = 0;
   EXPORT_STAGE_E s;

   total_weight += stage_weight(EXPORT_STAGE_NATIVE_EXPORT);

   if (include_remote_stages)
   {
      total_weight += stage_weight(EXPORT_STAGE_PACKAGE_UPLOAD);
      total_weight += stage_weight(EXPORT_STAGE_REMOTE_PROCESSING);
      total_weight += stage_weight(EXPORT_STAGE_DOWNLOAD);
      total_weight += stage_weight(EXPORT_STAGE_EXTRACT);
   }
   else
   {
      total_weight += stage_weight(EXPORT_STAGE_LOCAL_UPSCALE);
   }

   if (total_weight <= 0)
      return 0;

   /* Tally weights for stages that are fully done. */
   for (s = EXPORT_STAGE_PREPARE; s < s_state.stage; s++)
   {
      if (!include_remote_stages
          && (s == EXPORT_STAGE_PACKAGE_UPLOAD
              || s == EXPORT_STAGE_REMOTE_PROCESSING
              || s == EXPORT_STAGE_DOWNLOAD
              || s == EXPORT_STAGE_EXTRACT))
         continue;
      if (include_remote_stages && s == EXPORT_STAGE_LOCAL_UPSCALE)
         continue;
      passed += stage_weight(s);
   }

   /* Within-stage fraction. */
   {
      int sw = stage_weight(s_state.stage);
      if (s_state.items_total > 0)
         within = (sw * s_state.items_done) / s_state.items_total;
      else
         within = 0;
   }

   {
      int pct = ((passed + within) * 100) / total_weight;
      if (pct < 0)   pct = 0;
      if (pct > 100) pct = 100;
      return pct;
   }
}

/* Middle-truncate `src` so it fits within `cap` chars, ellipsizing the
 * middle. Useful for long virtual asset paths. */
static void middle_truncate(const char *src, char *out, int cap)
{
   int len;

   if (out == NULL || cap <= 0)
      return;
   if (src == NULL)
   {
      out[0] = 0;
      return;
   }

   len = (int) strlen(src);
   if (len < cap)
   {
      memcpy(out, src, len + 1);
      return;
   }
   if (cap < 8)
   {
      strncpy(out, src, cap - 1);
      out[cap - 1] = 0;
      return;
   }

   {
      int prefix = (cap - 4) / 2;
      int suffix = (cap - 4) - prefix;
      memcpy(out, src, prefix);
      out[prefix]     = '.';
      out[prefix + 1] = '.';
      out[prefix + 2] = '.';
      memcpy(out + prefix + 3, src + len - suffix, suffix);
      out[prefix + 3 + suffix] = 0;
   }
}

static void compute_button_rect(int x0, int y0, int *bx0, int *by0,
                                int *bx1, int *by1)
{
   int dialog_x1 = x0 + DIALOG_W;
   int dialog_y1 = y0 + DIALOG_H;
   *bx1 = dialog_x1 - 16;
   *bx0 = *bx1 - DIALOG_BUTTON_W;
   *by1 = dialog_y1 - DIALOG_STATUS_H - 10;
   *by0 = *by1 - DIALOG_BUTTON_H;
}

static void draw_dialog(int hover_cancel)
{
   ALLEGRO_BITMAP *prev;
   int line_h = al_get_font_line_height(a5_font);
   int x0, y0, x1, y1;
   int pct;
   int bar_x0, bar_y0, bar_x1, bar_y1;
   int bx0, by0, bx1, by1;
   char counts[64];
   char item_buf[128];

   if (a5_font == NULL || glb_ds1edit.screen_buff == NULL)
      return;

   x0 = (glb_config.screen.width - DIALOG_W) / 2;
   y0 = (glb_config.screen.height - DIALOG_H) / 2;
   x1 = x0 + DIALOG_W;
   y1 = y0 + DIALOG_H;

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
                            (float) x1, (float) (y0 + DIALOG_TITLE_H),
                            COLOR_TITLE_BG);
   al_draw_text(a5_font, COLOR_TITLE_FG,
                (float) (x0 + 8),
                (float) (y0 + (DIALOG_TITLE_H - line_h) / 2),
                0,
                s_state.title[0] ? s_state.title : "Export");

   /* stage label */
   al_draw_text(a5_font, COLOR_LABEL_FG,
                (float) (x0 + 16),
                (float) (y0 + DIALOG_TITLE_H + 8),
                0,
                s_state.stage_label[0] ? s_state.stage_label : "Working...");

   /* progress bar */
   pct = export_progress_percent(s_show_remote_stages);
   bar_x0 = x0 + 16;
   bar_x1 = x1 - 16;
   bar_y0 = y0 + DIALOG_TITLE_H + 8 + line_h + 6;
   bar_y1 = bar_y0 + DIALOG_BAR_H;

   al_draw_filled_rectangle((float) bar_x0, (float) bar_y0,
                            (float) bar_x1, (float) bar_y1, COLOR_BAR_BG);
   {
      int fill_x = bar_x0 + ((bar_x1 - bar_x0) * pct) / 100;
      ALLEGRO_COLOR fg = s_state.cancel_requested
         ? COLOR_BAR_FG_CANCEL : COLOR_BAR_FG;
      if (fill_x > bar_x0)
         al_draw_filled_rectangle((float) bar_x0, (float) bar_y0,
                                  (float) fill_x, (float) bar_y1, fg);
   }
   al_draw_rectangle((float) bar_x0 + 0.5f, (float) bar_y0 + 0.5f,
                     (float) bar_x1 - 0.5f, (float) bar_y1 - 0.5f,
                     COLOR_BORDER, 1.0f);

   /* item counts to the right of the bar's bottom edge */
   if (s_state.items_total > 0)
      snprintf(counts, sizeof(counts), "%d / %d  (%d%%)",
               s_state.items_done, s_state.items_total, pct);
   else
      snprintf(counts, sizeof(counts), "%d%%", pct);
   al_draw_text(a5_font, COLOR_DIM_FG,
                (float) bar_x0,
                (float) (bar_y1 + 4),
                0, counts);

   /* current item path (middle-truncated to ~58 chars) */
   if (s_state.current_item[0] != 0)
   {
      middle_truncate(s_state.current_item, item_buf, 58);
      al_draw_text(a5_font, COLOR_DIM_FG,
                   (float) bar_x0,
                   (float) (bar_y1 + 4 + line_h + 2),
                   0, item_buf);
   }

   /* cancel button */
   compute_button_rect(x0, y0, &bx0, &by0, &bx1, &by1);
   al_draw_filled_rectangle((float) bx0, (float) by0,
                            (float) bx1, (float) by1,
                            hover_cancel ? COLOR_BUTTON_HOVER : COLOR_BUTTON_BG);
   al_draw_rectangle((float) bx0 + 0.5f, (float) by0 + 0.5f,
                     (float) bx1 - 0.5f, (float) by1 - 0.5f,
                     COLOR_BORDER, 1.0f);
   {
      const char *label = s_state.cancel_requested ? "Canceling..." : "Cancel";
      int tw = al_get_text_width(a5_font, label);
      int tx = bx0 + ((bx1 - bx0) - tw) / 2;
      int ty = by0 + (DIALOG_BUTTON_H - line_h) / 2;
      al_draw_text(a5_font, COLOR_BUTTON_FG,
                   (float) tx, (float) ty, 0, label);
   }

   /* status hint */
   al_draw_filled_rectangle((float) x0,
                            (float) (y1 - DIALOG_STATUS_H),
                            (float) x1, (float) y1,
                            COLOR_TITLE_BG);
   al_draw_text(a5_font, COLOR_DIM_FG,
                (float) (x0 + 8),
                (float) (y1 - DIALOG_STATUS_H + (DIALOG_STATUS_H - line_h) / 2),
                0, "[Esc] cancel");

   if (prev != NULL)
      al_set_target_bitmap(prev);
}

static int point_in_button(int mx, int my)
{
   int x0 = (glb_config.screen.width - DIALOG_W) / 2;
   int y0 = (glb_config.screen.height - DIALOG_H) / 2;
   int bx0, by0, bx1, by1;
   compute_button_rect(x0, y0, &bx0, &by0, &bx1, &by1);
   return mx >= bx0 && mx < bx1 && my >= by0 && my < by1;
}

void export_progress_set_show_remote_stages(int yes)
{
   s_show_remote_stages = yes ? 1 : 0;
}

int export_progress_pump(void)
{
   ALLEGRO_EVENT ev;
   double now;
   int mx, my, mb;
   int hover_cancel;
   static int prev_mb = 1;

   if (!s_state.active)
      return 0;

   /* Drain pending events. */
   if (a5_event_queue != NULL)
   {
      while (al_get_next_event(a5_event_queue, &ev))
      {
         if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
            s_state.cancel_requested = 1;
         else if (ev.type == ALLEGRO_EVENT_KEY_CHAR
                  && ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
            s_state.cancel_requested = 1;
      }
   }

   al_get_mouse_state(&a5_ms_state);
   mx = a5_mouse_x;
   my = a5_mouse_y;
   mb = a5_mouse_b;
   hover_cancel = point_in_button(mx, my);
   if (mb && !prev_mb && hover_cancel)
      s_state.cancel_requested = 1;
   prev_mb = mb;

   /* Throttled paint: avoid redrawing on every single inner-loop call. */
   now = al_get_time();
   if (now - s_last_paint_time >= DIALOG_PAINT_INTERVAL)
   {
      draw_dialog(hover_cancel);
      misc_draw_screen(mx, my);
      s_last_paint_time = now;
   }

   return s_state.cancel_requested;
}
