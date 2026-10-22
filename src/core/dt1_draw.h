/* Derived from win_ds1edit by Paul Siramy.
 * See NOTICE at the repository root for attribution and license status. */

#ifndef _DT1_DRAW_H_

#define _DT1_DRAW_H_

/* New: decode to a raw index buffer (no Allegro dependency). */
void decode_sub_tile_isometric(unsigned char *buf, int buf_w, int buf_h,
                               int x0, int y0,
                               const unsigned char *data, int length);
void decode_sub_tile_normal   (unsigned char *buf, int buf_w, int buf_h,
                               int x0, int y0,
                               const unsigned char *data, int length);

/* Nearest-neighbor downscale of an index buffer */
void index_buf_scale_down(const unsigned char *src, int src_w, int src_h,
                          unsigned char *dst, int dst_w, int dst_h);

#endif
