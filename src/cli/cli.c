#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#include "cli/cli.h"
#include "config.h"
#include "structs.h"
#include "core/d2install.h"

extern void ds1edit_open_all_mpq(void);

/* CLI exit codes (consistent across verbs). */
#define CLI_EXIT_OK         0
#define CLI_EXIT_PARTIAL    1
#define CLI_EXIT_NOTHING    2
#define CLI_EXIT_BAD_ARGS   3

/* ---- Verb registry --------------------------------------------------- */

typedef int (*cli_verb_fn)(int argc, char **argv);

typedef struct CLI_VERB_S
{
   const char  *name;
   cli_verb_fn  fn;
   const char  *summary;
} CLI_VERB_S;

/* Forward declarations of per-verb entry points. As we add Phase N
 * verbs we register them here. */
static int verb_list_mpqs(int argc, char **argv);
static int verb_help     (int argc, char **argv);

static const CLI_VERB_S s_verbs[] = {
   { "list-mpqs", verb_list_mpqs,
     "Show which MPQ slots are open and how many files each contains." },
   { "help",      verb_help,
     "Show this help text." },
   { "--help",    verb_help, NULL },
   { "-h",        verb_help, NULL },
   { NULL, NULL, NULL }
};

static const CLI_VERB_S *find_verb(const char *name)
{
   int i;
   if (name == NULL) return NULL;
   for (i = 0; s_verbs[i].name != NULL; i++)
   {
      if (strcasecmp(s_verbs[i].name, name) == 0)
         return &s_verbs[i];
   }
   return NULL;
}

int cli_is_verb(int argc, char **argv)
{
   if (argc < 2 || argv == NULL || argv[1] == NULL) return 0;
   return (find_verb(argv[1]) != NULL) ? 1 : 0;
}

/* ---- Common option parsing ------------------------------------------ */

/* Pulled out of the main flag parser so verbs can call it directly when
 * they only need the config-affecting subset (e.g. list-mpqs). Walks
 * argv looking for --d2-install / --mpq / --mod-dir / --ini / --no-ini
 * and applies them on top of the loaded INI. */
typedef struct CLI_COMMON_OPTS_S
{
   const char *ini_path;       /* NULL = use default "ds1edit.ini" */
   int         no_ini;         /* 1 = don't load any INI */
   const char *d2_install;     /* NULL = no override */
   const char *mod_dir;        /* NULL = no override */
   const char *extra_mpqs[MAX_MPQ_FILE]; /* appended-style additions */
   int         extra_mpq_count;
   int         verbose;        /* 0 / 1 / 2 (-v / -vv) */
} CLI_COMMON_OPTS_S;

static int parse_common_opts(int argc, char **argv, CLI_COMMON_OPTS_S *out)
{
   int i;
   memset(out, 0, sizeof(*out));
   /* Walk all flags; ignore unknowns (per-verb parsers will see them). */
   for (i = 2; i < argc; i++)
   {
      const char *a = argv[i];
      if (a == NULL) continue;

      if (strcmp(a, "-v") == 0)
         out->verbose = (out->verbose < 1) ? 1 : out->verbose;
      else if (strcmp(a, "-vv") == 0 || strcmp(a, "--verbose") == 0)
         out->verbose = 2;
      else if (strcmp(a, "--no-ini") == 0)
         out->no_ini = 1;
      else if (strncmp(a, "--ini=", 6) == 0)
         out->ini_path = a + 6;
      else if (strncmp(a, "--d2-install=", 13) == 0)
         out->d2_install = a + 13;
      else if (strncmp(a, "--mod-dir=", 10) == 0)
         out->mod_dir = a + 10;
      else if (strncmp(a, "--mpq=", 6) == 0)
      {
         if (out->extra_mpq_count < MAX_MPQ_FILE)
            out->extra_mpqs[out->extra_mpq_count++] = a + 6;
      }
   }
   return 1;
}

