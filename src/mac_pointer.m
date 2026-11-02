/* mac_pointer.m -- the editor's pointer on macOS, read from Cocoa events.
 *
 * Allegro's macOS mouse driver (src/macosx/qzmouse.m, osx_init_mouse) counts
 * buttons by enumerating HID devices and refuses to install when it finds
 * none. A Mac driven over a remote desktop, or with nothing plugged in, has no
 * such device, and without Input Monitoring every Mac looks that way. Yet the
 * driver's events come from Cocoa, not HID, and Cocoa delivers them regardless.
 *
 * So on macOS the editor does not install the driver. A local event monitor
 * sees the same NSEvents and turns them into a mouse state and Allegro events.
 * Physical and remote pointers take this one path, and it needs no permission:
 * a local monitor only sees events already addressed to this application.
 *
 * The monitor runs on the main thread and the editor runs on Allegro's user
 * thread, so the state is behind a lock.
 */

#import <Cocoa/Cocoa.h>
#include <os/lock.h>
#include <stdatomic.h>
#include <string.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_osx.h>
#include "mac_pointer.h"

static atomic_int           g_active;
static ALLEGRO_DISPLAY     *g_display;
static NSWindow            *g_window;
static id                   g_monitor;
static ALLEGRO_EVENT_SOURCE g_source;
static os_unfair_lock       g_lock = OS_UNFAIR_LOCK_INIT;
static ALLEGRO_MOUSE_STATE  g_state;
static double               g_scroll_rest;

static void pointer_on_main(dispatch_block_t block)
{
   if ([NSThread isMainThread])
      block();
   else
      dispatch_sync(dispatch_get_main_queue(), block);
}

static int pointer_clamp(int v, int max)
{
   return v < 0 ? 0 : (v > max ? max : v);
}

/* View points to display pixels, top-left origin. The ratio rather than the
   backing scale, so a window Allegro did not make high-DPI is still right. */
static void pointer_view_to_pixels(NSView *view, NSPoint p, int *x, int *y)
{
   NSRect b = view.bounds;
   int w = al_get_display_width(g_display);
   int h = al_get_display_height(g_display);

   *x = pointer_clamp((int)(p.x * w / b.size.width), w - 1);
   *y = pointer_clamp((int)((view.isFlipped ? p.y : b.size.height - p.y) * h / b.size.height), h - 1);
}

static void pointer_emit(ALLEGRO_EVENT_TYPE type, const ALLEGRO_MOUSE_STATE *s,
                         int dx, int dy, int dz, unsigned int button)
{
   ALLEGRO_EVENT ev;

   memset(&ev, 0, sizeof(ev));
   ev.mouse.type = type;
   ev.mouse.x = s->x;
   ev.mouse.y = s->y;
   ev.mouse.z = s->z;
   ev.mouse.dx = dx;
   ev.mouse.dy = dy;
   ev.mouse.dz = dz;
   ev.mouse.button = button;
   /* No destructor, so no reference counting: only the user-event descriptor
      field is touched, which overlaps ev.mouse.display. */
   al_emit_user_event(&g_source, &ev, NULL);
}

static unsigned int pointer_button(NSEvent *e)
{
   switch (e.type)
   {
   case NSEventTypeLeftMouseDown:
   case NSEventTypeLeftMouseUp:
      return 1;
   case NSEventTypeRightMouseDown:
   case NSEventTypeRightMouseUp:
      return 2;
   default:
      return (unsigned int)e.buttonNumber + 1;
   }
}

static void pointer_note(NSEvent *e)
{
   NSWindow *win = g_window;
   NSView *view = win.contentView;
   NSPoint p;
   int inside, x, y;
   ALLEGRO_MOUSE_STATE before, after;
   ALLEGRO_EVENT_TYPE button_type = 0;
   unsigned int button = 0, bit = 0;
   int moved, dz = 0;

   /* Ours, or no window at all (a mouse-moved outside every window). A file
      dialog or a menu is not a click on the map. */
   if (e.window != nil && e.window != win)
      return;

   p = e.window != nil ? e.locationInWindow : [win convertPointFromScreen:e.locationInWindow];
   p = [view convertPoint:p fromView:nil];
   inside = NSPointInRect(p, view.bounds);
   pointer_view_to_pixels(view, p, &x, &y);

   os_unfair_lock_lock(&g_lock);
   before = g_state;
   switch (e.type)
   {
   case NSEventTypeMouseMoved:
      /* A plain move means no button is down, whatever we last saw. A button
         released while another application had focus never reached us. */
      g_state.buttons = 0;
      /* fall through */
   case NSEventTypeLeftMouseDragged:
   case NSEventTypeRightMouseDragged:
   case NSEventTypeOtherMouseDragged:
      /* Outside the view only a drag keeps tracking, pinned to the edge. */
      if (inside || g_state.buttons != 0)
      {
         g_state.x = x;
         g_state.y = y;
      }
      break;

   case NSEventTypeLeftMouseDown:
   case NSEventTypeRightMouseDown:
   case NSEventTypeOtherMouseDown:
      if (inside)
      {
         button = pointer_button(e);
         bit = button <= 32 ? 1u << (button - 1) : 0;
         g_state.x = x;
         g_state.y = y;
         g_state.buttons |= bit;
         button_type = ALLEGRO_EVENT_MOUSE_BUTTON_DOWN;
      }
      break;

   case NSEventTypeLeftMouseUp:
   case NSEventTypeRightMouseUp:
   case NSEventTypeOtherMouseUp:
      button = pointer_button(e);
      bit = button <= 32 ? 1u << (button - 1) : 0;
      if (g_state.buttons & bit)
      {
         g_state.x = x;
         g_state.y = y;
         g_state.buttons &= ~bit;
         button_type = ALLEGRO_EVENT_MOUSE_BUTTON_UP;
      }
      break;

   case NSEventTypeScrollWheel:
      if (inside)
      {
         /* Trackpads report fractions of a notch; keep the remainder. */
         g_scroll_rest += e.deltaY;
         dz = (int)g_scroll_rest;
         g_scroll_rest -= dz;
         g_state.z += dz;
         g_state.x = x;
         g_state.y = y;
      }
      break;

   default:
      break;
   }
   after = g_state;
   os_unfair_lock_unlock(&g_lock);

   moved = after.x != before.x || after.y != before.y;
   if (moved || dz != 0)
      pointer_emit(ALLEGRO_EVENT_MOUSE_AXES, &after, after.x - before.x, after.y - before.y, dz, 0);
   if (button_type != 0)
      pointer_emit(button_type, &after, 0, 0, 0, button);
}

