/* See ds1_expect.h. */
#include <stdio.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "ds1_expect.h"

void ds1_expect_matches_spec(int idx, const FX_SPEC *s, int mode)
{
   const DS1_S *m = &glb_ds1[idx];
   int v = s->version, x, y, l, i, p;
   int walls = fx_wall_layers(s), floors = fx_floor_layers(s);
   int tag = fx_has_tag_layer(s);
   int expect_act;
   char msg[160];

   if (mode == DS1X_AS_RESAVED)
   {
      TEST_ASSERT_EQUAL_INT_MESSAGE(18, m->version, "resaved version");
      /* Below v4 the tag layer exists without a tagType; the writer only
       * emits a tag layer for tagType 1/2, so it is dropped on upgrade. */
      if (v < 4)
         tag = 0;
   }
   else
      TEST_ASSERT_EQUAL_INT_MESSAGE(v, m->version, "version");

   TEST_ASSERT_EQUAL_INT_MESSAGE(s->width, m->width, "width (+1)");
   TEST_ASSERT_EQUAL_INT_MESSAGE(s->height, m->height, "height (+1)");
   expect_act = v >= 8 ? (s->act > 5 ? 5 : s->act) : 1;
   TEST_ASSERT_EQUAL_INT_MESSAGE(expect_act, m->act, "act");
   TEST_ASSERT_EQUAL_INT_MESSAGE(v >= 10 ? s->tag_type : 0, m->tag_type, "tag_type");

   TEST_ASSERT_EQUAL_INT_MESSAGE(walls, m->wall_num, "wall layers");
   TEST_ASSERT_EQUAL_INT_MESSAGE(floors, m->floor_num, "floor layers");
   TEST_ASSERT_EQUAL_INT_MESSAGE(1, m->shadow_num, "shadow layers");
   TEST_ASSERT_EQUAL_INT_MESSAGE(tag ? 1 : 0, m->tag_num, "tag layers");

   /* filenames (no workspace entries: tests run with workspace disabled) */
   if (v >= 3)
   {
      const char *c = m->file_buff;
      TEST_ASSERT_EQUAL_INT_MESSAGE(s->file_num, m->file_num, "file count");
      for (i = 0; i < s->file_num; i++)
      {
         TEST_ASSERT_NOT_NULL(c);
         TEST_ASSERT_EQUAL_STRING_MESSAGE(s->files[i], c, "file name");
         c += strlen(c) + 1;
      }
   }
   else
      TEST_ASSERT_EQUAL_INT_MESSAGE(0, m->file_num, "no file list below v3");

   for (y = 0; y < s->height; y++)
   {
      for (x = 0; x < s->width; x++)
      {
         for (l = 0; l < walls; l++)
         {
            const CELL_W_S *w = m->wall_buff + y * m->wall_line + x * m->wall_num + l;
            /* below v7 the stored byte is remapped on read; a resaved map
             * carries the remapped value, which v18 reads as-is */
            int o = v < 7 ? fx_legacy_orientation(s->orient[l][y][x])
                          : s->orient[l][y][x];
            snprintf(msg, sizeof(msg), "wall %d at (%d,%d)", l, x, y);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->wall[l][y][x][0], w->prop1, msg);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->wall[l][y][x][1], w->prop2, msg);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->wall[l][y][x][2], w->prop3, msg);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->wall[l][y][x][3], w->prop4, msg);
            TEST_ASSERT_EQUAL_INT_MESSAGE(o, w->orientation, msg);
         }
         for (l = 0; l < floors; l++)
         {
            const CELL_F_S *f = m->floor_buff + y * m->floor_line + x * m->floor_num + l;
            snprintf(msg, sizeof(msg), "floor %d at (%d,%d)", l, x, y);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->floor[l][y][x][0], f->prop1, msg);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->floor[l][y][x][1], f->prop2, msg);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->floor[l][y][x][2], f->prop3, msg);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->floor[l][y][x][3], f->prop4, msg);
         }
         {
            const CELL_S_S *sh = m->shadow_buff + y * m->shadow_line + x * m->shadow_num;
            snprintf(msg, sizeof(msg), "shadow at (%d,%d)", x, y);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->shadow[y][x][0], sh->prop1, msg);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->shadow[y][x][1], sh->prop2, msg);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->shadow[y][x][2], sh->prop3, msg);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->shadow[y][x][3], sh->prop4, msg);
         }
         if (tag)
         {
            const CELL_T_S *t = m->tag_buff + y * m->tag_line + x * m->tag_num;
            snprintf(msg, sizeof(msg), "tag at (%d,%d)", x, y);
            TEST_ASSERT_EQUAL_HEX32_MESSAGE(s->tag[y][x], t->num, msg);
         }
      }
   }

   /* objects */
   TEST_ASSERT_EQUAL_INT_MESSAGE(v >= 2 ? s->obj_num : 0, m->obj_num, "object count");
   for (i = 0; v >= 2 && i < s->obj_num; i++)
   {
      snprintf(msg, sizeof(msg), "object %d", i);
      TEST_ASSERT_EQUAL_INT_MESSAGE(s->obj[i].type, m->obj[i].type, msg);
      TEST_ASSERT_EQUAL_INT_MESSAGE(s->obj[i].id, m->obj[i].id, msg);
      TEST_ASSERT_EQUAL_INT_MESSAGE(s->obj[i].x, m->obj[i].x, msg);
      TEST_ASSERT_EQUAL_INT_MESSAGE(s->obj[i].y, m->obj[i].y, msg);
      TEST_ASSERT_EQUAL_INT_MESSAGE(v >= 6 ? s->obj[i].flags : 0,
                                    (int32_t) m->obj[i].ds1_flags, msg);
   }

   /* groups */
   if (fx_has_groups(s))
   {
      TEST_ASSERT_EQUAL_INT_MESSAGE(s->group_num, m->group_num, "group count");
      for (i = 0; i < s->group_num; i++)
      {
         snprintf(msg, sizeof(msg), "group %d", i);
         TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t) s->group[i].tile_x, m->group[i].tile_x, msg);
         TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t) s->group[i].tile_y, m->group[i].tile_y, msg);
         TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t) s->group[i].width,  m->group[i].width, msg);
         TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t) s->group[i].height, m->group[i].height, msg);
         TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t) (v >= 13 ? s->group[i].unk : 0),
                                          m->group[i].unk, msg);
      }
   }
   else
      TEST_ASSERT_EQUAL_INT_MESSAGE(0, m->group_num, "no groups");

   /* paths: attached by matching coordinates to the object */
   for (i = 0; i < s->obj_num && v >= 2; i++)
   {
      int expect_n = 0, n;
      const FX_NPC *np = NULL;
      for (n = 0; v >= 14 && n < s->npc_num; n++)
         if (s->npc[n].x == s->obj[i].x && s->npc[n].y == s->obj[i].y)
         {
            np = &s->npc[n];
            expect_n = np->path_num;
         }
      snprintf(msg, sizeof(msg), "path count of object %d", i);
      TEST_ASSERT_EQUAL_INT_MESSAGE(expect_n, m->obj[i].path_num, msg);
      for (p = 0; p < expect_n; p++)
      {
         snprintf(msg, sizeof(msg), "object %d path point %d", i, p);
         TEST_ASSERT_EQUAL_INT_MESSAGE(np->pt[p][0], m->obj[i].path[p].x, msg);
         TEST_ASSERT_EQUAL_INT_MESSAGE(np->pt[p][1], m->obj[i].path[p].y, msg);
         TEST_ASSERT_EQUAL_INT_MESSAGE(v >= 15 ? np->pt[p][2] : 1,
                                       m->obj[i].path[p].action, msg);
      }
   }
}