/* Allocate a heap copy of `s` and assign through `target`. Replaces any
 * prior pointer; we leak it -- matching the convention of the existing
 * config layer in config.c, which does the same. */
static void replace_string(char **target, const char *s)
{
   size_t n;
   char *buf;
   if (target == NULL || s == NULL) return;
   n = strlen(s);
   buf = (char *) malloc(n + 1);
   if (buf == NULL) return;
   memcpy(buf, s, n + 1);
   *target = buf;
}

/* ---- Init: minimum config + MPQ open --------------------------------- */

/* Init the editor's config + MPQ state to the point where
 * misc_load_mpq_file works. Must be called once per CLI process before
 * any verb runs. Matches the pre-display init sequence in main.c
 * minus the GUI bits. */
static int cli_minimum_init(const CLI_COMMON_OPTS_S *opts)
{
   int i;
   const char *ininame = (opts->ini_path != NULL && opts->ini_path[0] != 0)
                         ? opts->ini_path : "ds1edit.ini";

   /* Zero the config + MPQ slot state. ds1edit_init normally does this
    * but it allocates DS1/DT1 buffers + cursors + a lot of other things
    * we don't need; bare metal is enough for the CLI. */
   memset(&glb_config, 0, sizeof(glb_config));
   for (i = 0; i < MAX_MPQ_FILE; i++)
   {
      memset(&glb_mpq_struct[i], 0, sizeof(GLB_MPQ_S));
      glb_mpq_struct[i].is_open = FALSE;
   }

   /* Load INI (unless --no-ini). */
   if (!opts->no_ini)
   {
      FILE *fp = fopen(ininame, "rb");
      if (fp != NULL)
      {
         fclose(fp);
         ini_read((char *) ininame);
      }
      else if (opts->ini_path != NULL)
      {
         fprintf(stderr, "ds1edit: cannot open --ini=%s\n", opts->ini_path);
         return CLI_EXIT_BAD_ARGS;
      }
      /* If default INI is missing we silently continue; CLI flags or
       * d2install_resolve_mpqs may still produce a working chain. */
   }

   /* Apply CLI overrides. d2_install fills empty MPQ slots via
    * d2install_resolve_mpqs. mod_dir replaces slot [0]. extra_mpqs
    * append into the first empty slot, OR replace the lowest-priority
    * one if the chain is full. */
   if (opts->d2_install != NULL)
      replace_string(&glb_config.d2_install, opts->d2_install);
   if (opts->mod_dir != NULL)
      replace_string(&glb_config.mod_dir[0], opts->mod_dir);

   d2install_resolve_mpqs();

   /* Append --mpq= entries into empty slots. We don't displace existing
    * filled slots: appending into an empty slot is the conservative
    * thing for "I'm just adding d2char.mpq because it was missing." */
   for (i = 0; i < opts->extra_mpq_count; i++)
   {
      int slot;
      const char *path = opts->extra_mpqs[i];
      if (path == NULL || path[0] == 0) continue;
      for (slot = 0; slot < MAX_MPQ_FILE; slot++)
      {
         if (glb_config.mpq_file[slot] == NULL
             || glb_config.mpq_file[slot][0] == 0)
         {
            replace_string(&glb_config.mpq_file[slot], path);
            break;
         }
      }
      if (slot >= MAX_MPQ_FILE)
         fprintf(stderr,
                 "ds1edit: --mpq=%s ignored (chain full, %d slots used)\n",
                 path, MAX_MPQ_FILE);
   }

   /* Open every MPQ that resolved to a path. ds1edit_open_all_mpq
    * iterates the slots, opens each, and leaves is_open == TRUE on
    * the global structs. */
   ds1edit_open_all_mpq();

   return CLI_EXIT_OK;
}

/* ---- list-mpqs ------------------------------------------------------- */

static const char *slot_label(int slot)
{
   /* Slot layout matches d2install.c: 0=patch_d2, 1=d2exp, 2=d2data,
    * 3=d2char. Names are useful for the missing-d2char debug case. */
   switch (slot)
   {
      case 0: return "patch_d2";
      case 1: return "d2exp";
      case 2: return "d2data";
      case 3: return "d2char";
      default: return "?";
   }
}

