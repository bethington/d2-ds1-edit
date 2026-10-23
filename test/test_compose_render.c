/* Composition through the real compose_render: COF path, COF parse,
 * per-layer DCC/DC6 lookup and decode, canvas bounds, priority-ordered blit,
 * palette conversion. The archive fetch serves authored COF/DC6 bytes and
 * one generated DCC fixture; everything else is production code. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"
#include "structs.h"
#include "core/compose_cof.h"
#include "core/compose_render.h"
#include "core/compose_apng.h"
#include "core/animdata.h"
#include "core/upscale.h"
#include "platform.h"
#include "visual_test_support.h"

#define BASE "data\\global\\monsters"

static RGBA_PALETTE s_pal_a, s_pal_b;
static void make_dir(const char *path);

/* ---- authoring ------------------------------------------------------------- */

static void put32(uint8_t *p, int32_t v)
{
   p[0] = (uint8_t) v; p[1] = (uint8_t) (v >> 8); p[2] = (uint8_t) (v >> 16); p[3] = (uint8_t) (v >> 24);
}

typedef struct { int w, h, x, y; uint8_t color; uint8_t hole; } RECT_FRAME;

/* DC6 with `dirs` x `fpd` solid rectangles (top-left at x, y). A nonzero
 * `hole` leaves the top-left pixel transparent. */
static void add_dc6(const char *path, int dirs, int fpd, const RECT_FRAME *fr)
{
   uint8_t buf[4096];
   size_t pos, n = (size_t) dirs * fpd, i;
   int x, y;

   memset(buf, 0, sizeof(buf));
   put32(buf, 6); put32(buf + 4, 1); put32(buf + 8, 0);
   put32(buf + 16, dirs); put32(buf + 20, fpd);
   pos = 24 + 4 * n;
   for (i = 0; i < n; i++)
   {
      size_t data = pos + 32, len = 0;
      put32(buf + 24 + 4 * i, (int32_t) pos);
      put32(buf + pos + 4, fr[i].w);
      put32(buf + pos + 8, fr[i].h);
      put32(buf + pos + 12, fr[i].x);
      put32(buf + pos + 16, fr[i].y + fr[i].h - 1);     /* offsetY = bottom row */
      for (y = fr[i].h - 1; y >= 0; y--)
      {
         x = 0;
         if (fr[i].hole && y == 0)
         {
            buf[data + len++] = 0x81;
            x = 1;
         }
         buf[data + len++] = (uint8_t) (fr[i].w - x);
         for (; x < fr[i].w; x++)
            buf[data + len++] = fr[i].color;
         buf[data + len++] = 0x80;
      }
      put32(buf + pos + 28, (int32_t) len);
      pos = data + len + 3;
   }
   visual_fetch_add(path, buf, pos);
}

typedef struct { uint8_t slot, trans_a, trans_b; const char *wclass; } COF_LAYER;

static void add_cof(const char *path, int dirs, int fpd, const COF_LAYER *layers,
                    int nl, const uint8_t *priority)
{
   uint8_t buf[2048];
   size_t pos = 28;
   int i;

   memset(buf, 0, sizeof(buf));
   buf[0] = (uint8_t) nl; buf[1] = (uint8_t) fpd; buf[2] = (uint8_t) dirs; buf[3] = 20;
   for (i = 0; i < nl; i++)
   {
      buf[pos + 0] = layers[i].slot;
      buf[pos + 1] = 1;
      buf[pos + 2] = 1;
      buf[pos + 3] = layers[i].trans_a;
      buf[pos + 4] = layers[i].trans_b;
      memcpy(buf + pos + 5, layers[i].wclass, strlen(layers[i].wclass));
      pos += 9;
   }
   pos += (size_t) fpd;                                  /* frame flags */
   memcpy(buf + pos, priority, (size_t) dirs * fpd * nl);
   pos += (size_t) dirs * fpd * nl;
   visual_fetch_add(path, buf, pos);
}

