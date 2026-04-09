/*
 * Allegro 5 Drawing Helpers and Input Compatibility
 *
 * Provides convenience wrappers around Allegro 5's target-bitmap model,
 * palette-indexed color conversion, D2 blend modes, and keyboard/mouse
 * state macros. Also defines short KEY_* aliases for the subset of
 * Allegro 5 key constants used by the editor.
 */
#ifndef _A5_COMPAT_H_
#define _A5_COMPAT_H_

#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

#include <stdint.h>
#include <stdio.h>

/* ---- Boolean constants ---- */
/* Allegro 5 does not define TRUE/FALSE */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* ---- Global display state ---- */
/* Allegro 5 has no global screen/font/color_map; we maintain our own */
extern ALLEGRO_DISPLAY     *a5_display;
extern ALLEGRO_FONT        *a5_font;
extern ALLEGRO_EVENT_QUEUE *a5_event_queue;
extern ALLEGRO_TIMER       *a5_tick_timer;
extern ALLEGRO_TIMER       *a5_fps_timer;

/* ---- Palette index to color conversion ---- */
/* Current active palette for pal_color() lookups */
#include "core/palette.h"
extern RGBA_PALETTE *a5_current_palette;

static inline ALLEGRO_COLOR pal_color(int index)
{
    if (a5_current_palette == NULL)
        return al_map_rgb(index, index, index);
    return al_map_rgba(
        a5_current_palette->colors[index & 0xFF].r,
        a5_current_palette->colors[index & 0xFF].g,
        a5_current_palette->colors[index & 0xFF].b,
        a5_current_palette->colors[index & 0xFF].a
    );
}

/* ---- Drawing helpers ---- */
/* These wrap Allegro 5's target-bitmap model with a convenient
 * pass-dst-bitmap calling convention */

static inline ALLEGRO_BITMAP * a5_begin_target_bitmap(ALLEGRO_BITMAP * dst)
{
    ALLEGRO_BITMAP * current = al_get_target_bitmap();
    if (current != dst)
        al_set_target_bitmap(dst);
    return current;
}

static inline void a5_end_target_bitmap(ALLEGRO_BITMAP * previous, ALLEGRO_BITMAP * dst)
{
    if (previous != dst)
        al_set_target_bitmap(previous);
}

static inline void a5_draw_sprite(ALLEGRO_BITMAP *dst, ALLEGRO_BITMAP *src, int x, int y)
{
    ALLEGRO_BITMAP *old_target = a5_begin_target_bitmap(dst);
    al_draw_bitmap(src, (float)x, (float)y, 0);
    a5_end_target_bitmap(old_target, dst);
}

/* Global: set before calling a5_draw_trans_sprite to control blend level.
 * D2 uses trans_b: 0=75% transparent, 1=50%, 2=25%, 3+=screen/luminance */
extern float a5_trans_alpha;

static inline void a5_draw_trans_sprite(ALLEGRO_BITMAP *dst, ALLEGRO_BITMAP *src, int x, int y)
{
    float alpha = a5_trans_alpha;
    ALLEGRO_BITMAP *old_target = a5_begin_target_bitmap(dst);
    al_draw_tinted_bitmap(src, al_map_rgba_f(alpha, alpha, alpha, alpha),
                          (float)x, (float)y, 0);
    a5_end_target_bitmap(old_target, dst);
}

/* Scaled blended sprite for zoomed-out rendering with D2 COF blend modes.
 * div is the scale divisor (1 = normal, 2 = half size, etc.).
 * trans_b selects the D2 blend mode; pass -1 for normal (opaque) drawing. */
