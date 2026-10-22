/* Checked, recoverable DS1 saving.
 *
 * ds1_save_checked() serializes a map into a sibling staging file, checking
 * every write and the close, and only then publishes it under the map's name
 * with the editor's existing backup policy:
 *
 *   real save: an existing <name>.ds1 is renamed to the first free
 *              <name>-NNN.ds1, then the staging file becomes <name>.ds1.
 *              If that last rename fails the backup is renamed back.
 *   .tmp save: the staging file replaces <name>.tmp (the rolling autosave).
 *
 * A failed save returns -1 with a message, leaves the existing destination's
 * bytes in place (or, if even the rollback failed, names the backup that
 * holds them), removes only its own staging file, and changes no map state:
 * the map keeps its name (copy-on-save redirection is committed only after a
 * successful publish) and its save counter.
 */
#ifndef DS1EDIT_DS1_SAVE_IO_H
#define DS1EDIT_DS1_SAVE_IO_H

#include <stdio.h>
#include <stddef.h>

typedef struct DS1_SAVE_IO
{
   FILE * (*open)      (const char * path, const char * mode);
   size_t (*write)     (const void * data, size_t size, size_t count, FILE * f);
   int    (*write_byte)(int c, FILE * f);
   int    (*close)     (FILE * f);
   int    (*rename)    (const char * from, const char * to);
   int    (*remove)    (const char * path);
} DS1_SAVE_IO;

/* The stdio implementation used by the editor. */
const DS1_SAVE_IO * ds1_save_default_io(void);

/* Save map `ds1_idx` (to <name>.tmp when is_tmp_file). 0 on success (also
 * when the slot holds no map), -1 on failure with a message in `error`. */
int ds1_save_checked(int ds1_idx, int is_tmp_file, char * error, size_t error_cap);

/* Same, through caller-supplied file operations (tests inject failures). */
int ds1_save_with_io(int ds1_idx, int is_tmp_file, const DS1_SAVE_IO * io,
                     char * error, size_t error_cap);

/* Suffix of the staging file written next to the destination. */
#define DS1_SAVE_STAGING_SUFFIX ".saving"

#endif
