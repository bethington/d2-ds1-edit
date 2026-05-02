/* MPQ-side glue for compose_index. Kept in its own translation unit
 * so the pure parsers in compose_index.c link cleanly into unit tests
 * without dragging in misc_load_mpq_file (which has a heavy
 * transitive global-state dependency). */

#include <stdlib.h>

#include "core/compose_index.h"

extern int misc_load_mpq_file(char *filename, char **buffer, long *buf_len, int output);

/* These three live in compose_index.c. */
extern int compose_index_parse_monstats(const char *txt_text,
                                        COMPOSE_TOKEN_S *monster_out, int monster_cap,
                                        int *monster_count_out,
                                        COMPOSE_TOKEN_S *npc_out, int npc_cap,
                                        int *npc_count_out);
extern int compose_index_parse_objects(const char *txt_text,
                                       COMPOSE_TOKEN_S *out, int cap,
                                       int *count_out);

/* Module storage shared with compose_index.c via these accessor
 * extern declarations. To keep the wiring simple we re-declare the
 * arrays here using the same names; the linker will deduplicate. */
extern COMPOSE_TOKEN_S compose_index_storage_monsters[COMPOSE_INDEX_MAX_PER_CATEGORY];
extern COMPOSE_TOKEN_S compose_index_storage_npcs[COMPOSE_INDEX_MAX_PER_CATEGORY];
extern COMPOSE_TOKEN_S compose_index_storage_objects[COMPOSE_INDEX_MAX_PER_CATEGORY];
extern int compose_index_storage_monster_count;
extern int compose_index_storage_npc_count;
extern int compose_index_storage_object_count;

extern void compose_index_reset(void);

int compose_index_build(void)
{
   char *buf = NULL;
   long buf_len = 0;
   int ok_mon = 0, ok_obj = 0;

   compose_index_reset();

   if (misc_load_mpq_file("Data\\Global\\Excel\\MonStats.txt",
                          &buf, &buf_len, 0) != -1
       && buf != NULL)
   {
      int mc = 0, nc = 0;
      ok_mon = compose_index_parse_monstats(buf,
         compose_index_storage_monsters, COMPOSE_INDEX_MAX_PER_CATEGORY, &mc,
         compose_index_storage_npcs,     COMPOSE_INDEX_MAX_PER_CATEGORY, &nc);
      compose_index_storage_monster_count = mc;
      compose_index_storage_npc_count     = nc;
      free(buf);
      buf = NULL;
   }

   if (misc_load_mpq_file("Data\\Global\\Excel\\Objects.txt",
                          &buf, &buf_len, 0) != -1
       && buf != NULL)
   {
      int oc = 0;
      ok_obj = compose_index_parse_objects(buf,
         compose_index_storage_objects, COMPOSE_INDEX_MAX_PER_CATEGORY, &oc);
      compose_index_storage_object_count = oc;
      free(buf);
   }

   return (ok_mon && ok_obj) ? 1 : 0;
}
