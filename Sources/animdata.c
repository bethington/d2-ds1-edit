#include <string.h>
#include <ctype.h>
#include "structs.h"
#include "misc.h"
#include "animdata.h"
 

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
void animdata_load(void)
{
   char               * name = "Data\\Global\\AnimData.d2", animdata_name[10];
   int                entry, done = FALSE, curr_block = 0;
   long               * lptr, nb_rec;
   ANIM_DATA_RECORD_S * rec_ptr = NULL;
   UBYTE              hash;


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
   lptr = ((long *) glb_ds1edit.anim_data.buffer);
   while ( ! done)
   {
      nb_rec = * lptr;
      lptr++;

      if (nb_rec)
      {
         rec_ptr = (ANIM_DATA_RECORD_S *) lptr;
         memset(animdata_name, 0, sizeof(animdata_name));
         strncpy(animdata_name, rec_ptr->cof_name, 8);
         hash = animdata_hash_value(animdata_name);
      }
      else
         hash = 0;

      // debug infos
      printf("   block %3i, hash = %3i, # records = %li\n", curr_block, hash, nb_rec);
      fflush(stdout);
      curr_block++;

      // update block pointer & records number
      if (nb_rec)
      {
         glb_ds1edit.anim_data.block_pointer[hash] = rec_ptr;
         glb_ds1edit.anim_data.nb_records[hash]    = nb_rec;
      }

      // next block, if any
      lptr += 40 * nb_rec; // skip nb_rec records of 160 bytes each
      if ( (UBYTE *) lptr >=
           ((UBYTE *) glb_ds1edit.anim_data.buffer + glb_ds1edit.anim_data.buffer_size)
         )
      {
         done = TRUE;
      }
   }
   printf("\n");
}


// ==========================================================================
// 'name' is the cof name, WITHOUT the 'data\global\...' in front.
// The ".cof" extension can be in the name, but it won't be used
int animdata_get_cof_info(char * name, long * fpd, long * speed)
{
   ANIM_DATA_RECORD_S * rec_ptr;
   UBYTE              hash;
   int                i;
   char               rec_name[80];


   hash = animdata_hash_value(name);
   printf("%s is hash %i", name, hash);
   if (glb_ds1edit.anim_data.block_pointer[hash])
   {
      rec_ptr = glb_ds1edit.anim_data.block_pointer[hash];
      for (i=0; i < glb_ds1edit.anim_data.nb_records[hash]; i++)
      {
         memset(rec_name, 0, sizeof(rec_name));
         strncpy(rec_name, rec_ptr[i].cof_name, 8);
         if (stricmp(rec_name, name) == 0)
         {
            * fpd   = rec_ptr[i].frames_per_dir;
            * speed = rec_ptr[i].speed;
            printf(", record %i in that block\n", i);
            fflush(stdout);
            return 0;
         }
      }
   }
   * fpd   = 0;
   * speed = 0;
   printf(", but error : not found in there\n");
   fflush(stdout);
   return -1;
}
