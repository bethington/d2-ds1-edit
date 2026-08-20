#ifndef _MONSTATS2_H_
#define _MONSTATS2_H_

// Per-monster sprite info from D2's MonStats2.txt. The asset paths a
// monster's sprite lives under are:
//
//   data\global\monsters\<token>\COF\<token><mode><basew>.cof
//   data\global\monsters\<token>\<layer>\<token><layer><skin><mode><wclass>.dcc
//
// where:
//   - token: typically MonStats.Code (e.g. "SK" for skeleton)
//   - basew: MonStats2.BaseW (e.g. "1hs" for skeleton, "hth" for zombie)
//   - skin:  per-layer; first entry of MonStats2.<layer>v (e.g. "LIT" for HD)
//   - wclass: per-layer; comes from the parsed COF's per-layer weapon_class
//             field (or BaseW if the COF says empty)
//
// Player chars don't go through this module -- their layout is
// hardcoded by compose_naming + compose_iter. This module is monsters
// / NPCs / objects.

#define MONSTATS2_LAYER_COUNT      16   /* HD,TR,LG,RA,LA,RH,LH,SH,S1..S8 */
#define MONSTATS2_ID_MAX           48
#define MONSTATS2_BASEW_MAX         8
#define MONSTATS2_SKIN_MAX          8

typedef struct MONSTATS2_LAYER_S
{
   int  used;                        /* 1 if this layer's HD=1 (etc.) */
   char skin[MONSTATS2_SKIN_MAX];    /* first entry of HDv / TRv / ... */
} MONSTATS2_LAYER_S;

typedef struct MONSTATS2_ENTRY_S
{
   char id[MONSTATS2_ID_MAX];        /* MonStats2.Id (e.g. "skeleton1") */
   char basew[MONSTATS2_BASEW_MAX];  /* MonStats2.BaseW (e.g. "1hs") */
   MONSTATS2_LAYER_S layers[MONSTATS2_LAYER_COUNT];
} MONSTATS2_ENTRY_S;

// Pure parser, exposed for unit tests. Walks a tab-separated TXT
// buffer with a header row; for each data row, reads Id / BaseW /
// per-layer used flag + first variant. Returns 1 on success, 0 on
// malformed header (missing required column). out is a caller-
// allocated array of capacity `cap`; the actual count is written to
// *count_out.
int monstats2_parse(const char *txt_text,
                    MONSTATS2_ENTRY_S *out, int cap, int *count_out);

// Build the in-memory index from the open MPQ chain. Reads
// data\global\excel\MonStats2.txt. Returns 1 on success, 0 on
// failure. Idempotent.
int monstats2_build(void);

// Reset state. Idempotent.
void monstats2_reset(void);

// Lookup by Id (case-insensitive). Returns NULL if not found OR if
// the index hasn't been built yet.
const MONSTATS2_ENTRY_S * monstats2_find(const char *id);

#endif
