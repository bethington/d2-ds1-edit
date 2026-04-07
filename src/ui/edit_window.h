#ifndef _WEDIT_H_

#define _WEDIT_H_

void wedit_draw_s             (int ds1_idx, int x, int y);
void wedit_draw_f             (int ds1_idx, int x, int y);
void wedit_draw_w             (int ds1_idx, int x, int y, int upper);
void wedit_draw_r             (int ds1_idx, int x, int y);
void wedit_draw_sp            (int ds1_idx, int x, int y);
void wedit_make_2nd_buttons   (void);
void wedit_read_pcx           (void);
void wedit_free               (void);
void wedit_draw_button        (BUT_TYP_E b_num);
void wedit_draw_tab           (BLK_TYP_E t_num);
int  wedit_tab_have           (int ds1_idx, BLK_TYP_E t);
void wedit_tiles_free         (void);
void wedit_tiles_make         (int ds1_idx);
int  wedit_search_tile        (int ds1_idx, int x, int y, BUT_TYP_E but);
void wedit_tab_tiles          (int ds1_idx, BLK_TYP_E t, int x0, int y0, int bt_sel, int m_idx, int s_idx);
void wedit_jump               (int ds1_idx, int bt_idx, int * xn, int * yn);
void wedit_mouse_tile         (int mx, int my, int x0, int y0, int t, int * m_idx, int * s_idx);
void wedit_update_tile        (int ds1_idx, int x, int y, BUT_TYP_E button, BLK_TYP_E type, int m_idx, int s_idx);
void wedit_save_tile          (int ds1_idx, int x, int y, CELL_F_S * dst_f_ptr, CELL_W_S * dst_w_ptr, CELL_S_S * dst_s_ptr);
void wedit_cancel_tile        (int ds1_idx, int x, int y, CELL_F_S * src_f_ptr, CELL_W_S * src_w_ptr, CELL_S_S * src_s_ptr);
void wedit_keep_tile          (int ds1_idx, int x, int y, CELL_F_S * src_f_ptr, CELL_W_S * src_w_ptr, CELL_S_S * src_s_ptr);
void wedit_test               (int ds1_idx, int tx, int ty);

#endif
