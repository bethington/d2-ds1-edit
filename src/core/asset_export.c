#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "structs.h"
#include "misc.h"
#include "mpq/MpqView.h"
#include "core/area_browser.h"
#include "core/project.h"
#include "core/dt1.h"
#include "core/dcc.h"
#include "core/dc6.h"
#include "core/dc6_header.h"
#include "core/export_progress.h"
#include "core/glob_match.h"
#include "core/asset_export.h"
#include "core/txtread.h"
#include "platform.h"

#ifdef WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#define DT1_HEADER_BYTES 276
#define DT1_BLOCK_HEADER_BYTES 96

static void normalize_asset_stem(const char *asset_path, char *out, int out_cap)
{
   int i, last_dot = -1;

   if (out == NULL || out_cap <= 0)
      return;
   out[0] = 0;
   if (asset_path == NULL)
      return;

   for (i = 0; asset_path[i] != 0 && i < (out_cap - 1); i++)
   {
      char ch = asset_path[i];
      if (ch == '/' || ch == '\\')
         ch = PATH_SEP;
      out[i] = ch;
      if (ch == '.')
         last_dot = i;
   }
   out[i] = 0;

   if (last_dot > 0)
      out[last_dot] = 0;
}

static int make_output_path(char *out, int out_cap,
                            const char *output_dir,
                            const char *asset_path,
                            const char *leaf_name)
{
   char stem[512];
   char stem_parent[512];
   const char *base;
   char base_png[128];
   char *last_sep;

   if (out == NULL || out_cap <= 0 || output_dir == NULL || leaf_name == NULL)
      return 0;

   normalize_asset_stem(asset_path, stem, sizeof(stem));
   if (stem[0] == 0)
      return 0;

   strcpy(stem_parent, stem);
   last_sep = strrchr(stem_parent, PATH_SEP);
   if (last_sep != NULL)
   {
      base = last_sep + 1;
      *last_sep = 0;
   }
   else
   {
      base = stem_parent;
      stem_parent[0] = 0;
   }

   snprintf(base_png, sizeof(base_png), "%s.png", base);
   if (stricmp(leaf_name, base_png) == 0)
   {
      if (stem_parent[0] != 0)
      {
         snprintf(out, out_cap, "%s%c%s%c%s",
            output_dir, PATH_SEP, stem_parent, PATH_SEP, leaf_name);
      }
      else
      {
         snprintf(out, out_cap, "%s%c%s",
            output_dir, PATH_SEP, leaf_name);
      }
      return 1;
   }

   snprintf(out, out_cap, "%s%c%s%c%s",
      output_dir, PATH_SEP, stem, PATH_SEP, leaf_name);
   return 1;
}

static void make_asset_basename_png(const char *asset_path, char *out, int out_cap)
{
   char stem[512];
   const char *base;

   if (out == NULL || out_cap <= 0)
      return;
   out[0] = 0;

   normalize_asset_stem(asset_path, stem, sizeof(stem));
   if (stem[0] == 0)
      return;

   base = strrchr(stem, PATH_SEP);
   if (base != NULL)
      base++;
   else
      base = stem;

   snprintf(out, out_cap, "%s.png", base);
}

// Build a FLAT output path for one frame of a single-direction
// multi-frame DC6:
//
//   <output_dir>\<stem_parent>\<basename>_<NN>.png
//
// The frame-index suffix zero-pads to 1 digit when total_frames <= 9
// and to 3 digits otherwise (per the user-visible naming convention).
// For multi-direction DC6 (and for DCC) we keep the legacy nested
// layout via make_anim_leaf + make_output_path.
static int make_flat_frame_output(char *out, int out_cap,
                                  const char *output_dir,
                                  const char *asset_path,
                                  int frame_idx, int total_frames)
{
   char stem[512];
   char stem_parent[512];
   const char *base;
   char *last_sep;
   int width;

   if (out == NULL || out_cap <= 0
       || output_dir == NULL || asset_path == NULL)
      return 0;

   normalize_asset_stem(asset_path, stem, sizeof(stem));
   if (stem[0] == 0)
      return 0;

   strcpy(stem_parent, stem);
   last_sep = strrchr(stem_parent, PATH_SEP);
   if (last_sep != NULL)
   {
      base = last_sep + 1;
      *last_sep = 0;
   }
   else
   {
      base = stem_parent;
      stem_parent[0] = 0;
   }

   width = (total_frames > 9) ? 3 : 1;

   if (stem_parent[0] != 0)
   {
      snprintf(out, out_cap, "%s%c%s%c%s_%0*d.png",
         output_dir, PATH_SEP, stem_parent, PATH_SEP,
         base, width, frame_idx);
   }
   else
   {
      snprintf(out, out_cap, "%s%c%s_%0*d.png",
         output_dir, PATH_SEP, base, width, frame_idx);
   }
   return 1;
}

static int save_bitmap_png(const char *path, ALLEGRO_BITMAP *bmp)
{
   if (path == NULL || bmp == NULL)
      return 0;

   if (!project_ensure_parent_dirs(path))
      return 0;

   return al_save_bitmap(path, bmp) ? 1 : 0;
}

static void make_anim_leaf(const char *asset_path, char *out, int out_cap, int directions,
                           int frames_per_direction, int direction_idx,
                           int frame_idx)
{
   if (out == NULL || out_cap <= 0)
      return;

   if (directions > 1)
   {
      if (frames_per_direction > 1)
      {
         snprintf(out, out_cap, "dir_%02i%cframe_%03i.png",
            direction_idx, PATH_SEP, frame_idx);
      }
      else
      {
         snprintf(out, out_cap, "dir_%02i.png", direction_idx);
      }
   }
   else if (frames_per_direction > 1)
   {
      snprintf(out, out_cap, "frame_%03i.png", frame_idx);
   }
   else
   {
      make_asset_basename_png(asset_path, out, out_cap);
   }
}

// EXPORT_PATH_LIST_S is the legacy internal name; ASSET_EXPORT_PLAN_S
// (declared in asset_export.h) is the public type. They are layout-
// compatible: the path array fields share names + types, with the public
// version adding `total_candidates` for diagnostic accounting. Internal
// helpers below operate on the public type directly.
typedef ASSET_EXPORT_PLAN_S EXPORT_PATH_LIST_S;

