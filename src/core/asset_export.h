#ifndef _ASSET_EXPORT_H_

#define _ASSET_EXPORT_H_

#include "structs.h"

// A discovered set of asset paths to export, plus accounting for
// diagnostics. `total_candidates` tracks the count BEFORE any
// content-level filter (e.g. single-frame DC6) rejects entries; it lets
// the caller distinguish "pattern matched no files at all" from "found
// N files but all were filtered out." For commit 3, total_candidates
// equals count; later commits widen the gap.
typedef struct ASSET_EXPORT_PLAN_S
{
   char **paths;
   int count;
   int capacity;
   int total_candidates;
} ASSET_EXPORT_PLAN_S;

// Initialise an empty plan. Equivalent to memset to zero; safe to call
// multiple times.
void asset_export_plan_init(ASSET_EXPORT_PLAN_S *plan);

// Build a plan from a virtual asset prefix + type filter. The prefix
// matches recursively (a path matches if it starts with prefix +
// separator). Type filter is one of "all"/"dt1"/"dc6"/"dcc" or NULL/""
// for all. Returns 1 on success, 0 on parameter error.
int asset_export_plan_for_prefix(const char *asset_prefix,
                                 const char *type_filter,
                                 ASSET_EXPORT_PLAN_S *plan_out);

// Build a plan from an area group (the assets referenced by the loaded
// area's lvltype/lvlprest entries). Returns 1 on success, 0 on
// parameter error.
int asset_export_plan_for_area_group(const AREA_GROUP_S *group,
                                     ASSET_EXPORT_PLAN_S *plan_out);

// Iterate the plan and write PNGs into output_dir. Returns the number
// of successfully exported files.
int asset_export_run_plan(const ASSET_EXPORT_PLAN_S *plan,
                          const char *output_dir);

// Free all memory owned by the plan and reset it to empty.
void asset_export_plan_free(ASSET_EXPORT_PLAN_S *plan);

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