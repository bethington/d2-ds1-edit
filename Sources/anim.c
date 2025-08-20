#include <string.h> 
#include "structs.h"
#include "dccinfo.h"
#include "misc.h"
#include "dc6info.h"
#include "animdata.h"
#include "anim.h"


// ==========================================================================
// palshift must point to an array of 256 bytes
int anim_load_dcc(
   char  * name,
   COF_S * cof,
   int   lay_idx,
   long  user_dir,
   UBYTE * palshift
)
{
   LAY_INF_S  * lay;
   DCC_S      * dcc;
   int        entry, i, size, w, h, x, y;
   char       * buff;
   long       dir = 0, len;


   if ((cof == NULL) || (lay_idx >= COMPOSIT_NB))
      return 1;
   lay = & cof->lay_inf[lay_idx];

   // load dcc file
   entry = misc_load_mpq_file(name, & buff, & len, FALSE);
   if (entry == -1)
      return 1;

   // prepare dcc
   dcc = dcc_mem_load(buff, len);
   free(buff);
   if (dcc == NULL)
      return 1;

   // choose direction
   switch (cof->dir)
   {
      case  1 : dir = glb_ds1edit.new_dir1[user_dir];  break;
      case  4 : dir = glb_ds1edit.new_dir4[user_dir];  break;
      case  8 : dir = glb_ds1edit.new_dir8[user_dir];  break;
      case 16 : dir = glb_ds1edit.new_dir16[user_dir]; break;
      case 32 : dir = glb_ds1edit.new_dir32[user_dir]; break;
   }

   // decode dcc direction
   if (dcc_decode(dcc, 1 << dir))
   {
      dcc_destroy(dcc);
      return 1;
   }

   // allocate the bitmaps
   size = dcc->header.frames_per_dir * sizeof(BITMAP *);
   lay->bmp_num = dcc->header.frames_per_dir;
   lay->bmp = (BITMAP **) malloc(size);
   if (lay->bmp == NULL)
   {
      dcc_destroy(dcc);
      return 1;
   }
   memset(lay->bmp, 0, size);
   
   // copy the bitmaps
   w = dcc->frame[dir][0].bmp->w;
   h = dcc->frame[dir][0].bmp->h;
   lay->off_x = dcc->direction[dir].box.xmin;
   lay->off_y = dcc->direction[dir].box.ymin;
   for (i=0; i < dcc->header.frames_per_dir; i++)
   {
      lay->bmp[i] = create_bitmap(w, h);
      if (lay->bmp[i] == NULL)
      {
         while (i)
         {
            i--;
            destroy_bitmap(lay->bmp[i]);
         }
         dcc_destroy(dcc);
         return 1;
      }
      if (palshift)
      {
         for (y=0; y<h; y++)
            for (x=0; x<w; x++)
               putpixel(lay->bmp[i], x, y, palshift[
                  getpixel(dcc->frame[dir][i].bmp, x, y)]);
      }
      else
         blit(dcc->frame[dir][i].bmp, lay->bmp[i], 0, 0, 0, 0, w, h);
   }

   // end
   dcc_destroy(dcc);
   return 0;
}