/* Defined below, but called from functions that appear above it. */
static int export_path_list_add(EXPORT_PATH_LIST_S *list, const char *asset_path);

// Per-discovery-pass set of every path the candidate-consideration
// helper has decided about, regardless of whether the path was
// accepted into the plan or rejected by the single-frame DC6 filter.
// Without this, a multi-frame DC6 visible in BOTH the mod overlay and
// the MPQ chain gets counted twice in total_candidates (the existing
// dedup against the plan's `paths` list misses it because rejected
// paths are never added there).
static EXPORT_PATH_LIST_S g_seen_candidates;

typedef struct DT1_DISCOVERY_CACHE_ENTRY_S
{
   char *asset_path;
   int is_valid;
} DT1_DISCOVERY_CACHE_ENTRY_S;

typedef struct DT1_DISCOVERY_CACHE_S
{
   DT1_DISCOVERY_CACHE_ENTRY_S *items;
   int count;
   int capacity;
} DT1_DISCOVERY_CACHE_S;

static DT1_DISCOVERY_CACHE_S g_dt1_discovery_cache;

static void normalize_slashes_copy(const char *src, char *dst, int dst_cap)
{
   int i;

   if (dst == NULL || dst_cap <= 0)
      return;
   dst[0] = 0;
   if (src == NULL)
      return;

   for (i = 0; src[i] != 0 && i < (dst_cap - 1); i++)
   {
      char ch = src[i];
      if (ch == '/')
         ch = '\\';
      dst[i] = ch;
   }
   dst[i] = 0;
}

static void dt1_discovery_cache_reset(void);
static int dt1_discovery_cache_lookup(const char *asset_path, int *is_valid_out);
static void dt1_discovery_cache_store(const char *asset_path, int is_valid);

static void trim_trailing_slash(char *path)
{
   int len;

   if (path == NULL)
      return;
   len = (int) strlen(path);
   while (len > 0 && (path[len - 1] == '\\' || path[len - 1] == '/'))
   {
      path[len - 1] = 0;
      len--;
   }
}

int asset_export_guess_palette_act(const char *asset_path)
{
   char path_norm[512];
   const char *act_root;

   if (asset_path == NULL || asset_path[0] == 0)
      return 0;

   normalize_slashes_copy(asset_path, path_norm, sizeof(path_norm));
   act_root = path_norm;
   if (strnicmp(act_root, "Data\\Global\\Tiles\\", 18) == 0)
      act_root += 18;

   if (strnicmp(act_root, "Act1\\", 5) == 0)
      return 1;
   if (strnicmp(act_root, "Act2\\", 5) == 0)
      return 2;
   if (strnicmp(act_root, "Act3\\", 5) == 0)
      return 3;
   if (strnicmp(act_root, "Act4\\", 5) == 0)
      return 4;
   if (strnicmp(act_root, "Expansion\\", 10) == 0)
      return 5;

   return 0;
}

static RGBA_PALETTE *asset_export_resolve_palette(const char *asset_path)
{
   int pal_idx;
   int guessed_act;

   if (glb_ds1edit.cmd_line.force_pal_num >= 1 &&
       glb_ds1edit.cmd_line.force_pal_num <= PAL_ACT_MAX)
   {
      pal_idx = glb_ds1edit.cmd_line.force_pal_num - 1;
      return &glb_ds1edit.vga_pal[pal_idx];
   }

   guessed_act = asset_export_guess_palette_act(asset_path);
   if (guessed_act >= 1 && guessed_act <= PAL_ACT_MAX)
   {
      pal_idx = guessed_act - 1;
      return &glb_ds1edit.vga_pal[pal_idx];
   }

   if (a5_current_palette != NULL)
      return a5_current_palette;

   return &glb_ds1edit.vga_pal[0];
}

static int dt1_asset_path_is_discoverable(const char *asset_path)
{
   char path_norm[512];

   if (asset_path == NULL)
      return 0;

   normalize_slashes_copy(asset_path, path_norm, sizeof(path_norm));
   return strnicmp(path_norm, "Data\\Global\\Tiles\\", 18) == 0;
}

int asset_export_dt1_header_looks_valid(const void *buffer, long len)
{
   const UBYTE *bytes = (const UBYTE *) buffer;
   int32_t x1, x2, block_num, bh_start;
   long header_bytes;

   if (bytes == NULL || len < DT1_HEADER_BYTES)
      return 0;

   /* DT1 stores these as 32-bit fields. Reading them through `long` picked up
      8 bytes under LP64, so x1 swallowed x2 and no DT1 ever validated on
      Linux or macOS. memcpy rather than a cast: the offsets are not aligned
      for a 4-byte load and the cast would also alias. */
   memcpy(&x1,        bytes,       sizeof(x1));
   memcpy(&x2,        bytes + 4,   sizeof(x2));
   memcpy(&block_num, bytes + 268, sizeof(block_num));
   memcpy(&bh_start,  bytes + 272, sizeof(bh_start));

   if (x1 != 7 || x2 != 6)
      return 0;
   if (block_num < 0 || bh_start < 0)
      return 0;
   if (bh_start > len)
      return 0;

   header_bytes = len - bh_start;
   if (block_num > (header_bytes / DT1_BLOCK_HEADER_BYTES))
      return 0;

   return 1;
}

static int dt1_payload_looks_valid_for_export(const char *asset_path)
{
   char *buffer;
   long len;
   int valid;

   if (dt1_discovery_cache_lookup(asset_path, &valid))
      return valid;

   if (!dt1_asset_path_is_discoverable(asset_path))
      return 0;

   if (misc_load_mpq_file((char *) asset_path, &buffer, &len, FALSE) == -1)
   {
      dt1_discovery_cache_store(asset_path, 0);
      return 0;
   }

   valid = asset_export_dt1_header_looks_valid(buffer, len);
   free(buffer);
   dt1_discovery_cache_store(asset_path, valid);
   return valid;
}

static int has_supported_asset_extension(const char *asset_path)
{
   const char *ext;

   if (asset_path == NULL)
      return 0;

   ext = a5_get_extension(asset_path);
   if (ext == NULL)
      return 0;

   return (stricmp(ext, "dt1") == 0) ||
          (stricmp(ext, "dcc") == 0) ||
          (stricmp(ext, "dc6") == 0);
}

