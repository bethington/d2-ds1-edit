#ifndef _ASSET_EXPORT_H_

#define _ASSET_EXPORT_H_

#include "structs.h"

int asset_export_png(const char *asset_path, const char *output_dir);
int asset_export_area_group_png(const AREA_GROUP_S *group, const char *output_dir);
int asset_export_prefix_png(const char *asset_prefix, const char *type_filter,
	const char *output_dir);
int asset_export_all_png(const char *type_filter, const char *output_dir);
int asset_export_dcc_buffer_png(const char *asset_path, const void *buffer,
	long len, const char *output_dir);
int asset_export_dc6_buffer_png(const char *asset_path, const void *buffer,
	long len, const char *output_dir);
int asset_export_dt1_header_looks_valid(const void *buffer, long len);
int asset_export_guess_palette_act(const char *asset_path);

int asset_export_filter_matches_prefix(const char *asset_path, const char *asset_prefix);
int asset_export_filter_matches_type(const char *asset_path, const char *type_filter);

#endif