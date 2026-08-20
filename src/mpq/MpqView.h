/* Stormless MPQ sample code by Tom Amigo (http://www.angelfire.com/sc/mpq/, 2000),
 * as adapted by Paul Siramy for win_ds1edit. See NOTICE at the repository root
 * for attribution and license status. */

#ifndef _MPQVIEW_H_

#define  _MPQVIEW_H_

int  mod_load_in_mem       (char * moddir, char * filename, void ** buffer, long * buf_len);
int  mpq_load_in_mem       (char * mpqname, char * filename, void ** buffer, long * buf_len, int output);
void mpq_batch_open        (char * mpqname);
int  mpq_batch_load_in_mem (char * filename, void ** buffer, long * buf_len, int output);
void mpq_batch_close       (void);

#endif
