#ifndef _AREA_METADATA_H_
#define _AREA_METADATA_H_

int area_name_parse_act(const char * name);
int area_name_has_act_mismatch(int txt_act, const char * name);
int area_group_resolve_act(int txt_act, const char * name);
const char * area_name_strip_prefix(const char * name);

#endif