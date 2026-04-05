#include <string.h>
#include "palette.h"


/* ========================================================================== */
void palette_d2_to_rgba(const uint8_t *d2_pal, long d2_pal_size,
                        const uint8_t *gamma_table,
                        RGBA_PALETTE *out)
{
    int i;
    uint8_t r, g, b;

    if (d2_pal == NULL || d2_pal_size < 1024)
    {
        /* No palette data — generate a grayscale ramp */
        for (i = 0; i < PAL_COLORS; i++)
        {
            out->colors[i].r = (uint8_t)i;
            out->colors[i].g = (uint8_t)i;
            out->colors[i].b = (uint8_t)i;
            out->colors[i].a = (i == 0) ? 0 : 255;
        }
        return;
    }

    for (i = 0; i < PAL_COLORS; i++)
    {
        r = d2_pal[4 * i];
        g = d2_pal[4 * i + 1];
        b = d2_pal[4 * i + 2];
        /* byte [4*i+3] is padding, ignored */

        if (gamma_table != NULL)
        {
            r = gamma_table[r];
            g = gamma_table[g];
            b = gamma_table[b];
        }

        out->colors[i].r = r;
        out->colors[i].g = g;
        out->colors[i].b = b;
        out->colors[i].a = (i == 0) ? 0 : 255;
    }
}


/* ========================================================================== */
uint8_t palette_find_closest(const RGBA_PALETTE *pal, uint8_t r, uint8_t g, uint8_t b)
{
    int i;
    int best_idx = 0;
    int best_dist = 0x7FFFFFFF;

    for (i = 1; i < PAL_COLORS; i++) /* skip index 0 (transparent) */
    {
        int dr = (int)pal->colors[i].r - (int)r;
        int dg = (int)pal->colors[i].g - (int)g;
        int db = (int)pal->colors[i].b - (int)b;
        int dist = dr * dr + dg * dg + db * db;

        if (dist < best_dist)
        {
            best_dist = dist;
            best_idx = i;
        }
    }
    return (uint8_t)best_idx;
}


/* ========================================================================== */
/*
 * SELECT colormap: for each (src, dst) pair, produce an output index.
 *
 * This replicates the logic in misc_make_cmaps_helper():
 *   - COL_SHADOW (168): maps to white (255,255,255)
 *   - COL_MOUSE (255):  brightens toward white
 *   - All others:       50% blend between src and dst
 *
 * Since we work with full 8-bit colors but produce a palette index,
 * we need to find the closest palette match for the blended result.
 */
#define COL_SHADOW_IDX 168
#define COL_MOUSE_IDX  255

void palette_build_select_colormap(const RGBA_PALETTE *pal, INDEX_COLORMAP *out)
{
    int x, y;

    for (x = 0; x < PAL_COLORS; x++)
    {
        for (y = 0; y < PAL_COLORS; y++)
        {
            uint8_t r, g, b;

            if (x == COL_SHADOW_IDX)
            {
                /* full white */
                r = 255;
                g = 255;
                b = 255;
            }
            else if (x == COL_MOUSE_IDX)
            {
                /* half way between 2/3 of (2 * src) & 1/3 dst
                 * result is brighter & have more white
                 * Original: (pal[x].rgb + pal[y].rgb * 4) / 3
                 * (clamped to 63 in 6-bit, so 255 in 8-bit) */
                int tr = ((int)pal->colors[x].r + (int)pal->colors[y].r * 4) / 3;
                int tg = ((int)pal->colors[x].g + (int)pal->colors[y].g * 4) / 3;
                int tb = ((int)pal->colors[x].b + (int)pal->colors[y].b * 4) / 3;
                r = (uint8_t)(tr > 255 ? 255 : tr);
                g = (uint8_t)(tg > 255 ? 255 : tg);
                b = (uint8_t)(tb > 255 ? 255 : tb);
            }
            else
            {
                /* common transparency: 50% blend between src & dst */
                r = (uint8_t)(((int)pal->colors[x].r + (int)pal->colors[y].r) >> 1);
                g = (uint8_t)(((int)pal->colors[x].g + (int)pal->colors[y].g) >> 1);
                b = (uint8_t)(((int)pal->colors[x].b + (int)pal->colors[y].b) >> 1);
            }

            out->data[x][y] = palette_find_closest(pal, r, g, b);
        }
    }
}


/* ========================================================================== */
/*
 * TRANS colormap: 50% blend biased toward (128,128,128) gray.
 *
 * Original Allegro 4: create_trans_table(table, pal, 128, 128, 128, NULL)
 * This blends each palette color with (128,128,128) at 50%, then for each
 * (src, dst) pair finds the closest palette match.
 *
 * Actually, create_trans_table works as: for each (x, y) pair,
 * result = (pal[x] * (255-r) + pal[y] * r) / 255
 * where r,g,b = 128,128,128 means 50% blend.
 */
void palette_build_trans_colormap(const RGBA_PALETTE *pal, INDEX_COLORMAP *out)
{
    int x, y;

    for (x = 0; x < PAL_COLORS; x++)
    {
        for (y = 0; y < PAL_COLORS; y++)
        {
            /* 50% blend: (src + dst) / 2 */
            uint8_t r = (uint8_t)(((int)pal->colors[x].r + (int)pal->colors[y].r) >> 1);
            uint8_t g = (uint8_t)(((int)pal->colors[x].g + (int)pal->colors[y].g) >> 1);
            uint8_t b = (uint8_t)(((int)pal->colors[x].b + (int)pal->colors[y].b) >> 1);

            out->data[x][y] = palette_find_closest(pal, r, g, b);
        }
    }
}


/* ========================================================================== */
/*
 * SHADOW colormap: direct lookup from extended D2 palette data.
 *
 * The raw D2 palette file is larger than 1024 bytes. Bytes 1024+ contain
 * color transformation tables used for shadow rendering.
 * For each color index c, the shadow table starts at offset 1024 + (256 * (c/8)).
 *
 * Original code:
 *   start = 1024 + (256 * (c/8));
 *   for (i=0; i<256; i++)
 *       cmap.data[c][i] = d2_pal[start + i];
 */
void palette_build_shadow_colormap(const uint8_t *d2_pal, long d2_pal_size,
                                   INDEX_COLORMAP *out)
{
    int c, i;

    memset(out, 0, sizeof(INDEX_COLORMAP));

    if (d2_pal == NULL)
        return;

    for (c = 0; c < PAL_COLORS; c++)
    {
        long start = 1024 + (256 * (c / 8));

        for (i = 0; i < PAL_COLORS; i++)
        {
            if (start + i < d2_pal_size)
                out->data[c][i] = d2_pal[start + i];
            else
                out->data[c][i] = 0;
        }
    }
}
