/* input.c -- see input.h for why this exists and how to use it. */

#include <stdio.h>
#include <string.h>
#include <allegro5/allegro.h>
#include "structs.h"
#include "ui/compat.h"
#include "ui/input.h"
#include "misc.h"   /* ds1edit_recreate_render_targets */

unsigned char a5_key_hit[ALLEGRO_KEY_MAX];
unsigned char a5_key_suppressed[ALLEGRO_KEY_MAX];
unsigned char a5_mb_hit[DS1_MOUSE_BUTTON_MAX];
int a5_display_closed;

static int input_resize_pending;

void input_init(void)
{
   memset(a5_key_hit, 0, sizeof(a5_key_hit));
   memset(a5_mb_hit, 0, sizeof(a5_mb_hit));
   a5_display_closed = 0;
   input_resize_pending = 0;

   memset(a5_key_suppressed, 0, sizeof(a5_key_suppressed));
   al_get_keyboard_state(&a5_kb_state);
   al_get_mouse_state(&a5_ms_state);
}

void input_begin_frame(void)
{
   memset(a5_key_hit, 0, sizeof(a5_key_hit));
   memset(a5_mb_hit, 0, sizeof(a5_mb_hit));
}

void input_forget_held(void)
{
   int k;

   /* Allegro keeps reporting these as down, so mask them off ourselves until
      each one is pressed again for real. */
   al_get_keyboard_state(&a5_kb_state);
   for (k = 1; k < ALLEGRO_KEY_MAX; k++)
   {
      if (al_key_down(&a5_kb_state, k))
         a5_key_suppressed[k] = 1;
   }

   memset(a5_key_hit, 0, sizeof(a5_key_hit));
   memset(a5_mb_hit, 0, sizeof(a5_mb_hit));
}

void input_note_event(const ALLEGRO_EVENT *ev)
{
   if (ev == NULL)
      return;

   switch (ev->type)
   {
   case ALLEGRO_EVENT_KEY_DOWN:
      if (ev->keyboard.keycode > 0 && ev->keyboard.keycode < ALLEGRO_KEY_MAX)
      {
         /* A real press: whatever we were masking off after a focus loss is
            over, this key is legitimately down again. */
         a5_key_suppressed[ev->keyboard.keycode] = 0;
         a5_key_hit[ev->keyboard.keycode] = 1;
      }
      break;

   case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
      if (ev->mouse.button < DS1_MOUSE_BUTTON_MAX)
         a5_mb_hit[ev->mouse.button] = 1;
      break;

   case ALLEGRO_EVENT_DISPLAY_CLOSE:
      a5_display_closed = 1;
      break;

   case ALLEGRO_EVENT_DISPLAY_RESIZE:
      /* Acknowledge first -- until we do, Allegro holds the display at the old
         size and further resize events queue up behind this one. Then rebuild
         the offscreen targets, which are sized from glb_config.screen and are
         what everything actually draws into. */
      al_acknowledge_resize(ev->display.source);
      glb_config.screen.width = al_get_display_width(ev->display.source);
      glb_config.screen.height = al_get_display_height(ev->display.source);
      ds1edit_recreate_render_targets();
      input_resize_pending = 1;
      break;

   case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
      /* The key-up for anything held right now will be delivered to whichever
         window takes focus, never to us. Forget it all rather than latch. */
      input_forget_held();
      break;

   default:
      break;
   }
}

void input_end_frame(void)
{
   al_get_keyboard_state(&a5_kb_state);
   al_get_mouse_state(&a5_ms_state);
}

void input_pump(void)
{
   ALLEGRO_EVENT ev;

   input_begin_frame();

   if (a5_event_queue != NULL)
   {
      while (al_get_next_event(a5_event_queue, &ev))
         input_note_event(&ev);
   }

   input_end_frame();
}

int input_take_resize(void)
{
   int r = input_resize_pending;
   input_resize_pending = 0;
   return r;
}
