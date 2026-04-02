#ifndef _DT1_DRAW_H_

#define _DT1_DRAW_H_

/* New: decode to a raw index buffer (no Allegro dependency).
 * These use unsigned char to avoid stdint.h dependency in legacy code. */
void decode_sub_tile_isometric(unsigned char *buf, int buf_w, int buf_h,
                               int x0, int y0,
                               const unsigned char *data, int length);
void decode_sub_tile_normal   (unsigned char *buf, int buf_w, int buf_h,
                               int x0, int y0,
                               const unsigned char *data, int length);

/* Nearest-neighbor downscale of an index buffer */
void index_buf_scale_down(const unsigned char *src, int src_w, int src_h,
                          unsigned char *dst, int dst_w, int dst_h);

/* Legacy: draw to Allegro 4 BITMAP (palette indices via putpixel).
 * Only available when BITMAP type is defined (i.e., allegro.h included). */
#ifdef ALLEGRO_H
void draw_sub_tile_isometric (BITMAP * dst, int x0, int y0, UBYTE * data, int length);
void draw_sub_tile_normal    (BITMAP * dst, int x0, int y0, UBYTE * data, int length);
#endif

#endif
