/* A file copy that says whether it worked, for callers about to destroy the
 * original. On any failure after the destination was opened, the partial copy
 * is removed so it can't later be mistaken for a good one.
 */
#ifndef DS1EDIT_FILE_COPY_H
#define DS1EDIT_FILE_COPY_H

#define FILE_COPY_OK          0
#define FILE_COPY_NO_SOURCE  -1  /* the source can't be opened for reading */
#define FILE_COPY_FAILED     -2  /* the source is there but wasn't copied */

int file_copy_checked(const char * src_path, const char * dst_path);

#endif
