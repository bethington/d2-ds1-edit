#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "platform.h"
#include "core/lvlprest_text.h"
#include "core/map_ops.h"
#include "core/mpq_build.h"
#include "core/mpq_write.h"
#include "core/project.h"

#define PATH_CAP 1024
#define NAME_CHARS_MAX 200
#define WALK_DEPTH_MAX 16
#define LINE_CAP 1024
#define TILES_PREFIX "data\\global\\tiles\\"
#define EXCEL_PREFIX "data\\global\\excel\\"
#define LISTFILE_NAME "(listfile)"
#define LISTFILE_TEMP "build/listfile.tmp"
#define NOT_COMPILED "%s has changed since the game last compiled it. " \
                     "Run Build & Test and start a game first."

/* ---- small helpers ---------------------------------------------------- */

static char * dup_text(const char * text)
{
   size_t n = strlen(text);
   char * copy = (char *) malloc(n + 1);

   if (copy != NULL)
      memcpy(copy, text, n + 1);
   return copy;
}

static char * dup_lower_slashes(const char * text)
{
   char * copy = dup_text(text);
   char * p;

   if (copy == NULL)
      return NULL;
   for (p = copy; *p != '\0'; p++)
      *p = *p == '\\' ? '/' : (char) tolower((unsigned char) *p);
   return copy;
}

static int has_extension(const char * name, const char * ext)
{
   size_t n = strlen(name), e = strlen(ext);

   return n > e && strcasecmp(name + n - e, ext) == 0;
}

/* The size of a file, or -1 if it can't be opened. */
static long file_size(const char * path)
{
   FILE * f;
   long size = -1;

   if (path == NULL || path[0] == '\0')
      return -1;
   f = fopen(path, "rb");
   if (f == NULL)
      return -1;
   if (fseek(f, 0, SEEK_END) == 0)
      size = ftell(f);
   fclose(f);
   return size;
}

/* ---- problems --------------------------------------------------------- */

typedef struct PROBLEMS_S
{
   char * buf;
   int cap;
   int len;
   int count;
} PROBLEMS_S;

/* One more line, cut short if the buffer is full. */
static void add_problem(PROBLEMS_S * p, const char * fmt, ...)
{
   char line[LINE_CAP];
   va_list ap;
   int room, n;

   p->count++;
   if (p->buf == NULL || p->cap <= 0)
      return;
   va_start(ap, fmt);
   vsnprintf(line, sizeof(line), fmt, ap);
   va_end(ap);
   if (p->len > 0 && p->len < p->cap - 1)
      p->buf[p->len++] = '\n';
   room = p->cap - 1 - p->len;
   n = (int) strlen(line);
   if (n > room)
      n = room;
   if (n > 0)
      memcpy(p->buf + p->len, line, (size_t) n);
   p->len += n;
   p->buf[p->len] = '\0';
}

/* A name as the person should see it: anything that isn't printable ASCII is "?". */
static void printable(char * out, int cap, const char * text)
{
   int i = 0;

   for (; *text != '\0' && i < cap - 1; text++)
      out[i++] = (*text >= 0x20 && *text < 0x7f) ? *text : '?';
   out[i] = '\0';
}

/* ---- sets of table paths ---------------------------------------------- */

typedef struct STR_SET_S
{
   char ** items;
   int count;
   int cap;
} STR_SET_S;

static int compare_text(const void * a, const void * b)
{
   return strcmp(*(char * const *) a, *(char * const *) b);
}

static void set_add(STR_SET_S * s, const char * text)
{
   char * copy = dup_lower_slashes(text);

   if (copy == NULL)
      return;
   if (s->count == s->cap)
   {
      int cap = s->cap > 0 ? s->cap * 2 : 64;
      char ** grown = (char **) realloc(s->items, (size_t) cap * sizeof(char *));

      if (grown == NULL)
      {
         free(copy);
         return;
      }
      s->items = grown;
      s->cap = cap;
   }
   s->items[s->count++] = copy;
}