static int asset_path_matches_type(const char *asset_path, const char *type_filter)
{
   const char *ext;

   if (!has_supported_asset_extension(asset_path))
      return 0;
   if (type_filter == NULL || type_filter[0] == 0 || stricmp(type_filter, "all") == 0)
      return 1;

   ext = a5_get_extension(asset_path);
   if (ext != NULL && stricmp(ext, "dt1") == 0 && !dt1_asset_path_is_discoverable(asset_path))
      return 0;

   return (ext != NULL) && (stricmp(ext, type_filter) == 0);
}

static int asset_path_matches_discovery_filter(const char *asset_path,
                                               const char *type_filter)
{
   const char *ext;

   if (!asset_path_matches_type(asset_path, type_filter))
      return 0;

   ext = a5_get_extension(asset_path);
   if (ext != NULL && stricmp(ext, "dt1") == 0)
      return dt1_payload_looks_valid_for_export(asset_path);

   return 1;
}

// Forward declaration; export_path_list_contains is defined later.
static int export_path_list_contains(const EXPORT_PATH_LIST_S *list,
                                     const char *asset_path);

// Returns 1 if a DC6 file at `asset_path` is single-frame, 0 if multi-frame
// or unreadable. Loads the file via the MPQ chain and inspects the header.
// Only the first DC6_HEADER_MIN_BYTES bytes are looked at, but the current
// MPQ loader returns the full file; that overhead is acceptable since this
// is only called when the single-frame filter is on.
static int dc6_path_is_single_frame(const char *asset_path)
{
   char *buff = NULL;
   long len = 0;
   int sf;

   if (misc_load_mpq_file((char *) asset_path, &buff, &len, FALSE) == -1)
   {
      fprintf(stderr,
              "asset_export: dc6 single-frame check skipped (load failed): %s\n",
              asset_path);
      return 0;
   }

   sf = dc6_header_is_single_frame(buff, len);
   free(buff);

   if (sf < 0)
   {
      fprintf(stderr,
              "asset_export: dc6 single-frame check skipped (bad header): %s\n",
              asset_path);
      return 0;
   }

   return sf == 1;
}

// Applies content-level filters to a path that has already passed the
// scope check (type filter or glob pattern). Increments total_candidates
// for first-time-seen paths and adds to the plan if the content filters
// pass. Invalid DT1 payloads are silently discarded (not counted) to
// preserve existing behavior. Idempotent on duplicate paths.
static void consider_path_after_scope_match(const char *asset_path,
                                            EXPORT_PATH_LIST_S *out_paths)
{
   const char *ext;

   if (out_paths == NULL || asset_path == NULL || asset_path[0] == 0)
      return;
   /* Dedup against EVERY previously-considered path in this discovery
    * pass, not just the ones added to the plan. This is what keeps
    * multi-frame DC6 visible in both the overlay and the MPQ chain
    * from inflating total_candidates. */
   if (export_path_list_contains(&g_seen_candidates, asset_path))
      return;

   ext = a5_get_extension(asset_path);

   if (ext != NULL && stricmp(ext, "dt1") == 0
       && !dt1_payload_looks_valid_for_export(asset_path))
   {
      /* Invalid DT1 -- not a candidate at all. Still record it in the
       * seen set so we don't re-validate it on a second sighting. */
      export_path_list_add(&g_seen_candidates, asset_path);
      return;
   }

   /* Decided to count this as a candidate. Claim it before any
    * further filtering so a second sighting bails out above. */
   export_path_list_add(&g_seen_candidates, asset_path);
   out_paths->total_candidates++;

   if (ext != NULL && stricmp(ext, "dc6") == 0
       && glb_config.export_dc6_single_frame_only)
   {
      if (!dc6_path_is_single_frame(asset_path))
         return;
   }

   export_path_list_add(out_paths, asset_path);
}

// Considers a discovered asset path under a type-filter scope (legacy
// prefix-based discovery). Wraps the type-extension check around the
// shared content-filter helper.
static void consider_candidate_for_plan(const char *asset_path,
                                        const char *type_filter,
                                        EXPORT_PATH_LIST_S *out_paths)
{
   if (asset_path == NULL)
      return;
   if (!asset_path_matches_type(asset_path, type_filter))
      return;
   consider_path_after_scope_match(asset_path, out_paths);
}

// Considers a discovered asset path under a glob-pattern scope.
static void consider_candidate_for_pattern(const char *asset_path,
                                           const char *pattern,
                                           EXPORT_PATH_LIST_S *out_paths)
{
   if (asset_path == NULL || pattern == NULL)
      return;
   if (!glob_match(pattern, asset_path))
      return;
   consider_path_after_scope_match(asset_path, out_paths);
}

static int asset_path_matches_prefix(const char *asset_path, const char *asset_prefix)
{
   char path_norm[512];
   char prefix_norm[512];
   int prefix_len;

   if (asset_path == NULL || asset_prefix == NULL || asset_prefix[0] == 0)
      return 0;

   normalize_slashes_copy(asset_path, path_norm, sizeof(path_norm));
   normalize_slashes_copy(asset_prefix, prefix_norm, sizeof(prefix_norm));
   trim_trailing_slash(prefix_norm);

   prefix_len = (int) strlen(prefix_norm);
   if (prefix_len == 0)
      return 0;
   if (strnicmp(path_norm, prefix_norm, prefix_len) != 0)
      return 0;
   if (path_norm[prefix_len] == 0)
      return 1;

   return path_norm[prefix_len] == '\\';
}

int asset_export_filter_matches_prefix(const char *asset_path, const char *asset_prefix)
{
   return asset_path_matches_prefix(asset_path, asset_prefix);
}

int asset_export_filter_matches_type(const char *asset_path, const char *type_filter)
{
   return asset_path_matches_type(asset_path, type_filter);
}

static int export_path_list_contains(const EXPORT_PATH_LIST_S *list, const char *asset_path)
{
   int i;

   if (list == NULL || asset_path == NULL)
      return 0;

   for (i = 0; i < list->count; i++)
   {
      if (stricmp(list->paths[i], asset_path) == 0)
         return 1;
   }

   return 0;
}