static void params(COMPOSE_RENDER_PARAMS_S *p, const char *token, int direction)
{
   memset(p, 0, sizeof(*p));
   p->base = BASE;
   p->token = token;
   p->mode = "NU";
   p->wclass = "HTH";
   p->skin = "LIT";
   p->direction = direction;
}

static const uint8_t *px(const COMPOSE_RENDER_RESULT_S *r, int f, int x, int y)
{
   return r->frames[f] + ((size_t) y * r->width + x) * 4;
}

static void assert_color(const RGBA_PALETTE *pal, uint8_t idx, const uint8_t *p)
{
   if (idx == 0)
   {
      TEST_ASSERT_EQUAL_UINT8(0, p[3]);
      return;
   }
   TEST_ASSERT_EQUAL_UINT8(pal->colors[idx].r, p[0]);
   TEST_ASSERT_EQUAL_UINT8(pal->colors[idx].g, p[1]);
   TEST_ASSERT_EQUAL_UINT8(pal->colors[idx].b, p[2]);
   TEST_ASSERT_EQUAL_UINT8(255, p[3]);
}

void setUp(void)
{
   int i;
   for (i = 0; i < 256; i++)
   {
      s_pal_a.colors[i].r = (uint8_t) i;
      s_pal_a.colors[i].g = (uint8_t) (255 - i);
      s_pal_a.colors[i].b = (uint8_t) (i * 3);
      s_pal_a.colors[i].a = i ? 255 : 0;
      s_pal_b.colors[i].r = (uint8_t) (i * 7);
      s_pal_b.colors[i].g = 9;
      s_pal_b.colors[i].b = (uint8_t) (255 - i);
      s_pal_b.colors[i].a = i ? 255 : 0;
   }
   a5_current_palette = &s_pal_a;
   visual_fetch_reset();
}

void tearDown(void) { a5_current_palette = NULL; }

/* Two overlapping layers, 2 directions x 2 frames, priority reversed by
 * frame and by direction. HD: 2x2 index 10 at (0,0). TR: 3x3 index 20 at
 * (-2,-1) direction 0 and (1,1) direction 1. */
static void author_two_layers(void)
{
   static const COF_LAYER layers[2] = { {0, 0, 0, "HTH"}, {1, 0, 0, "HTH"} };
   /* [dir][frame][order] = composit index */
   static const uint8_t prio[2 * 2 * 2] = { 1, 0,   0, 1,     0, 1,   1, 0 };
   RECT_FRAME hd[4] = { {2, 2, 0, 0, 10, 0}, {2, 2, 0, 0, 11, 1},
                        {2, 2, 0, 0, 10, 0}, {2, 2, 0, 0, 11, 1} };
   RECT_FRAME tr[4] = { {3, 3, -2, -1, 20, 0}, {3, 3, -2, -1, 21, 0},
                        {3, 3, 1, 1, 20, 0},   {3, 3, 1, 1, 21, 0} };

   add_cof(BASE "\\XX\\COF\\XXNUHTH.cof", 2, 2, layers, 2, prio);
   add_dc6(BASE "\\XX\\HD\\XXHDLITNUHTH.dc6", 2, 2, hd);
   add_dc6(BASE "\\XX\\TR\\XXTRLITNUHTH.dc6", 2, 2, tr);
}

void test_cof_priority_is_read_per_direction_and_frame(void)
{
   COMPOSE_COF_S cof;
   char *buf;
   long len;

   author_two_layers();
   TEST_ASSERT_TRUE(misc_load_mpq_file(BASE "\\XX\\COF\\XXNUHTH.cof", &buf, &len, 0) >= 0);
   TEST_ASSERT_EQUAL_INT(1, compose_cof_parse(buf, len, &cof));
   TEST_ASSERT_EQUAL_INT(1, compose_cof_priority_at(&cof, 0, 0, 0));
   TEST_ASSERT_EQUAL_INT(0, compose_cof_priority_at(&cof, 0, 1, 0));
   TEST_ASSERT_EQUAL_INT(0, compose_cof_priority_at(&cof, 1, 0, 0));
   TEST_ASSERT_EQUAL_INT(0, compose_cof_priority_at(&cof, 1, 1, 1));
   TEST_ASSERT_EQUAL_INT(-1, compose_cof_priority_at(&cof, 2, 0, 0));
   compose_cof_free(&cof);
   free(buf);
}

