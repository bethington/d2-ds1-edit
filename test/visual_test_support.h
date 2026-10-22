/* Shared harness for the visual/export (package D) tests.
 *
 * Provides the editor globals the decoders and loaders reach for, and an
 * in-memory replacement for the archive fetch (misc_load_mpq_file) so the real
 * production lookup/parse/decode code runs on authored bytes. Nothing here
 * decodes or interprets assets. */

#ifndef VISUAL_TEST_SUPPORT_H
#define VISUAL_TEST_SUPPORT_H

#include <stddef.h>

/* Forget every registered file. */
void visual_fetch_reset(void);

/* Serve `bytes` (copied) whenever the code under test loads `name`
 * (case-insensitive, '/' and '\\' equivalent). Returns 1 on success. */
int  visual_fetch_add(const char *name, const void *bytes, size_t len);

/* The archive fetch this support file replaces (production prototype in
 * misc.h, which pulls in the whole editor). */
int  misc_load_mpq_file(char *filename, char **buffer, long *buf_len, int output);

/* Number of fetches answered / refused since the last reset. */
int  visual_fetch_hits(void);
int  visual_fetch_misses(void);

/* Read a whole file; caller frees. NULL on failure. */
unsigned char *visual_read_file(const char *path, size_t *len);

/* al_init() once, and make new bitmaps memory bitmaps (no display needed).
 * Returns 1 on success. */
int  visual_allegro_init(void);

/* Errors the code under test reported through ds1edit_error(). */
int         visual_error_count(void);
const char *visual_last_error(void);

#endif
