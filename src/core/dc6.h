/* Derived from win_ds1edit by Paul Siramy (originally dc6info.h).
 * See NOTICE at the repository root for attribution and license status. */

#ifndef _DC6_INFO_H_

#define _DC6_INFO_H_

#include <stddef.h>
#include "core/index_frame.h"

/* Raw decoding lives in core/index_frame.c (dc6_decode_indices); every DC6
 * consumer goes through it. */

int  anim_load_dc6   (char * name, COF_S * cof, int lay_idx, long user_dir, UBYTE * palshift);

#endif