void test_overlap_follows_priority_with_signed_offsets(void)
{
   COMPOSE_RENDER_PARAMS_S p;
   COMPOSE_RENDER_RESULT_S r;

   author_two_layers();
   params(&p, "XX", 0);
   TEST_ASSERT_EQUAL_INT(1, compose_render(&p, &r));

   /* bounds: TR x -2..0, y -1..1; HD x 0..1, y 0..1 -> x -2..1, y -1..1 */
   TEST_ASSERT_EQUAL_INT(4, r.width);
   TEST_ASSERT_EQUAL_INT(3, r.height);
   TEST_ASSERT_EQUAL_INT(2, r.frame_count);

   /* sprite (0,0) is canvas (2,1): both layers cover it */
   assert_color(&s_pal_a, 10, px(&r, 0, 2, 1));   /* frame 0: TR then HD */
   assert_color(&s_pal_a, 21, px(&r, 1, 2, 1));   /* frame 1: HD then TR */
   /* frame 1's HD has a transparent top-left: TR shows... and where TR is
    * absent, nothing */
   assert_color(&s_pal_a, 11, px(&r, 1, 3, 2));
   assert_color(&s_pal_a, 0, px(&r, 0, 3, 0));     /* neither layer */
   assert_color(&s_pal_a, 20, px(&r, 0, 0, 0));
   compose_render_free(&r);
}

void test_direction_selects_its_own_frames_and_priority(void)
{
   COMPOSE_RENDER_PARAMS_S p;
   COMPOSE_RENDER_RESULT_S r;

   author_two_layers();
   params(&p, "XX", 1);
   TEST_ASSERT_EQUAL_INT(1, compose_render(&p, &r));
   /* HD x 0..1, y 0..1; TR x 1..3, y 1..3 -> 4x4 from (0,0) */
   TEST_ASSERT_EQUAL_INT(4, r.width);
   TEST_ASSERT_EQUAL_INT(4, r.height);
   assert_color(&s_pal_a, 20, px(&r, 0, 1, 1));   /* dir 1 frame 0: HD then TR */
   assert_color(&s_pal_a, 11, px(&r, 1, 1, 1));   /* dir 1 frame 1: TR then HD */
   assert_color(&s_pal_a, 0, px(&r, 1, 0, 0));     /* HD's hole, TR absent */
   compose_render_free(&r);
}

void test_palette_selection_changes_colours_not_indices(void)
{
   COMPOSE_RENDER_PARAMS_S p;
   COMPOSE_RENDER_RESULT_S r;

   author_two_layers();
   a5_current_palette = &s_pal_b;
   params(&p, "XX", 0);
   TEST_ASSERT_EQUAL_INT(1, compose_render(&p, &r));
   assert_color(&s_pal_b, 10, px(&r, 0, 2, 1));
   assert_color(&s_pal_b, 20, px(&r, 0, 0, 0));
   compose_render_free(&r);
}

void test_absent_optional_layer_and_duplicate_component(void)
{
   /* LA (slot 4) is declared but has no file; HD is declared twice. */
   static const COF_LAYER layers[3] = { {0, 0, 0, "HTH"}, {4, 0, 0, "HTH"}, {0, 0, 0, "HTH"} };
   static const uint8_t prio[3] = { 4, 0, 0 };
   RECT_FRAME hd[1] = { {2, 1, 5, 5, 30, 0} };
   COMPOSE_RENDER_PARAMS_S p;
   COMPOSE_RENDER_RESULT_S r;

   add_cof(BASE "\\YY\\COF\\YYNUHTH.cof", 1, 1, layers, 3, prio);
   add_dc6(BASE "\\YY\\HD\\YYHDLITNUHTH.dc6", 1, 1, hd);
   params(&p, "YY", 0);
   TEST_ASSERT_EQUAL_INT(1, compose_render(&p, &r));
   TEST_ASSERT_EQUAL_INT(2, r.width);
   TEST_ASSERT_EQUAL_INT(1, r.height);
   assert_color(&s_pal_a, 30, px(&r, 0, 1, 0));
   compose_render_free(&r);
}

