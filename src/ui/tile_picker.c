#include "structs.h"
#include "ui/compat.h"
#include "ui/edit_window.h"
#include "ui/tile_picker.h"

#define TP_MARGIN_X    8
#define TP_LINE_H      14
#define TP_FONT_H      8
#define TP_CAT_BTN_W   60
#define TP_CAT_ROW_H   16
#define TP_ZOOM_ROW_H  16
#define TP_CELL_GAP    4

/* Category-to-BLK_TYP_E mapping. BT_NULL terminates each category's list. */
static const BLK_TYP_E tp_cat_types[TPC_MAX][4] = {
   { BT_STATIC, BT_ANIMATED, BT_NULL, BT_NULL },         /* Floors  */
   { BT_WALL_UP, BT_WALL_DOWN, BT_WALL_ANIMATED, BT_NULL }, /* Walls */
   { BT_SHADOW, BT_NULL, BT_NULL, BT_NULL },             /* Shadows */
   { BT_ROOF,   BT_NULL, BT_NULL, BT_NULL },             /* Roofs   */
   { BT_SPECIAL, BT_NULL, BT_NULL, BT_NULL }             /* Special */
};

static const char * tp_cat_labels[TPC_MAX] = {
   "Floors", "Walls", "Shadows", "Roofs", "Special"
};


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
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   TP_STATE_S * t = &pp->tiles;

   if (t->tiles_built_for_ds1 == ds1_idx) return;

   /* Rebuild for new DS1 */
   wedit_tiles_free();
   wedit_tiles_make(ds1_idx);
   t->tiles_built_for_ds1 = ds1_idx;
}


/* Draw one tile cell at screen position (cx, cy) with size (cw, ch).
 * Returns TRUE if drawn, FALSE if bitmap missing. */
static int pp_draw_tile_cell(int ds1_idx, int bt_idx, int cx, int cy,
                              int cw, int ch, int selected)
{
   BLOCK_TABLE_S * bt_ptr = glb_ds1[ds1_idx].block_table;
   ALLEGRO_BITMAP * bmp;
   int dt1_idx, block_idx;
   int src_w, src_h;
   int draw_w, draw_h;
   float scale;

   if (bt_idx <= 0) return FALSE;
   dt1_idx = bt_ptr[bt_idx].dt1_idx;
   block_idx = bt_ptr[bt_idx].block_idx;
   if (dt1_idx < 0) return FALSE;
   bmp = *(glb_dt1[dt1_idx].block_zoom[ZM_11] + block_idx);
   if (bmp == NULL) return FALSE;

   src_w = al_get_bitmap_width(bmp);
   src_h = al_get_bitmap_height(bmp);
   if (src_w <= 0 || src_h <= 0) return FALSE;

   /* Scale to fit within cell while preserving aspect */
   {
      float sx = (float)cw / (float)src_w;
      float sy = (float)ch / (float)src_h;
      scale = sx < sy ? sx : sy;
   }
   draw_w = (int)(src_w * scale);
   draw_h = (int)(src_h * scale);

   /* Cell background */
   al_draw_filled_rectangle((float)cx, (float)cy,
                             (float)(cx + cw), (float)(cy + ch),
                             al_map_rgba(30, 30, 40, 200));

   /* Draw the scaled tile, centered horizontally, bottom-aligned */
   al_draw_scaled_bitmap(bmp,
      0, 0, (float)src_w, (float)src_h,
      (float)(cx + (cw - draw_w) / 2),
      (float)(cy + (ch - draw_h)),
      (float)draw_w, (float)draw_h, 0);

   /* Selection border */
   if (selected)
   {
      ALLEGRO_COLOR col_sel = al_map_rgb(255, 220, 80);
      al_draw_rectangle((float)cx + 0.5f, (float)cy + 0.5f,
                         (float)(cx + cw) - 0.5f, (float)(cy + ch) - 0.5f,
                         col_sel, 2.0f);
   }
   else
   {
      al_draw_rectangle((float)cx + 0.5f, (float)cy + 0.5f,
                         (float)(cx + cw) - 0.5f, (float)(cy + ch) - 0.5f,
                         al_map_rgba(80, 80, 100, 150), 1.0f);
   }
   return TRUE;
}


/* Iterate the current category's tiles and invoke a callback for each cell.
 * Used by both draw and click. Callback returns 1 to stop iteration. */
