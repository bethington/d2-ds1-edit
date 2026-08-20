#include <string.h>

#ifdef _WIN32
#define cpres_stricmp _stricmp
#else
#include <strings.h>
#define cpres_stricmp strcasecmp
#endif

#include "core/compose_index.h"
#include "core/compose_palette.h"
#include "core/compose_palette_index.h"

int compose_palette_resolve_act(COMPOSE_CATEGORY_E category,
                                const char *token)
{
   /* Player chars + NPCs are stable across acts and always render
    * with Act 1's palette. NPCs in particular live in the towns of
    * each act but their sprites are drawn at the player-char palette,
    * not the per-act monster palette.
    *
    * Monsters: join compose_index.Code -> MonStatsEx -> Levels.txt
    * via compose_palette_index. Returns Act 1 when the join misses
    * (boss-only monsters in level rows we didn't recognise, mod
    * monsters not present in Levels.txt, etc.).
    *
    * Objects: stay at Act 1 -- they're rendered in the area's local
    * palette which the GUI sets independently. */
   if (category != COMPOSE_CATEGORY_MONSTER)
      return 1;
   if (token == NULL || token[0] == 0)
      return 1;

   /* Walk compose_index to find this monster Code's MonStatsEx Id. */
   {
      int i;
      for (i = 0; i < compose_index_monster_count(); i++)
      {
         const COMPOSE_TOKEN_S *t = compose_index_monster_at(i);
         if (t == NULL) continue;
         if (cpres_stricmp(t->code, token) != 0) continue;
         if (t->mon_stats_ex[0] == 0) return 1;
         return compose_palette_index_act_for_monster_id(t->mon_stats_ex);
      }
   }
   return 1;
}
