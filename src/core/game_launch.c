#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
#else
   #include <limits.h>
#endif

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

// The real location of an existing folder, links and junctions followed.
// 0 when it can't be resolved (it may not exist).
static int resolve_dir(const char *path, char *out, int cap)
{
#ifdef WIN32
   HANDLE h;
   DWORD n;

   h = CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
   if (h == INVALID_HANDLE_VALUE) return 0;
   n = GetFinalPathNameByHandleA(h, out, (DWORD) cap, FILE_NAME_NORMALIZED);
   CloseHandle(h);
   if (n == 0 || n >= (DWORD) cap) return 0;
   // Drop the \\?\ prefix; \\?\UNC\srv\share becomes \\srv\share.
   if (strncmp(out, "\\\\?\\UNC\\", 8) == 0)
      memmove(out + 1, out + 7, strlen(out + 7) + 1);
   else if (strncmp(out, "\\\\?\\", 4) == 0)
      memmove(out, out + 4, strlen(out + 4) + 1);
   return 1;
#else
   char buf[PATH_MAX];

   if (realpath(path, buf) == NULL || strlen(buf) >= (size_t) cap) return 0;
   strcpy(out, buf);
   return 1;
#endif
}

static int overlaps(const char *a, const char *b)
{
   return project_path_is_inside(a, b) || project_path_is_inside(b, a);
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
   char real_game[1100], real_install[1100];
   int game_resolved;
   FILE *fp;
   int i;

   if (error != NULL && cap > 0) error[0] = 0;

   if (p == NULL || !has_text(p->game_dir))
      return fail(error, cap, "%s", "Set the test folder: the copy of the game that runs your map.");

   if (!project_path_is_rooted(p->game_dir) || project_path_has_parent_ref(p->game_dir))
      return fail(error, cap, "%s", "Choose the test folder by its full path.");

   if (!has_text(p->exe) || project_path_is_rooted(p->exe) || project_path_has_parent_ref(p->exe))
      return fail(error, cap, "%s",
                  "The game program must be a file name inside the test folder, "
                  "such as Game.exe.");

   // Compare the spelling, and the real locations too, so a link or junction
   // to an install is caught.
   game_resolved = resolve_dir(p->game_dir, real_game, sizeof(real_game));
   for (i = 0; installs != NULL && i < install_count; i++)
   {
      if (!has_text(installs[i])) continue;
      if (overlaps(p->game_dir, installs[i]) ||
          (game_resolved && resolve_dir(installs[i], real_install, sizeof(real_install)) &&
           overlaps(real_game, real_install)))
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
