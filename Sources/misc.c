#include <string.h>
#include "structs.h"
#include "error.h"
#include "ds1misc.h"
#include "txtread.h"
#include "dt1misc.h"
#include "mpq/mpqview.h"
#include "misc.h"


// ==========================================================================
// make a palette from the original one, with the current gamma correction
void misc_pal_d2_2_vga(int pal_idx)
{
   UBYTE r, g, b;
   int   i, ridx;

   if (glb_ds1edit.d2_pal[pal_idx] == NULL)
   {
      for (i=0; i<256; i++)
      {
         glb_ds1edit.vga_pal[pal_idx][i].r = i >> 2;
         glb_ds1edit.vga_pal[pal_idx][i].g = i >> 2;
         glb_ds1edit.vga_pal[pal_idx][i].b = i >> 2;
      }
      return;
   }
      
   for (i=0; i<256; i++)
   {
      ridx = 4 * i;
      r = glb_ds1edit.d2_pal[pal_idx][ridx];
      g = glb_ds1edit.d2_pal[pal_idx][ridx + 1];
      b = glb_ds1edit.d2_pal[pal_idx][ridx + 2];
      r = glb_ds1edit.gamma_table[glb_ds1edit.cur_gamma][r];
      g = glb_ds1edit.gamma_table[glb_ds1edit.cur_gamma][g];
      b = glb_ds1edit.gamma_table[glb_ds1edit.cur_gamma][b];
      glb_ds1edit.vga_pal[pal_idx][i].r = r >> 2;
      glb_ds1edit.vga_pal[pal_idx][i].g = g >> 2;
      glb_ds1edit.vga_pal[pal_idx][i].b = b >> 2;
   }   
}

      
// ==========================================================================
// helper of the qsort function, same order as in the game
int misc_qsort_helper_block_table_1(const void * e1, const void * e2)
{
   BLOCK_TABLE_S * bt1, * bt2;
   long          n1=0, n2=0;

   bt1 = (BLOCK_TABLE_S *) e1;
   bt2 = (BLOCK_TABLE_S *) e2;
   if (bt1->orientation != bt2->orientation)
   {
      n1 = bt1->orientation;
      n2 = bt2->orientation;
   }
   else if (bt1->main_index != bt2->main_index)
   {
      n1 = bt1->main_index;
      n2 = bt2->main_index;
   }
   else if (bt1->sub_index != bt2->sub_index)
   {
      n1 = bt1->sub_index;
      n2 = bt2->sub_index;
   }
   else if (bt1->dt1_idx != bt2->dt1_idx)
   {
      n1 = bt1->dt1_idx_for_ds1;
      n2 = bt2->dt1_idx_for_ds1;

      if (n1 == 0) n1 = DT1_MAX;
      if (n2 == 0) n2 = DT1_MAX;
   }
   else if (bt1->rarity != bt2->rarity)
   {
      n1 = bt1->rarity;
      n2 = bt2->rarity;
   }
   else if (bt1->block_idx != bt2->block_idx)
   {
      n1 = bt1->block_idx;
      n2 = bt2->block_idx;
   }
   return n1 - n2;
}


// ==========================================================================
// helper of the qsort function, order for the end-user
int misc_qsort_helper_block_table_2(const void * e1, const void * e2)
{
   BLOCK_TABLE_S * bt1, * bt2;
   long          n1=0, n2=0;

   bt1 = (BLOCK_TABLE_S *) e1;
   bt2 = (BLOCK_TABLE_S *) e2;

/*
   if (bt1->dt1_idx != bt2->dt1_idx)
   {
      n1 = bt1->dt1_idx;
      n2 = bt2->dt1_idx;
   }
   else
*/
   if (bt1->main_index != bt2->main_index)
   {
      n1 = bt1->main_index;
      n2 = bt2->main_index;
   }
   else if (bt1->orientation != bt2->orientation)
   {
      n1 = bt1->orientation;
      n2 = bt2->orientation;
   }
   else if (bt1->sub_index != bt2->sub_index)
   {
      n1 = bt1->sub_index;
      n2 = bt2->sub_index;
   }

   // ----------------
   else if (bt1->dt1_idx != bt2->dt1_idx)
   {
      n1 = bt1->dt1_idx;
      n2 = bt2->dt1_idx;
   }
   // ----------------
   
   else if (bt1->rarity != bt2->rarity)
   {
      n1 = bt1->rarity;
      n2 = bt2->rarity;
   }
   else if (bt1->block_idx != bt2->block_idx)
   {
      n1 = bt1->block_idx;
      n2 = bt2->block_idx;
   }
   return n1 - n2;
}


// ==========================================================================
// after the 1st qsort, find the tiles the game will use
// it handles conflicts, and rarity of the tiles
void misc_check_tiles_conflicts(int ds1_idx)
{
   BLOCK_TABLE_S * bt = glb_ds1[ds1_idx].block_table;
   long          old_o, old_m, old_s, o, m, s;
   int           old_d, d, done = FALSE, done2, start_i = 0, i, end_i, r;
   int           last_block, first_block;
   int           bt_max = glb_ds1[ds1_idx].bt_num, b;
   long          sum_rarity, max_rarity;
   

   while ( ! done)
   {
      old_o = bt[start_i].orientation;
      old_m = bt[start_i].main_index;
      old_s = bt[start_i].sub_index;
      old_d = bt[start_i].dt1_idx;

      bt[start_i].used_by_game = bt[start_i].used_by_editor = FALSE;

      // how many tiles have the same Orientation / Main index / Sub index ?
      done2       = FALSE;
      sum_rarity  = bt[start_i].rarity;
      last_block  = start_i;
      i           = start_i + 1;
      max_rarity  = -1;
      if (bt[start_i].rarity)
         first_block = start_i;
      else
         first_block = -1;
      while ( ! done2)
      {
         o = bt[i].orientation;
         m = bt[i].main_index;
         s = bt[i].sub_index;
         d = bt[i].dt1_idx;
         r = bt[i].rarity;
         if ((old_o != o) || (old_m != m) || (old_s != s) || (i >= bt_max))
            done2 = TRUE;
         else
         {
            bt[i].used_by_game = bt[i].used_by_editor = FALSE;
            if (d == old_d)
               last_block = i; // last block of the first dt1
            if (r)
            {
               if (first_block == -1)
                  first_block = i; // first block having a rarity
                                   //    (whichever the dt1 is is)
               if (r > max_rarity)
               {
                  first_block = i;
                  max_rarity = r;
               }
            }
            sum_rarity += r;
            i++;
         }
      }
      end_i = i - 1;

      // which one of these tiles will use the game & the editor ?
      if (sum_rarity == 0)
      {
         // only last block of the first dt1
         bt[last_block].used_by_game = bt[last_block].used_by_editor = TRUE;
      }
      else
      {
         // editor : only first block having the highest rarity
         bt[first_block].used_by_editor = TRUE;
         
         // game   : same, but also others which have a non-zero rarity
         for (b = start_i; b <= end_i; b++)
         {
            if (bt[b].rarity)
               bt[b].used_by_game = TRUE;
         }
      }

      // next
      if (i >= bt_max)
         done = TRUE;
      else
         start_i = i;
   }
}


