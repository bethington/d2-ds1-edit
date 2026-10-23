/* Link stubs for test_tile_properties: hooks ds1.c reaches outside the
 * cell -> block association under test (reading/saving maps, objects, view
 * state). The tests never call them; each fails loudly if it is reached. */

#include <stdio.h>
#include <stdlib.h>
#include "structs.h"

char           glb_tiles_path[30] = "Data\\Global\\Tiles\\";
WRKSPC_DATAS_S glb_wrkspc_datas[WRKSPC_MAX];

static void unexpected(const char *what)
{
   fprintf(stderr, "test_tile_properties: unexpected call to %s\n", what);
   abort();
}

void change_zoom(int ds1_idx, ZOOM_E z) { (void) ds1_idx; (void) z; unexpected("change_zoom"); }
int ds1_save_checked(int ds1_idx, int is_tmp_file, char *error, size_t error_cap)
{
   (void) ds1_idx; (void) is_tmp_file; (void) error; (void) error_cap;
   unexpected("ds1_save_checked");
   return -1;
}
int dt1_del(int i) { (void) i; unexpected("dt1_del"); return 0; }
void editobj_make_obj_desc(int ds1_idx, int obj_idx) { (void) ds1_idx; (void) obj_idx; unexpected("editobj_make_obj_desc"); }
void editobj_make_obj_label(int ds1_idx) { (void) ds1_idx; unexpected("editobj_make_obj_label"); }
void editobj_set_drawing_order(int ds1_idx) { (void) ds1_idx; unexpected("editobj_set_drawing_order"); }
const char *find_tiles_suffix(const char *path) { (void) path; unexpected("find_tiles_suffix"); return NULL; }
int misc_increase_ds1_objects_max(int ds1_idx, long nb_objects)
{
   (void) ds1_idx; (void) nb_objects;
   unexpected("misc_increase_ds1_objects_max");
   return 1;
}
