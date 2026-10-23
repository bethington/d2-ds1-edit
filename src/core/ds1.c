/* Derived from win_ds1edit by Paul Siramy (originally ds1misc.c and ds1save.c).
 * See NOTICE at the repository root for attribution and license status. */

#include <string.h>
#include <stdint.h>
#include "structs.h"
#include "core/dt1.h"
#include "core/project.h"
#include "error.h"
#include "editor/objects.h"
#include "render/preview.h"
#include "core/ds1.h"
#include "core/ds1_save_io.h"
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

   // objects, the object undo copy and the drawing order (allocated by
   // ds1_read_checked and grown by misc_increase_ds1_objects_max)
   free(glb_ds1[i].obj);
   free(glb_ds1[i].obj_undo);
   free(glb_ds1[i].drawing_order);

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


/* --------------------------------------------------------------------------
 * Bounded little-endian cursor over the loaded DS1 image.
 *
 * The reader used to walk a raw int32 pointer through the buffer and checked
 * the bounds only around the group block. A file shorter than its own header,
 * an unterminated filename, a layer count above the editor's maxima, or any
 * count larger than the data behind it made it read (and for layer counts,
 * write) past the end of a buffer. Every field now comes through this cursor,
 * and every count is checked against the bytes that remain before anything is
 * allocated from it.
 *
 * Two tails are deliberately tolerant, because vanilla 1.13c ships files that
 * need it (docs/guides/ds1-map-format.md, "Reading a DS1 without falling
 * over"): the group block and the NPC-path count may run past the end of the
 * file. The game survives those because it allocates filesize + 800 bytes;
 * the editor reads the missing dwords as zero within that same window and
 * rejects anything that would need more.
 */
#define DS1_TAIL_SLACK 800

typedef struct DS1_CURSOR
{
   const unsigned char * base;
   size_t                len;
   size_t                pos;  /* may exceed len inside a tolerated tail */
} DS1_CURSOR;

static size_t ds1_cur_left(const DS1_CURSOR * c)
{
   return c->pos < c->len ? c->len - c->pos : 0;
}

static int32_t ds1_cur_peek_le(const DS1_CURSOR * c)
{
   const unsigned char * p = c->base + c->pos;
   return (int32_t) ((uint32_t) p[0] | ((uint32_t) p[1] << 8) |
                     ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24));
}

/* A required dword: 0 if fewer than 4 bytes remain. */
static int ds1_cur_i32(DS1_CURSOR * c, int32_t * out)
{
   if (ds1_cur_left(c) < 4)
      return 0;
   * out = ds1_cur_peek_le(c);
   c->pos += 4;
   return 1;
}

/* A dword of a tolerated tail: missing bytes read as zero, but only within
 * the game's own slack past the end of the file. 0 beyond it. */
static int ds1_cur_tail_i32(DS1_CURSOR * c, int32_t * out)
{
   if (ds1_cur_left(c) >= 4)
      return ds1_cur_i32(c, out);
   if (c->pos + 4 > c->len + DS1_TAIL_SLACK)
      return 0;
   * out = 0;
   if (c->pos < c->len)
   {
      /* a partial dword: keep the bytes that are there */
      unsigned char tmp[4] = { 0, 0, 0, 0 };
      memcpy(tmp, c->base + c->pos, c->len - c->pos);
      * out = (int32_t) ((uint32_t) tmp[0] | ((uint32_t) tmp[1] << 8) |
                         ((uint32_t) tmp[2] << 16) | ((uint32_t) tmp[3] << 24));
   }
   c->pos += 4;
   return 1;
}

static int ds1_cur_skip(DS1_CURSOR * c, size_t n)
{
   if (ds1_cur_left(c) < n)
      return 0;
   c->pos += n;
   return 1;
}

static void ds1_read_set_error(char * error, size_t error_cap, const char * fmt,
                               const char * ds1name, long a, long b)
{
   if (error != NULL && error_cap > 0)
   {
      char what[200];
      snprintf(what, sizeof(what), fmt, a, b);
      snprintf(error, error_cap, "can't load %s: %s", ds1name, what);
   }
}

/* Undo whatever a failed ds1_read_checked() put into the slot. Deliberately
 * not ds1_free(): that also releases the slot's dt1 references, and a map
 * that failed to load never took any (its dt1_idx[] are zero, which would
 * release dt1 slot 0 on behalf of another map). */
static void ds1_read_discard(int ds1_idx)
{
   DS1_S * d = & glb_ds1[ds1_idx];

   free(d->drawing_order);
   free(d->obj);
   free(d->obj_undo);
   free(d->file_buff);
   free(d->floor_buff);
   free(d->floor_buff2);
   free(d->shadow_buff);
   free(d->shadow_buff2);
   free(d->tag_buff);
   free(d->tag_buff2);
   free(d->wall_buff);
   free(d->wall_buff2);
   free(d->group);
   free(d->block_table);
   memset(d, 0, sizeof(DS1_S));
}

