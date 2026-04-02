#include <stdlib.h>
#include <string.h>
#include "rgba_cache.h"


/* ========================================================================== */
CACHED_TILE *cache_tile_create(int width, int height)
{
    CACHED_TILE *tile;
    int num_pixels;

    if (width <= 0 || height <= 0)
        return NULL;

    tile = (CACHED_TILE *)malloc(sizeof(CACHED_TILE));
    if (tile == NULL)
        return NULL;

    num_pixels = width * height;

    tile->indices = (uint8_t *)calloc(num_pixels, sizeof(uint8_t));
    tile->rgba = (uint32_t *)calloc(num_pixels, sizeof(uint32_t));
    tile->width = width;
    tile->height = height;

    if (tile->indices == NULL || tile->rgba == NULL)
    {
        free(tile->indices);
        free(tile->rgba);
        free(tile);
        return NULL;
    }

    return tile;
}


/* ========================================================================== */
void cache_tile_destroy(CACHED_TILE *tile)
{
    if (tile == NULL)
        return;

    free(tile->indices);
    free(tile->rgba);
    free(tile);
}


/* ========================================================================== */
void cache_tile_rebuild(CACHED_TILE *tile, const RGBA_PALETTE *pal)
{
    int i, num_pixels;

    if (tile == NULL || pal == NULL)
        return;

    num_pixels = tile->width * tile->height;

    for (i = 0; i < num_pixels; i++)
    {
        uint8_t idx = tile->indices[i];
        tile->rgba[i] = rgba_pack(pal->colors[idx]);
    }
}
