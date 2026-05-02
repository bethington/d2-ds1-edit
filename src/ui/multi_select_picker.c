#include <stdio.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "misc.h"
#include "ui/multi_select_picker.h"

#define ROW_HEIGHT         22
#define TITLE_HEIGHT       20
#define STATUS_HEIGHT      18
#define BORDER             2
#define CHECKBOX_W         16
#define PICKER_WIDTH       360
#define PICKER_MIN_HEIGHT  140

#define COLOR_PANEL_BG     al_map_rgba(10, 12, 16, 235)
#define COLOR_BORDER       al_map_rgb(120, 160, 200)
#define COLOR_TITLE_BG     al_map_rgb(20, 40, 60)
#define COLOR_TITLE_FG     al_map_rgb(200, 220, 255)
#define COLOR_ROW_FG       al_map_rgb(180, 200, 220)
#define COLOR_ROW_SEL_BG   al_map_rgb(40, 70, 110)
#define COLOR_ROW_SEL_FG   al_map_rgb(255, 255, 255)
#define COLOR_CHECK_BG     al_map_rgb(30, 35, 45)
#define COLOR_CHECK_BORDER al_map_rgb(80, 100, 130)
#define COLOR_CHECK_MARK   al_map_rgb(120, 200, 255)
#define COLOR_STATUS_FG    al_map_rgb(140, 160, 180)

typedef struct PICKER_STATE_S
{
   int selected;
   int x0, y0, x1, y1;
   int list_y0;
   const char *title;
   MULTI_SELECT_ITEM_S *items;
   int item_count;
} PICKER_STATE_S;

static int point_in_rect(int mx, int my, int x0, int y0, int x1, int y1)
{
   return mx >= x0 && mx < x1 && my >= y0 && my < y1;
}

static void compute_layout(PICKER_STATE_S *p)
{
   int height = TITLE_HEIGHT + p->item_count * ROW_HEIGHT
              + STATUS_HEIGHT + 2 * BORDER;
   if (height < PICKER_MIN_HEIGHT)
      height = PICKER_MIN_HEIGHT;

   p->x0 = (glb_config.screen.width  - PICKER_WIDTH) / 2;
   p->y0 = (glb_config.screen.height - height)       / 2;
   p->x1 = p->x0 + PICKER_WIDTH;
   p->y1 = p->y0 + height;
   p->list_y0 = p->y0 + TITLE_HEIGHT + BORDER;
}

/* When "All" toggle row is checked: set all other items to checked.
 * When unchecked: clear all other items. */
static void apply_all_toggle(MULTI_SELECT_ITEM_S *items, int n)
{
   int target;
   int i;
   if (items == NULL || n <= 0 || !items[0].is_all_toggle)
      return;
   target = items[0].selected ? 1 : 0;
   for (i = 1; i < n; i++)
      items[i].selected = target;
}

/* After toggling a specific item, sync the "All" row: if every item
 * other than the All-row is checked, the All-row should reflect that;
 * if any is unchecked, the All-row clears. */
static void sync_all_row(MULTI_SELECT_ITEM_S *items, int n)
{
   int all_set = 1;
   int i;
   if (items == NULL || n <= 0 || !items[0].is_all_toggle)
      return;
   for (i = 1; i < n; i++)
   {
      if (!items[i].selected)
      {
         all_set = 0;
         break;
      }
   }
   items[0].selected = all_set;
}

static int row_y_for(const PICKER_STATE_S *p, int idx)
{
   return p->list_y0 + idx * ROW_HEIGHT;
}

static int hit_test(const PICKER_STATE_S *p, int mx, int my)
{
   int row;
   if (mx < p->x0 || mx >= p->x1) return -1;
   if (my < p->list_y0) return -1;
   row = (my - p->list_y0) / ROW_HEIGHT;
   if (row < 0 || row >= p->item_count) return -1;
   return row;
}

static void toggle_at(PICKER_STATE_S *p, int idx)
{
   if (idx < 0 || idx >= p->item_count) return;
   p->items[idx].selected = !p->items[idx].selected;
   if (p->items[idx].is_all_toggle)
      apply_all_toggle(p->items, p->item_count);
   else
      sync_all_row(p->items, p->item_count);
}

