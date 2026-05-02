#include <ctype.h>
#include <stddef.h>

#include "core/glob_match.h"

#define GLOB_MAX_SEGMENTS 64

static int icase_eq(char a, char b)
{
   return tolower((unsigned char) a) == tolower((unsigned char) b);
}

static int is_sep(char c)
{
   return c == '\\' || c == '/';
}

static int tokenize(const char *s, const char **segs, int *lens, int max)
{
   int n = 0;

   if (s == NULL)
      return 0;

   while (*s != 0 && n < max)
   {
      const char *start = s;
      while (*s != 0 && !is_sep(*s))
         s++;
      segs[n] = start;
      lens[n] = (int) (s - start);
      n++;
      if (*s != 0)
         s++;
   }
   return n;
}

static int match_in_segment(const char *p, int plen, const char *s, int slen)
{
   int pi = 0;
   int si = 0;
   int star_pi = -1;
   int star_si = 0;

   while (si < slen)
   {
      if (pi < plen && p[pi] == '?')
      {
         pi++;
         si++;
      }
      else if (pi < plen && p[pi] == '*')
      {
         star_pi = pi;
         pi++;
         while (pi < plen && p[pi] == '*')
            pi++;
         star_si = si;
      }
      else if (pi < plen && icase_eq(p[pi], s[si]))
      {
         pi++;
         si++;
      }
      else if (star_pi >= 0)
      {
         pi = star_pi + 1;
         while (pi < plen && p[pi] == '*')
            pi++;
         star_si++;
         si = star_si;
      }
      else
      {
         return 0;
      }
   }

   while (pi < plen && p[pi] == '*')
      pi++;

   return pi == plen;
}

static int seg_is_double_star(const char *seg, int len)
{
   return len == 2 && seg[0] == '*' && seg[1] == '*';
}

static int match_segments(const char **psegs, const int *plens, int np, int pi,
                          const char **tsegs, const int *tlens, int nt, int ti)
{
   while (pi < np)
   {
      if (seg_is_double_star(psegs[pi], plens[pi]))
      {
         int k;
         for (k = 0; k <= nt - ti; k++)
         {
            if (match_segments(psegs, plens, np, pi + 1,
                               tsegs, tlens, nt, ti + k))
               return 1;
         }
         return 0;
      }

      if (ti >= nt)
         return 0;

      if (!match_in_segment(psegs[pi], plens[pi], tsegs[ti], tlens[ti]))
         return 0;

      pi++;
      ti++;
   }

   return ti == nt;
}

int glob_match(const char *pattern, const char *path)
{
   const char *psegs[GLOB_MAX_SEGMENTS];
   int plens[GLOB_MAX_SEGMENTS];
   const char *tsegs[GLOB_MAX_SEGMENTS];
   int tlens[GLOB_MAX_SEGMENTS];
   int np;
   int nt;

   if (pattern == NULL || path == NULL)
      return 0;

   np = tokenize(pattern, psegs, plens, GLOB_MAX_SEGMENTS);
   nt = tokenize(path, tsegs, tlens, GLOB_MAX_SEGMENTS);

   return match_segments(psegs, plens, np, 0, tsegs, tlens, nt, 0);
}
