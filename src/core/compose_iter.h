#ifndef _COMPOSE_ITER_H_
#define _COMPOSE_ITER_H_

#include "core/compose_palette.h"  /* COMPOSE_CATEGORY_E */

// Helpers that drive the compose-mode export iteration. These tie
// together the picker output (chosen modes / weapons / category) with
// the per-tuple primitives (compose_apng_export). The caller (the
// action_export_compose function in project_menu.c) walks the
// (category, token, mode, weapon, direction) tuple space using these
// accessors and the compose_index enumerators, calling
// compose_apng_export for each tuple.
//
// Three pieces:
//
//   1. Default (hardcoded) mode / weapon / player-class lists. These
//      back the picker's "All" entry: when the user keeps "All"
//      checked, the iteration walks the entire hardcoded set.
//
//   2. compose_iter_build_output_path: builds the per-tuple output
//      path under the user-selected output root. Respects the
//      compose_use_full_folder_names config flag (folder names full
//      vs codes; filenames always use codes).
//
//   3. compose_iter_probe_direction_count: opens the COF, parses it,
//      returns direction_count (8, 16, etc.) so the caller knows how
//      many direction APNGs to emit per (token, mode, weapon) tuple.
//      Returns 0 if the COF doesn't exist (so the caller can skip the
//      whole tuple cheaply without engaging compose_render).

// ---- Default mode/weapon/class lists ------------------------------

// Number of entries in each list.
int compose_iter_default_mode_count(void);
int compose_iter_default_weapon_count(void);
int compose_iter_player_class_count(void);

// Get an entry by index. Returns NULL on out-of-range. Returned
// pointer is to a NUL-terminated string with static lifetime.
const char * compose_iter_default_mode_at(int idx);
const char * compose_iter_default_weapon_at(int idx);
const char * compose_iter_player_class_at(int idx);

// ---- Per-category metadata ----------------------------------------

// MPQ-virtual base path for assets in the given category. Returns
// NULL for COMPOSE_CATEGORY_NONE.
//   PLAYER_CHAR -> "data\\global\\chars"
//   MONSTER     -> "data\\global\\monsters"
//   NPC         -> "data\\global\\npc"
//   OBJECT      -> "data\\global\\objects"
const char * compose_iter_category_base(COMPOSE_CATEGORY_E category);

// Skin code used in DCC filenames. "LIT" for player chars, "" for
// monsters / NPCs / objects. Returns "" for COMPOSE_CATEGORY_NONE.
const char * compose_iter_category_skin(COMPOSE_CATEGORY_E category);

// Short label suitable for output folder names. "Player_Characters",
// "Monsters", "NPCs", "Objects". Returns "Composed" for NONE.
const char * compose_iter_category_folder(COMPOSE_CATEGORY_E category);

// ---- Output path builder ------------------------------------------

// Build the full output path for one (category, token, mode, weapon,
// direction) tuple. Mirrors the source-asset structure underneath
// the user-selected root:
//
//   <root>\<category_folder>\<token_folder>\<token><mode><weapon>_dirN.png
//
// When the compose_use_full_folder_names config flag is set, the
// per-token folder uses the full descriptive name (e.g.
// "Necromancer", "Andariel") sanitized for filesystem use; otherwise
// it uses the bare code. Filenames always use the compact codes
// (per the locked Q9b decision: folder full / filename codes).
//
//   token_full_name  may be NULL or empty; the builder falls back to
//                    the code when the full name is missing.
//
// Returns 1 on success (out_buf populated), 0 on bad arguments or
// buffer overflow (out_buf set to "").
int compose_iter_build_output_path(char *out_buf, int out_cap,
                                   const char *root,
                                   COMPOSE_CATEGORY_E category,
                                   const char *token,
                                   const char *token_full_name,
                                   const char *mode,
                                   const char *wclass,
                                   int direction);

// Build the parent directory of the output path (i.e. without the
// final filename). Same semantics as compose_iter_build_output_path
// but stops at the per-token folder. Useful for the mkdir-recursive
// step before writing the APNG file.
int compose_iter_build_output_dir(char *out_buf, int out_cap,
                                  const char *root,
                                  COMPOSE_CATEGORY_E category,
                                  const char *token,
                                  const char *token_full_name);

// Best-effort recursive mkdir for the given absolute path (every
// component is created if missing). Returns 1 on success or if the
// directory already exists. Returns 0 on any other failure (e.g. a
// component exists as a regular file).
int compose_iter_ensure_dir(const char *path);

// Read the "use full folder names" flag. Production reads
// glb_config.compose_use_full_folder_names; tests stub this directly
// to control the path builder's behaviour without dragging in the
// full CONFIG_S struct + its transitive Allegro deps.
int compose_iter_use_full_folder_names(void);

// ---- Direction-count probe ----------------------------------------

// Open + parse the COF for the given (category, token, mode, wclass)
// tuple and return its direction_count. Returns 0 if the COF cannot
// be loaded or parsed (so the caller skips the entire tuple
// cheaply). The returned count is also the upper bound on the
// `direction` parameter to compose_apng_export.
int compose_iter_probe_direction_count(COMPOSE_CATEGORY_E category,
                                       const char *token,
                                       const char *mode,
                                       const char *wclass);

#endif