// ==========================================================================
// make the block table of 1 ds1
void misc_make_block_table(int ds1_idx)
{
   BLOCK_TABLE_S * bt_ptr;
   BLOCK_S       * b_ptr;
   char          tmp_str[80];
   int           i, d, b, size, n = 0;

   for (i=0; i<DT1_IN_DS1_MAX; i++)
   {
      d = glb_ds1[ds1_idx].dt1_idx[i];
      if (d != -1)
         n += glb_dt1[d].block_num;
   }
   n++;
   size = sizeof(BLOCK_TABLE_S) * n;
   glb_ds1[ds1_idx].block_table = (BLOCK_TABLE_S *) malloc(size);
   if (glb_ds1[ds1_idx].block_table == NULL)
   {
      sprintf(tmp_str, "make_block_table(%i), not enough mem for %i bytes\n",
         ds1_idx, size);
      ds1edit_error(tmp_str);
   }
   memset(glb_ds1[ds1_idx].block_table, 0, size);
   glb_ds1[ds1_idx].bt_num = n;

   // fill it
   bt_ptr = glb_ds1[ds1_idx].block_table;

   bt_ptr->orientation     = -1;
   bt_ptr->main_index      = -1;
   bt_ptr->sub_index       = -1;
   bt_ptr->rarity          = -1;
   bt_ptr->dt1_idx_for_ds1 = -1;
   bt_ptr->dt1_idx         = -1;
   bt_ptr->block_idx       = -1;
   bt_ptr->roof_y          = -1;
   bt_ptr->type            = -1;
   bt_ptr->zero_line       = -1;
   bt_ptr++;

   for (i=0; i<DT1_IN_DS1_MAX; i++)
   {
      d = glb_ds1[ds1_idx].dt1_idx[i];
      if (d != -1)
      {
         b_ptr = glb_dt1[d].bh_buffer;
         for (b=0; b<glb_dt1[d].block_num; b++)
         {
            // sort-key
            bt_ptr->orientation = b_ptr->orientation;
            bt_ptr->main_index  = b_ptr->main_index;
            bt_ptr->sub_index   = b_ptr->sub_index;
            bt_ptr->rarity      = b_ptr->rarity;

            // datas
            bt_ptr->dt1_idx_for_ds1 = i;
            bt_ptr->dt1_idx         = d;
            bt_ptr->block_idx       = b;
            bt_ptr->roof_y          = 0;
            bt_ptr->zero_line       = 0;
            
            // type
            if (bt_ptr->orientation == 0)
            {
               // floor
               bt_ptr->zero_line = 0;
               if (b_ptr->animated == 0x01)
               {
                  // animated
                  bt_ptr->type = BT_ANIMATED;
               }
               else
               {
                  // static
                  bt_ptr->type = BT_STATIC;
               }
            }
            else if (bt_ptr->orientation == 13)
            {
               // shadow
               bt_ptr->type      = BT_SHADOW;
               bt_ptr->zero_line = - b_ptr->size_y;
            }
            else
            {
               // walls
               if ((bt_ptr->orientation == 10) || (bt_ptr->orientation == 11))
               {
                  // special
                  bt_ptr->type      = BT_SPECIAL;
                  bt_ptr->zero_line = - b_ptr->size_y;
               }
               else if (bt_ptr->orientation == 15)
               {
                  // roof
                  bt_ptr->type      = BT_ROOF;
                  bt_ptr->roof_y    = b_ptr->roof_y;
                  bt_ptr->zero_line = 0;
               }
               else if (bt_ptr->orientation < 15)
               {
                  // wall up
                  bt_ptr->type      = BT_WALL_UP;
                  bt_ptr->zero_line = - b_ptr->size_y;
               }
               else if (bt_ptr->orientation > 15)
               {
                  // wall down
                  bt_ptr->type      = BT_WALL_DOWN;
                  bt_ptr->zero_line = 96;
               }
            }

            // next elements
            bt_ptr++;
            b_ptr++;
         }
      }
   }
   
   // display dt1 idx & file, to help user
   printf("\ndt1_idx file\n"
            "------- ---------------------------------------------------------------------\n");
   for (d=0; d<DT1_IN_DS1_MAX; d++)
   {
      if (glb_ds1[ds1_idx].dt1_idx[d] != -1)
      {
         for (i=0; i<DT1_MAX; i++)
         {
            if (i == glb_ds1[ds1_idx].dt1_idx[d])
            {
               if (glb_dt1[i].ds1_usage > 0)
                  printf("%7i %s\n", i, glb_dt1[i].name);
            }
         }
      }
   }
   
   // sort it for conflict
   qsort(glb_ds1[ds1_idx].block_table, n, sizeof(BLOCK_TABLE_S),
      misc_qsort_helper_block_table_1);

   // conflicts managment
   misc_check_tiles_conflicts(ds1_idx);
   
   // sort it for useability
   qsort(glb_ds1[ds1_idx].block_table, n, sizeof(BLOCK_TABLE_S),
      misc_qsort_helper_block_table_2);

   // display it
   printf("\nsorted block_table of glb_ds1[%i] (%i blocks) :\n", ds1_idx, n);
   printf("block orientation main_idx sub_idx frame dt1_idx blk_idx roof_y type line0\n");
   printf("----- ----------- -------- ------- ----- ------- ------- ------ ---- -----\n");
   for (b=0; b<glb_ds1[ds1_idx].bt_num; b++)
   {
      printf("%5i %11li %8li %7li %5li %7li %7li %6i %4i %5i",
         b,
         glb_ds1[ds1_idx].block_table[b].orientation,
         glb_ds1[ds1_idx].block_table[b].main_index,
         glb_ds1[ds1_idx].block_table[b].sub_index,
         glb_ds1[ds1_idx].block_table[b].rarity,
         glb_ds1[ds1_idx].block_table[b].dt1_idx,
         glb_ds1[ds1_idx].block_table[b].block_idx,
         glb_ds1[ds1_idx].block_table[b].roof_y,
         glb_ds1[ds1_idx].block_table[b].type,
         glb_ds1[ds1_idx].block_table[b].zero_line
      );
      glb_ds1[ds1_idx].block_table[b].conflict = FALSE;
      if (glb_ds1[ds1_idx].block_table[b].rarity == 0)
      {
         if (b>1)
         {
            if ( (glb_ds1[ds1_idx].block_table[b].orientation == glb_ds1[ds1_idx].block_table[b-1].orientation) &&
                 (glb_ds1[ds1_idx].block_table[b].main_index  == glb_ds1[ds1_idx].block_table[b-1].main_index)  &&
                 (glb_ds1[ds1_idx].block_table[b].sub_index   == glb_ds1[ds1_idx].block_table[b-1].sub_index)
            )
               glb_ds1[ds1_idx].block_table[b].conflict = TRUE;
         }
         if (b < (glb_ds1[ds1_idx].bt_num-1))
         {
            if ( (glb_ds1[ds1_idx].block_table[b].orientation == glb_ds1[ds1_idx].block_table[b+1].orientation) &&
                 (glb_ds1[ds1_idx].block_table[b].main_index  == glb_ds1[ds1_idx].block_table[b+1].main_index)  &&
                 (glb_ds1[ds1_idx].block_table[b].sub_index   == glb_ds1[ds1_idx].block_table[b+1].sub_index)
            )
               glb_ds1[ds1_idx].block_table[b].conflict = TRUE;
         }
      }  if (glb_ds1[ds1_idx].block_table[b].conflict)
         printf(" *");
      printf("\n");
   }
}


