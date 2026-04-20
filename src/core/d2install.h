#ifndef _D2INSTALL_H_
#define _D2INSTALL_H_

// Probe the Windows registry and common install locations for a Diablo II
// install. On success, writes the detected install directory (no trailing
// slash) into `out_path` and returns 1. Returns 0 if no install found.
// `out_cap` is the capacity of `out_path` in bytes.
int d2install_detect(char *out_path, int out_cap);

// Resolve the four Blizzard MPQ paths (patch_d2, d2exp, d2data, d2char) from
// `glb_config.d2_install` into empty slots of `glb_config.mpq_file[]`.
// Explicit per-MPQ paths already set by the user are never overridden.
// If `glb_config.d2_install` is empty, calls `d2install_detect()` first.
// Returns the number of slots newly populated (may be 0 if everything was
// already configured or nothing could be found).
int d2install_resolve_mpqs(void);

#endif
