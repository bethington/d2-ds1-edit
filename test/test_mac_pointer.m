/* The macOS pointer: NSEvents in, mouse state and Allegro events out.
 *
 * Needs a window, so it is labelled `display`, and exits 77 (CTest's skip) when
 * the host will not give it one. Events are posted to the application rather
 * than to the system, so the real pointer is not touched. No mouse driver is
 * installed, which also proves a click does not trip Allegro's own view code.
 */

#import <Cocoa/Cocoa.h>
#include <string.h>
#include "unity/unity.h"

#include <allegro5/allegro.h>
#include <allegro5/allegro_osx.h>
#include "ui/compat.h"

#define WIN_W 400
#define WIN_H 300

ALLEGRO_MOUSE_STATE a5_ms_state;

static ALLEGRO_DISPLAY     *s_display;
static ALLEGRO_EVENT_QUEUE *s_queue;

/* Post a mouse event at a fraction of the content view; fy runs down
   from the top of the view, and past 0 or 1 it leaves the view. */
static void post_mouse(NSEventType type, double fx, double fy)
{
   NSWindow *win = al_osx_get_window(s_display);

   dispatch_async(dispatch_get_main_queue(), ^{
      NSRect b = win.contentView.bounds;
      NSPoint p = NSMakePoint(b.size.width * fx, b.size.height * (1.0 - fy));
      NSEvent *e = [NSEvent mouseEventWithType:type
                                      location:p
                                 modifierFlags:0
                                     timestamp:NSProcessInfo.processInfo.systemUptime
                                  windowNumber:win.windowNumber
                                       context:nil
                                   eventNumber:0
                                    clickCount:1
                                      pressure:1.0f];
      [NSApp postEvent:e atStart:NO];
   });
}

/* Next event from the pointer, or fail. */
static ALLEGRO_EVENT wait_event(void)
{
   ALLEGRO_EVENT ev;

   memset(&ev, 0, sizeof(ev));
   TEST_ASSERT_TRUE_MESSAGE(al_wait_for_event_timed(s_queue, &ev, 2.0f),
                            "no pointer event arrived");
   return ev;
}

static void assert_no_event(void)
{
   ALLEGRO_EVENT ev;
   TEST_ASSERT_FALSE(al_wait_for_event_timed(s_queue, &ev, 0.2f));
}

/* Put the pointer somewhere known, and wait to see it land: the first event
   after the window opens can take a while to come through the run loop. */
static void settle_at(double fx, double fy)
{
   ALLEGRO_EVENT ev;

   post_mouse(NSEventTypeMouseMoved, fx, fy);
   al_wait_for_event_timed(s_queue, &ev, 2.0f);
   al_flush_event_queue(s_queue);
}

void setUp(void)
{
   TEST_ASSERT_TRUE(ds1_mac_pointer_start(s_display));
   a5_register_mouse_events(s_queue);
   settle_at(0.25, 0.25);
}

void tearDown(void)
{
   ds1_mac_pointer_stop();
}

void test_a_move_updates_state_and_emits_axes(void)
{
   ALLEGRO_EVENT ev;

   post_mouse(NSEventTypeMouseMoved, 0.5, 0.5);
   ev = wait_event();

   TEST_ASSERT_EQUAL_INT(ALLEGRO_EVENT_MOUSE_AXES, ev.type);
   TEST_ASSERT_INT_WITHIN(1, WIN_W / 2, ev.mouse.x);
   TEST_ASSERT_INT_WITHIN(1, WIN_H / 2, ev.mouse.y);

   a5_get_mouse_state(&a5_ms_state);
   TEST_ASSERT_INT_WITHIN(1, WIN_W / 2, a5_mouse_x);
   TEST_ASSERT_INT_WITHIN(1, WIN_H / 2, a5_mouse_y);
}

void test_y_runs_down_from_the_top_of_the_window(void)
{
   ALLEGRO_EVENT ev;

   post_mouse(NSEventTypeMouseMoved, 0.5, 0.1);
   ev = wait_event();

   TEST_ASSERT_INT_WITHIN(1, WIN_H / 10, ev.mouse.y);
}

void test_left_button_goes_down_and_up(void)
{
   ALLEGRO_EVENT ev;

   post_mouse(NSEventTypeLeftMouseDown, 0.25, 0.25);
   ev = wait_event();
   TEST_ASSERT_EQUAL_INT(ALLEGRO_EVENT_MOUSE_BUTTON_DOWN, ev.type);
   TEST_ASSERT_EQUAL_UINT(1, ev.mouse.button);
   a5_get_mouse_state(&a5_ms_state);
   TEST_ASSERT_EQUAL_INT(1, a5_mouse_b);

   post_mouse(NSEventTypeLeftMouseUp, 0.25, 0.25);
   ev = wait_event();
   TEST_ASSERT_EQUAL_INT(ALLEGRO_EVENT_MOUSE_BUTTON_UP, ev.type);
   a5_get_mouse_state(&a5_ms_state);
   TEST_ASSERT_EQUAL_INT(0, a5_mouse_b);
}

