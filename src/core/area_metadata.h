#ifndef _AREA_METADATA_H_
#define _AREA_METADATA_H_

int area_name_parse_act(const char * name);
int area_name_has_act_mismatch(int txt_act, const char * name);
int area_group_resolve_act(int txt_act, const char * name);
int area_preset_resolve_act(int level_id, int levels_act, const char * preset_name);
int area_resolve_txt_act(int lvltypes_act, int preset_act, int folder_act);
const char * area_name_strip_prefix(const char * name);

#endif