static void set_sort(STR_SET_S * s)
{
   if (s->count > 1)
      qsort(s->items, (size_t) s->count, sizeof(char *), compare_text);
}

static int set_has(const STR_SET_S * s, const char * key)
{
   return s->count > 0 &&
          bsearch(&key, s->items, (size_t) s->count, sizeof(char *), compare_text) != NULL;
}

static void set_free(STR_SET_S * s)
{
   int i;

   for (i = 0; i < s->count; i++)
      free(s->items[i]);
   free(s->items);
   memset(s, 0, sizeof(*s));
}

static void note_table_path(int def, const char * path, void * ctx)
{
   (void) def;
   set_add((STR_SET_S *) ctx, path);
}

/* The paths any File cell of this table lists, lowercase with forward slashes. */
static void table_paths(char * table, STR_SET_S * out)
{
   if (table != NULL)
      lvlprest_each_file(table, note_table_path, out);
   set_sort(out);
}

/* ---- choosing the files ----------------------------------------------- */

typedef struct ENTRY_S
{
   char * source;   /* a file on disk */
   char * rel;      /* relative to the Tiles or Excel folder, forward slashes */
   char * key;      /* rel in lowercase */
   int is_table;
   int priority;    /* 0 under Data/, 1 under Global/ */
   int drop;        /* a numbered backup, to be left out */
} ENTRY_S;

typedef struct ENTRY_LIST_S
{
   ENTRY_S * items;
   int count;
   int cap;
   int failed;
} ENTRY_LIST_S;

static void entry_free(ENTRY_S * e)
{
   free(e->source);
   free(e->rel);
   free(e->key);
}

static void list_free(ENTRY_LIST_S * l)
{
   int i;

   for (i = 0; i < l->count; i++)
      entry_free(&l->items[i]);
   free(l->items);
   memset(l, 0, sizeof(*l));
}

static void list_add(ENTRY_LIST_S * l, const char * source, const char * rel, int is_table,
                     int priority)
{
   ENTRY_S e;

   if (l->count == l->cap)
   {
      int cap = l->cap > 0 ? l->cap * 2 : 64;
      ENTRY_S * grown = (ENTRY_S *) realloc(l->items, (size_t) cap * sizeof(ENTRY_S));

      if (grown == NULL)
      {
         l->failed = 1;
         return;
      }
      l->items = grown;
      l->cap = cap;
   }
   e.source = dup_text(source);
   e.rel = dup_text(rel);
   e.key = dup_lower_slashes(rel);
   e.is_table = is_table;
   e.priority = priority;
   e.drop = 0;
   if (e.source == NULL || e.rel == NULL || e.key == NULL)
   {
      entry_free(&e);
      l->failed = 1;
      return;
   }
   l->items[l->count++] = e;
}

/* The files of a folder that a build takes: maps and tiles, in its folders too, or tables,
   in the folder only. prefix is "" or "<folder>/". */
static void walk(ENTRY_LIST_S * l, const char * dir_path, const char * prefix, int is_table,
                 int priority, int depth)
{
   DS1_DIR d;

   if (!ds1_dir_open(&d, dir_path))
      return;
   while (ds1_dir_next(&d))
   {
      char source[PATH_CAP], rel[PATH_CAP];

      if (snprintf(source, sizeof(source), "%s/%s", dir_path, d.name) >= (int) sizeof(source) ||
          snprintf(rel, sizeof(rel), "%s%s", prefix, d.name) >= (int) sizeof(rel))
         continue;
      if (d.is_dir)
      {
         if (!is_table && depth < WALK_DEPTH_MAX)
         {
            char sub_prefix[PATH_CAP];

            if (snprintf(sub_prefix, sizeof(sub_prefix), "%s/", rel) < (int) sizeof(sub_prefix))
               walk(l, source, sub_prefix, is_table, priority, depth + 1);
         }
      }
      else if (is_table ? has_extension(d.name, ".txt")
                        : (has_extension(d.name, ".ds1") || has_extension(d.name, ".dt1")))
         list_add(l, source, rel, is_table, priority);
   }
   ds1_dir_close(&d);
}

