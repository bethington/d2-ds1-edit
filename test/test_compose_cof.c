#include <stdlib.h>
#include <string.h>

#include "unity/unity.h"

#include "core/compose_cof.h"

void setUp(void) {}
void tearDown(void) {}

/* Build a synthetic COF byte buffer. We construct one that has:
 *   layer_count = 3 (HD, TR, LG with composit indices 0, 1, 2)
 *   frames_per_dir = 2
 *   direction_count = 4
 *   version = 20
 *   trans_a/b varied per layer so we can verify layer struct
 *   priority table = simple 1, 2, 0 ordering for every (dir, frame). */

/* COF header is 28 bytes: 4 bytes (lay/fpd/dirs/version) + 24 bytes
 * of bounds + anim_speed that we don't parse. Earlier this fixture
 * (and the parser) used 29 here, off by one, which silently accepted
 * synthetic data but rejected real D2 COFs. The size matches the
 * working anim_load_cof in core/cof.c. */
static unsigned char *build_synthetic_cof(int *out_len)
{
   long header = 4 + 24;
   long per_layer = 9;
   int layers = 3;
   int fpd = 2;
   int dirs = 4;
   long prio_size = (long) dirs * fpd * layers;
   long total = header + per_layer * layers + fpd + prio_size;
   unsigned char *buf = (unsigned char *) calloc((size_t) total, 1);
   long pos = 0;
   int d, f;

   if (buf == NULL) return NULL;

   buf[0] = (unsigned char) layers;
   buf[1] = (unsigned char) fpd;
   buf[2] = (unsigned char) dirs;
   buf[3] = 20;
   pos = header;

   /* Layer 0: HD, with weapon_class "HTH " */
   buf[pos + 0] = 0;       /* composit_index = HD */
   buf[pos + 1] = 0x10;    /* shadow_a */
   buf[pos + 2] = 0x20;    /* shadow_b */
   buf[pos + 3] = 0;       /* trans_a */
   buf[pos + 4] = 0;       /* trans_b */
   buf[pos + 5] = 'H'; buf[pos + 6] = 'T'; buf[pos + 7] = 'H'; buf[pos + 8] = ' ';
   pos += 9;

   /* Layer 1: TR */
   buf[pos + 0] = 1;       /* composit_index = TR */
   buf[pos + 1] = 0x11;
   buf[pos + 2] = 0x21;
   buf[pos + 3] = 1;
   buf[pos + 4] = 1;
   buf[pos + 5] = '1'; buf[pos + 6] = 'H'; buf[pos + 7] = 'S'; buf[pos + 8] = ' ';
   pos += 9;

   /* Layer 2: LG */
   buf[pos + 0] = 2;       /* composit_index = LG */
   buf[pos + 1] = 0x12;
   buf[pos + 2] = 0x22;
   buf[pos + 3] = 2;
   buf[pos + 4] = 2;
   buf[pos + 5] = 0;   buf[pos + 6] = 0;   buf[pos + 7] = 0;   buf[pos + 8] = 0;
   pos += 9;

   /* frame flags */
   buf[pos + 0] = 0; buf[pos + 1] = 0;
   pos += fpd;

   /* priority: for each (dir, frame), order is layers 1, 2, 0
    * i.e., TR drawn first, LG second, HD last (top). */
   for (d = 0; d < dirs; d++)
   {
      for (f = 0; f < fpd; f++)
      {
         buf[pos + 0] = 1;
         buf[pos + 1] = 2;
         buf[pos + 2] = 0;
         pos += layers;
      }
   }

   *out_len = (int) total;
   return buf;
}

static void test_parse_basic_header(void)
{
   COMPOSE_COF_S cof;
   int len;
   unsigned char *bytes = build_synthetic_cof(&len);
   TEST_ASSERT_NOT_NULL(bytes);

   TEST_ASSERT_EQUAL_INT(1, compose_cof_parse(bytes, len, &cof));
   TEST_ASSERT_EQUAL_INT(3, cof.layer_count);
   TEST_ASSERT_EQUAL_INT(2, cof.frames_per_dir);
   TEST_ASSERT_EQUAL_INT(4, cof.direction_count);
   TEST_ASSERT_EQUAL_INT(20, cof.version);

   compose_cof_free(&cof);
   free(bytes);
}

static void test_parse_layer_metadata(void)
{
   COMPOSE_COF_S cof;
   int len;
   unsigned char *bytes = build_synthetic_cof(&len);
   compose_cof_parse(bytes, len, &cof);

   /* Layer 0 (HD): trim weapon_class "HTH " -> "HTH" */
   TEST_ASSERT_EQUAL_INT(0, cof.layers[0].composit_index);
   TEST_ASSERT_EQUAL_INT(0x10, cof.layers[0].shadow_a);
   TEST_ASSERT_EQUAL_INT(0x20, cof.layers[0].shadow_b);
   TEST_ASSERT_EQUAL_STRING("HTH", cof.layers[0].weapon_class);

   /* Layer 1 (TR) */
   TEST_ASSERT_EQUAL_INT(1, cof.layers[1].composit_index);
   TEST_ASSERT_EQUAL_INT(1, cof.layers[1].trans_a);
   TEST_ASSERT_EQUAL_STRING("1HS", cof.layers[1].weapon_class);

   /* Layer 2 (LG) has empty weapon_class */
   TEST_ASSERT_EQUAL_INT(2, cof.layers[2].composit_index);
   TEST_ASSERT_EQUAL_STRING("", cof.layers[2].weapon_class);

   compose_cof_free(&cof);
   free(bytes);
}

