/* MPQ + COF-parsing glue for compose_iter. Kept in its own TU so the
 * pure helpers in compose_iter.c link cleanly into unit tests without
 * misc_load_mpq_file or compose_cof transitive dependencies. */

#include <stdlib.h>
#include <string.h>

#include "core/compose_cof.h"
#include "core/compose_cof_path.h"
#include "core/compose_iter.h"

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

int compose_iter_probe_direction_count_resolve(COMPOSE_CATEGORY_E category,
                                               const char *token,
                                               const char *mode,
                                               const char *wclass_in,
                                               char *out_wclass,
                                               int   out_wclass_cap)
{
   int dirs;
   const char *primary = wclass_in != NULL ? wclass_in : "";

   /* First try the caller's preferred wclass. This is what player
    * chars want -- the wclass is meaningful and varies. */
   dirs = probe_one(category, token, mode, primary, out_wclass, out_wclass_cap);
   if (dirs > 0) return dirs;

   /* For non-chars, the COF filename usually has "HTH" as a placeholder
    * even when the monster has no real weapon (e.g. data\global\
    * monsters\AN\COF\ANNUHTH.cof for Andariel). Fall back to that. */
   if (category != COMPOSE_CATEGORY_PLAYER_CHAR
       && (primary[0] == 0 || strcmp(primary, "HTH") != 0))
   {
      dirs = probe_one(category, token, mode, "HTH", out_wclass, out_wclass_cap);
      if (dirs > 0) return dirs;
   }

   return 0;
}
