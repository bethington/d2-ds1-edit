/* Input edge-detection and modal isolation.
 *
 * These two rules are what the Allegro 4 spin-waits used to enforce by
 * blocking, and both were broken in the port at some point:
 *
 *   key_hit() must be true only on the frame a key goes down, so a command
 *   fires once per press instead of sixty times a second while held.
 *
 *   input_suppress_held() must make a held key read as not-held until it is
 *   pressed again, so the keypress that opens a dialog cannot immediately
 *   trigger that dialog's own shortcut. Escape opened the quit menu and
 *   dismissed it on the same frame for want of this.
 *
 * ALLEGRO_KEYBOARD_STATE is a plain bitfield, so a state can be built by hand
 * here and no display, keyboard driver or window is needed.
 */

#include <string.h>
#include "unity/unity.h"

#include <allegro5/allegro.h>
#include "structs.h"
#include "ui/input.h"

/* The globals the input layer and compat.h operate on. Provided here rather
   than linking globals.c, which drags in the whole editor. */
ALLEGRO_KEYBOARD_STATE a5_kb_state;
ALLEGRO_MOUSE_STATE    a5_ms_state;
ALLEGRO_DISPLAY       *a5_display     = NULL;
ALLEGRO_EVENT_QUEUE   *a5_event_queue = NULL;

/* Pulled in by input.c's resize branch, which these tests never reach. */
CONFIG_S glb_config;
void ds1edit_recreate_render_targets(void) { }

/* Mirrors compat.h's key_pressed() without pulling in the editor's headers. */
static bool key_is_down(int k)
{
   return al_key_down(&a5_kb_state, k) && !a5_key_suppressed[k];
}

static void hold_key(int keycode)
{
   a5_kb_state.__key_down__internal__[keycode / 32] |= 1u << (keycode % 32);
}

static void release_key(int keycode)
{
   a5_kb_state.__key_down__internal__[keycode / 32] &= ~(1u << (keycode % 32));
}

static ALLEGRO_EVENT key_down_event(int keycode)
{
   ALLEGRO_EVENT ev;
   memset(&ev, 0, sizeof(ev));
   ev.keyboard.type = ALLEGRO_EVENT_KEY_DOWN;
   ev.keyboard.keycode = keycode;
   return ev;
}

void setUp(void)
{
   memset(&a5_kb_state, 0, sizeof(a5_kb_state));
   memset(a5_key_hit, 0, sizeof(a5_key_hit));
   memset(a5_key_suppressed, 0, sizeof(a5_key_suppressed));
   memset(a5_mb_hit, 0, sizeof(a5_mb_hit));
}

void tearDown(void)
{
}

/* ---- key_hit: once per press, not once per frame ---------------------- */

void test_key_hit_fires_on_the_frame_the_key_goes_down(void)
{
   ALLEGRO_EVENT ev = key_down_event(ALLEGRO_KEY_U);

   input_begin_frame();
   input_note_event(&ev);

   TEST_ASSERT_TRUE(key_hit(ALLEGRO_KEY_U));
}

void test_key_hit_is_clear_on_later_frames_while_still_held(void)
{
   ALLEGRO_EVENT ev = key_down_event(ALLEGRO_KEY_U);

   input_begin_frame();
   input_note_event(&ev);
   hold_key(ALLEGRO_KEY_U);          /* finger still down */
   TEST_ASSERT_TRUE(key_hit(ALLEGRO_KEY_U));

   /* Next frame: no new KEY_DOWN, because auto-repeat arrives as KEY_CHAR. */
   input_begin_frame();
   TEST_ASSERT_FALSE(key_hit(ALLEGRO_KEY_U));

   /* ...but it is still held, which is a different question. */
   TEST_ASSERT_TRUE(key_is_down(ALLEGRO_KEY_U));
}

void test_key_hit_fires_again_after_release_and_repress(void)
{
   ALLEGRO_EVENT ev = key_down_event(ALLEGRO_KEY_U);

   input_begin_frame();
   input_note_event(&ev);
   TEST_ASSERT_TRUE(key_hit(ALLEGRO_KEY_U));

   input_begin_frame();
   TEST_ASSERT_FALSE(key_hit(ALLEGRO_KEY_U));

   input_begin_frame();
   input_note_event(&ev);
   TEST_ASSERT_TRUE(key_hit(ALLEGRO_KEY_U));
}

