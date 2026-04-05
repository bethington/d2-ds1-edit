#include <string.h>
#include "structs.h"
#include "dt1misc.h"
#include "error.h"
#include "editobj.h"
#include "wpreview.h"
#include "ds1misc.h"
#include "misc.h"


// ==========================================================================
// memory free of a ds1 (with the dt1 it uses, and some other buffers)
int ds1_free(int i)
{
   int d, total = 0;
   
   if (strlen(glb_ds1[i].name) == 0)
      return 0;

   fprintf(stderr, "      . %s\n", glb_ds1[i].name);
   fflush(stderr);

   // dt1 of this ds1
   fprintf(stderr, "         _ dt1s");
   fflush(stderr);
   for (d=0; d<DT1_IN_DS1_MAX; d++)
      dt1_del(glb_ds1[i].dt1_idx[d]);
      
   // block table
   fprintf(stderr, ", block table");
   fflush(stderr);
   if (glb_ds1[i].block_table != NULL)
   {
      total += glb_ds1[i].bt_num * sizeof(BLOCK_TABLE_S);
      free(glb_ds1[i].block_table);
   }

   // file buffer
   fprintf(stderr, ", file buffer");
   fflush(stderr);
   if (glb_ds1[i].file_buff != NULL)
   {
      total += glb_ds1[i].file_len;
      free(glb_ds1[i].file_buff);
   }

   // floors
   fprintf(stderr, ", floors");
   fflush(stderr);
   if (glb_ds1[i].floor_buff != NULL)
   {
      total += glb_ds1[i].floor_buff_len;
      free(glb_ds1[i].floor_buff);
   }
   if (glb_ds1[i].floor_buff2 != NULL)
   {
      total += glb_ds1[i].floor_buff_len;
      free(glb_ds1[i].floor_buff2);
   }

   // shadow
   fprintf(stderr, ", shadows");
   fflush(stderr);
   if (glb_ds1[i].shadow_buff != NULL)
   {
      total += glb_ds1[i].shadow_buff_len;
      free(glb_ds1[i].shadow_buff);
   }
   if (glb_ds1[i].shadow_buff2 != NULL)
   {
      total += glb_ds1[i].shadow_buff_len;
      free(glb_ds1[i].shadow_buff2);
   }

   // tag
   fprintf(stderr, ", unknown");
   fflush(stderr);
   if (glb_ds1[i].tag_buff != NULL)
   {
      total += glb_ds1[i].tag_buff_len;
      free(glb_ds1[i].tag_buff);
   }
   if (glb_ds1[i].tag_buff2 != NULL)
   {
      total += glb_ds1[i].tag_buff_len;
      free(glb_ds1[i].tag_buff2);
   }

   // walls
   fprintf(stderr, ", walls\n");
   fflush(stderr);
   if (glb_ds1[i].wall_buff != NULL)
   {
      total += glb_ds1[i].wall_buff_len;
      free(glb_ds1[i].wall_buff);
   }
   if (glb_ds1[i].wall_buff2 != NULL)
   {
      total += glb_ds1[i].wall_buff_len;
      free(glb_ds1[i].wall_buff2);
   }

   // groups
   if (glb_ds1[i].group != NULL)
      free(glb_ds1[i].group);

   // end
   memset(& glb_ds1[i], 0, sizeof(DS1_S));
   fprintf(stderr, "         bytes free = %i\n", total);
   fflush(stderr);
   return total;
}


// ==========================================================================
// search the wall tile, giving the orientation/prop1/2/3/4
void wprop_2_block(int i, CELL_W_S * w_ptr)
{
   BLOCK_TABLE_S * bt_ptr = glb_ds1[i].block_table;
   long          orientation, main_index, sub_index;
   int           b;

   w_ptr->bt_idx = -1; // not found by default
   if (w_ptr->prop1 == 0)
   {
      // no tile here
      w_ptr->bt_idx = 0;
      return;
   }
      
   orientation = w_ptr->orientation;
   main_index  = (w_ptr->prop3 >> 4) + ((w_ptr->prop4 & 0x03) << 4);
   sub_index   = w_ptr->prop2;
   for (b=0; b<glb_ds1[i].bt_num; b++)
   {
      if ( (bt_ptr->orientation == orientation) &&
           (bt_ptr->main_index  == main_index)  &&
           (bt_ptr->sub_index   == sub_index)   &&
           (bt_ptr->used_by_editor)
         )
      {
         w_ptr->bt_idx = b;
         return;
      }
      bt_ptr++;
   }

   // trick of O=18 M=3 I=1 ---> O=19 M=3 I=0
   if ((orientation == 18) || (orientation == 19))
   {
      if (orientation == 18)
         orientation = 19;
      else
         orientation = 18;

      // search with same sub_index (just in case)
      bt_ptr = glb_ds1[i].block_table;
      for (b=0; b<glb_ds1[i].bt_num; b++)
      {
         if ( (bt_ptr->orientation == orientation) &&
              (bt_ptr->main_index  == main_index)  &&
              (bt_ptr->sub_index   == sub_index)   &&
              (bt_ptr->used_by_editor)
            )
         {
            w_ptr->bt_idx = b;
            return;
         }
         bt_ptr++;
      }

      // search with sub_index = 0
      sub_index = 0;
      bt_ptr = glb_ds1[i].block_table;
      for (b=0; b<glb_ds1[i].bt_num; b++)
      {
         if ( (bt_ptr->orientation == orientation) &&
              (bt_ptr->main_index  == main_index)  &&
              (bt_ptr->sub_index   == sub_index)   &&
              (bt_ptr->used_by_editor)
            )
         {
            w_ptr->bt_idx = b;
            return;
         }
         bt_ptr++;
      }
   }
}


