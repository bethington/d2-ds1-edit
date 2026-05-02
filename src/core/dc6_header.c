#include <stddef.h>

#include "core/dc6_header.h"

static unsigned int read_u32_le(const unsigned char *p)
{
   return ((unsigned int) p[0])
        | ((unsigned int) p[1] << 8)
        | ((unsigned int) p[2] << 16)
        | ((unsigned int) p[3] << 24);
}

int dc6_header_is_single_frame(const void *bytes, long len)
{
   const unsigned char *p;
   unsigned int directions;
   unsigned int frames_per_dir;

   if (bytes == NULL || len < DC6_HEADER_MIN_BYTES)
      return -1;

   p = (const unsigned char *) bytes;

   directions     = read_u32_le(p + 16);
   frames_per_dir = read_u32_le(p + 20);

   if (directions == 0 || frames_per_dir == 0)
      return -1;
   if (directions > 1024 || frames_per_dir > 65536)
      return -1;

   return (directions == 1 && frames_per_dir == 1) ? 1 : 0;
}
