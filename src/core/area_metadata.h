#ifndef _AREA_METADATA_H_
#define _AREA_METADATA_H_

int area_name_parse_act(const char * name);
int area_name_has_act_mismatch(int txt_act, const char * name);
int area_group_resolve_act(int txt_act, const char * name);
int area_preset_resolve_act(int level_id, int levels_act, const char * preset_name);
int area_tileless_palette_act(int ds1_act, int folder_act);
const char * area_name_strip_prefix(const char * name);

/* Where a map that the area build did not place belongs in the browser. */
#define AREA_DEST_UNLISTED 0
#define AREA_DEST_AREA     1
#define AREA_DEST_PD2      2

const char * area_preset_area_name(const char * preset_name);
const char * area_pd2_folder_group(const char * rel_path);
int area_map_destination(int is_pd2_only, int lvltype);
int area_section_key(int is_pd2, int act);

#endif
