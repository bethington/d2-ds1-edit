/* Derived from win_ds1edit by Paul Siramy.
 * See NOTICE at the repository root for attribution and license status. */

#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "structs.h"
#include "misc.h"
#include "core/animdata.h"
 

// ==========================================================================
UBYTE animdata_hash_value(char * name)
{
   unsigned int i, nb = strlen(name);
   UBYTE        hash = 0;

   for (i=0; i < strlen(name); i++)
   {
      if (name[i] == '.')
         nb = i;
   }
   for (i=0; i < nb; i++)
      hash += toupper(name[i]);
   return hash;
}


// ==========================================================================
// AnimData.d2 is exactly 256 buckets laid end to end, each a 32-bit record
// count followed by that many 160-byte records (animdata-d2.md). The game
// keeps one pointer per bucket *position*; a name is looked up in the bucket
// its hash selects. Buckets that run past the end of the file are dropped,
// and anything after the 256th bucket is ignored.
void animdata_load(void)
{
   char               * name = "Data\\Global\\AnimData.d2";
   int                entry, curr_block;
   long               nb_rec, pos, size;
   const UBYTE        * base;


   // free the file if already in mem
   if (glb_ds1edit.anim_data.buffer != NULL)
      free(glb_ds1edit.anim_data.buffer);

   // load it
   memset( & glb_ds1edit.anim_data, 0, sizeof(glb_ds1edit.anim_data));
   entry = misc_load_mpq_file(
      name,
      (char **) & glb_ds1edit.anim_data.buffer,
      & glb_ds1edit.anim_data.buffer_size,
      FALSE
   );
   if ((entry == -1) || (glb_ds1edit.anim_data.buffer_size <= 4))
   {
      printf("error : couldn't load <%s>\n", name);
      return;
   }
   else
      printf("ok, <%s> found\n", name);

   // read all blocks and update pointers
   base = (const UBYTE *) glb_ds1edit.anim_data.buffer;
   size = glb_ds1edit.anim_data.buffer_size;
   pos  = 0;
   for (curr_block = 0; (curr_block < 256) && (pos + 4 <= size); curr_block++)
   {
      int32_t count;

      memcpy(& count, base + pos, sizeof(count));
      pos += 4;
      nb_rec = count;
      if ((nb_rec < 0) || (nb_rec > (size - pos) / (long) sizeof(ANIM_DATA_RECORD_S)))
      {
         printf("   block %3i claims %li records past the end of the file\n",
                curr_block, nb_rec);
         break;
      }
      if (nb_rec)
      {
         glb_ds1edit.anim_data.block_pointer[curr_block] =
            (ANIM_DATA_RECORD_S *) (base + pos);
         glb_ds1edit.anim_data.nb_records[curr_block] = (int) nb_rec;
      }
      pos += nb_rec * (long) sizeof(ANIM_DATA_RECORD_S);
   }
   printf("   %i animdata blocks\n\n", curr_block);
}


// ==========================================================================
// 'name' is the cof name, WITHOUT the 'data\global\...' in front.
// The ".cof" extension can be in the name, but it won't be used
int animdata_get_cof_info(char * name, long * fpd, long * speed)
{
   int rc = animdata_get_cof_info_quiet(name, fpd, speed);
   UBYTE hash = animdata_hash_value(name);
   printf("%s is hash %i", name, hash);
   if (rc == 0)
      printf(", record found\n");
   else
      printf(", but error : not found in there\n");
   fflush(stdout);
   return rc;
}

// Quiet variant: same lookup, no debug output. Used by compose loops.
int animdata_get_cof_info_quiet(const char * name, long * fpd, long * speed)
{
   ANIM_DATA_RECORD_S * rec_ptr;
   UBYTE              hash;
   int                i;
   size_t             key_len, k;
   const char         * dot;
   char               key[8];

   if (name == NULL || fpd == NULL || speed == NULL)
   {
      if (fpd   != NULL) *fpd   = 0;
      if (speed != NULL) *speed = 0;
      return -1;
   }

   // the key is the name up to its extension, as the hash sees it
   dot = strrchr(name, '.');
   key_len = (dot != NULL) ? (size_t) (dot - name) : strlen(name);

   hash = animdata_hash_value((char *) name);
   /* The game (D2Common 1.13c 6fd91dd0) uppercases the query in place, then
    * 6fd91d00 compares the whole 8-byte name field as two dwords: the stored
    * name is matched exactly, not case-folded. Build the same zero-padded
    * uppercase key and compare all 8 bytes. */
   if ((key_len <= 8) && glb_ds1edit.anim_data.block_pointer[hash])
   {
      memset(key, 0, sizeof(key));
      for (k = 0; k < key_len; k++)
         key[k] = (char) toupper((unsigned char) name[k]);
      rec_ptr = glb_ds1edit.anim_data.block_pointer[hash];
      for (i = 0; i < glb_ds1edit.anim_data.nb_records[hash]; i++)
      {
         if (memcmp(rec_ptr[i].cof_name, key, sizeof(key)) == 0)
         {
            *fpd   = rec_ptr[i].frames_per_dir;
            *speed = rec_ptr[i].speed;
            return 0;
         }
      }
   }
   *fpd   = 0;
   *speed = 0;
   return -1;
}

// animdata_speed_to_frame_delay_ms lives in core/animdata_math.c
// so unit tests can link it without dragging in editor globals.

int animdata_get_frame_delay_ms(const char * cof_name, int default_ms)
{
   long fpd, speed;

   if (animdata_get_cof_info_quiet(cof_name, &fpd, &speed) != 0)
      return default_ms;
   return animdata_speed_to_frame_delay_ms(speed, default_ms);
}
