#ifndef _COMPOSE_SCALE_H_
#define _COMPOSE_SCALE_H_

#include <stddef.h>

// Integer nearest-neighbour upscale of an RGBA8 buffer. Each source
// pixel becomes a `scale x scale` block in the destination. Pixel-
// perfect for D2 sprite art (no blur, no anti-alias artefacts on the
// indexed-derived RGBA), and has no external dependency.
//
// Contract:
//   - src is src_w * src_h * 4 bytes (4 bytes per RGBA pixel)
//   - dst is (src_w * scale) * (src_h * scale) * 4 bytes (caller alloc)
//   - scale must be >= 1; scale==1 is identity (memcpy-equivalent)
//   - src and dst MUST NOT overlap
//
// Returns 1 on success, 0 on bad arguments (NULL pointer, zero
// dimension, scale < 1).
int compose_scale_nn_rgba(const unsigned char *src, int src_w, int src_h,
                          unsigned char *dst, int scale);

#endif