static inline void a5_draw_scaled_blended_sprite(ALLEGRO_BITMAP *dst, ALLEGRO_BITMAP *src,
                                                  int x, int y, int div, int trans_b)
{
    ALLEGRO_BITMAP *old_target = a5_begin_target_bitmap(dst);
    float sw = (float)al_get_bitmap_width(src);
    float sh = (float)al_get_bitmap_height(src);
    float dw = sw / (float)div;
    float dh = sh / (float)div;

    if (trans_b < 0) {
        /* Normal opaque drawing */
        al_draw_scaled_bitmap(src, 0, 0, sw, sh, (float)x, (float)y, dw, dh, 0);
    } else {
        switch (trans_b)
        {
            case 0:
                al_draw_tinted_scaled_bitmap(src, al_map_rgba_f(0.25f, 0.25f, 0.25f, 0.25f),
                                              0, 0, sw, sh, (float)x, (float)y, dw, dh, 0);
                break;
            case 1:
                al_draw_tinted_scaled_bitmap(src, al_map_rgba_f(0.50f, 0.50f, 0.50f, 0.50f),
                                              0, 0, sw, sh, (float)x, (float)y, dw, dh, 0);
                break;
            case 2:
                al_draw_tinted_scaled_bitmap(src, al_map_rgba_f(0.75f, 0.75f, 0.75f, 0.75f),
                                              0, 0, sw, sh, (float)x, (float)y, dw, dh, 0);
                break;
            case 3: /* Additive */
                al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_ONE);
                al_draw_scaled_bitmap(src, 0, 0, sw, sh, (float)x, (float)y, dw, dh, 0);
                al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
                break;
            case 4: /* Multiply */
                al_set_blender(ALLEGRO_ADD, ALLEGRO_DEST_COLOR, ALLEGRO_ZERO);
                al_draw_scaled_bitmap(src, 0, 0, sw, sh, (float)x, (float)y, dw, dh, 0);
                al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
                break;
            case 6: /* Screen */
                al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_INVERSE_SRC_COLOR);
                al_draw_scaled_bitmap(src, 0, 0, sw, sh, (float)x, (float)y, dw, dh, 0);
                al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
                break;
            default:
                al_draw_tinted_scaled_bitmap(src, al_map_rgba_f(0.50f, 0.50f, 0.50f, 0.50f),
                                              0, 0, sw, sh, (float)x, (float)y, dw, dh, 0);
                break;
        }
    }

    a5_end_target_bitmap(old_target, dst);
}

/* D2 shadow rendering: draws sprite as a black silhouette that darkens the
 * destination. darkness_level is 0..32 from D2CMP's darkness palette table,
 * where 0 = fully black and 32 = no darkening.
 * Formula: result = dst * (level/32), achieved via black tint with alpha. */
static inline void a5_draw_shadow_sprite(ALLEGRO_BITMAP *dst, ALLEGRO_BITMAP *src,
                                          int x, int y, int darkness_level)
{
    float alpha = 1.0f - (float)darkness_level / 32.0f;
    ALLEGRO_BITMAP *old_target = a5_begin_target_bitmap(dst);
    al_draw_tinted_bitmap(src, al_map_rgba_f(0, 0, 0, alpha),
                          (float)x, (float)y, 0);
    a5_end_target_bitmap(old_target, dst);
}

/* Scaled version for zoomed-out shadow rendering. */
static inline void a5_draw_scaled_shadow_sprite(ALLEGRO_BITMAP *dst, ALLEGRO_BITMAP *src,
                                                 int x, int y, int div, int darkness_level)
{
    float alpha = 1.0f - (float)darkness_level / 32.0f;
    float sw = (float)al_get_bitmap_width(src);
    float sh = (float)al_get_bitmap_height(src);
    float dw = sw / (float)div;
    float dh = sh / (float)div;
    ALLEGRO_BITMAP *old_target = a5_begin_target_bitmap(dst);
    al_draw_tinted_scaled_bitmap(src, al_map_rgba_f(0, 0, 0, alpha),
                                  0, 0, sw, sh, (float)x, (float)y, dw, dh, 0);
    a5_end_target_bitmap(old_target, dst);
}

