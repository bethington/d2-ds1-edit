#ifndef _COMPOSE_COF_H_
#define _COMPOSE_COF_H_

// Pure parser for the D2 COF (Component Object File) byte format,
// independent of the editor's runtime state. The existing
// anim_load_cof is keyed on obj.txt entries (an editor-specific
// concept); compose-mode export needs a parser that takes a buffer
// and produces a portable struct describing the layers, layer
// metadata, and priority table.
//
// COF byte layout:
//   offset 0  : 1 byte   layer count (1..16)
//   offset 1  : 1 byte   frames per direction
//   offset 2  : 1 byte   direction count
//   offset 3  : 1 byte   version
//   offset 4..28      : 25 unknown bytes (skipped)
//   offset 29..       : per-layer info (lay_count entries of 9 bytes each):
//                       1 byte  composit_index (0..15: HD,TR,LG,RA,LA,RH,LH,SH,S1..S8)
//                       1 byte  shadow A
//                       1 byte  shadow B
//                       1 byte  transparency A
//                       1 byte  transparency B
//                       4 bytes weapon-class hint (NUL-padded ASCII like "HTH ", "1HS ")
//   then            : frames-per-direction bytes of frame flags (skipped here)
//   then            : direction*fpd*lay_count bytes of priority table
//                     (z-order index for each (direction, frame, slot))

#define COMPOSE_COF_MAX_LAYERS  16

typedef struct COMPOSE_COF_LAYER_S
{
   unsigned char composit_index;       /* 0..15 = HD..S8 */
   unsigned char shadow_a;
   unsigned char shadow_b;
   unsigned char trans_a;
   unsigned char trans_b;
   char          weapon_class[5];      /* NUL-terminated trimmed copy */
} COMPOSE_COF_LAYER_S;

typedef struct COMPOSE_COF_S
{
   int                 layer_count;
   int                 frames_per_dir;
   int                 direction_count;
   int                 version;
   COMPOSE_COF_LAYER_S layers[COMPOSE_COF_MAX_LAYERS];
   /* priority is a 1-D array of length (direction_count * frames_per_dir
    * * layer_count). priority[((d * fpd) + f) * layer_count + n] gives
    * the n-th layer (in render order) for frame f in direction d. */
   unsigned char       *priority;
   long                priority_len;
} COMPOSE_COF_S;

// Parse a COF byte buffer into a COMPOSE_COF_S. Returns 1 on success,
// 0 on parse failure (truncated input, absurd dimensions). The caller
// owns the returned struct and must call compose_cof_free.
int  compose_cof_parse(const void *bytes, long len, COMPOSE_COF_S *out);

// Free heap data owned by a parsed COF. Safe to call on a zeroed COF.
void compose_cof_free(COMPOSE_COF_S *cof);

// Convenience: get the priority byte for a given (dir, frame,
// render_order_slot). render_order is 0..layer_count-1; slot value is
// the composit_index of the layer to draw at that slot.
int  compose_cof_priority_at(const COMPOSE_COF_S *cof,
                             int direction, int frame, int order_slot);

#endif
