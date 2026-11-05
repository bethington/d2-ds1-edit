#ifndef WIN32
   /* fork and setsid, and realpath, which glibc declares only for X/Open or GNU, under
      -std=c99 as well as the GNU modes. */
   #ifndef _GNU_SOURCE
      #ifndef _POSIX_C_SOURCE
         #define _POSIX_C_SOURCE 200809L
      #endif
      #ifndef _XOPEN_SOURCE
         #define _XOPEN_SOURCE 700
      #endif
   #endif
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
#else
   #include <limits.h>
   #include <unistd.h>
   #include <fcntl.h>
   #include <sys/types.h>
   #include <sys/wait.h>
#endif

#include "platform.h"
#include "core/file_copy.h"
#include "core/game_launch.h"
#include "core/mpq_build.h"
#include "core/project.h"

#define WRITE_TEST_NAME "ds1edit-write-test.tmp"
#define TILES_DIR       "DATA/GLOBAL/TILES"
#define EXCEL_DIR       "DATA/GLOBAL/EXCEL"
#define PATH_CAP        1100
#define PROJECT_OVERLAP "The test folder can't be your project folder, or contain it " \
                        "or sit inside it. Make a separate copy of the game and " \
                        "choose that."

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

// dir/rel+tail into out. 0 when it doesn't fit.
static int join_path(char *out, int cap, const char *dir, const char *rel, const char *tail)
{
   return snprintf(out, (size_t) cap, "%s/%s%s", dir, rel, tail) < cap;
}

// game_dir/rel+tail, with each part of rel that exists already spelled as it is on disk,
// so a copy holding data/global doesn't get a second DATA/GLOBAL on a case-sensitive
// disk. A part that isn't there keeps rel's spelling, and so does the last part unless
// name_too is set. 0 when it doesn't fit.
static int disk_path(const char *game_dir, const char *rel, int name_too, const char *tail,
                     char *out, int cap)
{
   char cur[PATH_CAP], next[PATH_CAP], part[PATH_CAP];
   const char *p = rel;
   int found = 1;

   if (snprintf(cur, sizeof(cur), "%s", game_dir) >= (int) sizeof(cur)) return 0;
   for (;;)
   {
      size_t n = strcspn(p, "/");
      int last = p[n] == 0;

      if (n >= sizeof(part)) return 0;
      memcpy(part, p, n);
      part[n] = 0;
      if (!join_path(next, sizeof(next), cur, part, "")) return 0;
      if (found && (!last || name_too))
         found = ds1_path_resolve_nocase(cur, part, next, sizeof(next));
      strcpy(cur, next);
      if (last) break;
      p += n + 1;
   }
   return snprintf(out, (size_t) cap, "%s%s", cur, tail) < cap;
}

