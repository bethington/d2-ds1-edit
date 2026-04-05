#ifndef _DS1MISC_H_

#define _DS1MISC_H_

int  ds1_free              (int i);
void wprop_2_block         (int i, CELL_W_S * w_ptr);
void fprop_2_block         (int i, CELL_F_S * f_ptr);
void sprop_2_block         (int i, CELL_S_S * s_ptr);
void ds1_make_prop_2_block (int i);
int  ds1_read              (const char * ds1name, int ds1_idx, int new_width, int new_height);

#endif
