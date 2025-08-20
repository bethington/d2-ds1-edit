#ifndef _WPREVIEW_H_

#define _WPREVIEW_H_

void wpreview_shape                     (BITMAP * tmp_bmp, int x0, int y0, int ds1_idx, COL_E col_idx);
void wpreview_gouraud_f                 (BITMAP * tmp_bmp, int x0, int y0, int ds1_idx, int c1, int c2, int c3, int c4);
void wpreview_gouraud_w                 (BITMAP * tmp_bmp, int x0, int y0, int ds1_idx, int c1, int c2, int c3, int c4);
int  wpreview_light_dist                (int x0, int y0, int mx, int my);
int  qsort_helper_order_data            (const void *e1, const void *e2);
void wpreview_draw_s                    (int ds1_idx, int x, int y, int mx, int my, int z, int selected);
void wpreview_draw_f                    (int ds1_idx, int x, int y, int mx, int my, int z, int selected);
void wpreview_draw_w                    (int ds1_idx, int x, int y, int mx, int my, int z, int selected, int upper);
void wpreview_draw_r                    (int ds1_idx, int x, int y, int mx, int my, int z, int selected);
void wpreview_draw_sp                   (int ds1_idx, int x, int y, int mx, int my, int z, int selected);
void coord_to_tile                      (int ds1_idx, int ax, int ay, int * layer_x, int * layer_y);
void mouse_to_tile                      (int ds1_idx, int * layer_x, int * layer_y);
void change_zoom                        (int ds1_idx, ZOOM_E z);
void wpreview_draw_paths                (int ds1_idx);
void wpreview_draw_paths_1obj           (int ds1_idx, int o);
void wpreview_obj_animate               (void);
void wpreview_draw_objects              (int ds1_idx);
void wpreview_draw_simple_wi            (int mx, int my, int z, UBYTE * walkinfo);
void wpreview_draw_wi                   (int mx, int my, int z, UBYTE * walkinfo);
void wpreview_draw_an_object            (int ds1_idx, int o);
void wpreview_draw_an_object_shad       (int ds1_idx, int o);
void wpreview_draw_obj_tile_shad        (int ds1_idx, int x, int y, int * cur_idx);
void wpreview_draw_obj_tile_0_2         (int ds1_idx, int x, int y, int * cur_idx);
void wpreview_draw_obj_tile_1           (int ds1_idx, int x, int y, int * cur_idx);
void wpreview_draw_tiles                (int ds1_idx);
void wpreview_reiinit_animated_floor    (int ds1_idx);
int  wpreview_draw_tiles_big_screenshot (int ds1_idx);

#endif
