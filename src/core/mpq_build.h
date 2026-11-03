/* Build MPQ: choose a project's files, check that the game could use them, and write
 * them over the install's patch_d2.mpq into a new archive. Nothing here reads a
 * display or a loaded game, and nothing writes to the game's install or the test
 * copy: base_mpq and test_game_dir are only read. */
#ifndef DS1EDIT_MPQ_BUILD_H
#define DS1EDIT_MPQ_BUILD_H

/* Is this map in the game's own MPQs? ds1_path is relative to the Tiles folder with
   forward slashes, as the project has it. */
typedef int (* MPQ_BUILD_IN_GAME_CB)(const char * ds1_path, void * ctx);

typedef struct MPQ_BUILD_ARGS_S
{
   const char * project_dir;
   const char * base_mpq;        /* the install's patch_d2.mpq; only read */
   const char * test_game_dir;   /* "" if none; where compiled .bin files come from */
   const char * out_mpq;
   char * game_table;            /* the game's own LvlPrest.txt */
   MPQ_BUILD_IN_GAME_CB in_game; /* is this map in the game's MPQs? */
   void * ctx;
} MPQ_BUILD_ARGS_S;

typedef struct MPQ_BUILD_RESULT_S
{
   int files;   /* project files and compiled tables packed, not the listfile */
   long bytes;  /* size of the archive written */
} MPQ_BUILD_RESULT_S;

/* Validate, choose and write. 0, or -1 with every problem in `problems`, one to a line,
   and nothing written. */
int mpq_build_run(const MPQ_BUILD_ARGS_S * a, MPQ_BUILD_RESULT_S * out, char * problems, int cap);

/* Calls cb for each project file that belongs in a build or a test install, by the
   "Files taken" and "Files left out" rules, in a fixed order. rel is relative to the
   Tiles or Excel folder, with forward slashes. is_table says which. Game_install uses
   this too. */
typedef void (* MPQ_BUILD_FILE_CB)(const char * source_path, const char * rel, int is_table,
                                   void * ctx);
void mpq_build_each_file(const char * project_dir, MPQ_BUILD_FILE_CB cb, void * ctx);

/* 1 if the game's MPQ can take this archive name: at most 200 characters, every byte
   printable ASCII. */
int mpq_build_name_ok(const char * name);

#endif