static int compare_entries(const void * a, const void * b)
{
   const ENTRY_S * x = (const ENTRY_S *) a, * y = (const ENTRY_S *) b;
   int c;

   if (x->is_table != y->is_table)
      return x->is_table - y->is_table;
   c = strcmp(x->key, y->key);
   if (c != 0)
      return c;
   if (x->priority != y->priority)
      return x->priority - y->priority;
   return strcmp(x->rel, y->rel);
}

/* The entry with this key, or NULL. The list is sorted and has no repeats. */
static const ENTRY_S * find_entry(const ENTRY_LIST_S * l, int is_table, const char * key)
{
   int lo = 0, hi = l->count - 1;

   while (lo <= hi)
   {
      int mid = (lo + hi) / 2, c;

      c = l->items[mid].is_table != is_table ? l->items[mid].is_table - is_table
                                              : strcmp(l->items[mid].key, key);
      if (c == 0)
         return &l->items[mid];
      if (c < 0)
         lo = mid + 1;
      else
         hi = mid - 1;
   }
   return NULL;
}

/* 1 if rel is "<stem>-NNN.ds1", the backup a save leaves; the key of "<stem>.ds1" goes
   in main_key. */
static int numbered_backup_main_key(const char * key, char * main_key, int cap)
{
   size_t n = strlen(key), dot, i, digits;

   if (n < 5 || strcmp(key + n - 4, ".ds1") != 0)
      return 0;
   dot = n - 4;
   for (digits = 0; digits < dot && isdigit((unsigned char) key[dot - 1 - digits]); digits++)
      ;
   i = dot - digits;   /* where the digits start */
   if (digits < 3 || i < 2 || key[i - 1] != '-' || key[i - 2] == '/')
      return 0;
   if ((int) (i - 1 + 4 + 1) > cap)
      return 0;
   memcpy(main_key, key, i - 1);
   strcpy(main_key + i - 1, ".ds1");
   return 1;
}

/* Drop the repeats (Data/ is first) and the saves' numbered backups. */
static void settle(ENTRY_LIST_S * l, const STR_SET_S * listed)
{
   int i, kept = 0;

   if (l->count > 1)
      qsort(l->items, (size_t) l->count, sizeof(ENTRY_S), compare_entries);
   for (i = 0; i < l->count; i++)
   {
      if (kept > 0 && l->items[kept - 1].is_table == l->items[i].is_table &&
          strcmp(l->items[kept - 1].key, l->items[i].key) == 0)
         entry_free(&l->items[i]);
      else
         l->items[kept++] = l->items[i];
   }
   l->count = kept;

   /* The files are sorted by key, so a backup's map is found by that key. Mark first, so
      the search sees every file. */
   for (i = 0; i < l->count; i++)
   {
      char main_key[PATH_CAP];

      l->items[i].drop = !l->items[i].is_table &&
         numbered_backup_main_key(l->items[i].key, main_key, sizeof(main_key)) &&
         find_entry(l, 0, main_key) != NULL && !set_has(listed, l->items[i].key);
   }
   kept = 0;
   for (i = 0; i < l->count; i++)
   {
      if (l->items[i].drop)
         entry_free(&l->items[i]);
      else
         l->items[kept++] = l->items[i];
   }
   l->count = kept;
}

