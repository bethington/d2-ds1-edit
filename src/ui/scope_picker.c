#include <stdio.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include "structs.h"
#include "misc.h"
#include "core/export_presets.h"
#include "ui/scope_picker.h"
#include "ui/text_input_modal.h"

#define ROW_HEIGHT       22
#define TITLE_HEIGHT     20
#define STATUS_HEIGHT    18
#define BORDER           2
#define SEPARATOR_H       8
#define PICKER_WIDTH    520
#define PICKER_MAX_ROWS  20

#define COLOR_PANEL_BG     al_map_rgba(10, 12, 16, 235)
#define COLOR_BORDER       al_map_rgb(120, 160, 200)
#define COLOR_TITLE_BG     al_map_rgb(20, 40, 60)
#define COLOR_TITLE_FG     al_map_rgb(200, 220, 255)
#define COLOR_ROW_FG       al_map_rgb(180, 200, 220)
#define COLOR_ROW_DIM_FG   al_map_rgb(95, 105, 120)
#define COLOR_ROW_SEL_BG   al_map_rgb(40, 70, 110)
#define COLOR_ROW_SEL_FG   al_map_rgb(255, 255, 255)
#define COLOR_PATTERN_FG   al_map_rgb(140, 160, 180)
#define COLOR_SEP          al_map_rgb(60, 80, 100)
#define COLOR_STATUS_FG    al_map_rgb(140, 160, 180)

typedef enum ENTRY_KIND_E
{
   ENTRY_BUILTIN_ALL = 0,
   ENTRY_BUILTIN_AREA,
   ENTRY_BUILTIN_FOLDER,
   ENTRY_BUILTIN_PATTERN,
   ENTRY_SEPARATOR,
   ENTRY_PRESET
} ENTRY_KIND_E;

typedef struct ENTRY_S
{
   ENTRY_KIND_E kind;
   const char  *label;
   const char  *secondary;   /* preset pattern, shown dimmed */
   int          enabled;
   const EXPORT_PRESET_S *preset;
} ENTRY_S;

#define ENTRIES_MAX 64

typedef struct SCOPE_PICKER_S
{
   int         entry_count;
   int         selected;
   ENTRY_S     entries[ENTRIES_MAX];

   char        all_label[64];

   const char *title;

   int x0, y0, x1, y1;
   int list_y0;
   int list_h;
} SCOPE_PICKER_S;

static int type_matches(const char *preset_type, const char *type_filter)
{
   if (preset_type == NULL || type_filter == NULL)
      return 0;
   if (stricmp(preset_type, "all") == 0)
      return 1;
   if (stricmp(type_filter, "all") == 0)
      return 1;
   return stricmp(preset_type, type_filter) == 0;
}

static void build_entries(SCOPE_PICKER_S *p, const char *type_filter,
                          int area_available)
{
   int i;
   int preset_total;
   int matched = 0;

   /* Built-ins */
   snprintf(p->all_label, sizeof(p->all_label),
            "All %s assets",
            (type_filter != NULL && stricmp(type_filter, "all") != 0)
               ? type_filter : "supported");
   p->entries[p->entry_count].kind = ENTRY_BUILTIN_ALL;
   p->entries[p->entry_count].label = p->all_label;
   p->entries[p->entry_count].secondary = NULL;
   p->entries[p->entry_count].enabled = 1;
   p->entries[p->entry_count].preset = NULL;
   p->entry_count++;

   p->entries[p->entry_count].kind = ENTRY_BUILTIN_AREA;
   p->entries[p->entry_count].label = "Current area's assets";
   p->entries[p->entry_count].secondary =
      area_available ? NULL : "(open a DS1 file first)";
   p->entries[p->entry_count].enabled = area_available ? 1 : 0;
   p->entries[p->entry_count].preset = NULL;
   p->entry_count++;

   p->entries[p->entry_count].kind = ENTRY_BUILTIN_FOLDER;
   p->entries[p->entry_count].label = "Choose folder...";
   p->entries[p->entry_count].secondary = NULL;
   p->entries[p->entry_count].enabled = 1;
   p->entries[p->entry_count].preset = NULL;
   p->entry_count++;

   p->entries[p->entry_count].kind = ENTRY_BUILTIN_PATTERN;
   p->entries[p->entry_count].label = "Type custom pattern...";
   p->entries[p->entry_count].secondary = NULL;
   p->entries[p->entry_count].enabled = 1;
   p->entries[p->entry_count].preset = NULL;
   p->entry_count++;

   /* Separator (only if at least one matching preset) */
   preset_total = export_presets_count();
   for (i = 0; i < preset_total; i++)
   {
      const EXPORT_PRESET_S *pr = export_presets_at(i);
      if (pr != NULL && type_matches(pr->type, type_filter))
         matched++;
   }

   if (matched > 0)
   {
      if (p->entry_count < ENTRIES_MAX)
      {
         p->entries[p->entry_count].kind = ENTRY_SEPARATOR;
         p->entries[p->entry_count].label = NULL;
         p->entries[p->entry_count].secondary = NULL;
         p->entries[p->entry_count].enabled = 0;
         p->entries[p->entry_count].preset = NULL;
         p->entry_count++;
      }

      for (i = 0; i < preset_total && p->entry_count < ENTRIES_MAX; i++)
      {
         const EXPORT_PRESET_S *pr = export_presets_at(i);
         if (pr == NULL || !type_matches(pr->type, type_filter))
            continue;

         p->entries[p->entry_count].kind = ENTRY_PRESET;
         p->entries[p->entry_count].label = pr->name;
         p->entries[p->entry_count].secondary = pr->pattern;
         p->entries[p->entry_count].enabled = 1;
         p->entries[p->entry_count].preset = pr;
         p->entry_count++;
      }
   }
}

