#ifndef _DT1MISC_H_

#define _DT1MISC_H_

int  dt1_already_loaded (char * dt1name, int * idx);
int  dt1_free           (int i);
int  dt1_del            (int i);
void dt1_bh_update      (int i);
void dt1_fill_subt      (SUB_TILE_S * ptr, int i, long tiles_ptr, int s);
void dt1_zoom           (ALLEGRO_BITMAP * src, int i, int b, int z,
                         const uint8_t *src_indices, int src_w, int src_h);
void dt1_all_zoom_make  (int i);
void dt1_struct_update  (int i);
int  dt1_add            (char * dt1name);
int  dt1_add_special    (char * dt1name);
void dt1_rebuild_bitmaps_from_cache(const RGBA_PALETTE *pal);

#endif