static void test_priority_at(void)
{
   COMPOSE_COF_S cof;
   int len;
   unsigned char *bytes = build_synthetic_cof(&len);
   compose_cof_parse(bytes, len, &cof);

   /* Direction 0, frame 0, order 0 should be layer 1 (TR). */
   TEST_ASSERT_EQUAL_INT(1, compose_cof_priority_at(&cof, 0, 0, 0));
   TEST_ASSERT_EQUAL_INT(2, compose_cof_priority_at(&cof, 0, 0, 1));
   TEST_ASSERT_EQUAL_INT(0, compose_cof_priority_at(&cof, 0, 0, 2));

   /* Same pattern repeats across directions and frames in our fixture. */
   TEST_ASSERT_EQUAL_INT(1, compose_cof_priority_at(&cof, 3, 1, 0));
   TEST_ASSERT_EQUAL_INT(0, compose_cof_priority_at(&cof, 3, 1, 2));

   /* Out-of-range queries return -1. */
   TEST_ASSERT_EQUAL_INT(-1, compose_cof_priority_at(&cof, -1, 0, 0));
   TEST_ASSERT_EQUAL_INT(-1, compose_cof_priority_at(&cof, 0, 99, 0));
   TEST_ASSERT_EQUAL_INT(-1, compose_cof_priority_at(&cof, 0, 0, 99));

   compose_cof_free(&cof);
   free(bytes);
}

static void test_parse_rejects_truncated_buffer(void)
{
   COMPOSE_COF_S cof;
   int len;
   unsigned char *bytes = build_synthetic_cof(&len);

   /* Truncate to just the header. */
   TEST_ASSERT_EQUAL_INT(0, compose_cof_parse(bytes, 4, &cof));
   /* Truncate before priority table. */
   TEST_ASSERT_EQUAL_INT(0, compose_cof_parse(bytes, len - 5, &cof));

   free(bytes);
}

static void test_parse_rejects_absurd_dimensions(void)
{
   COMPOSE_COF_S cof;
   unsigned char buf[64];
   memset(buf, 0, sizeof(buf));
   buf[0] = 99;  /* layer_count too big */
   buf[1] = 1;
   buf[2] = 1;
   TEST_ASSERT_EQUAL_INT(0, compose_cof_parse(buf, sizeof(buf), &cof));

   buf[0] = 1;
   buf[1] = 0;   /* fpd zero */
   TEST_ASSERT_EQUAL_INT(0, compose_cof_parse(buf, sizeof(buf), &cof));

   buf[1] = 1;
   buf[2] = 0;   /* directions zero */
   TEST_ASSERT_EQUAL_INT(0, compose_cof_parse(buf, sizeof(buf), &cof));
}

static void test_parse_rejects_bad_composit_index(void)
{
   COMPOSE_COF_S cof;
   unsigned char buf[256];
   memset(buf, 0, sizeof(buf));
   buf[0] = 1;
   buf[1] = 1;
   buf[2] = 1;
   /* layer block at offset 28 (header size): composit_index = 99
    * (out of range). */
   buf[28] = 99;
   TEST_ASSERT_EQUAL_INT(0, compose_cof_parse(buf, sizeof(buf), &cof));
}

/* Regression: real D2 COFs (e.g. NEWLHTH.cof) have 9 layers, fpd 8,
 * 16 directions. With the buggy 29-byte header parser, layer reads
 * were misaligned and the parse always failed. This test asserts that
 * a real-shaped header parses cleanly. */
static void test_parse_real_d2_dimensions(void)
{
   COMPOSE_COF_S cof;
   long header = 4 + 24;
   long per_layer = 9;
   int layers = 9;
   int fpd = 8;
   int dirs = 16;
   long prio = (long) dirs * fpd * layers;
   long total = header + per_layer * layers + fpd + prio;
   unsigned char *buf = (unsigned char *) calloc((size_t) total, 1);
   int i;
   TEST_ASSERT_NOT_NULL(buf);

   buf[0] = (unsigned char) layers;
   buf[1] = (unsigned char) fpd;
   buf[2] = (unsigned char) dirs;
   buf[3] = 20;
   /* layer composit indices 0..8 (HD, TR, LG, RA, LA, RH, LH, SH, S1) */
   for (i = 0; i < layers; i++)
      buf[header + (long) i * per_layer] = (unsigned char) i;

   TEST_ASSERT_EQUAL_INT(1, compose_cof_parse(buf, (long) total, &cof));
   TEST_ASSERT_EQUAL_INT(layers, cof.layer_count);
   TEST_ASSERT_EQUAL_INT(fpd,    cof.frames_per_dir);
   TEST_ASSERT_EQUAL_INT(dirs,   cof.direction_count);

   compose_cof_free(&cof);
   free(buf);
}

static void test_parse_rejects_null_inputs(void)
{
   COMPOSE_COF_S cof;
   unsigned char buf[64];
   memset(buf, 0, sizeof(buf));
   TEST_ASSERT_EQUAL_INT(0, compose_cof_parse(NULL, 64, &cof));
   TEST_ASSERT_EQUAL_INT(0, compose_cof_parse(buf, 64, NULL));
   TEST_ASSERT_EQUAL_INT(0, compose_cof_parse(buf, 0, &cof));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_parse_basic_header);
   RUN_TEST(test_parse_layer_metadata);
   RUN_TEST(test_priority_at);
   RUN_TEST(test_parse_rejects_truncated_buffer);
   RUN_TEST(test_parse_rejects_absurd_dimensions);
   RUN_TEST(test_parse_rejects_bad_composit_index);
   RUN_TEST(test_parse_rejects_null_inputs);
   RUN_TEST(test_parse_real_d2_dimensions);
   return UNITY_END();
}