typedef int (*TP_CELL_FN)(int ds1_idx, BLK_TYP_E type, int m_idx, int s_idx,
                           int bt_idx, int cx, int cy, int cw, int ch,
                           void * user_data);

static void pp_iter_cells(int ds1_idx, int content_x, int content_y,
                           int content_w, int content_h, int scroll_y,
                           TP_CELL_FN fn, void * user_data)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   TP_STATE_S * t = &pp->tiles;
   WIN_EDIT_S * w = &glb_ds1edit.win_edit;
   int cols = t->zoom_cols;
   int gap = TP_CELL_GAP;
   int inner_w = content_w - 2 * TP_MARGIN_X;
   int cell_w = (inner_w - (cols - 1) * gap) / cols;
   int cell_h = cell_w / 2;  /* default 2:1 aspect */
   int cur_col = 0;
   int cur_y = content_y - scroll_y;
   int row_max_h = 0;
   int ti, m, s;

   if (cell_w < 16) cell_w = 16;
   if (cell_h < 8) cell_h = 8;

   /* Walls can be taller — give them more vertical room */
   if (t->category == TPC_WALLS || t->category == TPC_ROOFS)
      cell_h = cell_w * 3 / 2;

   for (ti = 0; tp_cat_types[t->category][ti] != BT_NULL && ti < 4; ti++)
   {
      BLK_TYP_E type = tp_cat_types[t->category][ti];
      int n_lines = w->main_line_num[type];

      for (m = 0; m < n_lines; m++)
      {
         MAIN_LINE_S * mptr = w->main_line_tab[type] + m;
         if (mptr == NULL) continue;
         for (s = 0; s < mptr->bt_idx_num; s++)
         {
            SUB_ELM_S * sptr = mptr->sub_elm + s;
            int bt = sptr->bt_idx_tab;
            int cx, cy;

            cx = content_x + TP_MARGIN_X + cur_col * (cell_w + gap);
            cy = cur_y;

            /* Skip entirely-offscreen cells for performance */
            if (cy + cell_h >= content_y &&
                cy < content_y + content_h)
            {
               if (fn(ds1_idx, type, m, s, bt, cx, cy, cell_w, cell_h,
                      user_data))
                  return;
            }
            else if (cy < content_y)
            {
               /* scrolled off top — still need to advance */
            }
            else if (cy >= content_y + content_h)
            {
               /* scrolled off bottom — done iterating */
               return;
            }

            if (cell_h > row_max_h) row_max_h = cell_h;
            cur_col++;
            if (cur_col >= cols)
            {
               cur_col = 0;
               cur_y += row_max_h + gap;
               row_max_h = 0;
            }
         }
      }
   }
}


typedef struct {
   int ds1_idx;
} TP_DRAW_CTX;

static int pp_cell_draw_cb(int ds1_idx, BLK_TYP_E type, int m_idx, int s_idx,
                            int bt_idx, int cx, int cy, int cw, int ch,
                            void * user_data)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   TP_STATE_S * t = &pp->tiles;
   int selected;

   (void)user_data;
   (void)type; (void)m_idx; (void)s_idx;

   selected = (t->brush.valid && t->brush.bt_idx == bt_idx);
   pp_draw_tile_cell(ds1_idx, bt_idx, cx, cy, cw, ch, selected);
   return 0;
}


typedef struct {
   int mx, my;
   int found;
   BLK_TYP_E type;
   int m_idx, s_idx, bt_idx;
} TP_HIT_CTX;

static int pp_cell_hit_cb(int ds1_idx, BLK_TYP_E type, int m_idx, int s_idx,
                           int bt_idx, int cx, int cy, int cw, int ch,
                           void * user_data)
{
   TP_HIT_CTX * ctx = (TP_HIT_CTX *)user_data;
   (void)ds1_idx;

   if (ctx->mx >= cx && ctx->mx < cx + cw &&
       ctx->my >= cy && ctx->my < cy + ch)
   {
      ctx->found = 1;
      ctx->type = type;
      ctx->m_idx = m_idx;
      ctx->s_idx = s_idx;
      ctx->bt_idx = bt_idx;
      return 1;  /* stop iteration */
   }
   return 0;
}


