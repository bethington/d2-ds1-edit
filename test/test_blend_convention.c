/* Translucent GPU-path draws must blend as src*a + dst*(1-a).
 *
 * The editor draws every translucent tint under ALPHA / INVERSE_ALPHA (set by
 * a5_clear on the target). A tint built as al_map_rgba_f(a,a,a,a) is a
 * PREMULTIPLIED colour, so under that blender the source is weighted by a
 * twice and the "50 % tint" came out as 0.25*src + 0.5*dst. These tests draw
 * a known source over a known destination through the real compat.h helpers
 * and check the arithmetic of every translucent mode.
 *
 * Memory bitmaps are enough: the blender maths is the same as on the GPU, and
 * no display is created. */
#include <string.h>

#include "unity/unity.h"
#include "ui/compat.h"

/* Globals that compat.h declares extern. */
RGBA_PALETTE *a5_current_palette = NULL;
ALLEGRO_DISPLAY *a5_display = NULL;
ALLEGRO_FONT *a5_font = NULL;
ALLEGRO_EVENT_QUEUE *a5_event_queue = NULL;
ALLEGRO_TIMER *a5_tick_timer = NULL;
ALLEGRO_TIMER *a5_fps_timer = NULL;
float a5_trans_alpha = 0.5f;
unsigned a5_gpu_guard_hits = 0;
void a5_gpu_guard_report(const char *what, const char *file, int line,
                         const ALLEGRO_BITMAP *bmp)
{
   (void)what; (void)file; (void)line; (void)bmp;
   a5_gpu_guard_hits++;
}

#define SR 71
#define SG 66
#define SB 57
#define DR 200
#define DG 100
#define DB 40

static ALLEGRO_BITMAP *src_bmp;
static ALLEGRO_BITMAP *dst_bmp;

/* Same state a5_clear leaves on every editor target. */
static void reset_dst(void)
{
   al_set_target_bitmap(dst_bmp);
   al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_ZERO);
   al_clear_to_color(al_map_rgba(DR, DG, DB, 255));
   al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
}

void setUp(void)
{
   al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);
   src_bmp = al_create_bitmap(4, 4);
   dst_bmp = al_create_bitmap(4, 4);
   al_set_new_bitmap_flags(0);

   al_set_target_bitmap(src_bmp);
   al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_ZERO);
   al_clear_to_color(al_map_rgba(SR, SG, SB, 255));

   reset_dst();
}

void tearDown(void)
{
   al_set_target_bitmap(NULL);
   al_destroy_bitmap(src_bmp);
   al_destroy_bitmap(dst_bmp);
}

static void check_pixel(int er, int eg, int eb)
{
   unsigned char r, g, b, a;
   al_set_target_bitmap(dst_bmp);
   al_unmap_rgba(al_get_pixel(dst_bmp, 1, 1), &r, &g, &b, &a);
   TEST_ASSERT_INT_WITHIN(1, er, r);
   TEST_ASSERT_INT_WITHIN(1, eg, g);
   TEST_ASSERT_INT_WITHIN(1, eb, b);
   (void)a; /* destination alpha is not part of the colour convention */
}

static int mix(int s, int d, float a)
{
   return (int)(s * a + d * (1.0f - a) + 0.5f);
}

static void check_alpha(float a)
{
   check_pixel(mix(SR, DR, a), mix(SG, DG, a), mix(SB, DB, a));
}

static void check_blender_restored(void)
{
   int op, src, dst;
   al_get_blender(&op, &src, &dst);
   TEST_ASSERT_EQUAL_INT(ALLEGRO_ADD, op);
   TEST_ASSERT_EQUAL_INT(ALLEGRO_ALPHA, src);
   TEST_ASSERT_EQUAL_INT(ALLEGRO_INVERSE_ALPHA, dst);
}

static void test_trans_sprite_alpha(void)
{
   const float levels[3] = { 0.25f, 0.50f, 0.75f };
   int i;
   for (i = 0; i < 3; i++)
   {
      reset_dst();
      a5_trans_alpha = levels[i];
      a5_draw_trans_sprite(dst_bmp, src_bmp, 0, 0);
      check_alpha(levels[i]);
   }
}

static void test_blended_sprite_alpha_modes(void)
{
   const float levels[4] = { 0.25f, 0.50f, 0.75f, 0.50f }; /* trans_b 0,1,2, default */
   const int modes[4] = { 0, 1, 2, 5 };
   int i;
   for (i = 0; i < 4; i++)
   {
      reset_dst();
      a5_draw_scaled_blended_sprite(dst_bmp, src_bmp, 0, 0, 1, modes[i]);
      check_alpha(levels[i]);
   }
}

static void test_blend_additive(void)
{
   int r = SR + DR, g = SG + DG, b = SB + DB;
   a5_draw_scaled_blended_sprite(dst_bmp, src_bmp, 0, 0, 1, 3);
   check_pixel(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
   check_blender_restored();
}

static void test_blend_multiply(void)
{
   a5_draw_scaled_blended_sprite(dst_bmp, src_bmp, 0, 0, 1, 4);
   check_pixel(SR * DR / 255, SG * DG / 255, SB * DB / 255);
   check_blender_restored();
}

static void test_blend_screen(void)
{
   /* ONE, INVERSE_SRC_COLOR: src + dst*(1-src) */
   a5_draw_scaled_blended_sprite(dst_bmp, src_bmp, 0, 0, 1, 6);
   check_pixel((int)(SR + DR * (255 - SR) / 255.0f + 0.5f),
               (int)(SG + DG * (255 - SG) / 255.0f + 0.5f),
               (int)(SB + DB * (255 - SB) / 255.0f + 0.5f));
   check_blender_restored();
}

static void test_shadow_darkens_by_level(void)
{
   /* level 16/32: dst * 0.5, source colour irrelevant. */
   a5_draw_shadow_sprite(dst_bmp, src_bmp, 0, 0, 16);
   check_pixel(DR / 2, DG / 2, DB / 2);
}

int main(void)
{
   TEST_ASSERT_TRUE(al_init());
   UNITY_BEGIN();
   RUN_TEST(test_trans_sprite_alpha);
   RUN_TEST(test_blended_sprite_alpha_modes);
   RUN_TEST(test_blend_additive);
   RUN_TEST(test_blend_multiply);
   RUN_TEST(test_blend_screen);
   RUN_TEST(test_shadow_darkens_by_level);
   return UNITY_END();
}
