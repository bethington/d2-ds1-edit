#ifndef _AREA_METADATA_H_
#define _AREA_METADATA_H_

#include <stddef.h>

int area_name_parse_act(const char * name);
int area_name_has_act_mismatch(int txt_act, const char * name);
int area_group_resolve_act(int txt_act, const char * name);
int area_preset_resolve_act(int level_id, int levels_act, const char * preset_name);
int area_tileless_palette_act(int ds1_act, int folder_act);
int area_palette_act(int preset_level_type, int preset_pal, int type, int type_pal,
                     int lvltypes_act);
int area_most_common_pal(const int counts[5]);
int area_merge_level_type(int current, int level_type);
const char * area_name_strip_prefix(const char * name);

/* Where a map that the area build did not place belongs in the browser. */
#define AREA_DEST_UNLISTED 0
#define AREA_DEST_AREA     1
#define AREA_DEST_PD2      2

const char * area_preset_area_name(const char * preset_name);
const char * area_folder_area_name(const char * rel_path);
int area_row_accepts_name_match(int level_type, const char * rule_area,
                                int group_lvltype, const char * group_area);

/* The DT1 names a DS1 was saved with. Below this score they do not settle it. */
#define AREA_DT1_NAMES_MIN_SCORE 80

int area_ds1_embedded_names(const unsigned char * buf, size_t len, int * act,
                            const char ** names, int * count);
int area_dt1_names_match(const char * embedded, const char * lvltype_file);
int area_dt1_names_score(const char * names, int count,
                         const char * const * files, int file_count);
int area_dt1_names_min_score(int is_pd2_only);

const char * area_pd2_folder_group(const char * rel_path);
int area_map_destination(int is_pd2_only, int lvltype);
int area_section_key(int is_pd2, int act);

#endif
