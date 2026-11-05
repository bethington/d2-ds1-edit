/* A stand-in for the game in game_start tests. It writes its working folder and each
 * argument, one to a line, to fake_game.out in its working folder, and exits. */
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

int main(int argc, char ** argv)
{
   char cwd[1024];
   FILE * f;
   int i;

   if (getcwd(cwd, sizeof(cwd)) == NULL)
      return 2;
   f = fopen("fake_game.out.tmp", "wb");
   if (f == NULL)
      return 3;
   fprintf(f, "%s\n", cwd);
   for (i = 1; i < argc; i++)
      fprintf(f, "%s\n", argv[i]);
   if (fclose(f) != 0)
      return 4;
   /* Appear whole: a poller only sees the finished file. */
   remove("fake_game.out");
   return rename("fake_game.out.tmp", "fake_game.out") == 0 ? 0 : 5;
}
