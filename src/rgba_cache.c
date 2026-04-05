#include <stdlib.h>
#include <string.h>
#include "rgba_cache.h"
#include <allegro5/allegro.h>


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


/* ========================================================================== */
ALLEGRO_BITMAP *cache_tile_to_a5_bitmap(CACHED_TILE *tile, const RGBA_PALETTE *pal)
{
    ALLEGRO_BITMAP *bmp;
    ALLEGRO_LOCKED_REGION *lock;
    int y;

    if (tile == NULL || pal == NULL)
        return NULL;

    /* Rebuild RGBA data from current palette */
    cache_tile_rebuild(tile, pal);

    bmp = al_create_bitmap(tile->width, tile->height);
    if (bmp == NULL)
        return NULL;

    lock = al_lock_bitmap(bmp, ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
                          ALLEGRO_LOCK_WRITEONLY);
    if (lock == NULL)
    {
        al_destroy_bitmap(bmp);
        return NULL;
    }

    /* Copy RGBA data row by row (lock->pitch may differ from width*4) */
    for (y = 0; y < tile->height; y++)
    {
        memcpy(
            (uint8_t *)lock->data + y * lock->pitch,
            tile->rgba + y * tile->width,
            tile->width * sizeof(uint32_t)
        );
    }

    al_unlock_bitmap(bmp);
    return bmp;
}
