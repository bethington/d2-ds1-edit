/* Isolated editor state for map-integrity tests.
 *
 * The editor keeps its maps in globals (glb_ds1[], glb_config, glb_ds1edit)
 * normally defined in main.c, which a unit test cannot link. This fixture
 * defines those globals, allocates small in-memory maps the way ds1_read()
 * lays them out, points the undo scratch directory at an owned temporary
 * root, and tears everything down again so each test starts clean.
 *
 * It also traps ds1edit_error(): production code calls it to report a fatal
 * error and exit. In tests the call is recorded and, when a trap is armed,
 * control returns to the test through longjmp so the failure can be asserted
 * instead of ending the process.
 */
#ifndef DS1EDIT_EDITOR_TEST_STATE_H
#define DS1EDIT_EDITOR_TEST_STATE_H

#include <setjmp.h>
#include <string.h>
#include "structs.h"

/* Allocate glb_ds1[] (DS1_MAX slots), reset globals, create the owned temp
 * root and build `map_count` maps of width x height in slots 0..map_count-1
 * with two wall layers, two floor layers and one shadow layer, all zero.
 * Returns 1 on success. */
int  editor_test_state_init(int map_count, int width, int height);

/* Free every used slot (maps built here or loaded with ds1_read), close undo
 * files and delete the temp root. */
void editor_test_state_destroy(void);

/* Owned temp root, with trailing separator, e.g. "pkgc_state_1234\". */
const char *editor_test_state_root(void);

/* Build "<root><name>" into out. */
void editor_test_state_path(char *out, size_t cap, const char *name);

/* Release one slot (used by tests that read several files into one slot). */
void editor_test_state_free_slot(int ds1_idx);

/* ---- ds1edit_error trap ---- */
extern jmp_buf *editor_test_error_trap;
extern char     editor_test_last_error[1024];
extern int      editor_test_error_count;

/* Run `stmt`; evaluates to 1 if ds1edit_error() was called during it. */
#define EDITOR_TEST_TRAPS_ERROR(stmt, out_flag)                     \
   do {                                                              \
      jmp_buf _tj;                                                   \
      jmp_buf *_prev = editor_test_error_trap;                       \
      editor_test_error_trap = &_tj;                                 \
      if (setjmp(_tj) == 0) { stmt; (out_flag) = 0; }                \
      else                  { (out_flag) = 1; }                      \
      editor_test_error_trap = _prev;                                \
   } while (0)

/* Optional single-test filter: main() passes argv[1]; PKGC_RUN runs only the
 * tests whose name contains it (useful to observe one crashing case). */
extern const char *pkgc_test_filter;
#define PKGC_RUN(fn)    do { if (pkgc_test_filter == NULL || strstr(#fn, pkgc_test_filter) != NULL)            UnityDefaultTestRun(fn, #fn, __LINE__); } while (0)

#endif
