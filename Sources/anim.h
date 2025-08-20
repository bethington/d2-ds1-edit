#ifndef _ANIM_H_

#define _ANIM_H_

int     anim_load_dcc      (char * name, COF_S * cof, int lay_idx, long user_dir, UBYTE * palshift);
COF_S * anim_load_cof      (char * base, char * tok, char * mod, char * clas, long user_dir, int obj_line, int progress);
COF_S * anim_load_desc_gfx (int i, int progress);
int     anim_destroy_cof   (COF_S * cof);
void    anim_update_gfx    (int progress);
int     anim_exit          (void);


#endif
