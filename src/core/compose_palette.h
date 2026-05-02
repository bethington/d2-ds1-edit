#ifndef _COMPOSE_PALETTE_H_
#define _COMPOSE_PALETTE_H_

// Per-asset palette resolution for compose-mode export, per Q10 of
// the planning doc. Player chars / NPCs use Act 1; monsters and
// objects derive their natural act from monstats.txt + levels.txt
// (or objects.txt + levels.txt).
//
// v1 implementation: returns 1 (Act 1) for all categories. The
// proper TXT-driven join is intentionally deferred to a follow-up
// once the rest of compose-mode (UI, discovery, end-to-end smoke)
// is in place. Most D2 sprites look identical across acts (the
// palette indices fall in stable color regions); the visible
// differences are a minority of assets where Q10 follow-up would
// matter. Shipping the API surface now lets the rest of the
// composer wire calls in correctly without blocking on the join
// implementation.

typedef enum COMPOSE_CATEGORY_E
{
   COMPOSE_CATEGORY_NONE        = 0,
   COMPOSE_CATEGORY_PLAYER_CHAR = 1,
   COMPOSE_CATEGORY_MONSTER     = 2,
   COMPOSE_CATEGORY_NPC         = 3,
   COMPOSE_CATEGORY_OBJECT      = 4
} COMPOSE_CATEGORY_E;

// Resolve the act number (1..5) for the given asset. Returns 1 for
// all categories in v1. token may be NULL for player chars / NPCs
// (which always return 1 regardless).
int compose_palette_resolve_act(COMPOSE_CATEGORY_E category,
                                const char *token);

#endif