static void collect(const char * project_dir, char * project_table, ENTRY_LIST_S * l)
{
   static const char * const tiles[] = { "Data/Global/Tiles", "Global/Tiles" };
   static const char * const excel[] = { "Data/Global/Excel", "Global/Excel" };
   STR_SET_S listed;
   char found[PATH_CAP];
   int i;

   memset(l, 0, sizeof(*l));
   for (i = 0; i < 2; i++)
      if (ds1_path_resolve_nocase(project_dir, tiles[i], found, sizeof(found)))
         walk(l, found, "", 0, i, 0);
   for (i = 0; i < 2; i++)
      if (ds1_path_resolve_nocase(project_dir, excel[i], found, sizeof(found)))
         walk(l, found, "", 1, i, 0);
   memset(&listed, 0, sizeof(listed));
   table_paths(project_table, &listed);
   settle(l, &listed);
   set_free(&listed);
}

void mpq_build_each_file(const char * project_dir, MPQ_BUILD_FILE_CB cb, void * ctx)
{
   ENTRY_LIST_S list;
   char * table;
   long table_len;
   int i;

   table = map_ops_read_table(project_dir, &table_len);
   collect(project_dir, table, &list);
   for (i = 0; i < list.count; i++)
      cb(list.items[i].source, list.items[i].rel, list.items[i].is_table, ctx);
   list_free(&list);
   free(table);
}

/* ---- archive names ---------------------------------------------------- */

int mpq_build_name_ok(const char * name)
{
   const char * p;

   if (strlen(name) > NAME_CHARS_MAX)
      return 0;
   for (p = name; *p != '\0'; p++)
      if (*p < 0x20 || *p >= 0x7f)
         return 0;
   return 1;
}

/* prefix + rel with backslashes, malloc'd. */
static char * archive_name(const char * prefix, const char * rel, const char * replace_ext)
{
   size_t n = strlen(prefix) + strlen(rel) + 8;
   char * name = (char *) malloc(n);
   char * p;

   if (name == NULL)
      return NULL;
   sprintf(name, "%s%s", prefix, rel);
   if (replace_ext != NULL)
      strcpy(name + strlen(name) - 4, replace_ext);
   for (p = name; *p != '\0'; p++)
      if (*p == '/')
         *p = '\\';
   return name;
}

/* ---- the validation --------------------------------------------------- */

typedef struct RUN_S
{
   const MPQ_BUILD_ARGS_S * a;
   PROBLEMS_S * problems;
   char * project_table;
   char * game_table;
   const ENTRY_LIST_S * list;
} RUN_S;

/* A stored size plus one, as a number that can't overflow. */
static long le_size(const unsigned char * p)
{
   unsigned long v = (unsigned long) p[0] | ((unsigned long) p[1] << 8) |
                     ((unsigned long) p[2] << 16) | ((unsigned long) p[3] << 24);

   return v > 1000000ul ? 1000001L : (long) v + 1;
}

/* A map's width and height from its DS1 header: version, then width - 1 and height - 1,
   as map_ops_write_empty_ds1 writes them and ds1.c reads them. 0 if the file is too short
   to have them. */
static int ds1_header_size(const char * path, long * width, long * height)
{
   unsigned char h[12];
   FILE * f = fopen(path, "rb");
   size_t got;

   if (f == NULL)
      return 0;
   got = fread(h, 1, sizeof(h), f);
   fclose(f);
   if (got < sizeof(h))
      return 0;
   *width = le_size(h + 4);
   *height = le_size(h + 8);
   return 1;
}

/* Each map a sized row names must be the row's size: the game halts or draws wrongly
   on one that isn't. */
