#include <stdio.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "misc.h"
#include "ui/upscale_mode_picker.h"

#define MODE_COUNT         3
#define ROW_HEIGHT         26
#define TITLE_HEIGHT       20
#define STATUS_HEIGHT      56
#define BORDER             2
#define PICKER_WIDTH       420
#define PICKER_HEIGHT      172
#define PANEL_PADDING      8
#define COLOR_PANEL_BG     al_map_rgba(10, 12, 16, 235)
#define COLOR_BORDER       al_map_rgb(120, 160, 200)
#define COLOR_TITLE_BG     al_map_rgb(20, 40, 60)
#define COLOR_TITLE_FG     al_map_rgb(200, 220, 255)
#define COLOR_ROW_FG       al_map_rgb(180, 200, 220)
#define COLOR_ROW_SEL_BG   al_map_rgb(40, 70, 110)
#define COLOR_ROW_SEL_FG   al_map_rgb(255, 255, 255)
#define COLOR_STATUS_FG    al_map_rgb(140, 160, 180)

typedef struct UPSCALE_PICKER_S
{
   int selected;
   int x0, y0, x1, y1;
   int list_y0;
   const char *title;
   int remote_enabled;
} UPSCALE_PICKER_S;

static const int g_mode_values[MODE_COUNT] = {
   UPSCALE_MODE_NONE,
   UPSCALE_MODE_2X,
   UPSCALE_MODE_4X
};

static const char *g_mode_labels[MODE_COUNT] = {
   "Native PNG only",
   "Upscale to 2x",
   "Upscale to 4x"
};

static void compute_layout(UPSCALE_PICKER_S *picker)
{
   picker->x0 = (glb_config.screen.width - PICKER_WIDTH) / 2;
   picker->y0 = (glb_config.screen.height - PICKER_HEIGHT) / 2;
   picker->x1 = picker->x0 + PICKER_WIDTH;
   picker->y1 = picker->y0 + PICKER_HEIGHT;
   picker->list_y0 = picker->y0 + TITLE_HEIGHT + BORDER;
}

