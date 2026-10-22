/* Derived from win_ds1edit by Paul Siramy (originally ds1misc.h and ds1save.h).
 * See NOTICE at the repository root for attribution and license status. */

#ifndef _DS1MISC_H_

#define _DS1MISC_H_

#include <stddef.h>

int  ds1_free              (int i);
void wprop_2_block         (int i, CELL_W_S * w_ptr);
void fprop_2_block         (int i, CELL_F_S * f_ptr);
void sprop_2_block         (int i, CELL_S_S * s_ptr);
void ds1_make_prop_2_block (int i);
int  ds1_read              (const char * ds1name, int ds1_idx, int new_width, int new_height);

/* Like ds1_read(), but reports a file that cannot be loaded instead of
 * ending the program: returns 0 on success, or -1 with a message in `error`
 * and the slot left empty. Truncated or inconsistent files are rejected
 * before anything is read past their end; the legacy short group/path tails
 * that vanilla maps rely on are still accepted. */
int  ds1_read_checked      (const char * ds1name, int ds1_idx, int new_width, int new_height,
                            char * error, size_t error_cap);
/* Save a map (see core/ds1_save_io.h for the checked version and the
 * backup policy). Returns 0 on success, -1 if nothing was saved. */
int  ds1_save              (int ds1_idx, int is_tmp_file);

#endif
