#ifndef _UNDO_H_

#define _UNDO_H_

void undo_reset_flags              (int ds1_idx);
void undo_exit                     (void);
void undo_new_tile_buffer          (int ds1_idx);
void undo_start_seg_in_tile_buffer (int ds1_idx, UBYTE x, UBYTE y);
void undo_close_seg_in_tile_buffer (int ds1_idx);
void undo_add_to_tile_buffer       (int ds1_idx, int ix, int iy);
void undo_close_tile_buffer        (int ds1_idx);
void undo_apply_tile_buffer        (int ds1_idx);
void undo_add_this_tile_to_buffer  (int ds1_idx, int ix, int iy, CELL_F_S * f, CELL_W_S * w, CELL_S_S * s);

#endif
