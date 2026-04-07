#ifndef _PALETTE_H_
#define _PALETTE_H_

#include <stdint.h>

/* Number of colors in a palette */
#define PAL_COLORS 256

/* Number of acts (palettes) */
#define PAL_ACT_MAX 5

/* RGBA color (8-bit per channel, full range 0-255) */
typedef struct {
    uint8_t r, g, b, a;
} RGBA_COLOR;

/* 256-color RGBA palette */
typedef struct {
    RGBA_COLOR colors[PAL_COLORS];
} RGBA_PALETTE;

/* 256x256 index-to-index lookup table (replaces Allegro 4 COLOR_MAP) */
typedef struct {
    uint8_t data[PAL_COLORS][PAL_COLORS];
} INDEX_COLORMAP;

/*
 * Convert a raw Diablo II palette to an RGBA palette.
 *
 * d2_pal: raw D2 palette data (256 entries, 4 bytes each: R, G, B, padding)
 *         If NULL, generates a grayscale ramp.
 * d2_pal_size: size of d2_pal in bytes (expected: 1024+)
 * gamma_table: 256-entry gamma correction table (applied to each channel
 *              before storing). Pass NULL to skip gamma correction.
 * out: output RGBA palette
 *
 * Index 0 is treated as fully transparent (alpha=0).
 * All other indices get alpha=255.
 */
void palette_d2_to_rgba(const uint8_t *d2_pal, long d2_pal_size,
                        const uint8_t *gamma_table,
                        RGBA_PALETTE *out);

/*
 * Build a SELECT colormap (for mouse-over highlighting).
 *
 * For COL_SHADOW (index 168): maps to white
 * For COL_MOUSE (index 255): brightens toward white
 * For all others: 50% blend between source and destination palette colors
 *
 * pal: the RGBA palette (used for blending calculations)
 * out: 256x256 index lookup table
 */
void palette_build_select_colormap(const RGBA_PALETTE *pal, INDEX_COLORMAP *out);

/*
 * Build a TRANS colormap (50% transparency with gray bias).
 *
 * Each entry blends the source palette color with the destination,
 * biased 50% toward (128,128,128) gray.
 *
 * pal: the RGBA palette
 * out: 256x256 index lookup table
 */
void palette_build_trans_colormap(const RGBA_PALETTE *pal, INDEX_COLORMAP *out);

/*
 * Build a SHADOW colormap from the raw D2 palette data.
 *
 * Uses the extended palette data (bytes 1024+) which contains
 * shadow/transparency lookup tables from Diablo II.
 *
 * d2_pal: raw palette data (must be > 1024 bytes to contain shadow data)
 * d2_pal_size: total size of d2_pal
 * out: 256x256 index lookup table
 */
void palette_build_shadow_colormap(const uint8_t *d2_pal, long d2_pal_size,
                                   INDEX_COLORMAP *out);

/*
 * Find the closest palette index for a given RGB color.
 * Uses simple Euclidean distance in RGB space.
 *
 * pal: the palette to search
 * r, g, b: target color (0-255 range)
 * Returns: best matching palette index (0-255)
 */
uint8_t palette_find_closest(const RGBA_PALETTE *pal, uint8_t r, uint8_t g, uint8_t b);

#endif
