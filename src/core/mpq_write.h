/* Writes MPQ archives the game itself loads: stored files only (flag
 * 0x80000000), format 0, header size 32, sector shift 3, header at offset 0,
 * no (attributes). Names are backslash paths, locale 0, platform 0. */
#ifndef DS1EDIT_MPQ_WRITE_H
#define DS1EDIT_MPQ_WRITE_H

typedef struct MPQ_WRITE_FILE_S
{
   const char * archive_name;   /* data\global\... with backslashes */
   const char * source_path;    /* a file on disk */
} MPQ_WRITE_FILE_S;

/* Write out_path from base_path (NULL for a new archive) plus these files. A file
 * whose name base_path has replaces it there. Writes a staging file and renames it
 * over out_path only when complete, so a failure leaves out_path as it was. base_path
 * is only read. 0, or -1 with why in error. */
int mpq_write(const char * base_path, const char * out_path,
              const MPQ_WRITE_FILE_S * files, int count, char * error, int cap);

#endif
