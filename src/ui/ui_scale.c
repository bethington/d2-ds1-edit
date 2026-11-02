/* ui_scale.c -- see ui_scale.h. */

#include <stdio.h>
#include "structs.h"
#include <allegro5/allegro_ttf.h>
#include "ui/ui_scale.h"

#define UI_FONT_PATH "assets/ui/fonts/Inter.ttf"

/* Height of the screen the window is on, or of the window if none claims it. */
static int ui_screen_height(void)
{
   int wx, wy, i;
   int cx, cy;
   ALLEGRO_MONITOR_INFO mi;

   al_get_window_position(a5_display, &wx, &wy);
   cx = wx + al_get_display_width(a5_display) / 2;
   cy = wy + al_get_display_height(a5_display) / 2;
   for (i = 0; i < al_get_num_video_adapters(); i++)
   {
      if (al_get_monitor_info(i, &mi) && cx >= mi.x1 && cx < mi.x2 && cy >= mi.y1 && cy < mi.y2)
         return mi.y2 - mi.y1;
   }
   return al_get_display_height(a5_display);
}

void ui_scale_refresh(void)
{
   float scale;
   int px;
   ALLEGRO_FONT *loaded;

   if (a5_display == NULL)
      return;

   scale = ui_scale_choose(glb_config.ui_scale, ui_screen_height());
   if (scale == a5_ui_scale && a5_ui_font != NULL)
      return;

   px = (int)((float)UI_FONT_EM * scale + 0.5f);
   loaded = al_load_ttf_font(UI_FONT_PATH, px, 0);
   if (loaded == NULL)
   {
      /* Not fatal: ui_text() falls back to a5_font, scaled with everything else. */
      fprintf(stderr, "ui: can't load %s, using the built-in font\n", UI_FONT_PATH);
      a5_ui_scale = scale;
      return;
   }

   ui_scale_release();
   a5_ui_font = loaded;
   a5_ui_scale = scale;
   fprintf(stderr, "ui: scale %.0f%%, font %i px\n", scale * 100.0f, px);
}

void ui_scale_window_size(int *width, int *height)
{
   ALLEGRO_MONITOR_INFO mi;
   float scale;
   int mw, mh;

   if (!al_get_monitor_info(0, &mi))
      return;

   mw = mi.x2 - mi.x1;
   mh = mi.y2 - mi.y1;
   scale = ui_scale_choose(glb_config.ui_scale, mh);
   *width = (int)((float)*width * scale);
   *height = (int)((float)*height * scale);
   if (*width > mw)
      *width = mw;
   if (*height > mh)
      *height = mh;
}

void ui_scale_release(void)
{
   if (a5_ui_font != NULL)
   {
      al_destroy_font(a5_ui_font);
      a5_ui_font = NULL;
   }
}