// ==========================================================================
// read the gamma correction file
void misc_read_gamma(void)
{
   FILE * in;
   int  gt, i, v;
   char tmp[150], gamma_path[100];

   sprintf(gamma_path, "%s%s", glb_ds1edit_data_dir, "gamma.dat");
   fprintf(stderr, "read gamma correction table (%s)\n", gamma_path);
   in = fopen(gamma_path, "rb");
   if (in == NULL)
   {
      sprintf(tmp, "misc_read_gamma() : can't open %s", gamma_path);
      ds1edit_error(tmp);
   }
   for (gt=GC_060; gt<GC_MAX; gt++)
   {
      for (i=0; i<256; i++)
      {
         v = fgetc(in);
         glb_ds1edit.gamma_table[gt][i] = v;
      }
   }
   fclose(in);
}


// ==========================================================================
// update all the palettes with the current gamma correction
void misc_update_pal_with_gamma(void)
{
   int i;

   for (i=0; i<ACT_MAX; i++)
      misc_pal_d2_2_vga(i);
}


// ==========================================================================
// hexedit the screenshot to have the *exact* original palette
//    (allegro lost the 2 lowest bits)
// use the current gamma correction
void misc_pcx_put_d2_palette(char * name, int pal_idx)
{
   FILE * in;
   int  i, r, g, b, ridx;

   in = fopen(name, "rb+");
   if (in == NULL)
   {
      printf("warning : can't modify palette of file %s\n", name);
      return;
   }

   fseek(in, -768, SEEK_END);
   for (i=0; i<256; i++)
   {
      ridx = 4 * i;
      r = glb_ds1edit.d2_pal[pal_idx][ridx];
      g = glb_ds1edit.d2_pal[pal_idx][ridx + 1];
      b = glb_ds1edit.d2_pal[pal_idx][ridx + 2];
      fputc(glb_ds1edit.gamma_table[glb_ds1edit.cur_gamma][r], in);
      fputc(glb_ds1edit.gamma_table[glb_ds1edit.cur_gamma][g], in);
      fputc(glb_ds1edit.gamma_table[glb_ds1edit.cur_gamma][b], in);
   }
   fclose(in);
}


// ==========================================================================
// color map helper
void misc_make_cmaps_helper(const PALETTE pal, int x, int y, RGB * rgb)
{
   if (x == COL_SHADOW)
   {
      // full white
      rgb->r = 63;
      rgb->g = 63;
      rgb->b = 63;
   }
   else if (x == COL_MOUSE)
   {
      // half way between 2/3 of (2 * src) & 1/3 dst
      // result is brighter & have more white
      rgb->r = (pal[x].r + pal[y].r * 4) / 3;
      rgb->g = (pal[x].g + pal[y].g * 4) / 3;
      rgb->b = (pal[x].b + pal[y].b * 4) / 3;
      if (rgb->r > 63) rgb->r = 63;
      if (rgb->g > 63) rgb->g = 63;
      if (rgb->b > 63) rgb->b = 63;
   }
   else
   {
      // common transparency, half way between src & dst
      rgb->r = (pal[x].r + pal[y].r) >> 1;
      rgb->g = (pal[x].g + pal[y].g) >> 1;
      rgb->b = (pal[x].b + pal[y].b) >> 1;
   }
}


