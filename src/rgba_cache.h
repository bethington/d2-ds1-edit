#ifndef _RGBA_CACHE_H_
#define _RGBA_CACHE_H_

#include <stdint.h>
#include "palette.h"

/*
 * CACHED_TILE: holds both palette-indexed pixel data (for editing)
 * and pre-rendered RGBA pixel data (for display).
 *
 * The index buffer stores original palette indices (0-255).
 * The RGBA buffer stores pre-resolved 32-bit colors.
 * Index 0 is transparent (RGBA 0,0,0,0).
 *
 * Both buffers are width * height entries, stored row-major.
 */
typedef struct {
    uint8_t  *indices;   /* palette index per pixel (width * height) */
    uint32_t *rgba;      /* packed RGBA per pixel (width * height) */
    int       width;
    int       height;
} CACHED_TILE;

/*
 * Allocate a CACHED_TILE with the given dimensions.
 * Both index and RGBA buffers are zeroed (transparent).
 * Returns NULL on allocation failure.
 */
CACHED_TILE *cache_tile_create(int width, int height);

/*
 * Free a CACHED_TILE and its buffers.
 * Safe to call with NULL.
 */
void cache_tile_destroy(CACHED_TILE *tile);

/*
 * Rebuild the RGBA buffer from the index buffer using the given palette.
 * Call this after modifying indices or when the palette changes.
 */
void cache_tile_rebuild(CACHED_TILE *tile, const RGBA_PALETTE *pal);

/*
 * Convert a CACHED_TILE to an ALLEGRO_BITMAP using the given palette.
 * Rebuilds the RGBA buffer from indices, then copies to the bitmap.
 * Returns a new ALLEGRO_BITMAP (caller must destroy it).
 * Returns NULL on failure.
 */
struct ALLEGRO_BITMAP;
struct ALLEGRO_BITMAP *cache_tile_to_a5_bitmap(CACHED_TILE *tile, const RGBA_PALETTE *pal);

/*
 * Pack an RGBA_COLOR into a uint32_t (ABGR byte order for Allegro 5 compatibility).
 * Format: 0xAABBGGRR (little-endian: R at lowest byte)
 */
static inline uint32_t rgba_pack(RGBA_COLOR c)
{
    return ((uint32_t)c.a << 24) |
           ((uint32_t)c.b << 16) |
           ((uint32_t)c.g << 8)  |
           ((uint32_t)c.r);
}

/*
 * Unpack a uint32_t to RGBA_COLOR.
 */
static inline RGBA_COLOR rgba_unpack(uint32_t packed)
{
    RGBA_COLOR c;
    c.r = (uint8_t)(packed);
    c.g = (uint8_t)(packed >> 8);
    c.b = (uint8_t)(packed >> 16);
    c.a = (uint8_t)(packed >> 24);
    return c;
}

#endif