void test_mouse_hit_follows_the_same_rule(void)
{
   ALLEGRO_EVENT ev;
   memset(&ev, 0, sizeof(ev));
   ev.mouse.type = ALLEGRO_EVENT_MOUSE_BUTTON_DOWN;
   ev.mouse.button = 1;

   input_begin_frame();
   input_note_event(&ev);
   TEST_ASSERT_TRUE(mouse_hit(1));
   TEST_ASSERT_FALSE(mouse_hit(2));

   input_begin_frame();
   TEST_ASSERT_FALSE(mouse_hit(1));
}

/* ---- modal isolation: the Escape bug ---------------------------------- */

void test_suppress_held_hides_a_key_that_is_still_down(void)
{
   hold_key(ALLEGRO_KEY_ESCAPE);
   TEST_ASSERT_TRUE(key_is_down(ALLEGRO_KEY_ESCAPE));

   input_suppress_held();

   /* Physically still down, but the dialog we just opened must not see it. */
   TEST_ASSERT_TRUE(al_key_down(&a5_kb_state, ALLEGRO_KEY_ESCAPE));
   TEST_ASSERT_FALSE(key_is_down(ALLEGRO_KEY_ESCAPE));
}

void test_suppressed_key_stays_hidden_until_genuinely_repressed(void)
{
   ALLEGRO_EVENT ev = key_down_event(ALLEGRO_KEY_ESCAPE);

   hold_key(ALLEGRO_KEY_ESCAPE);
   input_suppress_held();
   TEST_ASSERT_FALSE(key_is_down(ALLEGRO_KEY_ESCAPE));

   /* Holding it for many frames changes nothing. */
   input_begin_frame();
   TEST_ASSERT_FALSE(key_is_down(ALLEGRO_KEY_ESCAPE));
   input_begin_frame();
   TEST_ASSERT_FALSE(key_is_down(ALLEGRO_KEY_ESCAPE));

   /* Release, then press again: a real new press, and it counts. */
   release_key(ALLEGRO_KEY_ESCAPE);
   input_begin_frame();
   input_note_event(&ev);
   hold_key(ALLEGRO_KEY_ESCAPE);

   TEST_ASSERT_TRUE(key_hit(ALLEGRO_KEY_ESCAPE));
   TEST_ASSERT_TRUE(key_is_down(ALLEGRO_KEY_ESCAPE));
}

void test_suppress_held_leaves_keys_that_were_up_alone(void)
{
   hold_key(ALLEGRO_KEY_ESCAPE);
   input_suppress_held();

   /* Enter was never down, so nothing about it should be masked. */
   hold_key(ALLEGRO_KEY_ENTER);
   TEST_ASSERT_TRUE(key_is_down(ALLEGRO_KEY_ENTER));
   TEST_ASSERT_FALSE(key_is_down(ALLEGRO_KEY_ESCAPE));
}

void test_suppress_held_clears_pending_edges(void)
{
   ALLEGRO_EVENT ev = key_down_event(ALLEGRO_KEY_ESCAPE);

   input_begin_frame();
   input_note_event(&ev);
   TEST_ASSERT_TRUE(key_hit(ALLEGRO_KEY_ESCAPE));

   /* Entering a modal must not hand it the edge that opened it. */
   input_suppress_held();
   TEST_ASSERT_FALSE(key_hit(ALLEGRO_KEY_ESCAPE));
}

int main(void)
{
   UNITY_BEGIN();
   RUN_TEST(test_key_hit_fires_on_the_frame_the_key_goes_down);
   RUN_TEST(test_key_hit_is_clear_on_later_frames_while_still_held);
   RUN_TEST(test_key_hit_fires_again_after_release_and_repress);
   RUN_TEST(test_mouse_hit_follows_the_same_rule);
   RUN_TEST(test_suppress_held_hides_a_key_that_is_still_down);
   RUN_TEST(test_suppressed_key_stays_hidden_until_genuinely_repressed);
   RUN_TEST(test_suppress_held_leaves_keys_that_were_up_alone);
   RUN_TEST(test_suppress_held_clears_pending_edges);
   return UNITY_END();
}