// ==========================================================================
// make (or read) all colormaps for all palettes
void misc_make_cmaps(void)
{
   CMAP_E cm;
   int    a, cmap_ok, i, c, start;
   char   tmp[100];
   FILE   * out, * in;

   printf("\n");
   for (cm=0; cm < CM_MAX; cm++)
   {
      for (a=0; a < ACT_MAX; a++)
      {
         fprintf(stderr, ".");
         sprintf(tmp, "%scmap%i_%i.bin", glb_ds1edit_data_dir, a, cm);
         cmap_ok = FALSE;
         if (file_exists(tmp, -1, NULL) && (glb_ds1edit.pal_loaded[a] == TRUE))
         {
            // load the colormap from disk, instead of making it
            in = fopen(tmp, "rb");
            if (in == NULL)
               printf("can't read %s\n", tmp);
            else
            {
               printf("loading %s\n", tmp);
               fread(& glb_ds1edit.cmap[cm][a], sizeof(COLOR_MAP), 1, in);
               fclose(in);
               cmap_ok = TRUE;
            }
         }
         
         if (cmap_ok == FALSE)
         {
            // not found or can't be open, so create it

            if (cm == CM_SELECT)
            {
               // color table
               create_color_table(& glb_ds1edit.cmap[cm][a],
                               glb_ds1edit.vga_pal[a],
                               misc_make_cmaps_helper,
                               NULL);

            }
            else if (cm == CM_TRANS)
            {
               create_trans_table(& glb_ds1edit.cmap[cm][a],
                               glb_ds1edit.vga_pal[a],
                               128, 128, 128,
                               NULL);
            }
            else if (cm == CM_SHADOW)
            {
               for (c=0; c < 256; c++)
               {
                  start = 1024 + (256 * (c/8));
                  for (i=0; i<256; i++)
                  {
                     glb_ds1edit.cmap[cm][a].data[c][i] =
                        glb_ds1edit.d2_pal[a][start + i];
                  }
               }
            }
            
            out = fopen(tmp, "wb");
            if (out == NULL)
               printf("can't write %s\n", tmp);
            else
            {
               printf("saving %s\n", tmp);
               fwrite(& glb_ds1edit.cmap[cm][a], sizeof(COLOR_MAP), 1, out);
               fclose(out);
            }
         }
      }
   }
}


// ==========================================================================
// try to load 1 palette from the data\ directory
int misc_load_pal_from_disk(int pal_idx)
{
   char tmp[100];
   FILE * in;
   long size;
   
   sprintf(tmp, "%spal%i.bin", glb_ds1edit_data_dir, pal_idx);
   if (file_exists(tmp, -1, NULL))
   {
      // load the palette from disk, instead of mpq
      in = fopen(tmp, "rb");
      if (in == NULL)
         printf("can't read %s\n", tmp);
      else
      {
         // size
         fseek(in, 0, SEEK_END);
         size = ftell(in);
         fseek(in, 0, SEEK_SET);
         
         // malloc
         glb_ds1edit.d2_pal[pal_idx] = (UBYTE *) malloc(size);
         if (glb_ds1edit.d2_pal[pal_idx] == NULL)
         {
            sprintf(tmp, "misc_load_pal_from_disk() : "
               "not enough mem (%li bytes) for palette %i", size, pal_idx);
            ds1edit_error(tmp);
         }
         
         // filling it
         printf("loading %s\n", tmp);
         fread(glb_ds1edit.d2_pal[pal_idx], size, 1, in);
         fclose(in);
         glb_ds1edit.pal_size[pal_idx] = size;
         return TRUE;
      }
   }
   return FALSE;
}


// ==========================================================================
// save the palette in the data\ directory (for quicker access next time)
void misc_save_pal_on_disk(int pal_idx, UBYTE * d2_pal_ptr)
{
   char tmp[100];
   FILE * out;
   
   sprintf(tmp, "%spal%i.bin", glb_ds1edit_data_dir, pal_idx);
   out = fopen(tmp, "wb");
   if (out == NULL)
      printf("can't write %s\n", tmp);
   else
   {
      printf("saving %s\n", tmp);
      fwrite(d2_pal_ptr, glb_ds1edit.pal_size[pal_idx], 1, out);
      fclose(out);
   }
}


// ==========================================================================
// own version of fgets()
// any special char means End Of String now, but NOT spaces !
int misc_my_fgets(char * dst, int max, FILE * in)
{
   int i=0, c;

   if (max <= 0)
      return 0;
      
   for(;;)
   {
      c = fgetc(in);
      if (c == EOF)
      {
         dst[i] = 0;
         if (i)
            return 1;
         else
            return 0;
      }
      if ((i >= (max - 1)) || (c < 32))
      {
         dst[i] = 0;
         while ((c < 32) && (c != EOF))
            c  = fgetc(in);
         if (c != EOF)
            ungetc(c, in);
         return 1;
      }
      dst[i] = c;
      i++;
   }
}


// ==========================================================================
// return start of ds1 file to open (multiple ds1 to open mode)
char * misc_search_name(char * tmp)
{
   int max, i=0;

   max = strlen(tmp);
   if (max < 1)
      return NULL;

   // skip spaces
   while ((i < max) && (tmp[i] <= 32))
      i++;
   if (i >= max)
      return NULL;

   // skip 1st number
   while ((i < max) && (tmp[i] > 32))
      i++;
   if (i >= max)
      return NULL;

   // skip spaces
   while ((i < max) && (tmp[i] <= 32))
      i++;
   if (i >= max)
      return NULL;

   // skip 2nd number
   while ((i < max) && (tmp[i] > 32))
      i++;
   if (i >= max)
      return NULL;

   // skip spaces
   while ((i < max) && (tmp[i] <= 32))
      i++;
   if (i >= max)
      return NULL;

   // found name
   return & tmp[i];
}


// ==========================================================================
// open 1 ds1, & all the dt1 it needs
void misc_open_1_ds1(int ds1_idx, char * name, int type, int def,
                     int new_width, int new_height)
{
   // ds1
   printf("\nreading ds1 : %s\n", name);
   fflush(stdout);
   fprintf(stderr, "reading %s...", name);
   ds1_read(name, ds1_idx, new_width, new_height);
   fprintf(stderr, "done\n");
      
   // lvl*.txt (and loading dt1 from mpq)
   fprintf(stderr, "searching Dt1Mask...");
   read_lvlprest_txt(ds1_idx, def);
   fprintf(stderr, "done\n");

   printf("\nloading dt1 from mpq...\n");
   glb_ds1[ds1_idx].txt_act = read_lvltypes_txt(ds1_idx, type);
   printf("\ndone\n");
   printf("txt_act = %i\n", glb_ds1[ds1_idx].txt_act);

   // make the block_table of this ds1
   misc_make_block_table(ds1_idx);

   // prop to index in block table
   ds1_make_prop_2_block(ds1_idx);
}


