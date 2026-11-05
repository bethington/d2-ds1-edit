#include <ctype.h>
#include <string.h>

#include "structs.h"
#include "core/map_state.h"

static int slot_has_map(int ds1_idx)
{
   return ds1_idx >= 0 && ds1_idx < DS1_MAX && glb_ds1[ds1_idx].name[0] != 0;
}

void map_dirty_mark(int ds1_idx)
{
   if (slot_has_map(ds1_idx))
      glb_ds1[ds1_idx].other_dirty = 1;
}

// FNV-1a over the tile fields ds1_serialize writes, field by field. Flags and
// block indexes are display state, not saved content.
static unsigned long long fnv8(unsigned long long h, unsigned long v)
{
   h ^= v & 0xFF;
   h *= 1099511628211ULL;
   return h;
}

// counts, sizes and tag words need all four bytes
static unsigned long long fnv(unsigned long long h, unsigned long v)
{
   int i;

   for (i = 0; i < 4; i++)
      h = fnv8(h, v >> (8 * i));
   return h;
}

static unsigned long long hash_tiles(const DS1_S *d)
{
   unsigned long long h = 14695981039346656037ULL;
   long x, y, i, t;

   h = fnv(h, (unsigned long) d->width);
   h = fnv(h, (unsigned long) d->height);
   h = fnv(h, (unsigned long) d->wall_num);
   h = fnv(h, (unsigned long) d->floor_num);
   h = fnv(h, (unsigned long) d->shadow_num);
   h = fnv(h, (unsigned long) d->tag_num);

   for (y = 0; y < d->height; y++)
      for (x = 0; x < d->width; x++)
      {
         if (d->wall_buff != NULL)
            for (i = 0; i < d->wall_num; i++)
            {
               const CELL_W_S *w;
               t = (y * d->wall_line) + (x * d->wall_num);
               w = d->wall_buff + t + i;
               h = fnv8(h, (unsigned long) w->prop1);
               h = fnv8(h, (unsigned long) w->prop2);
               h = fnv8(h, (unsigned long) w->prop3);
               h = fnv8(h, (unsigned long) w->prop4);
               h = fnv8(h, (unsigned long) w->orientation);
            }
         if (d->floor_buff != NULL)
            for (i = 0; i < d->floor_num; i++)
            {
               const CELL_F_S *f;
               t = (y * d->floor_line) + (x * d->floor_num);
               f = d->floor_buff + t + i;
               h = fnv8(h, (unsigned long) f->prop1);
               h = fnv8(h, (unsigned long) f->prop2);
               h = fnv8(h, (unsigned long) f->prop3);
               h = fnv8(h, (unsigned long) f->prop4);
            }
         if (d->shadow_buff != NULL)
            for (i = 0; i < d->shadow_num; i++)
            {
               const CELL_S_S *s;
               t = (y * d->shadow_line) + (x * d->shadow_num);
               s = d->shadow_buff + t + i;
               h = fnv8(h, (unsigned long) s->prop1);
               h = fnv8(h, (unsigned long) s->prop2);
               h = fnv8(h, (unsigned long) s->prop3);
               h = fnv8(h, (unsigned long) s->prop4);
            }
         if (d->tag_type && d->tag_buff != NULL)
            for (i = 0; i < d->tag_num; i++)
            {
               t = (y * d->tag_line) + (x * d->tag_num);
               h = fnv(h, (unsigned long) (d->tag_buff + t + i)->num);
            }
      }
   return h;
}

void map_dirty_before_tile_edit(int ds1_idx)
{
   DS1_S *d;

   if (!slot_has_map(ds1_idx)) return;
   d = &glb_ds1[ds1_idx];

   // Nothing has changed since load or save before the first edit, so the
   // map as it is now is the baseline.
   if (!d->tile_hash_valid)
   {
      d->saved_tile_hash = d->current_tile_hash = hash_tiles(d);
      d->tile_hash_valid = 1;
   }
}

void map_dirty_after_tile_change(int ds1_idx)
{
   DS1_S *d;

   if (!slot_has_map(ds1_idx)) return;
   d = &glb_ds1[ds1_idx];
   if (d->tile_hash_valid)
      d->current_tile_hash = hash_tiles(d);
}

void map_dirty_before_object_edit(int ds1_idx)
{
   DS1_S *d;

   if (!slot_has_map(ds1_idx)) return;
   d = &glb_ds1[ds1_idx];
   d->undo_object_dirty = d->object_dirty;
   d->object_dirty = 1;
}

void map_dirty_after_object_undo(int ds1_idx)
{
   if (slot_has_map(ds1_idx))
      glb_ds1[ds1_idx].object_dirty = glb_ds1[ds1_idx].undo_object_dirty;
}

void map_dirty_mark_saved(int ds1_idx)
{
   DS1_S *d;

   if (!slot_has_map(ds1_idx)) return;
   d = &glb_ds1[ds1_idx];
   d->other_dirty = 0;
   d->object_dirty = 0;
   d->undo_object_dirty = 1;   // the object undo backup now differs from disk
   d->saved_tile_hash = d->current_tile_hash = hash_tiles(d);
   d->tile_hash_valid = 1;
}

int map_is_dirty(int ds1_idx)
{
   const DS1_S *d;

   if (!slot_has_map(ds1_idx)) return 0;
   d = &glb_ds1[ds1_idx];
   return d->other_dirty || d->object_dirty ||
          (d->tile_hash_valid && d->current_tile_hash != d->saved_tile_hash);
}

int map_any_dirty(void)
{
   int i;

   for (i = 0; i < DS1_MAX; i++)
      if (map_is_dirty(i))
         return 1;
   return 0;
}

MAP_STATE_E map_state_classify(int in_game, int in_project, int slot_cleared)
{
   if (in_game && slot_cleared) return MAP_STATE_REMOVED;
   if (!in_project)             return MAP_STATE_GAME;
   return in_game ? MAP_STATE_MODIFIED : MAP_STATE_NEW;
}

const char *map_state_mark(MAP_STATE_E state)
{
   switch (state)
   {
   case MAP_STATE_MODIFIED: return "M";
   case MAP_STATE_NEW:      return "N";
   default:                 return "";
   }
}

static int same_path_char(char a, char b)
{
   if (a == '\\') a = '/';
   if (b == '\\') b = '/';
   return tolower((unsigned char) a) == tolower((unsigned char) b);
}

int map_state_path_matches(const char *map_name, const char *ds1_path)
{
   size_t nlen = strlen(map_name), plen = strlen(ds1_path), i;
   const char *tail;

   if (plen == 0 || nlen < plen) return 0;
   tail = map_name + (nlen - plen);
   for (i = 0; i < plen; i++)
      if (!same_path_char(tail[i], ds1_path[i]))
         return 0;
   return tail == map_name || tail[-1] == '/' || tail[-1] == '\\';
}