static void draw_picker(const PICKER_STATE_S *p)
{
   ALLEGRO_BITMAP *prev;
   int line_h = al_get_font_line_height(a5_font);
   int i;

   prev = al_get_target_bitmap();
   al_set_target_bitmap(glb_ds1edit.screen_buff);

   /* panel + border */
   al_draw_filled_rectangle((float) p->x0, (float) p->y0,
                            (float) p->x1, (float) p->y1, COLOR_PANEL_BG);
   al_draw_rectangle((float) p->x0 + 0.5f, (float) p->y0 + 0.5f,
                     (float) p->x1 - 0.5f, (float) p->y1 - 0.5f,
                     COLOR_BORDER, 1.0f);

   /* title bar */
   al_draw_filled_rectangle((float) p->x0, (float) p->y0,
                            (float) p->x1, (float) (p->y0 + TITLE_HEIGHT),
                            COLOR_TITLE_BG);
   al_draw_text(a5_font, COLOR_TITLE_FG,
                (float) (p->x0 + 8),
                (float) (p->y0 + (TITLE_HEIGHT - line_h) / 2),
                0,
                p->title != NULL ? p->title : "Choose");

   /* rows */
   for (i = 0; i < p->item_count; i++)
   {
      int y = row_y_for(p, i);
      int selected = (i == p->selected);
      int cb_x = p->x0 + 12;
      int cb_y = y + (ROW_HEIGHT - CHECKBOX_W) / 2;
      ALLEGRO_COLOR fg;

      if (selected)
      {
         al_draw_filled_rectangle((float) (p->x0 + BORDER), (float) y,
                                  (float) (p->x1 - BORDER),
                                  (float) (y + ROW_HEIGHT),
                                  COLOR_ROW_SEL_BG);
      }

      /* checkbox */
      al_draw_filled_rectangle((float) cb_x, (float) cb_y,
                               (float) (cb_x + CHECKBOX_W),
                               (float) (cb_y + CHECKBOX_W),
                               COLOR_CHECK_BG);
      al_draw_rectangle((float) cb_x + 0.5f, (float) cb_y + 0.5f,
                        (float) (cb_x + CHECKBOX_W) - 0.5f,
                        (float) (cb_y + CHECKBOX_W) - 0.5f,
                        COLOR_CHECK_BORDER, 1.0f);
      if (p->items[i].selected)
      {
         /* checkmark: simple X */
         al_draw_line((float) cb_x + 3, (float) cb_y + 3,
                      (float) (cb_x + CHECKBOX_W - 3),
                      (float) (cb_y + CHECKBOX_W - 3),
                      COLOR_CHECK_MARK, 2.0f);
         al_draw_line((float) (cb_x + CHECKBOX_W - 3), (float) cb_y + 3,
                      (float) cb_x + 3,
                      (float) (cb_y + CHECKBOX_W - 3),
                      COLOR_CHECK_MARK, 2.0f);
      }

      /* label */
      fg = selected ? COLOR_ROW_SEL_FG : COLOR_ROW_FG;
      al_draw_text(a5_font, fg,
                   (float) (cb_x + CHECKBOX_W + 8),
                   (float) (y + (ROW_HEIGHT - line_h) / 2),
                   0, p->items[i].label);
   }

   /* status hint */
   al_draw_filled_rectangle((float) p->x0,
                            (float) (p->y1 - STATUS_HEIGHT),
                            (float) p->x1,
                            (float) p->y1, COLOR_TITLE_BG);
   al_draw_text(a5_font, COLOR_STATUS_FG,
                (float) (p->x0 + 8),
                (float) (p->y1 - STATUS_HEIGHT + (STATUS_HEIGHT - line_h) / 2),
                0,
                "[Up/Down] move  [Space] toggle  [Enter] confirm  [Esc] cancel");

   if (prev != NULL)
      al_set_target_bitmap(prev);
}

int multi_select_picker_show(const char *title,
                             MULTI_SELECT_ITEM_S *items, int item_count)
{
   PICKER_STATE_S picker;
   int snapshot[MULTI_SELECT_MAX_ITEMS];
   int done = 0;
   int prev_mb = 1;
   int confirmed = 0;
   int i;

   if (items == NULL || item_count <= 0 || item_count > MULTI_SELECT_MAX_ITEMS)
      return 0;

   /* Snapshot initial selected state so cancel can restore. */
   for (i = 0; i < item_count; i++)
      snapshot[i] = items[i].selected;

   memset(&picker, 0, sizeof(picker));
   picker.title = title;
   picker.items = items;
   picker.item_count = item_count;
   picker.selected = 0;

   compute_layout(&picker);
   al_flush_event_queue(a5_event_queue);

   while (!done)
   {
      ALLEGRO_EVENT ev;
      int mx, my, mb;

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
               picker.selected--;
               if (picker.selected < 0)
                  picker.selected = picker.item_count - 1;
            }
            else if (kc == ALLEGRO_KEY_DOWN)
            {
               picker.selected++;
               if (picker.selected >= picker.item_count)
                  picker.selected = 0;
            }
            else if (kc == ALLEGRO_KEY_SPACE)
            {
               toggle_at(&picker, picker.selected);
            }
         }
      }

      al_get_mouse_state(&a5_ms_state);
      mx = a5_mouse_x;
      my = a5_mouse_y;
      mb = a5_mouse_b;
      if (mb && !prev_mb)
      {
         int hit = hit_test(&picker, mx, my);
         if (hit >= 0)
         {
            picker.selected = hit;
            toggle_at(&picker, hit);
         }
      }
      prev_mb = mb;

      draw_picker(&picker);
      misc_draw_screen(mx, my);
      al_rest(0.016);
   }

   if (!confirmed)
   {
      /* Restore the snapshot so cancel leaves items array unchanged
       * from the caller's perspective. */
      for (i = 0; i < item_count; i++)
         items[i].selected = snapshot[i];
      return 0;
   }

   return 1;
}
