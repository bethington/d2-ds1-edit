/* Compare a map loaded into glb_ds1[] with the authored spec it came from,
 * applying only the reader's documented interpretations (+1 dimensions,
 * act default/clamp, legacy orientation remap, layer-count defaults, default
 * path action). Unity assertions; call from inside a test. */
#ifndef DS1EDIT_TEST_DS1_EXPECT_H
#define DS1EDIT_TEST_DS1_EXPECT_H

#include "ds1_fixture.h"

/* Flags controlling which interpretations to expect. */
#define DS1X_AS_READ       0  /* spec was read directly by ds1_read */
#define DS1X_AS_RESAVED    1  /* spec was read, saved by ds1_save, re-read */

void ds1_expect_matches_spec(int ds1_idx, const FX_SPEC *s, int mode);

#endif
