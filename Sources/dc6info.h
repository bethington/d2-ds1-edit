#ifndef _DC6_INFO_H_

#define _DC6_INFO_H_

void dc6_decomp_norm (void * src, BITMAP * dst, long size, int x0, int y0);
void dc6_decomp_cmap (void * src, BITMAP * dst, long size, int x0, int y0, UBYTE * cmap);
int  anim_load_dc6   (char * name, COF_S * cof, int lay_idx, long user_dir, UBYTE * palshift);

#endif
