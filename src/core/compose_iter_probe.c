/* MPQ + COF-parsing glue for compose_iter. Kept in its own TU so the
 * pure helpers in compose_iter.c link cleanly into unit tests without
 * misc_load_mpq_file or compose_cof transitive dependencies. */

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define probe_strcasecmp _stricmp
#else
#include <strings.h>
#define probe_strcasecmp strcasecmp
#endif

#include "core/compose_cof.h"
#include "core/compose_cof_path.h"
#include "core/compose_index.h"
#include "core/compose_iter.h"
#include "core/monstats2.h"

extern int misc_load_mpq_file(char *filename, char **buffer, long *buf_len, int output);

/* Try one specific (category, token, mode, wclass). On success returns
 * the direction count and copies the wclass tried into out_wclass when
 * non-NULL. */
static int probe_one(COMPOSE_CATEGORY_E category,
                     const char *token, const char *mode,
                     const char *wclass,
                     char *out_wclass, int out_cap)
{
   const char *base;
   char path[512];
   char *buf = NULL;
   long buf_len = 0;
   COMPOSE_COF_S cof;
   int dirs = 0;

   base = compose_iter_category_base(category);
   if (base == NULL) return 0;
   if (token == NULL || token[0] == 0) return 0;
   if (mode  == NULL || mode [0] == 0) return 0;
   if (wclass == NULL) wclass = "";

   if (!compose_cof_path_build(path, (int) sizeof(path),
                               base, token, mode, wclass))
      return 0;
   if (misc_load_mpq_file(path, &buf, &buf_len, 0) == -1 || buf == NULL)
      return 0;

   memset(&cof, 0, sizeof(cof));
   if (compose_cof_parse(buf, buf_len, &cof))
      dirs = cof.direction_count;
   compose_cof_free(&cof);
   free(buf);

   if (dirs > 0 && out_wclass != NULL && out_cap > 0)
   {
      strncpy(out_wclass, wclass, (size_t) out_cap - 1);
      out_wclass[out_cap - 1] = 0;
   }
   return dirs;
}

int compose_iter_probe_direction_count(COMPOSE_CATEGORY_E category,
                                       const char *token,
                                       const char *mode,
                                       const char *wclass)
{
   return probe_one(category, token, mode,
                    wclass != NULL ? wclass : "", NULL, 0);
}

/* Look up the MonStats2 BaseW for a monster Code. Walks compose_index
 * to find the matching (Code, MonStatsEx) pair, then queries MonStats2
 * by MonStatsEx Id. Returns NULL on miss. */
static const char *resolve_basew_for_monster(const char *code)
{
   int i;
   for (i = 0; i < compose_index_monster_count(); i++)
   {
      const COMPOSE_TOKEN_S *t = compose_index_monster_at(i);
      const MONSTATS2_ENTRY_S *e;
      if (t == NULL) continue;
      if (probe_strcasecmp(t->code, code) != 0) continue;
      if (t->mon_stats_ex[0] == 0) return NULL;
      e = monstats2_find(t->mon_stats_ex);
      if (e == NULL) return NULL;
      return e->basew[0] != 0 ? e->basew : NULL;
   }
   /* Not found in monster list; check NPC list for completeness. */
   for (i = 0; i < compose_index_npc_count(); i++)
   {
      const COMPOSE_TOKEN_S *t = compose_index_npc_at(i);
      const MONSTATS2_ENTRY_S *e;
      if (t == NULL) continue;
      if (probe_strcasecmp(t->code, code) != 0) continue;
      if (t->mon_stats_ex[0] == 0) return NULL;
      e = monstats2_find(t->mon_stats_ex);
      if (e == NULL) return NULL;
      return e->basew[0] != 0 ? e->basew : NULL;
   }
   return NULL;
}

int compose_iter_probe_direction_count_resolve(COMPOSE_CATEGORY_E category,
                                               const char *token,
                                               const char *mode,
                                               const char *wclass_in,
                                               char *out_wclass,
                                               int   out_wclass_cap)
{
   int dirs;
   const char *primary = wclass_in != NULL ? wclass_in : "";

   /* First try the caller's preferred wclass. */
   dirs = probe_one(category, token, mode, primary, out_wclass, out_wclass_cap);
   if (dirs > 0) return dirs;

   /* For monsters / NPCs, the COF filename's wclass component is
    * MonStats2.BaseW (e.g. "1hs" for skeleton, "hth" for zombie,
    * empty for some bosses). Try the MonStats2-derived value. */
   if (category == COMPOSE_CATEGORY_MONSTER
       || category == COMPOSE_CATEGORY_NPC)
   {
      const char *basew = resolve_basew_for_monster(token);
      if (basew != NULL && (primary[0] == 0
                            || probe_strcasecmp(primary, basew) != 0))
      {
         dirs = probe_one(category, token, mode, basew,
                          out_wclass, out_wclass_cap);
         if (dirs > 0) return dirs;
      }
   }

   /* Last-ditch fallback: try "HTH" for any non-char category. Some
    * monsters (Andariel et al.) genuinely use HTH as a placeholder
    * even though they have no MonStats2.BaseW set. */
   if (category != COMPOSE_CATEGORY_PLAYER_CHAR
       && (primary[0] == 0 || probe_strcasecmp(primary, "HTH") != 0))
   {
      dirs = probe_one(category, token, mode, "HTH",
                       out_wclass, out_wclass_cap);
      if (dirs > 0) return dirs;
   }

   return 0;
}
