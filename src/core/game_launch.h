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

// Is this profile usable? 0, or -1 with why. `installs` are folders it may not
// be, contain or be inside; NULL or empty entries are skipped.
int game_profile_check(const GAME_PROFILE_S * p, const char * const * installs, int install_count,
                       char * error, int cap);

#endif
