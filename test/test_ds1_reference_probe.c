/*
 * test_ds1_reference_probe -- dump what the editor's real DS1 reader made of
 * a file, as a canonical JSON snapshot (schema "ds1edit.map/1").
 *
 *   test_ds1_reference_probe dump   INPUT.ds1 --out snapshot.json
 *   test_ds1_reference_probe resave INPUT.ds1 --out OUTPUT.ds1
 *
 * `dump` calls ds1_read_checked() -- the production reader -- and writes the
 * parsed fields in a stable order: cells row by row, column by column, layer
 * by layer; objects and path blocks in source order. No pointers, view state
 * or derived UI caches are included. Fields the editor keeps without
 * interpreting (prop bytes, tag dwords, object flags, the group record's fifth
 * dword, the tileset name list) are reported raw so preservation can be
 * checked. A file the reader rejects produces {"status": "error", ...} and
 * exit code 2.
 *
 * `resave` reads INPUT with the same reader and writes it with the
 * production checked writer (ds1_save_checked) to OUTPUT, which is how the
 * writer's version-18 upgrade is exercised from scripts.
 *
 * The workspace entries are disabled, so the tileset list is the file's own.
 * The archive hook is the test stub: only the given path on disk is read.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "core/ds1.h"
#include "core/ds1_save_io.h"

#include "support/editor_test_state.h"
#include "support/test_tmp.h"

/* Unity is linked for the support helpers; nothing here uses it. */
void setUp(void) {}
void tearDown(void) {}

static void json_string(FILE *f, const char *s)
{
   fputc('"', f);
   for (; *s; s++)
   {
      unsigned char c = (unsigned char) *s;
      if (c == '"' || c == '\\')
         fprintf(f, "\\%c", c);
      else if (c < 0x20 || c >= 0x7F)
         fprintf(f, "\\u%04x", c);
      else
         fputc(c, f);
   }
   fputc('"', f);
}

static void dump_snapshot(FILE *f, int idx, const char *source)
{
   const DS1_S *m = &glb_ds1[idx];
   const char *c;
   int x, y, l, i, p, first;

   fprintf(f, "{\n  \"schema\": \"ds1edit.map/1\",\n  \"status\": \"ok\",\n  \"source\": ");
   json_string(f, source);
   fprintf(f, ",\n  \"version\": %ld,\n  \"width\": %ld,\n  \"height\": %ld,\n"
              "  \"act\": %ld,\n  \"tag_type\": %ld,\n",
           (long) m->version, (long) m->width, (long) m->height,
           (long) m->act, (long) m->tag_type);
   fprintf(f, "  \"layers\": {\"walls\": %d, \"floors\": %d, \"shadows\": %d, \"tags\": %d},\n",
           m->wall_num, m->floor_num, m->shadow_num, m->tag_num);

   fprintf(f, "  \"files\": [");
   c = m->file_buff;
   for (i = 0; c != NULL && i < m->file_num; i++)
   {
      if (i) fprintf(f, ", ");
      json_string(f, c);
      c += strlen(c) + 1;
   }
   fprintf(f, "],\n");

   fprintf(f, "  \"cells\": [");
   first = 1;
   for (y = 0; y < m->height; y++)
   {
      for (x = 0; x < m->width; x++)
      {
         fprintf(f, "%s\n    {\"x\": %d, \"y\": %d, \"walls\": [", first ? "" : ",", x, y);
         first = 0;
         for (l = 0; l < m->wall_num; l++)
         {
            const CELL_W_S *w = m->wall_buff + y * m->wall_line + x * m->wall_num + l;
            fprintf(f, "%s[%u, %u, %u, %u, %u]", l ? ", " : "",
                    w->prop1, w->prop2, w->prop3, w->prop4, w->orientation);
         }
         fprintf(f, "], \"floors\": [");
         for (l = 0; l < m->floor_num; l++)
         {
            const CELL_F_S *fl = m->floor_buff + y * m->floor_line + x * m->floor_num + l;
            fprintf(f, "%s[%u, %u, %u, %u]", l ? ", " : "",
                    fl->prop1, fl->prop2, fl->prop3, fl->prop4);
         }
         fprintf(f, "], \"shadows\": [");
         for (l = 0; l < m->shadow_num; l++)
         {
            const CELL_S_S *s = m->shadow_buff + y * m->shadow_line + x * m->shadow_num + l;
            fprintf(f, "%s[%u, %u, %u, %u]", l ? ", " : "",
                    s->prop1, s->prop2, s->prop3, s->prop4);
         }
         fprintf(f, "], \"tags\": [");
         for (l = 0; l < m->tag_num; l++)
         {
            const CELL_T_S *t = m->tag_buff + y * m->tag_line + x * m->tag_num + l;
            fprintf(f, "%s%lu", l ? ", " : "", (unsigned long) t->num);
         }
         fprintf(f, "]}");
      }
   }
   fprintf(f, "\n  ],\n");

   fprintf(f, "  \"objects\": [");
   for (i = 0; i < m->obj_num; i++)
   {
      const OBJ_S *o = &m->obj[i];
      fprintf(f, "%s\n    {\"type\": %ld, \"id\": %ld, \"x\": %ld, \"y\": %ld, \"flags\": %ld}",
              i ? "," : "", (long) o->type, (long) o->id, (long) o->x, (long) o->y,
              (long) (int32_t) o->ds1_flags);
   }
   fprintf(f, "\n  ],\n");

   fprintf(f, "  \"groups\": [");
   for (i = 0; i < m->group_num; i++)
   {
      const GROUP_S *g = &m->group[i];
      fprintf(f, "%s\n    {\"tile_x\": %lu, \"tile_y\": %lu, \"width\": %lu, \"height\": %lu, \"unk\": %lu}",
              i ? "," : "", (unsigned long) g->tile_x, (unsigned long) g->tile_y,
              (unsigned long) g->width, (unsigned long) g->height, (unsigned long) g->unk);
   }
   fprintf(f, "\n  ],\n");

   fprintf(f, "  \"paths\": [");
   first = 1;
   for (i = 0; i < m->obj_num; i++)
   {
      const OBJ_S *o = &m->obj[i];
      if (o->path_num <= 0)
         continue;
      fprintf(f, "%s\n    {\"object\": %d, \"x\": %ld, \"y\": %ld, \"points\": [",
              first ? "" : ",", i, (long) o->x, (long) o->y);
      first = 0;
      for (p = 0; p < o->path_num; p++)
         fprintf(f, "%s[%ld, %ld, %ld]", p ? ", " : "",
                 (long) o->path[p].x, (long) o->path[p].y, (long) o->path[p].action);
      fprintf(f, "]}");
   }
   fprintf(f, "\n  ]\n}\n");
}

