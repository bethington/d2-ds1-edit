#ifndef _MISC_H_

#define _MISC_H_

void   misc_pal_d2_2_vga               (int pal_idx);
int    misc_qsort_helper_block_table_1 (const void * e1, const void * e2);
int    misc_qsort_helper_block_table_2 (const void * e1, const void * e2);
void   misc_check_tiles_conflicts      (int ds1_idx);
void   misc_make_block_table           (int ds1_idx);
void   misc_read_gamma                 (void);
void   misc_update_pal_with_gamma      (void);
void   misc_pcx_put_d2_palette         (char * name, int pal_idx);
void   misc_make_cmaps_helper          (const PALETTE pal, int x, int y, RGB * rgb);
void   misc_make_cmaps                 (void);
int    misc_load_pal_from_disk         (int pal_idx);
void   misc_save_pal_on_disk           (int pal_idx, UBYTE * d2_pal_ptr);
int    misc_my_fgets                   (char * dst, int max, FILE * in);
char * misc_search_name                (char * tmp);
void   misc_open_1_ds1                 (int ds1_idx, char * name, int type, int def, int new_width, int new_height);
void   misc_open_several_ds1           (char * filename);
void   misc_walkable_tile_info_pcx     (void);
int    misc_seach_block_or4            (int ds1_idx, BLOCK_TABLE_S * bt_ptr, int b, int m, int s);
void   misc_search_walk_infos          (int ds1_idx, int x, int y, UBYTE * dsttable);
int    misc_load_mpq_file              (char * filename, char ** buffer, long * buf_len, int output);
int    misc_get_txt_column_num         (RQ_ENUM txt_idx, char * col_name);
void   misc_open_1_ds1_force_dt1       (int ds1_idx);
void   misc_pl2_correct                (int i);
int    misc_cmd_line_parse             (int argc, char ** argv);
/*
void   misc_restore_mouse_background   (void);
void   misc_save_mouse_background      (int x, int y);
void   misc_draw_mouse_cursor          (int x, int y, int restore_background_first);
void   misc_set_mouse_cursor           (BITMAP * sprite);
*/
void   misc_draw_screen                (int mx, int my);
int    misc_increase_ds1_objects_max   (int ds1_idx, long nb_objects);
#endif
