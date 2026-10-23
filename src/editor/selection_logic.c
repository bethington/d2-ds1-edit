#include "selection_logic.h"

int selection_tile_multiset_equal(const int *a, size_t a_count,
                                  const int *b, size_t b_count)
{
   size_t i, j;
   if (a_count != b_count)
      return 0;
   for (i = 0; i < a_count; i++)
   {
      size_t a_seen = 0, b_seen = 0;
      for (j = 0; j < a_count; j++)
      {
         if (a[j] == a[i]) a_seen++;
         if (b[j] == a[i]) b_seen++;
      }
      if (a_seen != b_seen)
         return 0;
   }
   return 1;
}

int selection_point_in_box(int x1, int y1, int x2, int y2,
                           int point_x, int point_y)
{
   int left = x1 < x2 ? x1 : x2;
   int right = x1 > x2 ? x1 : x2;
   int top = y1 < y2 ? y1 : y2;
   int bottom = y1 > y2 ? y1 : y2;
   return point_x >= left && point_x <= right &&
          point_y >= top && point_y <= bottom;
}