static inline void a5_blit(ALLEGRO_BITMAP *src, ALLEGRO_BITMAP *dst,
                           int sx, int sy, int dx, int dy, int w, int h)
{
    ALLEGRO_BITMAP *old_target = a5_begin_target_bitmap(dst);
    al_draw_bitmap_region(src, (float)sx, (float)sy, (float)w, (float)h,
                          (float)dx, (float)dy, 0);
    a5_end_target_bitmap(old_target, dst);
}

static inline void a5_stretch_blit(ALLEGRO_BITMAP *src, ALLEGRO_BITMAP *dst,
                                   int sx, int sy, int sw, int sh,
                                   int dx, int dy, int dw, int dh)
{
    ALLEGRO_BITMAP *old_target = a5_begin_target_bitmap(dst);
    al_draw_scaled_bitmap(src, (float)sx, (float)sy, (float)sw, (float)sh,
                          (float)dx, (float)dy, (float)dw, (float)dh, 0);
    a5_end_target_bitmap(old_target, dst);
}

static inline void a5_clear(ALLEGRO_BITMAP *bmp)
{
    ALLEGRO_BITMAP *old_target;
    if (bmp == NULL || al_get_bitmap_width(bmp) == 0 || al_get_bitmap_height(bmp) == 0)
        return;
    old_target = a5_begin_target_bitmap(bmp);
    al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_ZERO);
    al_clear_to_color(al_map_rgba(0, 0, 0, 0));
    al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
    a5_end_target_bitmap(old_target, bmp);
}

static inline void a5_clear_to_color(ALLEGRO_BITMAP *bmp, int color)
{
    ALLEGRO_BITMAP *old_target = a5_begin_target_bitmap(bmp);
    al_clear_to_color(pal_color(color));
    a5_end_target_bitmap(old_target, bmp);
}

static inline void a5_putpixel(ALLEGRO_BITMAP *bmp, int x, int y, int color)
{
    ALLEGRO_BITMAP *old_target;
    if (bmp == NULL || al_get_bitmap_width(bmp) == 0 || al_get_bitmap_height(bmp) == 0)
        return;
    old_target = a5_begin_target_bitmap(bmp);
    al_put_pixel(x, y, pal_color(color));
    a5_end_target_bitmap(old_target, bmp);
}

/* ---- Drawing primitives ---- */
static inline void a5_line(ALLEGRO_BITMAP *bmp, int x1, int y1, int x2, int y2, int color)
{
    ALLEGRO_BITMAP *old_target = a5_begin_target_bitmap(bmp);
    al_draw_line((float)x1 + 0.5f, (float)y1 + 0.5f,
                 (float)x2 + 0.5f, (float)y2 + 0.5f, pal_color(color), 1.0f);
    a5_end_target_bitmap(old_target, bmp);
}

static inline void a5_rect(ALLEGRO_BITMAP *bmp, int x1, int y1, int x2, int y2, int color)
{
    ALLEGRO_BITMAP *old_target = a5_begin_target_bitmap(bmp);
    al_draw_rectangle((float)x1 + 0.5f, (float)y1 + 0.5f,
                      (float)x2 + 0.5f, (float)y2 + 0.5f, pal_color(color), 1.0f);
    a5_end_target_bitmap(old_target, bmp);
}

static inline void a5_rectfill(ALLEGRO_BITMAP *bmp, int x1, int y1, int x2, int y2, int color)
{
    ALLEGRO_BITMAP *old_target = a5_begin_target_bitmap(bmp);
    al_draw_filled_rectangle((float)x1, (float)y1, (float)x2 + 1.0f, (float)y2 + 1.0f,
                             pal_color(color));
    a5_end_target_bitmap(old_target, bmp);
}

static inline void a5_hline(ALLEGRO_BITMAP *bmp, int x1, int y, int x2, int color)
{
    a5_line(bmp, x1, y, x2, y, color);
}

static inline void a5_vline(ALLEGRO_BITMAP *bmp, int x, int y1, int y2, int color)
{
    a5_line(bmp, x, y1, x, y2, color);
}

