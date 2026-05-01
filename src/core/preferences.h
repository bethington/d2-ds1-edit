#ifndef _PREFERENCES_H_
#define _PREFERENCES_H_

// Global, per-user editor preferences stored at
// %APPDATA%\ds1edit\preferences.ini. Survives across projects; seeded
// defaults for things like the D2 install path and the recent-project list.

#define PREFS_RECENT_MAX 10
#define PREFS_PATH_MAX   512

typedef struct PREFERENCES_S
{
   int  loaded;                                             // 1 if prefs_load() found an existing file
   char last_d2_install[PREFS_PATH_MAX];
   char recent_projects[PREFS_RECENT_MAX][PREFS_PATH_MAX];
   int  recent_count;
} PREFERENCES_S;

extern PREFERENCES_S glb_prefs;

// Resolve the preferences file path into `out`. On Windows uses %APPDATA%;
// returns 1 on success, 0 if APPDATA is unavailable.
int  prefs_resolve_path(char *out, int out_cap);

// Load from the default location (%APPDATA%\ds1edit\preferences.ini).
// Returns 1 if a file was loaded, 0 if no file existed (glb_prefs is
// zero-initialised either way).
int  prefs_load(void);

// Save glb_prefs to the default location. Creates the parent directory if
// needed. Returns 1 on success, 0 on failure.
int  prefs_save(void);

// Load/save variants with an explicit file path. Used by tests and by
// callers that manage the path themselves.
int  prefs_load_from (const char *path);
int  prefs_save_to   (const char *path);

// Move `project_path` to the front of the recent list, dropping any
// existing occurrence and trimming to PREFS_RECENT_MAX. Does not persist;
// call prefs_save() afterwards.
void prefs_record_recent_project(const char *project_path);

#endif
