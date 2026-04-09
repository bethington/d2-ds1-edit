#include "structs.h"
#include "ui/compat.h"
#include "ui/tile_picker.h"

#define TP_MARGIN_X    8
#define TP_LINE_H      14
#define TP_FONT_H      8


void tile_picker_init(void)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   TP_STATE_S * t = &pp->tiles;
   int i, j;

   t->category = TPC_FLOORS;
   t->zoom_cols = 2;
   t->scroll_y = 0;
   t->brush.valid = FALSE;
   t->brush.bt_idx = 0;
   t->brush.type = BT_NULL;
   t->brush.button = BU_NULL;
   t->brush.m_idx = 0;
   t->brush.s_idx = 0;
   t->tiles_built_for_ds1 = -1;

   for (i = 0; i < BT_MAX; i++)
   {
      t->mru_count[i] = 0;
      for (j = 0; j < TP_MRU_MAX; j++)
         t->mru_bt_idx[i][j] = -1;
   }
}


void tile_picker_ensure_built(int ds1_idx)
{
   /* TODO: Phase 1 — call wedit_tiles_make(ds1_idx) once per DS1 */
   (void)ds1_idx;
}


void tile_picker_draw(int ds1_idx, int panel_x, int panel_right,
                       int y_top, int panel_bottom)
{
   ALLEGRO_COLOR col_title = al_map_rgb(255, 200, 80);
   ALLEGRO_COLOR col_info  = al_map_rgb(140, 130, 120);
   int y = y_top;

   (void)ds1_idx;
   (void)panel_bottom;

   /* Phase 0 stub — placeholder content */
   al_draw_textf(a5_font, col_title,
                  (float)(panel_x + TP_MARGIN_X), (float)y, 0,
                  "Tiles Tab (stub)");
   y += TP_LINE_H + 4;

   al_draw_textf(a5_font, col_info,
                  (float)(panel_x + TP_MARGIN_X), (float)y, 0,
                  "Phase 0 scaffold");
   y += TP_LINE_H;

   al_draw_textf(a5_font, col_info,
                  (float)(panel_x + TP_MARGIN_X), (float)y, 0,
                  "Category: Floors");
   y += TP_LINE_H;

   al_draw_textf(a5_font, col_info,
                  (float)(panel_x + TP_MARGIN_X), (float)y, 0,
                  "Grid coming in Phase 1");

   (void)panel_right;
}


int tile_picker_click(int ds1_idx, int mx, int my,
                       int panel_x, int panel_right,
                       int y_top, int panel_bottom)
{
   (void)ds1_idx;
   (void)mx; (void)my;
   (void)panel_x; (void)panel_right;
   (void)y_top; (void)panel_bottom;
   return -1;
}


int tile_picker_scroll(int dz, int ctrl_held)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   TP_STATE_S * t = &pp->tiles;

   if (ctrl_held)
   {
      t->zoom_cols -= dz;
      if (t->zoom_cols < 1) t->zoom_cols = 1;
      if (t->zoom_cols > 4) t->zoom_cols = 4;
      return 1;
   }

   t->scroll_y -= dz * (TP_LINE_H * 3);
   if (t->scroll_y < 0) t->scroll_y = 0;
   return 1;
}


void tile_picker_place_brush(int ds1_idx, int cx, int cy)
{
   /* TODO: Phase 2 */
   (void)ds1_idx; (void)cx; (void)cy;
}


void tile_picker_push_mru(BLK_TYP_E type, int bt_idx)
{
   /* TODO: Phase 3 */
   (void)type; (void)bt_idx;
}


int tile_picker_popup_run(int ds1_idx, int cx, int cy,
                           int screen_x, int screen_y)
{
   /* TODO: Phase 4 */
   (void)ds1_idx; (void)cx; (void)cy;
   (void)screen_x; (void)screen_y;
   return 0;
}


BUT_TYP_E tile_picker_layer_button_for(int ds1_idx, BLK_TYP_E type)
{
   /* Phase 0: simple defaults, Phase 3 reads layer masks */
   (void)ds1_idx;
   switch (type)
   {
      case BT_STATIC:
      case BT_ANIMATED:
         return BU_FLOOR1;
      case BT_SHADOW:
         return BU_SHADOW;
      case BT_WALL_UP:
      case BT_WALL_DOWN:
      case BT_ROOF:
      case BT_SPECIAL:
      case BT_WALL_ANIMATED:
         return BU_WALL1;
      default:
         return BU_NULL;
   }
}


void tile_picker_on_ds1_change(int new_ds1_idx)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   TP_STATE_S * t = &pp->tiles;
   int i, j;

   t->brush.valid = FALSE;
   t->scroll_y = 0;
   t->tiles_built_for_ds1 = -1;
   for (i = 0; i < BT_MAX; i++)
   {
      t->mru_count[i] = 0;
      for (j = 0; j < TP_MRU_MAX; j++)
         t->mru_bt_idx[i][j] = -1;
   }
   (void)new_ds1_idx;
}