// ==========================================================================
// open 1 ds1, and all the the dt1 it'll use
void misc_open_1_ds1_force_dt1(int ds1_idx)
{
   int  d;
   char * ds1_name = glb_ds1edit.cmd_line.ds1_filename;
   int  new_width  = glb_ds1edit.cmd_line.resize_width;
   int  new_height = glb_ds1edit.cmd_line.resize_height;
   char ds1edt_file[] = "ds1edit.dt1";
   char tmp[150];


   // ds1
   printf("\nreading ds1 : %s\n", ds1_name);
   fflush(stdout);
   fprintf(stderr, "reading %s...", ds1_name);
   ds1_read(ds1_name, ds1_idx, new_width, new_height);
   fprintf(stderr, "done\n");

   // always open ds1edit.dt1
   sprintf(tmp, "%s%s", glb_ds1edit_data_dir, ds1edt_file);
   printf("\nwant to read a dt1 : %s\n", tmp);
   glb_ds1[ds1_idx].dt1_idx[0] = dt1_add_special(tmp);

   // opening other dt1
   for (d=0; d < DT1_IN_DS1_MAX; d++)
      glb_ds1[ds1_idx].dt1_idx[d + 1] = -1;

   printf("\nloading dt1 from disk...\n");

   for (d=0; d < glb_ds1edit.cmd_line.dt1_list_num; d++)
   {
      printf("\nwant to read a dt1 : %s\n", glb_ds1edit.cmd_line.dt1_list_filename[d]);
      glb_ds1[ds1_idx].dt1_idx[d + 1] = dt1_add_special(glb_ds1edit.cmd_line.dt1_list_filename[d]);
   }

   glb_ds1[ds1_idx].txt_act = glb_ds1[ds1_idx].act;
   printf("\ndone\n");

   // make the block_table of this ds1
   misc_make_block_table(ds1_idx);

   // prop to index in block table
   ds1_make_prop_2_block(ds1_idx);
}


// ==========================================================================
// loop for opening several ds1
void misc_open_several_ds1(char * filename)
{
   FILE * in;
   int  type, def, ret, ds1_idx=0;
   char tmp[1000], * name;


   in = fopen(filename, "rt");
   if (in == NULL)
   {
      sprintf(tmp, "misc_open_several_ds1(), can't open %s", filename);
      ds1edit_error(tmp);
   }

   ret = misc_my_fgets(tmp, sizeof(tmp), in);
   while (ret != 0)
   {
      sscanf(tmp, "%i %i", &type, &def);
      name = misc_search_name(tmp);

      // read the ds1
      if (glb_ds1edit.cmd_line.dt1_list_num != -1)
      {
         // force dt1
         misc_open_1_ds1_force_dt1(ds1_idx);
      }
      else
      {
         // find dt1 list from .txt
         misc_open_1_ds1(
            ds1_idx,
            name,
            type,
            def,
            glb_ds1edit.cmd_line.resize_width,
            glb_ds1edit.cmd_line.resize_height
         );
      }

      ds1_idx++;
      ret = misc_my_fgets(tmp, sizeof(tmp), in);
   }

   fclose(in);
}


// ==========================================================================
// load several pcx and make all the various tiles pcx for walkable info
void misc_walkable_tile_info_pcx(void)
{
   static char pcxname[11][30] = {
             {"pcx\\bit0.pcx"},
             {"pcx\\bit1.pcx"},
             {"pcx\\bit2.pcx"},
             {"pcx\\bit3.pcx"},
             {"pcx\\bit4.pcx"},
             {"pcx\\bit5.pcx"},
             {"pcx\\bit6.pcx"},
             {"pcx\\bit7.pcx"},
             {"pcx\\bit8.pcx"},
             {"pcx\\st_nowalk.pcx"},
             {"pcx\\st_nojump.pcx"}
          };
   int    loop, i, x0, y0, z, w=0, h=0;
   BITMAP * tmpbmp, * subtile, * subtile2;
   char   tmp[150];
   int    b=0;


   fprintf(stderr, "walkable tile infos");
   fflush(stderr);
   glb_ds1edit.subtile_help = load_pcx("pcx/st_help.pcx", glb_ds1edit.dummy_pal);
   if (glb_ds1edit.subtile_help == NULL)
   {
      sprintf(tmp, "misc_walkable_tile_info_pcx(), can't open pcx/st_help.pcx");
      ds1edit_error(tmp);
   }

   for (loop=0; loop<11; loop++)
   {
      fprintf(stderr, ".");
      fflush(stderr);
      tmpbmp = load_pcx(pcxname[loop], glb_ds1edit.dummy_pal);
      if (tmpbmp == NULL)
      {
         sprintf(tmp, "misc_walkable_tile_info_pcx(), can't open %s",
            pcxname[loop]);
         ds1edit_error(tmp);
      }
      for (i=0; i<25; i++)
      {
         subtile = create_bitmap(160, 80);
         if (subtile == NULL)
         {
            sprintf(tmp, "misc_walkable_tile_info_pcx(), can't create "
               "the (%i - %i) bitmap ", loop, i);
            ds1edit_error(tmp);
         }
         clear(subtile);
         x0 = 64 - ((i/5) * 16) + ((i%5) * 16);
         y0 = ((i/5) * 8) + ((i%5) * 8);
         draw_sprite(subtile, tmpbmp, x0, y0);

         if (loop < 9)
            glb_ds1edit.subtile_flag[loop][ZM_11][i] = get_rle_sprite(subtile);
         else if (loop == 9)
            glb_ds1edit.subtile_nowalk[ZM_11][i] = get_rle_sprite(subtile);
         else
            glb_ds1edit.subtile_nojump[ZM_11][i] = get_rle_sprite(subtile);
            
         for (z=0; z<ZM_MAX; z++)
         {
            if (z == ZM_11)
               continue;
            switch(z)
            {
               case ZM_12  : w = 160 /  2; h = 80 /  2; break;
               case ZM_14  : w = 160 /  4; h = 80 /  4; break;
               case ZM_18  : w = 160 /  8; h = 80 /  8; break;
               case ZM_116 : w = 160 / 16; h = 80 / 16; break;
            }
            subtile2 = create_bitmap(w, h);
            clear(subtile2);
            if (subtile2 == NULL)
            {
               sprintf(tmp, "misc_walkable_tile_info_pcx(), can't create "
                  "the (%i - %i) bitmap at zoom %i", loop, i, z);
               ds1edit_error(tmp);
            }
            stretch_blit(subtile, subtile2, 0, 0, 160, 80, 0, 0, w, h);

            if (loop < 9)
               glb_ds1edit.subtile_flag[loop][z][i] = get_rle_sprite(subtile2);
            else if (loop == 9)
               glb_ds1edit.subtile_nowalk[z][i] = get_rle_sprite(subtile2);
            else
               glb_ds1edit.subtile_nojump[z][i] = get_rle_sprite(subtile2);
               
            destroy_bitmap(subtile2);
         }
         destroy_bitmap(subtile);
      }
      destroy_bitmap(tmpbmp);
   }

   // walkable tile infos combinations
   fprintf(stderr, "\nwalkable tile infos combinations");
   fflush(stderr);
   for (i=0; i<25; i++)
   {
      fprintf(stderr, ".");
      fflush(stderr);
      for (z=0; z<ZM_MAX; z++)
      {
         w = glb_ds1edit.subtile_flag[0][z][i]->w;
         h = glb_ds1edit.subtile_flag[0][z][i]->h;

         for (loop=0; loop<256; loop++)
         {
            subtile2 = create_bitmap(w, h);
            clear(subtile2);
            if (subtile2 == NULL)
            {
               sprintf(tmp, "misc_walkable_tile_info_pcx(), can't create "
                  "the (%i - %i) bitmap at zoom %i", loop, i, z);
               ds1edit_error(tmp);
            }

            draw_rle_sprite(subtile2, glb_ds1edit.subtile_flag[0][z][i], 0, 0);
            for (b=0; b < 8; b++)
            {
               if (loop & (1 << b))
               {
                  draw_rle_sprite(
                     subtile2,
                     glb_ds1edit.subtile_flag[b+1][z][i],
                     0, 0
                  );
               }
            }

            glb_ds1edit.subtile_flag_combination[loop][z][i] =
               get_rle_sprite(subtile2);

            destroy_bitmap(subtile2);
         }
      }
   }

   // we don't need the non-combination RLE anymore
   for (b=0; b<9; b++)
   {
      for (z=0; z<ZM_MAX; z++)
      {
         for (i=0; i<25; i++)
         {
            if (glb_ds1edit.subtile_flag[b][z][i] != NULL)
            {
               destroy_rle_sprite(glb_ds1edit.subtile_flag[b][z][i]);
               glb_ds1edit.subtile_flag[b][z][i] = NULL;
            }
         }
      }
   }

   fprintf(stderr, "\n");
   fflush(stderr);
}


