#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "core/game_launch.h"
#include "core/project.h"

#define WRITE_TEST_NAME "ds1edit-write-test.tmp"

static int fail(char *error, int cap, const char *fmt, const char *a)
{
   if (error != NULL && cap > 0)
      snprintf(error, cap, fmt, a);
   return -1;
}

static int has_text(const char *s)
{
   return s != NULL && s[0] != 0;
}

// Does `args` hold `word` as a whole, space-separated word, in any case?
static int args_have_word(const char *args, const char *word)
{
   size_t n = strlen(word);
   const char *p = args;

   while (*p != 0)
   {
      size_t len, i;

      while (*p == ' ' || *p == '\t') p++;
      for (len = 0; p[len] != 0 && p[len] != ' ' && p[len] != '\t'; len++) {}
      if (len == n)
      {
         for (i = 0; i < n; i++)
            if (tolower((unsigned char) p[i]) != tolower((unsigned char) word[i]))
               break;
         if (i == n) return 1;
      }
      p += len;
   }
   return 0;
}

int game_profile_check(const GAME_PROFILE_S * p, const char * const * installs, int install_count,
                       char * error, int cap)
{
   char path[1100];
   FILE *fp;
   int i;

   if (error != NULL && cap > 0) error[0] = 0;

   if (p == NULL || !has_text(p->game_dir))
      return fail(error, cap, "%s", "Set the test folder: the copy of the game that runs your map.");

   if (!has_text(p->exe) || project_path_is_rooted(p->exe) || project_path_has_parent_ref(p->exe))
      return fail(error, cap, "%s",
                  "The game program must be a file name inside the test folder, "
                  "such as Game.exe.");

   for (i = 0; installs != NULL && i < install_count; i++)
   {
      if (!has_text(installs[i])) continue;
      if (project_path_is_inside(p->game_dir, installs[i]) ||
          project_path_is_inside(installs[i], p->game_dir))
         return fail(error, cap, "%s",
                     "The test folder can't be your Diablo II install, or contain "
                     "it or sit inside it. Make a separate copy of the game and "
                     "choose that.");
   }

   if (snprintf(path, sizeof(path), "%s/%s", p->game_dir, p->exe) >= (int) sizeof(path))
      return fail(error, cap, "%s", "The test folder path is too long.");
   fp = fopen(path, "rb");
   if (fp == NULL)
      return fail(error, cap,
                  "The test folder has no %s. Choose the folder that holds it, or "
                  "change the game program name.", p->exe);
   fclose(fp);

   if (!has_text(p->args) || !args_have_word(p->args, "-direct") ||
       !args_have_word(p->args, "-txt"))
      return fail(error, cap, "%s",
                  "The test arguments need -direct and -txt, or the game ignores "
                  "the project.");

   // Prove the folder takes a file. A new name, so it can never be a linked
   // file of the game's.
   if (snprintf(path, sizeof(path), "%s/%s", p->game_dir, WRITE_TEST_NAME) >= (int) sizeof(path))
      return fail(error, cap, "%s", "The test folder path is too long.");
   fp = fopen(path, "wb");
   if (fp == NULL)
      return fail(error, cap,
                  "DS1Edit can't write to the test folder %s. Choose a folder you "
                  "can write to.", p->game_dir);
   fclose(fp);
   remove(path);
   return 0;
}