static void check_def_sizes(int def, void * ctx)
{
   RUN_S * r = (RUN_S *) ctx;
   int want_w = 0, want_h = 0, slot;

   if (!lvlprest_row_has_size(r->project_table, def))
      return;
   map_ops_default_size(r->project_table, def, &want_w, &want_h);
   if (want_w == 0 && want_h == 0)
      return;
   for (slot = 1; slot <= 6; slot++)
   {
      char cell[PATH_CAP], * key;
      const ENTRY_S * e;
      long w, h;

      if (!lvlprest_get_cell(r->project_table, def, slot, cell, sizeof(cell)) ||
          cell[0] == '\0' || strcmp(cell, "0") == 0)
         continue;
      key = dup_lower_slashes(cell);
      e = key != NULL ? find_entry(r->list, 0, key) : NULL;
      free(key);
      if (e == NULL || !has_extension(e->rel, ".ds1"))
         continue;
      if (!ds1_header_size(e->source, &w, &h))
      {
         add_problem(r->problems, "%s is too short to be a map, so the game can't use it.",
                     e->rel);
         continue;
      }
      if ((want_w != 0 && w != want_w) || (want_h != 0 && h != want_h))
      {
         if (want_w != 0 && want_h != 0)
            add_problem(r->problems, "%s is %ld by %ld, but Def %d's maps are %d by %d. "
                        "The game can't use it.", e->rel, w, h, def, want_w, want_h);
         else if (want_w != 0)
            add_problem(r->problems, "%s is %ld by %ld, but Def %d's maps are %d wide. "
                        "The game can't use it.", e->rel, w, h, def, want_w);
         else
            add_problem(r->problems, "%s is %ld by %ld, but Def %d's maps are %d high. "
                        "The game can't use it.", e->rel, w, h, def, want_h);
      }
   }
}

static int cell_is_empty(char * table, int def, int slot)
{
   char cell[PATH_CAP];

   return !lvlprest_get_cell(table, def, slot, cell, sizeof(cell)) || cell[0] == '\0' ||
          strcmp(cell, "0") == 0;
}

static void check_def_for_holes(int def, void * ctx)
{
   RUN_S * r = (RUN_S *) ctx;
   int files = lvlprest_files_value(r->project_table, def), slot;

   if (lvlprest_files_hole(r->project_table, def) == 0)
      return;
   for (slot = 1; slot <= 6 && slot <= files; slot++)
   {
      int game_has = lvlprest_files_value(r->game_table, def) >= slot &&
                     cell_is_empty(r->game_table, def, slot);

      if (cell_is_empty(r->project_table, def, slot) && !game_has)
         add_problem(r->problems,
                     "Def %d has an empty File%d below its Files count of %d, and the game "
                     "crashes when it picks that cell. Fill it in, or lower Files.",
                     def, slot, files);
   }
}

static void check_added_cell(int def, const char * path, void * ctx)
{
   RUN_S * r = (RUN_S *) ctx;
   char * slashed = dup_text(path), * p, found[PATH_CAP];
   int in_project, in_game = 0;

   if ((int) strlen(path) > MAP_OPS_TABLE_PATH_MAX)
   {
      add_problem(r->problems,
                  "Def %d lists %s, which is longer than the %d characters the game takes. "
                  "Use a shorter path.", def, path, MAP_OPS_TABLE_PATH_MAX);
      free(slashed);
      return;
   }
   if (slashed == NULL)
      return;
   for (p = slashed; *p != '\0'; p++)
      if (*p == '\\')
         *p = '/';
   in_project = map_ops_find_tile(r->a->project_dir, slashed, found, sizeof(found));
   if (!in_project && r->a->in_game != NULL)
      in_game = r->a->in_game(slashed, r->a->ctx) != 0;
   if (!in_project && !in_game)
      add_problem(r->problems,
                  "Def %d lists %s, which isn't in the project or the game. Add the map to "
                  "the project, or take it out of that cell.", def, path);
   free(slashed);
}

/* The file part of a path, after the last slash. */
static const char * file_part(const char * path)
{
   const char * name = path, * p;

   for (p = path; *p != '\0'; p++)
      if (*p == '/' || *p == '\\')
         name = p + 1;
   return name;
}

static int same_path(const char * a, const char * b)
{
   char * x = dup_lower_slashes(a), * y = dup_lower_slashes(b);
   int same = x != NULL && y != NULL && strcmp(x, y) == 0;

   free(x);
   free(y);
   return same;
}