static int export_path_list_add(EXPORT_PATH_LIST_S *list, const char *asset_path)
{
   char **new_items;
   char *copy;
   int new_capacity;

   if (list == NULL || asset_path == NULL || asset_path[0] == 0)
      return 0;
   if (export_path_list_contains(list, asset_path))
      return 1;

   if (list->count >= list->capacity)
   {
      new_capacity = (list->capacity == 0) ? 64 : (list->capacity * 2);
      new_items = (char **) realloc(list->paths, sizeof(char *) * new_capacity);
      if (new_items == NULL)
         return 0;
      list->paths = new_items;
      list->capacity = new_capacity;
   }

   copy = (char *) malloc(strlen(asset_path) + 1);
   if (copy == NULL)
      return 0;
   strcpy(copy, asset_path);
   list->paths[list->count++] = copy;
   return 1;
}

static void export_path_list_destroy(EXPORT_PATH_LIST_S *list)
{
   int i;

   if (list == NULL)
      return;

   for (i = 0; i < list->count; i++)
      free(list->paths[i]);
   free(list->paths);
   list->paths = NULL;
   list->count = 0;
   list->capacity = 0;
   list->total_candidates = 0;
}

static void dt1_discovery_cache_reset(void)
{
   int i;

   for (i = 0; i < g_dt1_discovery_cache.count; i++)
      free(g_dt1_discovery_cache.items[i].asset_path);
   free(g_dt1_discovery_cache.items);
   memset(&g_dt1_discovery_cache, 0, sizeof(g_dt1_discovery_cache));
}

static int dt1_discovery_cache_lookup(const char *asset_path, int *is_valid_out)
{
   int i;

   if (asset_path == NULL)
      return 0;

   for (i = 0; i < g_dt1_discovery_cache.count; i++)
   {
      if (stricmp(g_dt1_discovery_cache.items[i].asset_path, asset_path) == 0)
      {
         if (is_valid_out != NULL)
            *is_valid_out = g_dt1_discovery_cache.items[i].is_valid;
         return 1;
      }
   }

   return 0;
}

static void dt1_discovery_cache_store(const char *asset_path, int is_valid)
{
   DT1_DISCOVERY_CACHE_ENTRY_S *new_items;
   char *copy;
   int new_capacity;

   if (asset_path == NULL)
      return;
   if (dt1_discovery_cache_lookup(asset_path, NULL))
      return;

   if (g_dt1_discovery_cache.count >= g_dt1_discovery_cache.capacity)
   {
      new_capacity = (g_dt1_discovery_cache.capacity == 0)
         ? 32
         : (g_dt1_discovery_cache.capacity * 2);
      new_items = (DT1_DISCOVERY_CACHE_ENTRY_S *) realloc(
         g_dt1_discovery_cache.items,
         sizeof(DT1_DISCOVERY_CACHE_ENTRY_S) * new_capacity);
      if (new_items == NULL)
         return;
      g_dt1_discovery_cache.items = new_items;
      g_dt1_discovery_cache.capacity = new_capacity;
   }

   copy = (char *) malloc(strlen(asset_path) + 1);
   if (copy == NULL)
      return;
   strcpy(copy, asset_path);

   g_dt1_discovery_cache.items[g_dt1_discovery_cache.count].asset_path = copy;
   g_dt1_discovery_cache.items[g_dt1_discovery_cache.count].is_valid = is_valid;
   g_dt1_discovery_cache.count++;
}

// `type_filter` is consulted when `pattern` is NULL; otherwise pattern
// matching takes over and type_filter is ignored.
static void collect_overlay_assets_recursive(const char *root_dir,
                                             const char *current_dir,
                                             const char *type_filter,
                                             const char *pattern,
                                             EXPORT_PATH_LIST_S *out_paths)
{
   DS1_DIR d;

   if (!ds1_dir_open(&d, current_dir))
      return;

   while (ds1_dir_next(&d))
   {
      char disk_path[1024];

      snprintf(disk_path, sizeof(disk_path), "%s" DS1_SEP_STR "%s",
               current_dir, d.name);

      if (d.is_dir)
      {
         collect_overlay_assets_recursive(root_dir, disk_path,
                                          type_filter, pattern, out_paths);
      }
      else
      {
         char        virtual_path[1024];
         char       *p;
         const char *relative = disk_path + strlen(root_dir);

         if (DS1_IS_SEP(*relative))
            relative++;
         snprintf(virtual_path, sizeof(virtual_path), "Data\\%s", relative);

         /* An MPQ virtual path is backslash-separated on every platform --
            it is hashed by the archive, not resolved by the OS. The disk
            path we just built used the native separator, so fold it back. */
         for (p = virtual_path; *p != '\0'; p++)
         {
            if (*p == '/')
               *p = '\\';
         }

         if (pattern != NULL)
            consider_candidate_for_pattern(virtual_path, pattern, out_paths);
         else
            consider_candidate_for_plan(virtual_path, type_filter, out_paths);
      }
   }

   ds1_dir_close(&d);
}

static void collect_overlay_assets_for_prefix(const char *asset_prefix,
                                              const char *type_filter,
                                              EXPORT_PATH_LIST_S *out_paths)
{
   char        prefix_norm[512];
   char        disk_prefix[1024];
   const char *relative_prefix;

   if (glb_config.mod_dir[0] == NULL || asset_prefix == NULL || out_paths == NULL)
      return;

   normalize_slashes_copy(asset_prefix, prefix_norm, sizeof(prefix_norm));
   trim_trailing_slash(prefix_norm);

   relative_prefix = prefix_norm;
   if (stricmp(relative_prefix, "Data") == 0)
      relative_prefix += 4;
   else if (strnicmp(relative_prefix, "Data\\", 5) == 0)
      relative_prefix += 5;

   if (relative_prefix[0] == 0)
   {
      snprintf(disk_prefix, sizeof(disk_prefix), "%s", glb_config.mod_dir[0]);
   }
   else
   {
      snprintf(disk_prefix, sizeof(disk_prefix), "%s" DS1_SEP_STR "%s",
               glb_config.mod_dir[0], relative_prefix);
      /* relative_prefix came out of a virtual path, so it is backslashed. */
      ds1_path_normalize(disk_prefix);
   }

   if (ds1_dir_exists(disk_prefix))
      collect_overlay_assets_recursive(glb_config.mod_dir[0], disk_prefix,
                                       type_filter, NULL, out_paths);
   else if (ds1_file_exists(disk_prefix))
      consider_candidate_for_plan(prefix_norm, type_filter, out_paths);
}

