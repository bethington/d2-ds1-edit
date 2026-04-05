#ifndef _TXTREAD_H_

#define _TXTREAD_H_
                        
char  * txt_gets               (char * bptr, int * nb_char, int * is_new_line, int del_char);
void  txt_count_header_cols    (char * cur_col, int * col_count);
char  * txt_read_header        (char * cur_col, TXT_S * txt);
int   txt_check_type_and_size  (char * cur_col, TXT_S * txt);
int   txt_fill_data            (char * cur_col, TXT_S * txt);
TXT_S * txt_destroy            (TXT_S * txt);
void  txt_get_user_filter_cols (char * cur_col, TXT_S * txt);
TXT_S * txt_load               (char * mem, RQ_ENUM enum_txt, char * filename);
void  * txt_read_in_mem        (char * txtname);
void  txt_convert_slash        (char * str);
void  txt_debug                (char * file_path_mem, char * file_path_def, TXT_S * txt);
int   read_lvltypes_txt        (int ds1_idx, int type);
int   read_lvlprest_txt        (int ds1_idx, int def);
int   read_obj_txt             (void);
int   read_objects_txt         (void);

#endif
