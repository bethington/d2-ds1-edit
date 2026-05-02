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
#include "core/compose_cof.h"
#include "core/compose_cof_path.h"
#include "core/compose_index.h"
#include "core/compose_iter.h"
#include "core/compose_naming.h"
#include "core/compose_palette.h"
#include "core/compose_presets.h"
#include "core/d2install.h"

extern void ds1edit_open_all_mpq(void);
extern int  misc_load_mpq_file(char *filename, char **buffer, long *buf_len, int output);

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
static int verb_list_mpqs   (int argc, char **argv);
static int verb_probe       (int argc, char **argv);
static int verb_probe_cof   (int argc, char **argv);
static int verb_list_tokens (int argc, char **argv);
static int verb_list_presets(int argc, char **argv);
static int verb_help        (int argc, char **argv);

static const CLI_VERB_S s_verbs[] = {
   { "list-mpqs",    verb_list_mpqs,
     "Show which MPQ slots are open and how many files each contains." },
   { "probe",        verb_probe,
     "Look up a virtual path in the MPQ chain (e.g. data\\Global\\AnimData.d2)." },
   { "probe-cof",    verb_probe_cof,
     "Build the COF path for <category> <token> <mode> <weapon>, load + parse, dump info." },
   { "list-tokens",  verb_list_tokens,
     "Dump tokens compose_index discovered in <category> (chars/monsters/npcs/objects)." },
   { "list-presets", verb_list_presets,
     "Dump parsed [char_mode_presets] and [char_weapon_presets] from Ds1edit.ini." },
   { "help",         verb_help,
     "Show this help text." },
   { "--help",       verb_help, NULL },
   { "-h",           verb_help, NULL },
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

/* Collect positional (non-flag) args from argv into out_args (capacity
 * out_cap). Skips argv[0] (program) and argv[1] (verb). Flags are any
 * argument starting with "-". Returns the count actually collected. */
static int collect_positional(int argc, char **argv,
                              const char **out_args, int out_cap)
{
   int i, n = 0;
   for (i = 2; i < argc && n < out_cap; i++)
   {
      const char *a = argv[i];
      if (a == NULL) continue;
      if (a[0] == '-') continue;  /* flag */
      out_args[n++] = a;
   }
   return n;
}

/* Map a category name to COMPOSE_CATEGORY_E. Returns COMPOSE_CATEGORY_NONE
 * (which equals 0 / falsy) on unknown name. */
static COMPOSE_CATEGORY_E parse_category(const char *name)
{
   if (name == NULL) return COMPOSE_CATEGORY_NONE;
   if (strcasecmp(name, "chars") == 0
       || strcasecmp(name, "char") == 0
       || strcasecmp(name, "player") == 0
       || strcasecmp(name, "players") == 0)
      return COMPOSE_CATEGORY_PLAYER_CHAR;
   if (strcasecmp(name, "monsters") == 0
       || strcasecmp(name, "monster") == 0)
      return COMPOSE_CATEGORY_MONSTER;
   if (strcasecmp(name, "npcs") == 0
       || strcasecmp(name, "npc") == 0)
      return COMPOSE_CATEGORY_NPC;
   if (strcasecmp(name, "objects") == 0
       || strcasecmp(name, "object") == 0)
      return COMPOSE_CATEGORY_OBJECT;
   return COMPOSE_CATEGORY_NONE;
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

/* ---- probe ----------------------------------------------------------- */

static int verb_probe(int argc, char **argv)
{
   CLI_COMMON_OPTS_S opts;
   const char *positional[4];
   int n_pos;
   const char *path;
   char *buf = NULL;
   long buf_len = 0;
   int rc;
   int slot;
   int found_slot = -1;

   if (!parse_common_opts(argc, argv, &opts))
      return CLI_EXIT_BAD_ARGS;

   n_pos = collect_positional(argc, argv, positional, 4);
   if (n_pos < 1)
   {
      fprintf(stderr,
         "ds1edit probe: missing path argument\n"
         "Usage: ds1edit probe <virtual-path>\n"
         "Example: ds1edit probe \"data\\Global\\AnimData.d2\"\n");
      return CLI_EXIT_BAD_ARGS;
   }
   path = positional[0];

   rc = cli_minimum_init(&opts);
   if (rc != CLI_EXIT_OK) return rc;

   /* Try the chain. misc_load_mpq_file walks mod_dir then each open
    * MPQ slot in order; we want to know which slot answered. The
    * cleanest way is to call it once for the report, then walk slots
    * manually if the high-level call succeeded so we can identify
    * which one had it. */
   {
      char path_buf[512];
      strncpy(path_buf, path, sizeof(path_buf) - 1);
      path_buf[sizeof(path_buf) - 1] = 0;

      if (misc_load_mpq_file(path_buf, &buf, &buf_len, 0) == -1)
      {
         printf("not found in chain: %s\n", path);
         if (buf != NULL) free(buf);
         return CLI_EXIT_NOTHING;
      }
   }

   /* Identify which slot answered. We don't have a direct API for that
    * (misc_load_mpq_file is fire-and-forget), so we re-probe each
    * open slot's hash table directly. */
   for (slot = 0; slot < MAX_MPQ_FILE; slot++)
   {
      extern GLB_MPQ_S *glb_mpq;
      extern DWORD test_tell_entry(char *filename);
      char path_buf[512];
      DWORD entry;

      if (!glb_mpq_struct[slot].is_open) continue;

      strncpy(path_buf, path, sizeof(path_buf) - 1);
      path_buf[sizeof(path_buf) - 1] = 0;

      glb_mpq = &glb_mpq_struct[slot];
      entry = test_tell_entry(path_buf);
      if (entry != (DWORD) -1)
      {
         found_slot = slot;
         break;
      }
   }

   printf("found: %s\n", path);
   printf("  size:    %ld bytes\n", buf_len);
   if (found_slot >= 0)
      printf("  slot:    %d (%s) -- %s\n",
             found_slot, slot_label(found_slot),
             glb_config.mpq_file[found_slot] != NULL
                ? glb_config.mpq_file[found_slot] : "?");
   else
      printf("  source:  mod_dir overlay\n");

   if (buf != NULL) free(buf);
   return CLI_EXIT_OK;
}

/* ---- probe-cof ------------------------------------------------------- */

static int verb_probe_cof(int argc, char **argv)
{
   CLI_COMMON_OPTS_S opts;
   const char *positional[5];
   int n_pos;
   COMPOSE_CATEGORY_E category;
   const char *token, *mode, *wclass;
   char path[512];
   const char *base;
   char *buf = NULL;
   long buf_len = 0;
   COMPOSE_COF_S cof;
   int rc;

   if (!parse_common_opts(argc, argv, &opts))
      return CLI_EXIT_BAD_ARGS;

   n_pos = collect_positional(argc, argv, positional, 5);
   if (n_pos < 3)
   {
      fprintf(stderr,
         "ds1edit probe-cof: too few arguments\n"
         "Usage: ds1edit probe-cof <category> <token> <mode> [<weapon>]\n"
         "  category: chars / monsters / npcs / objects\n"
         "Examples:\n"
         "  ds1edit probe-cof chars NE WL HTH\n"
         "  ds1edit probe-cof monsters AN NU\n");
      return CLI_EXIT_BAD_ARGS;
   }

   category = parse_category(positional[0]);
   if (category == COMPOSE_CATEGORY_NONE)
   {
      fprintf(stderr,
         "ds1edit probe-cof: unknown category '%s'\n"
         "  expected one of: chars / monsters / npcs / objects\n",
         positional[0]);
      return CLI_EXIT_BAD_ARGS;
   }
   token  = positional[1];
   mode   = positional[2];
   wclass = (n_pos >= 4) ? positional[3] : "";
   if (wclass != NULL && (strcmp(wclass, "-") == 0
                          || strcasecmp(wclass, "none") == 0))
      wclass = "";

   rc = cli_minimum_init(&opts);
   if (rc != CLI_EXIT_OK) return rc;

   base = compose_iter_category_base(category);
   if (base == NULL)
   {
      fprintf(stderr,
         "ds1edit probe-cof: no base path for that category (internal)\n");
      return CLI_EXIT_BAD_ARGS;
   }

   if (!compose_cof_path_build(path, (int) sizeof(path),
                               base, token, mode,
                               wclass != NULL ? wclass : ""))
   {
      fprintf(stderr,
         "ds1edit probe-cof: failed to build COF path "
         "(token/mode/weapon too long?)\n");
      return CLI_EXIT_BAD_ARGS;
   }

   printf("path: %s\n", path);

   if (misc_load_mpq_file(path, &buf, &buf_len, 0) == -1)
   {
      printf("not found in chain.\n");
      if (buf != NULL) free(buf);
      return CLI_EXIT_NOTHING;
   }

   memset(&cof, 0, sizeof(cof));
   if (!compose_cof_parse(buf, buf_len, &cof))
   {
      printf("found (%ld bytes) but failed to parse as COF.\n", buf_len);
      free(buf);
      return CLI_EXIT_PARTIAL;
   }

   printf("loaded + parsed.\n");
   printf("  size:            %ld bytes\n", buf_len);
   printf("  layer_count:     %d\n", cof.layer_count);
   printf("  frames_per_dir:  %d\n", cof.frames_per_dir);
   printf("  direction_count: %d\n", cof.direction_count);
   printf("  version:         %d\n", cof.version);
   if (opts.verbose >= 1)
   {
      int i;
      printf("  layers:\n");
      for (i = 0; i < cof.layer_count; i++)
      {
         printf("    [%d] composit=%d weapon_class=\"%s\"\n",
                i,
                cof.layers[i].composit_index,
                cof.layers[i].weapon_class);
      }
   }

   compose_cof_free(&cof);
   free(buf);
   return CLI_EXIT_OK;
}

/* ---- list-tokens ----------------------------------------------------- */

static int verb_list_tokens(int argc, char **argv)
{
   CLI_COMMON_OPTS_S opts;
   const char *positional[2];
   int n_pos;
   COMPOSE_CATEGORY_E category;
   int rc, i, count;

   if (!parse_common_opts(argc, argv, &opts))
      return CLI_EXIT_BAD_ARGS;

   n_pos = collect_positional(argc, argv, positional, 2);
   if (n_pos < 1)
   {
      fprintf(stderr,
         "ds1edit list-tokens: missing category argument\n"
         "Usage: ds1edit list-tokens <chars|monsters|npcs|objects>\n");
      return CLI_EXIT_BAD_ARGS;
   }

   category = parse_category(positional[0]);
   if (category == COMPOSE_CATEGORY_NONE)
   {
      fprintf(stderr, "ds1edit list-tokens: unknown category '%s'\n",
              positional[0]);
      return CLI_EXIT_BAD_ARGS;
   }

   rc = cli_minimum_init(&opts);
   if (rc != CLI_EXIT_OK) return rc;

   /* Player chars come from the hardcoded list in compose_iter; the
    * other categories require the MonStats/Objects index. */
   if (category == COMPOSE_CATEGORY_PLAYER_CHAR)
   {
      count = compose_iter_player_class_count();
      printf("category: chars  count: %d\n", count);
      for (i = 0; i < count; i++)
      {
         const char *code = compose_iter_player_class_at(i);
         const char *name = compose_naming_class_name(code);
         printf("  %-8s %s\n", code != NULL ? code : "?",
                name != NULL ? name : "");
      }
      return CLI_EXIT_OK;
   }

   if (!compose_index_build())
   {
      fprintf(stderr,
         "ds1edit list-tokens: compose_index_build failed "
         "(MonStats.txt / Objects.txt missing or unparseable)\n");
      return CLI_EXIT_NOTHING;
   }

   switch (category)
   {
      case COMPOSE_CATEGORY_MONSTER:
         count = compose_index_monster_count();
         printf("category: monsters  count: %d\n", count);
         for (i = 0; i < count; i++)
         {
            const COMPOSE_TOKEN_S *t = compose_index_monster_at(i);
            if (t != NULL) printf("  %-8s %s\n", t->code, t->name);
         }
         break;
      case COMPOSE_CATEGORY_NPC:
         count = compose_index_npc_count();
         printf("category: npcs  count: %d\n", count);
         for (i = 0; i < count; i++)
         {
            const COMPOSE_TOKEN_S *t = compose_index_npc_at(i);
            if (t != NULL) printf("  %-8s %s\n", t->code, t->name);
         }
         break;
      case COMPOSE_CATEGORY_OBJECT:
         count = compose_index_object_count();
         printf("category: objects  count: %d\n", count);
         for (i = 0; i < count; i++)
         {
            const COMPOSE_TOKEN_S *t = compose_index_object_at(i);
            if (t != NULL) printf("  %-8s %s\n", t->code, t->name);
         }
         break;
      default:
         return CLI_EXIT_BAD_ARGS;
   }
   return CLI_EXIT_OK;
}

/* ---- list-presets ---------------------------------------------------- */

static int dump_preset_table(const char *label,
                             int (*count_fn)(void),
                             const COMPOSE_PRESET_S *(*at_fn)(int))
{
   int n = count_fn();
   int i, j;

   printf("[%s]  count: %d\n", label, n);
   for (i = 0; i < n; i++)
   {
      const COMPOSE_PRESET_S *p = at_fn(i);
      if (p == NULL) continue;
      printf("  %-24s = ", p->name);
      for (j = 0; j < p->code_count; j++)
      {
         if (j > 0) printf(", ");
         printf("%s", p->codes[j]);
      }
      printf("\n");
   }
   return n;
}

static int verb_list_presets(int argc, char **argv)
{
   CLI_COMMON_OPTS_S opts;
   int rc;

   if (!parse_common_opts(argc, argv, &opts))
      return CLI_EXIT_BAD_ARGS;

   /* INI load is the load-bearing thing here; MPQ chain is irrelevant.
    * cli_minimum_init runs both, which is fine -- the MPQ open just
    * adds a few hundred ms. */
   rc = cli_minimum_init(&opts);
   if (rc != CLI_EXIT_OK && rc != CLI_EXIT_NOTHING)
      return rc;
   /* Tolerate "no MPQs" here -- we only need the INI. */

   dump_preset_table("char_mode_presets",
                     compose_mode_presets_count,
                     compose_mode_presets_at);
   printf("\n");
   dump_preset_table("char_weapon_presets",
                     compose_weapon_presets_count,
                     compose_weapon_presets_at);
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