void tile_picker_draw(int ds1_idx, int panel_x, int panel_right,
                       int y_top, int panel_bottom)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   TP_STATE_S * t = &pp->tiles;
   int panel_w = panel_right - panel_x;
   int y = y_top;
   int content_y, content_h;
   int cat;
   ALLEGRO_COLOR col_btn      = al_map_rgba(40, 40, 60, 220);
   ALLEGRO_COLOR col_btn_act  = al_map_rgba(60, 90, 150, 240);
   ALLEGRO_COLOR col_btn_text = al_map_rgb(200, 200, 200);
   ALLEGRO_COLOR col_btn_act_text = al_map_rgb(255, 255, 255);

   tile_picker_ensure_built(ds1_idx);

   /* Category selector row */
   {
      int btn_w = (panel_w - 2 * TP_MARGIN_X - 4 * 2) / TPC_MAX;
      int btn_x;
      if (btn_w < 30) btn_w = 30;
      for (cat = 0; cat < TPC_MAX; cat++)
      {
         btn_x = panel_x + TP_MARGIN_X + cat * (btn_w + 2);
         al_draw_filled_rectangle((float)btn_x, (float)y,
                                   (float)(btn_x + btn_w),
                                   (float)(y + TP_CAT_ROW_H - 2),
                                   t->category == cat ? col_btn_act : col_btn);
         al_draw_textf(a5_font,
                        t->category == cat ? col_btn_act_text : col_btn_text,
                        (float)(btn_x + btn_w / 2), (float)(y + 4),
                        ALLEGRO_ALIGN_CENTRE, "%s", tp_cat_labels[cat]);
      }
   }
   y += TP_CAT_ROW_H;

   /* Zoom controls row */
   {
      int bx = panel_x + TP_MARGIN_X;
      char buf[32];
      /* [-] button */
      al_draw_filled_rectangle((float)bx, (float)y,
                                (float)(bx + 16), (float)(y + TP_ZOOM_ROW_H - 2),
                                col_btn);
      al_draw_textf(a5_font, col_btn_text,
                     (float)(bx + 8), (float)(y + 4),
                     ALLEGRO_ALIGN_CENTRE, "-");
      /* label */
      sprintf(buf, "%d cols", t->zoom_cols);
      al_draw_textf(a5_font, col_btn_text,
                     (float)(bx + 22), (float)(y + 4), 0, "%s", buf);
      /* [+] button */
      al_draw_filled_rectangle((float)(bx + 70), (float)y,
                                (float)(bx + 86), (float)(y + TP_ZOOM_ROW_H - 2),
                                col_btn);
      al_draw_textf(a5_font, col_btn_text,
                     (float)(bx + 78), (float)(y + 4),
                     ALLEGRO_ALIGN_CENTRE, "+");
      /* help text */
      al_draw_textf(a5_font, al_map_rgb(100, 100, 100),
                     (float)(bx + 96), (float)(y + 4), 0,
                     "Ctrl+wheel");
   }
   y += TP_ZOOM_ROW_H + 4;

   /* Tile grid content area */
   content_y = y;
   content_h = panel_bottom - y;
   if (content_h < 32)
      return;

   /* Check if the current category has any tiles */
   {
      WIN_EDIT_S * w = &glb_ds1edit.win_edit;
      int total = 0;
      int ti;
      for (ti = 0; tp_cat_types[t->category][ti] != BT_NULL && ti < 4; ti++)
      {
         BLK_TYP_E type = tp_cat_types[t->category][ti];
         int m;
         int n_lines = w->main_line_num[type];
         if (w->main_line_tab[type] == NULL) continue;
         for (m = 0; m < n_lines; m++)
            total += w->main_line_tab[type][m].bt_idx_num;
      }
      if (total == 0)
      {
         al_draw_textf(a5_font, al_map_rgb(100, 100, 100),
                        (float)(panel_x + TP_MARGIN_X), (float)(content_y + 8),
                        0, "No tiles in this category");
         return;
      }
   }

   /* Clip to content area */
   al_set_clipping_rectangle(panel_x, content_y, panel_w, content_h);
   pp_iter_cells(ds1_idx, panel_x, content_y, panel_w, content_h,
                  t->scroll_y, pp_cell_draw_cb, NULL);
   al_reset_clipping_rectangle();

   /* Draw brush indicator at the bottom */
   if (t->brush.valid)
   {
      ALLEGRO_COLOR col = al_map_rgb(255, 220, 80);
      al_draw_textf(a5_font, col,
                     (float)(panel_x + TP_MARGIN_X),
                     (float)(panel_bottom - 12), 0,
                     "Brush: bt=%d", t->brush.bt_idx);
   }
}