static int usage(void)
{
   fprintf(stderr,
      "usage: test_ds1_reference_probe dump INPUT.ds1 --out snapshot.json\n"
      "       test_ds1_reference_probe resave INPUT.ds1 --out OUTPUT.ds1\n");
   return 64;
}

int main(int argc, char **argv)
{
   const char *cmd, *input, *out = NULL;
   char err[512];
   int i, rc = 0;
   FILE *f;

   if (argc < 3)
      return usage();
   cmd = argv[1];
   input = argv[2];
   for (i = 3; i + 1 < argc; i++)
      if (strcmp(argv[i], "--out") == 0)
         out = argv[++i];
   if (out == NULL || (strcmp(cmd, "dump") != 0 && strcmp(cmd, "resave") != 0))
      return usage();

   if (!editor_test_state_init(0, 1, 1))
   {
      fprintf(stderr, "probe: could not create scratch state\n");
      return 3;
   }

   if (ds1_read_checked(input, 0, 0, 0, err, sizeof(err)) != 0)
   {
      if (strcmp(cmd, "dump") == 0 && (f = fopen(out, "w")) != NULL)
      {
         fprintf(f, "{\n  \"schema\": \"ds1edit.map/1\",\n  \"status\": \"error\",\n  \"source\": ");
         json_string(f, input);
         fprintf(f, ",\n  \"error\": ");
         json_string(f, err);
         fprintf(f, "\n}\n");
         fclose(f);
      }
      fprintf(stderr, "probe: %s\n", err);
      editor_test_state_destroy();
      return 2;
   }

   if (strcmp(cmd, "dump") == 0)
   {
      f = fopen(out, "w");
      if (f == NULL)
         rc = 3;
      else
      {
         dump_snapshot(f, 0, input);
         if (fclose(f) != 0)
            rc = 3;
      }
   }
   else
   {
      /* write to the requested path: point the map's name at it first so
       * the checked writer's backup/publish policy applies there */
      snprintf(glb_ds1[0].name, sizeof(glb_ds1[0].name), "%s", out);
      if (ds1_save_checked(0, 0, err, sizeof(err)) != 0)
      {
         fprintf(stderr, "probe: %s\n", err);
         rc = 3;
      }
   }
   editor_test_state_destroy();
   return rc;
}