void test_zero_loadable_components_fails_cleanly(void)
{
   static const COF_LAYER layers[1] = { {0, 0, 0, "HTH"} };
   static const uint8_t prio[1] = { 0 };
   COMPOSE_RENDER_PARAMS_S p;
   COMPOSE_RENDER_RESULT_S r;

   add_cof(BASE "\\ZZ\\COF\\ZZNUHTH.cof", 1, 1, layers, 1, prio);
   params(&p, "ZZ", 0);
   memset(&r, 0x5A, sizeof(r));
   TEST_ASSERT_EQUAL_INT(0, compose_render(&p, &r));
   TEST_ASSERT_NULL(r.frames);
   TEST_ASSERT_EQUAL_INT(0, r.frame_count);
   compose_render_free(&r);

   params(&p, "ZZ", 1);                            /* direction out of range */
   TEST_ASSERT_EQUAL_INT(0, compose_render(&p, &r));
   params(&p, "QQ", 0);                            /* no COF */
   TEST_ASSERT_EQUAL_INT(0, compose_render(&p, &r));
}

/* A zero-layer COF (three ship with Project Diablo 2) is valid but empty:
 * composing it is a success with nothing to write, not an error. */
void test_zero_layer_cof_composes_to_nothing_without_error(void)
{
   COMPOSE_RENDER_PARAMS_S p;
   COMPOSE_RENDER_RESULT_S r;
   char out[512];
   FILE *f;

   {
      static const COF_LAYER none[1] = { {0, 0, 0, ""} };
      static const uint8_t noprio[1] = { 0 };
      add_cof(BASE "\\ZL\\COF\\ZLNUHTH.cof", 1, 1, none, 0, noprio);
   }
   params(&p, "ZL", 0);
   memset(&r, 0x5A, sizeof(r));
   TEST_ASSERT_EQUAL_INT(1, compose_render(&p, &r));
   TEST_ASSERT_NULL(r.frames);
   TEST_ASSERT_EQUAL_INT(0, r.frame_count);
   compose_render_free(&r);

   params(&p, "ZL", 1);                            /* direction still checked */
   TEST_ASSERT_EQUAL_INT(0, compose_render(&p, &r));

   make_dir(DS1EDIT_TEST_OUT_DIR);
   snprintf(out, sizeof(out), "%s/zero_layer.apng", DS1EDIT_TEST_OUT_DIR);
   remove(out);
   params(&p, "ZL", 0);
   TEST_ASSERT_EQUAL_INT(1, compose_apng_export_scaled(&p, out, 1));
   TEST_ASSERT_EQUAL_INT(1, compose_apng_export_scaled(&p, out, 2));
   TEST_ASSERT_EQUAL_INT(1, compose_apng_export_method(&p, out, 2, "nn"));
   f = fopen(out, "rb");
   TEST_ASSERT_NULL(f);                            /* no output written */
}