// ==========================================================================
// search the floor tile, giving the prop1/2/3/4
void fprop_2_block(int i, CELL_F_S * f_ptr)
{
   BLOCK_TABLE_S * bt_ptr = glb_ds1[i].block_table;
   long          main_index, sub_index;
   int           b;

   f_ptr->bt_idx = -1; // not found by default
   if (f_ptr->prop1 == 0)
   {
      // no tile here
      f_ptr->bt_idx = 0;
      return;
   }
   
   main_index  = (f_ptr->prop3 >> 4) + ((f_ptr->prop4 & 0x03) << 4);
   sub_index   = f_ptr->prop2;
   for (b=0; b<glb_ds1[i].bt_num; b++)
   {
      if ( (bt_ptr->orientation == 0)          &&
           (bt_ptr->main_index  == main_index) &&
           (bt_ptr->sub_index   == sub_index)  &&
           (bt_ptr->used_by_editor)
         )
      {
         f_ptr->bt_idx = b;
         return;
      }
      bt_ptr++;
   }
}


// ==========================================================================
// search the shadow tile, giving the prop1/2/3/4
void sprop_2_block(int i, CELL_S_S * s_ptr)
{
   BLOCK_TABLE_S * bt_ptr = glb_ds1[i].block_table;
   long          main_index, sub_index;
   int           b;

   s_ptr->bt_idx = -1; // not found by default
   if (s_ptr->prop1 == 0)
   {
      // no tile here
      s_ptr->bt_idx = 0;
      return;
   }
      
   main_index  = (s_ptr->prop3 >> 4) + ((s_ptr->prop4 & 0x03) << 4);
   sub_index   = s_ptr->prop2;
   for (b=0; b<glb_ds1[i].bt_num; b++)
   {
      if ( (bt_ptr->orientation == 13)         &&
           (bt_ptr->main_index  == main_index) &&
           (bt_ptr->sub_index   == sub_index)  &&
           (bt_ptr->used_by_editor)
         )
      {
         s_ptr->bt_idx = b;
         return;
      }
      bt_ptr++;
   }
}


// ==========================================================================
// search all the tiles (ds1 to dt1 relations)
void ds1_make_prop_2_block(int i)
{
   CELL_F_S * f_ptr;
   CELL_S_S * s_ptr;
   CELL_W_S * w_ptr;
   int      x, y, n;
   
   w_ptr = glb_ds1[i].wall_buff;
   f_ptr = glb_ds1[i].floor_buff;
   s_ptr = glb_ds1[i].shadow_buff;

   for (y=0; y<glb_ds1[i].height; y++)
   {
      for (x=0; x<glb_ds1[i].width; x++)
      {
         for (n=0; n<glb_ds1[i].wall_num; n++)
         {
            wprop_2_block(i, w_ptr);
            w_ptr++;
         }
         for (n=0; n<glb_ds1[i].floor_num; n++)
         {
            fprop_2_block(i, f_ptr);
            f_ptr++;
         }
         for (n=0; n<glb_ds1[i].shadow_num; n++)
         {
            sprop_2_block(i, s_ptr);
/*
if (s_ptr->prop1)
{
 printf("shadow %3i, %3i : %3i %3i %3i %3i --> ",
 x, y,
 s_ptr->prop1,
 s_ptr->prop2,
 s_ptr->prop3,
 s_ptr->prop4);
 sprop_2_block(i, s_ptr);
 if (s_ptr->bt_idx == -1)
    printf("NOT FOUND with orientation 13\n");
 else
    printf("block table %i\n", s_ptr->bt_idx);
}
else
 sprop_2_block(i, s_ptr);
*/ 
            s_ptr++;
         }
      }
   }
}


