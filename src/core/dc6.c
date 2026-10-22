/* Derived from win_ds1edit by Paul Siramy (originally dc6info.c).
 * See NOTICE at the repository root for attribution and license status. */

#include "structs.h"
#include <stdint.h>
#include <stdlib.h>
#include "misc.h"
#include "core/dc6.h"
#include "core/cof.h"
#include "core/index_frame.h"


// ==========================================================================
int anim_load_dc6(char * name, COF_S * cof, int lay_idx, long user_dir,
                  UBYTE * palshift)
{
   LAY_INF_S           * lay;
   int                 entry, rc, w, h, off_x, off_y;
   char                * buff;
   long                dir = 0, len;
   int32_t             dc6_dir;
   DS1EDIT_INDEX_FRAME * frames = NULL;
   size_t              count = 0;
   UBYTE               ** canvases = NULL;


   if ((cof == NULL) || (lay_idx >= COMPOSIT_NB))
      return 1;
   lay = & cof->lay_inf[lay_idx];

   // load dc6 file
   entry = misc_load_mpq_file(name, & buff, & len, FALSE);
   if (entry == -1)
      return 1;
   if (len < 24)
   {
      free(buff);
      return 1;
   }

   // valid direction ?
   dc6_dir = *(const int32_t *) (buff + 16);
   if (dc6_dir != cof->dir)
   {
      free(buff);
      return 1;
   }

   // choose direction
   switch (cof->dir)
   {
      case  1 : dir = glb_ds1edit.new_dir1[user_dir];  break;
      case  4 : dir = glb_ds1edit.new_dir4[user_dir];  break;
      case  8 : dir = glb_ds1edit.new_dir8[user_dir];  break;
      case 16 : dir = glb_ds1edit.new_dir16[user_dir]; break;
      case 32 : dir = glb_ds1edit.new_dir32[user_dir]; break;
   }

   // decode the direction to palette indices, then lay it out
   rc = ! dc6_decode_indices(buff, (size_t) len, (int) dir, & frames, & count,
                             NULL, 0);
   free(buff);
   if (rc)
      return 1;

   rc = ds1edit_index_frames_to_canvases(frames, count, & canvases, & w, & h,
                               & off_x, & off_y);
   if (rc == 0)
      rc = anim_layer_from_index_frames(lay, canvases, (int) count, w, h,
                                        off_x, off_y, palshift);

   ds1edit_index_canvases_free(canvases, count);
   ds1edit_index_frames_free(frames, count);
   return rc;
}
