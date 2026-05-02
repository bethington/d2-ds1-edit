#ifndef _COMPOSE_BLIT_H_
#define _COMPOSE_BLIT_H_

// Alpha-over blit between two RGBA byte buffers, used by the composer
// to blend per-layer DCC frames onto a shared canvas. Kept as a pure
// function operating on plain unsigned char buffers so it can be
// unit-tested without Allegro initialised.
//
// Pixel format is 8-bit RGBA, row-major, no padding (stride =
// width * 4). Alpha 0 = fully transparent, alpha 255 = fully opaque.
//
// Blit semantics: any source pixel with alpha > 0 overwrites the
// destination pixel byte-for-byte. (We do not do per-pixel alpha
// blending in v1: D2 sprites use binary transparency for the
// non-tinted case.) Out-of-bounds source pixels are clipped silently.

void compose_blit_rgba(unsigned char *dst, int dst_w, int dst_h,
                       const unsigned char *src, int src_w, int src_h,
                       int dst_x, int dst_y);

#endif