/* Load the whole file: archive overlay/MPQ chain first, then local disk.
 * 0 on success with a malloc'd buffer. */
static int ds1_read_load_image(const char * ds1name, void ** out_buff,
                               size_t * out_len, char * error, size_t error_cap)
{
   char  mpq_name[512];
   long  mpq_len = 0;
   char *mpq_buff = NULL;
   int   entry;
   FILE *in;
   long  file_len;
   void *buff;

   * out_buff = NULL;
   * out_len  = 0;

   // Resolve the map: mod_dir overlay, then the MPQ chain, then local disk.
   //
   // misc_load_mpq_file() covers the first two. The disk fallback is last so
   // that explicit paths -- the command line, assets/*.ini, and files saved
   // into a project directory -- keep behaving exactly as they did.
   //
   // Try the name exactly as given -- that covers full virtual paths and
   // anything already rooted at the tiles directory. If it misses, try it
   // again with the Data\Global\Tiles\ prefix, which is the form browser
   // entries arrive in ("ACT1\TOWN\townE1.ds1"). Trying both beats
   // guessing which one a given caller meant.
   snprintf(mpq_name, sizeof(mpq_name), "%s", ds1name);
   entry = misc_load_mpq_file(mpq_name, &mpq_buff, &mpq_len, TRUE);

   if (entry == -1 || mpq_buff == NULL)
   {
      if (mpq_buff != NULL) { free(mpq_buff); mpq_buff = NULL; }
      mpq_len = 0;
      snprintf(mpq_name, sizeof(mpq_name), "%s%s", glb_tiles_path, ds1name);
      entry = misc_load_mpq_file(mpq_name, &mpq_buff, &mpq_len, TRUE);
   }

   /* Still nothing: the name may carry a disk prefix from the days when
    * tiles had to be extracted, as the shipped area INIs do
    * ("assets/tiles/expansion/Town/townWest.ds1"). Everything after the
    * last "/tiles/" is the form the archives are keyed on. */
   if (entry == -1 || mpq_buff == NULL)
   {
      const char *suffix = find_tiles_suffix(ds1name);

      if (suffix != NULL && suffix[0] != 0)
      {
         if (mpq_buff != NULL) { free(mpq_buff); mpq_buff = NULL; }
         mpq_len = 0;
         snprintf(mpq_name, sizeof(mpq_name), "%s%s", glb_tiles_path, suffix);
         entry = misc_load_mpq_file(mpq_name, &mpq_buff, &mpq_len, TRUE);
      }
   }
   if (entry != -1 && mpq_buff != NULL && mpq_len > 0)
   {
      * out_buff = (void *) mpq_buff;
      * out_len  = (size_t) mpq_len;
      return 0;
   }
   if (mpq_buff != NULL) { free(mpq_buff); mpq_buff = NULL; }

   in = fopen(ds1name, "rb");
   if (in == NULL)
   {
      ds1_read_set_error(error, error_cap,
         "not found in the mod directory, the MPQ chain, or on disk%.0ld%.0ld",
         ds1name, 0, 0);
      return -1;
   }
   if (fseek(in, 0, SEEK_END) != 0 || (file_len = ftell(in)) < 0 ||
       fseek(in, 0, SEEK_SET) != 0)
   {
      fclose(in);
      ds1_read_set_error(error, error_cap, "can't determine the file size%.0ld%.0ld",
                         ds1name, 0, 0);
      return -1;
   }
   buff = malloc(file_len > 0 ? (size_t) file_len : 1);
   if (buff == NULL)
   {
      fclose(in);
      ds1_read_set_error(error, error_cap, "not enough mem (%li bytes)%.0ld",
                         ds1name, file_len, 0);
      return -1;
   }
   if (file_len > 0 && fread(buff, (size_t) file_len, 1, in) != 1)
   {
      fclose(in);
      free(buff);
      ds1_read_set_error(error, error_cap, "read error%.0ld%.0ld", ds1name, 0, 0);
      return -1;
   }
   fclose(in);
   * out_buff = buff;
   * out_len  = (size_t) file_len;
   return 0;
}

