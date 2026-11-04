#ifndef _GAME_LAUNCH_H_
#define _GAME_LAUNCH_H_

// Running a project's map in a test copy of the game. This file uses no
// Allegro and no UI: parameters are explicit and errors come back as text.

typedef struct GAME_PROFILE_S
{
   const char * game_dir;
   const char * exe;
   const char * args;
   const char * wrapper;
} GAME_PROFILE_S;

// Is this profile usable? 0, or -1 with why. The test folder may not be, contain
// or be inside project_dir (NULL or empty when no project is open) or any of
// `installs`, by spelling or by real location. NULL or empty entries are skipped.
int game_profile_check(const GAME_PROFILE_S * p, const char * project_dir,
                       const char * const * installs, int install_count,
                       char * error, int cap);

/* Install the project's loose files in the game folder: remove what the last install
   listed and the .bin of each table, then copy. Copies go to staging files first, so a
   copy that fails changes nothing. Writes <game_dir>/ds1edit-installed.txt. A folder or
   file the copy already has in another case is used, not doubled. Refuses, changing
   nothing, a game_dir that isn't there or that overlaps project_dir, since its install
   list would then name the author's own files. 0, or -1 with why. *out_files gets the
   count. */
int game_install(const char * project_dir, const char * game_dir, int * out_files,
                 char * error, int cap);

/* Start the game and don't wait for it. The working folder is game_dir. 0, or -1 with
   the reason and the full command line in error. */
int game_start(const GAME_PROFILE_S * p, char * error, int cap);

/* The command line as text, for messages and tests. */
void game_command_text(const GAME_PROFILE_S * p, char * out, int cap);

#endif
