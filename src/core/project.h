#ifndef _PROJECT_H_
#define _PROJECT_H_

// A "project" is a folder on disk that holds the user's mod-in-progress as
// loose files. The folder contains a project.ini with metadata; content is
// laid out at in-game paths (e.g. <project>/data/global/tiles/...).
// The folder doubles as the editor's overlay mod_dir at load time.

#define PROJECT_PATH_MAX          512
#define PROJECT_NAME_MAX          128
#define PROJECT_EXTRA_MPQ_MAX       8

typedef struct PROJECT_S
{
   int  is_open;
   char path       [PROJECT_PATH_MAX];  // directory
   char name       [PROJECT_NAME_MAX];
   char d2_install [PROJECT_PATH_MAX];  // project's bound D2 install
   char extra_mod_mpqs[PROJECT_EXTRA_MPQ_MAX][PROJECT_PATH_MAX];
   int  extra_count;
} PROJECT_S;

extern PROJECT_S glb_project;

// Build the project.ini path for a project directory. Returns 1 on success.
int  project_ini_path(const char *project_dir, char *out, int out_cap);

// Create a new project on disk. Creates `path` if absent, writes a fresh
// project.ini with `name` and `d2_install`. Returns 1 on success.
// Fails if project.ini already exists at that location.
int  project_create(const char *path, const char *name,
                    const char *d2_install);

// Load a project's metadata from <path>/project.ini into glb_project.
// Does not apply the project to glb_config (see project_apply_to_config).
// Returns 1 on success, 0 on failure.
int  project_load(const char *path);

// Persist glb_project back to its project.ini. Returns 1 on success.
int  project_save(void);

// Clears glb_project (in-memory only; file is left on disk).
void project_close(void);

// Apply the currently-open project to glb_config: sets mod_dir[0] to the
// project directory and, if glb_config.d2_install is empty, fills it from
// the project. Safe to call multiple times.
void project_apply_to_config(void);

// Recursively mkdir every intermediate directory of `path` (not including
// the final basename). Returns 1 on success, 0 on any hard failure.
int  project_ensure_parent_dirs(const char *path);

// Copy-on-save: if a project is open and `src` lives outside it, rewrite
// the path into the project's Global\Tiles\<suffix> tree. `suffix` is the
// tail after the last "/tiles/" or "\tiles\" segment in `src`.
// Writes the resolved target path into `dst`.
//
// Returns 1 if the path was actually redirected (i.e., a copy-on-save just
// happened); 0 if it was passed through unchanged (no project, already
// inside the project, or no recognisable tiles segment to rewrite from).
int  project_redirect_ds1_save_path(const char *src, char *dst, int dst_cap);

#endif