static void collect_overlay_assets_for_pattern(const char *pattern,
                                               EXPORT_PATH_LIST_S *out_paths)
{
   if (glb_config.mod_dir[0] == NULL || pattern == NULL || out_paths == NULL)
      return;

   collect_overlay_assets_recursive(glb_config.mod_dir[0],
                                    glb_config.mod_dir[0],
                                    NULL, pattern, out_paths);
}

static void trim_listfile_line(char *line)
{
   char *start;
   int len;

   if (line == NULL)
      return;

   start = line;
   while (*start == ' ' || *start == '\t')
      start++;
   if (start != line)
      memmove(line, start, strlen(start) + 1);

   len = (int) strlen(line);
   while (len > 0)
   {
      char ch = line[len - 1];
      if (ch != '\r' && ch != '\n' && ch != ' ' && ch != '\t')
         break;
      line[len - 1] = 0;
      len--;
   }
}

// Walks the (listfile) inside `mpq`. When `pattern` is non-NULL the
// scope is glob-based and `asset_prefix`/`type_filter` are unused;
// otherwise the scope is the prefix + type combination.
static void collect_internal_listfile_assets(GLB_MPQ_S *mpq,
                                             const char *asset_prefix,
                                             const char *type_filter,
                                             const char *pattern,
                                             EXPORT_PATH_LIST_S *out_paths)
{
   GLB_MPQ_S *saved_mpq;
   void *buffer = NULL;
   long buf_len = 0;
   int entry;
   long i;
   char line[MPQTYPES_MAX_PATH * 2];
   int line_len = 0;

   if (mpq == NULL || out_paths == NULL)
      return;

   saved_mpq = glb_mpq;
   glb_mpq = mpq;
   entry = mpq_batch_load_in_mem("(listfile)", &buffer, &buf_len, FALSE);
   glb_mpq = saved_mpq;
   if (entry == -1 || buffer == NULL || buf_len <= 0)
   {
      if (buffer != NULL)
         free(buffer);
      return;
   }

   /* Defensive: detect uninitialised-buffer pattern (0xCD in MSVC
    * Debug). The MPQ ExtractToMem fixes (Zlib decompression + the
    * SECTOR_CRC header-entries-off-by-one) cover the cases we've
    * seen, but if a future file flag combination is still unhandled
    * we'd rather skip than feed garbage into the line scanner. */
   {
      const unsigned char *bs = (const unsigned char *) buffer;
      if (buf_len >= 16
          && (bs[0] == 0xCD || bs[0] == 0xBE)
          && bs[0] == bs[1] && bs[1] == bs[2] && bs[2] == bs[3]
          && bs[3] == bs[4] && bs[4] == bs[5] && bs[5] == bs[6])
      {
         fprintf(stderr,
            "asset_export: (listfile) in %s came back uninitialised; "
            "skipping. Supply --listfile=<path> or set "
            "[export_defaults] listfile= to work around.\n",
            mpq->file_name);
         free(buffer);
         return;
      }
   }

   for (i = 0; i < buf_len; i++)
   {
      char ch = ((const char *) buffer)[i];

      if (ch == '\n' || ch == 0)
      {
         line[line_len] = 0;
         trim_listfile_line(line);
         if (line[0] != 0)
         {
            if (pattern != NULL)
               consider_candidate_for_pattern(line, pattern, out_paths);
            else if (asset_path_matches_prefix(line, asset_prefix))
               consider_candidate_for_plan(line, type_filter, out_paths);
         }
         line_len = 0;
         continue;
      }

      if (line_len < ((int) sizeof(line) - 1))
         line[line_len++] = ch;
   }

   if (line_len > 0)
   {
      line[line_len] = 0;
      trim_listfile_line(line);
      if (line[0] != 0)
      {
         if (pattern != NULL)
            consider_candidate_for_pattern(line, pattern, out_paths);
         else if (asset_path_matches_prefix(line, asset_prefix))
            consider_candidate_for_plan(line, type_filter, out_paths);
      }
   }

   free(buffer);
}

// Walks every open MPQ's (listfile) and identify_table. When `pattern`
// is non-NULL the scope is glob-based; otherwise prefix + type-filter.
static void collect_known_mpq_assets(const char *asset_prefix,
                                     const char *type_filter,
                                     const char *pattern,
                                     EXPORT_PATH_LIST_S *out_paths)
{
   int mpq_idx;

   if (out_paths == NULL)
      return;
   if (pattern == NULL && asset_prefix == NULL)
      return;

   for (mpq_idx = 0; mpq_idx < MAX_MPQ_FILE; mpq_idx++)
   {
      GLB_MPQ_S *mpq = &glb_mpq_struct[mpq_idx];
      DWORD entry;

      if (mpq->is_open == FALSE)
         continue;

      collect_internal_listfile_assets(mpq, asset_prefix, type_filter,
                                       pattern, out_paths);

      if (mpq->filename_table == NULL || mpq->identify_table == NULL)
         continue;

      for (entry = 0; entry < mpq->count_files; entry++)
      {
         const char *asset_path;

         if ((mpq->identify_table[entry] & 0x1) == 0)
            continue;

         asset_path = mpq->filename_table + (MPQTYPES_MAX_PATH * entry);

         if (pattern != NULL)
         {
            consider_candidate_for_pattern(asset_path, pattern, out_paths);
         }
         else
         {
            if (!asset_path_matches_prefix(asset_path, asset_prefix))
               continue;
            consider_candidate_for_plan(asset_path, type_filter, out_paths);
         }
      }
   }
}

static void collect_known_mpq_assets_for_prefix(const char *asset_prefix,
                                                const char *type_filter,
                                                EXPORT_PATH_LIST_S *out_paths)
{
   collect_known_mpq_assets(asset_prefix, type_filter, NULL, out_paths);
}

static void collect_known_mpq_assets_for_pattern(const char *pattern,
                                                 EXPORT_PATH_LIST_S *out_paths)
{
   collect_known_mpq_assets(NULL, NULL, pattern, out_paths);
}

