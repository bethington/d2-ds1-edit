/*
 * Allegro 5 global state definitions.
 * Declared as extern in a5_compat.h.
 */
#include "a5_compat.h"

ALLEGRO_DISPLAY     *a5_display      = NULL;
ALLEGRO_FONT        *a5_font         = NULL;
ALLEGRO_EVENT_QUEUE *a5_event_queue  = NULL;
ALLEGRO_TIMER       *a5_tick_timer   = NULL;
ALLEGRO_TIMER       *a5_fps_timer    = NULL;
RGBA_PALETTE        *a5_current_palette = NULL;

ALLEGRO_KEYBOARD_STATE a5_kb_state;
ALLEGRO_MOUSE_STATE    a5_ms_state;
ALLEGRO_CONFIG         *a5_config      = NULL;
float                  a5_trans_alpha  = 0.5f;
