/* selftest_verdict.c -- see selftest_verdict.h. */

#include "ui/selftest_verdict.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

double selftest_thread_cpu_ms(void)
{
#if defined(_WIN32)
   FILETIME c, e, k, u;
   ULARGE_INTEGER kk, uu;

   if (!GetThreadTimes(GetCurrentThread(), &c, &e, &k, &u))
      return 0.0;
   kk.LowPart = k.dwLowDateTime;
   kk.HighPart = k.dwHighDateTime;
   uu.LowPart = u.dwLowDateTime;
   uu.HighPart = u.dwHighDateTime;
   return (double)(kk.QuadPart + uu.QuadPart) / 10000.0;   /* 100 ns units */
#else
   struct timespec ts;

   if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0)
      return 0.0;
   return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
#endif
}

const char *selftest_render_verdict(int frames, double cpu_ms_total,
                                    unsigned guard_hits)
{
   /* The direct evidence wins: it names the cause, the timing only infers it. */
   if (guard_hits > 0)
      return "a MEMORY bitmap was drawn onto a VIDEO target "
             "(see the render: WARNING line in stderr.txt)";
   if (frames > 0 && cpu_ms_total / frames > SELFTEST_RENDER_LIMIT_MS)
      return "composing a frame used over 250 ms of CPU on average; "
             "something drawn every frame is probably a MEMORY bitmap";
   return NULL;
}