static int path_list_contains(char paths[][256], int count, const char *path)
{
   int i;

   for (i = 0; i < count; i++)
   {
      if (stricmp(paths[i], path) == 0)
         return 1;
   }

   return 0;
}

static int load_export_tables(void)
{
   if (glb_ds1edit.lvltypes_buff != NULL && glb_ds1edit.lvlprest_buff != NULL)
      return 1;

   return area_browser_init() == 0;
}

static int find_lvlprest_mask(int lvlprest_def, long *mask_out)
{
   TXT_S *txt;
   int row;
   int def_col;
   int mask_col;

   if (mask_out == NULL || !load_export_tables())
      return 0;

   txt = glb_ds1edit.lvlprest_buff;
   if (txt == NULL)
      return 0;

   def_col = misc_get_txt_column_num(RQ_LVLPREST, "Def");
   mask_col = misc_get_txt_column_num(RQ_LVLPREST, "Dt1Mask");
   if (def_col < 0 || mask_col < 0)
      return 0;

   for (row = 0; row < txt->line_num; row++)
   {
      long *def_ptr = (long *) (txt->data + (row * txt->line_size) +
         txt->col[def_col].offset);
      if (*def_ptr == lvlprest_def)
      {
         long *mask_ptr = (long *) (txt->data + (row * txt->line_size) +
            txt->col[mask_col].offset);
         *mask_out = *mask_ptr;
         return 1;
      }
   }

   return 0;
}

static int collect_dt1_paths_for_entry(int lvltype_id, int lvlprest_def,
                                       char paths[][256], int max_paths,
                                       int *path_count)
{
   TXT_S *txt;
   int row;
   int id_col;
   int file1_col;
   int col_idx;
   int slot;
   long dt1_mask = 0x7FFFFFFF;

   if (paths == NULL || path_count == NULL || max_paths <= 0)
      return 0;
   if (!load_export_tables())
      return 0;

   txt = glb_ds1edit.lvltypes_buff;
   if (txt == NULL)
      return 0;

   if (lvlprest_def >= 0)
      find_lvlprest_mask(lvlprest_def, &dt1_mask);

   id_col = misc_get_txt_column_num(RQ_LVLTYPE, "Id");
   file1_col = misc_get_txt_column_num(RQ_LVLTYPE, "File 1");
   if (id_col < 0 || file1_col < 0)
      return 0;

   for (row = 0; row < txt->line_num; row++)
   {
      long *id_ptr = (long *) (txt->data + (row * txt->line_size) +
         txt->col[id_col].offset);
      if (*id_ptr != lvltype_id)
         continue;

      for (slot = 0; slot < 32; slot++)
      {
         char relative_path[256];
         char full_path[256];

         if ((dt1_mask & (1L << slot)) == 0)
            continue;

         col_idx = file1_col + slot;
         if (col_idx >= txt->col_num)
            break;
         if (txt->col[col_idx].type != CT_STR)
            continue;

         strcpy(relative_path,
            txt->data + (row * txt->line_size) + txt->col[col_idx].offset);
         txt_convert_slash(relative_path);
         if (relative_path[0] == 0 ||
             (relative_path[0] == '0' && relative_path[1] == 0))
            continue;

         snprintf(full_path, sizeof(full_path), "%s%s",
            glb_tiles_path, relative_path);
         if (path_list_contains(paths, *path_count, full_path))
            continue;
         if (*path_count >= max_paths)
            return 1;

         strncpy(paths[*path_count], full_path, 255);
         paths[*path_count][255] = 0;
         (*path_count)++;
      }

      return 1;
   }

   return 0;
}

static int export_dt1_png(const char *asset_path, const char *output_dir)
{
   int idx, block, exported = 0;
   char out_path[1024];
   char leaf[64];
   ALLEGRO_BITMAP *bmp;
   RGBA_PALETTE *export_palette;

   idx = dt1_add((char *) asset_path);
   if (idx < 0)
      return 0;

   export_palette = asset_export_resolve_palette(asset_path);
   if (export_palette != NULL)
   {
      a5_current_palette = export_palette;
      dt1_rebuild_bitmaps_from_cache(export_palette);
   }

   for (block = 0; block < glb_dt1[idx].block_num; block++)
   {
      if (glb_dt1[idx].block_zoom[ZM_11] == NULL)
         continue;

      bmp = glb_dt1[idx].block_zoom[ZM_11][block];
      if (bmp == NULL)
         continue;

      snprintf(leaf, sizeof(leaf), "tile_%04i.png", block);
      if (!make_output_path(out_path, sizeof(out_path), output_dir, asset_path, leaf))
         continue;

      if (save_bitmap_png(out_path, bmp))
         exported++;
   }

   dt1_del(idx);
   return exported;
}

int asset_export_dcc_buffer_png(const char *asset_path, const void *buffer,
                                long len, const char *output_dir)
{
   long bitfield = 0;
   DCC_S *dcc;
   int d, f, exported = 0;
   char out_path[1024];
   char leaf[64];

   if (asset_path == NULL || buffer == NULL || output_dir == NULL || len <= 0)
      return 0;

   dcc = dcc_mem_load((void *) buffer, (int) len);
   if (dcc == NULL)
      return 0;

   if (dcc_file_header(dcc) != 0)
   {
      dcc_destroy(dcc);
      return 0;
   }

   for (d = 0; d < dcc->header.directions; d++)
      bitfield |= (1L << d);

   if (dcc_decode(dcc, bitfield) != 0)
   {
      dcc_destroy(dcc);
      return 0;
   }

   for (d = 0; d < dcc->header.directions; d++)
   {
      for (f = 0; f < dcc->header.frames_per_dir; f++)
      {
         if (dcc->frame[d][f].bmp == NULL)
            continue;

         make_anim_leaf(asset_path, leaf, sizeof(leaf), dcc->header.directions,
            dcc->header.frames_per_dir, d, f);
         if (!make_output_path(out_path, sizeof(out_path), output_dir, asset_path, leaf))
            continue;

         if (save_bitmap_png(out_path, dcc->frame[d][f].bmp))
            exported++;
      }
   }

   dcc_destroy(dcc);
   return exported;
}