void test_dcc_layer_decodes_through_the_shared_seam(void)
{
   /* dcc.single_frame from the decoder fixtures as the torso: 8x6 at
    * (-4,-12); its first pixel is index 10 ('R'). */
   static const COF_LAYER layers[1] = { {1, 0, 0, "HTH"} };
   static const uint8_t prio[1] = { 1 };
   size_t len;
   unsigned char *dcc = visual_read_file(DS1EDIT_DECODER_FIXTURES "/dcc_single.dcc", &len);
   COMPOSE_RENDER_PARAMS_S p;
   COMPOSE_RENDER_RESULT_S r;

   TEST_ASSERT_NOT_NULL(dcc);
   add_cof(BASE "\\DD\\COF\\DDNUHTH.cof", 1, 1, layers, 1, prio);
   visual_fetch_add(BASE "\\DD\\TR\\DDTRLITNUHTH.dcc", dcc, len);
   params(&p, "DD", 0);
   TEST_ASSERT_EQUAL_INT(1, compose_render(&p, &r));
   TEST_ASSERT_EQUAL_INT(8, r.width);
   TEST_ASSERT_EQUAL_INT(6, r.height);
   assert_color(&s_pal_a, 10, px(&r, 0, 0, 0));
   assert_color(&s_pal_a, 0, px(&r, 0, 4, 0));
   compose_render_free(&r);
   free(dcc);
}

void test_assumption_COMPOSE_BLEND_OPAQUE_layers_with_trans_flags(void)
{
   /* Editor policy today: compose_render draws every layer opaque-over,
    * whatever its COF trans_a/trans_b (the in-editor preview blends; the
    * compose export does not). Pinned so a change is deliberate; the game's
    * blend for trans_b 0..6 is listed as unconfirmed in the contract. */
   static const COF_LAYER layers[2] = { {0, 0, 0, "HTH"}, {1, 1, 1, "HTH"} };
   static const uint8_t prio[2] = { 0, 1 };
   RECT_FRAME hd[1] = { {1, 1, 0, 0, 40, 0} };
   RECT_FRAME tr[1] = { {1, 1, 0, 0, 50, 0} };
   COMPOSE_RENDER_PARAMS_S p;
   COMPOSE_RENDER_RESULT_S r;

   add_cof(BASE "\\BB\\COF\\BBNUHTH.cof", 1, 1, layers, 2, prio);
   add_dc6(BASE "\\BB\\HD\\BBHDLITNUHTH.dc6", 1, 1, hd);
   add_dc6(BASE "\\BB\\TR\\BBTRLITNUHTH.dc6", 1, 1, tr);
   params(&p, "BB", 0);
   TEST_ASSERT_EQUAL_INT(1, compose_render(&p, &r));
   assert_color(&s_pal_a, 50, px(&r, 0, 0, 0));
   compose_render_free(&r);
}

/* ---- exports written for independent decoding --------------------------------
 * scripts/equivalence/export_cases.py authored these inputs and decodes the
 * files written here with Pillow (CTest visual_export_images). */

/* compose_apng.c's upscale paths are not exercised here. */
int upscale_is_remote_configured(void) { return 0; }
int upscale_create_temp_dir(char *out, int out_cap) { (void) out; (void) out_cap; return 0; }
int upscale_remove_tree(const char *path) { (void) path; return 0; }
int upscale_directory_local(const char *s, const char *d, int scale, char *e, int c)
{ (void) s; (void) d; (void) scale; (void) e; (void) c; return 0; }
int upscale_directory_remote(const char *s, const char *d, int scale, const char *m, char *e, int c)
{ (void) s; (void) d; (void) scale; (void) m; (void) e; (void) c; return 0; }

static RGBA_PALETTE s_export_pal;

static void add_fixture(const char *archive_path, const char *fixture)
{
   char path[512];
   size_t len;
   unsigned char *b;

   snprintf(path, sizeof(path), "%s/exports/%s", DS1EDIT_VISUAL_FIXTURES, fixture);
   b = visual_read_file(path, &len);
   TEST_ASSERT_NOT_NULL_MESSAGE(b, path);
   visual_fetch_add(archive_path, b, len);
   free(b);
}

static void make_dir(const char *path)
{
   DS1_MKDIR(path);
}