// Does the test folder overlap this folder? By spelling, by real location, and through
// a link on the way to any folder an install writes in.
static int test_folder_overlaps(const char *game_dir, const char *other)
{
   static const char * const written[] = { "DATA", "DATA/GLOBAL", TILES_DIR, EXCEL_DIR };
   char real_other[PATH_CAP], real[PATH_CAP], place[PATH_CAP];
   int i;

   if (overlaps(game_dir, other)) return 1;
   if (!resolve_dir(other, real_other, sizeof(real_other))) return 0;
   if (resolve_dir(game_dir, real, sizeof(real)) && overlaps(real, real_other)) return 1;
   for (i = 0; i < 4; i++)
      if (disk_path(game_dir, written[i], 1, "", place, sizeof(place)) &&
          resolve_dir(place, real, sizeof(real)) && overlaps(real, real_other))
         return 1;
   return 0;
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

int game_profile_check(const GAME_PROFILE_S * p, const char * project_dir,
                       const char * const * installs, int install_count,
                       char * error, int cap)
{
   char path[PATH_CAP];
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

   // An install into the project would list the author's maps as its own files, and
   // the next one would delete them.
   if (has_text(project_dir) && test_folder_overlaps(p->game_dir, project_dir))
      return fail(error, cap, "%s", PROJECT_OVERLAP);

   for (i = 0; installs != NULL && i < install_count; i++)
   {
      if (!has_text(installs[i])) continue;
      if (test_folder_overlaps(p->game_dir, installs[i]))
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

/* ---- install ---------------------------------------------------------- */

#define MANIFEST_NAME "ds1edit-installed.txt"
#define STAGE_EXT     ".ds1edit-new"
#define RUNNING_HINT  "The game may be running. Close it and try again."
#define PARTLY_HINT_HEAD "The test copy is only partly installed."
#define PARTLY_HINT   PARTLY_HINT_HEAD " Close the game, then run Build & Test again."

static int failf(char *error, int cap, const char *fmt, ...)
{
   va_list ap;

   if (error != NULL && cap > 0)
   {
      va_start(ap, fmt);
      vsnprintf(error, (size_t) cap, fmt, ap);
      va_end(ap);
   }
   return -1;
}

static char *dup_text(const char *s)
{
   size_t n = strlen(s) + 1;
   char *d = (char *) malloc(n);

   if (d != NULL) memcpy(d, s, n);
   return d;
}

typedef struct INSTALL_FILE_S
{
   char *source;
   char *rel;      // relative to the game folder, forward slashes
   int is_table;
} INSTALL_FILE_S;

typedef struct INSTALL_LIST_S
{
   INSTALL_FILE_S *items;
   int count, cap;
   int oom;
} INSTALL_LIST_S;

static void install_collect(const char *source, const char *rel, int is_table, void *ctx)
{
   INSTALL_LIST_S *l = (INSTALL_LIST_S *) ctx;
   const char *dir = is_table ? EXCEL_DIR : TILES_DIR;
   INSTALL_FILE_S *f;
   size_t n = strlen(dir) + 1 + strlen(rel) + 1;

   if (l->oom) return;
   if (l->count == l->cap)
   {
      int cap = l->cap == 0 ? 32 : l->cap * 2;
      INSTALL_FILE_S *items = (INSTALL_FILE_S *) realloc(l->items, sizeof(*items) * (size_t) cap);

      if (items == NULL) { l->oom = 1; return; }
      l->items = items;
      l->cap = cap;
   }
   f = &l->items[l->count];
   f->source = dup_text(source);
   f->rel = (char *) malloc(n);
   f->is_table = is_table;
   if (f->source == NULL || f->rel == NULL)
   {
      free(f->source);
      free(f->rel);
      l->oom = 1;
      return;
   }
   snprintf(f->rel, n, "%s/%s", dir, rel);
   l->count++;
}

static void install_list_free(INSTALL_LIST_S *l)
{
   int i;

   for (i = 0; i < l->count; i++)
   {
      free(l->items[i].source);
      free(l->items[i].rel);
   }
   free(l->items);
}

static int starts_with_nocase(const char *s, const char *prefix)
{
   for (; *prefix != 0; s++, prefix++)
      if (tolower((unsigned char) *s) != tolower((unsigned char) *prefix))
         return 0;
   return 1;
}

// A manifest line may only name a file this install could have made. Win32 drops
// trailing dots and spaces from a name, so such a segment could name something else.
static int manifest_line_ok(const char *line)
{
   const char *seg = line;

   if (project_path_is_rooted(line) || project_path_has_parent_ref(line))
      return 0;
   if (strchr(line, '\\') != NULL || strchr(line, ':') != NULL)
      return 0;
   if (!starts_with_nocase(line, TILES_DIR "/") && !starts_with_nocase(line, EXCEL_DIR "/"))
      return 0;
   for (;;)
   {
      size_t n = strcspn(seg, "/");

      if (n == 0 || seg[n - 1] == '.' || seg[n - 1] == ' ')
         return 0;   // an empty name, such as nothing after the folder
      if (seg[n] == 0) return 1;
      seg += n + 1;
   }
}

#define RM_OK   0
#define RM_BUSY 1   // would not go: probably in use
#define RM_DIR  2   // a folder is where a file belongs

static int is_directory(const char *path)
{
   struct stat st;

   return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

// Delete a file. A file that isn't there is fine. A folder is never touched. *removed
// is set to 1 when something was actually deleted.
static int remove_file(const char *path, int *removed)
{
   FILE *fp;

   if (is_directory(path)) return RM_DIR;
   if (remove(path) == 0)
   {
      if (removed != NULL) *removed = 1;
      return RM_OK;
   }
   fp = fopen(path, "rb");
   if (fp == NULL) return errno == ENOENT ? RM_OK : RM_BUSY;   // never was, or gone
   fclose(fp);
   return RM_BUSY;
}

// Remove the staging files of items [from, count).
static void remove_staged(const char *game_dir, const INSTALL_LIST_S *l, int from)
{
   char path[PATH_CAP];
   int i;

   for (i = from; i < l->count; i++)
      if (disk_path(game_dir, l->items[i].rel, 0, STAGE_EXT, path, sizeof(path)))
         remove(path);
}

// The compiled twin of a table: Foo.txt -> Foo.bin. 0 if rel isn't a .txt.
static int bin_rel(const char *rel, char *out, int cap)
{
   size_t n = strlen(rel);

   if (n < 4 || (int) n + 1 > cap || !starts_with_nocase(rel + n - 4, ".txt")) return 0;
   memcpy(out, rel, n - 3);
   strcpy(out + n - 3, "bin");
   return 1;
}

typedef struct NAMES_S
{
   char **v;
   int n, cap;
} NAMES_S;

static int names_add(NAMES_S *s, const char *name)
{
   if (s->n == s->cap)
   {
      int cap = s->cap == 0 ? 32 : s->cap * 2;
      char **v = (char **) realloc(s->v, sizeof(char *) * (size_t) cap);

      if (v == NULL) return 0;
      s->v = v;
      s->cap = cap;
   }
   s->v[s->n] = dup_text(name);
   if (s->v[s->n] == NULL) return 0;
   s->n++;
   return 1;
}

static void names_free(NAMES_S *s)
{
   int i;

   for (i = 0; i < s->n; i++) free(s->v[i]);
   free(s->v);
}

static int equal_nocase(const char *a, const char *b)
{
   return strlen(a) == strlen(b) && starts_with_nocase(a, b);
}

// The valid names of the last install's manifest. 0, or -1 when out of memory.
static int read_manifest(const char *game_dir, NAMES_S *names)
{
   char path[PATH_CAP], line[PATH_CAP];
   FILE *fp;

   memset(names, 0, sizeof(*names));
   if (!join_path(path, sizeof(path), game_dir, MANIFEST_NAME, "")) return 0;
   fp = fopen(path, "rb");
   if (fp == NULL) return 0;
   while (fgets(line, sizeof(line), fp) != NULL)
   {
      size_t n = strlen(line);

      if (n > 0 && line[n - 1] != '\n' && !feof(fp))
      {
         // Longer than any line we wrote: skip the rest of it.
         int c;

         while ((c = fgetc(fp)) != EOF && c != '\n') {}
         continue;
      }
      while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
      if (n == 0 || !manifest_line_ok(line)) continue;
      if (!names_add(names, line))
      {
         fclose(fp);
         return -1;
      }
   }
   fclose(fp);
   return 0;
}

// Replace dst with the finished staging file src. Never writes through dst: it is
// removed first, so another name linked to the same bytes keeps them.
static int put_in_place(const char *src, const char *dst)
{
   int rc = remove_file(dst, NULL);

   if (rc != RM_OK) return rc;
   return rename(src, dst) == 0 ? RM_OK : RM_BUSY;
}

// Write the manifest: the old names, then the new files not already among them (new
// is NULL for just the old). Through a fresh staging file. 1 on success.
static int write_manifest(const char *game_dir, const NAMES_S *old, const INSTALL_LIST_S *add)
{
   char stage[PATH_CAP], dst[PATH_CAP];
   FILE *fp;
   int i, j, ok;

   join_path(stage, sizeof(stage), game_dir, MANIFEST_NAME, STAGE_EXT);
   join_path(dst, sizeof(dst), game_dir, MANIFEST_NAME, "");
   remove(stage);   // a leftover could be a link to something else
   fp = fopen(stage, "wb");
   if (fp == NULL) return 0;
   for (i = 0; old != NULL && i < old->n; i++)
      fprintf(fp, "%s\n", old->v[i]);
   for (i = 0; add != NULL && i < add->count; i++)
   {
      int seen = 0;

      for (j = 0; old != NULL && j < old->n && !seen; j++)
         seen = equal_nocase(old->v[j], add->items[i].rel);
      if (!seen) fprintf(fp, "%s\n", add->items[i].rel);
   }
   ok = fclose(fp) == 0 && put_in_place(stage, dst) == RM_OK;
   if (!ok) remove(stage);
   return ok;
}

// The error for a file that would not go. partly: the folder already changed.
static int fail_removal(char *error, int cap, int code, const char *path, int partly)
{
   if (code == RM_DIR)
      return partly
         ? failf(error, cap, "%s %s is a folder where a file belongs: delete it, then run "
                             "Build & Test again.", PARTLY_HINT_HEAD, path)
         : failf(error, cap, "%s is a folder where a file belongs. Delete it and try again.",
                 path);
   return failf(error, cap, "%s", partly ? PARTLY_HINT : RUNNING_HINT);
}

int game_install(const char * project_dir, const char * game_dir, int * out_files,
                 char * error, int cap)
{
   INSTALL_LIST_S list;
   NAMES_S old;
   char stage[PATH_CAP], dst[PATH_CAP], bin[PATH_CAP];
   int i, rc, gone, removed = 0;
   FILE *fp;

   if (error != NULL && cap > 0) error[0] = 0;
   if (out_files != NULL) *out_files = 0;
   if (!has_text(project_dir) || !has_text(game_dir))
      return failf(error, cap, "Set the project and the test folder first.");
   if (test_folder_overlaps(game_dir, project_dir))
      return failf(error, cap, "%s", PROJECT_OVERLAP);
   // A folder that isn't there can't be compared, and a game can't start from it.
   if (!resolve_dir(game_dir, stage, sizeof(stage)))
      return failf(error, cap, "Can't find the test folder %s.", game_dir);

   memset(&list, 0, sizeof(list));
   mpq_build_each_file(project_dir, install_collect, &list);
   if (list.oom)
   {
      install_list_free(&list);
      return failf(error, cap, "Out of memory listing the project's files.");
   }

   // Nothing is changed until every file is known to be readable and fits.
   for (i = 0; i < list.count; i++)
   {
      fp = fopen(list.items[i].source, "rb");
      if (fp == NULL)
      {
         failf(error, cap, "Can't read %s.", list.items[i].source);
         install_list_free(&list);
         return -1;
      }
      fclose(fp);
      if (!join_path(stage, sizeof(stage), game_dir, list.items[i].rel, STAGE_EXT))
      {
         install_list_free(&list);
         return failf(error, cap, "The test folder path is too long.");
      }
   }

   // The two folders, even when the project has nothing for one of them.
   if (!disk_path(game_dir, TILES_DIR "/x", 0, "", stage, sizeof(stage)) ||
       !project_ensure_parent_dirs(stage) ||
       !disk_path(game_dir, EXCEL_DIR "/x", 0, "", stage, sizeof(stage)) ||
       !project_ensure_parent_dirs(stage))
   {
      install_list_free(&list);
      return failf(error, cap, "Can't make the DATA/GLOBAL folders in %s.", game_dir);
   }

   // Copy everything beside its target first; a failure here changes nothing. A staging
   // file left by a crash goes first: it could be a link to a file of the game's.
   for (i = 0; i < list.count; i++)
   {
      disk_path(game_dir, list.items[i].rel, 0, STAGE_EXT, stage, sizeof(stage));
      remove(stage);
      if (!project_ensure_parent_dirs(stage) ||
          file_copy_checked(list.items[i].source, stage) != FILE_COPY_OK)
      {
         remove_staged(game_dir, &list, 0);
         failf(error, cap, "Can't copy %s into the test folder.", list.items[i].source);
         install_list_free(&list);
         return -1;
      }
   }

   // Before anything goes, the manifest lists the old files and the new ones, so a
   // install that stops partway can still be cleaned up by the next one.
   if (read_manifest(game_dir, &old) != 0 || !write_manifest(game_dir, &old, &list))
   {
      names_free(&old);
      remove_staged(game_dir, &list, 0);
      install_list_free(&list);
      return failf(error, cap, "Can't write the install list in %s.", game_dir);
   }

   // The last install's files go, and the compiled form of each table.
   for (i = 0; i < old.n; i++)
   {
      if (!disk_path(game_dir, old.v[i], 1, "", dst, sizeof(dst))) continue;
      if (is_directory(dst)) continue;   // only files were ever listed
      rc = remove_file(dst, &removed);
      if (rc != RM_OK)
      {
         remove_staged(game_dir, &list, 0);
         names_free(&old);
         install_list_free(&list);
         return fail_removal(error, cap, rc, dst, removed);
      }
   }
   names_free(&old);
   for (i = 0; i < list.count; i++)
   {
      if (!list.items[i].is_table || !bin_rel(list.items[i].rel, bin, sizeof(bin))) continue;
      // Every spelling: the game writes lvlprest.bin for LvlPrest.txt.
      do
      {
         gone = 0;
         disk_path(game_dir, bin, 1, "", dst, sizeof(dst));
         rc = remove_file(dst, &gone);
         if (rc != RM_OK)
         {
            remove_staged(game_dir, &list, 0);
            install_list_free(&list);
            return fail_removal(error, cap, rc, dst, removed);
         }
         removed |= gone;
      } while (gone);
   }

   for (i = 0; i < list.count; i++)
   {
      disk_path(game_dir, list.items[i].rel, 0, STAGE_EXT, stage, sizeof(stage));
      disk_path(game_dir, list.items[i].rel, 1, "", dst, sizeof(dst));
      rc = put_in_place(stage, dst);
      if (rc != RM_OK)
      {
         remove_staged(game_dir, &list, i);   // the ones before it are in place
         install_list_free(&list);
         return fail_removal(error, cap, rc, dst, removed || i > 0);
      }
   }

   // What this install made, for the next one to remove.
   if (!write_manifest(game_dir, NULL, &list))
   {
      install_list_free(&list);
      return failf(error, cap, "%s", PARTLY_HINT);
   }

   if (out_files != NULL) *out_files = list.count;
   install_list_free(&list);
   return 0;
}

/* ---- start ------------------------------------------------------------ */

typedef struct ARGS_S
{
   char **item;
   char *buf;
   int count;
} ARGS_S;

// Split on white space; a double-quoted run stays whole, quotes dropped, and "" is an
// empty argument.
static int args_split(const char *text, ARGS_S *a)
{
   size_t len = text != NULL ? strlen(text) : 0;
   const char *p = text;
   char *out;

   a->count = 0;
   a->item = (char **) malloc(sizeof(char *) * (len + 2));
   a->buf = (char *) malloc(len + 2);
   if (a->item == NULL || a->buf == NULL)
   {
      free(a->item);
      free(a->buf);
      return 0;
   }
   out = a->buf;
   while (p != NULL && *p != 0)
   {
      int quoted = 0;

      while (*p == ' ' || *p == '\t') p++;
      if (*p == 0) break;
      a->item[a->count++] = out;
      while (*p != 0 && (quoted || (*p != ' ' && *p != '\t')))
      {
         if (*p == '"') quoted = !quoted;
         else *out++ = *p;
         p++;
      }
      *out++ = 0;
   }
   return 1;
}

static void args_free(ARGS_S *a)
{
   free(a->item);
   free(a->buf);
}

// Append one argument as the Windows command line rules quote it.
static void append_quoted(char *out, size_t *len, const char *arg)
{
   int plain = arg[0] != 0 && strpbrk(arg, " \t\"") == NULL;
   const char *c;

   if (*len > 0) out[(*len)++] = ' ';
   if (plain)
   {
      strcpy(out + *len, arg);
      *len += strlen(arg);
      return;
   }
   out[(*len)++] = '"';
   for (c = arg; ; c++)
   {
      int slashes = 0;

      while (*c == '\\') { slashes++; c++; }
      if (*c == 0)
      {
         while (slashes-- > 0) { out[(*len)++] = '\\'; out[(*len)++] = '\\'; }
         break;
      }
      if (*c == '"')
      {
         while (slashes-- > 0) { out[(*len)++] = '\\'; out[(*len)++] = '\\'; }
         out[(*len)++] = '\\';
         out[(*len)++] = '"';
      }
      else
      {
         while (slashes-- > 0) out[(*len)++] = '\\';
         out[(*len)++] = *c;
      }
   }
   out[(*len)++] = '"';
   out[*len] = 0;
}

// The program's full path: the test folder, a slash, the name.
static void exe_path(const GAME_PROFILE_S *p, char *out, int cap)
{
   size_t n = strlen(p->game_dir);
   int slash = n > 0 && (p->game_dir[n - 1] == '/' || p->game_dir[n - 1] == '\\');

   snprintf(out, (size_t) cap, "%s%s%s", p->game_dir, slash ? "" : "/", p->exe);
}

// The command line, malloc'd: the wrapper if any, the program's full path, the
// arguments. NULL when out of memory.
static char *command_line(const GAME_PROFILE_S *p)
{
   ARGS_S a;
   char path[PATH_CAP];
   char *out;
   size_t len = 0, room;
   int i;

   if (!args_split(p->args, &a)) return NULL;
   exe_path(p, path, sizeof(path));
   // Every character may be escaped, and each argument gets two quotes and a space.
   room = strlen(path) * 2 + 8 + (p->args != NULL ? strlen(p->args) * 2 : 0) +
          (has_text(p->wrapper) ? strlen(p->wrapper) * 2 + 8 : 0) + (size_t) a.count * 4;
   out = (char *) malloc(room + 8);
   if (out != NULL)
   {
      out[0] = 0;
      if (has_text(p->wrapper)) append_quoted(out, &len, p->wrapper);
      append_quoted(out, &len, path);
      for (i = 0; i < a.count; i++)
         append_quoted(out, &len, a.item[i]);
   }
   args_free(&a);
   return out;
}

void game_command_text(const GAME_PROFILE_S * p, char * out, int cap)
{
   char *text;

   if (out == NULL || cap <= 0) return;
   out[0] = 0;
   if (p == NULL || !has_text(p->game_dir) || !has_text(p->exe)) return;
   text = command_line(p);
   if (text == NULL) return;
   snprintf(out, (size_t) cap, "%s", text);
   free(text);
}

static int has_separator(const char *s)
{
   return strchr(s, '/') != NULL || strchr(s, '\\') != NULL;
}

static int file_there(const char *path)
{
   FILE *fp = fopen(path, "rb");

   if (fp == NULL) return 0;
   fclose(fp);
   return 1;
}

#ifdef WIN32

static int spawn(const GAME_PROFILE_S *p, char *cmd, char *reason, int cap)
{
   STARTUPINFOA si;
   PROCESS_INFORMATION pi;
   DWORD code, n, mode = 0;
   BOOL started;

   memset(&si, 0, sizeof(si));
   si.cb = sizeof(si);
   memset(&pi, 0, sizeof(pi));
   // A file that isn't a program can make Windows put up its own box, which waits for a
   // click, instead of failing. The caller reports the failure. The game doesn't inherit this.
   SetThreadErrorMode(GetThreadErrorMode() | SEM_FAILCRITICALERRORS, &mode);
   started = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE, NULL,
                            p->game_dir, &si, &pi);
   code = GetLastError();
   SetThreadErrorMode(mode, NULL);
   if (!started)
   {
      n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                         code, 0, reason, (DWORD) cap, NULL);
      while (n > 0 && (reason[n - 1] == '\r' || reason[n - 1] == '\n' ||
                       reason[n - 1] == ' ' || reason[n - 1] == '.'))
         reason[--n] = 0;
      if (n == 0) snprintf(reason, (size_t) cap, "Windows error %lu", (unsigned long) code);
      return -1;
   }
   CloseHandle(pi.hThread);
   CloseHandle(pi.hProcess);
   return 0;
}

#else

static void child_fail(int fd, int e)
{
   ssize_t r = write(fd, &e, sizeof(e));

   (void) r;
   _exit(127);
}

// Start argv[0] in game_dir, detached: the intermediate child exits at once, so the
// game is not ours to reap. The pipe closes when the program is exec'd; an errno on it
// means it never was.
static int spawn(const GAME_PROFILE_S *p, char **argv, char *reason, int cap)
{
   int fds[2], status, e = 0;
   ssize_t got;
   pid_t pid;

   if (pipe(fds) != 0)
   {
      snprintf(reason, (size_t) cap, "%s", strerror(errno));
      return -1;
   }
   fcntl(fds[0], F_SETFD, FD_CLOEXEC);
   fcntl(fds[1], F_SETFD, FD_CLOEXEC);
   pid = fork();
   if (pid < 0)
   {
      snprintf(reason, (size_t) cap, "%s", strerror(errno));
      close(fds[0]);
      close(fds[1]);
      return -1;
   }
   if (pid == 0)
   {
      pid_t again;

      close(fds[0]);
      setsid();
      if (chdir(p->game_dir) != 0) child_fail(fds[1], errno);
      again = fork();
      if (again < 0) child_fail(fds[1], errno);
      if (again > 0) _exit(0);
      // A path is run as it is; execvp would hand a non-program to the shell.
      if (strchr(argv[0], '/') != NULL) execv(argv[0], argv);
      else execvp(argv[0], argv);
      child_fail(fds[1], errno);
   }
   close(fds[1]);
   while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
   do got = read(fds[0], &e, sizeof(e)); while (got < 0 && errno == EINTR);
   close(fds[0]);
   if (got == (ssize_t) sizeof(e))
   {
      snprintf(reason, (size_t) cap, "%s", strerror(e));
      return -1;
   }
   return 0;
}

#endif

int game_start(const GAME_PROFILE_S * p, char * error, int cap)
{
   char path[PATH_CAP], reason[256], *cmd;
   int rc;

   if (error != NULL && cap > 0) error[0] = 0;
   if (p == NULL || !has_text(p->game_dir) || !has_text(p->exe))
      return failf(error, cap, "Set the test folder and the game program first.");
   cmd = command_line(p);
   if (cmd == NULL)
      return failf(error, cap, "Out of memory building the command line.");

   exe_path(p, path, sizeof(path));
   if (!file_there(path))
      rc = failf(error, cap, "The test folder has no %s.\nCommand: %s", p->exe, cmd);
   else if (has_text(p->wrapper) && has_separator(p->wrapper) && !file_there(p->wrapper))
      rc = failf(error, cap, "The wrapper %s isn't there.\nCommand: %s", p->wrapper, cmd);
   else
   {
      reason[0] = 0;
#ifdef WIN32
      rc = spawn(p, cmd, reason, sizeof(reason));
#else
      {
         ARGS_S a;
         char **argv;
         int n = 0, i;

         if (!args_split(p->args, &a))
            rc = failf(error, cap, "Out of memory building the command line.");
         else
         {
            argv = (char **) malloc(sizeof(char *) * (size_t) (a.count + 3));
            if (argv == NULL)
               rc = failf(error, cap, "Out of memory building the command line.");
            else
            {
               if (has_text(p->wrapper)) argv[n++] = (char *) p->wrapper;
               argv[n++] = path;
               for (i = 0; i < a.count; i++) argv[n++] = a.item[i];
               argv[n] = NULL;
               rc = spawn(p, argv, reason, sizeof(reason));
               free(argv);
            }
            args_free(&a);
         }
      }
#endif
      if (rc != 0 && reason[0] != 0)
         failf(error, cap, "Can't start the game: %s.\nCommand: %s", reason, cmd);
   }
   free(cmd);
   return rc == 0 ? 0 : -1;
}