static int row_height_for(const ENTRY_S *e)
{
   return (e->kind == ENTRY_SEPARATOR) ? SEPARATOR_H : ROW_HEIGHT;
}

static void compute_layout(SCOPE_PICKER_S *p)
{
   int i;
   int list_h = 0;

   for (i = 0; i < p->entry_count; i++)
      list_h += row_height_for(&p->entries[i]);

   p->x0 = (glb_config.screen.width - PICKER_WIDTH) / 2;
   p->y0 = (glb_config.screen.height -
            (TITLE_HEIGHT + list_h + STATUS_HEIGHT + 2 * BORDER)) / 2;
   p->x1 = p->x0 + PICKER_WIDTH;
   p->y1 = p->y0 + TITLE_HEIGHT + list_h + STATUS_HEIGHT + 2 * BORDER;
   p->list_y0 = p->y0 + TITLE_HEIGHT + BORDER;
   p->list_h = list_h;
}

static int row_y_for(const SCOPE_PICKER_S *p, int idx)
{
   int y = p->list_y0;
   int i;
   for (i = 0; i < idx; i++)
      y += row_height_for(&p->entries[i]);
   return y;
}

static int hit_test(const SCOPE_PICKER_S *p, int mx, int my)
{
   int i;
   int y = p->list_y0;

   if (mx < p->x0 || mx >= p->x1)
      return -1;

   for (i = 0; i < p->entry_count; i++)
   {
      int h = row_height_for(&p->entries[i]);
      if (my >= y && my < y + h)
      {
         if (p->entries[i].kind == ENTRY_SEPARATOR)
            return -1;
         return p->entries[i].enabled ? i : -1;
      }
      y += h;
   }
   return -1;
}

static void move_selection(SCOPE_PICKER_S *p, int dir)
{
   int n = p->entry_count;
   int s = p->selected;
   int tries;

   for (tries = 0; tries < n; tries++)
   {
      s += dir;
      if (s < 0)
         s = n - 1;
      if (s >= n)
         s = 0;
      if (p->entries[s].kind != ENTRY_SEPARATOR && p->entries[s].enabled)
      {
         p->selected = s;
         return;
      }
   }
}