// ==========================================================================
// search the 2nd block table index of a upper / left corner tile
// (2nd corner tile = orientation 4)
int misc_seach_block_or4(int ds1_idx, BLOCK_TABLE_S * bt_ptr, int b,
                         int m, int s)
{
   for(;;)
   {
      if (b >= glb_ds1[ds1_idx].bt_num)
         return -1;
      else
      {
         if ( (bt_ptr[b].orientation == 4) &&
              (bt_ptr[b].main_index  == m) &&
              (bt_ptr[b].sub_index   == s)
            )
         {
            return b;
         }
      }
      b++;
   }
}


// ==========================================================================
// fill the table with the walkable infos of all layers for 1 cell
void misc_search_walk_infos(int ds1_idx, int x, int y, UBYTE * dsttable)
{
   BLOCK_TABLE_S * bt_ptr;
   CELL_F_S      * f_ptr;
   CELL_W_S      * w_ptr;
   int           tf, tw, b, f, w, di, bi, i;
   BLOCK_S       * bh_ptr;
   UBYTE         * u_ptr, all_floor_props = 0;
   
   
   tf    = (y * glb_ds1[ds1_idx].floor_line) + (x * glb_ds1[ds1_idx].floor_num);
   tw    = (y * glb_ds1[ds1_idx].wall_line)  + (x * glb_ds1[ds1_idx].wall_num);
   f_ptr = glb_ds1[ds1_idx].floor_buff + tf;
   w_ptr = glb_ds1[ds1_idx].wall_buff  + tw;

   // init
   for (i=0; i<25; i++)
      dsttable[i] = 0; // no flags by default
   bt_ptr = glb_ds1[ds1_idx].block_table;
   
   // floors
   for (f=0; f < glb_ds1[ds1_idx].floor_num; f++)
   {
      all_floor_props |= f_ptr[f].prop1 | f_ptr[f].prop2 |
                         f_ptr[f].prop3 | f_ptr[f].prop4;
      if (f_ptr[f].prop3 & 0x02)
      {
         // this is a global unwalkable info
         for (i=0; i<25; i++)
            dsttable[i] |= 1;
      }
      b = f_ptr[f].bt_idx;
      if (b > 0) // not -1 and not 0
      {
         di     = bt_ptr[b].dt1_idx;
         bi     = bt_ptr[b].block_idx;
         bh_ptr = glb_dt1[di].bh_buffer;
         u_ptr  = bh_ptr[bi].sub_tiles_flags;

         // add the flags
         for (i=0; i<25; i++)
            dsttable[i] |= u_ptr[i];
      }
   }

   // if no floor at all (F1 & F2 layer) the tile is completly unwalkable
   if (glb_ds1[ds1_idx].floor_num == 1)
   {
      if (f_ptr[0].prop1 == 0)
      {
         for (i=0; i<25; i++)
            dsttable[i] |= 1;
      }
   }
   else if (glb_ds1[ds1_idx].floor_num == 2)
   {
      if ((f_ptr[0].prop1 == 0) && (f_ptr[1].prop1 == 0))
      {
         for (i=0; i<25; i++)
            dsttable[i] |= 1;
      }
   }

   // walls
   for (w=0; w < glb_ds1[ds1_idx].wall_num; w++)
   {
      if (w_ptr[w].prop3 & 0x02)
      {
         // this is a global unwalkable info
         for (i=0; i<25; i++)
            dsttable[i] |= 1;
      }
      b = w_ptr[w].bt_idx;
      if (b > 0) // not -1 and not 0
      {
         di     = bt_ptr[b].dt1_idx;
         bi     = bt_ptr[b].block_idx;
         bh_ptr = glb_dt1[di].bh_buffer;
         u_ptr  = bh_ptr[bi].sub_tiles_flags;

         // add the flags
         for (i=0; i<25; i++)
            dsttable[i] |= u_ptr[i];

         // upper / left tile corner 2nd tile
         if (w_ptr[w].orientation == 3)
         {
            i = misc_seach_block_or4(ds1_idx,
                   bt_ptr,
                   b,
                   bt_ptr[b].main_index,
                   bt_ptr[b].sub_index
                );
            if (i != -1)
            {
               b      = i;
               di     = bt_ptr[b].dt1_idx;
               bi     = bt_ptr[b].block_idx;
               bh_ptr = glb_dt1[di].bh_buffer;
               u_ptr  = bh_ptr[bi].sub_tiles_flags;

               // add the flags
               for (i=0; i<25; i++)
                  dsttable[i] |= u_ptr[i];
            }
         }
      }
   }
}