/* ---- Text rendering ---- */
/* Wraps al_draw_textf with target-bitmap save/restore */
#define a5_textprintf(bmp, fnt, x, y, color, ...) \
    do { \
        ALLEGRO_BITMAP *_old = a5_begin_target_bitmap(bmp); \
        al_draw_textf(a5_font, pal_color(color), (float)(x), (float)(y), 0, __VA_ARGS__); \
        a5_end_target_bitmap(_old, bmp); \
    } while(0)


/* ---- Keyboard compat ---- */
/* Keyboard state: al_key_down(&state, ALLEGRO_KEY_X) */
extern ALLEGRO_KEYBOARD_STATE a5_kb_state;

/* Short KEY_* aliases for Allegro 5 key constants used by the editor */
#define KEY_A         ALLEGRO_KEY_A
#define KEY_C         ALLEGRO_KEY_C
#define KEY_D         ALLEGRO_KEY_D
#define KEY_G         ALLEGRO_KEY_G
#define KEY_H         ALLEGRO_KEY_H
#define KEY_I         ALLEGRO_KEY_I
#define KEY_N         ALLEGRO_KEY_N
#define KEY_P         ALLEGRO_KEY_P
#define KEY_Q         ALLEGRO_KEY_Q
#define KEY_R         ALLEGRO_KEY_R
#define KEY_S         ALLEGRO_KEY_S
#define KEY_T         ALLEGRO_KEY_T
#define KEY_U         ALLEGRO_KEY_U
#define KEY_W         ALLEGRO_KEY_W
#define KEY_X         ALLEGRO_KEY_X
#define KEY_0         ALLEGRO_KEY_0
#define KEY_1         ALLEGRO_KEY_1
#define KEY_2         ALLEGRO_KEY_2
#define KEY_3         ALLEGRO_KEY_3
#define KEY_4         ALLEGRO_KEY_4
#define KEY_5         ALLEGRO_KEY_5
#define KEY_6         ALLEGRO_KEY_6
#define KEY_7         ALLEGRO_KEY_7
#define KEY_8         ALLEGRO_KEY_8
#define KEY_9         ALLEGRO_KEY_9
#define KEY_UP        ALLEGRO_KEY_UP
#define KEY_DOWN      ALLEGRO_KEY_DOWN
#define KEY_LEFT      ALLEGRO_KEY_LEFT
#define KEY_RIGHT     ALLEGRO_KEY_RIGHT
#define KEY_ESC       ALLEGRO_KEY_ESCAPE
#define KEY_SPACE     ALLEGRO_KEY_SPACE
#define KEY_TAB       ALLEGRO_KEY_TAB
#define KEY_ENTER     ALLEGRO_KEY_ENTER
#define KEY_F1        ALLEGRO_KEY_F1
#define KEY_F2        ALLEGRO_KEY_F2
#define KEY_F3        ALLEGRO_KEY_F3
#define KEY_F4        ALLEGRO_KEY_F4
#define KEY_F5        ALLEGRO_KEY_F5
#define KEY_F6        ALLEGRO_KEY_F6
#define KEY_F7        ALLEGRO_KEY_F7
#define KEY_F8        ALLEGRO_KEY_F8
#define KEY_F9        ALLEGRO_KEY_F9
#define KEY_F10       ALLEGRO_KEY_F10
#define KEY_F11       ALLEGRO_KEY_F11
#define KEY_F12       ALLEGRO_KEY_F12
#define KEY_LSHIFT    ALLEGRO_KEY_LSHIFT
#define KEY_RSHIFT    ALLEGRO_KEY_RSHIFT
#define KEY_LCONTROL  ALLEGRO_KEY_LCTRL
#define KEY_RCONTROL  ALLEGRO_KEY_RCTRL
#define KEY_MINUS_PAD ALLEGRO_KEY_PAD_MINUS
#define KEY_PLUS_PAD  ALLEGRO_KEY_PAD_PLUS
#define KEY_EQUALS    ALLEGRO_KEY_EQUALS
#define KEY_DEL       ALLEGRO_KEY_DELETE
#define KEY_MINUS     ALLEGRO_KEY_MINUS
#define KEY_BACKSPACE ALLEGRO_KEY_BACKSPACE
#define KEY_HOME      ALLEGRO_KEY_HOME
#define KEY_END       ALLEGRO_KEY_END
#define KEY_TILDE     ALLEGRO_KEY_TILDE
#define KEY_DEL_PAD   ALLEGRO_KEY_PAD_DELETE
#define KEY_ENTER_PAD ALLEGRO_KEY_PAD_ENTER
#define KEY_ALT       ALLEGRO_KEY_ALT
#define KEY_ALTGR     ALLEGRO_KEY_ALTGR
#define KEY_INSERT    ALLEGRO_KEY_INSERT
#define KEY_0_PAD     ALLEGRO_KEY_PAD_0
#define KEY_1_PAD     ALLEGRO_KEY_PAD_1
#define KEY_2_PAD     ALLEGRO_KEY_PAD_2
#define KEY_3_PAD     ALLEGRO_KEY_PAD_3
#define KEY_4_PAD     ALLEGRO_KEY_PAD_4
#define KEY_5_PAD     ALLEGRO_KEY_PAD_5
#define KEY_6_PAD     ALLEGRO_KEY_PAD_6
#define KEY_7_PAD     ALLEGRO_KEY_PAD_7
#define KEY_8_PAD     ALLEGRO_KEY_PAD_8
#define KEY_9_PAD     ALLEGRO_KEY_PAD_9
#define KEY_PGUP      ALLEGRO_KEY_PGUP
#define KEY_PGDN      ALLEGRO_KEY_PGDN
#define KEY_LBRACKET  ALLEGRO_KEY_OPENBRACE
#define KEY_RBRACKET  ALLEGRO_KEY_CLOSEBRACE

