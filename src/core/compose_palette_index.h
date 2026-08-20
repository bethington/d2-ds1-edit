#ifndef _COMPOSE_PALETTE_INDEX_H_
#define _COMPOSE_PALETTE_INDEX_H_

#include "core/compose_palette.h"  /* COMPOSE_CATEGORY_E */

// Per-asset palette resolution v2: a Levels.txt-driven join that maps
// each monster (or NPC, or object) to its natural Act so the right
// palette can be active when the sprite is composed.
//
// The mapping is built like this:
//   1. Parse Levels.txt -- each row has an Act column (0..4 for the
//      five acts) and the columns mon1..mon10, nmon1..nmon10,
//      umon1..umon10 (Normal/Nightmare/Unique monster spawn slots).
//   2. Each monNN value is a MonStats.Id string (e.g. "zombie3").
//   3. For each (level.Act, mon_id) pair seen, record Act as the
//      monster's natural act. First seen wins.
//
// Lookup goes: monster Code -> compose_index entry -> MonStatsEx Id
// -> palette index. Returns 1 (Act 1) when nothing matches.
//
// Player chars / NPCs / objects don't have a "natural act" the same
// way -- they always render against Act 1. This module is monsters
// only.

// Build the index by parsing data\Global\Excel\Levels.txt out of the
// open MPQ chain. Idempotent. Returns 1 on success, 0 if Levels.txt
// can't be loaded or parsed.
int compose_palette_index_build(void);

// Reset state. Idempotent.
void compose_palette_index_reset(void);

// Lookup the natural act (1..5) for a monster Id. Returns 1 if not
// found OR if the index hasn't been built.
int compose_palette_index_act_for_monster_id(const char *monstats_id);

// Pure parser, exposed for unit tests. Walks a tab-separated TXT
// buffer with a header row. For each data row, reads Act and
// mon1..mon10 / nmon1..nmon10 / umon1..umon10 monster Id strings,
// emitting (mon_id, act) pairs into the user-supplied callback. The
// callback is responsible for first-wins semantics.
//
// Returns 1 on success, 0 on missing required columns.
int compose_palette_index_parse_levels(
   const char *txt_text,
   void (*emit)(const char *mon_id, int act, void *userdata),
   void *userdata);

#endif
