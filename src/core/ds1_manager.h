#ifndef _DS1_MANAGER_H_
#define _DS1_MANAGER_H_

/* LvlPrest.txt modification — reads raw file, modifies specific cells, writes back */
int  ds1_manager_lvlprest_set_file(int def, int file_slot, const char * ds1_path);
int  ds1_manager_lvlprest_clear_file(int def, int file_slot);
int  ds1_manager_lvlprest_find_empty_slot(int def);
int  ds1_manager_lvlprest_find_file_slot(int def, const char * ds1_path);
int  ds1_manager_delete(int group_idx, int entry_idx);

/* Backup system */
int  ds1_manager_backup(int group_idx, int entry_idx);
int  ds1_manager_restore(int group_idx, int entry_idx);
int  ds1_manager_delete_permanent(int group_idx, int entry_idx);

/* DS1 creation */
int  ds1_manager_create_empty(int group_idx, int entry_idx, int width, int height, int act);
int  ds1_manager_clone(int src_ds1_idx, int group_idx, int entry_idx);

/* Smart filename generation — finds gap in numbering sequence */
void ds1_manager_suggest_name(const char * base_ds1_path,
                               const AREA_GROUP_S * group,
                               char * out_name, int out_size);

/* Generic TXT cell editor */
int  ds1_manager_txt_set_cell(RQ_ENUM txt_type, const char * key_col, int key_val,
                               const char * target_col, const char * new_value);

/* JSON metadata */
int  ds1_manager_write_backup_json(const char * json_path, int group_idx,
                                    int entry_idx, int ds1_idx);

#endif