static int export_dcc_png(const char *asset_path, const char *output_dir)
{
   char *buff;
   long len;
   int exported;

   if (misc_load_mpq_file((char *) asset_path, &buff, &len, FALSE) == -1)
      return 0;

   exported = asset_export_dcc_buffer_png(asset_path, buff, len, output_dir);
   free(buff);
   return exported;
}

int asset_export_dc6_buffer_png(const char *asset_path, const void *buffer,
                                long len, const char *output_dir)
{
   long *lptr, *dc6_fptr;
   long offset;
   long dc6_ver, dc6_unk1, dc6_unk2, dc6_dir, dc6_fpd;
   long f_w, f_h, f_offx, f_offy, f_x1, f_x2, f_y1, f_y2, f_len;
   UBYTE *f_data;
   int d, f, exported = 0, w, h, x1, y1, x2, y2;
   ALLEGRO_BITMAP *bmp;
   char out_path[1024];
   char leaf[64];

   if (asset_path == NULL || buffer == NULL || output_dir == NULL || len <= 0)
      return 0;

   lptr     = (long *) buffer;
   dc6_ver  = lptr[0];
   dc6_unk1 = lptr[1];
   dc6_unk2 = lptr[2];
   dc6_dir  = lptr[4];
   dc6_fpd  = lptr[5];
   dc6_fptr = &lptr[6];
   if ((dc6_ver != 6) || (dc6_unk1 != 1) || (dc6_unk2 != 0))
      return 0;

   for (d = 0; d < dc6_dir; d++)
   {
      x1 = y1 = 0x7FFFFFFF;
      x2 = y2 = 0x80000000;
      for (f = 0; f < dc6_fpd; f++)
      {
         offset = dc6_fptr[d * dc6_fpd + f];
         if (offset >= len)
            continue;

         lptr = (long *) (((UBYTE *) buffer) + offset);
         f_w    = lptr[1];
         f_h    = lptr[2];
         f_offx = lptr[3];
         f_offy = lptr[4];
         f_x1 = f_offx;
         f_x2 = f_x1 + f_w - 1;
         f_y2 = f_offy;
         f_y1 = f_y2 - f_h + 1;

         if (f_x1 < x1) x1 = (int) f_x1;
         if (f_x2 > x2) x2 = (int) f_x2;
         if (f_y1 < y1) y1 = (int) f_y1;
         if (f_y2 > y2) y2 = (int) f_y2;
      }

      w = x2 - x1 + 1;
      h = y2 - y1 + 1;
      if (w <= 0 || h <= 0)
         continue;

      for (f = 0; f < dc6_fpd; f++)
      {
         offset = dc6_fptr[d * dc6_fpd + f];
         if (offset >= len)
            continue;

         lptr = (long *) (((UBYTE *) buffer) + offset);
         f_offx = lptr[3];
         f_offy = lptr[4];
         f_len  = lptr[7];
         f_data = (UBYTE *) (&lptr[8]);

         bmp = al_create_bitmap(w, h);
         if (bmp == NULL)
            continue;

         a5_clear(bmp);
         dc6_decomp_norm(
            f_data,
            bmp,
            f_len,
            (int) (f_offx - x1),
            h - 1 + (int) (f_offy - y2)
         );

         /* For single-direction multi-frame DC6 (UI spritesheets like
          * inv1x1, inv2x3, etc.) emit a flat <basename>_NN.png in the
          * stem parent. For multi-direction or single-frame DC6, fall
          * back to the legacy nested make_anim_leaf + make_output_path
          * pair. */
         if (dc6_dir == 1 && dc6_fpd > 1)
         {
            if (make_flat_frame_output(out_path, sizeof(out_path),
                                       output_dir, asset_path,
                                       f, (int) dc6_fpd) &&
                save_bitmap_png(out_path, bmp))
               exported++;
         }
         else
         {
            make_anim_leaf(asset_path, leaf, sizeof(leaf), (int) dc6_dir,
               (int) dc6_fpd, d, f);
            if (make_output_path(out_path, sizeof(out_path), output_dir,
                                 asset_path, leaf) &&
                save_bitmap_png(out_path, bmp))
               exported++;
         }

         al_destroy_bitmap(bmp);
      }
   }

   return exported;
}

static int export_dc6_png(const char *asset_path, const char *output_dir)
{
   char *buff;
   long len;
   int exported;

   if (misc_load_mpq_file((char *) asset_path, &buff, &len, FALSE) == -1)
      return 0;

   exported = asset_export_dc6_buffer_png(asset_path, buff, len, output_dir);
   free(buff);
   return exported;
}

int asset_export_plan_for_area_group(const AREA_GROUP_S *group,
                                     ASSET_EXPORT_PLAN_S *plan_out)
{
   char tmp_paths[256][256];
   int tmp_count = 0;
   int entry_idx;
   int asset_idx;

   if (plan_out == NULL || group == NULL || group->lvltype_id < 0)
      return 0;

   asset_export_plan_init(plan_out);

   for (entry_idx = 0; entry_idx < group->entry_count; entry_idx++)
   {
      collect_dt1_paths_for_entry(
         group->entries[entry_idx].lvltype_id,
         group->entries[entry_idx].lvlprest_def,
         tmp_paths,
         256,
         &tmp_count
      );
   }

   if (tmp_count == 0)
      collect_dt1_paths_for_entry(group->lvltype_id, -1, tmp_paths, 256, &tmp_count);

   for (asset_idx = 0; asset_idx < tmp_count; asset_idx++)
      export_path_list_add(plan_out, tmp_paths[asset_idx]);

   plan_out->total_candidates = plan_out->count;
   return 1;
}

int asset_export_area_group_png(const AREA_GROUP_S *group, const char *output_dir)
{
   ASSET_EXPORT_PLAN_S plan;
   int exported_total;

   if (output_dir == NULL)
      return 0;

   if (!asset_export_plan_for_area_group(group, &plan))
      return 0;

   exported_total = asset_export_run_plan(&plan, output_dir);
   asset_export_plan_free(&plan);
   return exported_total;
}

void asset_export_plan_init(ASSET_EXPORT_PLAN_S *plan)
{
   if (plan == NULL)
      return;
   memset(plan, 0, sizeof(*plan));
}

void asset_export_plan_free(ASSET_EXPORT_PLAN_S *plan)
{
   export_path_list_destroy(plan);
}

