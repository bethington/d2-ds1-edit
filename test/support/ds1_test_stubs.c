/* Hooks the DS1 / editing code calls into modules that map-integrity tests
 * do not link (misc.c, render/preview.c, core/dt1.c).
 *
 * None of these is under test here, and none of them decides what a map
 * contains:
 *   - misc_load_mpq_file: archive/overlay lookup. Answering "not found" makes
 *     ds1_read() take its documented local-disk fallback, so every authored
 *     fixture is read from the owned temp directory and no game install,
 *     MPQ or mod directory is ever consulted.
 *   - change_zoom / coord_to_tile: view-port bookkeeping for drawing.
 *   - dt1_del: releases tile graphics; test maps load none.
 *   - misc_increase_ds1_objects_max: grows the object arrays. misc.c drags in
 *     the whole application, so the growth step is reproduced here with the
 *     same contract (append OBJ_MAX_GRANULARITY-sized chunks, keep contents,
 *     zero the new tail, return 0 on success / -1 on allocation failure).
 */
#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "misc.h"
#include "core/dt1.h"
#include "render/preview.h"
#include "core/cof.h"

int misc_load_mpq_file(char *filename, char **buffer, long *buf_len, int output)
{
   (void) filename;
   (void) output;
   if (buffer != NULL)
      *buffer = NULL;
   if (buf_len != NULL)
      *buf_len = 0;
   return -1;
}

void change_zoom(int ds1_idx, ZOOM_E z)
{
   glb_ds1[ds1_idx].cur_zoom = z;
}

void coord_to_tile(int ds1_idx, int ax, int ay, int *layer_x, int *layer_y)
{
   (void) ds1_idx;
   (void) ax;
   (void) ay;
   *layer_x = 0;
   *layer_y = 0;
}

int dt1_del(int i)
{
   (void) i;
   return 0;
}

static void *grow(void *old, size_t old_size, size_t new_size)
{
   void *p = malloc(new_size);
   if (p == NULL)
      return NULL;
   memset(p, 0, new_size);
   if (old != NULL)
      memcpy(p, old, old_size);
   free(old);
   return p;
}

int misc_increase_ds1_objects_max(int ds1_idx, long nb_objects)
{
   DS1_S *m = &glb_ds1[ds1_idx];
   long old_max = m->current_obj_max;
   long new_max;
   void *p;

   if (nb_objects < 0)
      return 0;
   new_max = old_max + (1 + nb_objects / OBJ_MAX_GRANULARITY) * OBJ_MAX_GRANULARITY;

   p = grow(m->drawing_order, (size_t) old_max * sizeof(int), (size_t) new_max * sizeof(int));
   if (p == NULL) return -1;
   m->drawing_order = (int *) p;
   p = grow(m->obj, (size_t) old_max * sizeof(OBJ_S), (size_t) new_max * sizeof(OBJ_S));
   if (p == NULL) return -1;
   m->obj = (OBJ_S *) p;
   p = grow(m->obj_undo, (size_t) old_max * sizeof(OBJ_S), (size_t) new_max * sizeof(OBJ_S));
   if (p == NULL) return -1;
   m->obj_undo = (OBJ_S *) p;
   m->current_obj_max = new_max;
   return 0;
}

/* Drawing-side hooks reached only from interactive code paths
 * (editobj_handler / editobj_draw_edit_obj) that these tests do not drive. */
void ds1edit_recreate_render_targets(void) { }
void ds1edit_restore_gpu_bitmaps(void) { }

void anim_update_gfx(int progress)
{
   (void) progress;
}

int misc_get_txt_column_num(RQ_ENUM txt_idx, char *col_name)
{
   (void) txt_idx;
   (void) col_name;
   return -1;
}