/* The compiled .bin of one project table in the test copy, if the copy has compiled this
   very text, not before it and not empty. 1 and the bin's path in bin_path, or 0. */
static int compiled_bin(const char * test_dir, const char * name, const char * source,
                        char * bin_path, int cap)
{
   char rel[PATH_CAP], txt_path[PATH_CAP];
   char * mine, * theirs;
   long mine_len, theirs_len;
   struct stat txt_st, bin_st;
   int same;

   if (test_dir == NULL || test_dir[0] == '\0')
      return 0;
   if (snprintf(rel, sizeof(rel), "DATA/GLOBAL/EXCEL/%s", name) >= (int) sizeof(rel) ||
       !ds1_path_resolve_nocase(test_dir, rel, txt_path, sizeof(txt_path)))
      return 0;
   mine = map_ops_read_file(source, &mine_len);
   theirs = map_ops_read_file(txt_path, &theirs_len);
   same = mine != NULL && theirs != NULL && mine_len == theirs_len &&
          memcmp(mine, theirs, (size_t) mine_len) == 0;
   free(mine);
   free(theirs);
   if (!same)
      return 0;
   strcpy(rel + strlen(rel) - 4, ".bin");
   if (!ds1_path_resolve_nocase(test_dir, rel, bin_path, (size_t) cap))
      return 0;
   /* A compile writes the .bin after reading the text, and never leaves it empty. */
   if (stat(txt_path, &txt_st) != 0 || stat(bin_path, &bin_st) != 0 || bin_st.st_size <= 0 ||
       bin_st.st_mtime < txt_st.st_mtime)
      return 0;
   return 1;
}

/* ---- run -------------------------------------------------------------- */

static void free_names(char ** names, int count)
{
   int i;

   if (names == NULL)
      return;
   for (i = 0; i < count; i++)
      free(names[i]);
   free(names);
}

void mpq_build_base_path(const char * game_dir, char * out, int cap)
{
   if (!ds1_path_resolve_nocase(game_dir, "patch_d2.mpq", out, (size_t) cap))
      snprintf(out, (size_t) cap, "%s/patch_d2.mpq", game_dir);
}

/* A Project Diablo 2 folder: base_mpq's or the test copy's has pd2data.mpq. */
static int folder_has_pd2_data(const char * folder)
{
   char path[PATH_CAP];

   return folder != NULL && folder[0] != '\0' &&
          ds1_path_resolve_nocase(folder, "pd2data.mpq", path, sizeof(path)) &&
          file_size(path) >= 0;
}

static int is_pd2_game(const MPQ_BUILD_ARGS_S * a)
{
   char folder[PATH_CAP];
   char * slash, * back;

   if (folder_has_pd2_data(a->test_game_dir))
      return 1;
   if (a->base_mpq == NULL || strlen(a->base_mpq) >= sizeof(folder))
      return 0;
   strcpy(folder, a->base_mpq);
   slash = strrchr(folder, '/');
   back = strrchr(folder, '\\');
   if (back != NULL && (slash == NULL || back > slash))
      slash = back;
   if (slash == NULL)
      return 0;
   *slash = '\0';
   return folder_has_pd2_data(folder);
}

