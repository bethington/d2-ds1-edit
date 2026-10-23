#ifndef DS1EDIT_SELECTION_LOGIC_H
#define DS1EDIT_SELECTION_LOGIC_H

#include <stddef.h>

int selection_tile_multiset_equal(const int *a, size_t a_count,
                                  const int *b, size_t b_count);
int selection_point_in_box(int x1, int y1, int x2, int y2,
                           int point_x, int point_y);

#endif
