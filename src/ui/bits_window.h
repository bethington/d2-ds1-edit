/* Derived from win_ds1edit by Paul Siramy (originally wBits.h).
 * See NOTICE at the repository root for attribution and license status. */

#ifndef _WBITS_H_

#define _WBITS_H_

void wbits_main_single_tile    (int ds1_idx, int tx, int ty);
void wbits_apply_modification  (int ds1_idx, WBITSDATA_S * ptr_wbitsdata);
void wbits_main_multiple_tiles (int ds1_idx, WBITSDATA_S * ptr_wbitsdata);
void wbits_main                (int ds1_idx, int tx, int ty);

#endif