// ==========================================================================
// load a file from mpq
//    first try in the mod directory, if not found in the 1st mpq it can
int misc_load_mpq_file(char * filename, char ** buffer, long * buf_len, int output)
{
   int  i, entry, max;


   // convert slash ('/') to backslash ('\\')
   // seems the mpq library expect backslash and not slash as directory separator
   strcpy(glb_ds1edit.strtmp, filename);
   max = strlen(glb_ds1edit.strtmp);
   for (i=0; i < max; i++)
   {
      if (glb_ds1edit.strtmp[i] == '/')
         glb_ds1edit.strtmp[i] = '\\';
   }

   // 1st try in mod directory
   for (i=0; i<MAX_MOD_DIR; i++)
   {
      entry = mod_load_in_mem(
                 glb_config.mod_dir[i],
                 glb_ds1edit.strtmp,
                 buffer,
                 buf_len
              );
      if (entry != -1)
      {
         if (output)
            printf(", found in %s\n", glb_config.mod_dir[i]);
         return entry;
      }
   }

   // 2nd try, in a mpq
   for (i=0; i<MAX_MPQ_FILE; i++)
   {
      if (glb_mpq_struct[i].is_open != FALSE)
      {
         glb_mpq = & glb_mpq_struct[i];
         entry = mpq_batch_load_in_mem(
                    glb_ds1edit.strtmp,
                    buffer,
                    buf_len,
                    output
                 );
         if (entry != -1)
         {
            if (output)
               printf(", found in %s\n", glb_config.mpq_file[i]);
            return entry;
         }
      }
   }

   // not found
   return -1;
}


// ==========================================================================
// return the column number of a txt file, given its name
int misc_get_txt_column_num(RQ_ENUM txt_idx, char * col_name)
{
   int   i = 0;
   char  * desc, tmp_str[256],
         txt_list[RQ_MAX][80] = {
            {"Data\\Global\\Excel\\LvlTypes.txt"},
            {"Data\\Global\\Excel\\LvlPrest.txt"},
            {"data\\obj.txt"},
            {"Data\\Global\\Excel\\Objects.txt"},
         };

   if (txt_idx >= RQ_MAX)
      return -1;
   for(;;)
   {
      desc = glb_txt_req_ptr[txt_idx][i];
      if (desc == NULL)
      {
         sprintf(tmp_str, "misc_get_txt_column_num() :\n"
            "   can't find <%s> in txt file <%i> <%s>\n",
            col_name, txt_idx, txt_list[txt_idx]
         );
         ds1edit_error(tmp_str);
      }
      else
      {
         if (stricmp(col_name, desc) == 0)
            return i;
         else
            i++;
      }
   }
}


// ==========================================================================
int misc_is_numerical(char * str)
{
   int i, max = strlen(str);

   for (i=0; i < max; i++)
   {
      if ( (str[i] == '-') ||
           (str[i] == '+') ||
           (str[i] == '.') ||
           ((str[i] >= '0') && (str[i] <= '9'))
         )
      {
         // ok
      }
      else
         return FALSE;
   }
   return TRUE;
}


// ==========================================================================
// correct the pl2, for making allegro draw_sprite work as expected
void misc_pl2_correct(int i)
{
   UBYTE     * bptr;
   int       c;
   COLOR_MAP * cmap;

   bptr = glb_ds1edit.d2_pal[i];
   for (c=0; c < 256; c++)
   {
      cmap = (COLOR_MAP *) & bptr[COF_75TRANS * 256];
      cmap->data[0][c] = c;

      cmap = (COLOR_MAP *) & bptr[COF_50TRANS * 256];
      cmap->data[0][c] = c;

      cmap = (COLOR_MAP *) & bptr[COF_25TRANS * 256];
      cmap->data[0][c] = c;

      cmap = (COLOR_MAP *) & bptr[COF_ALPHA * 256];
      cmap->data[0][c] = c;

      cmap = (COLOR_MAP *) & bptr[COF_LUMINANCE * 256];
      cmap->data[0][c] = c;

      cmap = (COLOR_MAP *) & bptr[COF_ALPHABRIGHT * 256];
      cmap->data[0][c] = c;
   }
}