/* Global 'font' alias for a5_font */
#define font a5_font

/* 'screen' is NOT #defined here because it is used as a struct member name
 * (glb_config.screen). Use al_get_backbuffer(a5_display) directly in the
 * few places that need the display backbuffer. */

/* key_pressed(k): check if key k is currently held down */
#define key_pressed(k) al_key_down(&a5_kb_state, (k))

/* ---- Mouse compat ---- */
extern ALLEGRO_MOUSE_STATE a5_ms_state;

#define a5_mouse_x (a5_ms_state.x)
#define a5_mouse_y (a5_ms_state.y)
#define a5_mouse_b (a5_ms_state.buttons)
#define a5_mouse_z (a5_ms_state.z)

/* ---- Misc compat ---- */
#define rest(ms) al_rest((double)(ms) / 1000.0)

/* makecol - color from RGB. Returns closest palette index. */
static inline int a5_makecol(int r, int g, int b)
{
    if (a5_current_palette != NULL)
        return palette_find_closest(a5_current_palette, (uint8_t)r, (uint8_t)g, (uint8_t)b);
    /* Fallback: encode as a raw index (won't look right but won't crash) */
    return (r > 128) ? 255 : 0;
}
#define makecol(r,g,b) a5_makecol(r,g,b)

/* Allegro 5 config handle — loaded in config.c */
extern ALLEGRO_CONFIG *a5_config;

/* textout - fixed text rendering (no format string) */
#define a5_textout(bmp, fnt, text, x, y, color) a5_textprintf(bmp, fnt, x, y, color, "%s", text)



/* stricmp is MSVC-specific, may be needed on other platforms */
#ifndef stricmp
#define stricmp _stricmp
#endif

/* file_exists - wraps al_filename_exists */
static inline int a5_file_exists(const char *path)
{
    return al_filename_exists(path);
}

/* get_extension - returns pointer to extension after the dot */
static inline const char *a5_get_extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    return dot ? dot + 1 : "";
}

#endif /* _A5_COMPAT_H_ */
