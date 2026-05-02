#ifndef _ANIMDATA_H_

#define _ANIMDATA_H_

#include "types.h"  /* UBYTE */

UBYTE animdata_hash_value   (char * name);
void  animdata_load         (void);
int   animdata_get_cof_info (char * name, long * fpd, long * speed);

// Quiet variant of animdata_get_cof_info: same lookup, no printf
// debug output. Use this from compose loops that call thousands of
// times. Returns 0 on success, -1 on miss. fpd and speed are zeroed
// on miss.
int   animdata_get_cof_info_quiet(const char * name, long * fpd, long * speed);

// Convert D2's animation speed value to a per-frame delay in
// milliseconds. The D2 animation engine ticks at 25 FPS (40 ms per
// tick) and advances by speed/256 frames per tick, so each frame's
// duration is 256 * 40 / speed milliseconds. A speed of 0 (or a
// suspiciously large/small value) returns default_ms.
int   animdata_speed_to_frame_delay_ms(long speed, int default_ms);

// Convenience: look up the COF in animdata and return per-frame
// delay in milliseconds. Returns default_ms on lookup failure.
int   animdata_get_frame_delay_ms(const char * cof_name, int default_ms);

#endif