static int verb_list_mpqs(int argc, char **argv)
{
   CLI_COMMON_OPTS_S opts;
   int rc;
   int i;
   int open_count = 0;

   if (!parse_common_opts(argc, argv, &opts))
      return CLI_EXIT_BAD_ARGS;

   rc = cli_minimum_init(&opts);
   if (rc != CLI_EXIT_OK) return rc;

   /* Mod dir status. Reported even when not configured -- the absence
    * is the diagnostic. */
   printf("mod_dir: %s\n",
          (glb_config.mod_dir[0] != NULL && glb_config.mod_dir[0][0] != 0)
             ? glb_config.mod_dir[0]
             : "(none)");

   /* Per-slot status. */
   for (i = 0; i < MAX_MPQ_FILE; i++)
   {
      const char *path = glb_config.mpq_file[i];
      int is_open = glb_mpq_struct[i].is_open ? 1 : 0;
      DWORD count_files = is_open ? glb_mpq_struct[i].count_files : 0;

      printf("slot %d (%-9s): ", i, slot_label(i));
      if (path == NULL || path[0] == 0)
      {
         printf("(unset)\n");
         continue;
      }
      printf("%s -> %s",
             path,
             is_open ? "open" : "FAILED to open");
      if (is_open)
      {
         printf(" (%lu files)", (unsigned long) count_files);
         open_count++;
      }
      printf("\n");
   }

   printf("\n%d MPQ slot(s) open of %d configured.\n",
          open_count, MAX_MPQ_FILE);

   if (open_count == 0)
   {
      fprintf(stderr,
              "ds1edit: no MPQs are open. Set d2_install in Ds1edit.ini, "
              "pass --d2-install=<dir>, or use --mpq=<path> to add one.\n");
      return CLI_EXIT_NOTHING;
   }
   return CLI_EXIT_OK;
}

/* ---- help ------------------------------------------------------------ */

static int verb_help(int argc, char **argv)
{
   int i;
   (void) argc; (void) argv;

   printf(
      "ds1edit -- Diablo II DS1 area editor (CLI mode)\n"
      "\n"
      "Usage:\n"
      "  ds1edit [verb] [args...]\n"
      "  ds1edit                       run in GUI mode\n"
      "\n"
      "Verbs:\n");
   for (i = 0; s_verbs[i].name != NULL; i++)
   {
      if (s_verbs[i].summary == NULL) continue;  /* alias */
      printf("  %-20s  %s\n", s_verbs[i].name, s_verbs[i].summary);
   }
   printf(
      "\n"
      "Common flags (apply to most verbs):\n"
      "  --ini=<path>          load this INI instead of ds1edit.ini\n"
      "  --no-ini              do not load any INI; use only CLI flags\n"
      "  --d2-install=<dir>    override d2_install (auto-fills MPQ slots)\n"
      "  --mod-dir=<dir>       override mod_dir[0] (overlay)\n"
      "  --mpq=<path>          append an MPQ to the chain (repeatable)\n"
      "  -v, -vv               increase verbosity\n"
      "\n"
      "Exit codes:\n"
      "  0 = clean run; 1 = some failures; 2 = nothing produced;\n"
      "  3 = bad arguments.\n");
   return CLI_EXIT_OK;
}

/* ---- Top-level dispatcher ------------------------------------------- */

int cli_run(int argc, char **argv)
{
   const CLI_VERB_S *v;

   if (argc < 2 || argv[1] == NULL)
      return verb_help(argc, argv);

   v = find_verb(argv[1]);
   if (v == NULL)
   {
      fprintf(stderr, "ds1edit: unknown verb '%s'\n", argv[1]);
      verb_help(argc, argv);
      return CLI_EXIT_BAD_ARGS;
   }
   return v->fn(argc, argv);
}