// ==========================================================================
// read a ds1
//    load the file into memory, then copy the datas into the right structures
//    init some var, to prepare its display (zoom, center, layer mask...)
// if new_width OR new_height is <= 0, then no change to the size of the ds1, else
// it'll crop or add necessary Tiles 
int ds1_read(const char * ds1name, int ds1_idx, int new_width, int new_height)
{
   FILE        * in;
   CELL_F_S    * f_ptr[FLOOR_MAX_LAYER];
   CELL_S_S    * s_ptr[SHADOW_MAX_LAYER];
   CELL_T_S    * t_ptr[TAG_MAX_LAYER];
   CELL_W_S    * w_ptr[WALL_MAX_LAYER];
   CELL_W_S    * o_ptr[WALL_MAX_LAYER];
   OBJ_LABEL_S * label;
   int         o, x, y, nb_layer, size, n, p, ds1_len, done, cx, cy, dx, dy,
               current_valid_obj_idx=0, max_subtile_width, max_subtile_height;
   long        w_num, f_num, s_num, t_num, * ptr, npc, path;
   int         lay_stream[14],
               dir_lookup[25] = {
                  0x00, 0x01, 0x02, 0x01, 0x02, 0x03, 0x03, 0x05, 0x05, 0x06,
                  0x06, 0x07, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                  0x0F, 0x10, 0x11, 0x12, 0x14
               };
   UBYTE       * bptr;
   void        * ds1_buff;
   int         obj_path_warn_wrote = FALSE, last_o, nb;
   char        tmp[150], * cptr;
   long        tile_x, tile_y, zoom;
   long        incr;
   

   if (ds1_idx >= DS1_MAX)
   {
      sprintf(tmp, "not enough free ds1 slot for %s, max is %i", ds1name,
         DS1_MAX);
      ds1edit_error(tmp);
   }

   if (strlen(glb_ds1[ds1_idx].name))
   {
      sprintf(tmp, "ds1 slot %i is already used by %s", ds1_idx, glb_ds1[ds1_idx].name);
      ds1edit_error(tmp);
   }
   if (stricmp(glb_ds1[ds1_idx].name, ds1name) == 0)
   {
      sprintf(tmp, "ds1 slot %i is already loaded (%s)", ds1_idx, glb_ds1[ds1_idx].name);
      ds1edit_error(tmp);
   }

   // take file name without path
   x = strlen(ds1name) - 1;
   while ((x >= 0) && (ds1name[x] != '\\') && (ds1name[x] != '/') && (ds1name[x] != ':'))
      x--;
   if (x == 0)
      strcpy(glb_ds1[ds1_idx].filename, ds1name);
   else
      strcpy(glb_ds1[ds1_idx].filename, ds1name + x + 1);
   strcpy(glb_ds1[ds1_idx].name, ds1name);

   // initialise max number of objects
   glb_ds1[ds1_idx].current_obj_max = OBJ_MAX_START;

   // drawing_order
   size = glb_ds1[ds1_idx].current_obj_max * sizeof(int);
   glb_ds1[ds1_idx].drawing_order = (int *) calloc(glb_ds1[ds1_idx].current_obj_max, sizeof(int));
   if (glb_ds1[ds1_idx].drawing_order == NULL)
   {
      sprintf(tmp, "not enough mem (%i bytes) for glb_ds1[].drawing_order of %s\n", size, ds1name);
      ds1edit_error(tmp);
   }

   // obj
   size = glb_ds1[ds1_idx].current_obj_max * sizeof(OBJ_S);
   glb_ds1[ds1_idx].obj = (OBJ_S *) calloc(glb_ds1[ds1_idx].current_obj_max, sizeof(OBJ_S));
   if (glb_ds1[ds1_idx].obj == NULL)
   {
      sprintf(tmp, "not enough mem (%i bytes) for glb_ds1[].obj of %s\n", size, ds1name);
      ds1edit_error(tmp);
   }

   // obj_undo
   glb_ds1[ds1_idx].obj_undo = (OBJ_S *) calloc(glb_ds1[ds1_idx].current_obj_max, sizeof(OBJ_S));
   if (glb_ds1[ds1_idx].obj_undo == NULL)
   {
      sprintf(tmp, "not enough mem (%i bytes) for glb_ds1[].obj_undo of %s\n", size, ds1name);
      ds1edit_error(tmp);
   }
   
   // layers mask
   for (x=0; x < WALL_MAX_LAYER; x++)
      glb_ds1[ds1_idx].wall_layer_mask[x] = 1;

   for (x=0; x < FLOOR_MAX_LAYER; x++)
      glb_ds1[ds1_idx].floor_layer_mask[x] = 1;

   for (x=0; x < SHADOW_MAX_LAYER; x++)
      glb_ds1[ds1_idx].shadow_layer_mask[x] = 3; // transparent is default

   glb_ds1[ds1_idx].objects_layer_mask    = OL_NONE;
   glb_ds1[ds1_idx].paths_layer_mask      = 1;
   glb_ds1[ds1_idx].walkable_layer_mask   = 0;
   glb_ds1[ds1_idx].subtile_help_display  = 1;
   glb_ds1[ds1_idx].animations_layer_mask = 1;
   glb_ds1[ds1_idx].special_layer_mask    = 1;
   
   // load from disk into memory
   in = fopen(ds1name, "rb");
   if (in == NULL)
   {
      sprintf(tmp, "can't open %s\n", ds1name);
      ds1edit_error(tmp);
   }
   fseek(in, 0, SEEK_END);
   ds1_len = ftell(in);
   fseek(in, 0, SEEK_SET);
   ds1_buff = (void *) malloc(ds1_len);
   if (ds1_buff == NULL)
   {
      fclose(in);
      sprintf(tmp, "not enough mem (%i bytes) for %s\n", ds1_len, ds1name);
      ds1edit_error(tmp);
   }
   fread(ds1_buff, ds1_len, 1, in);
   fclose(in);

   // inits
   w_num = 0; // # of wall & orientation layers
   f_num = 0; // # of floor layer
   s_num = 1; // # of shadow layer, always here
   t_num = 0; // # of tag layer
   ptr   = (long *) ds1_buff;


   // copy datas from buffer into ds1 struct :

   // version
   glb_ds1[ds1_idx].version = * ptr;
   ptr++;

   // widht
   glb_ds1[ds1_idx].width = (* ptr) + 1;
   ptr++;

   // height
   glb_ds1[ds1_idx].height = (* ptr) + 1;
   ptr++;
   
   // will we use new size ?
   if ((new_width <= 0) || (new_height <= 0))
   {
      new_width  = glb_ds1[ds1_idx].width;
      new_height = glb_ds1[ds1_idx].height;
   }
               
   // act
   glb_ds1[ds1_idx].act = 1;
   if (glb_ds1[ds1_idx].version >= 8)
   {
      glb_ds1[ds1_idx].act = (* ptr) + 1;
      ptr++;
      if (glb_ds1[ds1_idx].act > 5)
         glb_ds1[ds1_idx].act = 5;
   }

   // tag_type
   glb_ds1[ds1_idx].tag_type = 0;
   if (glb_ds1[ds1_idx].version >= 10)
   {
      glb_ds1[ds1_idx].tag_type = * ptr;
      ptr++;

      // adjust eventually the # of tag layer
      if ((glb_ds1[ds1_idx].tag_type == 1) || (glb_ds1[ds1_idx].tag_type == 2))
         t_num = 1;
   }

   // debug
   printf("\nversion         : %li\n", glb_ds1[ds1_idx].version);
   printf("width           : %li\n", glb_ds1[ds1_idx].width);
   printf("height          : %li\n", glb_ds1[ds1_idx].height);
   printf("act             : %li\n", glb_ds1[ds1_idx].act);
   printf("tag_type        : %li\n", glb_ds1[ds1_idx].tag_type);
   
   // filenames
   glb_ds1[ds1_idx].file_num = 0;
   if (glb_ds1[ds1_idx].version >= 3)
   {
      glb_ds1[ds1_idx].file_num = * ptr;
      ptr++;
      n = 0;
      printf("filenames       : %li\n", glb_ds1[ds1_idx].file_num);
      
      for (x=0; x<glb_ds1[ds1_idx].file_num; x++)
      {
         printf("   %2i : %s\n", x + 1, ((char *) ptr) + n);
         n += (strlen( ((char *) ptr) + n) + 1);
      }
         
      glb_ds1[ds1_idx].file_buff = (void *) malloc(n);
      if (glb_ds1[ds1_idx].file_buff == NULL)
      {
         free(ds1_buff);
         sprintf(tmp, "not enough mem (%i bytes) for files in the ds1\n", n);
         ds1edit_error(tmp);
      }
      glb_ds1[ds1_idx].file_len = n;
      memcpy(glb_ds1[ds1_idx].file_buff, ptr, n);
      ptr = (long *) (((char *) ptr) + n);
   }
   else
      printf("no filenames\n");

   // skip 2 dwords ?
   if ((glb_ds1[ds1_idx].version >= 9) && (glb_ds1[ds1_idx].version <= 13))
      ptr += 2;

   // number of wall, floor and tag layers
   if (glb_ds1[ds1_idx].version >= 4)
   {
      // number of wall (and orientation) layers
      w_num = * ptr;
      ptr++;

      // number of floor layers
      if (glb_ds1[ds1_idx].version >= 16)
      {
         f_num = * ptr;
         ptr++;
      }
      else
         f_num = 1; // default # of floor layer
   }
   else // in version < 4
   {
      // # of layers hardcoded
      w_num = 1;
      f_num = 1;
      t_num = 1;
   }

   // which order ?
   if (glb_ds1[ds1_idx].version < 4)
   {
      lay_stream[0] =  1; // wall 1
      lay_stream[1] =  9; // floor 1
      lay_stream[2] =  5; // orientation 1
      lay_stream[3] = 12; // tag
      lay_stream[4] = 11; // shadow
      nb_layer = 5;
   }
   else
   {
      nb_layer = 0;
      for (x=0; x<w_num; x++)
      {
         lay_stream[nb_layer++] = 1 + x; // wall x
         lay_stream[nb_layer++] = 5 + x; // orientation x
      }
      for (x=0; x<f_num; x++)
         lay_stream[nb_layer++] = 9 + x; // floor x
      if (s_num)
         lay_stream[nb_layer++] = 11;    // shadow
      if (t_num)
         lay_stream[nb_layer++] = 12;    // tag
   }
   printf("layers          : (2 * %li walls) + %li floors + %li shadow"
   " + %li tag\n", w_num, f_num, s_num, t_num);
   
   // layers num
   if (glb_config.always_max_layers)
   {
      f_num = FLOOR_MAX_LAYER;
      w_num = WALL_MAX_LAYER;
   }
   glb_ds1[ds1_idx].floor_num  = f_num;
   glb_ds1[ds1_idx].shadow_num = s_num;
   glb_ds1[ds1_idx].tag_num    = t_num;
   glb_ds1[ds1_idx].wall_num   = w_num;

   // floor buffer
   glb_ds1[ds1_idx].floor_line     = new_width * glb_ds1[ds1_idx].floor_num;
   glb_ds1[ds1_idx].floor_len      = glb_ds1[ds1_idx].floor_line * new_height;
   glb_ds1[ds1_idx].floor_buff_len = glb_ds1[ds1_idx].floor_len * sizeof(CELL_F_S);
   glb_ds1[ds1_idx].floor_buff     = (CELL_F_S *) malloc(glb_ds1[ds1_idx].floor_buff_len);
   if (glb_ds1[ds1_idx].floor_buff == NULL)
   {
      free(ds1_buff);
      sprintf(tmp, "not enough mem (%i bytes) for floor buffer\n",
         glb_ds1[ds1_idx].floor_buff_len);
      ds1edit_error(tmp);
   }
   memset(glb_ds1[ds1_idx].floor_buff, 0, glb_ds1[ds1_idx].floor_buff_len);

   glb_ds1[ds1_idx].floor_buff2     = (CELL_F_S *) malloc(glb_ds1[ds1_idx].floor_buff_len);
   if (glb_ds1[ds1_idx].floor_buff2 == NULL)
   {
      free(ds1_buff);
      sprintf(tmp, "not enough mem (%i bytes) for floor 2nd buffer\n",
         glb_ds1[ds1_idx].floor_buff_len);
      ds1edit_error(tmp);
   }
   memset(glb_ds1[ds1_idx].floor_buff2, 0, glb_ds1[ds1_idx].floor_buff_len);

   // shadow buffer
   glb_ds1[ds1_idx].shadow_line     = new_width * glb_ds1[ds1_idx].shadow_num;
   glb_ds1[ds1_idx].shadow_len      = glb_ds1[ds1_idx].shadow_line * new_height;
   glb_ds1[ds1_idx].shadow_buff_len = glb_ds1[ds1_idx].shadow_len * sizeof(CELL_S_S);
   glb_ds1[ds1_idx].shadow_buff     = (CELL_S_S *) malloc(glb_ds1[ds1_idx].shadow_buff_len);
   if (glb_ds1[ds1_idx].shadow_buff == NULL)
   {
      free(ds1_buff);
      sprintf(tmp, "not enough mem (%i bytes) for shadow buffer\n",
         glb_ds1[ds1_idx].shadow_buff_len);
      ds1edit_error(tmp);
   }
   memset(glb_ds1[ds1_idx].shadow_buff, 0, glb_ds1[ds1_idx].shadow_buff_len);
   
   glb_ds1[ds1_idx].shadow_buff2     = (CELL_S_S *) malloc(glb_ds1[ds1_idx].shadow_buff_len);
   if (glb_ds1[ds1_idx].shadow_buff2 == NULL)
   {
      free(ds1_buff);
      sprintf(tmp, "not enough mem (%i bytes) for shadow 2nd buffer\n",
         glb_ds1[ds1_idx].shadow_buff_len);
      ds1edit_error(tmp);
   }
   memset(glb_ds1[ds1_idx].shadow_buff2, 0, glb_ds1[ds1_idx].shadow_buff_len);

   // tag buffer
   glb_ds1[ds1_idx].tag_line     = new_width * glb_ds1[ds1_idx].tag_num;
   glb_ds1[ds1_idx].tag_len      = glb_ds1[ds1_idx].tag_line * new_height;
   glb_ds1[ds1_idx].tag_buff_len = glb_ds1[ds1_idx].tag_len * sizeof(CELL_T_S);
   glb_ds1[ds1_idx].tag_buff     = (CELL_T_S *) malloc(glb_ds1[ds1_idx].tag_buff_len);
   if (glb_ds1[ds1_idx].tag_buff == NULL)
   {
      free(ds1_buff);
      sprintf(tmp, "not enough mem (%i bytes) for tag buffer\n",
         glb_ds1[ds1_idx].tag_buff_len);
      ds1edit_error(tmp);
   }
   memset(glb_ds1[ds1_idx].tag_buff, 0, glb_ds1[ds1_idx].tag_buff_len);

   glb_ds1[ds1_idx].tag_buff2     = (CELL_T_S *) malloc(glb_ds1[ds1_idx].tag_buff_len);
   if (glb_ds1[ds1_idx].tag_buff2 == NULL)
   {
      free(ds1_buff);
      sprintf(tmp, "not enough mem (%i bytes) for tag 2nd buffer\n",
         glb_ds1[ds1_idx].tag_buff_len);
      ds1edit_error(tmp);
   }
   memset(glb_ds1[ds1_idx].tag_buff2, 0, glb_ds1[ds1_idx].tag_buff_len);

   // wall buffer
   glb_ds1[ds1_idx].wall_line     = new_width * glb_ds1[ds1_idx].wall_num;
   glb_ds1[ds1_idx].wall_len      = glb_ds1[ds1_idx].wall_line * new_height;
   glb_ds1[ds1_idx].wall_buff_len = glb_ds1[ds1_idx].wall_len * sizeof(CELL_W_S);
   glb_ds1[ds1_idx].wall_buff     = (CELL_W_S *) malloc(glb_ds1[ds1_idx].wall_buff_len);
   if (glb_ds1[ds1_idx].wall_buff == NULL)
   {
      free(ds1_buff);
      sprintf(tmp, "not enough mem (%i bytes) for wall buffer\n",
         glb_ds1[ds1_idx].wall_buff_len);
      ds1edit_error(tmp);
   }
   memset(glb_ds1[ds1_idx].wall_buff, 0, glb_ds1[ds1_idx].wall_buff_len);

   glb_ds1[ds1_idx].wall_buff2     = (CELL_W_S *) malloc(glb_ds1[ds1_idx].wall_buff_len);
   if (glb_ds1[ds1_idx].wall_buff2 == NULL)
   {
      free(ds1_buff);
      sprintf(tmp, "not enough mem (%i bytes) for wall 2nd buffer\n",
         glb_ds1[ds1_idx].wall_buff_len);
      ds1edit_error(tmp);
   }
   memset(glb_ds1[ds1_idx].wall_buff2, 0, glb_ds1[ds1_idx].wall_buff_len);

   // read tiles of layers

   // set pointers
   for (x=0; x<FLOOR_MAX_LAYER; x++)
      f_ptr[x] = glb_ds1[ds1_idx].floor_buff + x;
      
   for (x=0; x<SHADOW_MAX_LAYER; x++)
      s_ptr[x] = glb_ds1[ds1_idx].shadow_buff + x;

   for (x=0; x<TAG_MAX_LAYER; x++)
      t_ptr[x] = glb_ds1[ds1_idx].tag_buff + x;

   for (x=0; x<WALL_MAX_LAYER; x++)
      o_ptr[x] = w_ptr[x] = glb_ds1[ds1_idx].wall_buff + x;
   
   bptr  = (UBYTE *) ptr;
   
   for (n=0; n < nb_layer; n++)
   {
      for (y=0; y < glb_ds1[ds1_idx].height; y++)
      {
         for (x=0; x < glb_ds1[ds1_idx].width; x++)
         {
            switch (lay_stream[n])
            {
               // walls
               case  1:
               case  2:
               case  3:
               case  4:
                  if ((x < new_width) && (y < new_height))
                  {
                     p = lay_stream[n] - 1;
                     w_ptr[p]->prop1 = * bptr;
                     bptr++;
                     w_ptr[p]->prop2 = * bptr;
                     bptr++;
                     w_ptr[p]->prop3 = * bptr;
                     bptr++;
                     w_ptr[p]->prop4 = * bptr;
                     bptr++;
                     w_ptr[p] += w_num;
                  }
                  else
                     bptr += 4;
                  break;

               // orientations
               case  5:
               case  6:
               case  7:
               case  8:
                  if ((x < new_width) && (y < new_height))
                  {
                     p = lay_stream[n] - 5;
                     if (glb_ds1[ds1_idx].version < 7)
                        o_ptr[p]->orientation = dir_lookup[* bptr];
                     else
                        o_ptr[p]->orientation = * bptr;
                     o_ptr[p] += w_num;
                  }
                  bptr += 4;
                  break;

               // floors
               case  9:
               case 10:
                  if ((x < new_width) && (y < new_height))
                  {
                     p = lay_stream[n] - 9;
                     f_ptr[p]->prop1 = * bptr;
                     bptr++;
                     f_ptr[p]->prop2 = * bptr;
                     bptr++;
                     f_ptr[p]->prop3 = * bptr;
                     bptr++;
                     f_ptr[p]->prop4 = * bptr;
                     bptr++;
                     f_ptr[p] += f_num;
                  }
                  else
                     bptr += 4;
                  break;

               // shadow
               case 11:
                  if ((x < new_width) && (y < new_height))
                  {
                     p = lay_stream[n] - 11;
                     s_ptr[p]->prop1 = * bptr;
                     bptr++;
                     s_ptr[p]->prop2 = * bptr;
                     bptr++;
                     s_ptr[p]->prop3 = * bptr;
                     bptr++;
                     s_ptr[p]->prop4 = * bptr;
                     bptr++;
                     s_ptr[p] += s_num;
                  }
                  else
                     bptr += 4;
                  break;

               // tag
               case 12:
                  if ((x < new_width) && (y < new_height))
                  {
                     p = lay_stream[n] - 12;
                     t_ptr[p]->num = (UDWORD) * ((UDWORD *) bptr);
                     t_ptr[p] += t_num;
                  }
                  bptr += 4;
                  break;
            }
         }

         // in case of bigger width
         p = new_width - glb_ds1[ds1_idx].width;
         if (p > 0)
         {
            switch (lay_stream[n])
            {
               // walls
               case  1:
               case  2:
               case  3:
               case  4:
                  w_ptr[lay_stream[n] - 1] += p * w_num;
                  break;

               // orientations
               case  5:
               case  6:
               case  7:
               case  8:
                  o_ptr[lay_stream[n] - 5] += p * w_num;
                  break;

               // floors
               case  9:
               case 10:
                  f_ptr[lay_stream[n] - 9] += p * f_num;
                  break;
      
               // shadow
               case 11:
                  s_ptr[lay_stream[n] - 11] += p * s_num;
                  break;

               // tag
               case 12:
                  t_ptr[lay_stream[n] - 12] += p * t_num;
                  break;
            }
         }
      }
   }

   // update new size of the ds1
   glb_ds1[ds1_idx].width  = new_width;
   glb_ds1[ds1_idx].height = new_height;

   //now we're on the objects data
   ptr = (long *) bptr;
   
   glb_ds1[ds1_idx].obj_num = 0;
   if (glb_ds1[ds1_idx].version >= 2)
   {
      glb_ds1[ds1_idx].obj_num = * ptr;
      ptr++;

      printf("objects         : %li\n", glb_ds1[ds1_idx].obj_num);
   
      if (glb_ds1[ds1_idx].obj_num > glb_ds1[ds1_idx].current_obj_max)
	  {
		  incr = 1 + glb_ds1[ds1_idx].obj_num - glb_ds1[ds1_idx].current_obj_max;
		  if (misc_increase_ds1_objects_max(ds1_idx, incr) != 0)
		  {
			 free(ds1_buff);
			 sprintf(tmp, "too many objects (%i), editor max is <%i>\n",
				glb_ds1[ds1_idx].obj_num, glb_ds1[ds1_idx].current_obj_max);
			 ds1edit_error(tmp);
		  }
	  }
      current_valid_obj_idx = 0;
      max_subtile_width     = new_width * 5;
      max_subtile_height    = new_height * 5;
      for (n=0; n < glb_ds1[ds1_idx].obj_num; n++)
      {
         glb_ds1[ds1_idx].obj[current_valid_obj_idx].type  = * ptr;
         ptr++;
         glb_ds1[ds1_idx].obj[current_valid_obj_idx].id    = * ptr;
         ptr++;
         x = glb_ds1[ds1_idx].obj[current_valid_obj_idx].x = * ptr;
         ptr++;
         y = glb_ds1[ds1_idx].obj[current_valid_obj_idx].y = * ptr;
         ptr++;

         if (glb_ds1[ds1_idx].version > 5)
         {
            // flags
            glb_ds1[ds1_idx].obj[current_valid_obj_idx].ds1_flags = * ptr;
            ptr++;
         }

         // integrity check (not done by the game I believe)
         if ((x >= 0) && (x < max_subtile_width) && (y >= 0) && (y < max_subtile_height))
         {
            // some init for the paths of this object
            glb_ds1[ds1_idx].obj[current_valid_obj_idx].path_num = 0;
            glb_ds1[ds1_idx].obj[current_valid_obj_idx].desc_idx = -1;
            glb_ds1[ds1_idx].obj[current_valid_obj_idx].flags    = 0;

			glb_ds1[ds1_idx].obj[current_valid_obj_idx].frame_delta = rand()%256;

            label = & glb_ds1[ds1_idx].obj[current_valid_obj_idx].label;
            label->rx = label->ry = label->w = label->h = label->flags = 0;

            editobj_make_obj_desc(ds1_idx, current_valid_obj_idx);
            current_valid_obj_idx++;
         }
         else
         {
            // don't use that object
            memset(
               & glb_ds1[ds1_idx].obj[current_valid_obj_idx],
               0,
               sizeof(glb_ds1[ds1_idx].obj[current_valid_obj_idx])
            );
         }
      }
   }
   else
      printf("no objects\n");

   // update the new number of objects in that ds1
   glb_ds1[ds1_idx].obj_num = current_valid_obj_idx;

    /* groups for tag layer

      warning : in fact there can be less groups than expected
      like in data\global\tiles\act1\outdoors\trees.ds1, where the file
      stop right after the last tile_x group data, leaving the other
      datas unknown (tile_y, width, height), and npc paths unknown */
   
   if ( (glb_ds1[ds1_idx].version >= 12)  &&
        ((glb_ds1[ds1_idx].tag_type == 1) || (glb_ds1[ds1_idx].tag_type == 2))
      )
   {
      // skip 1 dword ?
      if (glb_ds1[ds1_idx].version >= 18)
         ptr++;
         
      glb_ds1[ds1_idx].group_num = n = * ptr;
      ptr++;

      printf("groups          : %li\n", n);
      
      // malloc
      glb_ds1[ds1_idx].group_size = size = n * sizeof(GROUP_S);
      glb_ds1[ds1_idx].group = (GROUP_S *) malloc(size);
      if (glb_ds1[ds1_idx].group == NULL)
      {
         free(ds1_buff);
         sprintf(tmp, "not enough mem (%i bytes) for groups\n", size);
         ds1edit_error(tmp);
      }
      memset(glb_ds1[ds1_idx].group, 0, size);

      // fill it
      for (x=0; x<n; x++)
      {
         if ((UDWORD) ptr < (((UDWORD) ds1_buff) + ds1_len))
            glb_ds1[ds1_idx].group[x].tile_x = * ptr;
         ptr++;
         if ((UDWORD) ptr < (((UDWORD) ds1_buff) + ds1_len))
            glb_ds1[ds1_idx].group[x].tile_y = * ptr;
         ptr++;
         if ((UDWORD) ptr < (((UDWORD) ds1_buff) + ds1_len))
            glb_ds1[ds1_idx].group[x].width  = * ptr;
         ptr++;
         if ((UDWORD) ptr < (((UDWORD) ds1_buff) + ds1_len))
            glb_ds1[ds1_idx].group[x].height = * ptr;
         ptr++;
         if (glb_ds1[ds1_idx].version >= 13)
         {
            if ((UDWORD) ptr < (((UDWORD) ds1_buff) + ds1_len))
               glb_ds1[ds1_idx].group[x].unk = * ptr;
            ptr++;
         }
      }
   }
   else
      printf("no groups\n");

   // now we're on the npc's paths datas
   if (glb_ds1[ds1_idx].version >= 14)
   {
      if ((UDWORD) ptr < (((UDWORD) ds1_buff) + ds1_len))
         npc = * ptr;
      else
         npc = 0;
      ptr++;
      printf("npc paths       : %li\n", npc);
      for (n=0; n<npc; n++)
      {
         path = * ptr;
         ptr++;
         x = * ptr;
         ptr++;
         y = * ptr;
         ptr++;
            
         // search of which object are these paths datas
         o = last_o = nb = 0;
         done = FALSE;
         while (! done)
         {
            if (o < glb_ds1[ds1_idx].obj_num)
            {
               if ((glb_ds1[ds1_idx].obj[o].x == x) && (glb_ds1[ds1_idx].obj[o].y == y))
               {
                  last_o = o;
                  nb++;
                  if (nb >= 2)
                     done = TRUE;
               }
               o++; // next object
            }
            else
               done = TRUE;
         }

         if (nb >= 2)
         {
            // there are a least 2 objects at the same coordinates

            // put a warning
            if (obj_path_warn_wrote != TRUE)
            {
               obj_path_warn_wrote = TRUE;
               printf("\n"
                      "ds1_read() : WARNING, there are at least 2 objects at the same coordinates for some paths datas.\n"
               );
            }
            printf("   * Removing %i paths points of 1 object at coordinates (%i, %i)\n",
               path, x, y);
            fflush(stdout);

            // first, delete already assigned paths
            for (o=0; o < glb_ds1[ds1_idx].obj_num; o++)
            {
               if ((glb_ds1[ds1_idx].obj[o].x == x) && (glb_ds1[ds1_idx].obj[o].y == y) &&
                   (glb_ds1[ds1_idx].obj[o].path_num != 0))
               {
                  for (p=0; p < glb_ds1[ds1_idx].obj[o].path_num; p++)
                  {
                     glb_ds1[ds1_idx].obj[o].path[p].x      = 0;
                     glb_ds1[ds1_idx].obj[o].path[p].y      = 0;
                     glb_ds1[ds1_idx].obj[o].path[p].action = 0;
                     glb_ds1[ds1_idx].obj[o].path[p].flags  = 0;
                  }
                  glb_ds1[ds1_idx].obj[o].path_num = 0;
               }
            }

            // now, skip these paths
            if (glb_ds1[ds1_idx].version >= 15)
            {
               for (p=0; p < path; p++)
                  ptr += 3; // skip 3 dwords per path
            }
            else
            {
               for (p=0; p < path; p++)
                  ptr += 2; // skip 2 dwords only per path, no 'action'
            }
         }
         else
         {
            // only 1 object at these coordinates for paths, it's ok
            o = last_o;

            // does these paths are pointing to a valid object position ?
            if (o >= glb_ds1[ds1_idx].obj_num)
            {
               // nope
               // the game don't alert the user, so why me ?
               // but we'll skip them
               if (glb_ds1[ds1_idx].version >= 15)
               {
                  for (p=0; p < path; p++)
                     ptr += 3; // skip 3 dwords per path
               }
               else
               {
                  for (p=0; p < path; p++)
                     ptr += 2; // skip 2 dwords only per path, no 'action'
               }
            }
            else
            {
               // yep, valid object

               if (path > WINDS1EDIT_PATH_MAX)
               {
                  free(ds1_buff);
                  sprintf(tmp, "object %li have too much paths (%i), editor max is %i\n",
                     o, path, WINDS1EDIT_PATH_MAX);
                  ds1edit_error(tmp);
               }

               // all ok for assigning the paths to this object
               glb_ds1[ds1_idx].obj[o].path_num = path;
               for (p=0; p < path; p++)
               {
                  glb_ds1[ds1_idx].obj[o].path[p].x = * ptr;
                  ptr++;
                  glb_ds1[ds1_idx].obj[o].path[p].y = * ptr;
                  ptr++;
                  if (glb_ds1[ds1_idx].version >= 15)
                  {
                     glb_ds1[ds1_idx].obj[o].path[p].action = * ptr;
                     ptr++;
                  }
                  else
                     glb_ds1[ds1_idx].obj[o].path[p].action = 1; // default action
               }
            }
         }
      }
      editobj_make_obj_label(ds1_idx);
   }
   else
      printf("no npc paths\n");

   // sort objects, for drawing purpose
   editobj_set_drawing_order(ds1_idx);

   // internal
   glb_ds1[ds1_idx].cur_zoom = ZM_11;
   glb_ds1[ds1_idx].tile_w = 160;
   glb_ds1[ds1_idx].tile_h = 80;
   glb_ds1[ds1_idx].height_mul = 1;
   glb_ds1[ds1_idx].height_div = 1;
   glb_ds1[ds1_idx].cur_scroll.keyb.x = glb_config.scroll.keyb.x;
   glb_ds1[ds1_idx].cur_scroll.keyb.y = glb_config.scroll.keyb.y;
   glb_ds1[ds1_idx].cur_scroll.mouse.x = glb_config.scroll.mouse.x;
   glb_ds1[ds1_idx].cur_scroll.mouse.y = glb_config.scroll.mouse.y;
   
   // center it
   cx = glb_ds1[ds1_idx].width/2 + 1;
   cy = glb_ds1[ds1_idx].height/2;
   dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
   dy = (cy *  glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);

   glb_ds1[ds1_idx].own_wpreview.x0 = dx - glb_config.screen.width  / 2;
   glb_ds1[ds1_idx].own_wpreview.y0 = dy - glb_config.screen.height / 2;
   glb_ds1[ds1_idx].own_wpreview.w  = glb_config.screen.width;
   glb_ds1[ds1_idx].own_wpreview.h  = glb_config.screen.height;
   
   change_zoom(ds1_idx, ZM_14); // start with zoom of 1:4 in this ds1

   // some inits
   cptr = glb_ds1[ds1_idx].file_buff;
   tile_x = tile_y = zoom = -1;

   // workspace (user environment)
   if (glb_config.workspace_enable)
   {

      // for all filenames
      for (x=0; x<glb_ds1[ds1_idx].file_num; x++)
      {
         // for all workspace datas
         for(y=0; y < WRKSPC_MAX; y++)
         {
            // check if it's one of our datas

            // for all characters
            o = 0;
            while ( (cptr[o] != 0x00) &&
                    (glb_wrkspc_datas[y].id[o] != 0x00) &&
                    (cptr[o] == glb_wrkspc_datas[y].id[o])
                  )
            {
               // continue on next character
               o++;
            }
            if ((glb_wrkspc_datas[y].id[o] == 0x00) && (cptr[o]== '='))
            {
               // this is our data, read its value
               o++;
               if (cptr[o] != 0x00)
               {
                  switch(y)
                  {
                     case WRKSPC_TILE_X:
                        sscanf(cptr + o, "%li", & tile_x);
                        break;

                     case WRKSPC_TILE_Y:
                        sscanf(cptr + o, "%li", & tile_y);
                        break;

                     case WRKSPC_ZOOM:
                        sscanf(cptr + o, "%li", & zoom);
                        break;

                     case WRKSPC_VERSION:
                        // just ignore it
                        break;

                     case WRKSPC_SAVE_COUNT:
                        sscanf(cptr + o, "%lu", & glb_ds1[ds1_idx].save_count);
                        break;
                  }
               }
            }
         }

         // next filename
         cptr += (strlen(cptr) + 1);
      }

      // update ds1 values with workspace datas
      if (zoom != -1)
      {
         change_zoom(ds1_idx, zoom);
      }
      if ((tile_x != -1) && (tile_y != -1))
      {
         if (tile_x < 0)
            tile_x = 0;
         if (tile_x >= glb_ds1[ds1_idx].width)
            tile_x = glb_ds1[ds1_idx].width - 1;
         if (tile_y < 0)
            tile_y = 0;
         if (tile_y >= glb_ds1[ds1_idx].height)
            tile_y = glb_ds1[ds1_idx].height - 1;

         tile_x++;
         dx = (tile_y * -glb_ds1[ds1_idx].tile_w / 2) + (tile_x * glb_ds1[ds1_idx].tile_w / 2);
         dy = (tile_y *  glb_ds1[ds1_idx].tile_h / 2) + (tile_x * glb_ds1[ds1_idx].tile_h / 2);
         tile_x--;

         if (ds1_idx == 0)
         {
            glb_ds1edit.win_preview.x0 = dx - glb_ds1edit.win_preview.w / 2;
            glb_ds1edit.win_preview.y0 = dy - glb_ds1edit.win_preview.h / 2;
         }
         glb_ds1[ds1_idx].own_wpreview.x0 = dx - glb_ds1edit.win_preview.w / 2;
         glb_ds1[ds1_idx].own_wpreview.y0 = dy - glb_ds1edit.win_preview.h / 2;
         glb_ds1[ds1_idx].own_wpreview.w  = glb_ds1edit.win_preview.w;
         glb_ds1[ds1_idx].own_wpreview.h  = glb_ds1edit.win_preview.h;
      }
   }

   // end, some last init
   free(ds1_buff);
   strncpy(glb_ds1[ds1_idx].undo.tag, "UNDO", sizeof(glb_ds1[ds1_idx].undo.tag));
   glb_ds1[ds1_idx].undo.cur_buf_num = -1;
   glb_ds1[ds1_idx].path_edit_win.obj_idx = -1;
   
   return 0;
}
