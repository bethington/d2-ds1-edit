#ifndef _EDITPATH_H_

#define _EDITPATH_H_

int  editpath_mouse_in            (int ds1_idx, int mx, int my);
void editpath_init                (int ds1_idx);
void editpath_search_selected_obj (int ds1_idx);
void editpath_draw                (int ds1_idx, int mx, int my, int mb, long tx, long ty);

#endif
