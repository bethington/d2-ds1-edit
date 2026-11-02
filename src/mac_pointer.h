/* mac_pointer.h -- the editor's pointer on macOS, read from Cocoa events.
 * See mac_pointer.m for why this replaces Allegro's mouse driver there. */

#ifndef MAC_POINTER_H
#define MAC_POINTER_H

#include <allegro5/allegro.h>

/* Start reading the pointer for this display. Returns 0 when the display has
   no window to watch (exclusive fullscreen), leaving the pointer inactive. */
int  ds1_mac_pointer_start(ALLEGRO_DISPLAY *display);
void ds1_mac_pointer_stop(void);
int  ds1_mac_pointer_active(void);

/* The same contract as al_get_mouse_state() and al_set_mouse_xy(). */
void ds1_mac_pointer_get_state(ALLEGRO_MOUSE_STATE *state);
bool ds1_mac_pointer_warp(int x, int y);

/* Emits ALLEGRO_EVENT_MOUSE_AXES and _BUTTON_DOWN/_UP events, with
   ev.mouse.display left NULL. Valid while the pointer is active. */
ALLEGRO_EVENT_SOURCE *ds1_mac_pointer_event_source(void);

#endif
