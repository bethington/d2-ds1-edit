#include <stdlib.h>
#include <string.h>

#include "core/compose_cof.h"

void compose_cof_free(COMPOSE_COF_S *cof)
{
   if (cof == NULL)
      return;
   if (cof->priority != NULL)
   {
      free(cof->priority);
      cof->priority = NULL;
   }
   cof->priority_len = 0;
}

int compose_cof_parse(const void *bytes, long len, COMPOSE_COF_S *out)
{
   const unsigned char *p;
   long pos = 0;
   /* Header layout: 4 bytes (layers, fpd, dirs, version) followed by
    * 24 bytes of padding/bounds/anim_speed that we don't need. Total
    * header is 28 bytes -- matches the working anim_load_cof in
    * core/cof.c which reads 3 bytes + skips 25 = 28. Earlier this
    * was 4+25=29 (off by one), which misaligned the per-layer reads
    * and made every COF parse fail. */
   long header_size = 4 + 24;
   long per_layer = 9;
   long required;
   int i;

   if (out == NULL || bytes == NULL || len <= 0)
      return 0;

   memset(out, 0, sizeof(*out));

   p = (const unsigned char *) bytes;

   if (len < header_size)
      return 0;

   out->layer_count    = p[0];
   out->frames_per_dir = p[1];
   out->direction_count= p[2];
   out->version        = p[3];

   if (out->layer_count    <= 0 || out->layer_count    > COMPOSE_COF_MAX_LAYERS)
      return 0;
   if (out->frames_per_dir <= 0 || out->frames_per_dir > 1024)
      return 0;
   if (out->direction_count<= 0 || out->direction_count > 64)
      return 0;

   pos = header_size;

   /* Each layer info block is 9 bytes. */
   required = pos + per_layer * out->layer_count;
   if (len < required)
      return 0;

   for (i = 0; i < out->layer_count; i++)
   {
      const unsigned char *q = p + pos + (long) i * per_layer;
      unsigned char idx = q[0];
      COMPOSE_COF_LAYER_S *lay;
      int wc;
      int trim;

      if (idx >= COMPOSE_COF_MAX_LAYERS)
         return 0;
      lay = &out->layers[idx];

      /* Some boss COFs (Baal Throne, Mephisto, ...) declare the same
       * composit_index more than once -- typically a back layer + a
       * front layer of the same body part with different z-order in
       * the priority table. Our per-layer storage is keyed by
       * composit_index, so duplicates would otherwise stomp the
       * first entry's metadata. Keep first-wins: if this slot
       * already has a non-empty weapon_class, skip overwriting. */
      if (lay->weapon_class[0] != 0)
         continue;

      lay->composit_index = idx;
      lay->shadow_a       = q[1];
      lay->shadow_b       = q[2];
      lay->trans_a        = q[3];
      lay->trans_b        = q[4];

      memcpy(lay->weapon_class, q + 5, 4);
      lay->weapon_class[4] = 0;
      /* Trim trailing whitespace / control characters from the
       * weapon_class field. */
      trim = 4;
      while (trim > 0)
      {
         char c = lay->weapon_class[trim - 1];
         if (c == ' ' || c == '\t' || c == 0)
            lay->weapon_class[--trim] = 0;
         else
            break;
      }
      (void) wc;
   }
   pos += per_layer * out->layer_count;

   /* Skip frame flags (one byte per frame). */
   if (len < pos + out->frames_per_dir)
      return 0;
   pos += out->frames_per_dir;

   /* Priority table. */
   {
      long prio_size = (long) out->direction_count
                     * (long) out->frames_per_dir
                     * (long) out->layer_count;
      if (len < pos + prio_size)
         return 0;

      out->priority = (unsigned char *) malloc((size_t) prio_size);
      if (out->priority == NULL)
         return 0;
      memcpy(out->priority, p + pos, (size_t) prio_size);
      out->priority_len = prio_size;
   }

   return 1;
}

int compose_cof_priority_at(const COMPOSE_COF_S *cof,
                            int direction, int frame, int order_slot)
{
   long idx;

   if (cof == NULL || cof->priority == NULL)
      return -1;
   if (direction  < 0 || direction  >= cof->direction_count)  return -1;
   if (frame      < 0 || frame      >= cof->frames_per_dir)   return -1;
   if (order_slot < 0 || order_slot >= cof->layer_count)      return -1;

   idx = ((long) direction * cof->frames_per_dir + frame)
       * cof->layer_count + order_slot;
   if (idx >= cof->priority_len)
      return -1;
   return cof->priority[idx];
}
