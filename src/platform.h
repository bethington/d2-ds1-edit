/*
 * platform.h -- the thin layer between this codebase and the OS.
 *
 * The editor grew up on Win32 and still speaks Win32 in places: backslash
 * separators, one-argument _mkdir, FindFirstFile enumeration. Rather than
 * rewrite those call sites into something nominally portable and subtly
 * wrong, this header gives each one an explicit POSIX counterpart.
 *
 * Two rules worth remembering when touching path code here:
 *
 *   1. MPQ *virtual* paths always use backslash, on every platform. They are
 *      hashed by the archive, not resolved by the OS, and D2 stores them with
 *      backslashes. Never "fix" a backslash inside an MPQ path.
 *
 *   2. *Filesystem* paths must use DS1_SEP. Everything the editor writes --
 *      exports, previews, project files, Debug/ -- goes through the OS.
 *
 * Confusing the two is the single easiest way to break the non-Windows build
 * in a way that still compiles.
 */

#ifndef DS1EDIT_PLATFORM_H
#define DS1EDIT_PLATFORM_H

#include <stddef.h>
#include <stdio.h>

#ifdef WIN32
   #include <direct.h>
   #include <io.h>
#else
   #include <sys/stat.h>
   #include <sys/types.h>
   #include <dirent.h>
   #include <strings.h>
   #include <unistd.h>
#endif

/* ---- case-insensitive compare ---------------------------------------- */
/* Scattered translation units each defined their own shim for this; new code
   should include this header instead of adding another one. */
#ifdef WIN32
   #ifndef strcasecmp
      #define strcasecmp  _stricmp
   #endif
   #ifndef strncasecmp
      #define strncasecmp _strnicmp
   #endif
#endif

/* ---- directory creation ---------------------------------------------- */
/* MSVC's _mkdir takes one argument, POSIX mkdir takes two. Returns 0 on
   success, non-zero on failure (errno set), matching both originals. */
#ifdef WIN32
   #define DS1_MKDIR(path) _mkdir(path)
#else
   #define DS1_MKDIR(path) mkdir((path), 0755)
#endif

/* ---- process pipes ---------------------------------------------------- */
/* MSVC prefixes these with an underscore; POSIX does not.
 *
 * The return values differ in a way that silently corrupts comparisons:
 * _pclose gives the child's exit code directly, while POSIX pclose gives a
 * wait status, where the exit code lives in the high byte (exit 3 -> 768).
 * DS1_PCLOSE normalises to the exit code, and reports a signal death as
 * 128 + signum the way a shell does.
 */
#ifdef WIN32
   #define DS1_POPEN(cmd, mode)  _popen((cmd), (mode))
   /* Shell redirect target for output nobody wants to see. */
   #define DS1_DEVNULL           "NUL"
   #define DS1_PCLOSE(fp)        _pclose(fp)
#else
   #include <sys/wait.h>
   #define DS1_POPEN(cmd, mode)  popen((cmd), (mode))
   #define DS1_DEVNULL           "/dev/null"
   #define DS1_PCLOSE(fp)        ds1_pclose_status(fp)

   static inline int ds1_pclose_status(FILE *fp)
   {
      int st = pclose(fp);
      if (st == -1) return -1;
      if (WIFEXITED(st))   return WEXITSTATUS(st);
      if (WIFSIGNALED(st)) return 128 + WTERMSIG(st);
      return st;
   }
#endif

/* ---- path separators -------------------------------------------------- */
/* DS1_SEP is for paths handed to the OS. MPQ virtual paths keep '\\'. */
#ifdef WIN32
   #define DS1_SEP      '\\'
   #define DS1_SEP_STR  "\\"
#else
   #define DS1_SEP      '/'
   #define DS1_SEP_STR  "/"
#endif

/* True for either separator, so code can parse paths that came from a config
   file written on the other platform. */
#define DS1_IS_SEP(c) ((c) == '/' || (c) == '\\')

/* Rewrite every separator in `path` (in place) to the native one. Safe to
   call on a NULL pointer. Do NOT call this on an MPQ virtual path. */
void ds1_path_normalize(char *path);

/* ---- directory enumeration -------------------------------------------- */
/*
 * A minimal read-only directory iterator, because FindFirstFileA and
 * opendir/readdir disagree about almost everything. Usage:
 *
 *    DS1_DIR d;
 *    if (ds1_dir_open(&d, "some/dir")) {
 *       while (ds1_dir_next(&d)) {
 *          if (d.is_dir) ...
 *          use d.name;          // basename only, never "." or ".."
 *       }
 *       ds1_dir_close(&d);
 *    }
 *
 * `name` is a basename, not a full path, on both platforms. "." and ".." are
 * skipped by ds1_dir_next so callers never have to filter them.
 */
#define DS1_DIR_NAME_MAX 260

typedef struct DS1_DIR_S
{
   char name[DS1_DIR_NAME_MAX];  /* basename of the current entry */
   int  is_dir;                  /* non-zero if the entry is a directory */

   /* -- internals; do not touch from calling code -- */
#ifdef WIN32
   void *handle;                 /* HANDLE from FindFirstFileA */
   void *find_data;              /* heap LPWIN32_FIND_DATAA */
   int   pending;                /* first entry already fetched by open */
#else
   void *handle;                 /* DIR * */
   char  root[1024];             /* needed to stat() entries for is_dir */
#endif
} DS1_DIR;

/* Returns non-zero on success. A directory that does not exist is a plain
   failure, not an error worth reporting. */
int  ds1_dir_open(DS1_DIR *d, const char *path);

/* Advances to the next entry. Returns non-zero while entries remain. */
int  ds1_dir_next(DS1_DIR *d);

/* Idempotent; safe on a failed open. */
void ds1_dir_close(DS1_DIR *d);

/* ---- filesystem queries ----------------------------------------------- */
int ds1_dir_exists(const char *path);
int ds1_file_exists(const char *path);

/*
 * Resolve `relative` under `root` case-insensitively, writing the real path
 * into `out`. Returns non-zero if something was found.
 *
 * This exists for the mod_dir overlay. D2's TXT and COF data reference files
 * in whatever case the original authors typed, which Windows and the MPQ hash
 * both forgive and ext4 does not. On Windows this is a straight join; on
 * POSIX it walks the path component by component, comparing case-insensitively
 * only when an exact match fails.
 */
int ds1_path_resolve_nocase(const char *root, const char *relative,
                            char *out, size_t out_cap);

#endif /* DS1EDIT_PLATFORM_H */
