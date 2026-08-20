/* Production glue between compose_iter and the editor's global
 * CONFIG_S. Kept in its own TU so compose_iter.c links cleanly into
 * unit tests without pulling in structs.h (which transitively pulls
 * in Allegro). */

#include "structs.h"
#include "core/compose_iter.h"

int compose_iter_use_full_folder_names(void)
{
   return glb_config.compose_use_full_folder_names ? 1 : 0;
}
