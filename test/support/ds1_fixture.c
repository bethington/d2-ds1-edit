/* Independent DS1 encoder for authored fixtures. See ds1_fixture.h.
 *
 * The layout follows docs/guides/ds1-map-format.md ("Version differences"):
 *   act dword v>=8, tagType v>=10, filenames v>=3, skipped pair v9..13,
 *   wall count v>=4, floor count v>=16, five-block legacy order below v4,
 *   orientation remap below v7, object flags v>=6, groups v>=12 with tagType
 *   1/2 (fifth dword v>=13, leading dword v>=18), NPC paths v>=14 (action
 *   v>=15). Everything is written little-endian, byte by byte.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ds1_fixture.h"

typedef struct FX_BUF
{
   unsigned char *p;
   size_t len, cap;
} FX_BUF;

static void put_bytes(FX_BUF *b, const void *src, size_t n)
{
   if (b->len + n > b->cap)
   {
      size_t cap = b->cap ? b->cap : 256;
      while (cap < b->len + n)
         cap *= 2;
      b->p = (unsigned char *) realloc(b->p, cap);
      b->cap = cap;
   }
   memcpy(b->p + b->len, src, n);
   b->len += n;
}

static void put_u8(FX_BUF *b, unsigned v)
{
   unsigned char c = (unsigned char) v;
   put_bytes(b, &c, 1);
}

static void put_i32(FX_BUF *b, int32_t v)
{
   uint32_t u = (uint32_t) v;
   unsigned char c[4];
   c[0] = (unsigned char) (u & 0xFF);
   c[1] = (unsigned char) ((u >> 8) & 0xFF);
   c[2] = (unsigned char) ((u >> 16) & 0xFF);
   c[3] = (unsigned char) ((u >> 24) & 0xFF);
   put_bytes(b, c, 4);
}

int fx_legacy_orientation(int raw)
{
   static const int dir_lookup[25] = {
      0x00, 0x01, 0x02, 0x01, 0x02, 0x03, 0x03, 0x05, 0x05, 0x06,
      0x06, 0x07, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
      0x0F, 0x10, 0x11, 0x12, 0x14
   };
   return (raw >= 0 && raw < 25) ? dir_lookup[raw] : -1;
}

int fx_wall_layers(const FX_SPEC *s)  { return s->version >= 4 ? s->wall_num : 1; }
int fx_floor_layers(const FX_SPEC *s) { return s->version >= 16 ? s->floor_num : 1; }

int fx_has_tag_layer(const FX_SPEC *s)
{
   if (s->version < 4)
      return 1;
   return s->version >= 10 && (s->tag_type == 1 || s->tag_type == 2);
}

int fx_has_groups(const FX_SPEC *s)
{
   return s->version >= 12 && (s->tag_type == 1 || s->tag_type == 2);
}

void fx_default(FX_SPEC *s, int version)
{
   int l, x, y, k, cell;

   memset(s, 0, sizeof(*s));
   s->version  = version;
   s->width    = 3;
   s->height   = 2;
   s->act      = version >= 8 ? 3 : 1;
   s->tag_type = version >= 10 ? 1 : 0;
   if (version >= 3)
   {
      s->file_num = 2;
      s->files[0] = "fx\\alpha.dt1";
      s->files[1] = "fx\\beta.dt1";
   }
   s->skip[0]   = 0x11111111;
   s->skip[1]   = 0x22222222;
   s->wall_num  = version >= 4 ? 2 : 1;
   s->floor_num = version >= 16 ? 2 : 1;

   for (y = 0; y < FX_MAX_H; y++)
   {
      for (x = 0; x < FX_MAX_W; x++)
      {
         cell = y * s->width + x;
         for (l = 0; l < 4; l++)
         {
            for (k = 0; k < 4; k++)
               s->wall[l][y][x][k] = (uint8_t) (0x01 + l * 0x20 + (cell * 4 + k) % 0x20);
            if (version < 7)
               s->orient[l][y][x] = (uint8_t) ((cell + l * 6 + 1) % 25);
            else
               s->orient[l][y][x] = (uint8_t) (0x01 + l * 0x08 + cell % 8);
            for (k = 0; k < 3; k++)
               s->orient_pad[l][y][x][k] = 0xEE;
         }
         for (l = 0; l < 2; l++)
            for (k = 0; k < 4; k++)
               s->floor[l][y][x][k] = (uint8_t) (0x81 + l * 0x20 + (cell * 4 + k) % 0x20);
         for (k = 0; k < 4; k++)
            s->shadow[y][x][k] = (uint8_t) (0xC1 + (cell * 4 + k) % 0x20);
         s->tag[y][x] = 0xA0000000u | (uint32_t) (cell + 1);
      }
   }

   if (version >= 2)
   {
      s->obj_num = 2;
      s->obj[0].type = 2; s->obj[0].id = 7; s->obj[0].x = 4;  s->obj[0].y = 6;
      s->obj[0].flags = version >= 6 ? 0x101 : 0;
      s->obj[1].type = 1; s->obj[1].id = 3; s->obj[1].x = 12; s->obj[1].y = 2;
      s->obj[1].flags = 0;
   }

   s->pre_group_dword = 0x13579BDF;
   if (fx_has_groups(s))
   {
      s->group_num = 2;
      s->group[0].tile_x = 0; s->group[0].tile_y = 0;
      s->group[0].width  = 2; s->group[0].height = 1;
      s->group[0].unk    = version >= 13 ? 0x5A5A : 0;
      s->group[1].tile_x = 2; s->group[1].tile_y = 1;
      s->group[1].width  = 1; s->group[1].height = 1;
      s->group[1].unk    = version >= 13 ? 7 : 0;
   }

   if (version >= 14)
   {
      s->npc_num = 1;
      s->npc[0].path_num = 2;
      s->npc[0].x = 4;
      s->npc[0].y = 6;
      s->npc[0].pt[0][0] = 5; s->npc[0].pt[0][1] = 7; s->npc[0].pt[0][2] = 1;
      s->npc[0].pt[1][0] = 6; s->npc[0].pt[1][1] = 8; s->npc[0].pt[1][2] = 3;
   }
}

static void put_cells4(FX_BUF *b, const FX_SPEC *s, const uint8_t cells[FX_MAX_H][FX_MAX_W][4])
{
   int x, y;
   for (y = 0; y < s->height; y++)
      for (x = 0; x < s->width; x++)
         put_bytes(b, cells[y][x], 4);
}

static void put_orient(FX_BUF *b, const FX_SPEC *s, int l)
{
   int x, y;
   for (y = 0; y < s->height; y++)
   {
      for (x = 0; x < s->width; x++)
      {
         put_u8(b, s->orient[l][y][x]);
         put_bytes(b, s->orient_pad[l][y][x], 3);
      }
   }
}

static void put_tags(FX_BUF *b, const FX_SPEC *s)
{
   int x, y;
   for (y = 0; y < s->height; y++)
      for (x = 0; x < s->width; x++)
         put_i32(b, (int32_t) s->tag[y][x]);
}

unsigned char *fx_encode(const FX_SPEC *s, size_t *len, FX_LAYOUT *layout)
{
   FX_BUF b = { NULL, 0, 0 };
   FX_LAYOUT lay;
   int i, p, l;
   int v = s->version;

   memset(&lay, 0, sizeof(lay));
   put_i32(&b, v);
   put_i32(&b, s->width - 1);
   put_i32(&b, s->height - 1);
   if (v >= 8)
      put_i32(&b, s->act - 1);
   if (v >= 10)
      put_i32(&b, s->tag_type);
   lay.files_at = b.len;
   if (v >= 3)
   {
      put_i32(&b, s->file_num);
      for (i = 0; i < s->file_num && i < FX_MAX_FILES; i++)
         put_bytes(&b, s->files[i], strlen(s->files[i]) + 1);
   }
   if (v >= 9 && v <= 13)
   {
      put_i32(&b, s->skip[0]);
      put_i32(&b, s->skip[1]);
   }
   if (v >= 4)
   {
      put_i32(&b, s->wall_num);
      if (v >= 16)
         put_i32(&b, s->floor_num);
   }

   lay.layers_at = b.len;
   if (v < 4)
   {
      put_cells4(&b, s, (const uint8_t (*)[FX_MAX_W][4]) s->wall[0]);
      put_cells4(&b, s, (const uint8_t (*)[FX_MAX_W][4]) s->floor[0]);
      put_orient(&b, s, 0);
      put_tags(&b, s);
      put_cells4(&b, s, (const uint8_t (*)[FX_MAX_W][4]) s->shadow);
   }
   else
   {
      for (l = 0; l < s->wall_num && l < 4; l++)
      {
         put_cells4(&b, s, (const uint8_t (*)[FX_MAX_W][4]) s->wall[l]);
         put_orient(&b, s, l);
      }
      for (l = 0; l < fx_floor_layers(s) && l < 2; l++)
         put_cells4(&b, s, (const uint8_t (*)[FX_MAX_W][4]) s->floor[l]);
      put_cells4(&b, s, (const uint8_t (*)[FX_MAX_W][4]) s->shadow);
      if (fx_has_tag_layer(s))
         put_tags(&b, s);
   }

   lay.objects_at = b.len;
   if (v >= 2)
   {
      put_i32(&b, s->obj_num);
      for (i = 0; i < s->obj_num && i < FX_MAX_OBJ; i++)
      {
         put_i32(&b, s->obj[i].type);
         put_i32(&b, s->obj[i].id);
         put_i32(&b, s->obj[i].x);
         put_i32(&b, s->obj[i].y);
         if (v >= 6)
            put_i32(&b, s->obj[i].flags);
      }
   }

   lay.groups_at = b.len;
   if (fx_has_groups(s))
   {
      if (v >= 18)
         put_i32(&b, s->pre_group_dword);
      put_i32(&b, s->group_num);
      for (i = 0; i < s->group_num && i < FX_MAX_GROUP; i++)
      {
         put_i32(&b, s->group[i].tile_x);
         put_i32(&b, s->group[i].tile_y);
         put_i32(&b, s->group[i].width);
         put_i32(&b, s->group[i].height);
         if (v >= 13)
            put_i32(&b, s->group[i].unk);
      }
   }

   lay.paths_at = b.len;
   if (v >= 14)
   {
      put_i32(&b, s->npc_num);
      for (i = 0; i < s->npc_num && i < FX_MAX_NPC; i++)
      {
         put_i32(&b, s->npc[i].path_num);
         put_i32(&b, s->npc[i].x);
         put_i32(&b, s->npc[i].y);
         for (p = 0; p < s->npc[i].path_num && p < FX_MAX_PTS; p++)
         {
            put_i32(&b, s->npc[i].pt[p][0]);
            put_i32(&b, s->npc[i].pt[p][1]);
            if (v >= 15)
               put_i32(&b, s->npc[i].pt[p][2]);
         }
      }
   }
   lay.end = b.len;

   if (layout != NULL)
      *layout = lay;
   *len = b.len;
   return b.p;
}

int fx_write(const FX_SPEC *s, const char *path, long truncate_to)
{
   size_t len;
   unsigned char *buf = fx_encode(s, &len, NULL);
   FILE *f;
   int ok;

   if (buf == NULL)
      return 0;
   if (truncate_to >= 0 && (size_t) truncate_to < len)
      len = (size_t) truncate_to;
   f = fopen(path, "wb");
   if (f == NULL)
   {
      free(buf);
      return 0;
   }
   ok = fwrite(buf, 1, len, f) == len;
   ok = (fclose(f) == 0) && ok;
   free(buf);
   return ok;
}

/* ---- independent v18 decoder ---- */

