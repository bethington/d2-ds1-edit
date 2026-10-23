/* Owned scratch directories and small file helpers for package C tests.
 *
 * Every file a map-integrity test creates lives under a root made by
 * tt_root_create(): a fresh directory below the test's working directory
 * whose name carries the process id, so two runs never share files and the
 * tests never touch the user's maps, MPQs or the editor's own tmp/ folder.
 */
#ifndef DS1EDIT_TEST_TMP_H
#define DS1EDIT_TEST_TMP_H

#include <stddef.h>

/* Create "<cwd>/pkgc_<tag>_<pid>" (removing any leftover of the same name)
 * and write its path, with a trailing separator, into `out`. 1 on success. */
int  tt_root_create(const char *tag, char *out, size_t cap);

/* Recursively delete a directory tree this test created. */
void tt_remove_tree(const char *path);

/* mkdir one directory (parent must exist). 1 on success or if it exists. */
int  tt_mkdir(const char *path);

int  tt_write_file(const char *path, const void *data, size_t len);
/* malloc'd contents (caller frees) or NULL; *len receives the size. */
unsigned char *tt_read_file(const char *path, size_t *len);
int  tt_file_exists(const char *path);
/* 1 if both files exist and hold identical bytes. */
int  tt_files_equal(const char *a, const char *b);
/* Number of directory entries (files and dirs) directly inside `dir`. */
int  tt_count_entries(const char *dir);

#endif
