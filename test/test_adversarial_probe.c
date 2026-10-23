/* Adversarial probe for the formats that have no other CLI probe: parse one
 * file with the editor's real code and report accept/reject.
 *
 *   test_adversarial_probe cof      --input PATH   compose_cof_parse + a sweep of
 *                                                  every priority entry
 *   test_adversarial_probe animdata --input PATH   animdata_load + a lookup of every
 *                                                  record name and the frame delay
 *
 * Exit 0 accepted, 1 rejected (declared), 2 bad usage. Anything else is a
 * defect for the campaign (scripts/equivalence/adversarial.py). Nothing is
 * written. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "core/animdata.h"
#include "core/compose_cof.h"
#include "visual_test_support.h"

static int probe_cof(const unsigned char *bytes, size_t len)
{
   COMPOSE_COF_S cof;
   int d, f, s;
   long sum = 0;

   /* compose_cof_parse() returns before touching `cof` for empty input, and
    * compose_cof_free() is only safe on a zeroed struct */
   memset(&cof, 0, sizeof(cof));
   if (!compose_cof_parse(bytes, (long) len, &cof))
   {
      compose_cof_free(&cof);
      fprintf(stderr, "cof rejected\n");
      return 1;
   }
   for (d = 0; d < cof.direction_count; d++)
      for (f = 0; f < cof.frames_per_dir; f++)
         for (s = 0; s < cof.layer_count; s++)
            sum += compose_cof_priority_at(&cof, d, f, s);
   /* out-of-range queries must be answered, not read */
   sum += compose_cof_priority_at(&cof, cof.direction_count, 0, 0);
   sum += compose_cof_priority_at(&cof, 0, cof.frames_per_dir, 0);
   sum += compose_cof_priority_at(&cof, 0, 0, cof.layer_count);
   sum += compose_cof_priority_at(&cof, -1, -1, -1);
   compose_cof_free(&cof);
   printf("cof accepted (%ld)\n", sum & 0xFF);
   return 0;
}

static int probe_animdata(const unsigned char *bytes, size_t len)
{
   int b, i, found = 0;
   long fpd, spd;
   char name[16];

   visual_fetch_reset();
   visual_fetch_add("Data\\Global\\AnimData.d2", bytes, len);
   animdata_load();
   if (glb_ds1edit.anim_data.buffer == NULL)
   {
      fprintf(stderr, "animdata rejected\n");
      return 1;
   }
   for (b = 0; b < 256; b++)
   {
      const ANIM_DATA_RECORD_S *rec = glb_ds1edit.anim_data.block_pointer[b];
      for (i = 0; rec != NULL && i < glb_ds1edit.anim_data.nb_records[b]; i++)
      {
         memset(name, 0, sizeof(name));
         memcpy(name, rec[i].cof_name, 8);
         if (animdata_get_cof_info_quiet(name, &fpd, &spd) == 0)
            found++;
         animdata_get_frame_delay_ms(name, 40);
      }
   }
   animdata_get_cof_info_quiet("ZZZZZZZZ", &fpd, &spd);
   printf("animdata accepted (%d records)\n", found);
   return 0;
}

int main(int argc, char **argv)
{
   unsigned char *bytes;
   size_t len;
   int rc;

   if (argc != 4 || strcmp(argv[2], "--input") != 0
       || (strcmp(argv[1], "cof") != 0 && strcmp(argv[1], "animdata") != 0))
   {
      fprintf(stderr, "usage: test_adversarial_probe cof|animdata --input PATH\n");
      return 2;
   }
   bytes = visual_read_file(argv[3], &len);
   if (bytes == NULL)
   {
      fprintf(stderr, "cannot read %s\n", argv[3]);
      return 2;
   }
   rc = (strcmp(argv[1], "cof") == 0) ? probe_cof(bytes, len)
                                       : probe_animdata(bytes, len);
   free(bytes);
   return rc;
}
