/* Derived from win_ds1edit by Paul Siramy (originally dc6info.h).
 * See NOTICE at the repository root for attribution and license status. */

#ifndef _DC6_INFO_H_

#define _DC6_INFO_H_

void dc6_decomp_norm (void * src, ALLEGRO_BITMAP * dst, long size, int x0, int y0);
void dc6_decomp_cmap (void * src, ALLEGRO_BITMAP * dst, long size, int x0, int y0, UBYTE * cmap);
int  anim_load_dc6   (char * name, COF_S * cof, int lay_idx, long user_dir, UBYTE * palshift);

#endif
