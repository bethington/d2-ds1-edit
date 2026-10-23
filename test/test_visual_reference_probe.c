/* Reference probe: decode one asset with the editor's real decoders and write
 * the raw palette-index frames for independent comparison.
 *
 *   test_visual_reference_probe decode --kind dt1|dcc|dc6 --input PATH
 *                                      --direction N --out-dir PATH
 *
 * Writes PATH/frames.json and one raw buffer per frame (stride * height
 * bytes, top-down rows). N is a direction index, or -1 for all directions
 * (DT1 has no directions; pass 0 or -1). Exit 0 on success, 1 on a decode
 * failure (reported on stderr), 2 on bad usage.
 *
 * frames.json:
 *   {"schema": "ds1edit.index-frames/1", "kind": ..., "input_sha256": ...,
 *    "direction": N, "transparency": "index0",
 *    "frames": [{"direction", "frame", "width", "height", "stride",
 *                "origin_x", "origin_y", "file", "sha256", ...}]}
 * DT1 frames are blocks in file order, with origin (0, 0) at the top-left of
 * the editor's block image, and add the block header fields. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "core/index_frame.h"
#include "core/dt1.h"
#include "core/rgba_cache.h"
#include "visual_test_support.h"

/* ---- SHA-256 (FIPS 180-4), for input/output identities ----------------- */

typedef struct { uint32_t h[8]; uint64_t n; unsigned char buf[64]; size_t used; } SHA256;

static uint32_t ror(uint32_t x, int r) { return (x >> r) | (x << (32 - r)); }