// ==========================================================================
COF_S * anim_load_cof(char * base, char * tok, char * mod, char * clas,
                      long user_dir, int obj_line, int progress)
{
   TXT_S * txt  = glb_ds1edit.obj_buff,
         * txt2 = glb_ds1edit.objects_buff;
   COF_S * cof;
   char  * buff, * bptr, * sptr, * pal_name, * pal_buff;
   char  animdata_name[80];
   long  * lptr, pal_idx=0, pal_len, id, line, animdata_fpd, animdata_speed, len;
   int   i, size, entry, idx, done, mode;
   char  comp_str[COMPOSIT_NB][3] = {
            {"HD"}, {"TR"}, {"LG"}, {"RA"}, {"LA"}, {"RH"}, {"LH"}, {"SH"},
            {"S1"}, {"S2"}, {"S3"}, {"S4"}, {"S5"}, {"S6"}, {"S7"}, {"S8"}},
         name[256];


   if (progress)
      printf("\n");
         
   // load palshift
   pal_buff = NULL;
   if (txt->col[misc_get_txt_column_num(RQ_OBJ, "Colormap")].size &&
       txt->col[misc_get_txt_column_num(RQ_OBJ, "Index")].size)
   {
      pal_name = txt->data + (obj_line * txt->line_size) +
                 txt->col[misc_get_txt_column_num(RQ_OBJ, "Colormap")].offset;
      sptr     = txt->data + (obj_line * txt->line_size) +
                 txt->col[misc_get_txt_column_num(RQ_OBJ, "Index")].offset;
      pal_idx = * ((long *) sptr);
      if (pal_name[0] && pal_idx)
      {
         if (progress)
         {
            printf("load colormap <%s>, will use index %li\n",
               pal_name, pal_idx);
            fflush(stdout);
         }
         entry = misc_load_mpq_file(pal_name, & pal_buff, & pal_len, progress);
         if (entry != -1)
         {
            pal_idx = (pal_idx - 1) * 256;
            if ((pal_idx + 255) >= pal_len)
            {
               free(pal_buff);
               pal_buff = NULL;
            }
         }
         else if (progress)
         {
            printf("   (couldn't load the colormap)\n");
            fflush(stdout);
         }
      }
   }

   // load cof
   sprintf(
      name,
      "%s\\%s\\Cof\\%s%s%s.cof", base, tok, tok, mod, clas
   );
   if (progress)
   {
      printf("\nload COF <%s>\n", name);
      fflush(stdout);
   }
   entry = misc_load_mpq_file(name, & buff, & len, progress);
   if (entry == -1)
      return NULL;

   // allocate COF_S structure
   size = sizeof(COF_S);
   cof = (COF_S *) malloc(size);
   if (cof == NULL)
   {
      free(buff);
      return NULL;
   }
   memset(cof, 0, size);

   // fill it

   cof->cur_dir = user_dir;
   
   bptr = buff;

   // layers
   cof->lay = * bptr;
   bptr++;

   // frames per direction
   cof->fpd = * bptr;
   cof->cur_frame = rand() % cof->fpd;
   bptr++;

   // directions
   cof->dir = * bptr;
   bptr++;

   // skip 25 unknown bytes
   bptr += 25;

   // layers infos
   for (i=0; i < cof->lay; i++)
   {
      // composit index
      idx = * bptr;
      bptr++;

      // shadows
      cof->lay_inf[idx].shad_a = * bptr;
      bptr++;
      cof->lay_inf[idx].shad_b = * bptr;
      bptr++;
      
      // transparency
      cof->lay_inf[idx].trans_a = * bptr;
      bptr++;
      cof->lay_inf[idx].trans_b = * bptr;
      bptr++;

      // weapon class (used to know a part of the dcc name)
      memcpy(cof->lay_inf[idx].wclass, bptr, 4);
      bptr += 4;

      // dcc / dc6
      sptr = txt->data + (obj_line * txt->line_size) +
             txt->col[misc_get_txt_column_num(RQ_OBJ, "HD") + idx].offset;
      sprintf(name, "%s\\%s\\%s\\%s%s%s%s%s.dcc",
         base,
         tok,
         comp_str[idx],
         tok,
         comp_str[idx],
         sptr,
         mod,
         cof->lay_inf[idx].wclass
      );

      if (progress)
      {
         printf("\nload DCC <%s>\n", name);
         fflush(stdout);
      }

      if (strlen(name))
      {
         if ((pal_buff != NULL) && (cof->lay_inf[idx].trans_a == 0))
         {
            // a colormap, and not on 1 alpha-blended layer
            // try to load a dcc
            if (anim_load_dcc(name, cof, idx, user_dir, (UBYTE *) (pal_buff + pal_idx)))
            {
               // try with a .dc6 instead of a .dcc
               name[strlen(name) - 1] = '6';
               anim_load_dc6(name, cof, idx, user_dir, (UBYTE *) (pal_buff + pal_idx));
            }
         }
         else // don't use a colormap
         {
            // try to load a dcc
            if (anim_load_dcc(name, cof, idx, user_dir, NULL))
            {
               // try with a .dc6 instead of a .dcc
               name[strlen(name) - 1] = '6';
               anim_load_dc6(name, cof, idx, user_dir, NULL);
            }
         }
      }
   }
   
   // skip flags of each frames
   bptr += cof->fpd;

   // priority layer

   // allocate
   size = cof->dir * cof->fpd * cof->lay;
   cof->priority = (UBYTE *) malloc(size);
   if (cof->priority == NULL)
   {
      free(cof);
      free(buff);
      return NULL;
   }

   // fill
   memcpy(cof->priority, bptr, size);

   // default animation speed
   cof->spd_mul = 1;
   cof->spd_div = 256;

   // default x and y offsets
   cof->xoffset = cof->yoffset = 0;

   // speed info : try in animdata.d2
   sprintf(animdata_name, "%s%s%s", tok, mod, clas);
   if (animdata_get_cof_info(animdata_name, & animdata_fpd, & animdata_speed) == 0)
   {
      // found
//      cof->fpd     = animdata_fpd;
      cof->spd_mul = animdata_speed; // can be override by objects.txt values
      cof->spd_div = 256;
   }
   
   // objects.txt ID of that obj
   sptr = txt->data +
          (obj_line * txt->line_size) +
          txt->col[glb_ds1edit.col_obj_id].offset;
   lptr = (long *) sptr;
   id   = * lptr;
   printf("object %s ID = %li\n", name, id);


   // which mode is this obj ?
   if (stricmp(mod, "NU") == 0)
      mode = 0;
   else if (stricmp(mod, "OP") == 0)
      mode = 1;
   else if (stricmp(mod, "ON") == 0)
      mode = 2;
   else if (stricmp(mod, "S1") == 0)
      mode = 3;
   else if (stricmp(mod, "S2") == 0)
      mode = 4;
   else if (stricmp(mod, "S3") == 0)
      mode = 5;
   else if (stricmp(mod, "S4") == 0)
      mode = 6;
   else if (stricmp(mod, "S5") == 0)
      mode = 7;
   else
   {
      // invalid object's mode, or simply not an object COF (like a monster COF)
      // end
      free(buff);
      if (pal_buff)
         free(pal_buff);
      return cof;
   }
   
   // search line in objects.txt for this ID
   if (id)
   {
      done = FALSE;
      i = 0;
      line = 0;
      glb_ds1edit.obj_desc[obj_line].objects_line = -1;
      while ( ! done)
      {
         sptr = txt2->data +
                (i * txt2->line_size) +
                txt2->col[glb_ds1edit.col_objects_id].offset;
         lptr = (long *) sptr;
         if ( (* lptr) == id)
         {
            done = TRUE;
            line = i;
         }
         else
         {
            i++;
            if (i >= txt2->line_num)
            {
               // end
               free(buff);
               if (pal_buff)
                  free(pal_buff);
               return cof;
            }
         }
      }
      glb_ds1edit.obj_desc[obj_line].objects_line = line;

      // speed multiplicator
      sptr =
         txt2->data +
         (line * txt2->line_size) +
         txt2->col[glb_ds1edit.col_frame_delta[mode]].offset;
      lptr = (long *) sptr;
      cof->spd_mul = (* lptr) == 0 ? 256 : (* lptr);
   
      // speed divisor
      cof->spd_div = 256;

      // xoffset & yoffset
      if (txt2->col[misc_get_txt_column_num(RQ_OBJECTS, "Xoffset")].size)
      {
         sptr = txt2->data + (line * txt2->line_size) +
                txt2->col[misc_get_txt_column_num(RQ_OBJECTS, "Xoffset")].offset;
         lptr = (long *) sptr;
         cof->xoffset = * lptr;
      }
      if (txt2->col[misc_get_txt_column_num(RQ_OBJECTS, "Yoffset")].size)
      {
         sptr = txt2->data + (line * txt2->line_size) +
                txt2->col[misc_get_txt_column_num(RQ_OBJECTS, "Yoffset")].offset;
         lptr = (long *) sptr;
         cof->yoffset = * lptr;
      }

      // orderflag
      if (txt2->col[glb_ds1edit.col_orderflag[mode]].size)
      {
         sptr =
            txt2->data +
            (line * txt2->line_size) +
            txt2->col[glb_ds1edit.col_orderflag[mode]].offset;
         lptr = (long *) sptr;
         cof->orderflag = * lptr;

         // if 0, check NU
         // because Mephisto bridge only have a 1 in the NU mode
         if ( * lptr == 0)
         {
            if (txt2->col[glb_ds1edit.col_orderflag[0]].size)
            {
               sptr =
                  txt2->data +
                  (line * txt2->line_size) +
                  txt2->col[glb_ds1edit.col_orderflag[0]].offset;
               lptr = (long *) sptr;
               cof->orderflag = * lptr;
            }
         }

         printf("object %s orderflag = %li\n", name, cof->orderflag);
      }
   }

   // end
   free(buff);
   if (pal_buff)
      free(pal_buff);
   return cof;
}


