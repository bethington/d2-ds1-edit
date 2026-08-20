#ifndef _COMPOSE_RENDER_H_
#define _COMPOSE_RENDER_H_

// Composer driver: load a COF file via the MPQ chain, parse it,
// load each referenced DCC layer, decode the requested direction's
// frames, and blend the layers in COF-priority order onto a single
// canvas per frame. Output is an array of per-frame RGBA buffers
// suitable for direct hand-off to the APNG writer.
//
// This is the integration piece that ties together all the pure-
// logic primitives shipped in earlier commits:
//   compose_cof_path_build     <- COF virtual path
//   compose_cof_parse          <- COF byte parser
//   compose_dcc_path_build     <- per-layer DCC virtual path
//   misc_load_mpq_file         <- MPQ + overlay loader
//   dcc_mem_load + dcc_decode  <- existing DCC primitives
//   compose_bbox_union         <- canvas sizing
//   compose_blit_rgba          <- RGBA-buffer blit
//
// Requires Allegro to be initialised (DCC decoding allocates
// ALLEGRO_BITMAPs internally) and the MPQ chain + a5_current_palette
// to be set up. Must therefore run on the editor's main thread.

/* Layer skin codes are short -- "lit", "med", "hvy", "nil", "axe",
 * etc. -- and fit in 8 chars including NUL. */
#define COMPOSE_RENDER_SKIN_MAX  8
#define COMPOSE_RENDER_LAYER_COUNT  16

typedef struct COMPOSE_RENDER_PARAMS_S
{
   const char *base;       /* "data\\global\\chars" or "...\\monsters" etc. */
   const char *token;      /* "NE", "AN", "TownPortal" */
   const char *mode;       /* "WL", "NU", "A1" */
   const char *wclass;     /* "HTH", "1HS", "" for monsters with no weapon */
   const char *skin;       /* "LIT" for chars; default for layers when the
                            * per-layer override below is empty. */
   int         direction;  /* 0..N-1 (no remapping in v1) */

   /* Optional per-layer skin override, indexed by COF composit_index
    * (HD=0, TR=1, ..., S8=15). For monsters, each layer can have a
    * different skin variant ("lit" for HD, "axe" for RH, "nil" for
    * SH, etc.) per MonStats2. Empty string means "fall back to
    * params->skin". Player chars leave this empty -> uniform LIT. */
   char skin_per_layer[COMPOSE_RENDER_LAYER_COUNT][COMPOSE_RENDER_SKIN_MAX];
} COMPOSE_RENDER_PARAMS_S;

typedef struct COMPOSE_RENDER_RESULT_S
{
   int width;
   int height;
   int frame_count;
   unsigned char **frames;  /* frames[i] = width*height*4 bytes RGBA */
} COMPOSE_RENDER_RESULT_S;

// Compose one (token, mode, wclass, direction) tuple. Returns 1 on
// success (out populated), 0 on any of: COF load/parse failure,
// invalid direction, no layers loadable, allocation failure. Caller
// frees the result via compose_render_free regardless of return.
int  compose_render(const COMPOSE_RENDER_PARAMS_S *params,
                    COMPOSE_RENDER_RESULT_S *out);

// Free heap data owned by a COMPOSE_RENDER_RESULT_S and zero it.
// Safe to call on a zeroed/uninitialised result.
void compose_render_free(COMPOSE_RENDER_RESULT_S *result);

#endif