int asset_export_plan_for_prefix(const char *asset_prefix,
                                 const char *type_filter,
                                 ASSET_EXPORT_PLAN_S *plan_out)
{
   if (plan_out == NULL || asset_prefix == NULL)
      return 0;
   if (type_filter != NULL && type_filter[0] != 0
       && stricmp(type_filter, "all") != 0
       && stricmp(type_filter, "dt1") != 0
       && stricmp(type_filter, "dc6") != 0
       && stricmp(type_filter, "dcc") != 0)
      return 0;

   asset_export_plan_init(plan_out);
   dt1_discovery_cache_reset();
   export_path_list_destroy(&g_seen_candidates);

   collect_overlay_assets_for_prefix(asset_prefix, type_filter, plan_out);
   collect_known_mpq_assets_for_prefix(asset_prefix, type_filter, plan_out);

   dt1_discovery_cache_reset();
   export_path_list_destroy(&g_seen_candidates);
   return 1;
}

int asset_export_plan_for_pattern(const char *pattern,
                                  ASSET_EXPORT_PLAN_S *plan_out)
{
   if (plan_out == NULL || pattern == NULL || pattern[0] == 0)
      return 0;

   asset_export_plan_init(plan_out);
   dt1_discovery_cache_reset();
   export_path_list_destroy(&g_seen_candidates);

   collect_overlay_assets_for_pattern(pattern, plan_out);
   collect_known_mpq_assets_for_pattern(pattern, plan_out);

   dt1_discovery_cache_reset();
   export_path_list_destroy(&g_seen_candidates);
   return 1;
}

int asset_export_run_plan(const ASSET_EXPORT_PLAN_S *plan,
                          const char *output_dir)
{
   int i;
   int exported_total = 0;

   if (plan == NULL || output_dir == NULL)
      return 0;

   for (i = 0; i < plan->count; i++)
   {
      /* Per-item progress reporting + cancellation. Dormant when no
       * export task is active (the legacy CLI export path does not
       * begin a task; this loop just runs straight through). */
      if (export_task_is_active())
      {
         export_progress_set_current_item(plan->paths[i]);
         if (export_progress_pump())
            return exported_total;
      }

      exported_total += asset_export_png(plan->paths[i], output_dir);

      if (export_task_is_active())
      {
         export_progress_advance(1);
         /* Pump again AFTER advance so the just-completed item is
          * visible on the bar (subject to the throttle window). */
         export_progress_pump();
      }
   }

   /* Final paint: the last advance() may have been throttled out, and
    * after the loop nothing else pumps until we hand off to the next
    * stage. Force a non-throttled repaint so the dialog actually shows
    * items_total/items_total. */
   if (export_task_is_active())
      export_progress_force_repaint();

   return exported_total;
}

int asset_export_prefix_png(const char *asset_prefix, const char *type_filter,
                            const char *output_dir)
{
   ASSET_EXPORT_PLAN_S plan;
   int exported_total;

   if (output_dir == NULL)
      return 0;

   if (!asset_export_plan_for_prefix(asset_prefix, type_filter, &plan))
      return 0;

   exported_total = asset_export_run_plan(&plan, output_dir);
   asset_export_plan_free(&plan);
   return exported_total;
}

int asset_export_all_png(const char *type_filter, const char *output_dir)
{
   return asset_export_prefix_png("Data", type_filter, output_dir);
}

int asset_export_png(const char *asset_path, const char *output_dir)
{
   const char *ext;

   if (asset_path == NULL || output_dir == NULL)
      return 0;

   ext = a5_get_extension(asset_path);
   if (ext == NULL)
      return 0;

   if (stricmp(ext, "dt1") == 0)
      return export_dt1_png(asset_path, output_dir);
   if (stricmp(ext, "dcc") == 0)
      return export_dcc_png(asset_path, output_dir);
   if (stricmp(ext, "dc6") == 0)
      return export_dc6_png(asset_path, output_dir);

   fprintf(stderr, "asset_export_png: unsupported asset type <%s>\n", asset_path);
   return 0;
}

extern DWORD test_tell_entry(char *filename);

int asset_export_seed_listfile_from_file(const char *file_path)
{
   FILE *fp;
   GLB_MPQ_S *saved_mpq;
   char line[MPQTYPES_MAX_PATH];
   int seeded = 0;

   if (file_path == NULL || file_path[0] == 0)
      return 0;
   fp = fopen(file_path, "r");
   if (fp == NULL)
   {
      fprintf(stderr,
         "asset_export_seed_listfile: can't open %s\n", file_path);
      return 0;
   }

   saved_mpq = glb_mpq;
   while (fgets(line, sizeof(line), fp) != NULL)
   {
      char *start;
      char *nl;
      int slot;

      /* Trim trailing CR/LF and any whitespace. */
      nl = strpbrk(line, "\r\n");
      if (nl != NULL) *nl = 0;
      {
         int n = (int) strlen(line);
         while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t'))
            line[--n] = 0;
      }
      /* Trim leading whitespace; skip blank / comment lines. */
      start = line;
      while (*start == ' ' || *start == '\t') start++;
      if (*start == 0 || *start == '#' || *start == ';') continue;

      /* Hash-lookup against each open MPQ. test_tell_entry's side
       * effect is to mark the entry as known and stash the filename
       * via mpq_batch_load_in_mem -- but test_tell_entry alone DOES
       * NOT do that side effect. So we replicate the same write
       * mpq_batch_load_in_mem does on success. */
      for (slot = 0; slot < MAX_MPQ_FILE; slot++)
      {
         GLB_MPQ_S *mpq = &glb_mpq_struct[slot];
         DWORD num_entry;

         if (mpq->is_open == FALSE) continue;

         glb_mpq = mpq;
         num_entry = test_tell_entry(start);
         if (num_entry == (DWORD) -1) continue;
         if (num_entry >= mpq->count_files) continue;

         {
            char *fn_slot =
               mpq->filename_table + (num_entry * MPQTYPES_MAX_PATH);
            strncpy(fn_slot, start, MPQTYPES_MAX_PATH - 1);
            fn_slot[MPQTYPES_MAX_PATH - 1] = 0;
            mpq->identify_table[num_entry] |= 0x1;
         }
         seeded++;
         break;
      }
   }
   glb_mpq = saved_mpq;
   fclose(fp);

   return seeded;
}