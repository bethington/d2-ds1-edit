#include "core/compose_palette.h"

int compose_palette_resolve_act(COMPOSE_CATEGORY_E category,
                                const char *token)
{
   /* Player chars + NPCs are stable across acts and always render
    * with Act 1's palette per the locked Q10 decision. Monsters and
    * objects resolve to their natural act via monstats.txt + levels.txt
    * in the v2 follow-up; for now they fall back to Act 1 too -- a
    * sensible default that matches what the editor's preview pipeline
    * uses by default. */
   (void) category;
   (void) token;
   return 1;
}