// ==========================================================================
COF_S * anim_load_desc_gfx(int i, int progress)
{
   TXT_S * txt = glb_ds1edit.obj_buff;
   COF_S * cof = NULL;
   char  * base, * tok, * mod, * clas, *sptr;
   long  * dirptr, dir;


   if (txt == NULL)
      return NULL;

   base   = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_OBJ, "Base")].offset;
   tok    = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_OBJ, "Token")].offset;
   mod    = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_OBJ, "Mode")].offset;
   clas   = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_OBJ, "Class")].offset;
   sptr   = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_OBJ, "Direction")].offset;
   dirptr = (long *) sptr;

   dir = 0;
   if (txt->col[misc_get_txt_column_num(RQ_OBJ, "Direction")].size)
      dir = * dirptr;

   if (base[0] && tok[0] && mod[0] && clas[0])
      cof = anim_load_cof(base, tok, mod, clas, dir, i, progress);

   return cof;
}


// ==========================================================================
int anim_destroy_cof(COF_S * cof)
{
   BITMAP ** bmp;
   int    b, max_b, c, size = 0;
   
   if (cof == NULL)
      return 0;
      
   // free bitmaps
   for (c=0; c < COMPOSIT_NB; c++)
   {
      bmp = cof->lay_inf[c].bmp;
      if (bmp != NULL)
      {
         max_b = cof->lay_inf[c].bmp_num;
         for (b=0; b < max_b; b++)
         {
            if (bmp[b] != NULL)
            {
               size += sizeof(BITMAP);
               size += bmp[b]->w * bmp[b]->h;
               destroy_bitmap(bmp[b]);
            }
         }
         size += max_b * sizeof (BITMAP *);
         free(bmp);
      }
   }

   // free priority
   if (cof->priority != NULL)
   {
      size += cof->dir * cof->fpd * cof->lay;
      free(cof->priority);
   }

   // free cof
   size += sizeof(COF_S);
   free(cof);

   // end
   return size;
}