int tile_picker_click(int ds1_idx, int mx, int my,
                       int panel_x, int panel_right,
                       int y_top, int panel_bottom)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   TP_STATE_S * t = &pp->tiles;
   int panel_w = panel_right - panel_x;
   int y = y_top;
   int content_y, content_h;
   int cat;

   /* Category selector */
   {
      int btn_w = (panel_w - 2 * TP_MARGIN_X - 4 * 2) / TPC_MAX;
      int btn_x;
      if (btn_w < 30) btn_w = 30;
      if (my >= y && my < y + TP_CAT_ROW_H)
      {
         for (cat = 0; cat < TPC_MAX; cat++)
         {
            btn_x = panel_x + TP_MARGIN_X + cat * (btn_w + 2);
            if (mx >= btn_x && mx < btn_x + btn_w)
            {
               t->category = cat;
               t->scroll_y = 0;
               return 0;
            }
         }
         return -1;
      }
   }
   y += TP_CAT_ROW_H;

   /* Zoom buttons */
   {
      int bx = panel_x + TP_MARGIN_X;
      if (my >= y && my < y + TP_ZOOM_ROW_H)
      {
         if (mx >= bx && mx < bx + 16)
         {
            t->zoom_cols--;
            if (t->zoom_cols < 1) t->zoom_cols = 1;
            return 0;
         }
         if (mx >= bx + 70 && mx < bx + 86)
         {
            t->zoom_cols++;
            if (t->zoom_cols > 4) t->zoom_cols = 4;
            return 0;
         }
         return -1;
      }
   }
   y += TP_ZOOM_ROW_H + 4;

   content_y = y;
   content_h = panel_bottom - y;
   if (my < content_y || my >= content_y + content_h)
      return -1;

   /* Hit-test cells */
   {
      TP_HIT_CTX ctx;
      ctx.mx = mx; ctx.my = my;
      ctx.found = 0;
      ctx.type = BT_NULL;
      ctx.m_idx = 0; ctx.s_idx = 0; ctx.bt_idx = 0;
      pp_iter_cells(ds1_idx, panel_x, content_y, panel_w, content_h,
                     t->scroll_y, pp_cell_hit_cb, &ctx);
      if (ctx.found)
      {
         t->brush.valid = TRUE;
         t->brush.bt_idx = ctx.bt_idx;
         t->brush.type = ctx.type;
         t->brush.m_idx = ctx.m_idx;
         t->brush.s_idx = ctx.s_idx;
         t->brush.button = tile_picker_layer_button_for(ds1_idx, ctx.type);
         return 0;
      }
   }
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
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   TP_STATE_S * t = &pp->tiles;
   CELL_F_S save_f[FLOOR_MAX_LAYER];
   CELL_W_S save_w[WALL_MAX_LAYER];
   CELL_S_S save_s[SHADOW_MAX_LAYER];

   if (!t->brush.valid) return;
   if (ds1_idx < 0 || ds1_idx >= DS1_MAX) return;
   if (cx < 0 || cy < 0) return;
   if (cx >= glb_ds1[ds1_idx].width || cy >= glb_ds1[ds1_idx].height) return;

   wedit_save_tile(ds1_idx, cx, cy, save_f, save_w, save_s);
   wedit_update_tile(ds1_idx, cx, cy, t->brush.button, t->brush.type,
                      t->brush.m_idx, t->brush.s_idx);
   wedit_keep_tile(ds1_idx, cx, cy, save_f, save_w, save_s);
   tile_picker_push_mru(t->brush.type, t->brush.bt_idx);
}


