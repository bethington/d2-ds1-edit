/*
 * input.h -- one place where Allegro 5 input reaches the editor.
 *
 * The editor was written against Allegro 4, where the runtime polled the
 * hardware behind your back and `key[]` was simply true while a key was held.
 * Debouncing a command therefore meant "act, then spin until the user lets
 * go":
 *
 *    if (key_pressed(KEY_U)) { undo(); while (key_pressed(KEY_U)) rest(10); }
 *
 * Ported literally onto A5 that is worse than it looks. Nothing drains the
 * event queue while it spins, so the window stops redrawing, stops responding,
 * and cannot even be closed -- for as long as a finger rests on a key. In
 * editpath_enter_action it was fatal outright: the spin had an empty body and
 * no state refresh at all, so it could never terminate.
 *
 * A5 already reports the thing that code was trying to reconstruct. A physical
 * press arrives once as ALLEGRO_EVENT_KEY_DOWN -- auto-repeat comes separately
 * as KEY_CHAR -- so recording which keys went down during a pump gives exact
 * press semantics with no spinning and no lost input. That is key_hit().
 *
 * Two questions, two answers, and they are not interchangeable:
 *
 *    key_hit(k)      did this key go down since the last pump?   commands
 *    key_pressed(k)  is it held right now?                       continuous
 *
 * Use key_hit for anything that should happen once per press (undo, toggle a
 * panel, confirm a dialog). Use key_pressed for anything that should continue
 * while held (scrolling the map). Reaching for key_pressed where key_hit
 * belongs is what the spin loops were compensating for.
 *
 * Every loop that reads input must call input_pump() once per iteration --
 * modal dialogs included. Before, a dialog's only refresh came as a side
 * effect of a spin loop, so a dialog nobody was mashing keys at saw a frozen
 * mouse position.
 */

#ifndef DS1EDIT_INPUT_H
#define DS1EDIT_INPUT_H

#include <allegro5/allegro.h>

/* Keys that transitioned to down during the most recent pump. Indexed by
   ALLEGRO_KEY_*; read it through key_hit(). */
extern unsigned char a5_key_hit[ALLEGRO_KEY_MAX];

/* Keys masked off after a focus loss; declared in ui/compat.h, which is where
   key_pressed() reads it. Defined here alongside the rest of the input state. */

/* Mouse buttons that went down during the most recent pump, 1-based to match
   al_mouse_button_down(). Index 0 is unused. */
#define DS1_MOUSE_BUTTON_MAX 8
extern unsigned char a5_mb_hit[DS1_MOUSE_BUTTON_MAX];

/* Set once the user asks to close the window. Modal loops should treat this
   as a cancel and unwind -- previously a dialog swallowed DISPLAY_CLOSE and
   the window could not be closed while one was open. */
extern int a5_display_closed;

/* Raised when the display was resized and the render targets were rebuilt to
   match. Cleared by input_take_resize(). */
int  input_take_resize(void);

/* Wiring telemetry. A loop that forgets to pump still renders perfectly --
   it just never sees a key again -- so these let --selftest prove the main
   loop is actually connected rather than merely drawing. */
extern unsigned long a5_input_frames;   /* input_end_frame() calls */
extern unsigned long a5_input_events;   /* events handed to input_note_event() */
/* Timer events specifically. The tick timer runs at 25 Hz off the real queue
   and nothing synthesises one, so a non-zero count is proof that the queue is
   genuinely being drained through this layer -- which a test that injects its
   own events cannot fake. */
extern unsigned long a5_input_timer_events;

void input_init(void);

/* Clear per-pump state, drain the queue, then sample the held state.
   input_begin_frame/input_note_event/input_end_frame are the same thing split
   apart, for the main loop, which needs to inspect events itself. */
void input_pump(void);
void input_begin_frame(void);
void input_note_event(const ALLEGRO_EVENT *ev);
void input_end_frame(void);

/* Drop every "still held" belief. Called on focus loss, where the release
   event is delivered to whoever has focus instead of to us -- without this a
   key held during Cmd-Tab stays latched down forever. */
void input_forget_held(void);

/* Did this key go down since the last pump? */
#define key_hit(k)     (a5_key_hit[(k)] != 0)

/* Did this mouse button go down since the last pump? Button numbers match
   al_mouse_button_down(): 1 left, 2 right, 3 middle. */
#define mouse_hit(b)   (((unsigned)(b) < DS1_MOUSE_BUTTON_MAX) && a5_mb_hit[(b)] != 0)

#endif /* DS1EDIT_INPUT_H */
