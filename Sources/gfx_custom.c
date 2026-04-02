#include "structs.h"
#include "gfx_custom.h"


// ==========================================================================
// draw a sprite at the zoom 1:div
// dst & sprite MUST be 8bpp color depth both
// dst & sprite MUST be linear bitmaps both (not plannar)
// if color_map point to NULL, regular sprite, else transparent one
void stretch_trans_sprite_8bpp(ALLEGRO_BITMAP * dst, ALLEGRO_BITMAP * sprite, int x0, int y0, int div)
{
   /* TODO: Implement with al_lock_bitmap for Allegro 5 */
   (void)dst; (void)sprite; (void)x0; (void)y0; (void)div;
}


// ==========================================================================
// draw the shadow projection of a sprite at the zoom 1:div
// dst & sprite MUST be 8bpp color depth both
// dst & sprite MUST be linear bitmaps both (not plannar)
// cmap must point on an array of 256 bytes
// offx and offy are the coordinates of the pivot point of the sprite
void stretch_trans_shadow_8bpp(ALLEGRO_BITMAP * dst, ALLEGRO_BITMAP * sprite, int x0, int y0, int div,
                               UBYTE * cmap, int offy)
{
   /* TODO: Implement with al_lock_bitmap for Allegro 5 */
   (void)dst; (void)sprite; (void)x0; (void)y0; (void)div;
   (void)cmap; (void)offy;
}