int mpq_build_run(const MPQ_BUILD_ARGS_S * a, MPQ_BUILD_RESULT_S * out, char * problems_buf,
                  int cap)
{
   PROBLEMS_S problems;
   RUN_S run;
   ENTRY_LIST_S list;
   STR_SET_S listed;
   char ** names = NULL, ** bin_sources = NULL, ** bin_names = NULL;
   char * table = NULL, * listfile_text = NULL;
   long table_len, base_size;
   int i, bins = 0, rc = -1;

   problems.buf = problems_buf;
   problems.cap = cap;
   problems.len = 0;
   problems.count = 0;
   if (problems_buf != NULL && cap > 0)
      problems_buf[0] = '\0';
   if (out != NULL)
   {
      out->files = 0;
      out->bytes = 0;
      out->pd2_ignores_tables = 0;
   }
   memset(&listed, 0, sizeof(listed));

   table = map_ops_read_table(a->project_dir, &table_len);
   if (table == NULL && errno != ENOENT)
      add_problem(&problems, "The project's LvlPrest.txt can't be read.");
   collect(a->project_dir, table, &list);
   run.a = a;
   run.problems = &problems;
   run.project_table = table;
   run.game_table = a->game_table;
   run.list = &list;

   base_size = file_size(a->base_mpq);
   if (base_size < 0)
      add_problem(&problems, "The game folder has no patch_d2.mpq to build on.");
   if (a->base_mpq != NULL && a->out_mpq != NULL && same_path(a->base_mpq, a->out_mpq))
      add_problem(&problems, "The build can't be written over the game's patch_d2.mpq.");
   if (list.failed)
      add_problem(&problems, "Out of memory while choosing the project's files.");

   /* Names the archive can take. */
   names = (char **) calloc((size_t) list.count + 1, sizeof(char *));
   bin_sources = (char **) calloc((size_t) list.count + 1, sizeof(char *));
   bin_names = (char **) calloc((size_t) list.count + 1, sizeof(char *));
   if (names == NULL || bin_sources == NULL || bin_names == NULL)
   {
      add_problem(&problems, "Out of memory while choosing the project's files.");
      goto done;
   }
   for (i = 0; i < list.count; i++)
   {
      names[i] = archive_name(list.items[i].is_table ? EXCEL_PREFIX : TILES_PREFIX,
                              list.items[i].rel, NULL);
      if (names[i] == NULL)
      {
         add_problem(&problems, "Out of memory while choosing the project's files.");
         goto done;
      }
      if (!mpq_build_name_ok(names[i]))
      {
         char shown[PATH_CAP];

         printable(shown, sizeof(shown), list.items[i].rel);
         add_problem(&problems,
                     "%s can't be packed: the archive takes names of up to %d characters in "
                     "plain ASCII. Rename the file or its folder.", shown, NAME_CHARS_MAX);
      }
   }

   /* The project's table against the game's. */
   if (table != NULL)
   {
      if (a->game_table == NULL)
         add_problem(&problems, "The game's LvlPrest.txt can't be read, so the project's "
                                "can't be checked against it.");
      else
      {
         lvlprest_each_cleared(table, a->game_table, check_added_cell, &run);
         lvlprest_each_def(table, check_def_for_holes, &run);
      }
   }

   if (table != NULL)
      lvlprest_each_def(table, check_def_sizes, &run);

   /* A map the game's table can't reach. */
   table_paths(table, &listed);
   for (i = 0; i < list.count; i++)
   {
      if (list.items[i].is_table || !has_extension(list.items[i].rel, ".ds1"))
         continue;
      if (a->in_game != NULL && a->in_game(list.items[i].rel, a->ctx))
         continue;
      if (!set_has(&listed, list.items[i].key))
         add_problem(&problems, "%s isn't in LvlPrest.txt, so the game can't reach it.",
                     list.items[i].rel);
   }

   /* A table the game has compiled as it is now. */
   for (i = 0; i < list.count; i++)
   {
      char bin[PATH_CAP];

      if (!list.items[i].is_table)
         continue;
      if (compiled_bin(a->test_game_dir, file_part(list.items[i].rel), list.items[i].source,
                       bin, sizeof(bin)))
      {
         bin_sources[bins] = dup_text(bin);
         bin_names[bins] = archive_name(EXCEL_PREFIX, list.items[i].rel, ".bin");
         if (bin_sources[bins] == NULL || bin_names[bins] == NULL)
         {
            add_problem(&problems, "Out of memory while choosing the project's files.");
            goto done;
         }
         bins++;
      }
      else
         add_problem(&problems, NOT_COMPILED, file_part(list.items[i].rel));
   }
   if (bins > 0 && base_size >= 0)
   {
      char test_patch[PATH_CAP];

      if (!ds1_path_resolve_nocase(a->test_game_dir, "patch_d2.mpq", test_patch,
                                   sizeof(test_patch)) || file_size(test_patch) != base_size)
         add_problem(&problems, "The test copy is a different game version from the install, "
                                "so its compiled tables can't be used.");
   }

   if (list.count == 0)
      add_problem(&problems, "The project has nothing to pack: no maps, tiles or tables.");
   if (problems.count > 0)
      goto done;

   /* Choose and write. */
   {
      MPQ_WRITE_FILE_S * files = (MPQ_WRITE_FILE_S *)
         calloc((size_t) list.count + (size_t) bins + 1, sizeof(MPQ_WRITE_FILE_S));
      char listfile_path[PATH_CAP], error[512];
      size_t text_len = 1;
      FILE * lf;
      int n = 0;

      if (files == NULL)
      {
         add_problem(&problems, "Out of memory while writing the archive.");
         goto done;
      }
      for (i = 0; i < list.count; i++)
      {
         files[n].archive_name = names[i];
         files[n++].source_path = list.items[i].source;
         text_len += strlen(names[i]) + 2;
      }
      for (i = 0; i < bins; i++)
      {
         files[n].archive_name = bin_names[i];
         files[n++].source_path = bin_sources[i];
         text_len += strlen(bin_names[i]) + 2;
      }

      listfile_text = (char *) malloc(text_len);
      if (listfile_text == NULL ||
          snprintf(listfile_path, sizeof(listfile_path), "%s/" LISTFILE_TEMP,
                   a->project_dir) >= (int) sizeof(listfile_path))
      {
         add_problem(&problems, "Out of memory while writing the archive.");
         free(files);
         goto done;
      }
      text_len = 0;
      for (i = 0; i < n; i++)
      {
         if (i > 0)
         {
            listfile_text[text_len++] = '\r';
            listfile_text[text_len++] = '\n';
         }
         strcpy(listfile_text + text_len, files[i].archive_name);
         text_len += strlen(files[i].archive_name);
      }
      listfile_text[text_len] = '\0';

      if (!project_ensure_parent_dirs(listfile_path) ||
          !project_ensure_parent_dirs(a->out_mpq) ||
          (lf = fopen(listfile_path, "wb")) == NULL)
      {
         add_problem(&problems, "The build folder can't be written in. Check the project's "
                                "folder isn't read-only.");
         free(files);
         goto done;
      }
      if (fwrite(listfile_text, 1, text_len, lf) != text_len)
      {
         fclose(lf);
         remove(listfile_path);
         add_problem(&problems, "The build folder is full or can't be written.");
         free(files);
         goto done;
      }
      if (fclose(lf) != 0)
      {
         remove(listfile_path);
         add_problem(&problems, "The build folder is full or can't be written.");
         free(files);
         goto done;
      }
      files[n].archive_name = LISTFILE_NAME;
      files[n].source_path = listfile_path;

      error[0] = '\0';
      if (mpq_write(a->base_mpq, a->out_mpq, files, n + 1, error, sizeof(error)) != 0)
         add_problem(&problems, "The archive couldn't be written: %s", error);
      else
      {
         rc = 0;
         if (out != NULL)
         {
            out->files = n;
            out->bytes = file_size(a->out_mpq);
            out->pd2_ignores_tables = bins > 0 && is_pd2_game(a);
         }
      }
      remove(listfile_path);
      free(files);
   }

done:
   free_names(names, list.count + 1);
   free_names(bin_sources, list.count + 1);
   free_names(bin_names, list.count + 1);
   free(listfile_text);
   set_free(&listed);
   list_free(&list);
   free(table);
   return rc;
}
