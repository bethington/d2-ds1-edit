/* Authored DS1 fixtures, encoded in C independently of src/core/ds1.c.
 *
 * The encoder below follows the on-disk layout documented in
 * docs/guides/ds1-map-format.md, version by version. It never calls the
 * editor's reader or writer, so a test that encodes a spec, reads it with the
 * real ds1_read() and compares against the spec is checking the reader
 * against an independent description of the format. Every value is synthetic;
 * no game data is used.
 */
#ifndef DS1EDIT_TEST_DS1_FIXTURE_H
#define DS1EDIT_TEST_DS1_FIXTURE_H

#include <stddef.h>
#include <stdint.h>

#define FX_MAX_W      8
#define FX_MAX_H      8
#define FX_MAX_OBJ    8
#define FX_MAX_GROUP  4
#define FX_MAX_NPC    4
#define FX_MAX_PTS    8
#define FX_MAX_FILES  4

typedef struct FX_OBJ
{
   int32_t type, id, x, y, flags;
} FX_OBJ;

typedef struct FX_GROUP
{
   int32_t tile_x, tile_y, width, height, unk;
} FX_GROUP;

typedef struct FX_NPC
{
   int32_t path_num, x, y;
   int32_t pt[FX_MAX_PTS][3]; /* x, y, action */
} FX_NPC;

typedef struct FX_SPEC
{
   int32_t version;
   int32_t width, height;     /* real cell counts; the file stores -1 */
   int32_t act;               /* 1-based; the file stores act-1 (v>=8) */
   int32_t tag_type;          /* v>=10 */
   int32_t file_num;
   const char *files[FX_MAX_FILES];
   int32_t skip[2];           /* the dword pair present in v9..13 */
   int32_t wall_num, floor_num;          /* v>=4 / v>=16 */
   uint8_t wall[4][FX_MAX_H][FX_MAX_W][4];
   uint8_t orient[4][FX_MAX_H][FX_MAX_W]; /* raw on-disk byte */
   uint8_t orient_pad[4][FX_MAX_H][FX_MAX_W][3];
   uint8_t floor[2][FX_MAX_H][FX_MAX_W][4];
   uint8_t shadow[FX_MAX_H][FX_MAX_W][4];
   uint32_t tag[FX_MAX_H][FX_MAX_W];
   int32_t obj_num;
   FX_OBJ  obj[FX_MAX_OBJ];
   int32_t pre_group_dword;   /* v>=18, before the group count */
   int32_t group_num;
   FX_GROUP group[FX_MAX_GROUP];
   int32_t npc_num;
   FX_NPC  npc[FX_MAX_NPC];
} FX_SPEC;

/* Section offsets recorded while encoding. */
typedef struct FX_LAYOUT
{
   size_t files_at, layers_at, objects_at, groups_at, paths_at, end;
} FX_LAYOUT;

/* Default authored content for `version`: 3x2 map, every byte distinct and
 * nonzero, two walls and two floors where the version allows them, a tag
 * layer and groups where it allows them, one object with one path. */
void   fx_default(FX_SPEC *s, int version);

/* Layer counts the format implies for this spec. */
int    fx_wall_layers(const FX_SPEC *s);
int    fx_floor_layers(const FX_SPEC *s);
int    fx_has_tag_layer(const FX_SPEC *s);
int    fx_has_groups(const FX_SPEC *s);

/* Encode into a malloc'd buffer (caller frees). */
unsigned char *fx_encode(const FX_SPEC *s, size_t *len, FX_LAYOUT *layout);

/* Encode and write to `path`; 1 on success. Optionally truncated to
 * `truncate_to` bytes when that is smaller than the encoded length. */
int    fx_write(const FX_SPEC *s, const char *path, long truncate_to);

/* Decode a version-18 file (what the editor writes) into `out`, without
 * using the editor's reader. Returns 0, or -1 if the bytes do not follow the
 * v18 layout exactly (including trailing garbage) or exceed the FX_ maxima. */
int    fx_decode_v18(const unsigned char *buf, size_t len, FX_SPEC *out);

/* The orientation remap the editor applies to versions below 7. */
int    fx_legacy_orientation(int raw);

#endif
