#ifndef _COMPOSE_COF_PATH_H_
#define _COMPOSE_COF_PATH_H_

// Build the virtual asset path for a COF file used by the composer.
// D2 stores COFs at:
//
//   <base>\<token>\COF\<token><mode><wclass>.cof
//
// Examples:
//   data\global\chars\NE\COF\NEWLHTH.cof
//     (Necromancer, walk, bare hands)
//   data\global\chars\SO\COF\SONUSTF.cof
//     (Sorceress, idle, staff)
//   data\global\monsters\AN\COF\ANNUHTH.cof
//     (Andariel, idle, no weapon variation)
//
// Some monsters use a token-only COF without a weapon-class suffix
// (e.g. ANNU.cof). compose_cof_path_build_no_wclass covers that case.

// Build a COF path with explicit weapon class. Writes a NUL-terminated
// path of at most out_cap-1 chars into out_buf. Returns 1 on success,
// 0 on bad arguments or buffer too small (out_buf set to "").
int compose_cof_path_build(char *out_buf, int out_cap,
                           const char *base,
                           const char *token,
                           const char *mode,
                           const char *wclass);

#endif
