/* Derived from win_ds1edit by Paul Siramy (originally dt1misc.h).
 * See NOTICE at the repository root for attribution and license status. */

#ifndef _DT1MISC_H_

#define _DT1MISC_H_

int dt1_already_loaded(char *dt1name, int *idx);
int dt1_free(int i);
int dt1_del(int i);
void dt1_bh_update(int i);
void dt1_fill_subt(SUB_TILE_S *ptr, int i, long tiles_ptr, int s);
void dt1_zoom(ALLEGRO_BITMAP *src, int i, int b, int z,
              const uint8_t *src_indices, int src_w, int src_h);
void dt1_all_zoom_make(int i);
int dt1_struct_update(int i);
int dt1_add(char *dt1name);
int dt1_add_special(char *dt1name);
void dt1_rebuild_bitmaps_from_cache(const RGBA_PALETTE *pal);

/* Hold every loaded DT1 across a map switch so a shared tileset is not
 * freed by the teardown and immediately decoded again. Retain before
 * unloading the old map, release after loading the new one. */
void dt1_retain_loaded    (void);
void dt1_release_retained (void);

#endif
