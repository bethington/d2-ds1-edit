/* Pure math helpers for animdata. Kept in its own translation unit so
 * unit tests can link the converter without dragging in glb_ds1edit
 * and the rest of the editor's global state. */

#include "core/animdata.h"

int animdata_speed_to_frame_delay_ms(long speed, int default_ms)
{
   long delay_ms;

   if (speed <= 0 || speed > 65535)
      return default_ms;

   /* (256 ticks per frame target) * (40 ms per tick) = 10240. */
   delay_ms = 10240L / speed;
   if (delay_ms <= 0)
      return default_ms;
   if (delay_ms > 10000)
      delay_ms = 10000;
   return (int) delay_ms;
}
