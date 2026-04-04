#include <math.h>
#include <string.h>
#include "structs.h"
#include "gfx_custom.h"
#include "misc.h"
#include "editobj.h"
#include "editpath.h"
#include "wPreview.h"
#include "dt1misc.h"

typedef struct
{
   double clear_ms_total;
   double palette_ms_total;
   double mouse_ms_total;
   double obj_anim_ms_total;
   double loop_1a_ms_total;
   double loop_1a_w_ms_total;
   double loop_1a_f_ms_total;
   double loop_1a_s_ms_total;
   double loop_1b_ms_total;
   double loop_2_ms_total;
   double loop_3_ms_total;
   double loop_3_w_ms_total;
   double loop_3_obj_ms_total;
   double loop_4_ms_total;
   double loop_5_ms_total;
   double loop_6_ms_total;
   double cursor_ms_total;
   double paths_ms_total;
   double obj_info_ms_total;
   double path_info_ms_total;
   double help_ms_total;
   double hud_ms_total;
   double present_ms_total;
   double reinit_ms_total;
   double total_ms_total;

   double clear_ms_max;
   double palette_ms_max;
   double mouse_ms_max;
   double obj_anim_ms_max;
   double loop_1a_ms_max;
   double loop_1a_w_ms_max;
   double loop_1a_f_ms_max;
   double loop_1a_s_ms_max;
   double loop_1b_ms_max;
   double loop_2_ms_max;
   double loop_3_ms_max;
   double loop_3_w_ms_max;
   double loop_3_obj_ms_max;
   double loop_4_ms_max;
   double loop_5_ms_max;
   double loop_6_ms_max;
   double cursor_ms_max;
   double paths_ms_max;
   double obj_info_ms_max;
   double path_info_ms_max;
   double help_ms_max;
   double hud_ms_max;
   double present_ms_max;
   double reinit_ms_max;
   double total_ms_max;

   int frames;
} RENDER_PERF_STATS_S;

static RENDER_PERF_STATS_S glb_render_perf_stats;

static double render_perf_now_ms(void)
{
   return al_get_time() * 1000.0;
}

static void render_perf_add(double * total, double * max, double dt_ms)
{
   (*total) += dt_ms;
   if (dt_ms > *max)
      *max = dt_ms;
}

