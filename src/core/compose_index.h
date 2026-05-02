#ifndef _COMPOSE_INDEX_H_
#define _COMPOSE_INDEX_H_

// Asset enumeration for character composition. Returns the codes
// (folder codes used in the MPQ structure) of every monster, NPC,
// and animated object available to compose-mode export, along with
// their human-readable names from the corresponding TXT tables.
//
// Player class codes (AI, AM, BA, DZ, NE, PA, SO) are hardcoded by
// compose_naming and are not enumerated here.

#define COMPOSE_TOKEN_CODE_MAX  16
#define COMPOSE_TOKEN_NAME_MAX  64
#define COMPOSE_INDEX_MAX_PER_CATEGORY 512

typedef struct COMPOSE_TOKEN_S
{
   char code[COMPOSE_TOKEN_CODE_MAX];
   char name[COMPOSE_TOKEN_NAME_MAX];
} COMPOSE_TOKEN_S;

// Build the in-memory index from the open MPQ chain + mod overlay.
// Reads Data\Global\Excel\MonStats.txt for monsters + NPCs and
// Data\Global\Excel\Objects.txt for animated objects. Returns 1 on
// success, 0 on failure (e.g. either TXT not found or unparseable).
int  compose_index_build(void);

// Reset state. Idempotent.
void compose_index_reset(void);

// Query: counts + get-by-index for each category.
int compose_index_monster_count(void);
int compose_index_npc_count(void);
int compose_index_object_count(void);
const COMPOSE_TOKEN_S * compose_index_monster_at(int idx);
const COMPOSE_TOKEN_S * compose_index_npc_at(int idx);
const COMPOSE_TOKEN_S * compose_index_object_at(int idx);

// Pure parsers, exposed for unit tests. Operate on a tab-separated
// text buffer with a header row.
//
// MonStats: emits monster entries to monster_out[] and NPC entries
// (rows where the npc column equals "1") to npc_out[]. Both arrays
// are caller-allocated; the count is returned via the *_count_out
// pointers. Returns 1 on success, 0 on parse failure.
int compose_index_parse_monstats(const char *txt_text,
                                 COMPOSE_TOKEN_S *monster_out, int monster_cap,
                                 int *monster_count_out,
                                 COMPOSE_TOKEN_S *npc_out, int npc_cap,
                                 int *npc_count_out);

// Objects: emits all rows. Returns 1 on success, 0 on parse failure.
int compose_index_parse_objects(const char *txt_text,
                                COMPOSE_TOKEN_S *out, int cap,
                                int *count_out);

#endif
