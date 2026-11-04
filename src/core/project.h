#ifndef _PROJECT_H_
#define _PROJECT_H_

// A "project" is a folder on disk that holds the user's mod-in-progress as
// loose files. The folder contains a project.ini with metadata; content is
// laid out at in-game paths (e.g. <project>/data/global/tiles/...).
// The folder doubles as the editor's overlay mod_dir at load time.

#define PROJECT_PATH_MAX          512
#define PROJECT_NAME_MAX          128
#define PROJECT_EXTRA_MPQ_MAX       8
#define PROJECT_DEFAULT_TEST_ARGS   "-w -direct -txt"

typedef struct PROJECT_S
{
   int  is_open;
   char path       [PROJECT_PATH_MAX];  // directory
   char name       [PROJECT_NAME_MAX];
   char d2_install [PROJECT_PATH_MAX];  // project's bound D2 install
   char extra_mod_mpqs[PROJECT_EXTRA_MPQ_MAX][PROJECT_PATH_MAX];
   int  extra_count;
   // The launch profile: a separate test copy of the game that runs this
   // project's map. Never the install the project is bound to.
   char test_game_dir[PROJECT_PATH_MAX];
   char test_exe     [128];                 // default "Game.exe"
   char test_args    [256];                 // default PROJECT_DEFAULT_TEST_ARGS
   char test_wrapper [PROJECT_PATH_MAX];    // default "" on Windows, "wine" elsewhere
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

// Clears glb_project (in-memory only; file is left on disk) and puts every
// glb_config.mod_dir entry back as it was before the project was applied.
void project_close(void);

// Apply the currently-open project to glb_config: puts the project directory at
// mod_dir[0], moving the configured entries down one (a full list drops its last
// entry until the project closes) and, if glb_config.d2_install is empty, fills
// it from the project. Safe to call multiple times: a later call, for another
// project, replaces only mod_dir[0].
void project_apply_to_config(void);

// Recursively mkdir every intermediate directory of `path` (not including
// the final basename). Returns 1 if the file's directory exists afterwards,
// 0 otherwise (e.g. a regular file sits where a directory must go).
int  project_ensure_parent_dirs(const char *path);

// Copy-on-save: if a project is open and `src` lives outside it, rewrite
// the path into the project's Global\Tiles\<suffix> tree. `suffix` is the
// tail after the last "/tiles/" or "\tiles\" segment in `src`.
// Writes the resolved target path into `dst`.
//
// Returns 1 if the path was actually redirected (i.e., a copy-on-save just
// happened); 0 if it was passed through unchanged (no project, already
// inside the project -- at a path-component boundary -- or a rooted path with
// no tiles segment to rewrite from); -1 if it must be redirected but the target
// does not fit in `dst_cap`, or if a ".." component could take it out of the
// project (dst then holds `src`; do not save in place).
int  project_redirect_ds1_save_path(const char *src, char *dst, int dst_cap);

// 1 only when a real save of `src` would redirect into the project and land on
// a file that is already there, which the save would then replace (the old file
// becoming a numbered backup). The UI asks first; see map_save_ui_save().
int  project_save_would_replace(const char *src);

// 1 if `path` has a ".." component, in either slash style.
int  project_path_has_parent_ref(const char *path);

// 1 for "/x", "\x" and "C:\x". Anything else is relative.
int  project_path_is_rooted(const char *path);

// May `path` be inside the Diablo II install? 1 if it is inside the configured
// or the project's d2_install, and also for a rooted path with a ".." component
// while an install is known, since nothing here can tell where that one ends up.
int  project_path_in_game_install(const char *path);

// 1 if `path` is `dir` or lies inside it (compared at path-component
// boundaries, ignoring case and slash direction); 0 otherwise, and for a
// NULL or empty `dir`.
int  project_path_is_inside(const char *path, const char *dir);

/* The game folder whose archives the editor has open: the configured d2_install (the
   one the archives were read from at startup), else the project's, which only fills in
   an empty one. Never NULL. */
const char *project_open_game_folder(const char *configured, const char *project_install);

/* Everything after the last "/tiles/" in a path, i.e. the form MPQ
 * virtual paths under Data\\Global\\Tiles\\ are keyed on.
 * NULL if the path has no tiles component. */
const char *find_tiles_suffix(const char *path);

#endif
