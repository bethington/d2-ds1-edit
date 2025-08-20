#ifndef _GFX_CUSTOM_H_

#define _GFX_CUSTOM_H_

void stretch_trans_sprite_8bpp(BITMAP * dst, BITMAP * sprite, int x0, int y0, int div);
void stretch_trans_shadow_8bpp(BITMAP * dst, BITMAP * sprite, int x0, int y0, int div, UBYTE * cmap_ptr, int offy);

#endif