static void sha256_block(SHA256 *s, const unsigned char *p)
{
   static const uint32_t k[64] = {
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
   uint32_t w[64], a, b, c, d, e, f, g, h;
   int i;

   for (i = 0; i < 16; i++)
      w[i] = ((uint32_t) p[4*i] << 24) | ((uint32_t) p[4*i+1] << 16)
           | ((uint32_t) p[4*i+2] << 8) | p[4*i+3];
   for (i = 16; i < 64; i++)
      w[i] = w[i-16] + (ror(w[i-15],7) ^ ror(w[i-15],18) ^ (w[i-15] >> 3))
           + w[i-7] + (ror(w[i-2],17) ^ ror(w[i-2],19) ^ (w[i-2] >> 10));
   a=s->h[0]; b=s->h[1]; c=s->h[2]; d=s->h[3]; e=s->h[4]; f=s->h[5]; g=s->h[6]; h=s->h[7];
   for (i = 0; i < 64; i++)
   {
      uint32_t t1 = h + (ror(e,6) ^ ror(e,11) ^ ror(e,25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
      uint32_t t2 = (ror(a,2) ^ ror(a,13) ^ ror(a,22)) + ((a & b) ^ (a & c) ^ (b & c));
      h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
   }
   s->h[0]+=a; s->h[1]+=b; s->h[2]+=c; s->h[3]+=d; s->h[4]+=e; s->h[5]+=f; s->h[6]+=g; s->h[7]+=h;
}

static void sha256_hex(const void *data, size_t len, char out[65])
{
   static const uint32_t init[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                                    0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
   SHA256 s;
   const unsigned char *p = (const unsigned char *) data;
   unsigned char tail[128];
   size_t i, rem, tail_len;
   uint64_t bits = (uint64_t) len * 8;

   memcpy(s.h, init, sizeof(init));
   for (i = 0; i + 64 <= len; i += 64)
      sha256_block(&s, p + i);
   rem = len - i;
   memset(tail, 0, sizeof(tail));
   memcpy(tail, p + i, rem);
   tail[rem] = 0x80;
   tail_len = (rem + 9 <= 64) ? 64 : 128;
   for (i = 0; i < 8; i++)
      tail[tail_len - 1 - i] = (unsigned char) (bits >> (8 * i));
   sha256_block(&s, tail);
   if (tail_len == 128)
      sha256_block(&s, tail + 64);
   for (i = 0; i < 8; i++)
      sprintf(out + 8 * i, "%08x", s.h[i]);
   out[64] = 0;
}

/* ---- output -------------------------------------------------------------- */

static int write_frame_file(const char *dir, const char *leaf,
                            const unsigned char *px, size_t len)
{
   char path[1024];
   FILE *fp;
   int ok;

   snprintf(path, sizeof(path), "%s/%s", dir, leaf);
   fp = fopen(path, "wb");
   if (fp == NULL)
      return 0;
   ok = (len == 0) || (fwrite(px, 1, len, fp) == len);
   if (fclose(fp) != 0)
      ok = 0;
   return ok;
}

static FILE *open_json(const char *dir, const char *kind, const char *in_sha,
                       int direction)
{
   char path[1024];
   FILE *fp;

   snprintf(path, sizeof(path), "%s/frames.json", dir);
   fp = fopen(path, "wb");
   if (fp == NULL)
      return NULL;
   fprintf(fp, "{\"schema\": \"ds1edit.index-frames/1\", \"kind\": \"%s\", "
               "\"input_sha256\": \"%s\", \"direction\": %d, "
               "\"transparency\": \"index0\", \"frames\": [", kind, in_sha,
           direction);
   return fp;
}

static int emit_frame(FILE *js, const char *dir, int first,
                      const DS1EDIT_INDEX_FRAME *f, const char *extra)
{
   char leaf[64], sha[65];

   snprintf(leaf, sizeof(leaf), "d%02d_f%04d.bin", (int) f->direction, (int) f->frame);
   if (!write_frame_file(dir, leaf, f->pixels, f->pixels_len))
      return 0;
   sha256_hex(f->pixels ? (const void *) f->pixels : (const void *) "", f->pixels_len, sha);
   fprintf(js, "%s\n {\"direction\": %d, \"frame\": %d, \"width\": %d, "
               "\"height\": %d, \"stride\": %d, \"origin_x\": %d, "
               "\"origin_y\": %d, \"file\": \"%s\", \"sha256\": \"%s\"%s}",
           first ? "" : ",", (int) f->direction, (int) f->frame,
           (int) f->width, (int) f->height, (int) f->stride,
           (int) f->origin_x, (int) f->origin_y, leaf, sha,
           extra ? extra : "");
   return 1;
}

static int finish_json(FILE *fp)
{
   fprintf(fp, "\n]}\n");
   return fclose(fp) == 0;
}

/* ---- decoders ------------------------------------------------------------ */

static int probe_sprite(const char *kind, const unsigned char *bytes, size_t len,
                        int direction, const char *out_dir, const char *in_sha)
{
   DS1EDIT_INDEX_FRAME *frames = NULL;
   size_t count = 0, i;
   char err[256];
   int ok;
   FILE *js;

   if (strcmp(kind, "dc6") == 0)
      ok = dc6_decode_indices(bytes, len, direction, &frames, &count, err, sizeof(err));
   else
      ok = dcc_decode_indices(bytes, len, direction, &frames, &count, err, sizeof(err));
   if (!ok)
   {
      fprintf(stderr, "decode failed: %s\n", err);
      return 1;
   }

   js = open_json(out_dir, kind, in_sha, direction);
   ok = (js != NULL);
   for (i = 0; ok && i < count; i++)
      ok = emit_frame(js, out_dir, i == 0, &frames[i], NULL);
   if (js != NULL && !finish_json(js))
      ok = 0;
   ds1edit_index_frames_free(frames, count);
   if (!ok)
      fprintf(stderr, "could not write the output in %s\n", out_dir);
   return ok ? 0 : 1;
}

static int probe_dt1(const unsigned char *bytes, size_t len,
                     const char *out_dir, const char *in_sha)
{
   char name[] = "probe\\input.dt1";
   int idx, b, ok = 1;
   FILE *js;

   if (!visual_allegro_init())
   {
      fprintf(stderr, "al_init failed\n");
      return 1;
   }
   visual_fetch_reset();
   visual_fetch_add(name, bytes, len);
   idx = dt1_add(name);
   if (idx < 0)
   {
      fprintf(stderr, "dt1_add rejected the input\n");
      return 1;
   }

   js = open_json(out_dir, "dt1", in_sha, 0);
   ok = (js != NULL);
   for (b = 0; ok && b < glb_dt1[idx].block_num; b++)
   {
      BLOCK_S *bh = ((BLOCK_S *) glb_dt1[idx].bh_buffer) + b;
      CACHED_TILE *ct = (glb_dt1[idx].block_cache[ZM_11] != NULL)
                        ? glb_dt1[idx].block_cache[ZM_11][b] : NULL;
      DS1EDIT_INDEX_FRAME f;
      char extra[256];

      memset(&f, 0, sizeof(f));
      f.frame = b;
      if (ct != NULL)
      {
         f.width = ct->width;
         f.height = ct->height;
         f.stride = ct->width;
         f.pixels = ct->indices;
         f.pixels_len = (size_t) ct->width * (size_t) ct->height;
      }
      snprintf(extra, sizeof(extra),
               ", \"orientation\": %ld, \"main_index\": %ld, \"sub_index\": %ld, "
               "\"rarity\": %ld, \"block_direction\": %ld, \"roof_y\": %ld, "
               "\"animated\": %d, \"subtiles\": %ld",
               (long) bh->orientation, (long) bh->main_index,
               (long) bh->sub_index, (long) bh->rarity, (long) bh->direction,
               (long) bh->roof_y, (int) bh->animated, (long) bh->tiles_number);
      ok = emit_frame(js, out_dir, b == 0, &f, extra);
   }
   if (js != NULL && !finish_json(js))
      ok = 0;
   dt1_del(idx);
   return ok ? 0 : 1;
}

static int usage(void)
{
   fprintf(stderr, "usage: test_visual_reference_probe decode --kind dt1|dcc|dc6 "
                   "--input PATH --direction N --out-dir PATH\n");
   return 2;
}

int main(int argc, char **argv)
{
   const char *kind = NULL, *input = NULL, *out_dir = NULL;
   int direction = 0, have_dir = 0, i, rc;
   unsigned char *bytes;
   size_t len;
   char in_sha[65];

   if (argc < 2 || strcmp(argv[1], "decode") != 0)
      return usage();
   for (i = 2; i + 1 < argc; i += 2)
   {
      if (strcmp(argv[i], "--kind") == 0)            kind = argv[i + 1];
      else if (strcmp(argv[i], "--input") == 0)      input = argv[i + 1];
      else if (strcmp(argv[i], "--out-dir") == 0)    out_dir = argv[i + 1];
      else if (strcmp(argv[i], "--direction") == 0)  { direction = atoi(argv[i + 1]); have_dir = 1; }
      else return usage();
   }
   if (i != argc || kind == NULL || input == NULL || out_dir == NULL || !have_dir)
      return usage();
   if (strcmp(kind, "dt1") != 0 && strcmp(kind, "dcc") != 0 && strcmp(kind, "dc6") != 0)
      return usage();

   bytes = visual_read_file(input, &len);
   if (bytes == NULL)
   {
      fprintf(stderr, "cannot read %s\n", input);
      return 1;
   }
   sha256_hex(bytes, len, in_sha);

   if (strcmp(kind, "dt1") == 0)
      rc = probe_dt1(bytes, len, out_dir, in_sha);
   else
      rc = probe_sprite(kind, bytes, len, direction, out_dir, in_sha);
   free(bytes);
   return rc;
}