void test_right_button_is_button_two(void)
{
   ALLEGRO_EVENT ev;

   post_mouse(NSEventTypeRightMouseDown, 0.25, 0.25);
   ev = wait_event();
   TEST_ASSERT_EQUAL_UINT(2, ev.mouse.button);
   a5_get_mouse_state(&a5_ms_state);
   TEST_ASSERT_EQUAL_INT(2, a5_mouse_b);

   post_mouse(NSEventTypeRightMouseUp, 0.25, 0.25);
   wait_event();
}

void test_a_click_on_the_title_bar_is_not_a_click_on_the_map(void)
{
   post_mouse(NSEventTypeLeftMouseDown, 0.5, -0.02);

   assert_no_event();
   a5_get_mouse_state(&a5_ms_state);
   TEST_ASSERT_EQUAL_INT(0, a5_mouse_b);

   post_mouse(NSEventTypeLeftMouseUp, 0.5, -0.02);
   assert_no_event();
}

void test_a_move_outside_the_view_is_ignored(void)
{
   post_mouse(NSEventTypeMouseMoved, 0.5, -0.02);

   assert_no_event();
   a5_get_mouse_state(&a5_ms_state);
   TEST_ASSERT_INT_WITHIN(1, WIN_W / 4, a5_mouse_x);
}

void test_a_drag_outside_the_view_is_pinned_to_the_edge(void)
{
   post_mouse(NSEventTypeLeftMouseDown, 0.25, 0.25);
   wait_event();

   post_mouse(NSEventTypeLeftMouseDragged, 2.0, 0.25);
   wait_event();
   a5_get_mouse_state(&a5_ms_state);
   TEST_ASSERT_EQUAL_INT(WIN_W - 1, a5_mouse_x);

   post_mouse(NSEventTypeLeftMouseUp, 2.0, 0.25);
   wait_event();
}

void test_a_plain_move_clears_a_button_whose_release_was_lost(void)
{
   post_mouse(NSEventTypeLeftMouseDown, 0.25, 0.25);
   wait_event();

   post_mouse(NSEventTypeMouseMoved, 0.5, 0.5);
   wait_event();
   a5_get_mouse_state(&a5_ms_state);
   TEST_ASSERT_EQUAL_INT(0, a5_mouse_b);
}

void test_the_scroll_wheel_moves_z(void)
{
   NSWindow *win = al_osx_get_window(s_display);
   ALLEGRO_EVENT ev;

   dispatch_async(dispatch_get_main_queue(), ^{
      NSRect f = [win convertRectToScreen:win.contentLayoutRect];
      CGEventRef cg = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitLine, 1, 3);
      CGEventSetLocation(cg, CGPointMake(NSMidX(f), NSScreen.screens[0].frame.size.height - NSMidY(f)));
      [NSApp postEvent:[NSEvent eventWithCGEvent:cg] atStart:NO];
      CFRelease(cg);
   });

   ev = wait_event();
   TEST_ASSERT_EQUAL_INT(ALLEGRO_EVENT_MOUSE_AXES, ev.type);
   TEST_ASSERT_GREATER_THAN_INT(0, ev.mouse.dz);
   a5_get_mouse_state(&a5_ms_state);
   TEST_ASSERT_EQUAL_INT(ev.mouse.z, a5_mouse_z);
}

void test_the_helpers_report_a_pointer_while_it_runs(void)
{
   TEST_ASSERT_TRUE(a5_mouse_available());
   ds1_mac_pointer_stop();
   TEST_ASSERT_FALSE(a5_mouse_available());
   TEST_ASSERT_FALSE(a5_set_mouse_xy(s_display, 1, 1));

   a5_ms_state.x = 123;
   a5_get_mouse_state(&a5_ms_state);
   TEST_ASSERT_EQUAL_INT(0, a5_mouse_x);
}

static int run_tests(int argc, char **argv)
{
   int failures;
   (void)argc;
   (void)argv;

   if (!al_init())
      return 77;
   al_set_new_display_flags(ALLEGRO_WINDOWED);
   s_display = al_create_display(WIN_W, WIN_H);
   if (s_display == NULL)
      return 77;
   s_queue = al_create_event_queue();

   UNITY_BEGIN();
   RUN_TEST(test_a_move_updates_state_and_emits_axes);
   RUN_TEST(test_y_runs_down_from_the_top_of_the_window);
   RUN_TEST(test_left_button_goes_down_and_up);
   RUN_TEST(test_right_button_is_button_two);
   RUN_TEST(test_a_click_on_the_title_bar_is_not_a_click_on_the_map);
   RUN_TEST(test_a_move_outside_the_view_is_ignored);
   RUN_TEST(test_a_drag_outside_the_view_is_pinned_to_the_edge);
   RUN_TEST(test_a_plain_move_clears_a_button_whose_release_was_lost);
   RUN_TEST(test_the_scroll_wheel_moves_z);
   RUN_TEST(test_the_helpers_report_a_pointer_while_it_runs);
   failures = UNITY_END();

   al_destroy_event_queue(s_queue);
   al_destroy_display(s_display);
   return failures;
}

int main(int argc, char **argv)
{
   return al_run_main(argc, argv, run_tests);
}