typedef struct FX_RD
{
   const unsigned char *p;
   size_t len, pos;
   int ok;
} FX_RD;

static int32_t get_i32(FX_RD *r)
{
   uint32_t u;
   if (!r->ok || r->len - r->pos < 4 || r->pos > r->len)
   {
      r->ok = 0;
      return 0;
   }
   u = (uint32_t) r->p[r->pos] | ((uint32_t) r->p[r->pos + 1] << 8) |
       ((uint32_t) r->p[r->pos + 2] << 16) | ((uint32_t) r->p[r->pos + 3] << 24);
   r->pos += 4;
   return (int32_t) u;
}

static void get_bytes(FX_RD *r, uint8_t *dst, size_t n)
{
   if (!r->ok || r->len - r->pos < n)
   {
      r->ok = 0;
      return;
   }
   memcpy(dst, r->p + r->pos, n);
   r->pos += n;
}

static void get_cells4(FX_RD *r, const FX_SPEC *s, uint8_t cells[FX_MAX_H][FX_MAX_W][4])
{
   int x, y;
   for (y = 0; y < s->height; y++)
      for (x = 0; x < s->width; x++)
         get_bytes(r, cells[y][x], 4);
}

int fx_decode_v18(const unsigned char *buf, size_t len, FX_SPEC *s)
{
   static char names[FX_MAX_FILES][128];
   FX_RD r;
   int i, l, x, y, p;

   memset(s, 0, sizeof(*s));
   r.p = buf; r.len = len; r.pos = 0; r.ok = 1;
   s->version = get_i32(&r);
   if (s->version != 18)
      return -1;
   s->width    = get_i32(&r) + 1;
   s->height   = get_i32(&r) + 1;
   s->act      = get_i32(&r) + 1;
   s->tag_type = get_i32(&r);
   s->file_num = get_i32(&r);
   if (!r.ok || s->width < 1 || s->height < 1 || s->width > FX_MAX_W ||
       s->height > FX_MAX_H || s->file_num < 0)
      return -1;
   for (i = 0; i < s->file_num; i++)
   {
      const char *start = (const char *) r.p + r.pos;
      const char *nul = (const char *) memchr(start, 0, r.len - r.pos);
      if (nul == NULL)
         return -1;
      if (i < FX_MAX_FILES)
      {
         snprintf(names[i], sizeof(names[i]), "%s", start);
         s->files[i] = names[i];
      }
      r.pos += (size_t) (nul - start) + 1;
   }
   s->wall_num  = get_i32(&r);
   s->floor_num = get_i32(&r);
   if (!r.ok || s->wall_num < 0 || s->wall_num > 4 || s->floor_num < 0 || s->floor_num > 2)
      return -1;
   for (l = 0; l < s->wall_num; l++)
   {
      get_cells4(&r, s, s->wall[l]);
      for (y = 0; y < s->height; y++)
         for (x = 0; x < s->width; x++)
         {
            get_bytes(&r, &s->orient[l][y][x], 1);
            get_bytes(&r, s->orient_pad[l][y][x], 3);
         }
   }
   for (l = 0; l < s->floor_num; l++)
      get_cells4(&r, s, s->floor[l]);
   get_cells4(&r, s, s->shadow);
   if (s->tag_type == 1 || s->tag_type == 2)
      for (y = 0; y < s->height; y++)
         for (x = 0; x < s->width; x++)
            s->tag[y][x] = (uint32_t) get_i32(&r);
   s->obj_num = get_i32(&r);
   if (!r.ok || s->obj_num < 0 || s->obj_num > FX_MAX_OBJ)
      return -1;
   for (i = 0; i < s->obj_num; i++)
   {
      s->obj[i].type  = get_i32(&r);
      s->obj[i].id    = get_i32(&r);
      s->obj[i].x     = get_i32(&r);
      s->obj[i].y     = get_i32(&r);
      s->obj[i].flags = get_i32(&r);
   }
   if (s->tag_type == 1 || s->tag_type == 2)
   {
      s->pre_group_dword = get_i32(&r);
      s->group_num = get_i32(&r);
      if (!r.ok || s->group_num < 0 || s->group_num > FX_MAX_GROUP)
         return -1;
      for (i = 0; i < s->group_num; i++)
      {
         s->group[i].tile_x = get_i32(&r);
         s->group[i].tile_y = get_i32(&r);
         s->group[i].width  = get_i32(&r);
         s->group[i].height = get_i32(&r);
         s->group[i].unk    = get_i32(&r);
      }
   }
   s->npc_num = get_i32(&r);
   if (!r.ok || s->npc_num < 0 || s->npc_num > FX_MAX_NPC)
      return -1;
   for (i = 0; i < s->npc_num; i++)
   {
      s->npc[i].path_num = get_i32(&r);
      s->npc[i].x = get_i32(&r);
      s->npc[i].y = get_i32(&r);
      if (!r.ok || s->npc[i].path_num < 0 || s->npc[i].path_num > FX_MAX_PTS)
         return -1;
      for (p = 0; p < s->npc[i].path_num; p++)
      {
         s->npc[i].pt[p][0] = get_i32(&r);
         s->npc[i].pt[p][1] = get_i32(&r);
         s->npc[i].pt[p][2] = get_i32(&r);
      }
   }
   if (!r.ok || r.pos != r.len)
      return -1;
   return 0;
}
