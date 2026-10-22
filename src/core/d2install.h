#ifndef _D2INSTALL_H_
#define _D2INSTALL_H_

#include <stddef.h>
#include "structs.h"

#define D2INSTALL_PATH_MAX 512

/* ---- injectable environment ------------------------------------------
 * Detection never touches the registry or the disk directly: it asks an
 * environment. d2install_real_env() is the production one (registry on
 * Windows, nothing elsewhere); tests pass fakes, so every precedence rule
 * is checked identically on every platform.
 * Any callback may be NULL (treated as "nothing there"). */

typedef enum
{
   D2REG_HKCU,         /* HKEY_CURRENT_USER                               */
   D2REG_HKLM_NATIVE,  /* HKEY_LOCAL_MACHINE, 64-bit view                 */
   D2REG_HKLM_WOW64    /* HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node view    */
} D2REG_ROOT;

typedef struct D2INSTALL_ENV
{
   void *ctx;

   /* Find <dir>/<name> ignoring filename case. On success write the path
    * that really exists to `out` and return 1. */
   int (*find_file)(void *ctx, const char *dir, const char *name,
                    char *out, size_t cap);

   /* Software\Blizzard Entertainment\Diablo II : InstallPath. */
   int (*reg_install_path)(void *ctx, D2REG_ROOT root, char *out, int cap);

   /* Uninstall entry whose DisplayName equals `display_name`: its
    * InstallLocation. Roots map to HKCU, HKLM and HKLM\WOW6432Node. */
   int (*reg_uninstall_location)(void *ctx, D2REG_ROOT root,
                                 const char *display_name,
                                 char *out, int cap);

   /* NULL-terminated list probed last; NULL = the built-in defaults. */
   const char * const *common_paths;
} D2INSTALL_ENV;

/* Where an answer came from (for logs and tests). */
typedef enum
{
   D2SRC_NONE = 0,
   D2SRC_INI,            /* d2_install / pd2_dir from the ini (or CLI)      */
   D2SRC_REG_HKCU,       /* HKCU Blizzard InstallPath                        */
   D2SRC_REG_HKLM,       /* HKLM Blizzard InstallPath, native view           */
   D2SRC_REG_WOW64,      /* HKLM Blizzard InstallPath, WOW6432Node view      */
   D2SRC_UNINSTALL,      /* Uninstall entry InstallLocation                  */
   D2SRC_COMMON,         /* a common install folder                          */
   D2SRC_SIBLING         /* <Diablo II>\ProjectD2                            */
} D2INSTALL_SRC;

const char *d2install_src_name(D2INSTALL_SRC src);

/* Everything the resolver reads from the configuration. */
typedef struct D2INSTALL_CONFIG
{
   const char *d2_install;                 /* NULL / "" = auto-detect      */
   const char *pd2_dir;                    /* NULL / "" = auto-detect      */
   int         use_pd2;                    /* D2_USE_PD2_*                 */
   const char *explicit_mpq[MAX_MPQ_FILE]; /* NULL / "" = not set          */
} D2INSTALL_CONFIG;

typedef struct D2INSTALL_RESULT
{
   char          d2_dir[D2INSTALL_PATH_MAX];    /* "" = no install found   */
   D2INSTALL_SRC d2_source;
   char          pd2_dir[D2INSTALL_PATH_MAX];   /* "" = no PD2 found       */
   D2INSTALL_SRC pd2_source;
   int           pd2_active;                    /* PD2 archives in the chain */
   int           pd2_required_missing;          /* use_pd2=yes, not found  */
   /* Chain slots to fill, indexed like glb_config.mpq_file[]. "" = leave
    * the slot alone (not found, or the ini set it explicitly). */
   char          mpq[MAX_MPQ_FILE][D2INSTALL_PATH_MAX];
   int           filled;                        /* non-empty entries in mpq[] */
} D2INSTALL_RESULT;

/* Find the Diablo II install. Order, first valid one wins:
 *   `ini_dir` -> HKCU Blizzard InstallPath -> HKLM native -> HKLM WOW64 ->
 *   Uninstall "Diablo II" (HKCU, HKLM, WOW6432Node) -> common paths.
 * Valid = holds d2data.mpq or d2exp.mpq. A configured `ini_dir` is
 * authoritative: if it is set but not valid it is returned anyway (so a typo
 * is visible and never silently replaced by another install) with
 * D2SRC_INI. Returned path has no trailing separator. 1 if one is found. */
int d2install_find_d2(const D2INSTALL_ENV *env, const char *ini_dir,
                      char *out, int out_cap, D2INSTALL_SRC *src);

/* Find the Project Diablo 2 folder. Order, first valid one wins:
 *   `ini_dir` -> Uninstall "Project Diablo 2" InstallLocation\ProjectD2 ->
 *   <d2_dir>\ProjectD2 (when d2_dir is non-empty).
 * Valid = holds pd2data.mpq. 1 if found. */
int d2install_find_pd2(const D2INSTALL_ENV *env, const char *ini_dir,
                       const char *d2_dir, char *out, int out_cap,
                       D2INSTALL_SRC *src);

/* The pure resolver: install, PD2 and the ordered archive list. */
void d2install_resolve(const D2INSTALL_ENV *env, const D2INSTALL_CONFIG *cfg,
                       D2INSTALL_RESULT *out);

/* Production environment (registry on Windows; file probing everywhere). */
const D2INSTALL_ENV *d2install_real_env(void);

/* Replace the environment d2install_resolve_mpqs()/d2install_detect() use
 * (NULL restores the real one). For tests, so they never read the host's
 * registry. */
void d2install_set_env(const D2INSTALL_ENV *env);

/* 1 if `dir` holds a Diablo II archive (real file system). */
int d2install_is_install_dir(const char *dir);

/* Detect the install with the real environment. Writes the directory (no
 * trailing separator) into `out_path` and returns 1, or returns 0. */
int d2install_detect(char *out_path, int out_cap);

/* Resolve the archive chain from `glb_config` (d2_install, pd2_dir, use_pd2,
 * explicit mpq_file[] entries) into the empty slots of
 * `glb_config.mpq_file[]`, using the real environment. Explicit per-MPQ
 * paths already set are never overridden. Caches the install in
 * glb_config.d2_install, and the PD2 folder in glb_config.pd2_dir when used.
 * Logs which data set is active. Returns the number of slots populated. */
int d2install_resolve_mpqs(void);

/* "Project Diablo 2" or "vanilla": the data set the chain resolved to, for
 * the UI and logs. */
const char *d2install_data_set_name(void);

#endif