void tile_picker_push_mru(BLK_TYP_E type, int bt_idx)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   TP_STATE_S * t = &pp->tiles;
   int i, found_idx;

   if (type < 0 || type >= BT_MAX) return;
   if (bt_idx <= 0) return;

   /* Check if already in list — move to front */
   found_idx = -1;
   for (i = 0; i < t->mru_count[type]; i++)
   {
      if (t->mru_bt_idx[type][i] == bt_idx)
      {
         found_idx = i;
         break;
      }
   }

   if (found_idx > 0)
   {
      /* Shift [0..found-1] down by 1 */
      for (i = found_idx; i > 0; i--)
         t->mru_bt_idx[type][i] = t->mru_bt_idx[type][i - 1];
      t->mru_bt_idx[type][0] = bt_idx;
   }
   else if (found_idx < 0)
   {
      /* New entry — shift all down */
      int cap = t->mru_count[type];
      if (cap >= TP_MRU_MAX) cap = TP_MRU_MAX - 1;
      for (i = cap; i > 0; i--)
         t->mru_bt_idx[type][i] = t->mru_bt_idx[type][i - 1];
      t->mru_bt_idx[type][0] = bt_idx;
      if (t->mru_count[type] < TP_MRU_MAX)
         t->mru_count[type]++;
   }
   /* else found_idx == 0, already at front */
}


/* Find (type, m_idx, s_idx) for a given bt_idx by walking main_line_tab.
 * Returns TRUE if found. */
static int tp_find_tile_in_main_line(int bt_idx, BLK_TYP_E * out_type,
                                      int * out_m, int * out_s)
{
   WIN_EDIT_S * w = &glb_ds1edit.win_edit;
   int t, m, s;
   for (t = 0; t < BT_MAX; t++)
   {
      int n_lines = w->main_line_num[t];
      if (w->main_line_tab[t] == NULL) continue;
      for (m = 0; m < n_lines; m++)
      {
         MAIN_LINE_S * mptr = w->main_line_tab[t] + m;
         for (s = 0; s < mptr->bt_idx_num; s++)
         {
            if (mptr->sub_elm[s].bt_idx_tab == bt_idx)
            {
               *out_type = (BLK_TYP_E)t;
               *out_m = m;
               *out_s = s;
               return TRUE;
            }
         }
      }
   }
   return FALSE;
}


/* Collect all variants (same main_index) of the given bt_idx from block_table.
 * Writes up to max_out bt_idx values to out_bt_idxs. Returns count. */
static int tp_collect_variants(int ds1_idx, int hovered_bt,
                                int * out_bt_idxs, int max_out)
{
   BLOCK_TABLE_S * bt_ptr = glb_ds1[ds1_idx].block_table;
   int bt_num = glb_ds1[ds1_idx].bt_num;
   int i, count = 0;
   long target_main;
   BLK_TYP_E target_type;

   if (hovered_bt <= 0 || hovered_bt >= bt_num) return 0;
   target_main = bt_ptr[hovered_bt].main_index;
   target_type = bt_ptr[hovered_bt].type;

   for (i = 1; i < bt_num && count < max_out; i++)
   {
      if (bt_ptr[i].main_index == target_main &&
          bt_ptr[i].type == target_type)
      {
         out_bt_idxs[count++] = i;
      }
   }
   return count;
}


/* Determine the hovered tile's bt_idx at (cx, cy). Checks wall, floor,
 * then shadow. Returns -1 if nothing. Also sets *out_button. */
static int tp_find_hovered_bt(int ds1_idx, int cx, int cy, BUT_TYP_E * out_button)
{
   int bt;

   /* Try walls first (top of stack visually) */
   bt = wedit_search_tile(ds1_idx, cx, cy, BU_WALL1);
   if (bt > 0) { *out_button = BU_WALL1; return bt; }
   bt = wedit_search_tile(ds1_idx, cx, cy, BU_WALL2);
   if (bt > 0) { *out_button = BU_WALL2; return bt; }

   /* Then floor */
   bt = wedit_search_tile(ds1_idx, cx, cy, BU_FLOOR1);
   if (bt > 0) { *out_button = BU_FLOOR1; return bt; }
   bt = wedit_search_tile(ds1_idx, cx, cy, BU_FLOOR2);
   if (bt > 0) { *out_button = BU_FLOOR2; return bt; }

   /* Shadow as last resort */
   bt = wedit_search_tile(ds1_idx, cx, cy, BU_SHADOW);
   if (bt > 0) { *out_button = BU_SHADOW; return bt; }

   *out_button = BU_NULL;
   return -1;
}