static void draw_picker(UPSCALE_PICKER_S *picker)
{
   ALLEGRO_BITMAP *prev;
   int line_h = al_get_font_line_height(a5_font);
   int clip_x;
   int clip_y;
   int clip_w;
   int clip_h;
   int row;
   const char *status;

   prev = al_get_target_bitmap();
   al_set_target_bitmap(glb_ds1edit.screen_buff);

   al_draw_filled_rectangle((float) picker->x0, (float) picker->y0,
                            (float) picker->x1, (float) picker->y1,
                            COLOR_PANEL_BG);
   al_draw_rectangle((float) picker->x0 + 0.5f, (float) picker->y0 + 0.5f,
                     (float) picker->x1 - 0.5f, (float) picker->y1 - 0.5f,
                     COLOR_BORDER, 1.0f);

   al_draw_filled_rectangle((float) picker->x0, (float) picker->y0,
                            (float) picker->x1, (float) (picker->y0 + TITLE_HEIGHT),
                            COLOR_TITLE_BG);
   al_get_clipping_rectangle(&clip_x, &clip_y, &clip_w, &clip_h);
   al_set_clipping_rectangle(picker->x0 + PANEL_PADDING,
                             picker->y0,
                             PICKER_WIDTH - (PANEL_PADDING * 2),
                             TITLE_HEIGHT);
   al_draw_text(a5_font, COLOR_TITLE_FG,
                (float) (picker->x0 + PANEL_PADDING),
                (float) (picker->y0 + (TITLE_HEIGHT - line_h) / 2),
                0,
                picker->title != NULL ? picker->title : "Choose Export Scale");
   al_set_clipping_rectangle(clip_x, clip_y, clip_w, clip_h);

   for (row = 0; row < MODE_COUNT; row++)
   {
      int y = picker->list_y0 + row * ROW_HEIGHT;
      int selected = (row == picker->selected);

      if (selected)
      {
         al_draw_filled_rectangle((float) (picker->x0 + BORDER), (float) y,
                                  (float) (picker->x1 - BORDER),
                                  (float) (y + ROW_HEIGHT),
                                  COLOR_ROW_SEL_BG);
      }

      al_draw_text(a5_font,
                   selected ? COLOR_ROW_SEL_FG : COLOR_ROW_FG,
                   (float) (picker->x0 + 12),
                   (float) (y + (ROW_HEIGHT - line_h) / 2),
                   0,
                   g_mode_labels[row]);
   }

   status = picker->remote_enabled
      ? "Remote service configured. 2x/4x will prefer remote upscale."
      : "Remote service not configured. 2x/4x will use local fallback when available.";
   al_draw_filled_rectangle((float) picker->x0,
                            (float) (picker->y1 - STATUS_HEIGHT),
                            (float) picker->x1,
                            (float) picker->y1,
                            COLOR_TITLE_BG);
   al_get_clipping_rectangle(&clip_x, &clip_y, &clip_w, &clip_h);
   al_set_clipping_rectangle(picker->x0 + PANEL_PADDING,
                             picker->y1 - STATUS_HEIGHT,
                             PICKER_WIDTH - (PANEL_PADDING * 2),
                             STATUS_HEIGHT);
   al_draw_multiline_text(a5_font, COLOR_STATUS_FG,
                          (float) (picker->x0 + PANEL_PADDING),
                          (float) (picker->y1 - STATUS_HEIGHT + 4),
                          (float) (PICKER_WIDTH - (PANEL_PADDING * 2)),
                          (float) line_h,
                          0,
                          status);
   al_draw_text(a5_font, COLOR_STATUS_FG,
                (float) (picker->x0 + PANEL_PADDING),
                (float) (picker->y1 - STATUS_HEIGHT + 4 + (line_h * 2)),
                0,
                "[Up/Down] select  [Enter] confirm  [Esc] cancel");
   al_set_clipping_rectangle(clip_x, clip_y, clip_w, clip_h);

   if (prev != NULL)
      al_set_target_bitmap(prev);
}

int upscale_mode_picker_choose(const char *title, int remote_enabled)
{
   UPSCALE_PICKER_S picker;
   int done = 0;
   int result = -1;
   int prev_mb = 1;

   memset(&picker, 0, sizeof(picker));
   picker.title = title;
   picker.remote_enabled = remote_enabled;
   compute_layout(&picker);

   al_flush_event_queue(a5_event_queue);

   while (!done)
   {
      ALLEGRO_EVENT ev;
      int mx;
      int my;
      int mb;

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
               result = g_mode_values[picker.selected];
               done = 1;
               break;
            }
            if (kc == ALLEGRO_KEY_UP)
            {
               picker.selected--;
               if (picker.selected < 0)
                  picker.selected = MODE_COUNT - 1;
            }
            else if (kc == ALLEGRO_KEY_DOWN)
            {
               picker.selected++;
               if (picker.selected >= MODE_COUNT)
                  picker.selected = 0;
            }
         }
      }

      al_get_mouse_state(&a5_ms_state);
      mx = a5_mouse_x;
      my = a5_mouse_y;
      mb = a5_mouse_b;
      if (mb && !prev_mb && mx >= picker.x0 && mx <= picker.x1 &&
          my >= picker.list_y0 && my < picker.list_y0 + MODE_COUNT * ROW_HEIGHT)
      {
         picker.selected = (my - picker.list_y0) / ROW_HEIGHT;
         if (picker.selected < 0)
            picker.selected = 0;
         if (picker.selected >= MODE_COUNT)
            picker.selected = MODE_COUNT - 1;
         result = g_mode_values[picker.selected];
         done = 1;
      }
      prev_mb = mb;

      draw_picker(&picker);
      misc_draw_screen(mx, my);
      al_rest(0.016);
   }

   return result;
}