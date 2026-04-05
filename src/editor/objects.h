#ifndef _EDITOBJ_H_

#define _EDITOBJ_H_                                                           

int  editobj_qsort_helper_drawing_order (const void * e1, const void * e2);
void editobj_set_drawing_order          (int ds1_idx);
void editobj_make_obj_label             (int ds1_idx);
void editobj_draw_obj_lab               (int ds1_idx, int is_shadow);
void editobj_make_obj_desc              (int ds1_idx, int obj_idx);
void editobj_make_obj_new_id            (int ds1_idx, int obj_idx);
void editobj_clear_obj_lab_sel          (int ds1_idx);
void editobj_clear_obj_lab_over         (int ds1_idx);
void editobj_clear_obj_lab_flags        (int ds1_idx);
int  editobj_over_obj_lab               (int ds1_idx, int * ptr_t, int * ptr_o, int cx, int cy, int mx, int my);
void editobj_prepare_moving             (int ds1_idx);
int  editobj_moving_obj_lab             (int ds1_idx, int dcx, int dcy, int dmx, int dmy);
void editobj_end_move_obj_lab           (int ds1_idx);
void editobj_prepare_undo               (int ds1_idx);
void editobj_undo                       (int ds1_idx);
void editobj_del_obj                    (int ds1_idx);
void editobj_copy_obj                   (int ds1_idx);
// void editobj_search_copy_center         (int ds1_idx, int * cx, int * cy);
// int  editobj_center_before_copy         (int ds1_idx, int * cx0, int * cy0, int * mx,  int * my);
int  editobj_count_sel_obj              (int ds1_idx);
int  editobj_insert_obj                 (int ds1_idx, int cx, int cy);
void editobj_prepare_edit_obj_win       (int ds1_idx, int obj_idx);
void editobj_draw_edit_obj              (int ds1_idx);
int  editobj_edit_obj                   (int ds1_idx, int * edit_end, int mx, int my, int mb);
int  editobj_handler                    (int ds1_idx, int cx, int cy, int mx, int my, int mb);

#endif
