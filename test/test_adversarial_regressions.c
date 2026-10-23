/*
 * Package E regressions: resource lifetime of the DS1 reader/free pair, plus
 * the minimized defects the adversarial campaign finds (each named after its
 * campaign case id).
 *
 * src/core/ds1.c is compiled into this translation unit *after* malloc,
 * calloc, realloc and free are redirected to a tracking allocator and dt1_del
 * to a recording hook. That lets the test say exactly which allocations the
 * production reader/free code left behind, and which tile-set slots
 * ds1_free() released, without a debug CRT or a sanitizer. Allocations made
 * by other translation units are not tracked and are not counted.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- tracking allocator (ds1.c only) ----------------------------------- */

#define TRACK_MAX 4096
static void *s_live[TRACK_MAX];
static int   s_live_n;
static int   s_dt1_del_calls;
static int   s_dt1_del_arg[64];

static void track_add(void *p)
{
   if (p != NULL && s_live_n < TRACK_MAX)
      s_live[s_live_n++] = p;
}

static void track_del(void *p)
{
   int i;
   for (i = 0; i < s_live_n; i++)
      if (s_live[i] == p)
      {
         s_live[i] = s_live[--s_live_n];
         return;
      }
}

static void *trk_malloc(size_t n)           { void *p = malloc(n);     track_add(p); return p; }
static void *trk_calloc(size_t n, size_t m) { void *p = calloc(n, m);  track_add(p); return p; }
static void *trk_realloc(void *o, size_t n)
{
   void *p = realloc(o, n);
   if (p != NULL)
   {
      track_del(o);
      track_add(p);
   }
   return p;
}
static void trk_free(void *p)               { track_del(p); free(p); }

static int trk_dt1_del(int i)
{
   if (s_dt1_del_calls < (int) (sizeof(s_dt1_del_arg) / sizeof(s_dt1_del_arg[0])))
      s_dt1_del_arg[s_dt1_del_calls] = i;
   s_dt1_del_calls++;
   return 0;
}

#define malloc  trk_malloc
#define calloc  trk_calloc
#define realloc trk_realloc
#define free    trk_free
#define dt1_del trk_dt1_del
#include "../src/core/ds1.c"
#undef malloc
#undef calloc
#undef realloc
#undef free
#undef dt1_del

#include "unity/unity.h"
#include "support/editor_test_state.h"
#include "support/ds1_fixture.h"
#include "support/test_tmp.h"

static char path[512];

void setUp(void)
{
   TEST_ASSERT_TRUE(editor_test_state_init(0, 1, 1));
   editor_test_state_path(path, sizeof(path), "life.ds1");
   s_live_n = 0;
   s_dt1_del_calls = 0;
}

void tearDown(void)
{
   editor_test_state_destroy();
}

static void read_default(int version, long truncate_to)
{
   FX_SPEC s;
   char err[256];
   int rc;

   fx_default(&s, version);
   TEST_ASSERT_TRUE(fx_write(&s, path, truncate_to));
   rc = ds1_read_checked(path, 0, 0, 0, err, sizeof(err));
   if (truncate_to < 0)
      TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, err);
}

/* ---- ds1_free ------------------------------------------------------------ */

void test_ds1_free_releases_everything_the_reader_allocated(void)
{
   int version;
   static const int versions[] = { 3, 13, 18 };

   for (version = 0; version < 3; version++)
   {
      s_live_n = 0;
      read_default(versions[version], -1);
      TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, s_live_n, "the reader allocated nothing?");
      /* the object, object-undo and drawing-order arrays are among them */
      ds1_free(0);
      TEST_ASSERT_EQUAL_INT_MESSAGE(0, s_live_n, "ds1_free() left allocations behind");
   }
}

void test_ds1_free_can_be_repeated_across_map_switches(void)
{
   int i;
   read_default(18, -1);
   ds1_free(0);
   for (i = 0; i < 5; i++)
   {
      read_default(18, -1);
      ds1_free(0);
      TEST_ASSERT_EQUAL_INT(0, s_live_n);
   }
}

void test_ds1_free_releases_only_tilesets_the_map_took(void)
{
   int i, taken = 0;

   read_default(18, -1);
   /* a freshly read map owns no tile sets: every slot must say so */
   for (i = 0; i < DT1_IN_DS1_MAX; i++)
      TEST_ASSERT_EQUAL_INT_MESSAGE(-1, glb_ds1[0].dt1_idx[i], "unused dt1 slot is not -1");
   glb_ds1[0].dt1_idx[0] = 5;
   glb_ds1[0].dt1_idx[3] = 7;
   s_dt1_del_calls = 0;
   ds1_free(0);
   for (i = 0; i < s_dt1_del_calls && i < 64; i++)
      if (s_dt1_del_arg[i] >= 0)
         taken++;
   TEST_ASSERT_EQUAL_INT_MESSAGE(2, taken,
      "ds1_free() released a tile-set slot the map never took");
}

/* ---- a rejected map leaves nothing behind --------------------------------- */

void test_rejected_map_leaves_no_allocation_and_an_empty_slot(void)
{
   FX_SPEC s;
   char err[256];

   fx_default(&s, 18);
   TEST_ASSERT_TRUE(fx_write(&s, path, 60));
   TEST_ASSERT_EQUAL_INT(-1, ds1_read_checked(path, 0, 0, 0, err, sizeof(err)));
   TEST_ASSERT_EQUAL_INT(0, s_live_n);
   TEST_ASSERT_EQUAL_CHAR(0, glb_ds1[0].name[0]);
   /* and the slot is immediately usable for the next map */
   read_default(18, -1);
   ds1_free(0);
   TEST_ASSERT_EQUAL_INT(0, s_live_n);
}

int main(int argc, char **argv)
{
   pkgc_test_filter = argc > 1 ? argv[1] : NULL;
   UNITY_BEGIN();
   PKGC_RUN(test_ds1_free_releases_everything_the_reader_allocated);
   PKGC_RUN(test_ds1_free_can_be_repeated_across_map_switches);
   PKGC_RUN(test_ds1_free_releases_only_tilesets_the_map_took);
   PKGC_RUN(test_rejected_map_leaves_no_allocation_and_an_empty_slot);
   return UNITY_END();
}