static void draw_picker(const SCOPE_PICKER_S *p)
{
   ALLEGRO_BITMAP *prev;
   int line_h = al_get_font_line_height(a5_font);
   int i;

   prev = al_get_target_bitmap();
   al_set_target_bitmap(glb_ds1edit.screen_buff);

   al_draw_filled_rectangle((float) p->x0, (float) p->y0,
                            (float) p->x1, (float) p->y1,
                            COLOR_PANEL_BG);
   al_draw_rectangle((float) p->x0 + 0.5f, (float) p->y0 + 0.5f,
                     (float) p->x1 - 0.5f, (float) p->y1 - 0.5f,
                     COLOR_BORDER, 1.0f);

   /* title */
   al_draw_filled_rectangle((float) p->x0, (float) p->y0,
                            (float) p->x1, (float) (p->y0 + TITLE_HEIGHT),
                            COLOR_TITLE_BG);
   al_draw_text(a5_font, COLOR_TITLE_FG,
                (float) (p->x0 + 8),
                (float) (p->y0 + (TITLE_HEIGHT - line_h) / 2),
                0,
                p->title != NULL ? p->title : "Choose Export Scope");

   /* rows */
   for (i = 0; i < p->entry_count; i++)
   {
      int y = row_y_for(p, i);
      int h = row_height_for(&p->entries[i]);
      const ENTRY_S *e = &p->entries[i];

      if (e->kind == ENTRY_SEPARATOR)
      {
         al_draw_line((float) (p->x0 + 12),
                      (float) (y + h / 2),
                      (float) (p->x1 - 12),
                      (float) (y + h / 2),
                      COLOR_SEP, 1.0f);
         continue;
      }

      if (i == p->selected)
      {
         al_draw_filled_rectangle((float) (p->x0 + BORDER), (float) y,
                                  (float) (p->x1 - BORDER),
                                  (float) (y + h),
                                  COLOR_ROW_SEL_BG);
      }

      {
         ALLEGRO_COLOR fg;
         int label_x = p->x0 + 12;
         int label_y = y + (h - line_h) / 2;
         int label_w = 0;

         if (!e->enabled)
            fg = COLOR_ROW_DIM_FG;
         else if (i == p->selected)
            fg = COLOR_ROW_SEL_FG;
         else
            fg = COLOR_ROW_FG;

         if (e->label != NULL)
         {
            al_draw_text(a5_font, fg, (float) label_x, (float) label_y,
                         0, e->label);
            label_w = al_get_text_width(a5_font, e->label);
         }

         if (e->secondary != NULL)
         {
            int sec_x = label_x + label_w + 16;
            int max_x = p->x1 - 16;
            char clipped[256];
            int n = (int) strlen(e->secondary);
            if (n >= (int) sizeof(clipped))
               n = (int) sizeof(clipped) - 1;
            memcpy(clipped, e->secondary, n);
            clipped[n] = 0;

            /* Clip with ellipsis if too wide. */
            while (n > 4 &&
                   sec_x + al_get_text_width(a5_font, clipped) > max_x)
            {
               clipped[--n] = 0;
               clipped[n - 1] = '.';
               clipped[n - 2] = '.';
               clipped[n - 3] = '.';
            }

            al_draw_text(a5_font, COLOR_PATTERN_FG,
                         (float) sec_x, (float) label_y, 0, clipped);
         }
      }
   }

   /* status row */
   al_draw_filled_rectangle((float) p->x0,
                            (float) (p->y1 - STATUS_HEIGHT),
                            (float) p->x1,
                            (float) p->y1,
                            COLOR_TITLE_BG);
   al_draw_text(a5_font, COLOR_STATUS_FG,
                (float) (p->x0 + 8),
                (float) (p->y1 - STATUS_HEIGHT + (STATUS_HEIGHT - line_h) / 2),
                0,
                "[Up/Down] select  [Enter] confirm  [Esc] cancel");

   if (prev != NULL)
      al_set_target_bitmap(prev);
}

static int populate_result_from_entry(const ENTRY_S *e,
                                      const char *type_filter,
                                      SCOPE_RESULT_S *out)
{
   memset(out, 0, sizeof(*out));
   (void) type_filter;

   switch (e->kind)
   {
   case ENTRY_BUILTIN_ALL:
      out->kind = SCOPE_KIND_ALL;
      return 1;
   case ENTRY_BUILTIN_AREA:
      out->kind = SCOPE_KIND_AREA;
      return 1;
   case ENTRY_BUILTIN_FOLDER:
      out->kind = SCOPE_KIND_FOLDER;
      return 1;
   case ENTRY_BUILTIN_PATTERN:
   {
      char buf[256];
      buf[0] = 0;
      if (text_input_modal_show("Type Custom Pattern",
                                "Enter a glob pattern (e.g. data\\global\\items\\inv*.dc6):",
                                NULL, buf, (int) sizeof(buf)))
      {
         out->kind = SCOPE_KIND_PATTERN;
         strncpy(out->pattern, buf, sizeof(out->pattern) - 1);
         out->pattern[sizeof(out->pattern) - 1] = 0;
         return 1;
      }
      return 0;
   }
   case ENTRY_PRESET:
      if (e->preset == NULL)
         return 0;
      out->kind = SCOPE_KIND_PATTERN;
      strncpy(out->pattern, e->preset->pattern, sizeof(out->pattern) - 1);
      out->pattern[sizeof(out->pattern) - 1] = 0;
      return 1;
   default:
      return 0;
   }
}

int scope_picker_choose(const char *title,
                        const char *type_filter,
                        int area_available,
                        SCOPE_RESULT_S *out)
{
   SCOPE_PICKER_S picker;
   int done = 0;
   int prev_mb = 1;
   int confirmed = 0;
   int confirmed_idx = -1;

   if (out == NULL)
      return 0;

   memset(&picker, 0, sizeof(picker));
   picker.title = title;
   build_entries(&picker, type_filter, area_available);

   /* Select first enabled non-separator row. */
   picker.selected = 0;
   if (picker.entries[0].kind == ENTRY_SEPARATOR
       || !picker.entries[0].enabled)
   {
      move_selection(&picker, 1);
   }

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
               confirmed_idx = picker.selected;
               done = 1;
               break;
            }
            if (kc == ALLEGRO_KEY_UP)
               move_selection(&picker, -1);
            else if (kc == ALLEGRO_KEY_DOWN)
               move_selection(&picker, 1);
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
            confirmed = 1;
            confirmed_idx = hit;
            done = 1;
         }
      }
      prev_mb = mb;

      draw_picker(&picker);
      misc_draw_screen(mx, my);
      al_rest(0.016);
   }

   if (!confirmed || confirmed_idx < 0 || confirmed_idx >= picker.entry_count)
      return 0;

   return populate_result_from_entry(&picker.entries[confirmed_idx],
                                     type_filter, out);
}