static void render_perf_print_summary(void)
{
   double inv;

   if (glb_render_perf_stats.frames <= 0)
      return;

   inv = 1.0 / glb_render_perf_stats.frames;
   fprintf(stderr, "\n[render-perf] last %d frames\n", glb_render_perf_stats.frames);
   fprintf(stderr, "[render-perf] total:      avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.total_ms_total * inv, glb_render_perf_stats.total_ms_max);
   fprintf(stderr, "[render-perf] clear:      avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.clear_ms_total * inv, glb_render_perf_stats.clear_ms_max);
   fprintf(stderr, "[render-perf] palette:    avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.palette_ms_total * inv, glb_render_perf_stats.palette_ms_max);
   fprintf(stderr, "[render-perf] mouse:      avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.mouse_ms_total * inv, glb_render_perf_stats.mouse_ms_max);
   fprintf(stderr, "[render-perf] obj_anim:   avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.obj_anim_ms_total * inv, glb_render_perf_stats.obj_anim_ms_max);
   fprintf(stderr, "[render-perf] loop1a:     avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.loop_1a_ms_total * inv, glb_render_perf_stats.loop_1a_ms_max);
   fprintf(stderr, "[render-perf] loop1a_w:   avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.loop_1a_w_ms_total * inv, glb_render_perf_stats.loop_1a_w_ms_max);
   fprintf(stderr, "[render-perf] loop1a_f:   avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.loop_1a_f_ms_total * inv, glb_render_perf_stats.loop_1a_f_ms_max);
   fprintf(stderr, "[render-perf] loop1a_s:   avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.loop_1a_s_ms_total * inv, glb_render_perf_stats.loop_1a_s_ms_max);
   fprintf(stderr, "[render-perf] loop1b:     avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.loop_1b_ms_total * inv, glb_render_perf_stats.loop_1b_ms_max);
   fprintf(stderr, "[render-perf] loop2:      avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.loop_2_ms_total * inv, glb_render_perf_stats.loop_2_ms_max);
   fprintf(stderr, "[render-perf] loop3:      avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.loop_3_ms_total * inv, glb_render_perf_stats.loop_3_ms_max);
   fprintf(stderr, "[render-perf] loop3_w:    avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.loop_3_w_ms_total * inv, glb_render_perf_stats.loop_3_w_ms_max);
   fprintf(stderr, "[render-perf] loop3_obj:  avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.loop_3_obj_ms_total * inv, glb_render_perf_stats.loop_3_obj_ms_max);
   fprintf(stderr, "[render-perf] loop4:      avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.loop_4_ms_total * inv, glb_render_perf_stats.loop_4_ms_max);
   fprintf(stderr, "[render-perf] loop5:      avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.loop_5_ms_total * inv, glb_render_perf_stats.loop_5_ms_max);
   fprintf(stderr, "[render-perf] loop6:      avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.loop_6_ms_total * inv, glb_render_perf_stats.loop_6_ms_max);
   fprintf(stderr, "[render-perf] cursor:     avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.cursor_ms_total * inv, glb_render_perf_stats.cursor_ms_max);
   fprintf(stderr, "[render-perf] paths:      avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.paths_ms_total * inv, glb_render_perf_stats.paths_ms_max);
   fprintf(stderr, "[render-perf] obj_info:   avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.obj_info_ms_total * inv, glb_render_perf_stats.obj_info_ms_max);
   fprintf(stderr, "[render-perf] path_info:  avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.path_info_ms_total * inv, glb_render_perf_stats.path_info_ms_max);
   fprintf(stderr, "[render-perf] help:       avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.help_ms_total * inv, glb_render_perf_stats.help_ms_max);
   fprintf(stderr, "[render-perf] hud:        avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.hud_ms_total * inv, glb_render_perf_stats.hud_ms_max);
   fprintf(stderr, "[render-perf] present:    avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.present_ms_total * inv, glb_render_perf_stats.present_ms_max);
   fprintf(stderr, "[render-perf] reinit:     avg %7.2f ms  max %7.2f ms\n",
      glb_render_perf_stats.reinit_ms_total * inv, glb_render_perf_stats.reinit_ms_max);
   fflush(stderr);

   memset(&glb_render_perf_stats, 0, sizeof(glb_render_perf_stats));
}

// Palette state: tracks which palette is active to avoid redundant rebuilds.
// Initialized by wpreview_init_palette_state() before the render loop starts.
static int wpreview_old_pal = -1;

void wpreview_init_palette_state(int ds1_idx)
{
   if (glb_ds1edit.cmd_line.force_pal_num == -1)
      wpreview_old_pal = glb_ds1[ds1_idx].act - 1;
   else
      wpreview_old_pal = glb_ds1edit.cmd_line.force_pal_num - 1;
}

static int _draw_call_count = 0;
static double _draw_call_total_ms = 0;
static double _draw_call_max_ms = 0;
static int _draw_call_mem_count = 0;

static void wpreview_draw_bitmap(ALLEGRO_BITMAP * bmp, int x, int y)
{
   double t0 = al_get_time();
   al_draw_bitmap(bmp, (float) x, (float) y, 0);
   double dt = (al_get_time() - t0) * 1000.0;
   _draw_call_count++;
   _draw_call_total_ms += dt;
   if (dt > _draw_call_max_ms) _draw_call_max_ms = dt;
   if (bmp && (al_get_bitmap_flags(bmp) & ALLEGRO_MEMORY_BITMAP))
      _draw_call_mem_count++;
}

static void wpreview_draw_trans_bitmap(ALLEGRO_BITMAP * bmp, int x, int y, float alpha)
{
   al_draw_tinted_bitmap(
      bmp,
      al_map_rgba_f(alpha, alpha, alpha, alpha),
      (float) x,
      (float) y,
      0
   );
}

static void wpreview_draw_blended_bitmap(ALLEGRO_BITMAP * bmp, int x, int y, int trans_b)
{
   switch (trans_b)
   {
      case 0:
         wpreview_draw_trans_bitmap(bmp, x, y, 0.25f);
         break;
      case 1:
         wpreview_draw_trans_bitmap(bmp, x, y, 0.50f);
         break;
      case 2:
         wpreview_draw_trans_bitmap(bmp, x, y, 0.75f);
         break;
      case 3:
         al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_ONE);
         wpreview_draw_bitmap(bmp, x, y);
         al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
         break;
      case 4:
         al_set_blender(ALLEGRO_ADD, ALLEGRO_DEST_COLOR, ALLEGRO_ZERO);
         wpreview_draw_bitmap(bmp, x, y);
         al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
         break;
      case 6:
         al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_INVERSE_SRC_COLOR);
         wpreview_draw_bitmap(bmp, x, y);
         al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
         break;
      default:
         wpreview_draw_trans_bitmap(bmp, x, y, 0.50f);
         break;
   }
}

static void wpreview_draw_scaled_blended_bitmap(ALLEGRO_BITMAP * bmp, int x, int y,
                                                int div, int trans_b)
{
   float sw, sh, dw, dh;

   sw = (float) al_get_bitmap_width(bmp);
   sh = (float) al_get_bitmap_height(bmp);
   dw = sw / (float) div;
   dh = sh / (float) div;

   if (trans_b < 0)
   {
      al_draw_scaled_bitmap(bmp, 0, 0, sw, sh, (float) x, (float) y, dw, dh, 0);
      return;
   }

   switch (trans_b)
   {
      case 0:
         al_draw_tinted_scaled_bitmap(
            bmp, al_map_rgba_f(0.25f, 0.25f, 0.25f, 0.25f),
            0, 0, sw, sh, (float) x, (float) y, dw, dh, 0
         );
         break;
      case 1:
         al_draw_tinted_scaled_bitmap(
            bmp, al_map_rgba_f(0.50f, 0.50f, 0.50f, 0.50f),
            0, 0, sw, sh, (float) x, (float) y, dw, dh, 0
         );
         break;
      case 2:
         al_draw_tinted_scaled_bitmap(
            bmp, al_map_rgba_f(0.75f, 0.75f, 0.75f, 0.75f),
            0, 0, sw, sh, (float) x, (float) y, dw, dh, 0
         );
         break;
      case 3:
         al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_ONE);
         al_draw_scaled_bitmap(bmp, 0, 0, sw, sh, (float) x, (float) y, dw, dh, 0);
         al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
         break;
      case 4:
         al_set_blender(ALLEGRO_ADD, ALLEGRO_DEST_COLOR, ALLEGRO_ZERO);
         al_draw_scaled_bitmap(bmp, 0, 0, sw, sh, (float) x, (float) y, dw, dh, 0);
         al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
         break;
      case 6:
         al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_INVERSE_SRC_COLOR);
         al_draw_scaled_bitmap(bmp, 0, 0, sw, sh, (float) x, (float) y, dw, dh, 0);
         al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
         break;
      default:
         al_draw_tinted_scaled_bitmap(
            bmp, al_map_rgba_f(0.50f, 0.50f, 0.50f, 0.50f),
            0, 0, sw, sh, (float) x, (float) y, dw, dh, 0
         );
         break;
   }
}

static void wpreview_blit_region(ALLEGRO_BITMAP * src,
                                 int sx, int sy, int dx, int dy, int w, int h)
{
   al_draw_bitmap_region(src, (float) sx, (float) sy, (float) w, (float) h,
                         (float) dx, (float) dy, 0);
}

static void wpreview_draw_line(int x1, int y1, int x2, int y2, int color)
{
   al_draw_line((float) x1 + 0.5f, (float) y1 + 0.5f,
                (float) x2 + 0.5f, (float) y2 + 0.5f,
                pal_color(color), 1.0f);
}

static void wpreview_draw_rect(int x1, int y1, int x2, int y2, int color)
{
   al_draw_rectangle((float) x1 + 0.5f, (float) y1 + 0.5f,
                     (float) x2 + 0.5f, (float) y2 + 0.5f,
                     pal_color(color), 1.0f);
}

static void wpreview_draw_rectfill(int x1, int y1, int x2, int y2, int color)
{
   al_draw_filled_rectangle((float) x1, (float) y1,
                            (float) x2 + 1.0f, (float) y2 + 1.0f,
                            pal_color(color));
}

#define wpreview_draw_hline(x1, y, x2, color) wpreview_draw_line((x1), (y), (x2), (y), (color))
#define wpreview_draw_vline(x, y1, y2, color) wpreview_draw_line((x), (y1), (x), (y2), (color))
#define wpreview_textprintf(x, y, color, ...) \
   al_draw_textf(a5_font, pal_color(color), (float) (x), (float) (y), 0, __VA_ARGS__)

#ifdef WIN32
   #pragma warning (push)
   #pragma warning (disable  : 4244 4267 )
#endif

// ==========================================================================
// draw a sprite with a tint
void wpreview_shape(ALLEGRO_BITMAP * tmp_bmp, int x0, int y0, int ds1_idx,
                    COL_E col_idx)
{
   if (glb_ds1edit.cmd_line.force_pal_num == -1)
      ((void)0); /* A5: selection tinting handled by draw_lit_sprite */
   else
      ((void)0);
   wpreview_draw_trans_bitmap(tmp_bmp, x0, y0, a5_trans_alpha);
}


// ==========================================================================
// draw a gouraud floor sprite
void wpreview_gouraud_f(ALLEGRO_BITMAP * tmp_bmp, int x0, int y0, int ds1_idx,
                        int c1, int c2, int c3, int c4)
{
   if ((c1 == c2) && (c2 == c3) && (c3 == c4))
   {
      if (c1 <= 7)
         return;
      if (c1 >= 248)
         wpreview_draw_bitmap(tmp_bmp, x0, y0);
      else
      {
         if (glb_ds1edit.cmd_line.force_pal_num == -1)
            ((void)0);
         else
            ((void)0);
         wpreview_draw_trans_bitmap(tmp_bmp, x0, y0, a5_trans_alpha);
      }
   }
   else
   {
      if (glb_ds1edit.cmd_line.force_pal_num == -1)
         ((void)0);
      else
         ((void)0);
      if (glb_ds1edit.night_mode == 1)
      {
         wpreview_draw_trans_bitmap(tmp_bmp, x0, y0, a5_trans_alpha);
      }
      else
      {
         wpreview_draw_trans_bitmap(tmp_bmp, x0, y0, a5_trans_alpha);
      }
   }
}


// ==========================================================================
// draw a gouraud wall sprite
void wpreview_gouraud_w(ALLEGRO_BITMAP * tmp_bmp, int x0, int y0, int ds1_idx,
                        int c1, int c2, int c3, int c4)
{
   if (glb_ds1edit.cmd_line.force_pal_num == -1)
      ((void)0);
   else
      ((void)0);
   
   if ((c1 == c2) && (c2 == c3) && (c3 == c4))
   {
      if (c1 >= 248)
         wpreview_draw_bitmap(tmp_bmp, x0, y0);
      else
      {
         if (glb_ds1edit.cmd_line.force_pal_num == -1)
            ((void)0);
         else
            ((void)0);
         wpreview_draw_trans_bitmap(tmp_bmp, x0, y0, a5_trans_alpha);
      }
   }
   else
   {
      if (glb_ds1edit.night_mode == 1)
      {
         wpreview_draw_trans_bitmap(tmp_bmp, x0, y0, a5_trans_alpha);
      }
      else
      {
         wpreview_draw_trans_bitmap(tmp_bmp, x0, y0, a5_trans_alpha);
      }
   }
}


// ==========================================================================
// compute the distance between 2 points
int wpreview_light_dist(int x0, int y0, int mx, int my)
{
   double dx, dy;

   dx = mx - x0;
   dx *= dx;

   dy = (my - y0) * 2;
   dy *= dy;

   return ceil(sqrt(dx + dy));
}


// ==========================================================================
// helper of qsort, for tile drawing priority
int qsort_helper_order_data(const void *e1, const void *e2)
{
   ORDER_DATA_S * o1, * o2;

   o1 = (ORDER_DATA_S *) e1;
   o2 = (ORDER_DATA_S *) e2;

   // damn microsoft vc6 implementation of qsort() #=?*@!!
   if (o1->height == o2->height)
      return o1->idx - o2->idx; // else it reverse the original order !
      
   return o1->height - o2->height;
}


// ==========================================================================
// The draw passes sort at most 4 layers per tile; a tiny insertion sort is
// much cheaper here than calling qsort thousands of times per frame.
static void wpreview_sort_order_data(ORDER_DATA_S * order_data, int count)
{
   int i, j;
   ORDER_DATA_S key;

   for (i=1; i<count; i++)
   {
      key = order_data[i];
      j = i - 1;
      while (j >= 0)
      {
         if (order_data[j].height < key.height)
            break;
         if ((order_data[j].height == key.height) &&
             (order_data[j].idx <= key.idx))
            break;
         order_data[j + 1] = order_data[j];
         j--;
      }
      order_data[j + 1] = key;
   }
}


// ==========================================================================
// Convert absolute pixel coordinates to major tile coordinates without the
// mode-specific 5x scaling used for object/path sub-cells.
static void wpreview_coord_to_major_tile(int ds1_idx, int ax, int ay,
                                         int * layer_x, int * layer_y)
{
   int bx, by, cx, cy, rx, ry;

   if (ax < 0)
      ax -= glb_ds1[ds1_idx].tile_w;
   if (ay < 0)
      ay -= glb_ds1[ds1_idx].tile_h;

   bx = ax / glb_ds1[ds1_idx].tile_w;
   by = ay / glb_ds1[ds1_idx].tile_h;
   cx = bx  + by;
   cy = -bx + by;

   rx = ax % glb_ds1[ds1_idx].tile_w;
   ry = ay % glb_ds1[ds1_idx].tile_h;
   if (ax < 0) rx = glb_ds1[ds1_idx].tile_w + rx;
   if (ay < 0) ry = glb_ds1[ds1_idx].tile_h + ry;

   if (ry >= -rx / 2 + glb_ds1[ds1_idx].tile_w/2 + glb_ds1[ds1_idx].tile_h/2)     cx++;
   else if (ry < -rx / 2 + glb_ds1[ds1_idx].tile_h/2)                               cx--;
   else if (glb_ds1[ds1_idx].tile_w/2 - ry >=
            -rx / 2 + glb_ds1[ds1_idx].tile_w/2 + glb_ds1[ds1_idx].tile_h/2)        cy--;
   else if (glb_ds1[ds1_idx].tile_w/2 - ry < -rx / 2 + glb_ds1[ds1_idx].tile_h/2)   cy++;

   * layer_x = cx;
   * layer_y = cy;
}


// ==========================================================================
// Estimate the major-tile bounds that can contribute to the current view.
static void wpreview_get_visible_tile_bounds(int ds1_idx,
                                             int * min_x, int * max_x,
                                             int * min_y, int * max_y)
{
   int px, py, pw, ph;
   int corners[4][2];
   int i, cx, cy;
   int x_pad = 2;
   int y_pad = 4;

   px = glb_ds1edit.win_preview.x0;
   py = glb_ds1edit.win_preview.y0;
   pw = glb_ds1edit.win_preview.w;
   ph = glb_ds1edit.win_preview.h;

   corners[0][0] = px - glb_ds1[ds1_idx].tile_w;
   corners[0][1] = py - glb_ds1[ds1_idx].tile_h * y_pad;
   corners[1][0] = px + pw + glb_ds1[ds1_idx].tile_w;
   corners[1][1] = py - glb_ds1[ds1_idx].tile_h * y_pad;
   corners[2][0] = px - glb_ds1[ds1_idx].tile_w;
   corners[2][1] = py + ph + glb_ds1[ds1_idx].tile_h * y_pad;
   corners[3][0] = px + pw + glb_ds1[ds1_idx].tile_w;
   corners[3][1] = py + ph + glb_ds1[ds1_idx].tile_h * y_pad;

   * min_x = glb_ds1[ds1_idx].width - 1;
   * max_x = 0;
   * min_y = glb_ds1[ds1_idx].height - 1;
   * max_y = 0;

   for (i=0; i<4; i++)
   {
      wpreview_coord_to_major_tile(ds1_idx, corners[i][0], corners[i][1], &cx, &cy);
      if (cx < * min_x) * min_x = cx;
      if (cx > * max_x) * max_x = cx;
      if (cy < * min_y) * min_y = cy;
      if (cy > * max_y) * max_y = cy;
   }

   * min_x -= x_pad;
   * max_x += x_pad;
   * min_y -= y_pad;
   * max_y += y_pad;

   if (* min_x < 0)
      * min_x = 0;
   if (* min_y < 0)
      * min_y = 0;
   if (* max_x >= glb_ds1[ds1_idx].width)
      * max_x = glb_ds1[ds1_idx].width - 1;
   if (* max_y >= glb_ds1[ds1_idx].height)
      * max_y = glb_ds1[ds1_idx].height - 1;
}

     
// ==========================================================================
// draw shadow layer
void wpreview_draw_s(int ds1_idx, int x, int y, int mx, int my, int z,
                     int selected)
{
   BLOCK_TABLE_S * bt_ptr;
   ALLEGRO_BITMAP * tmp_bmp;
   CELL_S_S      * s_ptr;
   int           n, t, bt_idx, dt1_idx, block_idx, y1, color;
   int           ux1, ux2, ux3, ux4, uy1, uy2, uy3;


   t     = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
   s_ptr = glb_ds1[ds1_idx].shadow_buff + t;

   for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
   {
      color = -1;
      if (glb_ds1[ds1_idx].shadow_layer_mask[n] == 0)
         continue;

      if (IS_HIDE(s_ptr[n].flags))
         continue;

      bt_idx = s_ptr[n].bt_idx; // index in block table

      if (s_ptr[n].prop4 & 0x80) // binary : 1000-0000
           bt_idx = -1; // consider that tile as "unknown"

      if (bt_idx == -1)
      {
         ux1 = mx - glb_ds1edit.win_preview.x0;
         ux2 = ux1 + glb_ds1[ds1_idx].tile_w / 2 - 1;
         ux3 = ux1 + glb_ds1[ds1_idx].tile_w / 2;
         ux4 = ux1 + glb_ds1[ds1_idx].tile_w - 1;

         uy1 = my - glb_ds1edit.win_preview.y0;
         uy2 = uy1 + glb_ds1[ds1_idx].tile_h / 2 - 1;
         uy3 = uy1 + glb_ds1[ds1_idx].tile_h - 2;

         wpreview_draw_line(ux1, uy2, ux2, uy1, 168);
         wpreview_draw_line(ux3, uy1, ux4, uy2, 168);
         wpreview_draw_line(ux3, uy3, ux4, uy2, 168);
         wpreview_draw_line(ux1, uy2, ux2, uy3, 168);

         if (glb_ds1[ds1_idx].cur_zoom == ZM_11)
         {
            wpreview_textprintf(ux1+8, uy2-2, 0,
               "%u %u %u %u",
               s_ptr[n].prop1,
               s_ptr[n].prop2,
               s_ptr[n].prop3,
               s_ptr[n].prop4
            );
            wpreview_textprintf(ux1+7, uy2-3, 255,
               "%u %u %u %u",
               s_ptr[n].prop1,
               s_ptr[n].prop2,
               s_ptr[n].prop3,
               s_ptr[n].prop4
            );
         }
      }
      if (bt_idx <= 0)
         continue;

      bt_ptr = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table

      if (bt_ptr->type != BT_SHADOW)
         continue; // only shadows

      dt1_idx   = bt_ptr->dt1_idx;
      block_idx = bt_ptr->block_idx;

      tmp_bmp = * (glb_dt1[dt1_idx].block_zoom[z] + block_idx);

      if (tmp_bmp == NULL)
         continue;

      y1 = my - glb_ds1edit.win_preview.y0 -
           bt_ptr->zero_line * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div;
      y1 += glb_ds1[ds1_idx].tile_h; // shadow, like walls, are lower than floors
                                 //    (and than roofs) by 80 pixels

      if ( (y1 + al_get_bitmap_height(tmp_bmp)) < 0)
         continue;
      if (y1 >= glb_ds1edit.win_preview.h)
         continue;

      if (selected)
      {
         if (IS_COPY_INFO(s_ptr[n].flags))
         {
            color = COL_PASTE_OK;
            if (IS_COPY_STATE(s_ptr[n].flags))
               color = COL_PASTE_KO;
         }
         else if (IS_TMP_SELECT(s_ptr[n].flags))
            color = COL_TMPSEL_M;
         else if (IS_SELECTED(s_ptr[n].flags))
            color = COL_FLOOR_M;
         else
            color = COL_MOUSE;
      }
      else
      {
         if (IS_COPY_INFO(s_ptr[n].flags))
         {
            color = COL_PASTE_OK;
            if (IS_COPY_STATE(s_ptr[n].flags))
               color = COL_PASTE_KO;
         }
         else if (IS_TMP_SELECT(s_ptr[n].flags))
            color = COL_TMPSEL;
         else if (IS_SELECTED(s_ptr[n].flags))
            color = COL_FLOOR;
      }
      if ((color != -1) && (glb_ds1edit.mode != MOD_L))
      {
         wpreview_shape(tmp_bmp,
                        mx - glb_ds1edit.win_preview.x0, y1,
                        ds1_idx, color);
      }
      else
      {
         switch(glb_ds1[ds1_idx].shadow_layer_mask[0])
         {
            case 0 : // nothing to draw
               break;

            case 1 : // normal sprite
               wpreview_draw_bitmap(tmp_bmp,
                           mx - glb_ds1edit.win_preview.x0, y1);
               break;

            case 2 : // only 1 color, full white sprite
               wpreview_shape(tmp_bmp,
                              mx - glb_ds1edit.win_preview.x0, y1,
                              ds1_idx, COL_SHADOW);
               break;

            case 3 : // transparent (CM_TRANS = 50% blend)
               a5_trans_alpha = 0.50f;
               wpreview_draw_trans_bitmap(
                  tmp_bmp,
                  mx - glb_ds1edit.win_preview.x0,
                  y1,
                  a5_trans_alpha
               );
               break;
         }
      }
   }
}


// ==========================================================================
// prepare animated floor for a frame to draw
void wpreview_reiinit_animated_floor(int ds1_idx)
{
   BLOCK_TABLE_S * bt_ptr;
   int i;


   bt_ptr = glb_ds1[ds1_idx].block_table;
   for (i=0; i < glb_ds1[ds1_idx].bt_num; i++)
   {
      if (bt_ptr->type == BT_ANIMATED)
         bt_ptr->updated = FALSE;
      bt_ptr++;
   }
}


// ==========================================================================
// draw floor layer
void wpreview_draw_f(int ds1_idx, int x, int y, int mx, int my, int z, int selected)
{
   BLOCK_TABLE_S * bt_ptr;
   ALLEGRO_BITMAP        * tmp_bmp;
   CELL_F_S      * f_ptr;
   int           n, t, bt_idx, dt1_idx, block_idx, y1, color;
   ORDER_DATA_S  order_data[4];
   int           dist, c1, c2, c3, c4;
   int           ux1, ux2, ux3, ux4, uy1, uy2, uy3;


   t     = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
   f_ptr = glb_ds1[ds1_idx].floor_buff + t;

   // search the order in which to draw the 4 floor layers of this tile
   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
   {
      order_data[n].idx    = n;
      order_data[n].height = f_ptr[n].prop1;
   }
   wpreview_sort_order_data(order_data, glb_ds1[ds1_idx].floor_num);
   
   for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
   {
      color = -1;
      if (glb_ds1[ds1_idx].floor_layer_mask[order_data[n].idx] == 0)
         continue;
      
      if (IS_HIDE(f_ptr[order_data[n].idx].flags))
         continue;
      
      bt_idx = f_ptr[order_data[n].idx].bt_idx; // index in block table

      if (bt_idx == 0) // no tiles here
         continue;

      if (f_ptr[order_data[n].idx].prop4 & 0x80) // binary : 1000-0000
         bt_idx = -1; // consider that tile as "unknown"

      if (bt_idx == -1)
      {
         ux1 = mx - glb_ds1edit.win_preview.x0;
         ux2 = ux1 + glb_ds1[ds1_idx].tile_w / 2 - 1;
         ux3 = ux1 + glb_ds1[ds1_idx].tile_w / 2;
         ux4 = ux1 + glb_ds1[ds1_idx].tile_w - 1;

         uy1 = my - glb_ds1edit.win_preview.y0;
         uy2 = uy1 + glb_ds1[ds1_idx].tile_h / 2 - 1;
         uy3 = uy1 + glb_ds1[ds1_idx].tile_h - 2;
   
         wpreview_draw_line(ux1, uy2, ux2, uy1, 168);
         wpreview_draw_line(ux3, uy1, ux4, uy2, 168);
         wpreview_draw_line(ux3, uy3, ux4, uy2, 168);
         wpreview_draw_line(ux1, uy2, ux2, uy3, 168);

         if (glb_ds1[ds1_idx].cur_zoom == ZM_11)
         {
            wpreview_textprintf(ux1+8, uy2-2, 0,
               "%u %u %u %u",
               f_ptr[order_data[n].idx].prop1,
               f_ptr[order_data[n].idx].prop2,
               f_ptr[order_data[n].idx].prop3,
               f_ptr[order_data[n].idx].prop4
            );
            wpreview_textprintf(ux1+7, uy2-3, 255,
               "%u %u %u %u",
               f_ptr[order_data[n].idx].prop1,
               f_ptr[order_data[n].idx].prop2,
               f_ptr[order_data[n].idx].prop3,
               f_ptr[order_data[n].idx].prop4
            );
         }
      }
      if (bt_idx <= 0)
         continue;

      bt_ptr = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table

      if ( (bt_ptr->type != BT_STATIC) && (bt_ptr->type != BT_ANIMATED) )
         continue; // only floors

      if (bt_ptr->type == BT_ANIMATED)
      {
         // find the frame of an animated floor to draw
         if (bt_ptr->updated == FALSE)
         {
            bt_ptr->curr_frame =
               (glb_ds1[ds1_idx].cur_anim_floor_frame / 5) % (bt_ptr->rarity + 1);
            bt_ptr->updated = TRUE;
         }
         bt_ptr += bt_ptr->curr_frame - bt_ptr->rarity;
      }

      dt1_idx   = bt_ptr->dt1_idx;
      block_idx = bt_ptr->block_idx;

      tmp_bmp = * (glb_dt1[dt1_idx].block_zoom[z] + block_idx);

      if (tmp_bmp == NULL)
         continue;

      y1 = my - glb_ds1edit.win_preview.y0 -
           bt_ptr->zero_line * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div;
      if ( (y1 + al_get_bitmap_height(tmp_bmp)) < 0)
         continue;
      if (y1 >= glb_ds1edit.win_preview.h)
         continue;

      if (glb_ds1edit.mode == MOD_L)
      {
         // c1
         dist = wpreview_light_dist(
                   mx,
                   my,
                   glb_ds1edit.win_preview.x0 + a5_mouse_x,
                   glb_ds1edit.win_preview.y0 + a5_mouse_y
                );
         if (dist < 200)
            c1 = 255;
         else if (dist > 500)
            c1 = 0;
         else
            c1 = 255 - ((dist - 200) * 255 / (500 - 200));

         // c2
         dist = wpreview_light_dist(
                   mx + al_get_bitmap_width(tmp_bmp),
                   my,
                   glb_ds1edit.win_preview.x0 + a5_mouse_x,
                   glb_ds1edit.win_preview.y0 + a5_mouse_y
                );
         if (dist < 200)
            c2 = 255;
         else if (dist > 500)
            c2 = 0;
         else
            c2 = 255 - ((dist - 200) * 255 / (500 - 200));

         // c3
         dist = wpreview_light_dist(
                   mx + al_get_bitmap_width(tmp_bmp),
                   my + glb_ds1[ds1_idx].tile_h,
                   glb_ds1edit.win_preview.x0 + a5_mouse_x,
                   glb_ds1edit.win_preview.y0 + a5_mouse_y
                );
         if (dist < 200)
            c3 = 255;
         else if (dist > 500)
            c3 = 0;
         else
            c3 = 255 - ((dist - 200) * 255 / (500 - 200));

         // c4
         dist = wpreview_light_dist(
                   mx,
                   my + glb_ds1[ds1_idx].tile_h,
                   glb_ds1edit.win_preview.x0 + a5_mouse_x,
                   glb_ds1edit.win_preview.y0 + a5_mouse_y
                );
         if (dist < 200)
            c4 = 255;
         else if (dist > 500)
            c4 = 0;
         else
            c4 = 255 - ((dist - 200) * 255 / (500 - 200));

         // draw it
         wpreview_gouraud_f(tmp_bmp,
                            mx - glb_ds1edit.win_preview.x0, y1,
                            ds1_idx,
                            c1, c2, c3, c4);
      }
      else
      {
         if (selected)
         {
            if (IS_COPY_INFO(f_ptr[order_data[n].idx].flags))
            {
               color = COL_PASTE_OK;
               if (IS_COPY_STATE(f_ptr[order_data[n].idx].flags))
                  color = COL_PASTE_KO;
            }
            else if (IS_TMP_SELECT(f_ptr[order_data[n].idx].flags))
               color = COL_TMPSEL_M;
            else if (IS_SELECTED(f_ptr[order_data[n].idx].flags))
               color = COL_FLOOR_M;
            else
               color = COL_MOUSE;
         }
         else
         {
            if (IS_COPY_INFO(f_ptr[order_data[n].idx].flags))
            {
               color = COL_PASTE_OK;
               if (IS_COPY_STATE(f_ptr[order_data[n].idx].flags))
                  color = COL_PASTE_KO;
            }
            else if (IS_TMP_SELECT(f_ptr[order_data[n].idx].flags))
               color = COL_TMPSEL;
            else if (IS_SELECTED(f_ptr[order_data[n].idx].flags))
               color = COL_FLOOR;
         }

         if (color != -1)
         {
            wpreview_shape(tmp_bmp,
                           mx - glb_ds1edit.win_preview.x0, y1,
                           ds1_idx, color);
         }
         else
         {
            wpreview_draw_bitmap(tmp_bmp,
                        mx - glb_ds1edit.win_preview.x0, y1);
         }
      }
   }
}


// ==========================================================================
// draw wall layer
void wpreview_draw_w(int ds1_idx, int x, int y, int mx, int my, int z,
                     int selected, int upper)
{
   BLOCK_TABLE_S * bt_ptr;
   ALLEGRO_BITMAP        * tmp_bmp;
   CELL_W_S      * w_ptr;
   int           n, t, bt_idx, dt1_idx, block_idx, y1;
   int           o, color;
   ORDER_DATA_S  order_data[4];
   int           dist, c1=0, c2=0;
   int           ux1, ux2, ux3, ux4, uy1, uy2, uy3;
   

   t     = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
   w_ptr = glb_ds1[ds1_idx].wall_buff + t;
   
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      order_data[n].idx    = n;
      order_data[n].height = w_ptr[n].prop1;
      if (w_ptr[n].orientation == 10)
         order_data[n].height = 255;
   }
   wpreview_sort_order_data(order_data, glb_ds1[ds1_idx].wall_num);
   
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      color = -1;
      if (glb_ds1[ds1_idx].wall_layer_mask[order_data[n].idx] == 0)
         continue;
      
      if (IS_HIDE(w_ptr[order_data[n].idx].flags))
         continue;
      
      // draw only lower or upper wall, according to 'upper' param
      o = w_ptr[order_data[n].idx].orientation;
      if ((upper == TRUE) && (o >= 15))
         continue;
      if ((upper == FALSE) && (o <= 15))
         continue;

      bt_idx = w_ptr[order_data[n].idx].bt_idx; // index in block table

      if (bt_idx == 0) // no tiles here
         continue;
      
      if (glb_ds1[ds1_idx].special_layer_mask && ((o == 10) || (o == 11)))
      {
         // special tile asked to draw later
         continue;
      }
      
      if (w_ptr[order_data[n].idx].prop4 & 0x80) // binary : 1000-0000
      {
         // hidden
         if ((o != 10) && (o != 11))
            bt_idx = -1; // a hidden floor --> "unknown"
      }

      if (bt_idx == -1)
      {
         ux1 = mx - glb_ds1edit.win_preview.x0;
         ux2 = ux1 + glb_ds1[ds1_idx].tile_w / 2 - 1;
         ux3 = ux1 + glb_ds1[ds1_idx].tile_w / 2;
         ux4 = ux1 + glb_ds1[ds1_idx].tile_w - 1;

         uy1 = my - glb_ds1edit.win_preview.y0;
         uy2 = uy1 + glb_ds1[ds1_idx].tile_h / 2 - 1;
         uy3 = uy1 + glb_ds1[ds1_idx].tile_h - 2;
   
         wpreview_draw_line(ux1, uy2, ux2, uy1, 168);
         wpreview_draw_line(ux3, uy1, ux4, uy2, 168);
         wpreview_draw_line(ux3, uy3, ux4, uy2, 168);
         wpreview_draw_line(ux1, uy2, ux2, uy3, 168);
         
         if (glb_ds1[ds1_idx].cur_zoom == ZM_11)
         {
            wpreview_textprintf(ux1+8, uy2-2, 0,
               "(%u) %u %u %u %u",
               w_ptr[order_data[n].idx].orientation,
               w_ptr[order_data[n].idx].prop1,
               w_ptr[order_data[n].idx].prop2,
               w_ptr[order_data[n].idx].prop3,
               w_ptr[order_data[n].idx].prop4
            );
            wpreview_textprintf(ux1+7, uy2-3, 255,
               "(%u) %u %u %u %u",
               w_ptr[order_data[n].idx].orientation,
               w_ptr[order_data[n].idx].prop1,
               w_ptr[order_data[n].idx].prop2,
               w_ptr[order_data[n].idx].prop3,
               w_ptr[order_data[n].idx].prop4
            );
         }
         continue;
      }

      bt_ptr = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table
      if ( (bt_ptr->type != BT_WALL_UP)   &&
           (bt_ptr->type != BT_WALL_DOWN) &&
           (bt_ptr->type != BT_SPECIAL))
      {
         // only walls or special tiles, but no roof
         continue;
      }

      dt1_idx   = bt_ptr->dt1_idx;
      block_idx = bt_ptr->block_idx;

      tmp_bmp = * (glb_dt1[dt1_idx].block_zoom[z] + block_idx);

      if (tmp_bmp == NULL)
         continue;

      y1 = my - glb_ds1edit.win_preview.y0 -
           bt_ptr->zero_line * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div;
      y1 += glb_ds1[ds1_idx].tile_h; // walls are lower than floors (and than roofs) by 80 pixels
      if ( (y1 + al_get_bitmap_height(tmp_bmp)) < 0)
         continue;
      if (y1 >= glb_ds1edit.win_preview.h)
         continue;
      
      if (glb_ds1edit.mode == MOD_L)
      {
         // c1
         dist = wpreview_light_dist(
                   mx,
                   my + (glb_ds1[ds1_idx].tile_h / 2),
                   glb_ds1edit.win_preview.x0 + a5_mouse_x,
                   glb_ds1edit.win_preview.y0 + a5_mouse_y
                );
         if (dist < 200)
            c1 = 255;
         else if (dist > 500)
            c1 = 0;
         else
            c1 = 255 - ((dist - 200) * 255 / (500 - 200));

         // c2
         dist = wpreview_light_dist(
                   mx + al_get_bitmap_width(tmp_bmp),
                   my + (glb_ds1[ds1_idx].tile_h / 2),
                   glb_ds1edit.win_preview.x0 + a5_mouse_x,
                   glb_ds1edit.win_preview.y0 + a5_mouse_y
                );
         if (dist < 200)
            c2 = 255;
         else if (dist > 500)
            c2 = 0;
         else
            c2 = 255 - ((dist - 200) * 255 / (500 - 200));

         // draw it
         wpreview_gouraud_w(tmp_bmp,
                            mx - glb_ds1edit.win_preview.x0, y1,
                            ds1_idx,
                            c1, c2, c2, c1);
      }
      else
      {
         if (selected)
         {
            if (IS_COPY_INFO(w_ptr[order_data[n].idx].flags))
            {
               color = COL_PASTE_OK;
               if (IS_COPY_STATE(w_ptr[order_data[n].idx].flags))
                  color = COL_PASTE_KO;
            }
            else if (IS_TMP_SELECT(w_ptr[order_data[n].idx].flags))
               color = COL_TMPSEL_M;
            else if (IS_SELECTED(w_ptr[order_data[n].idx].flags))
               color = COL_WALL_M;
            else
               color = COL_MOUSE;
         }
         else
         {
            if (IS_COPY_INFO(w_ptr[order_data[n].idx].flags))
            {
               color = COL_PASTE_OK;
               if (IS_COPY_STATE(w_ptr[order_data[n].idx].flags))
                  color = COL_PASTE_KO;
            }
            else if (IS_TMP_SELECT(w_ptr[order_data[n].idx].flags))
               color = COL_TMPSEL;
            else if (IS_SELECTED(w_ptr[order_data[n].idx].flags))
               color = COL_WALL;
         }
        if (color != -1)
           wpreview_shape(tmp_bmp,
                          mx - glb_ds1edit.win_preview.x0, y1,
                          ds1_idx, color);
        else
            wpreview_draw_bitmap(tmp_bmp,
                        mx - glb_ds1edit.win_preview.x0, y1);
      }

      // // upper-left corner
      if (bt_ptr->orientation == 3)
      {
         if (bt_ptr->corner_pair_idx >= 0)
         {
            bt_ptr = glb_ds1[ds1_idx].block_table + bt_ptr->corner_pair_idx;
            dt1_idx   = bt_ptr->dt1_idx;
            block_idx = bt_ptr->block_idx;
            tmp_bmp   = * (glb_dt1[dt1_idx].block_zoom[z] + block_idx);

            if (tmp_bmp == NULL)
               continue;

            y1 = my - glb_ds1edit.win_preview.y0 -
                 bt_ptr->zero_line * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div;
            y1 += glb_ds1[ds1_idx].tile_h; // walls are lower than floors (and than roofs) by 80 pixels
            if ( (y1 + al_get_bitmap_height(tmp_bmp)) < 0)
               continue;
            if (y1 >= glb_ds1edit.win_preview.h)
               continue;
               
            if (glb_ds1edit.mode == MOD_L)
            {
               wpreview_gouraud_w(tmp_bmp,
                                  mx - glb_ds1edit.win_preview.x0, y1,
                                  ds1_idx,
                                  c1, c2, c2, c1);
            }
            else
            {
               if (color != -1)
                  wpreview_shape(tmp_bmp,
                                 mx - glb_ds1edit.win_preview.x0, y1,
                                 ds1_idx, color);
               else
                  wpreview_draw_bitmap(tmp_bmp,
                              mx - glb_ds1edit.win_preview.x0, y1);
            }
         }
      }
   }
}


// ==========================================================================
// draw roof layer
void wpreview_draw_r(int ds1_idx, int x, int y, int mx, int my, int z,
                     int selected)
{
   BLOCK_TABLE_S * bt_ptr;
   ALLEGRO_BITMAP        * tmp_bmp;
   CELL_W_S      * r_ptr;
   int           n, t, bt_idx, dt1_idx, block_idx, y1, color;
   ORDER_DATA_S  order_data[4];
   
   
   t     = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
   r_ptr = glb_ds1[ds1_idx].wall_buff + t;
   
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      order_data[n].idx    = n;
      order_data[n].height = r_ptr[n].prop1;
   }
   wpreview_sort_order_data(order_data, glb_ds1[ds1_idx].wall_num);
   
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      color = -1;
      if (glb_ds1[ds1_idx].wall_layer_mask[order_data[n].idx] == 0)
         continue;

      if (IS_HIDE(r_ptr[order_data[n].idx].flags))
         continue;
      
      bt_idx = r_ptr[order_data[n].idx].bt_idx; // index in block table

      if (bt_idx <= 0) // no tiles here
         continue;

      if (r_ptr[order_data[n].idx].prop4 & 0x80) // binary : 1000-0000
         bt_idx = -1; // consider this tile as "unknown"
      
      if (bt_idx <= 0)
         continue;

      bt_ptr = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table

      if (bt_ptr->type != BT_ROOF)
         continue; // only roof

      dt1_idx   = bt_ptr->dt1_idx;
      block_idx = bt_ptr->block_idx;

      tmp_bmp = * (glb_dt1[dt1_idx].block_zoom[z] + block_idx);

      if (tmp_bmp == NULL)
         continue;

      y1 = my - glb_ds1edit.win_preview.y0 -
           bt_ptr->zero_line * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div;

      // roof height
      y1 -= bt_ptr->roof_y * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div;
      
      if ( (y1 + al_get_bitmap_height(tmp_bmp)) < 0)
         continue;
      if (y1 >= glb_ds1edit.win_preview.h)
         continue;

      if (glb_ds1edit.mode == MOD_L)
      {
         // draw it
         if (glb_ds1edit.cmd_line.force_pal_num == -1)
            ((void)0);
         else
            ((void)0);
         wpreview_draw_trans_bitmap(
            tmp_bmp,
            mx - glb_ds1edit.win_preview.x0,
            y1,
            a5_trans_alpha
         );
      }
      else
      {
         if (selected)
         {
            if (IS_COPY_INFO(r_ptr[order_data[n].idx].flags))
            {
               color = COL_PASTE_OK;
               if (IS_COPY_STATE(r_ptr[order_data[n].idx].flags))
                  color = COL_PASTE_KO;
            }
            else if (IS_TMP_SELECT(r_ptr[order_data[n].idx].flags))
               color = COL_TMPSEL_M;
            else if (IS_SELECTED(r_ptr[order_data[n].idx].flags))
               color = COL_WALL_M;
            else
               color = COL_MOUSE;
         }
         else
         {
            if (IS_COPY_INFO(r_ptr[order_data[n].idx].flags))
            {
               color = COL_PASTE_OK;
               if (IS_COPY_STATE(r_ptr[order_data[n].idx].flags))
                  color = COL_PASTE_KO;
            }
            else if (IS_TMP_SELECT(r_ptr[order_data[n].idx].flags))
               color = COL_TMPSEL;
            else if (IS_SELECTED(r_ptr[order_data[n].idx].flags))
               color = COL_WALL;
         }
         if (color != -1)
            wpreview_shape(tmp_bmp,
                           mx - glb_ds1edit.win_preview.x0, y1,
                           ds1_idx, color);
         else
            wpreview_draw_bitmap(tmp_bmp,
                        mx - glb_ds1edit.win_preview.x0, y1);
      }
   }
}


// ==========================================================================
// draw special tiles
void wpreview_draw_sp(int ds1_idx, int x, int y, int mx, int my, int z,
                     int selected)
{
   BLOCK_TABLE_S * bt_ptr;
   ALLEGRO_BITMAP        * tmp_bmp;
   CELL_W_S      * w_ptr;
   int           n, t, bt_idx, dt1_idx, block_idx, y1, o;
   int           color;
   ORDER_DATA_S  order_data[4];
   int           dist, c1, c2;
   int           ux1, ux2, ux3, ux4, uy1, uy2, uy3;
   

   t     = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
   w_ptr = glb_ds1[ds1_idx].wall_buff + t;
   
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      order_data[n].idx    = n;
      order_data[n].height = w_ptr[n].prop1;
      if (w_ptr[n].orientation == 10)
         order_data[n].height = 255;
   }
   wpreview_sort_order_data(order_data, glb_ds1[ds1_idx].wall_num);
   
   for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
   {
      color = -1;
      if (glb_ds1[ds1_idx].wall_layer_mask[order_data[n].idx] == 0)
         continue;
      
      if (IS_HIDE(w_ptr[order_data[n].idx].flags))
         continue;
      
      bt_idx = w_ptr[order_data[n].idx].bt_idx; // index in block table

      if (bt_idx == 0) // no tile here
         continue;

      o = w_ptr[order_data[n].idx].orientation;

      if ((o != 10) && (o != 11))
      {
         // not a special tile
         continue;
      }
      else
      {
         // special tile

         if (bt_idx <= 0)
         {
            // unknown special tile
            ux1 = mx - glb_ds1edit.win_preview.x0;
            ux2 = ux1 + glb_ds1[ds1_idx].tile_w / 2 - 1;
            ux3 = ux1 + glb_ds1[ds1_idx].tile_w / 2;
            ux4 = ux1 + glb_ds1[ds1_idx].tile_w - 1;

            uy1 = my - glb_ds1edit.win_preview.y0;
            uy2 = uy1 + glb_ds1[ds1_idx].tile_h / 2 - 1;
            uy3 = uy1 + glb_ds1[ds1_idx].tile_h - 2;
   
            wpreview_draw_line(ux1, uy2, ux2, uy1, 168);
            wpreview_draw_line(ux3, uy1, ux4, uy2, 168);
            wpreview_draw_line(ux3, uy3, ux4, uy2, 168);
            wpreview_draw_line(ux1, uy2, ux2, uy3, 168);
         
            if (glb_ds1[ds1_idx].cur_zoom == ZM_11)
            {
               wpreview_textprintf(ux1+8, uy2-2, 0,
                  "(%u) %u %u %u %u",
                  w_ptr[order_data[n].idx].orientation,
                  w_ptr[order_data[n].idx].prop1,
                  w_ptr[order_data[n].idx].prop2,
                  w_ptr[order_data[n].idx].prop3,
                  w_ptr[order_data[n].idx].prop4
               );
               wpreview_textprintf(ux1+7, uy2-3, 255,
                  "(%u) %u %u %u %u",
                  w_ptr[order_data[n].idx].orientation,
                  w_ptr[order_data[n].idx].prop1,
                  w_ptr[order_data[n].idx].prop2,
                  w_ptr[order_data[n].idx].prop3,
                  w_ptr[order_data[n].idx].prop4
               );
            }
            continue;
         }
      }

      bt_ptr    = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table
      dt1_idx   = bt_ptr->dt1_idx;
      block_idx = bt_ptr->block_idx;

      tmp_bmp = * (glb_dt1[dt1_idx].block_zoom[z] + block_idx);

      if (tmp_bmp == NULL)
         continue;

      y1 = my - glb_ds1edit.win_preview.y0 -
           bt_ptr->zero_line * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div;
      y1 += glb_ds1[ds1_idx].tile_h; // walls are lower than floors (and than roofs) by 80 pixels
      if ( (y1 + al_get_bitmap_height(tmp_bmp)) < 0)
         continue;
      if (y1 >= glb_ds1edit.win_preview.h)
         continue;
      
      if (glb_ds1edit.mode == MOD_L)
      {
         // c1
         dist = wpreview_light_dist(
                   mx,
                   my + (glb_ds1[ds1_idx].tile_h / 2),
                   glb_ds1edit.win_preview.x0 + a5_mouse_x,
                   glb_ds1edit.win_preview.y0 + a5_mouse_y
                );
         if (dist < 200)
            c1 = 255;
         else if (dist > 500)
            c1 = 0;
         else
            c1 = 255 - ((dist - 200) * 255 / (500 - 200));

         // c2
         dist = wpreview_light_dist(
                   mx + al_get_bitmap_width(tmp_bmp),
                   my + (glb_ds1[ds1_idx].tile_h / 2),
                   glb_ds1edit.win_preview.x0 + a5_mouse_x,
                   glb_ds1edit.win_preview.y0 + a5_mouse_y
                );
         if (dist < 200)
            c2 = 255;
         else if (dist > 500)
            c2 = 0;
         else
            c2 = 255 - ((dist - 200) * 255 / (500 - 200));

         // draw it
         wpreview_gouraud_w(tmp_bmp,
                            mx - glb_ds1edit.win_preview.x0, y1,
                            ds1_idx,
                            c1, c2, c2, c1);
      }
      else
      {
         if (selected)
         {
            if (IS_COPY_INFO(w_ptr[order_data[n].idx].flags))
            {
               color = COL_PASTE_OK;
               if (IS_COPY_STATE(w_ptr[order_data[n].idx].flags))
                  color = COL_PASTE_KO;
            }
            else if (IS_TMP_SELECT(w_ptr[order_data[n].idx].flags))
               color = COL_TMPSEL_M;
            else if (IS_SELECTED(w_ptr[order_data[n].idx].flags))
               color = COL_WALL_M;
            else
               color = COL_MOUSE;
         }
         else
         {
            if (IS_COPY_INFO(w_ptr[order_data[n].idx].flags))
            {
               color = COL_PASTE_OK;
               if (IS_COPY_STATE(w_ptr[order_data[n].idx].flags))
                  color = COL_PASTE_KO;
            }
            else if (IS_TMP_SELECT(w_ptr[order_data[n].idx].flags))
               color = COL_TMPSEL;
            else if (IS_SELECTED(w_ptr[order_data[n].idx].flags))
               color = COL_WALL;
         }
         if (color != -1)
            wpreview_shape(tmp_bmp,
                           mx - glb_ds1edit.win_preview.x0, y1,
                           ds1_idx, color);
         else
            wpreview_draw_bitmap(tmp_bmp,
                        mx - glb_ds1edit.win_preview.x0, y1);
      }
   }
}


// ==========================================================================
// which tile (or sub-tile) is at this coordinates ?
void coord_to_tile(int ds1_idx, int ax, int ay, int * layer_x, int * layer_y)
{
   int bx, by, cx, cy, rx, ry;

   // ax & ay adjustement, depending of the current mode
   if ( (glb_ds1edit.mode == MOD_O) ||
        (glb_ds1edit.mode == MOD_P) ||
        (glb_ds1edit.mode == MOD_L) )
   {
      ax *= 5;
      ay *= 5;
   }
   
   // negative tile coordinates adjustment
   if (ax < 0)
      ax -= glb_ds1[ds1_idx].tile_w;
   if (ay < 0)
      ay -= glb_ds1[ds1_idx].tile_h;
      
   // search the Major tile's Coordinates
   bx = ax / glb_ds1[ds1_idx].tile_w;
   by = ay / glb_ds1[ds1_idx].tile_h;
   cx = bx  + by;
   cy = -bx + by;

   // fine adjustements (Minor tile's Coordinates)
   rx = ax % glb_ds1[ds1_idx].tile_w;
   ry = ay % glb_ds1[ds1_idx].tile_h;
   if (ax < 0) rx = glb_ds1[ds1_idx].tile_w + rx;
   if (ay < 0) ry = glb_ds1[ds1_idx].tile_h + ry;

   if (ry >= -rx / 2 + glb_ds1[ds1_idx].tile_w/2 + glb_ds1[ds1_idx].tile_h/2)     cx++;
   else if (ry < -rx / 2 + glb_ds1[ds1_idx].tile_h/2)                         cx--;
   else if (glb_ds1[ds1_idx].tile_w/2 - ry >=
            -rx / 2 + glb_ds1[ds1_idx].tile_w/2 + glb_ds1[ds1_idx].tile_h/2)      cy--;
   else if (glb_ds1[ds1_idx].tile_w/2 - ry < -rx / 2 + glb_ds1[ds1_idx].tile_h/2) cy++;

   // end
   * layer_x = cx;
   * layer_y = cy;
}


// ==========================================================================
// which tile (or sub-tile) is under the mouse ?
void mouse_to_tile(int ds1_idx, int * layer_x, int * layer_y)
{
   int ax, ay;

   // tile under mouse
   ax = glb_ds1edit.win_preview.x0 + a5_mouse_x;
   ay = glb_ds1edit.win_preview.y0 + a5_mouse_y;

   coord_to_tile(ds1_idx, ax, ay, layer_x, layer_y);
}


// ==========================================================================
// as expected, change zoom of 1 ds1
void change_zoom(int ds1_idx, ZOOM_E z)
{
   int mul, div, dx, dy, cx, cy;
   
   if (z < ZM_11)
      z = ZM_11;
   else if (z > ZM_116)
      z = ZM_116;
   
   dx = glb_ds1[ds1_idx].own_wpreview.x0 + glb_ds1[ds1_idx].own_wpreview.w / 2;
   dy = glb_ds1[ds1_idx].own_wpreview.y0 + glb_ds1[ds1_idx].own_wpreview.h / 2;
   coord_to_tile(ds1_idx, dx, dy, &cx, &cy);
   cx++; // don't know why, but it is needed to keep the zoom centered
   
   switch(z)
   {
      case ZM_11  : mul = 1, div =  1;  break;
      case ZM_12  : mul = 1, div =  2;  break;
      case ZM_14  : mul = 1, div =  4;  break;
      case ZM_18  : mul = 1, div =  8;  break;
      case ZM_116 : mul = 1, div = 16;  break;
      default     : mul = 1, div =  1;  break;
   }

   glb_ds1[ds1_idx].cur_zoom = z;
   glb_ds1[ds1_idx].height_mul = mul;
   glb_ds1[ds1_idx].height_div = div;
   glb_ds1[ds1_idx].tile_w = 160 * mul / div;
   glb_ds1[ds1_idx].tile_h =  80 * mul / div;
   if (glb_config.lower_speed_zoom_out)
   {
      glb_ds1[ds1_idx].cur_scroll.keyb.x = glb_config.scroll.keyb.x * mul / div;
      glb_ds1[ds1_idx].cur_scroll.keyb.y = glb_config.scroll.keyb.y * mul / div;
      glb_ds1[ds1_idx].cur_scroll.mouse.x = glb_config.scroll.mouse.x * mul / div;
      glb_ds1[ds1_idx].cur_scroll.mouse.y = glb_config.scroll.mouse.y * mul / div;
   }

   if (glb_ds1edit.mode == MOD_T)
   {
      dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
      dy = (cy *  glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);
   }
   else
   {
      dx = (cy * -glb_ds1[ds1_idx].tile_w / 10) + (cx * glb_ds1[ds1_idx].tile_w / 10);
      dy = (cy *  glb_ds1[ds1_idx].tile_h / 10) + (cx * glb_ds1[ds1_idx].tile_h / 10);
   }

   glb_ds1[ds1_idx].own_wpreview.x0 = dx - glb_ds1[ds1_idx].own_wpreview.w / 2;
   glb_ds1[ds1_idx].own_wpreview.y0 = dy - glb_ds1[ds1_idx].own_wpreview.h / 2;
}


// ==========================================================================
// draw paths lines
void wpreview_draw_paths(int ds1_idx)
{
   int x, y, dx, dy, o, p, x1, y1, x2, y2, color1, color2;
   
   for (o=0; o < glb_ds1[ds1_idx].obj_num; o++)
   {
      if (glb_ds1[ds1_idx].obj[o].path_num == 0)
         continue;

      // color of path lines
      if (IS_SELECTED(glb_ds1[ds1_idx].obj[o].flags) || IS_SELECTED(glb_ds1[ds1_idx].obj[o].label.flags))
      {
         color1 = 255; // white
         color2 =  10; // dark red
      }
      else
      {
         color1 = 219; // gray
         color2 = 155; // dark magenta
      }

      // npc position to 1st path

      if (glb_ds1[ds1_idx].obj[o].path_num)
      {
         // 1st point
         x = glb_ds1[ds1_idx].obj[o].x;
         y = glb_ds1[ds1_idx].obj[o].y;
      
         dx = ((y-2) * -glb_ds1[ds1_idx].tile_w / 10) + ((x+3) * glb_ds1[ds1_idx].tile_w / 10);
         dy = ((y-2) *  glb_ds1[ds1_idx].tile_h / 10) + ((x+3) * glb_ds1[ds1_idx].tile_h / 10);

         x1 = dx - glb_ds1edit.win_preview.x0;
         y1 = dy - glb_ds1edit.win_preview.y0 - 1;
         
         // 2nd point
         x = glb_ds1[ds1_idx].obj[o].path[0].x;
         y = glb_ds1[ds1_idx].obj[o].path[0].y;
      
         dx = ((y-2) * -glb_ds1[ds1_idx].tile_w / 10) + ((x+3) * glb_ds1[ds1_idx].tile_w / 10);
         dy = ((y-2) *  glb_ds1[ds1_idx].tile_h / 10) + ((x+3) * glb_ds1[ds1_idx].tile_h / 10);

         x2 = dx - glb_ds1edit.win_preview.x0;
         y2 = dy - glb_ds1edit.win_preview.y0 - 1;

         // line
         wpreview_draw_line(x1+1, y1+1, x2+1, y2+1, 0);
         wpreview_draw_line(x1, y1, x2, y2, color1);
      }
      
      // paths
      for (p=0; p < glb_ds1[ds1_idx].obj[o].path_num; p++)
      {
         // 1st point
         x = glb_ds1[ds1_idx].obj[o].path[p].x;
         y = glb_ds1[ds1_idx].obj[o].path[p].y;
      
         dx = ((y-2) * -glb_ds1[ds1_idx].tile_w / 10) + ((x+3) * glb_ds1[ds1_idx].tile_w / 10);
         dy = ((y-2) *  glb_ds1[ds1_idx].tile_h / 10) + ((x+3) * glb_ds1[ds1_idx].tile_h / 10);

         x1 = dx - glb_ds1edit.win_preview.x0;
         y1 = dy - glb_ds1edit.win_preview.y0 - 1;
         
         // 2nd point
         if (p == glb_ds1[ds1_idx].obj[o].path_num - 1)
         {
            x = glb_ds1[ds1_idx].obj[o].path[0].x;
            y = glb_ds1[ds1_idx].obj[o].path[0].y;
         }
         else
         {
            x = glb_ds1[ds1_idx].obj[o].path[p + 1].x;
            y = glb_ds1[ds1_idx].obj[o].path[p + 1].y;
         }
      
         dx = ((y-2) * -glb_ds1[ds1_idx].tile_w / 10) + ((x+3) * glb_ds1[ds1_idx].tile_w / 10);
         dy = ((y-2) *  glb_ds1[ds1_idx].tile_h / 10) + ((x+3) * glb_ds1[ds1_idx].tile_h / 10);

         x2 = dx - glb_ds1edit.win_preview.x0;
         y2 = dy - glb_ds1edit.win_preview.y0 - 1;

         // line
         wpreview_draw_line(x1+1, y1+1, x2+1, y2+1, 0);
         wpreview_draw_line(x1, y1, x2, y2, color2);

         // big point
         wpreview_draw_rectfill(x1, y1, x1+2, y1+2, 0);
         wpreview_draw_rectfill(x1-1, y1-1, x1+1, y1+1, 157);
      }
   }
}


// ==========================================================================
// draw paths lines of 1 object
void wpreview_draw_paths_1obj(int ds1_idx, int o)
{
   int x, y, dx, dy, p, x1, y1, x2, y2, color;
   

   if ((o < 0) || (o >= glb_ds1[ds1_idx].current_obj_max))
      return;

   if (glb_ds1[ds1_idx].obj[o].path_num == 0)
         return;

   // npc position to 1st path
   if (glb_ds1[ds1_idx].obj[o].path_num)
   {
      // 1st point
      x = glb_ds1[ds1_idx].obj[o].x;
      y = glb_ds1[ds1_idx].obj[o].y;
   
      dx = ((y-2) * -glb_ds1[ds1_idx].tile_w / 10) + ((x+3) * glb_ds1[ds1_idx].tile_w / 10);
      dy = ((y-2) *  glb_ds1[ds1_idx].tile_h / 10) + ((x+3) * glb_ds1[ds1_idx].tile_h / 10);

      x1 = dx - glb_ds1edit.win_preview.x0;
      y1 = dy - glb_ds1edit.win_preview.y0 - 1;
      
      // 2nd point
      x = glb_ds1[ds1_idx].obj[o].path[0].x;
      y = glb_ds1[ds1_idx].obj[o].path[0].y;
   
      dx = ((y-2) * -glb_ds1[ds1_idx].tile_w / 10) + ((x+3) * glb_ds1[ds1_idx].tile_w / 10);
      dy = ((y-2) *  glb_ds1[ds1_idx].tile_h / 10) + ((x+3) * glb_ds1[ds1_idx].tile_h / 10);

      x2 = dx - glb_ds1edit.win_preview.x0;
      y2 = dy - glb_ds1edit.win_preview.y0 - 1;

      // line
      color = 219; // grey
      wpreview_draw_line(x1+1, y1+1, x2+1, y2+1, 0);
      wpreview_draw_line(x1, y1, x2, y2, color);
   }
      
   // paths
   for (p=0; p < glb_ds1[ds1_idx].obj[o].path_num; p++)
   {
      // 1st point
      x = glb_ds1[ds1_idx].obj[o].path[p].x;
      y = glb_ds1[ds1_idx].obj[o].path[p].y;
   
      dx = ((y-2) * -glb_ds1[ds1_idx].tile_w / 10) + ((x+3) * glb_ds1[ds1_idx].tile_w / 10);
      dy = ((y-2) *  glb_ds1[ds1_idx].tile_h / 10) + ((x+3) * glb_ds1[ds1_idx].tile_h / 10);

      x1 = dx - glb_ds1edit.win_preview.x0;
      y1 = dy - glb_ds1edit.win_preview.y0 - 1;
      
      // 2nd point
      if (p == glb_ds1[ds1_idx].obj[o].path_num - 1)
      {
         x = glb_ds1[ds1_idx].obj[o].path[0].x;
         y = glb_ds1[ds1_idx].obj[o].path[0].y;
      }
      else
      {
         x = glb_ds1[ds1_idx].obj[o].path[p + 1].x;
         y = glb_ds1[ds1_idx].obj[o].path[p + 1].y;
      }
   
      dx = ((y-2) * -glb_ds1[ds1_idx].tile_w / 10) + ((x+3) * glb_ds1[ds1_idx].tile_w / 10);
      dy = ((y-2) *  glb_ds1[ds1_idx].tile_h / 10) + ((x+3) * glb_ds1[ds1_idx].tile_h / 10);

      x2 = dx - glb_ds1edit.win_preview.x0;
      y2 = dy - glb_ds1edit.win_preview.y0 - 1;

      // line
      color = 155; // dark magenta
      wpreview_draw_line(x1+1, y1+1, x2+1, y2+1, 0);
      wpreview_draw_line(x1, y1, x2, y2, color);

      // big point
      wpreview_draw_rectfill(x1, y1, x1+2, y1+2, 0);
      wpreview_draw_rectfill(x1-1, y1-1, x1+1, y1+1, 157);
   }
}


// ==========================================================================
void wpreview_obj_animate(void)
{
   TXT_S     * txt = glb_ds1edit.obj_buff;
   COF_S     * cof;
   int       o;
   long      n, nb_ticks = glb_ds1edit.ticks_elapsed;


   glb_ds1edit.ticks_elapsed = 0;
   
   if (txt == NULL)
      return;

   // search frame number to draw for all TYPE of object
   for (o=0; o < txt->line_num; o++)
   {
      if (glb_ds1edit.obj_desc[o].usage_count)
      {
         cof = glb_ds1edit.obj_desc[o].cof;
         if (cof == NULL)
            continue;
         if (cof->fpd == 0)
            continue;
         if (cof->spd_div == 0)
            continue;
         n = nb_ticks * cof->spd_mul + cof->spd_mod;
         cof->cur_frame += n / cof->spd_div;
         cof->spd_mod    = n % cof->spd_div;
         while (cof->cur_frame >= cof->fpd)
            cof->cur_frame -= cof->fpd;
      }
   }
}


// ==========================================================================
// draw objects while in Tile mode (just simple text)
void wpreview_draw_objects(int ds1_idx)
{
   int  dx, dy, o, x1, y1, d;
   long type, id, x, y;


   for (o=0; o < glb_ds1[ds1_idx].obj_num; o++)
   {
      type = glb_ds1[ds1_idx].obj[o].type;
      id   = glb_ds1[ds1_idx].obj[o].id;
      x    = glb_ds1[ds1_idx].obj[o].x;
      y    = glb_ds1[ds1_idx].obj[o].y;
      d    = glb_ds1[ds1_idx].obj[o].desc_idx;

      dx = ((y-2) * -glb_ds1[ds1_idx].tile_w / 10) + ((x+3) * glb_ds1[ds1_idx].tile_w / 10);
      dy = ((y-2) *  glb_ds1[ds1_idx].tile_h / 10) + ((x+3) * glb_ds1[ds1_idx].tile_h / 10);

      if (glb_ds1[ds1_idx].objects_layer_mask == OL_DESC)
      {
         // draw name, except if "?"
         if (d != -1)
         {
            if (glb_ds1edit.obj_desc[d].desc[0] == '?')
               d = -1;
            else
            {
               // draw name
               x1 = dx - glb_ds1edit.win_preview.x0 -
                  (8 * strlen(glb_ds1edit.obj_desc[d].desc) / 2);
               y1 = dy - glb_ds1edit.win_preview.y0 - 4;

               wpreview_textprintf(x1+1, y1+1, 0, "%s",
                  glb_ds1edit.obj_desc[d].desc);

               wpreview_textprintf(x1, y1, 255, "%s",
                 glb_ds1edit.obj_desc[d].desc);
            }
         }
      }

      if ((glb_ds1[ds1_idx].objects_layer_mask == OL_TYPEID) || (d == -1))
      {
         // draw Type-Id
         x1 = dx - glb_ds1edit.win_preview.x0 - 20;
         y1 = dy - glb_ds1edit.win_preview.y0 - 4;

         wpreview_textprintf(x1+1, y1+1, 0, "%i,%3i",
           glb_ds1[ds1_idx].obj[o].type, glb_ds1[ds1_idx].obj[o].id);

         wpreview_textprintf(x1, y1, 11, "%i",
           glb_ds1[ds1_idx].obj[o].type);

         wpreview_textprintf(x1+8, y1, 255, ",%3i",
            glb_ds1[ds1_idx].obj[o].id);
      }
   }
}


// ==========================================================================
// draw simple walkable infos of 1 cell (only walk & jump infos)
void wpreview_draw_simple_wi(int mx, int my, int z, UBYTE * walkinfo)
{
   int i;
   
   for (i=0; i<25; i++)
   {
      if (walkinfo[i] & 0x04)
      {
         wpreview_draw_bitmap(
            glb_ds1edit.subtile_nojump[z][i],
            mx - glb_ds1edit.win_preview.x0,
            my - glb_ds1edit.win_preview.y0
         );
      }
      else if (walkinfo[i] & 0x09)
      {
         wpreview_draw_bitmap(
            glb_ds1edit.subtile_nowalk[z][i],
            mx - glb_ds1edit.win_preview.x0,
            my - glb_ds1edit.win_preview.y0
         );
      }
   }
}


// ==========================================================================
// draw accurate walkable infos of 1 cell (all flags appear)
void wpreview_draw_wi(int mx, int my, int z, UBYTE * walkinfo)
{
   int i;
   
   for (i=0; i<25; i++)
   {
      if (walkinfo[i])
      {
         wpreview_draw_bitmap(
            glb_ds1edit.subtile_flag_combination[walkinfo[i]][z][i],
            mx - glb_ds1edit.win_preview.x0,
            my - glb_ds1edit.win_preview.y0
         );
      }
   }
}


// ==========================================================================
void wpreview_draw_an_object(int ds1_idx, int o)
{
   COF_S     * cof;
   LAY_INF_S * lay;
   ALLEGRO_BITMAP    ** bmp_ptr, * bmp;
   int       c, f, x, y, d, dx0, dy0, dx, dy, p, col_black, col_white;
   UBYTE     * bptr;
   UBYTE     new_frame;

   
   d = glb_ds1[ds1_idx].obj[o].desc_idx;

   if (d == -1)
      return;
         
   if (glb_ds1edit.obj_desc[d].usage_count == 0)
      return;

   cof = glb_ds1edit.obj_desc[d].cof;
   if (cof == NULL)
      return;
   if (cof->fpd == 0)
      return;

   new_frame = cof->cur_frame + glb_ds1[ds1_idx].obj[o].frame_delta;
   while (new_frame >= cof->fpd)
      new_frame -= cof->fpd;

   x   = glb_ds1[ds1_idx].obj[o].x;
   y   = glb_ds1[ds1_idx].obj[o].y;
   dx0 = ((y-2) * -glb_ds1[ds1_idx].tile_w / 10) + ((x+3) * glb_ds1[ds1_idx].tile_w / 10);
   dy0 = 4 + ((y-2) *  glb_ds1[ds1_idx].tile_h / 10) + ((x+3) * glb_ds1[ds1_idx].tile_h / 10);

   col_black = 0;   /* palette index for black */
   col_white = 255; /* palette index for white */

   // body
   for (c=0; c < cof->lay; c++)
   {
      p  = (cof->cur_dir * cof->lay * cof->fpd);
	  p += c + (new_frame * cof->lay);
      p  = cof->priority[p];
      if (p >= COMPOSIT_NB)
         continue;
         
      lay = & cof->lay_inf[p];
      if (lay == NULL)
         continue;
      bmp_ptr = lay->bmp;
      if (bmp_ptr == NULL)
         continue;
      if (new_frame >= lay->bmp_num)
         f = lay->last_good_frame;
      else
         f = new_frame;
      bmp = bmp_ptr[f];
      if (bmp == NULL)
         continue;
      dx = dx0 - glb_ds1edit.win_preview.x0 + cof->xoffset + lay->off_x;
      dy = dy0 - glb_ds1edit.win_preview.y0 + cof->yoffset + lay->off_y;
      if (((dx + al_get_bitmap_width(bmp)) < 0) || ((dy + al_get_bitmap_height(bmp)) < 0))
         continue;
      if ( (dx >= glb_ds1edit.win_preview.w) ||
           (dy >= glb_ds1edit.win_preview.h))
         continue;

      if ((glb_ds1[ds1_idx].cur_zoom == ZM_11) || (glb_config.stretch_sprites != TRUE))
      {
         // normal drawing
         if ((lay->trans_a) && (lay->trans_b <= 6) && (lay->trans_b != 5))
         {
            wpreview_draw_blended_bitmap(bmp, dx, dy, lay->trans_b);
         }
         else
         {
            // normal colors
            wpreview_draw_bitmap(bmp, dx, dy);
         }
      }
      else
      {
         // stretch drawing
         dx = dx0 - glb_ds1edit.win_preview.x0 +
              (cof->xoffset * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div) +
              (lay->off_x * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div);

         dy = dy0 - glb_ds1edit.win_preview.y0 +
              (cof->yoffset * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div) +
              (lay->off_y * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div);

         if ((lay->trans_a) && (lay->trans_b <= 6) && (lay->trans_b != 5))
         {
            wpreview_draw_scaled_blended_bitmap(
               bmp, dx, dy, glb_ds1[ds1_idx].height_div, lay->trans_b
            );
         }
         else
         {
            // normal colors — opaque scaled drawing
            wpreview_draw_scaled_blended_bitmap(
               bmp, dx, dy, glb_ds1[ds1_idx].height_div, -1
            );
         }
      }

      // end
      if (new_frame < lay->bmp_num)
         lay->last_good_frame = f;
   }

   // if Object editing mode, no speed info
   if (glb_ds1edit.mode == MOD_O)
      return;
   
   // speed info
   if (glb_ds1[ds1_idx].objects_layer_mask == OL_SPEED)
   {
      dx = dx0 - glb_ds1edit.win_preview.x0 - 44;
      dy = dy0 - glb_ds1edit.win_preview.y0 - 8; // + cof->yoffset;

      wpreview_textprintf(dx+1, dy+1, col_black,
         "speed = %3li", cof->spd_mul);

      wpreview_textprintf(dx, dy, col_white,
         "speed = %3li", cof->spd_mul);
   }
}


// ==========================================================================
void wpreview_draw_an_object_shad(int ds1_idx, int o)
{
   COF_S     * cof;
   LAY_INF_S * lay;
   ALLEGRO_BITMAP    ** bmp_ptr, * bmp;
   int       c, f, x, y, d, dx0, dy0, dx, dy, p, offx, offy;
   UBYTE     * bptr;
   UBYTE     new_frame;
   
   d = glb_ds1[ds1_idx].obj[o].desc_idx;

   if (d == -1)
      return;
         
   if (glb_ds1edit.obj_desc[d].usage_count == 0)
      return;

   cof = glb_ds1edit.obj_desc[d].cof;
   if (cof == NULL)
      return;
   if (cof->fpd == 0)
      return;
         
   new_frame = cof->cur_frame + glb_ds1[ds1_idx].obj[o].frame_delta;
   while (new_frame >= cof->fpd)
      new_frame -= cof->fpd;

   x   = glb_ds1[ds1_idx].obj[o].x;
   y   = glb_ds1[ds1_idx].obj[o].y;

   dx0 = ((y-2) * -glb_ds1[ds1_idx].tile_w / 10) + ((x+3) * glb_ds1[ds1_idx].tile_w / 10);
   dy0 = 4 + ((y-2) *  glb_ds1[ds1_idx].tile_h / 10) + ((x+3) * glb_ds1[ds1_idx].tile_h / 10);

   // only projected shadow
   for (c=0; c < cof->lay; c++)
   {
      p  = (cof->cur_dir * cof->lay * cof->fpd);
      p += c + (new_frame * cof->lay);
      p  = cof->priority[p];
      if (p >= COMPOSIT_NB)
         continue;
         
      lay = & cof->lay_inf[p];
      if (lay == NULL)
         continue;

      // shadow for this layer ?
      if (lay->shad_a == 0)
         continue;

      bmp_ptr = lay->bmp;
      if (bmp_ptr == NULL)
         continue;
      if (new_frame >= lay->bmp_num)
         f = lay->last_good_frame;
      else
         f = new_frame;
      bmp = bmp_ptr[f];
      if (bmp == NULL)
         continue;
      dx = dx0 - glb_ds1edit.win_preview.x0 + cof->xoffset + lay->off_x;
      dy = dy0 - glb_ds1edit.win_preview.y0 + cof->yoffset + lay->off_y;
      if (((dx + al_get_bitmap_width(bmp)) < 0) || ((dy + al_get_bitmap_height(bmp)) < 0))
         continue;
      if ( (dx >= glb_ds1edit.win_preview.w) ||
           (dy >= glb_ds1edit.win_preview.h))
         continue;

      // Shadow rendering using D2 darkness levels (0-32 scale from D2CMP)
      // Transparent layers get a faint shadow (level 28), opaque layers get a dark one (level 10)
      {
         int shadow_level = lay->trans_a ? 28 : 10;

         if ((glb_ds1[ds1_idx].cur_zoom == ZM_11) || (glb_config.stretch_sprites != TRUE))
         {
            offx = cof->xoffset + lay->off_x;
            offy = cof->yoffset + lay->off_y;

            dx = dx0 - glb_ds1edit.win_preview.x0 + offx;
            dy = dy0 - glb_ds1edit.win_preview.y0 + offy;

            a5_draw_shadow_sprite(glb_ds1edit.screen_buff, bmp, dx, dy, shadow_level);
         }
         else
         {
            offx = (cof->xoffset * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div) +
                   (lay->off_x * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div);

            offy = (cof->yoffset * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div) +
                   (lay->off_y * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div);

            dx = dx0 - glb_ds1edit.win_preview.x0 + offx;
            dy = dy0 - glb_ds1edit.win_preview.y0 + offy;

            a5_draw_scaled_shadow_sprite(
               glb_ds1edit.screen_buff, bmp, dx, dy,
               glb_ds1[ds1_idx].height_div, shadow_level
            );
         }
      }
   }
}


// ==========================================================================
void wpreview_draw_obj_tile_shad(int ds1_idx, int x, int y, int * cur_idx)
{
   OBJ_S * obj;
   int   o;


   // 1st quick check
   if ((* cur_idx) >= glb_ds1[ds1_idx].obj_num)
      return;
         
   // go on 1st potential object to draw
   o   = glb_ds1[ds1_idx].drawing_order[* cur_idx];
   obj = & glb_ds1[ds1_idx].obj[o];
   while (obj->ty < y) // search line
   {
      (* cur_idx) ++;
      if ( (* cur_idx) >= glb_ds1[ds1_idx].obj_num )
         return;
      o   = glb_ds1[ds1_idx].drawing_order[* cur_idx];
      obj = & glb_ds1[ds1_idx].obj[o];
   }
   if (obj->ty == y)
   {
      while (obj->tx < x) // search column
      {
         (* cur_idx) ++;
         if ( (* cur_idx) >= glb_ds1[ds1_idx].obj_num )
            return;
         o   = glb_ds1[ds1_idx].drawing_order[* cur_idx];
         obj = & glb_ds1[ds1_idx].obj[o];
      }
   }
   else
      return;
   
   // draw all objects on this tile, if any
   for (;;)
   {
      if ((obj->tx == x) && (obj->ty == y))
      {
         // draw this obj
         wpreview_draw_an_object_shad(ds1_idx, o);

         // next obj
         (* cur_idx) ++;
         if ( (* cur_idx) >= glb_ds1[ds1_idx].obj_num)
            return;
         o   = glb_ds1[ds1_idx].drawing_order[* cur_idx];
         obj = & glb_ds1[ds1_idx].obj[o];
      }
      else
         return;
   }
}


// ==========================================================================
void wpreview_draw_obj_tile_0_2(int ds1_idx, int x, int y, int * cur_idx)
{
   OBJ_S * obj;
   int   o, d;
   COF_S * cof;


   // 1st quick check
   if ((* cur_idx) >= glb_ds1[ds1_idx].obj_num)
      return;
         
   // go on 1st potential object to draw
   o   = glb_ds1[ds1_idx].drawing_order[* cur_idx];
   obj = & glb_ds1[ds1_idx].obj[o];
   while (obj->ty < y) // search line
   {
      (* cur_idx) ++;
      if ( (* cur_idx) >= glb_ds1[ds1_idx].obj_num )
         return;
      o   = glb_ds1[ds1_idx].drawing_order[* cur_idx];
      obj = & glb_ds1[ds1_idx].obj[o];
   }
   if (obj->ty == y)
   {
      while (obj->tx < x) // search column
      {
         (* cur_idx) ++;
         if ( (* cur_idx) >= glb_ds1[ds1_idx].obj_num )
            return;
         o   = glb_ds1[ds1_idx].drawing_order[* cur_idx];
         obj = & glb_ds1[ds1_idx].obj[o];
      }
   }
   else
      return;
   
   // draw all objects on this tile, if any
   for (;;)
   {
      if ((obj->tx == x) && (obj->ty == y))
      {
         // draw this obj
         d = glb_ds1[ds1_idx].obj[o].desc_idx;
         if (d != -1)
         {
            if (glb_ds1edit.obj_desc[d].usage_count)
            {
               cof = glb_ds1edit.obj_desc[d].cof;
               if (cof != NULL)
               {
                  if ((cof->orderflag == 0) || (cof->orderflag == 2))
                     wpreview_draw_an_object(ds1_idx, o);
               }
            }
         }

         // next obj
         (* cur_idx) ++;
         if ( (* cur_idx) >= glb_ds1[ds1_idx].obj_num)
            return;
         o   = glb_ds1[ds1_idx].drawing_order[* cur_idx];
         obj = & glb_ds1[ds1_idx].obj[o];
      }
      else
         return;
   }
}


// ==========================================================================
void wpreview_draw_obj_tile_1(int ds1_idx, int x, int y, int * cur_idx)
{
   OBJ_S * obj;
   int   o, d;
   COF_S * cof;


   // 1st quick check
   if ((* cur_idx) >= glb_ds1[ds1_idx].obj_num)
      return;
         
   // go on 1st potential object to draw
   o   = glb_ds1[ds1_idx].drawing_order[* cur_idx];
   obj = & glb_ds1[ds1_idx].obj[o];
   while (obj->ty < y) // search line
   {
      (* cur_idx) ++;
      if ( (* cur_idx) >= glb_ds1[ds1_idx].obj_num )
         return;
      o   = glb_ds1[ds1_idx].drawing_order[* cur_idx];
      obj = & glb_ds1[ds1_idx].obj[o];
   }
   if (obj->ty == y)
   {
      while (obj->tx < x) // search column
      {
         (* cur_idx) ++;
         if ( (* cur_idx) >= glb_ds1[ds1_idx].obj_num )
            return;
         o   = glb_ds1[ds1_idx].drawing_order[* cur_idx];
         obj = & glb_ds1[ds1_idx].obj[o];
      }
   }
   else
      return;
   
   // draw all objects on this tile, if any
   for(;;)
   {
      if ((obj->tx == x) && (obj->ty == y))
      {
         // draw this obj
         d = glb_ds1[ds1_idx].obj[o].desc_idx;
         if (d != -1)
         {
            if (glb_ds1edit.obj_desc[d].usage_count)
            {
               cof = glb_ds1edit.obj_desc[d].cof;
               if (cof != NULL)
               {
                  if (cof->orderflag == 1)
                     wpreview_draw_an_object(ds1_idx, o);
               }
            }
         }

         // next obj
         (* cur_idx) ++;
         if ( (* cur_idx) >= glb_ds1[ds1_idx].obj_num)
            return;
         o   = glb_ds1[ds1_idx].drawing_order[* cur_idx];
         obj = & glb_ds1[ds1_idx].obj[o];
      }
      else
         return;
   }
}


// ==========================================================================
// draw the tile grid for this ds1
void wpreview_draw_tile_grid(int ds1_idx)
{
   int tw, th, dw, dh, px, py, base_x, base_y, mx, my, c, i, x1, y1, x2, y2;
   int min_x, min_y, max_x, max_y, dummy;


   tw = glb_ds1[ds1_idx].tile_w;
   th = glb_ds1[ds1_idx].tile_h;
   dw = glb_ds1[ds1_idx].width;
   dh = glb_ds1[ds1_idx].height;
   px = glb_ds1edit.win_preview.x0;
   py = glb_ds1edit.win_preview.y0;
   c  = 255; /* palette index for white */

   base_x = tw/2 - px - 1;
   base_y = - py - 1;
   mx     = base_x - (tw * dh / 2);
   my     = base_y + (th * dh / 2);

   x1 = px;
   y1 = py;
   coord_to_tile(ds1_idx, x1, y1, & min_x, & dummy);

   x1 = px + glb_config.screen.width;
   coord_to_tile(ds1_idx, x1, y1, & dummy, & min_y);

   x1 = px;
   y1 = py + glb_config.screen.height;
   coord_to_tile(ds1_idx, x1, y1, & dummy, & max_y);

   x1 = px + glb_config.screen.width;
   coord_to_tile(ds1_idx, x1, y1, & max_x, & dummy);

   if ( (glb_ds1edit.mode == MOD_O) ||
        (glb_ds1edit.mode == MOD_P) ||
        (glb_ds1edit.mode == MOD_L) )
   {
      min_x /= 5;
      min_y /= 5;
      max_x /= 5;
      max_y /= 5;
   }
   if (min_x < 0)                   min_x = 0;
   if (min_y < 0)                   min_y = 0;
   if (max_x > glb_ds1[ds1_idx].width)  max_x = glb_ds1[ds1_idx].width;
   if (max_y > glb_ds1[ds1_idx].height) max_y = glb_ds1[ds1_idx].height;

//   for (i=0; i <= dw ; i++)
   for (i=min_x; i <= max_x ; i++)
   {
      x1 = base_x + i * tw / 2;
      y1 = base_y + i * th / 2;
      x2 = mx + i * tw / 2;
      y2 = my + i * th / 2;
      wpreview_draw_line(x1, y1, x2, y2, c);
   }

   base_x = tw/2 - px - 1;
   base_y = - py - 1;
   mx     = base_x + (tw * dw / 2);
   my     = base_y + (th * dw / 2);
   for (i=min_y; i <= max_y ; i++)
   {
      x1 = base_x - i * tw / 2;
      y1 = base_y + i * th / 2;
      x2 = mx - i * tw / 2;
      y2 = my + i * th / 2;
      wpreview_draw_line(x1, y1, x2, y2, c);
   }
}


// ==========================================================================
// draw all tiles of this ds1
void wpreview_draw_tiles(int ds1_idx)
{
   int               x, y, base_x = 0, base_y = 0, mx, my;
   int               cx, cy, dx, dy, z, x1, x2, x3, x4, y1, y2, y3, i, select;
   int               min_tile_x, max_tile_x, min_tile_y, max_tile_y;
   char              * mode;
   UBYTE             walkinfo[25];
   int               objdraw_cur_idx = 0;
   void              (* fptr_wi) (int, int, int, UBYTE *);
   PATH_EDIT_WIN_S   * pwin = & glb_ds1[ds1_idx].path_edit_win;
   double            perf_total_start_ms, perf_section_start_ms, perf_call_start_ms;


   z = glb_ds1[ds1_idx].cur_zoom;
   perf_total_start_ms = render_perf_now_ms();
   perf_section_start_ms = render_perf_now_ms();
   a5_clear(glb_ds1edit.screen_buff);
   render_perf_add(
      &glb_render_perf_stats.clear_ms_total,
      &glb_render_perf_stats.clear_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // handle palette
   perf_section_start_ms = render_perf_now_ms();
   if (glb_ds1edit.cmd_line.force_pal_num == -1)
   {
      // use .ds1 act value for palette
      if (wpreview_old_pal != glb_ds1[ds1_idx].act - 1)
      {
         wpreview_old_pal = glb_ds1[ds1_idx].act - 1;
         a5_current_palette = &glb_ds1edit.vga_pal[wpreview_old_pal];
         dt1_rebuild_bitmaps_from_cache(a5_current_palette);
      }
   }
   else
   {
      // use force_pal value for palette
      if (wpreview_old_pal != glb_ds1edit.cmd_line.force_pal_num - 1)
      {
         wpreview_old_pal = glb_ds1edit.cmd_line.force_pal_num - 1;
         a5_current_palette = &glb_ds1edit.vga_pal[wpreview_old_pal];
         dt1_rebuild_bitmaps_from_cache(a5_current_palette);
      }
   }
   render_perf_add(
      &glb_render_perf_stats.palette_ms_total,
      &glb_render_perf_stats.palette_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // which tile is under the mouse ?
   perf_section_start_ms = render_perf_now_ms();
   mouse_to_tile(ds1_idx, &cx, &cy);
   if (glb_ds1edit.mode == MOD_T)
   {
      if (cx < 0)
         cx = 0;
      else if (cx >= glb_ds1[ds1_idx].width)
         cx = glb_ds1[ds1_idx].width - 1;
      if (cy < 0)
         cy = 0;
      else if (cy >= glb_ds1[ds1_idx].height)
         cy = glb_ds1[ds1_idx].height - 1;
   }
   else
   {
      // the 2 folowing lines... maybe because floors are not
      // displayed at the same height of walls ?
      // ... then why in TILE mode it's ok ? weird.
      cx -= 2;
      cy += 2;
      

      // restrict the range only if NOT in paths editing mode
      // (paths can be out of the ds1 borders)
      if (glb_ds1edit.mode != MOD_P)
      {
         if (cx < 0)
            cx = 0;
         else if (cx >= glb_ds1[ds1_idx].width * 5 - 1)
            cx = glb_ds1[ds1_idx].width * 5 - 1;
         if (cy < 0)
            cy = 0;
         else if (cy >= glb_ds1[ds1_idx].height * 5 - 1)
            cy = glb_ds1[ds1_idx].height * 5 - 1;
      }
   }
   render_perf_add(
      &glb_render_perf_stats.mouse_ms_total,
      &glb_render_perf_stats.mouse_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // update objects animation, only if in 'running' animation mode
   perf_section_start_ms = render_perf_now_ms();
   if (glb_ds1[ds1_idx].animations_layer_mask == 1)
   {
      wpreview_obj_animate();
   }
   render_perf_add(
      &glb_render_perf_stats.obj_anim_ms_total,
      &glb_render_perf_stats.obj_anim_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   wpreview_get_visible_tile_bounds(
      ds1_idx,
      &min_tile_x,
      &max_tile_x,
      &min_tile_y,
      &max_tile_y
   );

   // Pre-set target bitmap for ALL rendering — avoids thousands of FBO rebinds.
   // All a5_draw_* helpers use a5_begin/end_target_bitmap which skip the switch
   // when the target is already correct.
   al_set_target_bitmap(glb_ds1edit.screen_buff);

   // Pass 1: Base terrain (lower walls, floors, tile shadows)
   perf_section_start_ms = render_perf_now_ms();
   for (y=min_tile_y; y<=max_tile_y; y++)
   {
      base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
      base_y = y * glb_ds1[ds1_idx].tile_h / 2;
      for (x=min_tile_x; x<=max_tile_x; x++)
      {
         select = FALSE;
         if ((glb_ds1edit.mode == MOD_T) && (x==cx) && (y==cy))
            select = TRUE;
         mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
         my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;
         if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
             (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w) &&
             (my >= glb_ds1edit.win_preview.y0-glb_ds1[ds1_idx].tile_h * 2) &&
             (my < glb_ds1edit.win_preview.y0 + glb_ds1edit.win_preview.h + glb_ds1[ds1_idx].tile_h * 2))
         {
            perf_call_start_ms = render_perf_now_ms();
            wpreview_draw_w(ds1_idx, x, y, mx, my, z, select, FALSE); // lower walls
            render_perf_add(
               &glb_render_perf_stats.loop_1a_w_ms_total,
               &glb_render_perf_stats.loop_1a_w_ms_max,
               render_perf_now_ms() - perf_call_start_ms
            );
            perf_call_start_ms = render_perf_now_ms();
            wpreview_draw_f(ds1_idx, x, y, mx, my, z, select);        // floors
            render_perf_add(
               &glb_render_perf_stats.loop_1a_f_ms_total,
               &glb_render_perf_stats.loop_1a_f_ms_max,
               render_perf_now_ms() - perf_call_start_ms
            );
            perf_call_start_ms = render_perf_now_ms();
            wpreview_draw_s(ds1_idx, x, y, mx, my, z, select);        // shadows of dt1
            render_perf_add(
               &glb_render_perf_stats.loop_1a_s_ms_total,
               &glb_render_perf_stats.loop_1a_s_ms_max,
               render_perf_now_ms() - perf_call_start_ms
            );
         }
      }
   }
   render_perf_add(
      &glb_render_perf_stats.loop_1a_ms_total,
      &glb_render_perf_stats.loop_1a_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // Pass 2: Object shadows (uses tinted drawing)
   perf_section_start_ms = render_perf_now_ms();
   objdraw_cur_idx = 0;
   for (y=min_tile_y; y<=max_tile_y; y++)
   {
      base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
      base_y = y * glb_ds1[ds1_idx].tile_h / 2;
      for (x=min_tile_x; x<=max_tile_x; x++)
      {
         select = FALSE;
         if ((glb_ds1edit.mode == MOD_T) && (x==cx) && (y==cy))
            select = TRUE;
         mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
         my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;
         if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
             (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w) &&
             (my >= glb_ds1edit.win_preview.y0-glb_ds1[ds1_idx].tile_h * 2) &&
             (my < glb_ds1edit.win_preview.y0 + glb_ds1edit.win_preview.h + glb_ds1[ds1_idx].tile_h * 2))
         {
            // shadows of objects
            if (glb_ds1[ds1_idx].animations_layer_mask)
               wpreview_draw_obj_tile_shad(ds1_idx, x, y, & objdraw_cur_idx);
         }
      }
   }
   render_perf_add(
      &glb_render_perf_stats.loop_1b_ms_total,
      &glb_render_perf_stats.loop_1b_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // Pass 3: Objects behind walls (orderflag=1)
   perf_section_start_ms = render_perf_now_ms();
   objdraw_cur_idx = 0;
   if (glb_ds1[ds1_idx].animations_layer_mask)
   {
      for (y=min_tile_y; y<=max_tile_y; y++)
      {
         base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
         base_y = y * glb_ds1[ds1_idx].tile_h / 2;
         for (x=min_tile_x; x<=max_tile_x; x++)
         {
            select = FALSE;
            if ((glb_ds1edit.mode == MOD_T) && (x==cx) && (y==cy))
               select = TRUE;
            mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
            my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;
            if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
                (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w) &&
                (my >= glb_ds1edit.win_preview.y0-glb_ds1[ds1_idx].tile_h * 2) &&
                (my < glb_ds1edit.win_preview.y0 + glb_ds1edit.win_preview.h + glb_ds1[ds1_idx].tile_h * 2))
            {
               wpreview_draw_obj_tile_1(ds1_idx, x, y, & objdraw_cur_idx);
            }
         }
      }
   }
   render_perf_add(
      &glb_render_perf_stats.loop_2_ms_total,
      &glb_render_perf_stats.loop_2_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // Tile grid overlay (if configured to draw between floors and walls)
   if (glb_ds1edit.display_tile_grid == TG_OVERFLOOR)
   if (glb_ds1edit.display_tile_grid == TG_OVERFLOOR)
      wpreview_draw_tile_grid(ds1_idx);

   // Pass 4: Upper walls + objects in front (orderflag 0/2)
   perf_section_start_ms = render_perf_now_ms();
   objdraw_cur_idx = 0;
   for (y=min_tile_y; y<=max_tile_y; y++)
   {
      base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
      base_y = y * glb_ds1[ds1_idx].tile_h / 2;
      for (x=min_tile_x; x<=max_tile_x; x++)
      {
         select = FALSE;
         if ((glb_ds1edit.mode == MOD_T) && (x==cx) && (y==cy))
            select = TRUE;
         mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
         my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;
         if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
             (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w) &&
             (my >= glb_ds1edit.win_preview.y0-glb_ds1[ds1_idx].tile_h * 2) &&
             (my < glb_ds1edit.win_preview.y0 + glb_ds1edit.win_preview.h + glb_ds1[ds1_idx].tile_h * 2))
         {
            perf_call_start_ms = render_perf_now_ms();
            wpreview_draw_w(ds1_idx, x, y, mx, my, z, select, TRUE);  // upper walls
            render_perf_add(
               &glb_render_perf_stats.loop_3_w_ms_total,
               &glb_render_perf_stats.loop_3_w_ms_max,
               render_perf_now_ms() - perf_call_start_ms
            );

            // objects of this tile
            if (glb_ds1[ds1_idx].animations_layer_mask)
            {
               perf_call_start_ms = render_perf_now_ms();
               wpreview_draw_obj_tile_0_2(ds1_idx, x, y, & objdraw_cur_idx);
               render_perf_add(
                  &glb_render_perf_stats.loop_3_obj_ms_total,
                  &glb_render_perf_stats.loop_3_obj_ms_max,
                  render_perf_now_ms() - perf_call_start_ms
               );
            }
         }
      }
   }
   render_perf_add(
      &glb_render_perf_stats.loop_3_ms_total,
      &glb_render_perf_stats.loop_3_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // Pass 5: Roofs
   perf_section_start_ms = render_perf_now_ms();
   for (y=min_tile_y; y<=max_tile_y; y++)
   {
      base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
      base_y = y * glb_ds1[ds1_idx].tile_h / 2;
      for (x=min_tile_x; x<=max_tile_x; x++)
      {
         select = FALSE;
         if ((glb_ds1edit.mode == MOD_T) && (x==cx) && (y==cy))
            select = TRUE;
         mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
         my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;
         if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
             (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w) &&
             (my >= glb_ds1edit.win_preview.y0-glb_ds1[ds1_idx].tile_h * 2) &&
             (my < glb_ds1edit.win_preview.y0 + glb_ds1edit.win_preview.h + glb_ds1[ds1_idx].tile_h * 2))
         {
            wpreview_draw_r (ds1_idx, x, y, mx, my, z, select); // roofs
         }
      }
   }
   render_perf_add(
      &glb_render_perf_stats.loop_4_ms_total,
      &glb_render_perf_stats.loop_4_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // Pass 6: Special tiles — orientation 10/11 (optional)
   perf_section_start_ms = render_perf_now_ms();
   if (glb_ds1[ds1_idx].special_layer_mask)
   {
      for (y=min_tile_y; y<=max_tile_y; y++)
      {
         base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
         base_y = y * glb_ds1[ds1_idx].tile_h / 2;
         for (x=min_tile_x; x<=max_tile_x; x++)
         {
            select = FALSE;
            if ((glb_ds1edit.mode == MOD_T) && (x==cx) && (y==cy))
               select = TRUE;
            mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
            my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;
            if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
                (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w) &&
                (my >= glb_ds1edit.win_preview.y0-glb_ds1[ds1_idx].tile_h * 2) &&
                (my < glb_ds1edit.win_preview.y0 + glb_ds1edit.win_preview.h + glb_ds1[ds1_idx].tile_h * 2))
            {
               wpreview_draw_sp(ds1_idx, x, y, mx, my, z, select); // special
            }
         }
      }
   }
   render_perf_add(
      &glb_render_perf_stats.loop_5_ms_total,
      &glb_render_perf_stats.loop_5_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // Pass 7: Walkable info overlay (optional debug view)
   perf_section_start_ms = render_perf_now_ms();
   switch(glb_ds1[ds1_idx].walkable_layer_mask)
   {
      case 1  : fptr_wi = wpreview_draw_simple_wi; break;
      case 2  : fptr_wi = wpreview_draw_wi;        break;
      default : fptr_wi = NULL;
   }
   if (fptr_wi != NULL)
   {
      for (y=min_tile_y; y<=max_tile_y; y++)
      {
         base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
         base_y = y * glb_ds1[ds1_idx].tile_h / 2;
         for (x=min_tile_x; x<=max_tile_x; x++)
         {
            mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
            if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
                (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w))
            {
               my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;
               if ((my >= glb_ds1edit.win_preview.y0-glb_ds1[ds1_idx].tile_h) &&
                   (my < glb_ds1edit.win_preview.y0 + glb_ds1edit.win_preview.h))
               {
                  misc_search_walk_infos(ds1_idx, x, y, walkinfo);
                  fptr_wi(mx, my, z, walkinfo);
               }
            }
         }
      }
   }
   render_perf_add(
      &glb_render_perf_stats.loop_6_ms_total,
      &glb_render_perf_stats.loop_6_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // tile grid : if over floor and walls, draw it now
   if (glb_ds1edit.display_tile_grid == TG_OVERWALL)
      wpreview_draw_tile_grid(ds1_idx);

   // mouse floor cursor
   perf_section_start_ms = render_perf_now_ms();
   if (glb_ds1edit.mode == MOD_T)
   {
      // tile cursor
      dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
      dy = (cy *  glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);

      x1 = dx - glb_ds1edit.win_preview.x0;
      x2 = x1 + glb_ds1[ds1_idx].tile_w / 2 - 1;
      x3 = x1 + glb_ds1[ds1_idx].tile_w / 2;
      x4 = x1 + glb_ds1[ds1_idx].tile_w - 1;

      y1 = dy - glb_ds1edit.win_preview.y0;
      y2 = y1 + glb_ds1[ds1_idx].tile_h / 2 - 1;
      y3 = y1 + glb_ds1[ds1_idx].tile_h - 2;
   
      wpreview_draw_line(x1, y2, x2, y1, 129);
      wpreview_draw_line(x3, y1, x4, y2, 129);
      wpreview_draw_line(x3, y3, x4, y2, 129);
      wpreview_draw_line(x1, y2, x2, y3, 129);
   }
   else // if (glb_ds1edit.mode != MOD_L)
   {
      // sub-cell cursor
      
      // invert the change
      cx += 2;
      cy -= 2;
      
      // compute the pixels position
      dx = (cy * -glb_ds1[ds1_idx].tile_w / 10) + (cx * glb_ds1[ds1_idx].tile_w / 10);
      dy = (cy *  glb_ds1[ds1_idx].tile_h / 10) + (cx * glb_ds1[ds1_idx].tile_h / 10);

      // redo it
      cx -= 2;
      cy += 2;
      
      x1 = dx - glb_ds1edit.win_preview.x0;
      x2 = x1 + glb_ds1[ds1_idx].tile_w / 10 - 1;
      x3 = x1 + glb_ds1[ds1_idx].tile_w / 10;
      x4 = x1 + glb_ds1[ds1_idx].tile_w / 5 - 1;

      y1 = dy - glb_ds1edit.win_preview.y0;
      y2 = y1 + glb_ds1[ds1_idx].tile_h / 10 - 1;
      y3 = y1 + glb_ds1[ds1_idx].tile_h / 5 - 2;
   
      wpreview_draw_line(x1, y2, x2, y1, 129);
      wpreview_draw_line(x3, y1, x4, y2, 129);
      wpreview_draw_line(x3, y3, x4, y2, 129);
      wpreview_draw_line(x1, y2, x2, y3, 129);
   }
   render_perf_add(
      &glb_render_perf_stats.cursor_ms_total,
      &glb_render_perf_stats.cursor_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );
   
   // npc paths
   perf_section_start_ms = render_perf_now_ms();
   if (glb_ds1edit.mode == MOD_P)
      wpreview_draw_paths_1obj(ds1_idx, pwin->obj_idx);
   else if (glb_ds1[ds1_idx].paths_layer_mask)
      wpreview_draw_paths(ds1_idx);
   render_perf_add(
      &glb_render_perf_stats.paths_ms_total,
      &glb_render_perf_stats.paths_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );
   
   // objects infos
   perf_section_start_ms = render_perf_now_ms();
   if (glb_ds1edit.mode == MOD_O)
   {
      editobj_draw_obj_lab(ds1_idx, TRUE);
      editobj_draw_obj_lab(ds1_idx, FALSE);
      if (glb_ds1[ds1_idx].draw_edit_obj)
         editobj_draw_edit_obj(ds1_idx);
   }
   else if (glb_ds1[ds1_idx].objects_layer_mask != OL_NONE)
      wpreview_draw_objects(ds1_idx);
   render_perf_add(
      &glb_render_perf_stats.obj_info_ms_total,
      &glb_render_perf_stats.obj_info_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // path infos window
   perf_section_start_ms = render_perf_now_ms();
   if (glb_ds1edit.mode == MOD_P)
   {
      editpath_draw(ds1_idx, a5_mouse_x, a5_mouse_y, a5_mouse_b, cx, cy);
   }
   render_perf_add(
      &glb_render_perf_stats.path_info_ms_total,
      &glb_render_perf_stats.path_info_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // help window for accurate sub-tile flags
   perf_section_start_ms = render_perf_now_ms();
   if ((glb_ds1[ds1_idx].walkable_layer_mask == 2) &&
       (glb_ds1[ds1_idx].subtile_help_display))
   {
      x1 = al_get_bitmap_width(glb_ds1edit.screen_buff) - al_get_bitmap_width(glb_ds1edit.subtile_help) - 10;
      x2 = x1 + al_get_bitmap_width(glb_ds1edit.subtile_help);
      y1 = al_get_bitmap_height(glb_ds1edit.screen_buff) - al_get_bitmap_height(glb_ds1edit.subtile_help) - 20;
      y2 = y1 + al_get_bitmap_height(glb_ds1edit.subtile_help);
      wpreview_blit_region(glb_ds1edit.subtile_help,
         0, 0, x1, y1, x2, y2);
      wpreview_draw_rect(x1-1, y1-1, x2+1, y2+1, 255);
   }
   render_perf_add(
      &glb_render_perf_stats.help_ms_total,
      &glb_render_perf_stats.help_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );
   
   // make up & bottom border black, with white line
   perf_section_start_ms = render_perf_now_ms();
   if (glb_ds1edit.show_2nd_row == TRUE)
   {
      wpreview_draw_rectfill(0,  0, glb_config.screen.width, 19, 0);
      wpreview_draw_hline(   0,  9, glb_config.screen.width, 29);
      wpreview_draw_hline(   0, 20, glb_config.screen.width, 255);

      // 2nd row datas
      wpreview_textprintf(  0, 11, 255, "Set:");
      wpreview_textprintf( 32, 11, 109, "%i", glb_ds1edit.ds1_group_idx + 1);

      wpreview_textprintf( 65, 11, 255, "Ds1Index:");
      wpreview_textprintf(137, 11, 109, "%i", ds1_idx + 1);

      wpreview_textprintf(175, 11, 255, "File:");
      wpreview_textprintf(215, 11, 109, "%s", glb_ds1[ds1_idx].name);
   }
   else
   {
      wpreview_draw_rectfill(0, 0, glb_config.screen.width, 8, 0);
      wpreview_draw_hline(   0, 9, glb_config.screen.width, 255);
   }

   // bottom row background
   wpreview_draw_rectfill(0, glb_config.screen.height-9,  glb_config.screen.width, glb_config.screen.height, 0);
   wpreview_draw_hline(   0, glb_config.screen.height-10, glb_config.screen.width, 255);

   // layers toggle
   a5_text_mode(-1);

   // floor layers (F1, F2)
   for (i=0; i < glb_ds1[ds1_idx].floor_num; i++)
   {
      if (glb_ds1[ds1_idx].floor_layer_mask[i] == 0)
         wpreview_textprintf(20*i, 0,  98, "f%i", i+1);
      else
         wpreview_textprintf(20*i, 0, 132, "f%i", i+1);
   }

   // animation layer (F3)
   switch(glb_ds1[ds1_idx].animations_layer_mask)
   {
      case 0 :
         wpreview_textprintf(50, 0, 98, "anims");
         break;
         
      case 1 :
         wpreview_textprintf(50, 0, 132, "anims");
         break;
         
      default :
         wpreview_textprintf(50, 0, 108, "anims");
         break;
   }

   // objects layer (F4)
   switch(glb_ds1[ds1_idx].objects_layer_mask)
   {
      case OL_NONE :
         wpreview_textprintf(100, 0, 98, "obj");
         break;
         
      case OL_TYPEID :
         wpreview_textprintf(100, 0, 210, "obj");
         break;
         
      case OL_SPEED :
         wpreview_textprintf(100, 0, 108, "obj");
         break;
         
      case OL_DESC :
         wpreview_textprintf(100, 0, 132, "obj");
         break;
         
      default :
         break;
   }

   // drawing order for special tiles (F9)
   switch(glb_ds1[ds1_idx].special_layer_mask)
   {
      case 0 :
         wpreview_textprintf(240, 0, 98, "spl");
         break;
         
      default :
         wpreview_textprintf(240, 0, 132, "spl");
         break;
   }

   // wall layers (F5, F6, F7 F8)
   for (i=0; i < glb_ds1[ds1_idx].wall_num; i++)
   {
      if (glb_ds1[ds1_idx].wall_layer_mask[i] == 0)
         wpreview_textprintf(140+20*i, 0,  98, "w%i", i+1);
      else
         wpreview_textprintf(140+20*i, 0, 132, "w%i", i+1);
   }

   // paths layer (F10)
   if (glb_ds1[ds1_idx].paths_layer_mask == 0)
      wpreview_textprintf(280, 0, 98, "path");
   else
      wpreview_textprintf(280, 0, 132, "path");

   // shadow layer (F11)
   switch(glb_ds1[ds1_idx].shadow_layer_mask[0])
   {
      case 1 :
         wpreview_textprintf(340, 0, 210, "shad");
         break;
         
      case 2 :
         wpreview_textprintf(340, 0, 255, "shad");
         break;
         
      case 3 :
         wpreview_textprintf(340, 0, 132, "shad");
         break;

      default :
         wpreview_textprintf(340, 0, 98, "shad");
         break;
         
   }

   // other infos

   // zoom (+, -)
   wpreview_textprintf(390, 0, 255, "zoom=%i:%i",
      glb_ds1[ds1_idx].height_mul, glb_ds1[ds1_idx].height_div);

   // gamma (F12)
   wpreview_textprintf(490, 0, 255, "gamma=%s",
      glb_gamma_str[glb_ds1edit.cur_gamma].str);

   // ds1 file name
   wpreview_textprintf(606, 0, 109, "%s",
      glb_ds1[ds1_idx].filename);

   // cell coordinates
   if (glb_ds1edit.mode == MOD_T)
   {
      // mode Tiles, 1 cell = 1 Tile
      wpreview_textprintf(
         0, glb_config.screen.height-8, 255,
         "Cell (%3i, %3i)",
         cx, cy
      );
   }
   else
   {
      // mode Objects or Paths, 1 cell = 1 sub-Tile
      wpreview_textprintf(
         0, glb_config.screen.height-8, 255,
         "Sub-Cell (%3i, %3i)",
         cx, cy
      );
   }

   // frames per second
   wpreview_textprintf(
      glb_config.screen.width-80, 0, 255,
      "  fps=%i",
      glb_ds1edit.old_fps
   );

   // refresh rate
   wpreview_textprintf(
      glb_config.screen.width-168, glb_config.screen.height-8,
      92,
      "%iHz",
      glb_ds1edit.current_refresh_rate
   );

   // current number of objects / max number of objects
   wpreview_textprintf(
      200, glb_config.screen.height-8,
      92,
      "[Objects : %ld / %ld]",
      glb_ds1[ds1_idx].obj_num,
	  glb_ds1[ds1_idx].current_obj_max
   );

   // editing mode
   switch(glb_ds1edit.mode)
   {
      case MOD_T : mode = "Tiles  "; break;
      case MOD_O : mode = "Objects"; break;
      case MOD_P : mode = "Paths  "; break;

      case MOD_L : if (glb_ds1edit.night_mode == 0)
                      mode = "Night 1";
                   else
                      mode = "Night 2";
                   break;
         
      default    : mode = "?      "; break;
   }
   wpreview_textprintf(
      glb_config.screen.width-112, glb_config.screen.height-8, 255, "Mode :");
   wpreview_textprintf(
      glb_config.screen.width-56, glb_config.screen.height-8, 108, "%s", mode);
   render_perf_add(
      &glb_render_perf_stats.hud_ms_total,
      &glb_render_perf_stats.hud_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   // uncomment this part to understand the axis and
   // glb_ds1edit.win_preview.x0 (and y0) relations, at different zooms
/*
   wpreview_textprintf(0, 40, 255, "prev x0, y0  = "
      "%i, %i", glb_ds1edit.win_preview.x0, glb_ds1edit.win_preview.y0);
   wpreview_draw_hline(0, - glb_ds1edit.win_preview.y0, glb_config.screen.width, 255);
   wpreview_draw_vline(- glb_ds1edit.win_preview.x0, 0, glb_config.screen.height, 255);
*/

   // draw screen
   perf_section_start_ms = render_perf_now_ms();
   misc_draw_screen(a5_mouse_x, a5_mouse_y);
   render_perf_add(
      &glb_render_perf_stats.present_ms_total,
      &glb_render_perf_stats.present_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   perf_section_start_ms = render_perf_now_ms();
   wpreview_reiinit_animated_floor(ds1_idx);
   render_perf_add(
      &glb_render_perf_stats.reinit_ms_total,
      &glb_render_perf_stats.reinit_ms_max,
      render_perf_now_ms() - perf_section_start_ms
   );

   render_perf_add(
      &glb_render_perf_stats.total_ms_total,
      &glb_render_perf_stats.total_ms_max,
      render_perf_now_ms() - perf_total_start_ms
   );
   glb_render_perf_stats.frames++;
   if (glb_render_perf_stats.frames >= 30)
      render_perf_print_summary();

   // Per-frame perf log — writes directly to file (not stderr which has flush issues)
   {
      static FILE *_plog = NULL;
      static int _pframe = 0;
      static double _prev_clear = 0, _prev_pal = 0, _prev_1a = 0, _prev_1b = 0;
      static double _prev_3 = 0, _prev_4 = 0, _prev_present = 0;
      double frame_ms = render_perf_now_ms() - perf_total_start_ms;
      // Compute per-frame deltas from cumulative totals
      double d_clear   = glb_render_perf_stats.clear_ms_total - _prev_clear;
      double d_pal     = glb_render_perf_stats.palette_ms_total - _prev_pal;
      double d_1a      = glb_render_perf_stats.loop_1a_ms_total - _prev_1a;
      double d_1b      = glb_render_perf_stats.loop_1b_ms_total - _prev_1b;
      double d_3       = glb_render_perf_stats.loop_3_ms_total - _prev_3;
      double d_4       = glb_render_perf_stats.loop_4_ms_total - _prev_4;
      double d_present = glb_render_perf_stats.present_ms_total - _prev_present;
      _prev_clear = glb_render_perf_stats.clear_ms_total;
      _prev_pal   = glb_render_perf_stats.palette_ms_total;
      _prev_1a    = glb_render_perf_stats.loop_1a_ms_total;
      _prev_1b    = glb_render_perf_stats.loop_1b_ms_total;
      _prev_3     = glb_render_perf_stats.loop_3_ms_total;
      _prev_4     = glb_render_perf_stats.loop_4_ms_total;
      _prev_present = glb_render_perf_stats.present_ms_total;
      _pframe++;
      if (_plog == NULL) {
         _plog = fopen("perf_log.csv", "w");
         if (_plog) {
            fprintf(_plog, "frame,time_s,total_ms,terrain_ms,present_ms,draw_calls,draw_total_ms,draw_max_ms,draw_avg_us,mem_draws,target_flags\n");
            fflush(_plog);
         }
      }
      if (_plog && _pframe <= 120) {
         double draw_avg_us = _draw_call_count > 0 ? (_draw_call_total_ms * 1000.0 / _draw_call_count) : 0;
         fprintf(_plog, "%d,%.3f,%.1f,%.2f,%.2f,%d,%.2f,%.3f,%.1f,%d,0x%x\n",
            _pframe, al_get_time(), frame_ms,
            d_1a, d_present,
            _draw_call_count, _draw_call_total_ms, _draw_call_max_ms,
            draw_avg_us, _draw_call_mem_count,
            al_get_bitmap_flags(glb_ds1edit.screen_buff));
         fflush(_plog);
      }
      // Reset per-frame draw call stats
      _draw_call_count = 0;
      _draw_call_total_ms = 0;
      _draw_call_max_ms = 0;
      _draw_call_mem_count = 0;
   }
}


// ==========================================================================
// draw all tiles of this ds1
int wpreview_draw_tiles_big_screenshot(int ds1_idx)
{
   int               x, y, base_x = 0, base_y = 0, mx, my;
   int               cx, cy, z, x1, x2, y1, y2, select;
   static int        old_pal = -1;
   UBYTE             walkinfo[25];
   int               objdraw_cur_idx = 0;
   void              (* fptr_wi) (int, int, int, UBYTE *);
   PATH_EDIT_WIN_S   * pwin = & glb_ds1[ds1_idx].path_edit_win;

   long              minx, miny, maxx, maxy, tmpw, tmph;
   BLOCK_TABLE_S     * bt_ptr;
   ALLEGRO_BITMAP            * tmp_bmp;
   CELL_S_S          * s_ptr;
   CELL_F_S          * f_ptr;
   CELL_W_S          * w_ptr;
   int               n, t, bt_idx, dt1_idx, block_idx;
   int               old_screen_width, old_screen_height,
                     old_screen_x0, old_screen_y0;

   // some inits
   z = glb_ds1[ds1_idx].cur_zoom;

   // init coordinates
   minx = miny = 0x7FFFFFFFLU;
   maxx = maxy = 0x80000000LU;

   // find coordinates
   for (y=0; y<glb_ds1[ds1_idx].height; y++)
   {
      base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
      base_y = y * glb_ds1[ds1_idx].tile_h / 2;
      for (x=0; x<glb_ds1[ds1_idx].width; x++)
      {
         mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
         my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;

         // for all shadow layers of that tile
         t     = (y * glb_ds1[ds1_idx].shadow_line) + (x * glb_ds1[ds1_idx].shadow_num);
         s_ptr = glb_ds1[ds1_idx].shadow_buff + t;
         for (n=0; n<glb_ds1[ds1_idx].shadow_num; n++)
         {
            bt_idx = s_ptr[n].bt_idx; // index in block table

            if (s_ptr[n].prop4 & 0x80) // binary : 1000-0000
                 bt_idx = -1; // consider that tile as "unknown"
           
            if (bt_idx != -1)
            {
               bt_ptr = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table

               if (bt_ptr->type != BT_SHADOW)
                  continue; // only shadows

               dt1_idx   = bt_ptr->dt1_idx;
               block_idx = bt_ptr->block_idx;

               tmp_bmp = * (glb_dt1[dt1_idx].block_zoom[z] + block_idx);

               if (tmp_bmp == NULL)
                  continue;

               y1 = my -
                    bt_ptr->zero_line * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div;
               y1 += glb_ds1[ds1_idx].tile_h; // shadow, like walls, are lower than floors
                                          //    (and than roofs) by 80 pixels
               y2 = y1 + al_get_bitmap_height(tmp_bmp);
               x1 = mx;
               x2 = x1 + al_get_bitmap_width(tmp_bmp);
               if (x1 < minx)
                  minx = x1;
               if (x2 > maxx)
                  maxx = x2;
               if (y1 < miny)
                  miny = y1;
               if (y2 > maxy)
                  maxy = y2;
            }
         }

         // for all floor layers of that tile
         t     = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
         f_ptr = glb_ds1[ds1_idx].floor_buff + t;
         for (n=0; n<glb_ds1[ds1_idx].floor_num; n++)
         {
            bt_idx = f_ptr[n].bt_idx; // index in block table

            if (bt_idx != -1)
            {
               bt_ptr = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table

               if ((bt_ptr->type != BT_STATIC) && (bt_ptr->type != BT_ANIMATED))
                  continue; // only floors

               dt1_idx   = bt_ptr->dt1_idx;
               block_idx = bt_ptr->block_idx;

               tmp_bmp = * (glb_dt1[dt1_idx].block_zoom[z] + block_idx);

               if (tmp_bmp == NULL)
                  continue;

               y1 = my -
                    bt_ptr->zero_line * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div;
               y2 = y1 + al_get_bitmap_height(tmp_bmp);
               x1 = mx;
               x2 = x1 + al_get_bitmap_width(tmp_bmp);
               if (x1 < minx)
                  minx = x1;
               if (x2 > maxx)
                  maxx = x2;
               if (y1 < miny)
                  miny = y1;
               if (y2 > maxy)
                  maxy = y2;
            }
         }

         // for all wall layers of that tile
         t     = (y * glb_ds1[ds1_idx].wall_line) + (x * glb_ds1[ds1_idx].wall_num);
         w_ptr = glb_ds1[ds1_idx].wall_buff + t;
         for (n=0; n<glb_ds1[ds1_idx].wall_num; n++)
         {
            bt_idx = w_ptr[n].bt_idx; // index in block table

            if (bt_idx != -1)
            {
               bt_ptr = glb_ds1[ds1_idx].block_table + bt_idx; // pointer in block table

               if ( (bt_ptr->type != BT_WALL_UP) &&
                    (bt_ptr->type != BT_WALL_DOWN) &&
                    (bt_ptr->type != BT_ROOF) &&
                    (bt_ptr->type != BT_SPECIAL) &&
                    (bt_ptr->type != BT_WALL_ANIMATED)
                  )
               {
                  continue; // only walls
               }

               dt1_idx   = bt_ptr->dt1_idx;
               block_idx = bt_ptr->block_idx;

               tmp_bmp = * (glb_dt1[dt1_idx].block_zoom[z] + block_idx);

               if (tmp_bmp == NULL)
                  continue;

               y1 = my -
                    bt_ptr->zero_line * glb_ds1[ds1_idx].height_mul / glb_ds1[ds1_idx].height_div;
               y1 += glb_ds1[ds1_idx].tile_h; // shadow, like walls, are lower than floors
                                          //    (and than roofs) by 80 pixels
               y2 = y1 + al_get_bitmap_height(tmp_bmp);
               x1 = mx;
               x2 = x1 + al_get_bitmap_width(tmp_bmp);
               if (x1 < minx)
                  minx = x1;
               if (x2 > maxx)
                  maxx = x2;
               if (y1 < miny)
                  miny = y1;
               if (y2 > maxy)
                  maxy = y2;
            }
         }
      }
   }

   // deduce BIG screenshot dimensions
   tmpw = maxx - minx;
   tmph = maxy - miny;
/*
   printf("bigscreenshot : (%i, %i) - (%i, %i) = %i * %i pixels\n",
          minx, miny, maxx, maxy, tmpw, tmph
   );
*/

   // make a temporary new screen buffer
   if ((tmpw <= 0) || (tmph <= 0))
      return -1;
   tmp_bmp = al_create_bitmap(tmpw, tmph);
   if (tmp_bmp == NULL)
      return -1;

   // swap it with the normal one
   glb_ds1edit.screen_buff = tmp_bmp;

   old_screen_width  = glb_ds1edit.win_preview.w;
   old_screen_height = glb_ds1edit.win_preview.h;
   old_screen_x0     = glb_ds1edit.win_preview.x0;
   old_screen_y0     = glb_ds1edit.win_preview.y0;

   glb_ds1edit.win_preview.w  = tmpw;
   glb_ds1edit.win_preview.h  = tmph;
   glb_ds1edit.win_preview.x0 = minx;
   glb_ds1edit.win_preview.y0 = miny;
   glb_config.screen.width        = tmpw;
   glb_config.screen.height       = tmph;

   // let's go
   a5_clear(glb_ds1edit.screen_buff);
   al_set_target_bitmap(glb_ds1edit.screen_buff);

   // handle palette
   if (glb_ds1edit.cmd_line.force_pal_num == -1)
   {
      // use .ds1 act value for palette
      if (old_pal != glb_ds1[ds1_idx].act - 1)
      {
         old_pal = glb_ds1[ds1_idx].act - 1;
         a5_current_palette = &glb_ds1edit.vga_pal[old_pal];
         dt1_rebuild_bitmaps_from_cache(a5_current_palette);
      }
   }
   else
   {
      // use force_pal value for palette
      if (old_pal != glb_ds1edit.cmd_line.force_pal_num - 1)
      {
         old_pal = glb_ds1edit.cmd_line.force_pal_num - 1;
         a5_current_palette = &glb_ds1edit.vga_pal[old_pal];
         dt1_rebuild_bitmaps_from_cache(a5_current_palette);
      }
   }

   // tile cursor under the mouse
   cx = cy = -1;

   // loop 1A : lower walls, floors, shadows of dt1
   for (y=0; y<glb_ds1[ds1_idx].height; y++)
   {
      base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
      base_y = y * glb_ds1[ds1_idx].tile_h / 2;
      for (x=0; x<glb_ds1[ds1_idx].width; x++)
      {
         select = FALSE;
         if ((glb_ds1edit.mode == MOD_T) && (x==cx) && (y==cy))
            select = TRUE;
         mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
         if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
             (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w))
         {
            my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;
            wpreview_draw_w(ds1_idx, x, y, mx, my, z, select, FALSE); // lower walls
            wpreview_draw_f(ds1_idx, x, y, mx, my, z, select);        // floors
            wpreview_draw_s(ds1_idx, x, y, mx, my, z, select);        // shadows of dt1
         }
      }
   }

   // loop 1B : shadows of objects
   objdraw_cur_idx = 0;
   for (y=0; y<glb_ds1[ds1_idx].height; y++)
   {
      base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
      base_y = y * glb_ds1[ds1_idx].tile_h / 2;
      for (x=0; x<glb_ds1[ds1_idx].width; x++)
      {
         select = FALSE;
         if ((glb_ds1edit.mode == MOD_T) && (x==cx) && (y==cy))
            select = TRUE;
         mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
         if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
             (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w))
         {
            // shadows of objects
            if (glb_ds1[ds1_idx].animations_layer_mask)
               wpreview_draw_obj_tile_shad(ds1_idx, x, y, & objdraw_cur_idx);
         }
      }
   }

   // loop 2 : objects with orderflag set to 1 (optional)
   objdraw_cur_idx = 0;
   if (glb_ds1[ds1_idx].animations_layer_mask)
   {
      for (y=0; y<glb_ds1[ds1_idx].height; y++)
      {
         base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
         base_y = y * glb_ds1[ds1_idx].tile_h / 2;
         for (x=0; x<glb_ds1[ds1_idx].width; x++)
         {
            select = FALSE;
            if ((glb_ds1edit.mode == MOD_T) && (x==cx) && (y==cy))
               select = TRUE;
            mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
            if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
                (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w))
            {
               // objects of this tile
               wpreview_draw_obj_tile_1(ds1_idx, x, y, & objdraw_cur_idx);
            }
         }
      }
   }

   // loop 3 : upper walls, objects with orderflag set to 0 or 2
   objdraw_cur_idx = 0;
   for (y=0; y<glb_ds1[ds1_idx].height; y++)
   {
      base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
      base_y = y * glb_ds1[ds1_idx].tile_h / 2;
      for (x=0; x<glb_ds1[ds1_idx].width; x++)
      {
         select = FALSE;
         if ((glb_ds1edit.mode == MOD_T) && (x==cx) && (y==cy))
            select = TRUE;
         mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
         if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
             (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w))
         {
            my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;
            wpreview_draw_w(ds1_idx, x, y, mx, my, z, select, TRUE);  // upper walls

            // objects of this tile
            if (glb_ds1[ds1_idx].animations_layer_mask)
               wpreview_draw_obj_tile_0_2(ds1_idx, x, y, & objdraw_cur_idx);
         }
      }
   }

   // loop 4 : roofs
   for (y=0; y<glb_ds1[ds1_idx].height; y++)
   {
      base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
      base_y = y * glb_ds1[ds1_idx].tile_h / 2;
      for (x=0; x<glb_ds1[ds1_idx].width; x++)
      {
         select = FALSE;
         if ((glb_ds1edit.mode == MOD_T) && (x==cx) && (y==cy))
            select = TRUE;
         mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
         if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
             (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w))
         {
            my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;
            wpreview_draw_r (ds1_idx, x, y, mx, my, z, select); // roofs
         }
      }
   }

   // loop 5 : special tiles (optional)
   if (glb_ds1[ds1_idx].special_layer_mask)
   {
      for (y=0; y<glb_ds1[ds1_idx].height; y++)
      {
         base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
         base_y = y * glb_ds1[ds1_idx].tile_h / 2;
         for (x=0; x<glb_ds1[ds1_idx].width; x++)
         {
            select = FALSE;
            if ((glb_ds1edit.mode == MOD_T) && (x==cx) && (y==cy))
               select = TRUE;
            mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
            if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
                (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w))
            {
               my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;
               wpreview_draw_sp(ds1_idx, x, y, mx, my, z, select); // special
            }
         }
      }
   }

   // loop 6 : walkable infos (optional)
   switch(glb_ds1[ds1_idx].walkable_layer_mask)
   {
      case 1  : fptr_wi = wpreview_draw_simple_wi; break;
      case 2  : fptr_wi = wpreview_draw_wi;        break;
      default : fptr_wi = NULL;
   }
   if (fptr_wi != NULL)
   {
      for (y=0; y<glb_ds1[ds1_idx].height; y++)
      {
         base_x = y * -glb_ds1[ds1_idx].tile_w / 2;
         base_y = y * glb_ds1[ds1_idx].tile_h / 2;
         for (x=0; x<glb_ds1[ds1_idx].width; x++)
         {
            mx = base_x + x * glb_ds1[ds1_idx].tile_w / 2;
            if ((mx >= glb_ds1edit.win_preview.x0-glb_ds1[ds1_idx].tile_w) &&
                (mx < glb_ds1edit.win_preview.x0 + glb_ds1edit.win_preview.w))
            {
               my = base_y + x * glb_ds1[ds1_idx].tile_h / 2;
               if ((my >= glb_ds1edit.win_preview.y0-glb_ds1[ds1_idx].tile_h) &&
                   (my < glb_ds1edit.win_preview.y0 + glb_ds1edit.win_preview.h))
               {
                  misc_search_walk_infos(ds1_idx, x, y, walkinfo);
                  fptr_wi(mx, my, z, walkinfo);
               }
            }
         }
      }
   }

   // tile grid
   if (glb_ds1edit.display_tile_grid == TRUE)
      wpreview_draw_tile_grid(ds1_idx);

   // npc paths
   if (glb_ds1edit.mode == MOD_P)
      wpreview_draw_paths_1obj(ds1_idx, pwin->obj_idx);
   else if (glb_ds1[ds1_idx].paths_layer_mask)
      wpreview_draw_paths(ds1_idx);
   
   // objects infos
   if (glb_ds1edit.mode == MOD_O)
   {
      editobj_draw_obj_lab(ds1_idx, TRUE);
      editobj_draw_obj_lab(ds1_idx, FALSE);
      if (glb_ds1[ds1_idx].draw_edit_obj)
         editobj_draw_edit_obj(ds1_idx);
   }
   else if (glb_ds1[ds1_idx].objects_layer_mask != OL_NONE)
      wpreview_draw_objects(ds1_idx);

   // path infos window
   if (glb_ds1edit.mode == MOD_P)
   {
      editpath_draw(ds1_idx, a5_mouse_x, a5_mouse_y, a5_mouse_b, cx, cy);
   }

   // help window for accurate sub-tile flags
   if ((glb_ds1[ds1_idx].walkable_layer_mask == 2) &&
       (glb_ds1[ds1_idx].subtile_help_display))
   {
      x1 = al_get_bitmap_width(glb_ds1edit.screen_buff) - al_get_bitmap_width(glb_ds1edit.subtile_help) - 10;
      x2 = x1 + al_get_bitmap_width(glb_ds1edit.subtile_help);
      y1 = al_get_bitmap_height(glb_ds1edit.screen_buff) - al_get_bitmap_height(glb_ds1edit.subtile_help) - 20;
      y2 = y1 + al_get_bitmap_height(glb_ds1edit.subtile_help);
      wpreview_blit_region(glb_ds1edit.subtile_help,
         0, 0, x1, y1, x2, y2);
      wpreview_draw_rect(x1-1, y1-1, x2+1, y2+1, 255);
   }
   
   glb_ds1edit.win_preview.w  = old_screen_width;
   glb_ds1edit.win_preview.h  = old_screen_height;
   glb_ds1edit.win_preview.x0 = old_screen_x0;
   glb_ds1edit.win_preview.y0 = old_screen_y0;
   glb_config.screen.width        = old_screen_width;
   glb_config.screen.height       = old_screen_height;

   return 0;
}

#ifdef WIN32
   #pragma warning (pop)
#endif

