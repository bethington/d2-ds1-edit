#ifndef _EDITTILE_H_

#define _EDITTILE_H_                                                           

// COPY / PASTE MANAGMENT
void edittile_middle_select               (int ds1_idx, int * start_x, int * start_y);
void edittile_paste_prepare               (int ds1_idx);
void edittile_paste_undo                  (int ds1_idx);
void edittile_paste_preview               (int ds1_idx, int dx, int dy, PASTE_POS_S * p);
void edittile_paste_final                 (int ds1_idx);

// DELETE TILES MANAGMENT
void edittile_delete_selected_tiles       (int ds1_idx);
void edittile_delete_selected_tiles2      (int ds1_idx);

// MODIFY SELECTION MANAGMENT
void edittile_deselect_one                (int ds1_idx, int x, int y);
void edittile_deselect_one_completly      (int ds1_idx, int x, int y);
void edittile_select_one                  (int ds1_idx, int x, int y);
void edittile_select_new_one              (int ds1_idx, int x, int y);
void edittile_identical                   (int ds1_idx, IT_ENUM itype, int tx, int ty);

// HIDE / SHOW (UN-HIDE) MANAGMENT
void edittile_unhide_all                  (int ds1_idx);
void edittile_hide_add_one                (int ds1_idx, int x, int y);

// TEMP SELECTION MANAGMENT
void edittile_delete_all_tmpsel           (int ds1_idx);
void edittile_add_tmpsel_one              (int ds1_idx, int x, int y);
void edittile_set_tmpsel                  (int ds1_idx, TMP_SEL_S * s);
void edittile_change_to_new_permanent_sel (int ds1_idx, TMP_SEL_S * s);
void edittile_change_to_add_permanent_sel (int ds1_idx, TMP_SEL_S * s);
void edittile_change_to_hide_sel          (int ds1_idx, TMP_SEL_S * s);
void edittile_change_to_del_sel           (int ds1_idx, TMP_SEL_S * s);

#endif
