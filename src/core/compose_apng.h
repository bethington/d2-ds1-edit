#ifndef _COMPOSE_APNG_H_
#define _COMPOSE_APNG_H_

#include "core/compose_render.h"

// Glue between compose_render (produces RGBA frame buffers) and
// apng_writer (consumes RGBA frame buffers + per-frame delays). Two
// entry points:
//
//   compose_apng_write -- write an already-composed result to APNG
//   compose_apng_export -- compose + write in a single call
//
// Per-frame delay is looked up from animdata.d2 via the existing
// animdata_get_frame_delay_ms helper, keyed on the basename of the
// COF (e.g. "NEWLHTH"). Falls back to 40 ms (25 FPS) when the COF
// is not in the animdata table.

// Write a composed result to an APNG file. Returns 1 on success,
// 0 on any failure (writer-open failure, frame-write failure,
// underfilled close).
//
//   result        non-NULL; produced by compose_render
//   cof_name      basename without extension (e.g. "NEWLHTH"). Used
//                 only for animdata lookup. If NULL or unknown, the
//                 default 40 ms / frame is used.
//   output_path   target file path. Parent directory must exist.
int compose_apng_write(const COMPOSE_RENDER_RESULT_S *result,
                       const char *cof_name,
                       const char *output_path);

// One-shot: render a (token, mode, wclass, direction) tuple and
// write the result to APNG at output_path. Internally calls
// compose_render then compose_apng_write. Returns 1 on success, 0
// on any failure along the way.
int compose_apng_export(const COMPOSE_RENDER_PARAMS_S *params,
                        const char *output_path);

// Same, but also nearest-neighbour-upscale every frame by `scale`
// (must be 1, 2, or 4) before writing. Scale=1 is a pass-through and
// equivalent to compose_apng_export. The integer-NN scaler is
// pixel-perfect for D2 sprite art and preserves the APNG's animation
// (the alternative -- post-process upscale via Allegro's
// al_load_bitmap -- only sees the first frame).
int compose_apng_export_scaled(const COMPOSE_RENDER_PARAMS_S *params,
                               const char *output_path,
                               int scale);

#endif
