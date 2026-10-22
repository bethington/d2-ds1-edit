/* Derived from win_ds1edit by Paul Siramy.
 * See NOTICE at the repository root for attribution and license status. */

/* The bitmap-writing sub-tile decoders that lived here ran in parallel with
 * the index decoders in dt1_decode.c. DT1 blocks now decode once, to palette
 * indices, and the block bitmap is built from those (dt1_all_zoom_make); the
 * duplicate decoders are gone. This translation unit is kept so existing build
 * lists stay valid. */

#include "core/dt1_draw.h"

typedef int dt1_draw_unit_is_not_empty;