// ==========================================================================
void anim_update_gfx(int progress)
{
   TXT_S * txt = glb_ds1edit.obj_buff;
   COF_S * cof;
   int   d, od, o, idx, ds1_anim_used;

   if (txt == NULL)
   {
      return;
   }

   // counters to zero
   for (od=0; od < txt->line_num; od++)
      glb_ds1edit.obj_desc[od].usage_count = 0;

   // update the counters
   for (d=0; d < DS1_MAX; d++)
   {
      if (glb_ds1[d].name[0] == 0)
         continue;
      for (o=0; o < glb_ds1[d].obj_num; o++)
      {
         idx = glb_ds1[d].obj[o].desc_idx;
         if (idx != -1)
            glb_ds1edit.obj_desc[idx].usage_count++;
      }
   }

   // update cof datas
   for (od=0; od < txt->line_num; od++)
   {
      cof = glb_ds1edit.obj_desc[od].cof;
      if (glb_ds1edit.obj_desc[od].usage_count == 0)
      {
         // don't have to be used
         if (cof != NULL)
         {
            // cof exists so delete it
            anim_destroy_cof(cof);
            glb_ds1edit.obj_desc[od].cof = NULL;
         }
      }
      else
      {
         // have to be used
         if (cof == NULL)
         {
            // no cof, so load it
            glb_ds1edit.obj_desc[od].cof = anim_load_desc_gfx(od, progress);
            if (progress)
            {
               fprintf(stderr, ".");
               fflush(stderr);
            }
         }
      }
   }

   // if no animations at all for a ds1, disable the animation layer mask
   for (d=0; d < DS1_MAX; d++)
   {
      if (glb_ds1[d].name[0] == 0)
         continue;
      ds1_anim_used = FALSE;
      for (o=0; o < glb_ds1[d].obj_num; o++)
      {
         idx = glb_ds1[d].obj[o].desc_idx;
         if (idx != -1)
         {
            if (glb_ds1edit.obj_desc[idx].cof != NULL)
            {
               ds1_anim_used = TRUE;
               o = glb_ds1[d].obj_num; // stop the check of all objects
            }
         }
      }
      if (ds1_anim_used == FALSE)
         glb_ds1[d].animations_layer_mask = 0;
   }
}


// ==========================================================================
int anim_exit(void)
{
   TXT_S  * txt = glb_ds1edit.obj_buff;
   COF_S  * cof;
   int    i, size = 0;


   if (txt == NULL)
      return 0;
   if (glb_ds1edit.obj_desc == NULL)
      return 0;
      
   for (i=0; i < txt->line_num; i++)
   {
      cof = glb_ds1edit.obj_desc[i].cof;
      if (cof != NULL)
      {
         size += anim_destroy_cof(cof);
         glb_ds1edit.obj_desc[i].cof = NULL;
      }
   }
   return size;
}