int tile_picker_popup_run(int ds1_idx, int cx, int cy,
                           int screen_x, int screen_y)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   TP_STATE_S * t = &pp->tiles;
   int hovered_bt;
   BUT_TYP_E hovered_button = BU_NULL;
   BLK_TYP_E hovered_type = BT_NULL;
   int variants[TP_VARIANTS_POPUP_MAX];
   int variant_count = 0;
   int recent_count;
   int recents[TP_RECENT_POPUP_MAX];
   int total_cells, n_rows;
   int cell_w = 64, cell_h = 32;
   int gap = 4;
   int popup_x, popup_y, popup_w, popup_h;
   int disp_w, disp_h;
   int menu_done = 0;
   int hover_idx = -1;
   int i;

   tile_picker_ensure_built(ds1_idx);

   /* Figure out which tile the cursor is on */
   hovered_bt = tp_find_hovered_bt(ds1_idx, cx, cy, &hovered_button);
   if (hovered_bt > 0)
   {
      BLOCK_TABLE_S * bt_ptr = glb_ds1[ds1_idx].block_table;
      hovered_type = bt_ptr[hovered_bt].type;
      variant_count = tp_collect_variants(ds1_idx, hovered_bt,
                                           variants, TP_VARIANTS_POPUP_MAX);
   }

   /* Recent list from MRU for the hovered type (or first non-empty) */
   {
      BLK_TYP_E rt = hovered_type;
      if (rt == BT_NULL || t->mru_count[rt] == 0)
      {
         /* Fall back to any type with recents */
         int ti;
         rt = BT_NULL;
         for (ti = 1; ti < BT_MAX; ti++)
         {
            if (t->mru_count[ti] > 0)
            {
               rt = (BLK_TYP_E)ti;
               break;
            }
         }
      }
      recent_count = (rt != BT_NULL) ? t->mru_count[rt] : 0;
      if (recent_count > TP_RECENT_POPUP_MAX)
         recent_count = TP_RECENT_POPUP_MAX;
      for (i = 0; i < recent_count; i++)
         recents[i] = t->mru_bt_idx[rt][i];
   }

   total_cells = recent_count + variant_count;
   if (total_cells == 0)
   {
      /* Nothing to show — fall through to old wedit_test */
      wedit_test(ds1_idx, cx, cy);
      return 0;
   }

   /* Popup layout: 2 rows (recents top, variants bottom), cells 64x32 */
   n_rows = 0;
   if (recent_count > 0) n_rows++;
   if (variant_count > 0) n_rows++;

   {
      int max_in_row = recent_count > variant_count ? recent_count : variant_count;
      if (max_in_row < 1) max_in_row = 1;
      popup_w = max_in_row * (cell_w + gap) + gap + 8;
      popup_h = n_rows * (cell_h + gap) + gap + 8;
   }

   disp_w = al_get_display_width(a5_display);
   disp_h = al_get_display_height(a5_display);

   popup_x = screen_x - popup_w / 2;
   popup_y = screen_y - popup_h / 2;
   if (popup_x < 2) popup_x = 2;
   if (popup_y < 2) popup_y = 2;
   if (popup_x + popup_w > disp_w - 2) popup_x = disp_w - popup_w - 2;
   if (popup_y + popup_h > disp_h - 2) popup_y = disp_h - popup_h - 2;

   /* Modal loop while right-button is held */
   while (!menu_done)
   {
      int mx, my, row, col;
      int recent_y = popup_y + 4;
      int variant_y = recent_count > 0
                       ? popup_y + 4 + cell_h + gap
                       : popup_y + 4;
      int best_idx = -1;

      al_get_mouse_state(&a5_ms_state);
      al_get_keyboard_state(&a5_kb_state);
      mx = al_get_mouse_state_axis(&a5_ms_state, 0);
      my = al_get_mouse_state_axis(&a5_ms_state, 1);

      /* Hit-test cells — negative index = none */
      for (i = 0; i < recent_count; i++)
      {
         int cx_cell = popup_x + 4 + i * (cell_w + gap);
         if (mx >= cx_cell && mx < cx_cell + cell_w &&
             my >= recent_y && my < recent_y + cell_h)
         {
            best_idx = i;  /* 0..recent_count-1 = recents */
            break;
         }
      }
      if (best_idx < 0)
      {
         for (i = 0; i < variant_count; i++)
         {
            int cx_cell = popup_x + 4 + i * (cell_w + gap);
            if (mx >= cx_cell && mx < cx_cell + cell_w &&
                my >= variant_y && my < variant_y + cell_h)
            {
               best_idx = recent_count + i;  /* recent_count+.. = variants */
               break;
            }
         }
      }
      hover_idx = best_idx;

      /* Draw popup background on top of current backbuffer */
      al_set_target_backbuffer(a5_display);
      al_draw_filled_rectangle((float)popup_x, (float)popup_y,
                                (float)(popup_x + popup_w),
                                (float)(popup_y + popup_h),
                                al_map_rgba(28, 32, 48, 240));
      al_draw_rectangle((float)popup_x + 0.5f, (float)popup_y + 0.5f,
                         (float)(popup_x + popup_w) - 0.5f,
                         (float)(popup_y + popup_h) - 0.5f,
                         al_map_rgb(80, 100, 160), 2.0f);

      /* Recents row */
      for (i = 0; i < recent_count; i++)
      {
         int cx_cell = popup_x + 4 + i * (cell_w + gap);
         pp_draw_tile_cell(ds1_idx, recents[i], cx_cell, recent_y,
                            cell_w, cell_h, hover_idx == i);
      }

      /* Separator between rows */
      if (recent_count > 0 && variant_count > 0)
      {
         int sep_y = recent_y + cell_h + gap / 2;
         al_draw_line((float)(popup_x + 4), (float)sep_y,
                      (float)(popup_x + popup_w - 4), (float)sep_y,
                      al_map_rgb(60, 70, 90), 1.0f);
      }

      /* Variants row */
      for (i = 0; i < variant_count; i++)
      {
         int cx_cell = popup_x + 4 + i * (cell_w + gap);
         pp_draw_tile_cell(ds1_idx, variants[i], cx_cell, variant_y,
                            cell_w, cell_h, hover_idx == recent_count + i);
      }

      al_flip_display();
      al_rest(0.016);

      /* Right-button release or Escape exits loop */
      if (!al_mouse_button_down(&a5_ms_state, 2))
         menu_done = 1;
      if (al_key_down(&a5_kb_state, ALLEGRO_KEY_ESCAPE))
      {
         hover_idx = -1;  /* cancel */
         menu_done = 1;
      }
   }

   /* On release: if hover_idx valid, set brush AND place it */
   if (hover_idx >= 0)
   {
      int picked_bt;
      BLK_TYP_E pick_type = BT_NULL;
      int pick_m = 0, pick_s = 0;

      if (hover_idx < recent_count)
         picked_bt = recents[hover_idx];
      else
         picked_bt = variants[hover_idx - recent_count];

      if (tp_find_tile_in_main_line(picked_bt, &pick_type, &pick_m, &pick_s))
      {
         t->brush.valid = TRUE;
         t->brush.bt_idx = picked_bt;
         t->brush.type = pick_type;
         t->brush.m_idx = pick_m;
         t->brush.s_idx = pick_s;
         t->brush.button = hovered_button != BU_NULL
                            ? hovered_button
                            : tile_picker_layer_button_for(ds1_idx, pick_type);
         tile_picker_place_brush(ds1_idx, cx, cy);
      }
   }
   return 0;
}