// ==========================================================================
// parse a command line
// return -1 if error, 0 otherwise
int misc_cmd_line_parse(int argc, char ** argv)
{
   int  i                  = 0;
   int  force_dt1_present  = FALSE;
   char * ext              = NULL;
   int  n                  = 0;
   int  lvltype_id_found   = 0;
   int  lvlprest_def_found = 0;


   // is -force_dt1 present ?
   for (i=1; i < argc; i++)
   {
      if (stricmp(argv[i], "-force_dt1") == 0)
         force_dt1_present = TRUE; // no LvlType.txt ID and no LvlPrest.txt DEF expected in the arguments
   }

   // scan all parameters 1 by 1
   for (i=1; i < argc; i++)
   {
      if (i == 1)
      {
         // .ini or .ds1 ?
         ext = get_extension(argv[i]);
         if (stricmp(ext, "ini") == 0)
            glb_ds1edit.cmd_line.ini_filename = argv[i];
         else if (stricmp(ext, "ds1") == 0)
            glb_ds1edit.cmd_line.ds1_filename = argv[i];
         else
         {
            printf("misc_cmd_line_parse(), error : the first argument must be a .INI or a .DS1 file (current extension = \"%s\")\n", ext);
            return -1;
         }
      }
      else if (stricmp(argv[i], "-debug") == 0)
      {
         // -debug
         glb_ds1edit.cmd_line.debug_mode = TRUE;
      }
      else if (stricmp(argv[i], "-no_vis_debug") == 0)
      {
         // -no_vis_debug
         glb_ds1edit.cmd_line.no_vis_debug = TRUE;
      }
      else if (stricmp(argv[i], "-force_dt1") == 0)
      {
         // -force_dt1, folowed by 1 to 32 .dt1 files
         i++;
         for (n=0; ((i + n) < argc) && (stricmp(get_extension(argv[i + n]), "dt1") == 0); n++)
         {}
         if ((n < 1) || (n > DT1_IN_DS1_MAX))
         {
            printf("misc_cmd_line_parse(), error : there must be between 1 and %i .DT1 files folowing the -force_dt1 parameter\n", DT1_IN_DS1_MAX);
            return -1;
         }
         glb_ds1edit.cmd_line.dt1_list_num = n;
         for (n=0; n < glb_ds1edit.cmd_line.dt1_list_num; n++)
            glb_ds1edit.cmd_line.dt1_list_filename[n] = argv[i + n];
         i += (n - 1);
      }
      else if (stricmp(argv[i], "-resize") == 0)
      {
         // -resize, folowed by width and height values
         i++;
         if (i >= argc)
         {
            printf("misc_cmd_line_parse(), error : the -resize parameter should be folowed by width and height values\n");
            return -1;
         }
         if (misc_is_numerical(argv[i]))
            glb_ds1edit.cmd_line.resize_width = atoi(argv[i]);
         else
         {
            printf("misc_cmd_line_parse(), error : the width value folowing the -resize parameter is not a numerical value\n");
            return -1;
         }
         // height
         i++;
         if (i >= argc)
         {
            printf("misc_cmd_line_parse(), error : the -resize parameter is folowed by just a width value, the height value is missing\n");
            return -1;
         }
         if (misc_is_numerical(argv[i]))
            glb_ds1edit.cmd_line.resize_height = atoi(argv[i]);
         else
         {
            printf("misc_cmd_line_parse(), error : the height value folowing the -resize parameter is not a numerical value\n");
            return -1;
         }
      }
      else if (stricmp(argv[i], "-force_pal") == 0)
      {
         // -force_pal, folowed by an act value
         i++;
         if (i >= argc)
         {
            printf("misc_cmd_line_parse(), error : the parameter -force_pal should be folowed by an act value\n");
            return -1;
         }
         if (misc_is_numerical(argv[i]))
         {
            n = atoi(argv[i]);
            if ((n < 1) || (n > 5))
            {
               printf("misc_cmd_line_parse(), error : the act value folowing the -force_pal parameter must be between 1 and 5\n");
               return -1;
            }
            glb_ds1edit.cmd_line.force_pal_num = n;
         }
         else
         {
            printf("misc_cmd_line_parse(), error : the act value folowing the -force_pal parameter is not a numerical value\n");
            return -1;
         }
      }
      else if (stricmp(argv[i], "-no_check_act") == 0)
      {
         // -no_check_act is a flag
         glb_ds1edit.cmd_line.no_check_act = TRUE;
      }
      else if ((force_dt1_present == FALSE) && (glb_ds1edit.cmd_line.ds1_filename != NULL))
      {
         if ( ! lvltype_id_found)
         {
            // LvlType.txt ID
            lvltype_id_found = 1;
            if (misc_is_numerical(argv[i]))
               glb_ds1edit.cmd_line.lvltype_id = atoi(argv[i]);
            else
            {
               printf("misc_cmd_line_parse(), error : the LvlType.txt ID value is not a numerical value\n");
               return -1;
            }
         }
         else if ( ! lvlprest_def_found)
         {
            // LvlPrest.txt DEF
            lvlprest_def_found = 1;
            if (misc_is_numerical(argv[i]))
               glb_ds1edit.cmd_line.lvlprest_def = atoi(argv[i]);
            else
            {
               printf("misc_cmd_line_parse(), error : the LvlPrest.txt DEF value is not a numerical value\n");
               return -1;
            }
         }
         else
         {
            printf("misc_cmd_line_parse(), error : unexpected \"%s\" parameter\n", argv[i]);
            return -1;
         }
      }
   }

   return 0;
}


// ==========================================================================
void misc_draw_screen(int mx, int my)
{
   BITMAP * video_bmp    = glb_ds1edit.video_page[glb_ds1edit.video_page_num];
   BITMAP * mouse_sprite = glb_ds1edit.mouse_cursor[glb_ds1edit.mode];


   blit(
      glb_ds1edit.screen_buff,
      video_bmp,
      0, 0,
      0, 0,
      glb_config.screen.width,
      glb_config.screen.height
   );
   draw_sprite(video_bmp, mouse_sprite, mx, my);
   show_video_bitmap(video_bmp);
   glb_ds1edit.video_page_num = (glb_ds1edit.video_page_num + 1) % 2;
}

// ==================================================================================
// for a given ds1, increase its objects structures by (at least) nb_objects elements
// return 0 if ok, -1 if error
int misc_increase_ds1_objects_max(int ds1_idx, long nb_objects)
{
	long old_max;
	long new_max;
	void * new_buffer;
	long old_size;
	long new_size;
	long nb_gran;


	if (nb_objects < 0)
		return 0;

	nb_gran = 1 + (nb_objects / OBJ_MAX_GRANULARITY);
    old_max = glb_ds1[ds1_idx].current_obj_max;
    new_max = old_max + (nb_gran * OBJ_MAX_GRANULARITY);

	// drawing_order
	old_size = old_max * sizeof(int);
	new_size = new_max * sizeof(int);
	new_buffer = (int *) malloc(new_size);
	if (new_buffer == NULL)
		return -1;
    memset(new_buffer, 0x00, new_size);
	memcpy(new_buffer, glb_ds1[ds1_idx].drawing_order, old_size);
	free(glb_ds1[ds1_idx].drawing_order);
	glb_ds1[ds1_idx].drawing_order = (int *) new_buffer;

	// obj
	old_size = old_max * sizeof(OBJ_S);
	new_size = new_max * sizeof(OBJ_S);
	new_buffer = (OBJ_S *) malloc(new_size);
	if (new_buffer == NULL)
		return -1;

	memset(new_buffer, 0x00, new_size);
	memcpy(new_buffer, glb_ds1[ds1_idx].obj, old_size);
	free(glb_ds1[ds1_idx].obj);
	glb_ds1[ds1_idx].obj = (OBJ_S *) new_buffer;

	// obj_undo
	old_size = old_max * sizeof(OBJ_S);
	new_size = new_max * sizeof(OBJ_S);
	new_buffer = (OBJ_S *) malloc(new_size);
	if (new_buffer == NULL)
		return -1;
    memset(new_buffer, 0x00, new_size);
	memcpy(new_buffer, glb_ds1[ds1_idx].obj_undo, old_size);
	free(glb_ds1[ds1_idx].obj_undo);
	glb_ds1[ds1_idx].obj_undo = (OBJ_S *) new_buffer;

	glb_ds1[ds1_idx].current_obj_max = new_max;
	return 0;
}
