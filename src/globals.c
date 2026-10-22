/*
 * Allegro 5 global state definitions.
 * Declared as extern in a5_compat.h.
 */
/* al_get_glyph (used by a5_gpu_guard_font) is declared behind
   ALLEGRO_UNSTABLE. It is only called when the headers are new enough to have
   it, and the font addon exports it unconditionally. */
#define ALLEGRO_UNSTABLE
#include "ui/compat.h"

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


/* ---- GPU draw guard (see ui/compat.h) ---- */
unsigned a5_gpu_guard_hits = 0;

void a5_gpu_guard_report(const char *what, const char *file, int line,
                         const ALLEGRO_BITMAP *bmp)
{
    static int warned = 0;

    a5_gpu_guard_hits++;
    if (warned)
        return;
    warned = 1;
    fprintf(stderr,
            "render: WARNING -- %s at %s:%d drew a MEMORY bitmap (%dx%d) onto a "
            "VIDEO target. Allegro locks the whole target for every such draw; "
            "expect a fraction of a frame per second. The bitmap was created "
            "before the display and never promoted.\n",
            what, file, line,
            al_get_bitmap_width((ALLEGRO_BITMAP *)bmp),
            al_get_bitmap_height((ALLEGRO_BITMAP *)bmp));
    fflush(stderr);
}

int a5_gpu_guard_font(const ALLEGRO_FONT *font, const char *name)
{
#if ALLEGRO_VERSION_INT >= ((5 << 24) | (2 << 16) | (5 << 8))
    ALLEGRO_GLYPH glyph;

    if (font == NULL)
        return 0;
    if (!al_get_glyph(font, 0, 'A', &glyph) || glyph.bitmap == NULL)
        return 0;
    if (!(al_get_bitmap_flags(glyph.bitmap) & ALLEGRO_MEMORY_BITMAP))
        return 0;

    a5_gpu_guard_hits++;
    fprintf(stderr,
            "render: WARNING -- font '%s' has MEMORY glyph bitmaps; every text "
            "draw onto a video target locks it (~2.5 s/frame on Direct3D). Call "
            "al_convert_memory_bitmaps() after the display exists.\n",
            name != NULL ? name : "?");
    fflush(stderr);
    return 1;
#else
    (void)font;
    (void)name;
    return 0;
#endif
}