int ds1_mac_pointer_start(ALLEGRO_DISPLAY *display)
{
   NSWindow *win = al_osx_get_window(display);

   if (win == nil || atomic_load(&g_active))
      return 0;

   g_display = display;
   g_window = win;
   al_init_user_event_source(&g_source);

   pointer_on_main(^{
      NSView *view = win.contentView;
      NSPoint p = [view convertPoint:[win convertPointFromScreen:[NSEvent mouseLocation]] fromView:nil];
      int x, y;

      /* Start where the pointer is, or the view's middle when it is elsewhere:
         the origin would read as touching the window's edge. */
      if (!NSPointInRect(p, view.bounds))
         p = NSMakePoint(NSMidX(view.bounds), NSMidY(view.bounds));
      pointer_view_to_pixels(view, p, &x, &y);

      os_unfair_lock_lock(&g_lock);
      memset(&g_state, 0, sizeof(g_state));
      g_state.x = x;
      g_state.y = y;
      g_state.display = display;
      g_scroll_rest = 0;
      os_unfair_lock_unlock(&g_lock);

      [win setAcceptsMouseMovedEvents:YES];
      g_monitor = [NSEvent addLocalMonitorForEventsMatchingMask:
                      NSEventMaskMouseMoved | NSEventMaskScrollWheel
                      | NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged
                      | NSEventMaskRightMouseDown | NSEventMaskRightMouseUp | NSEventMaskRightMouseDragged
                      | NSEventMaskOtherMouseDown | NSEventMaskOtherMouseUp | NSEventMaskOtherMouseDragged
                  handler:^NSEvent *(NSEvent *e) {
                     pointer_note(e);
                     return e;
                  }];
   });

   atomic_store(&g_active, 1);
   return 1;
}

void ds1_mac_pointer_stop(void)
{
   if (!atomic_exchange(&g_active, 0))
      return;

   pointer_on_main(^{
      [NSEvent removeMonitor:g_monitor];
      g_monitor = nil;
   });
   al_destroy_user_event_source(&g_source);
   g_display = NULL;
   g_window = nil;
}

int ds1_mac_pointer_active(void)
{
   return atomic_load(&g_active);
}

void ds1_mac_pointer_get_state(ALLEGRO_MOUSE_STATE *state)
{
   os_unfair_lock_lock(&g_lock);
   *state = g_state;
   os_unfair_lock_unlock(&g_lock);
}

bool ds1_mac_pointer_warp(int x, int y)
{
   if (!atomic_load(&g_active))
      return false;

   pointer_on_main(^{
      NSView *view = g_window.contentView;
      NSRect b = view.bounds;
      int w = al_get_display_width(g_display);
      int h = al_get_display_height(g_display);
      NSPoint p = NSMakePoint(x * b.size.width / w,
                              view.isFlipped ? y * b.size.height / h
                                             : b.size.height - y * b.size.height / h);

      p = [g_window convertPointToScreen:[view convertPoint:p toView:nil]];
      /* Quartz measures down from the top of the primary screen. */
      p.y = NSScreen.screens[0].frame.size.height - p.y;
      CGWarpMouseCursorPosition(p);
      /* A warp otherwise freezes the pointer for a moment afterwards. */
      CGAssociateMouseAndMouseCursorPosition(true);
   });

   /* A warp produces no event, so the state has to move with it. */
   os_unfair_lock_lock(&g_lock);
   g_state.x = x;
   g_state.y = y;
   os_unfair_lock_unlock(&g_lock);
   return true;
}

ALLEGRO_EVENT_SOURCE *ds1_mac_pointer_event_source(void)
{
   return &g_source;
}