BUT_TYP_E tile_picker_layer_button_for(int ds1_idx, BLK_TYP_E type)
{
   /* Return the first enabled layer for the tile's family.
    * Floors: FLOOR1/FLOOR2; Walls: WALL1-4; Shadows: SHADOW. */
   if (ds1_idx < 0 || ds1_idx >= DS1_MAX) return BU_NULL;

   switch (type)
   {
      case BT_STATIC:
      case BT_ANIMATED:
         if (glb_ds1[ds1_idx].floor_layer_mask[0]) return BU_FLOOR1;
         if (FLOOR_MAX_LAYER > 1 &&
             glb_ds1[ds1_idx].floor_layer_mask[1]) return BU_FLOOR2;
         return BU_FLOOR1;

      case BT_SHADOW:
         return BU_SHADOW;

      case BT_WALL_UP:
      case BT_WALL_DOWN:
      case BT_ROOF:
      case BT_SPECIAL:
      case BT_WALL_ANIMATED:
         if (glb_ds1[ds1_idx].wall_layer_mask[0]) return BU_WALL1;
         if (WALL_MAX_LAYER > 1 &&
             glb_ds1[ds1_idx].wall_layer_mask[1]) return BU_WALL2;
         if (WALL_MAX_LAYER > 2 &&
             glb_ds1[ds1_idx].wall_layer_mask[2]) return BU_WALL3;
         if (WALL_MAX_LAYER > 3 &&
             glb_ds1[ds1_idx].wall_layer_mask[3]) return BU_WALL4;
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
