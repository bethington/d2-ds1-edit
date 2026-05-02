#ifndef _COMPOSE_CATEGORY_PICKER_H_
#define _COMPOSE_CATEGORY_PICKER_H_

#include "core/compose_palette.h"  /* COMPOSE_CATEGORY_E */

// Category picker shown after the user confirms compose mode in the
// follow-up modal. Five entries:
//
//   All composed
//   Player characters
//   Monsters
//   NPCs
//   Objects
//
// On confirm, *out is set to the chosen category (or COMPOSE_CATEGORY_NONE
// for "All composed", which the discovery loop interprets as "iterate
// each of the four narrow categories"). Returns 1 on confirm, 0 on
// cancel.
int compose_category_picker_show(COMPOSE_CATEGORY_E *out);

#endif