void test_compose_exports_write_apng(void)
{
   COMPOSE_RENDER_PARAMS_S p;
   COMPOSE_RENDER_RESULT_S r, one;
   char out[512];
   size_t len;
   unsigned char *pal = visual_read_file(DS1EDIT_VISUAL_FIXTURES "/exports/palette.rgba", &len);

   TEST_ASSERT_NOT_NULL(pal);
   TEST_ASSERT_EQUAL_size_t(1024, len);
   memcpy(s_export_pal.colors, pal, 1024);
   free(pal);
   a5_current_palette = &s_export_pal;

   add_fixture("Data\\Global\\AnimData.d2", "animdata.d2");
   animdata_load();
   add_fixture(BASE "\\AM\\COF\\AMTNHTH.cof", "compose/AMTNHTH.cof");
   add_fixture(BASE "\\AM\\HD\\AMHDLITTNHTH.dc6", "compose/HD.dc6");
   add_fixture(BASE "\\AM\\TR\\AMTRLITTNHTH.dc6", "compose/TR.dc6");

   params(&p, "AM", 0);
   p.mode = "TN";
   TEST_ASSERT_EQUAL_INT(1, compose_render(&p, &r));

   make_dir(DS1EDIT_TEST_OUT_DIR);
   snprintf(out, sizeof(out), "%s/compose", DS1EDIT_TEST_OUT_DIR);
   make_dir(out);
   snprintf(out, sizeof(out), "%s/compose/AMTNHTH.apng", DS1EDIT_TEST_OUT_DIR);
   remove(out);
   TEST_ASSERT_EQUAL_INT(1, compose_apng_write(&r, "AMTNHTH", out));

   one = r;
   one.frame_count = 1;
   snprintf(out, sizeof(out), "%s/compose/one_frame.apng", DS1EDIT_TEST_OUT_DIR);
   remove(out);
   TEST_ASSERT_EQUAL_INT(1, compose_apng_write(&one, "NOSUCHCOF", out));

   /* a failed write reports failure and leaves nothing behind */
   snprintf(out, sizeof(out), "%s/compose/missing_dir/x.apng", DS1EDIT_TEST_OUT_DIR);
   TEST_ASSERT_EQUAL_INT(0, compose_apng_write(&r, "AMTNHTH", out));
   one.frames = NULL;
   snprintf(out, sizeof(out), "%s/compose/null_frames.apng", DS1EDIT_TEST_OUT_DIR);
   TEST_ASSERT_EQUAL_INT(0, compose_apng_write(&one, "AMTNHTH", out));
   {
      unsigned char *frames_copy[3];
      COMPOSE_RENDER_RESULT_S partial = r;
      FILE *fp;
      frames_copy[0] = r.frames[0];
      frames_copy[1] = NULL;            /* a frame missing mid-sequence */
      frames_copy[2] = r.frames[2];
      partial.frames = frames_copy;
      snprintf(out, sizeof(out), "%s/compose/partial.apng", DS1EDIT_TEST_OUT_DIR);
      TEST_ASSERT_EQUAL_INT(0, compose_apng_write(&partial, "AMTNHTH", out));
      fp = fopen(out, "rb");
      if (fp != NULL)
         fclose(fp);
      TEST_ASSERT_NULL_MESSAGE(fp, "a failed APNG write left a file behind");
   }
   compose_render_free(&r);
}

int main(int argc, char **argv)
{
   (void) argc;
   (void) argv;
   UNITY_BEGIN();
   RUN_TEST(test_cof_priority_is_read_per_direction_and_frame);
   RUN_TEST(test_overlap_follows_priority_with_signed_offsets);
   RUN_TEST(test_direction_selects_its_own_frames_and_priority);
   RUN_TEST(test_palette_selection_changes_colours_not_indices);
   RUN_TEST(test_absent_optional_layer_and_duplicate_component);
   RUN_TEST(test_zero_loadable_components_fails_cleanly);
   RUN_TEST(test_zero_layer_cof_composes_to_nothing_without_error);
   RUN_TEST(test_dcc_layer_decodes_through_the_shared_seam);
   RUN_TEST(test_assumption_COMPOSE_BLEND_OPAQUE_layers_with_trans_flags);
   RUN_TEST(test_compose_exports_write_apng);
   return UNITY_END();
}