// ==========================================================================
// read a ds1
//    load the file into memory, then copy the datas into the right structures
//    init some var, to prepare its display (zoom, center, layer mask...)
// if new_width OR new_height is <= 0, then no change to the size of the ds1, else
// it'll crop or add necessary Tiles
//
// Returns 0 on success. On failure returns -1, writes a message into `error`,
// and leaves the slot empty -- nothing is read past the end of the file and
// nothing is allocated from a count the file cannot back.
int ds1_read_checked(const char * ds1name, int ds1_idx, int new_width, int new_height,
                     char * error, size_t error_cap)
{
   CELL_F_S    * f_ptr[FLOOR_MAX_LAYER];
   CELL_S_S    * s_ptr[SHADOW_MAX_LAYER];
   CELL_T_S    * t_ptr[TAG_MAX_LAYER];
   CELL_W_S    * w_ptr[WALL_MAX_LAYER];
   CELL_W_S    * o_ptr[WALL_MAX_LAYER];
   OBJ_LABEL_S * label;
   DS1_S       * d;
   DS1_CURSOR  cur;
   int         o, x, y, nb_layer, n, p, done, cx, cy, dx, dy,
               current_valid_obj_idx=0, max_subtile_width, max_subtile_height,
               oob_obj_num=0;
   long        w_num, f_num, s_num, t_num, npc, path;
   int32_t     v32, path_x, path_y;
   int         lay_stream[14],
               dir_lookup[25] = {
                  0x00, 0x01, 0x02, 0x01, 0x02, 0x03, 0x03, 0x05, 0x05, 0x06,
                  0x06, 0x07, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                  0x0F, 0x10, 0x11, 0x12, 0x14
               };
   const UBYTE * bptr;
   void        * ds1_buff = NULL;
   size_t      ds1_len = 0;
   int         obj_path_warn_wrote = FALSE, last_o, nb;
   char        * cptr;
   const char  * base;
   long        tile_x, tile_y, zoom;
   long        incr;
   int64_t     need, cells_new;
   size_t      rec_size, name_len;

#define DS1_FAIL(fmt, a, b) \
   do { ds1_read_set_error(error, error_cap, fmt, ds1name, (long) (a), (long) (b)); \
        goto fail; } while (0)

   if (error != NULL && error_cap > 0)
      error[0] = 0;

   if (ds1name == NULL)
   {
      if (error != NULL && error_cap > 0)
         snprintf(error, error_cap, "ds1_read: no file name");
      return -1;
   }
   if ((ds1_idx < 0) || (ds1_idx >= DS1_MAX))
   {
      ds1_read_set_error(error, error_cap, "not enough free ds1 slot (slot %li, max is %li)",
                         ds1name, ds1_idx, DS1_MAX);
      return -1;
   }
   d = & glb_ds1[ds1_idx];
   if (strlen(d->name))
   {
      ds1_read_set_error(error, error_cap, "ds1 slot %li is already used%.0ld",
                         ds1name, ds1_idx, 0);
      return -1;
   }
   name_len = strlen(ds1name);
   if (name_len == 0 || name_len >= sizeof(d->name))
   {
      ds1_read_set_error(error, error_cap, "path length %li is not in 1..%li",
                         ds1name, (long) name_len, (long) sizeof(d->name) - 1);
      return -1;
   }

   if (ds1_read_load_image(ds1name, &ds1_buff, &ds1_len, error, error_cap) != 0)
      return -1;

   // take file name without path (the short name is for display; truncate it
   // to its field rather than writing past it into the undo state)
   base = ds1name + name_len;
   while ((base > ds1name) && (base[-1] != '\\') && (base[-1] != '/') && (base[-1] != ':'))
      base--;
   snprintf(d->filename, sizeof(d->filename), "%s", base);
   memcpy(d->name, ds1name, name_len + 1);

   // initialise max number of objects
   d->current_obj_max = OBJ_MAX_START;
   d->drawing_order = (int *) calloc(d->current_obj_max, sizeof(int));
   d->obj      = (OBJ_S *) calloc(d->current_obj_max, sizeof(OBJ_S));
   d->obj_undo = (OBJ_S *) calloc(d->current_obj_max, sizeof(OBJ_S));
   if (d->drawing_order == NULL || d->obj == NULL || d->obj_undo == NULL)
      DS1_FAIL("not enough mem for %li objects%.0ld", d->current_obj_max, 0);

   // no tile set taken yet: ds1_free() releases only the slots that say so
   for (x=0; x < DT1_IN_DS1_MAX; x++)
      d->dt1_idx[x] = -1;

   // layers mask
   for (x=0; x < WALL_MAX_LAYER; x++)
      d->wall_layer_mask[x] = 1;

   for (x=0; x < FLOOR_MAX_LAYER; x++)
      d->floor_layer_mask[x] = 1;

   for (x=0; x < SHADOW_MAX_LAYER; x++)
      d->shadow_layer_mask[x] = 3; // transparent is default

   d->objects_layer_mask    = OL_NONE;
   d->paths_layer_mask      = 0;
   d->walkable_layer_mask   = 0;
   d->subtile_help_display  = 1;
   d->animations_layer_mask = 1;
   d->special_layer_mask    = 0;

   // inits
   w_num = 0; // # of wall & orientation layers
   f_num = 0; // # of floor layer
   s_num = 1; // # of shadow layer, always here
   t_num = 0; // # of tag layer
   cur.base = (const unsigned char *) ds1_buff;
   cur.len  = ds1_len;
   cur.pos  = 0;

   // copy datas from buffer into ds1 struct :

   // version, width, height
   if (! ds1_cur_i32(&cur, &v32))
      DS1_FAIL("file is %li bytes, too short for a header%.0ld", ds1_len, 0);
   d->version = v32;
   if (! ds1_cur_i32(&cur, &v32))
      DS1_FAIL("file is %li bytes, too short for a header%.0ld", ds1_len, 0);
   if (v32 < 0 || v32 >= 0x7FFFFFFF)
      DS1_FAIL("invalid width %li%.0ld", (long) v32 + 1, 0);
   d->width = (long) v32 + 1;
   if (! ds1_cur_i32(&cur, &v32))
      DS1_FAIL("file is %li bytes, too short for a header%.0ld", ds1_len, 0);
   if (v32 < 0 || v32 >= 0x7FFFFFFF)
      DS1_FAIL("invalid height %li%.0ld", (long) v32 + 1, 0);
   d->height = (long) v32 + 1;

   // will we use new size ?
   if ((new_width <= 0) || (new_height <= 0))
   {
      new_width  = d->width;
      new_height = d->height;
   }

   // act
   d->act = 1;
   if (d->version >= 8)
   {
      if (! ds1_cur_i32(&cur, &v32))
         DS1_FAIL("header ends before the act field%.0ld%.0ld", 0, 0);
      d->act = (long) v32 + 1;
      if (d->act > 5)
         d->act = 5;
   }

   // tag_type
   d->tag_type = 0;
   if (d->version >= 10)
   {
      if (! ds1_cur_i32(&cur, &v32))
         DS1_FAIL("header ends before the tag type field%.0ld%.0ld", 0, 0);
      d->tag_type = v32;

      // adjust eventually the # of tag layer
      if ((d->tag_type == 1) || (d->tag_type == 2))
         t_num = 1;
   }

   // debug
   printf("\nversion         : %li\n", d->version);
   printf("width           : %li\n", d->width);
   printf("height          : %li\n", d->height);
   printf("act             : %li\n", d->act);
   printf("tag_type        : %li\n", d->tag_type);

   // filenames
   d->file_num = 0;
   if (d->version >= 3)
   {
      const char * names;
      size_t       left, total = 0, len;

      if (! ds1_cur_i32(&cur, &v32))
         DS1_FAIL("header ends before the filename count%.0ld%.0ld", 0, 0);
      if (v32 < 0)
         DS1_FAIL("invalid filename count %li%.0ld", v32, 0);
      d->file_num = v32;
      printf("filenames       : %li\n", d->file_num);

      names = (const char *) cur.base + cur.pos;
      left  = ds1_cur_left(&cur);
      for (x=0; x<d->file_num; x++)
      {
         const char * nul = (const char *) memchr(names + total, 0, left - total);
         if (nul == NULL)
            DS1_FAIL("filename %li of %li is not terminated inside the file", x + 1, d->file_num);
         len = (size_t) (nul - (names + total)) + 1;
         printf("   %2i : %s\n", x + 1, names + total);
         total += len;
      }

      d->file_buff = (char *) malloc(total > 0 ? total : 1);
      if (d->file_buff == NULL)
         DS1_FAIL("not enough mem (%li bytes) for files in the ds1%.0ld", (long) total, 0);
      d->file_len = (int) total;
      memcpy(d->file_buff, names, total);
      cur.pos += total;
   }
   else
      printf("no filenames\n");

   // skip 2 dwords ?
   if ((d->version >= 9) && (d->version <= 13))
   {
      if (! ds1_cur_skip(&cur, 8))
         DS1_FAIL("header ends inside the v9-13 reserved dwords%.0ld%.0ld", 0, 0);
   }

   // number of wall, floor and tag layers
   if (d->version >= 4)
   {
      // number of wall (and orientation) layers
      if (! ds1_cur_i32(&cur, &v32))
         DS1_FAIL("header ends before the wall layer count%.0ld%.0ld", 0, 0);
      if (v32 < 0 || v32 > WALL_MAX_LAYER)
         DS1_FAIL("wall layer count %li is outside 0..%li", v32, WALL_MAX_LAYER);
      w_num = v32;

      // number of floor layers
      if (d->version >= 16)
      {
         if (! ds1_cur_i32(&cur, &v32))
            DS1_FAIL("header ends before the floor layer count%.0ld%.0ld", 0, 0);
         if (v32 < 0 || v32 > FLOOR_MAX_LAYER)
            DS1_FAIL("floor layer count %li is outside 0..%li", v32, FLOOR_MAX_LAYER);
         f_num = v32;
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
   if (d->version < 4)
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

   // the layers must all be in the file before anything is sized from them
   need = (int64_t) nb_layer * (int64_t) d->width * (int64_t) d->height * 4;
   if (need > (int64_t) ds1_cur_left(&cur))
      DS1_FAIL("%li x %li cells need more layer data than the file holds%.0ld",
               d->width, d->height);

   // and the in-memory map must be addressable with the editor's int sizes
   cells_new = (int64_t) new_width * (int64_t) new_height;
   if (cells_new * WALL_MAX_LAYER * (int64_t) sizeof(CELL_W_S) > 0x7FFFFFFF)
      DS1_FAIL("map size %li x %li is too large", new_width, new_height);

   // layers num
   if (glb_config.always_max_layers)
   {
      f_num = FLOOR_MAX_LAYER;
      w_num = WALL_MAX_LAYER;
   }
   d->floor_num  = f_num;
   d->shadow_num = s_num;
   d->tag_num    = t_num;
   d->wall_num   = w_num;

   // floor buffer
   d->floor_line     = new_width * d->floor_num;
   d->floor_len      = d->floor_line * new_height;
   d->floor_buff_len = d->floor_len * sizeof(CELL_F_S);
   d->floor_buff     = (CELL_F_S *) calloc(1, d->floor_buff_len > 0 ? d->floor_buff_len : 1);
   d->floor_buff2    = (CELL_F_S *) calloc(1, d->floor_buff_len > 0 ? d->floor_buff_len : 1);
   if (d->floor_buff == NULL || d->floor_buff2 == NULL)
      DS1_FAIL("not enough mem (%li bytes) for floor buffers%.0ld", d->floor_buff_len, 0);

   // shadow buffer
   d->shadow_line     = new_width * d->shadow_num;
   d->shadow_len      = d->shadow_line * new_height;
   d->shadow_buff_len = d->shadow_len * sizeof(CELL_S_S);
   d->shadow_buff     = (CELL_S_S *) calloc(1, d->shadow_buff_len > 0 ? d->shadow_buff_len : 1);
   d->shadow_buff2    = (CELL_S_S *) calloc(1, d->shadow_buff_len > 0 ? d->shadow_buff_len : 1);
   if (d->shadow_buff == NULL || d->shadow_buff2 == NULL)
      DS1_FAIL("not enough mem (%li bytes) for shadow buffers%.0ld", d->shadow_buff_len, 0);

   // tag buffer
   d->tag_line     = new_width * d->tag_num;
   d->tag_len      = d->tag_line * new_height;
   d->tag_buff_len = d->tag_len * sizeof(CELL_T_S);
   d->tag_buff     = (CELL_T_S *) calloc(1, d->tag_buff_len > 0 ? d->tag_buff_len : 1);
   d->tag_buff2    = (CELL_T_S *) calloc(1, d->tag_buff_len > 0 ? d->tag_buff_len : 1);
   if (d->tag_buff == NULL || d->tag_buff2 == NULL)
      DS1_FAIL("not enough mem (%li bytes) for tag buffers%.0ld", d->tag_buff_len, 0);

   // wall buffer
   d->wall_line     = new_width * d->wall_num;
   d->wall_len      = d->wall_line * new_height;
   d->wall_buff_len = d->wall_len * sizeof(CELL_W_S);
   d->wall_buff     = (CELL_W_S *) calloc(1, d->wall_buff_len > 0 ? d->wall_buff_len : 1);
   d->wall_buff2    = (CELL_W_S *) calloc(1, d->wall_buff_len > 0 ? d->wall_buff_len : 1);
   if (d->wall_buff == NULL || d->wall_buff2 == NULL)
      DS1_FAIL("not enough mem (%li bytes) for wall buffers%.0ld", d->wall_buff_len, 0);

   // read tiles of layers

   // set pointers
   for (x=0; x<FLOOR_MAX_LAYER; x++)
      f_ptr[x] = d->floor_buff + x;

   for (x=0; x<SHADOW_MAX_LAYER; x++)
      s_ptr[x] = d->shadow_buff + x;

   for (x=0; x<TAG_MAX_LAYER; x++)
      t_ptr[x] = d->tag_buff + x;

   for (x=0; x<WALL_MAX_LAYER; x++)
      o_ptr[x] = w_ptr[x] = d->wall_buff + x;

   bptr = cur.base + cur.pos;

   for (n=0; n < nb_layer; n++)
   {
      for (y=0; y < d->height; y++)
      {
         for (x=0; x < d->width; x++)
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
                     w_ptr[p]->prop1 = bptr[0];
                     w_ptr[p]->prop2 = bptr[1];
                     w_ptr[p]->prop3 = bptr[2];
                     w_ptr[p]->prop4 = bptr[3];
                     w_ptr[p] += w_num;
                  }
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
                     if (d->version < 7)
                        o_ptr[p]->orientation = (* bptr < 25) ? dir_lookup[* bptr] : * bptr;
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
                     f_ptr[p]->prop1 = bptr[0];
                     f_ptr[p]->prop2 = bptr[1];
                     f_ptr[p]->prop3 = bptr[2];
                     f_ptr[p]->prop4 = bptr[3];
                     f_ptr[p] += f_num;
                  }
                  bptr += 4;
                  break;

               // shadow
               case 11:
                  if ((x < new_width) && (y < new_height))
                  {
                     p = lay_stream[n] - 11;
                     s_ptr[p]->prop1 = bptr[0];
                     s_ptr[p]->prop2 = bptr[1];
                     s_ptr[p]->prop3 = bptr[2];
                     s_ptr[p]->prop4 = bptr[3];
                     s_ptr[p] += s_num;
                  }
                  bptr += 4;
                  break;

               // tag
               case 12:
                  if ((x < new_width) && (y < new_height))
                  {
                     p = lay_stream[n] - 12;
                     t_ptr[p]->num = (UDWORD) bptr[0] | ((UDWORD) bptr[1] << 8) |
                                     ((UDWORD) bptr[2] << 16) | ((UDWORD) bptr[3] << 24);
                     t_ptr[p] += t_num;
                  }
                  bptr += 4;
                  break;
            }
         }

         // in case of bigger width
         p = new_width - d->width;
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
   cur.pos = (size_t) (bptr - cur.base);

   // update new size of the ds1
   d->width  = new_width;
   d->height = new_height;

   //now we're on the objects data
   d->obj_num = 0;
   if (d->version >= 2)
   {
      if (! ds1_cur_i32(&cur, &v32))
         DS1_FAIL("file ends before the object count%.0ld%.0ld", 0, 0);
      if (v32 < 0)
         DS1_FAIL("invalid object count %li%.0ld", v32, 0);
      rec_size = (d->version > 5) ? 20 : 16;
      if ((int64_t) v32 * (int64_t) rec_size > (int64_t) ds1_cur_left(&cur))
         DS1_FAIL("%li objects need more data than the file holds (%li bytes left)",
                  v32, (long) ds1_cur_left(&cur));
      d->obj_num = v32;

      printf("objects         : %li\n", d->obj_num);

      if (d->obj_num > d->current_obj_max)
      {
         incr = 1 + d->obj_num - d->current_obj_max;
         if (misc_increase_ds1_objects_max(ds1_idx, incr) != 0)
            DS1_FAIL("too many objects (%li), editor max is <%li>",
                     d->obj_num, d->current_obj_max);
      }
      current_valid_obj_idx = 0;
      max_subtile_width     = new_width * 5;
      max_subtile_height    = new_height * 5;
      for (n=0; n < d->obj_num; n++)
      {
         OBJ_S * ob = & d->obj[current_valid_obj_idx];

         ds1_cur_i32(&cur, &v32); ob->type = v32;
         ds1_cur_i32(&cur, &v32); ob->id   = v32;
         ds1_cur_i32(&cur, &v32); x = ob->x = v32;
         ds1_cur_i32(&cur, &v32); y = ob->y = v32;

         if (d->version > 5)
         {
            // flags
            ds1_cur_i32(&cur, &v32);
            ob->ds1_flags = v32;
         }

         // The game does not bounds-check object positions (D2Common 1.13c
         // keeps them; 80 vanilla objects in 38 maps lie outside the map),
         // so neither do we: they are loaded, shown and saved unchanged.
         if ((x < 0) || (x >= max_subtile_width) || (y < 0) || (y >= max_subtile_height))
            oob_obj_num++;
         {
            // some init for the paths of this object
            ob->path_num = 0;
            ob->desc_idx = -1;
            ob->flags    = 0;

            ob->frame_delta = rand()%256;

            label = & ob->label;
            label->rx = label->ry = label->w = label->h = label->flags = 0;

            editobj_make_obj_desc(ds1_idx, current_valid_obj_idx);
            current_valid_obj_idx++;
         }
      }
      if (oob_obj_num > 0)
      {
         printf("WARNING : %li object(s) outside the map bounds (kept, drawn with a warning tint)\n",
                (long) oob_obj_num);
         fprintf(stderr, "%li object(s) outside the map bounds\n", (long) oob_obj_num);
      }
   }
   else
      printf("no objects\n");

   // update the new number of objects in that ds1
   d->obj_num = current_valid_obj_idx;

    /* groups for tag layer

      warning : in fact there can be less groups than expected
      like in data\global\tiles\act1\outdoors\trees.ds1, where the file
      stop right after the last tile_x group data, leaving the other
      datas unknown (tile_y, width, height), and npc paths unknown.
      Those missing dwords read as zero (see DS1_TAIL_SLACK). */

   if ( (d->version >= 12)  &&
        ((d->tag_type == 1) || (d->tag_type == 2))
      )
   {
      // skip 1 dword ?
      if (d->version >= 18)
      {
         if (! ds1_cur_tail_i32(&cur, &v32))
            DS1_FAIL("group block starts past the end of the file%.0ld%.0ld", 0, 0);
      }

      if (! ds1_cur_tail_i32(&cur, &v32))
         DS1_FAIL("group block starts past the end of the file%.0ld%.0ld", 0, 0);
      if (v32 < 0)
         DS1_FAIL("invalid group count %li%.0ld", v32, 0);
      rec_size = (d->version >= 13) ? 20 : 16;
      if ((int64_t) v32 * (int64_t) rec_size >
          (int64_t) ds1_cur_left(&cur) + DS1_TAIL_SLACK)
         DS1_FAIL("%li groups need more data than the file holds (%li bytes left)",
                  v32, (long) ds1_cur_left(&cur));
      d->group_num = n = v32;

      printf("groups          : %li\n", (long) n);

      // malloc
      d->group_size = n * sizeof(GROUP_S);
      d->group = (GROUP_S *) calloc(n > 0 ? n : 1, sizeof(GROUP_S));
      if (d->group == NULL)
         DS1_FAIL("not enough mem (%li bytes) for groups%.0ld", d->group_size, 0);

      // fill it (dwords past the end of the file read as zero)
      for (x=0; x<n; x++)
      {
         ds1_cur_tail_i32(&cur, &v32); d->group[x].tile_x = (UDWORD) v32;
         ds1_cur_tail_i32(&cur, &v32); d->group[x].tile_y = (UDWORD) v32;
         ds1_cur_tail_i32(&cur, &v32); d->group[x].width  = (UDWORD) v32;
         ds1_cur_tail_i32(&cur, &v32); d->group[x].height = (UDWORD) v32;
         if (d->version >= 13)
         {
            ds1_cur_tail_i32(&cur, &v32);
            d->group[x].unk = (UDWORD) v32;
         }
      }
   }
   else
      printf("no groups\n");

   // now we're on the npc's paths datas
   if (d->version >= 14)
   {
      // a missing count (the file stops before it) means no paths
      if (ds1_cur_left(&cur) >= 4)
      {
         ds1_cur_i32(&cur, &v32);
         npc = v32;
      }
      else
         npc = 0;
      if (npc < 0)
         DS1_FAIL("invalid npc path count %li%.0ld", npc, 0);
      printf("npc paths       : %li\n", npc);
      rec_size = (d->version >= 15) ? 12 : 8;
      for (n=0; n<npc; n++)
      {
         if (! ds1_cur_i32(&cur, &v32))
            DS1_FAIL("path block %li of %li is cut short", n + 1, npc);
         path = v32;
         if (! ds1_cur_i32(&cur, &path_x) || ! ds1_cur_i32(&cur, &path_y))
            DS1_FAIL("path block %li of %li is cut short", n + 1, npc);
         x = path_x;
         y = path_y;
         if (path < 0)
            DS1_FAIL("path block %li has an invalid point count %li", n + 1, path);
         if ((int64_t) path * (int64_t) rec_size > (int64_t) ds1_cur_left(&cur))
            DS1_FAIL("path block %li needs %li points the file does not hold", n + 1, path);

         // search of which object are these paths datas
         o = last_o = nb = 0;
         done = FALSE;
         while (! done)
         {
            if (o < d->obj_num)
            {
               if ((d->obj[o].x == x) && (d->obj[o].y == y))
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
            printf("   * Removing %li paths points of 1 object at coordinates (%li, %li)\n",
               (long) path, (long) x, (long) y);
            fflush(stdout);

            // first, delete already assigned paths
            for (o=0; o < d->obj_num; o++)
            {
               if ((d->obj[o].x == x) && (d->obj[o].y == y) &&
                   (d->obj[o].path_num != 0))
               {
                  for (p=0; p < d->obj[o].path_num; p++)
                  {
                     d->obj[o].path[p].x      = 0;
                     d->obj[o].path[p].y      = 0;
                     d->obj[o].path[p].action = 0;
                     d->obj[o].path[p].flags  = 0;
                  }
                  d->obj[o].path_num = 0;
               }
            }

            // now, skip these paths
            cur.pos += (size_t) path * rec_size;
         }
         else if (nb == 0)
         {
            // these paths are not pointing to a valid object position
            // the game don't alert the user, so why me ?
            // but we'll skip them
            cur.pos += (size_t) path * rec_size;
         }
         else
         {
            // only 1 object at these coordinates for paths, it's ok
            o = last_o;

            if (path > WINDS1EDIT_PATH_MAX)
               DS1_FAIL("object %li has %li path points, more than the editor's "
                        "WINDS1EDIT_PATH_MAX", o, path);

            // all ok for assigning the paths to this object
            d->obj[o].path_num = path;
            for (p=0; p < path; p++)
            {
               ds1_cur_i32(&cur, &v32); d->obj[o].path[p].x = v32;
               ds1_cur_i32(&cur, &v32); d->obj[o].path[p].y = v32;
               if (d->version >= 15)
               {
                  ds1_cur_i32(&cur, &v32);
                  d->obj[o].path[p].action = v32;
               }
               else
                  d->obj[o].path[p].action = 1; // default action
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
   d->cur_zoom = ZM_11;
   d->tile_w = 160;
   d->tile_h = 80;
   d->height_mul = 1;
   d->height_div = 1;
   d->cur_scroll.keyb.x = glb_config.scroll.keyb.x;
   d->cur_scroll.keyb.y = glb_config.scroll.keyb.y;
   d->cur_scroll.mouse.x = glb_config.scroll.mouse.x;
   d->cur_scroll.mouse.y = glb_config.scroll.mouse.y;

   // center it
   cx = d->width/2 + 1;
   cy = d->height/2;
   dx = (cy * -d->tile_w / 2) + (cx * d->tile_w / 2);
   dy = (cy *  d->tile_h / 2) + (cx * d->tile_h / 2);

   d->own_wpreview.x0 = dx - glb_config.screen.width  / 2;
   d->own_wpreview.y0 = dy - glb_config.screen.height / 2;
   d->own_wpreview.w  = glb_config.screen.width;
   d->own_wpreview.h  = glb_config.screen.height;

   change_zoom(ds1_idx, glb_config.default_zoom >= 0 ? glb_config.default_zoom : ZM_11);

   // some inits
   cptr = d->file_buff;
   tile_x = tile_y = zoom = -1;

   // workspace (user environment)
   if (glb_config.workspace_enable && cptr != NULL)
   {

      // for all filenames
      for (x=0; x<d->file_num; x++)
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
                        {
                           /* save_count is UDWORD (32-bit). "%lu" makes sscanf
                              store an unsigned long, which is 8 bytes here --
                              writing 4 bytes past the field into whatever
                              follows it in the struct. Scan into a local of
                              the width the format actually promises, then
                              narrow deliberately. */
                           unsigned long sc = 0;
                           sscanf(cptr + o, "%lu", & sc);
                           d->save_count = (UDWORD) sc;
                        }
                        break;
                  }
               }
            }
         }

         // next filename
         cptr += (strlen(cptr) + 1);
      }

      // update ds1 values with workspace datas
      // (zoom ignored — always default to 1:1)
      (void)zoom;
      if ((tile_x != -1) && (tile_y != -1))
      {
         if (tile_x < 0)
            tile_x = 0;
         if (tile_x >= d->width)
            tile_x = d->width - 1;
         if (tile_y < 0)
            tile_y = 0;
         if (tile_y >= d->height)
            tile_y = d->height - 1;

         tile_x++;
         dx = (tile_y * -d->tile_w / 2) + (tile_x * d->tile_w / 2);
         dy = (tile_y *  d->tile_h / 2) + (tile_x * d->tile_h / 2);
         tile_x--;

         if (ds1_idx == 0)
         {
            glb_ds1edit.win_preview.x0 = dx - glb_ds1edit.win_preview.w / 2;
            glb_ds1edit.win_preview.y0 = dy - glb_ds1edit.win_preview.h / 2;
         }
         d->own_wpreview.x0 = dx - glb_ds1edit.win_preview.w / 2;
         d->own_wpreview.y0 = dy - glb_ds1edit.win_preview.h / 2;
         d->own_wpreview.w  = glb_ds1edit.win_preview.w;
         d->own_wpreview.h  = glb_ds1edit.win_preview.h;
      }
   }

   // end, some last init
   free(ds1_buff);
   memcpy(d->undo.tag, "UNDO", sizeof(d->undo.tag));
   d->undo.cur_buf_num = -1;
   d->path_edit_win.obj_idx = -1;

   return 0;

fail:
   free(ds1_buff);
   ds1_read_discard(ds1_idx);
   return -1;
#undef DS1_FAIL
}


// ==========================================================================
// read a ds1, ending the program with a message if it can't be loaded
// (the historical contract every caller relies on)
int ds1_read(const char * ds1name, int ds1_idx, int new_width, int new_height)
{
   char error[512];

   if (ds1_read_checked(ds1name, ds1_idx, new_width, new_height,
                        error, sizeof(error)) != 0)
      ds1edit_error(error);
   return 0;
}


// ==========================================================================
// as expected, save the ds1
//    if is_tmp_file == TRUE, will save a file "*.tmp", else "*.ds1" with the
//    previous file kept as "*-nnn.ds1"
//
// Compatibility wrapper over ds1_save_checked() (core/ds1_save_io.c). It used
// to end the program when a save could not be written and to say nothing at
// all when a write or the close failed; now it reports the failure on the
// console and returns -1 so a caller can keep the map open and tell the user.
// Callers that present the result should use ds1_save_checked() directly.
int ds1_save(int ds1_idx, int is_tmp_file)
{
   char error[768];

   if (ds1_save_checked(ds1_idx, is_tmp_file, error, sizeof(error)) != 0)
   {
      printf("\nds1_save() : %s\n", error);
      fflush(stdout);
      return -1;
   }
   return 0;
}
