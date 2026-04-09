#ifndef _TILE_PICKER_H_
#define _TILE_PICKER_H_

void tile_picker_init         (void);
void tile_picker_ensure_built (int ds1_idx);
void tile_picker_draw         (int ds1_idx, int panel_x, int panel_right,
                                int y_top, int panel_bottom);
int  tile_picker_click        (int ds1_idx, int mx, int my,
                                int panel_x, int panel_right,
                                int y_top, int panel_bottom);
int  tile_picker_scroll       (int dz, int ctrl_held);
void tile_picker_place_brush  (int ds1_idx, int cx, int cy);
void tile_picker_push_mru     (BLK_TYP_E type, int bt_idx);
int  tile_picker_popup_run    (int ds1_idx, int cx, int cy,
                                int screen_x, int screen_y);
BUT_TYP_E tile_picker_layer_button_for(int ds1_idx, BLK_TYP_E type);
void tile_picker_on_ds1_change(int new_ds1_idx);

#endif
