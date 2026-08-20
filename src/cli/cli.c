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
#include "core/asset_export.h"
#include "core/compose_apng.h"
#include "core/compose_cof.h"
#include "core/compose_cof_path.h"
#include "core/compose_index.h"
#include "core/compose_iter.h"
#include "core/compose_naming.h"
#include "core/compose_palette.h"
#include "core/compose_palette_index.h"
#include "core/compose_presets.h"
#include "core/compose_render.h"
#include "core/d2install.h"
#include "core/export_presets.h"
#include "core/monstats2.h"
#include "core/upscale.h"
#include "core/area_browser.h"

extern void ds1edit_load_palettes(void);
extern void misc_read_gamma(void);

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
static int verb_list_mpqs     (int argc, char **argv);
static int verb_probe         (int argc, char **argv);
static int verb_probe_cof     (int argc, char **argv);
static int verb_list_tokens   (int argc, char **argv);
static int verb_list_presets  (int argc, char **argv);
static int verb_export_compose(int argc, char **argv);
static int verb_export_raw    (int argc, char **argv);
static int verb_dump_txt_row  (int argc, char **argv);
static int verb_dump_listfile (int argc, char **argv);
static int verb_list_areas    (int argc, char **argv);
static int verb_list_files    (int argc, char **argv);
static int verb_audit_lvltypes(int argc, char **argv);
static int verb_help          (int argc, char **argv);

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
   { "export",         verb_export_compose,
     "Compose-mode APNG export. Alias for export-compose." },
   { "export-compose", verb_export_compose,
     "Compose layered DCC files into APNG animations for one or more tuples." },
   { "export-raw",     verb_export_raw,
     "Raw-frame export (DCC/DC6/DT1) -> per-frame PNG, mirrors the GUI flow." },
   { "dump-txt-row",   verb_dump_txt_row,
     "Load <txt-path>, find row matching <id> in column 0, dump column=value pairs." },
   { "dump-listfile",  verb_dump_listfile,
     "Print the in-MPQ (listfile) for each open slot to stdout (paths only)." },
   { "list-areas",   verb_list_areas,
     "List every area the browser discovered, grouped by Act." },
   { "list-files",   verb_list_files,
     "List DS1 files known to the browser; optional substring filter." },
   { "audit-lvltypes", verb_audit_lvltypes,
     "Report LvlTypes.txt rows whose name prefix disagrees with their Act column." },
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
   const char *a;
   if (argc < 2 || argv == NULL || argv[1] == NULL) return 0;
   a = argv[1];
   if (a[0] == 0) return 0;
   /* A '-'-prefixed argv[1] only belongs to the CLI when it actually names
    * a verb (or is a help flag). Claiming every dashed argument is what made
    * --list-areas, --area, --file and friends answer 'unknown verb': they
    * never reached misc_cmd_line_parse. Anything we do not recognise is left
    * to the legacy parser, which is what owns those flags. */
   if (a[0] == '-')
   {
      const char *bare = a;
      while (*bare == '-') bare++;
      if (*bare == 0) return 0;
      if (strcasecmp(bare, "help") == 0 || strcasecmp(bare, "h") == 0)
         return 1;
      if (strcasecmp(bare, "list-areas-ext") == 0)
         return 1;
      return find_verb(bare) != NULL;
   }
   /* A path-shaped argv[1] (e.g. drag-and-drop a .ds1 file onto the
    * exe) is a GUI invocation, not a verb. Heuristic: contains a
    * path separator, a colon, or ends in a known file extension. */
   if (strchr(a, '\\') != NULL || strchr(a, '/') != NULL
       || strchr(a, ':') != NULL)
      return 0;
   {
      const char *dot = strrchr(a, '.');
      if (dot != NULL
          && (strcasecmp(dot, ".ds1") == 0
              || strcasecmp(dot, ".dt1") == 0
              || strcasecmp(dot, ".dc6") == 0
              || strcasecmp(dot, ".dcc") == 0
              || strcasecmp(dot, ".cof") == 0))
         return 0;
   }
   /* Treat as an attempted CLI verb -- known ones dispatch, unknowns
    * return CLI_EXIT_BAD_ARGS via cli_run. */
   return 1;
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

/* ---- export-compose -------------------------------------------------- */

/* Find a flag of the form "--name=<value>" in argv; return the value
 * (pointer into argv[i]) or NULL if not present. */
static const char *find_flag_value(int argc, char **argv, const char *name)
{
   size_t nlen = strlen(name);
   int i;
   for (i = 2; i < argc; i++)
   {
      const char *a = argv[i];
      if (a == NULL || a[0] != '-' || a[1] != '-') continue;
      if (strncmp(a + 2, name, nlen) == 0 && a[2 + nlen] == '=')
         return a + 2 + nlen + 1;
   }
   return NULL;
}

/* A list of code strings parsed from "all" / "NU,WL" / "NU". */
#define TUPLE_LIST_MAX 32

typedef struct TUPLE_LIST_S
{
   int  use_all;     /* 1 = use the default list provided by caller */
   int  count;
   char codes[TUPLE_LIST_MAX][16];
} TUPLE_LIST_S;

static int parse_tuple_list(const char *raw, TUPLE_LIST_S *out)
{
   const char *p, *start;
   int n;
   memset(out, 0, sizeof(*out));
   if (raw == NULL || raw[0] == 0
       || strcasecmp(raw, "all") == 0
       || strcasecmp(raw, "*") == 0)
   {
      out->use_all = 1;
      return 1;
   }
   p = raw;
   while (*p != 0 && out->count < TUPLE_LIST_MAX)
   {
      while (*p == ' ' || *p == ',') p++;
      if (*p == 0) break;
      start = p;
      while (*p != 0 && *p != ',') p++;
      n = (int) (p - start);
      while (n > 0 && (start[n-1] == ' ')) n--;
      if (n <= 0) continue;
      if (n > 15) n = 15;
      memcpy(out->codes[out->count], start, (size_t) n);
      out->codes[out->count][n] = 0;
      out->count++;
   }
   return out->count > 0;
}

/* Direction selector: "all" / "N" / "N,M" / "N-M". The output is a
 * fixed-size array of int directions; count==-1 sentinel means "all
 * directions, decided per-tuple from the COF probe". */
#define DIR_LIST_MAX 64

typedef struct DIR_LIST_S
{
   int use_all;
   int count;
   int dirs[DIR_LIST_MAX];
} DIR_LIST_S;

static int parse_dir_list(const char *raw, DIR_LIST_S *out)
{
   const char *p;
   memset(out, 0, sizeof(*out));
   if (raw == NULL || raw[0] == 0 || strcasecmp(raw, "all") == 0)
   {
      out->use_all = 1;
      return 1;
   }
   /* Range "N-M" */
   p = strchr(raw, '-');
   if (p != NULL && p > raw)
   {
      int a = atoi(raw);
      int b = atoi(p + 1);
      int i;
      if (b < a) { int t = a; a = b; b = t; }
      for (i = a; i <= b && out->count < DIR_LIST_MAX; i++)
         out->dirs[out->count++] = i;
      return out->count > 0;
   }
   /* Comma list (or single int) */
   {
      const char *q = raw;
      while (*q != 0 && out->count < DIR_LIST_MAX)
      {
         while (*q == ' ' || *q == ',') q++;
         if (*q == 0) break;
         out->dirs[out->count++] = atoi(q);
         while (*q != 0 && *q != ',') q++;
      }
   }
   return out->count > 0;
}

/* Resolve the leading token of an "A/B[/C]" --target= shorthand to a
 * compose category. Player class codes win; otherwise we look in
 * compose_index (which must already be built) for a unique match. */
static int resolve_target_token_category(const char *token,
                                         COMPOSE_CATEGORY_E *out_cat,
                                         char *full_name, int full_name_cap)
{
   int i, hits = 0;
   COMPOSE_CATEGORY_E cat = COMPOSE_CATEGORY_NONE;
   const char *name = NULL;

   if (token == NULL || token[0] == 0) return 0;
   if (full_name != NULL && full_name_cap > 0) full_name[0] = 0;

   if (compose_naming_class_name(token) != NULL)
   {
      *out_cat = COMPOSE_CATEGORY_PLAYER_CHAR;
      name = compose_naming_class_name(token);
      if (full_name != NULL && name != NULL)
      {
         strncpy(full_name, name, (size_t) full_name_cap - 1);
         full_name[full_name_cap - 1] = 0;
      }
      return 1;
   }

   for (i = 0; i < compose_index_monster_count(); i++)
   {
      const COMPOSE_TOKEN_S *t = compose_index_monster_at(i);
      if (t != NULL && strcasecmp(t->code, token) == 0)
      {
         cat = COMPOSE_CATEGORY_MONSTER; name = t->name; hits++;
         break;
      }
   }
   if (hits == 0)
   {
      for (i = 0; i < compose_index_npc_count(); i++)
      {
         const COMPOSE_TOKEN_S *t = compose_index_npc_at(i);
         if (t != NULL && strcasecmp(t->code, token) == 0)
         {
            cat = COMPOSE_CATEGORY_NPC; name = t->name; hits++;
            break;
         }
      }
   }
   if (hits == 0)
   {
      for (i = 0; i < compose_index_object_count(); i++)
      {
         const COMPOSE_TOKEN_S *t = compose_index_object_at(i);
         if (t != NULL && strcasecmp(t->code, token) == 0)
         {
            cat = COMPOSE_CATEGORY_OBJECT; name = t->name; hits++;
            break;
         }
      }
   }
   if (hits == 0) return 0;

   *out_cat = cat;
   if (full_name != NULL && name != NULL)
   {
      strncpy(full_name, name, (size_t) full_name_cap - 1);
      full_name[full_name_cap - 1] = 0;
   }
   return 1;
}

/* Bring the editor up to the point where compose_apng_export works:
 * MPQ chain open, palettes loaded, a5_current_palette set. Caller has
 * already done cli_minimum_init. */
/* Allocate the global DS1/DT1 storage tables. The full ds1edit_init
 * does this plus a lot of GUI work (cursors, font, screen buffer,
 * etc.) we deliberately skip in CLI mode -- but the asset_export
 * pipeline's DT1 path uses glb_dt1[idx] as a slot table, so it
 * needs to exist. Idempotent. */
static void cli_alloc_global_buffers(void)
{
   if (glb_ds1 == NULL)
   {
      size_t bytes = sizeof(DS1_S) * DS1_MAX;
      glb_ds1 = (DS1_S *) malloc(bytes);
      if (glb_ds1 != NULL) memset(glb_ds1, 0, bytes);
   }
   if (glb_dt1 == NULL)
   {
      size_t bytes = sizeof(DT1_S) * DT1_MAX;
      glb_dt1 = (DT1_S *) malloc(bytes);
      if (glb_dt1 != NULL) memset(glb_dt1, 0, bytes);
   }
}

/* Build the gamma tables. This used to read Data/gamma.dat and fall back to
 * identity gamma when the file was absent -- which quietly produced
 * under-corrected output for any out-of-tree CLI run. misc_read_gamma now
 * computes the same curves the file held, so there is nothing to miss and no
 * fallback to be wrong about. */
static void cli_load_gamma_with_fallback(void)
{
   misc_read_gamma();
}

static int compose_runtime_init(void)
{
   cli_alloc_global_buffers();
   /* Gamma correction has to be loaded BEFORE ds1edit_load_palettes
    * because palette_d2_to_rgba feeds every palette byte through
    * gamma_table[cur_gamma]. Without this the table is all zeros and
    * every D2 palette index resolves to RGB(0,0,0) -- compose output
    * would be a pure-black silhouette instead of the actual sprite.
    *
    * cur_gamma defaults to GC_130 (1.30) -- matches the editor's
    * default and what the GUI flow uses when ini_read sets it from
    * the [gamma_correction] key. */
   if (glb_ds1edit.cur_gamma == 0 && glb_config.gamma == 0)
      glb_ds1edit.cur_gamma = GC_130;
   else if (glb_ds1edit.cur_gamma == 0)
      glb_ds1edit.cur_gamma = glb_config.gamma;
   cli_load_gamma_with_fallback();
   ds1edit_load_palettes();
   a5_current_palette = &glb_ds1edit.vga_pal[0];
   /* compose_index is needed for monsters / NPCs / objects; idempotent
    * for player-chars-only runs. */
   (void) compose_index_build();
   /* MonStats2 supplies the per-monster sprite info (BaseW, per-layer
    * skin variants, layer-used flags). Without this the COF probe and
    * DCC paths for monsters point at files that don't exist. */
   (void) monstats2_build();
   /* Levels.txt drives per-monster Act resolution so each composed
    * sprite renders against the right act palette. Optional -- the
    * compose pipeline falls back to Act 1 when this isn't built. */
   (void) compose_palette_index_build();
   return CLI_EXIT_OK;
}

typedef struct EXPORT_RUN_S
{
   int success;
   int failure;
   int skipped;
   int verbose;
   int scale;          /* 1, 2, or 4 */
   const char *upscale_method;  /* NULL = realesrgan; or ultrasharp /
                                   nmkd-superscale / anime-6b /
                                   scale2x / nn */
   const char *out_root;
   char first_failure[256];
} EXPORT_RUN_S;

/* Iterate the (mode, weapon, direction) sub-cube for one (category,
 * token) pair. Modes/weapons drive the COF path; direction drives the
 * APNG. The mode/weapon lists are TUPLE_LIST_S so a count==0 +
 * use_all=1 falls back to the hardcoded compose_iter defaults. */
static void run_one_token(COMPOSE_CATEGORY_E category,
                          const char *token,
                          const char *token_name,
                          const TUPLE_LIST_S *modes,
                          const TUPLE_LIST_S *weapons,
                          const DIR_LIST_S *dirs,
                          EXPORT_RUN_S *r)
{
   int n_modes = modes->use_all
                 ? compose_iter_default_mode_count()
                 : modes->count;
   int n_weapons;
   const char *base = compose_iter_category_base(category);
   const char *skin = compose_iter_category_skin(category);
   COMPOSE_RENDER_PARAMS_S p;
   char path_buf[1024];
   char dir_buf[1024];
   char resolved_wclass_buf[16];
   int m, w;
   /* Per-token palette: monsters render against their natural act's
    * palette (per the locked Q10 + v2 follow-up). All other categories
    * default to Act 1. The save/restore wraps the inner loop so
    * palette state never leaks to the next token. */
   RGBA_PALETTE *saved_palette = a5_current_palette;
   {
      int act = compose_palette_resolve_act(category, token);
      if (act < 1 || act > ACT_MAX) act = 1;
      a5_current_palette = &glb_ds1edit.vga_pal[act - 1];
   }

   if (base == NULL) { a5_current_palette = saved_palette; return; }

   /* Player chars iterate the weapon list; everything else has a
    * single empty weapon class. */
   if (category == COMPOSE_CATEGORY_PLAYER_CHAR)
      n_weapons = weapons->use_all
                  ? compose_iter_default_weapon_count()
                  : weapons->count;
   else
      n_weapons = 1;

   for (m = 0; m < n_modes; m++)
   {
      const char *mode = modes->use_all
                         ? compose_iter_default_mode_at(m)
                         : modes->codes[m];
      if (mode == NULL || mode[0] == 0) continue;

      for (w = 0; w < n_weapons; w++)
      {
         const char *wclass;
         int dir_count, d, di;

         if (category == COMPOSE_CATEGORY_PLAYER_CHAR)
            wclass = weapons->use_all
                     ? compose_iter_default_weapon_at(w)
                     : weapons->codes[w];
         else
            wclass = "";
         if (wclass == NULL) wclass = "";

         dir_count = compose_iter_probe_direction_count_resolve(
            category, token, mode, wclass,
            resolved_wclass_buf, (int) sizeof(resolved_wclass_buf));
         if (dir_count <= 0)
         {
            r->skipped++;
            if (r->verbose >= 2)
               printf("skip: %s %s %s (no COF)\n",
                      token, mode, wclass[0] ? wclass : "-");
            continue;
         }
         /* Use the resolved wclass for the actual export so the COF
          * + DCC path resolution inside compose_render is consistent
          * with the probe. */
         wclass = resolved_wclass_buf;

         if (compose_iter_build_output_dir(dir_buf, (int) sizeof(dir_buf),
                                           r->out_root, category, token,
                                           token_name))
            compose_iter_ensure_dir(dir_buf);

         memset(&p, 0, sizeof(p));
         p.base   = base;
         p.token  = token;
         p.mode   = mode;
         p.wclass = wclass;
         p.skin   = skin;

         /* For monsters / NPCs, MonStats2 has per-layer skin variants.
          * Look them up via the compose_index's stored MonStatsEx. */
         if (category == COMPOSE_CATEGORY_MONSTER
             || category == COMPOSE_CATEGORY_NPC)
         {
            const COMPOSE_TOKEN_S *(*at)(int) =
               (category == COMPOSE_CATEGORY_MONSTER)
                  ? compose_index_monster_at
                  : compose_index_npc_at;
            int n = (category == COMPOSE_CATEGORY_MONSTER)
                       ? compose_index_monster_count()
                       : compose_index_npc_count();
            int ti;
            for (ti = 0; ti < n; ti++)
            {
               const COMPOSE_TOKEN_S *t = at(ti);
               const MONSTATS2_ENTRY_S *e;
               int li;
               if (t == NULL) continue;
               if (strcasecmp(t->code, token) != 0) continue;
               if (t->mon_stats_ex[0] == 0) break;
               e = monstats2_find(t->mon_stats_ex);
               if (e == NULL) break;
               for (li = 0; li < COMPOSE_RENDER_LAYER_COUNT
                        && li < MONSTATS2_LAYER_COUNT; li++)
               {
                  if (e->layers[li].used && e->layers[li].skin[0] != 0)
                     strncpy(p.skin_per_layer[li], e->layers[li].skin,
                             COMPOSE_RENDER_SKIN_MAX - 1);
               }
               break;
            }
         }

         /* Iterate either an explicit dir list or the full set the
          * COF advertises. */
         {
            int loop_count = dirs->use_all ? dir_count : dirs->count;
            for (di = 0; di < loop_count; di++)
            {
               int ok;
               d = dirs->use_all ? di : dirs->dirs[di];
               if (d < 0 || d >= dir_count)
               {
                  r->skipped++;
                  if (r->verbose >= 2)
                     printf("skip: %s %s %s dir %d (out of range, COF has %d)\n",
                            token, mode, wclass[0] ? wclass : "-",
                            d, dir_count);
                  continue;
               }

               p.direction = d;
               if (!compose_iter_build_output_path(
                      path_buf, (int) sizeof(path_buf),
                      r->out_root, category, token, token_name,
                      mode, wclass, d))
               {
                  r->failure++;
                  fprintf(stderr,
                     "fail: %s %s %s dir %d (output path build)\n",
                     token, mode, wclass[0] ? wclass : "-", d);
                  continue;
               }

               ok = compose_apng_export_method(&p, path_buf,
                                              r->scale > 0 ? r->scale : 1,
                                              r->upscale_method);
               if (ok)
               {
                  r->success++;
                  if (r->verbose >= 1)
                     printf("ok: %s\n", path_buf);
               }
               else
               {
                  r->failure++;
                  fprintf(stderr,
                     "fail: %s %s %s dir %d -> %s\n",
                     token, mode, wclass[0] ? wclass : "-", d, path_buf);
                  if (r->first_failure[0] == 0)
                  {
                     strncpy(r->first_failure, path_buf,
                             sizeof(r->first_failure) - 1);
                     r->first_failure[sizeof(r->first_failure) - 1] = 0;
                  }
               }
            }
         }
      }
   }
   /* Restore the saved palette so the next token starts from a clean
    * baseline rather than inheriting this token's act. */
   a5_current_palette = saved_palette;
}

static int verb_export_compose(int argc, char **argv)
{
   CLI_COMMON_OPTS_S opts;
   const char *target_str;
   const char *category_str;
   const char *token_str;
   const char *mode_str;
   const char *weapon_str;
   const char *dir_str;
   const char *out_str;
   int rc;
   COMPOSE_CATEGORY_E category = COMPOSE_CATEGORY_NONE;
   TUPLE_LIST_S modes, weapons;
   DIR_LIST_S  dirs;
   EXPORT_RUN_S run;

   if (!parse_common_opts(argc, argv, &opts))
      return CLI_EXIT_BAD_ARGS;

   target_str   = find_flag_value(argc, argv, "target");
   category_str = find_flag_value(argc, argv, "category");
   token_str    = find_flag_value(argc, argv, "token");
   mode_str     = find_flag_value(argc, argv, "mode");
   weapon_str   = find_flag_value(argc, argv, "weapon");
   dir_str      = find_flag_value(argc, argv, "direction");
   out_str      = find_flag_value(argc, argv, "out");

   /* Init config + MPQs first so glb_config.export_default_* are
    * available below for fallbacks. */
   rc = cli_minimum_init(&opts);
   if (rc != CLI_EXIT_OK) return rc;
   compose_runtime_init();

   /* --out= falls back to [export_defaults] compose_output. */
   if ((out_str == NULL || out_str[0] == 0)
       && glb_config.export_default_compose_output != NULL
       && glb_config.export_default_compose_output[0] != 0)
   {
      out_str = glb_config.export_default_compose_output;
   }
   if (out_str == NULL || out_str[0] == 0)
   {
      fprintf(stderr,
         "ds1edit export-compose: no output dir.\n"
         "Pass --out=<dir> or set [export_defaults] compose_output in "
         "Ds1edit.ini.\n");
      return CLI_EXIT_BAD_ARGS;
   }

   /* --mode / --weapon fall back to named presets. */
   if (mode_str == NULL || mode_str[0] == 0)
   {
      const char *preset_name = glb_config.export_default_compose_modes_preset;
      if (preset_name != NULL && preset_name[0] != 0)
      {
         int i;
         for (i = 0; i < compose_mode_presets_count(); i++)
         {
            const COMPOSE_PRESET_S *p = compose_mode_presets_at(i);
            if (p != NULL && strcasecmp(p->name, preset_name) == 0)
            {
               static char joined[512];
               int j, off = 0;
               joined[0] = 0;
               for (j = 0; j < p->code_count && off < (int) sizeof(joined) - 1; j++)
               {
                  int n = snprintf(joined + off, sizeof(joined) - (size_t) off,
                                   "%s%s", j == 0 ? "" : ",", p->codes[j]);
                  if (n < 0) break;
                  off += n;
               }
               mode_str = joined;
               break;
            }
         }
      }
   }
   if (weapon_str == NULL || weapon_str[0] == 0)
   {
      const char *preset_name = glb_config.export_default_compose_weapons_preset;
      if (preset_name != NULL && preset_name[0] != 0)
      {
         int i;
         for (i = 0; i < compose_weapon_presets_count(); i++)
         {
            const COMPOSE_PRESET_S *p = compose_weapon_presets_at(i);
            if (p != NULL && strcasecmp(p->name, preset_name) == 0)
            {
               static char joined[512];
               int j, off = 0;
               joined[0] = 0;
               for (j = 0; j < p->code_count && off < (int) sizeof(joined) - 1; j++)
               {
                  int n = snprintf(joined + off, sizeof(joined) - (size_t) off,
                                   "%s%s", j == 0 ? "" : ",", p->codes[j]);
                  if (n < 0) break;
                  off += n;
               }
               weapon_str = joined;
               break;
            }
         }
      }
   }

   /* Default verbosity from config. CLI -v / -vv flags still escalate. */
   if (opts.verbose == 0 && glb_config.export_default_verbose)
      opts.verbose = 1;

   /* Resolve category. --category= wins; otherwise inferred from
    * --target= or --token=. */
   if (category_str != NULL)
   {
      category = parse_category(category_str);
      if (category == COMPOSE_CATEGORY_NONE)
      {
         fprintf(stderr,
            "ds1edit export-compose: unknown category '%s'\n", category_str);
         return CLI_EXIT_BAD_ARGS;
      }
   }

   memset(&modes,   0, sizeof(modes));
   memset(&weapons, 0, sizeof(weapons));
   memset(&dirs,    0, sizeof(dirs));
   memset(&run,     0, sizeof(run));
   run.verbose = opts.verbose;
   run.out_root = out_str;
   run.scale = 1;
   run.upscale_method = find_flag_value(argc, argv, "upscale-method");
   if (run.upscale_method != NULL
       && strcasecmp(run.upscale_method, "realesrgan")      != 0
       && strcasecmp(run.upscale_method, "ultrasharp")      != 0
       && strcasecmp(run.upscale_method, "nmkd-superscale") != 0
       && strcasecmp(run.upscale_method, "anime-6b")        != 0
       && strcasecmp(run.upscale_method, "apisr")           != 0
       && strcasecmp(run.upscale_method, "sd-x4")           != 0
       && strcasecmp(run.upscale_method, "sdxl-refine")     != 0
       && strcasecmp(run.upscale_method, "bilinear")        != 0
       && strcasecmp(run.upscale_method, "bicubic")         != 0
       && strcasecmp(run.upscale_method, "lanczos")         != 0
       && strcasecmp(run.upscale_method, "xbrz")            != 0
       && strcasecmp(run.upscale_method, "scale2x")         != 0
       && strcasecmp(run.upscale_method, "nn")              != 0)
   {
      fprintf(stderr,
         "ds1edit export-compose: --upscale-method=%s not recognised "
         "(expected realesrgan / ultrasharp / nmkd-superscale / "
         "anime-6b / apisr / sd-x4 / sdxl-refine / bilinear / "
         "bicubic / lanczos / xbrz / scale2x / nn)\n",
         run.upscale_method);
      return CLI_EXIT_BAD_ARGS;
   }
   {
      const char *scale_str = find_flag_value(argc, argv, "upscale");
      if (scale_str != NULL)
      {
         /* Accept 1 / 2 / 4 (and "none" / "2x" / "4x"). */
         if (strcasecmp(scale_str, "none") == 0
             || strcasecmp(scale_str, "1") == 0
             || strcasecmp(scale_str, "1x") == 0)
            run.scale = 1;
         else if (strcasecmp(scale_str, "2") == 0
                  || strcasecmp(scale_str, "2x") == 0)
            run.scale = 2;
         else if (strcasecmp(scale_str, "4") == 0
                  || strcasecmp(scale_str, "4x") == 0)
            run.scale = 4;
         else
         {
            fprintf(stderr,
               "ds1edit export-compose: --upscale=%s expected 1 / 2 / 4 "
               "(or 'none' / '2x' / '4x')\n", scale_str);
            return CLI_EXIT_BAD_ARGS;
         }
      }
   }

   /* --target=A/B[/C] short form -- splits into token + mode + weapon.
    * Locks the relevant axes to count=1, ignoring --token / --mode /
    * --weapon when --target is present. */
   if (target_str != NULL && target_str[0] != 0)
   {
      char tok_buf[16], mode_buf[16], wclass_buf[16];
      char full_name[64];
      const char *p1, *p2;
      int n;

      p1 = strchr(target_str, '/');
      if (p1 == NULL)
      {
         fprintf(stderr,
            "ds1edit export-compose: --target=%s expected form "
            "TOKEN/MODE[/WEAPON]\n", target_str);
         return CLI_EXIT_BAD_ARGS;
      }
      n = (int) (p1 - target_str);
      if (n <= 0 || n >= (int) sizeof(tok_buf)) return CLI_EXIT_BAD_ARGS;
      memcpy(tok_buf, target_str, (size_t) n);
      tok_buf[n] = 0;

      p2 = strchr(p1 + 1, '/');
      if (p2 == NULL)
      {
         strncpy(mode_buf, p1 + 1, sizeof(mode_buf) - 1);
         mode_buf[sizeof(mode_buf) - 1] = 0;
         wclass_buf[0] = 0;
      }
      else
      {
         n = (int) (p2 - (p1 + 1));
         if (n < 0 || n >= (int) sizeof(mode_buf)) return CLI_EXIT_BAD_ARGS;
         memcpy(mode_buf, p1 + 1, (size_t) n);
         mode_buf[n] = 0;
         strncpy(wclass_buf, p2 + 1, sizeof(wclass_buf) - 1);
         wclass_buf[sizeof(wclass_buf) - 1] = 0;
         if (strcmp(wclass_buf, "-") == 0) wclass_buf[0] = 0;
      }

      if (category == COMPOSE_CATEGORY_NONE)
      {
         if (!resolve_target_token_category(tok_buf, &category,
                                            full_name, sizeof(full_name)))
         {
            fprintf(stderr,
               "ds1edit export-compose: cannot resolve category for token "
               "'%s'. Pass --category=<chars|monsters|npcs|objects>.\n",
               tok_buf);
            return CLI_EXIT_BAD_ARGS;
         }
      }
      else
         full_name[0] = 0;

      /* Build single-element selector lists. */
      modes.count = 1;
      strncpy(modes.codes[0], mode_buf, sizeof(modes.codes[0]) - 1);
      weapons.count = 1;
      strncpy(weapons.codes[0], wclass_buf, sizeof(weapons.codes[0]) - 1);

      if (!parse_dir_list(dir_str, &dirs))
      {
         fprintf(stderr,
            "ds1edit export-compose: --direction=%s could not be parsed\n",
            dir_str);
         return CLI_EXIT_BAD_ARGS;
      }

      run_one_token(category, tok_buf, full_name,
                    &modes, &weapons, &dirs, &run);
   }
   else
   {
      /* Bulk mode: no --target. Walk the categories selected (default
       * all four), apply --token/--mode/--weapon filters. */
      if (!parse_tuple_list(mode_str,   &modes))   modes.use_all   = 1;
      if (!parse_tuple_list(weapon_str, &weapons)) weapons.use_all = 1;
      if (!parse_dir_list(dir_str,      &dirs))    dirs.use_all    = 1;

      {
         COMPOSE_CATEGORY_E cats[4];
         int n_cats, ci, ti, n_tokens;

         if (category == COMPOSE_CATEGORY_NONE)
         {
            cats[0] = COMPOSE_CATEGORY_PLAYER_CHAR;
            cats[1] = COMPOSE_CATEGORY_MONSTER;
            cats[2] = COMPOSE_CATEGORY_NPC;
            cats[3] = COMPOSE_CATEGORY_OBJECT;
            n_cats = 4;
         }
         else
         {
            cats[0] = category;
            n_cats = 1;
         }

         for (ci = 0; ci < n_cats; ci++)
         {
            COMPOSE_CATEGORY_E cat = cats[ci];
            n_tokens = (cat == COMPOSE_CATEGORY_PLAYER_CHAR)
                       ? compose_iter_player_class_count()
                       : (cat == COMPOSE_CATEGORY_MONSTER) ? compose_index_monster_count()
                       : (cat == COMPOSE_CATEGORY_NPC)     ? compose_index_npc_count()
                       :                                     compose_index_object_count();
            for (ti = 0; ti < n_tokens; ti++)
            {
               const char *tok_code; const char *tok_name;
               if (cat == COMPOSE_CATEGORY_PLAYER_CHAR)
               {
                  tok_code = compose_iter_player_class_at(ti);
                  tok_name = compose_naming_class_name(tok_code);
                  if (tok_name == NULL) tok_name = "";
               }
               else
               {
                  const COMPOSE_TOKEN_S *t = (cat == COMPOSE_CATEGORY_MONSTER)
                                              ? compose_index_monster_at(ti)
                                              : (cat == COMPOSE_CATEGORY_NPC)
                                                ? compose_index_npc_at(ti)
                                                : compose_index_object_at(ti);
                  if (t == NULL) continue;
                  tok_code = t->code;
                  tok_name = t->name;
               }
               if (tok_code == NULL || tok_code[0] == 0) continue;

               /* --token= is a per-token filter (case-insensitive). */
               if (token_str != NULL && token_str[0] != 0
                   && strcasecmp(tok_code, token_str) != 0)
                  continue;

               run_one_token(cat, tok_code, tok_name,
                             &modes, &weapons, &dirs, &run);
            }
         }
      }
   }

   /* Summary line + exit code. */
   printf("\n");
   printf("exported=%d failed=%d skipped=%d  out=%s\n",
          run.success, run.failure, run.skipped, run.out_root);

   if (run.success > 0 && run.failure == 0)
      return CLI_EXIT_OK;
   if (run.success > 0 && run.failure > 0)
      return CLI_EXIT_PARTIAL;
   return CLI_EXIT_NOTHING;
}

/* ---- dump-txt-row (debug helper) ------------------------------------ */

/* Quick-and-dirty: load <virtual-path>, find the row whose first field
 * matches <id>, dump it column-by-column with the header names. Used
 * to discover D2 TXT schemas like MonStats2 without reaching for an
 * external editor. Returns the column count on success.
 *
 * Usage:
 *   ds1edit dump-txt-row data\global\excel\MonStats2.txt skeleton1
 */
static int verb_dump_txt_row(int argc, char **argv);

/* ---- export-raw ----------------------------------------------------- */

/* Resolve the [export_defaults] fallback for the raw-export output dir
 * given a type filter. Returns NULL if no default is set. */
static const char *raw_default_out_for_type(const char *type_filter)
{
   if (type_filter == NULL || type_filter[0] == 0) return NULL;
   if (strcasecmp(type_filter, "dcc") == 0)
      return glb_config.export_default_raw_dcc_output;
   if (strcasecmp(type_filter, "dc6") == 0)
      return glb_config.export_default_raw_dc6_output;
   if (strcasecmp(type_filter, "dt1") == 0)
      return glb_config.export_default_raw_dt1_output;
   /* "all" / "cof" / unknown have no default. */
   return NULL;
}

/* Look up an [export_presets] entry by name (case-insensitive). */
static const EXPORT_PRESET_S *find_export_preset(const char *name)
{
   int i;
   if (name == NULL || name[0] == 0) return NULL;
   for (i = 0; i < export_presets_count(); i++)
   {
      const EXPORT_PRESET_S *p = export_presets_at(i);
      if (p != NULL && strcasecmp(p->name, name) == 0)
         return p;
   }
   return NULL;
}

/* Run the same Real-ESRGAN remote upscale that the GUI raw export +
 * compose-mode use, but with CLI-friendly error reporting. The
 * staging dir already contains native-export PNGs (single files or a
 * tree, the upscale walks recursively). On success the upscaled PNGs
 * land in `dst_dir` mirroring the staging tree. */
static int cli_run_remote_upscale(const char *staging_dir,
                                  const char *dst_dir, int scale)
{
   char err[512];
   if (!upscale_is_remote_configured())
   {
      fprintf(stderr,
         "ds1edit export-raw: --upscale=%d requires the remote service. "
         "Set upscale_enabled = YES and upscale_service_url = <url> in "
         "Ds1edit.ini.\n", scale);
      return 0;
   }
   err[0] = 0;
   if (upscale_directory_remote(staging_dir, dst_dir, scale,
                                "realesrgan", err, sizeof(err)))
      return 1;
   fprintf(stderr,
      "ds1edit export-raw: remote upscale failed: %s\n",
      err[0] ? err : "no detail");
   return 0;
}

static int verb_export_raw(int argc, char **argv)
{
   CLI_COMMON_OPTS_S opts;
   const char *type_str;
   const char *target_str;
   const char *scope_str;
   const char *folder_str;
   const char *preset_str;
   const char *pattern_str;
   const char *out_str;
   const char *listfile_str;
   const char *upscale_str;
   int upscale_factor = 1;
   const char *export_dst;   /* final or staging dir, depending on upscale */
   char staging_dir[512];
   int rc;
   ASSET_EXPORT_PLAN_S plan;
   int planned_ok = 0;
   int exported = 0;

   if (!parse_common_opts(argc, argv, &opts))
      return CLI_EXIT_BAD_ARGS;

   type_str     = find_flag_value(argc, argv, "type");
   target_str   = find_flag_value(argc, argv, "target");
   scope_str    = find_flag_value(argc, argv, "scope");
   folder_str   = find_flag_value(argc, argv, "folder");
   preset_str   = find_flag_value(argc, argv, "preset");
   pattern_str  = find_flag_value(argc, argv, "pattern");
   out_str      = find_flag_value(argc, argv, "out");
   listfile_str = find_flag_value(argc, argv, "listfile");
   upscale_str  = find_flag_value(argc, argv, "upscale");

   if (upscale_str != NULL)
   {
      if (strcasecmp(upscale_str, "1") == 0
          || strcasecmp(upscale_str, "1x") == 0
          || strcasecmp(upscale_str, "none") == 0)
         upscale_factor = 1;
      else if (strcasecmp(upscale_str, "2") == 0
               || strcasecmp(upscale_str, "2x") == 0)
         upscale_factor = 2;
      else if (strcasecmp(upscale_str, "4") == 0
               || strcasecmp(upscale_str, "4x") == 0)
         upscale_factor = 4;
      else
      {
         fprintf(stderr,
            "ds1edit export-raw: --upscale=%s expected 1 / 2 / 4\n",
            upscale_str);
         return CLI_EXIT_BAD_ARGS;
      }
   }
   staging_dir[0] = 0;

   /* Mutually-exclusive selectors: --target / --preset / --pattern /
    * --scope drive different code paths. We allow exactly one; if the
    * user passes none, --type+--scope=all is the implicit fallback. */
   {
      int n_selectors = 0;
      if (target_str  != NULL) n_selectors++;
      if (preset_str  != NULL) n_selectors++;
      if (pattern_str != NULL) n_selectors++;
      if (scope_str   != NULL) n_selectors++;
      if (n_selectors > 1)
      {
         fprintf(stderr,
            "ds1edit export-raw: --target / --preset / --pattern / --scope "
            "are mutually exclusive (got %d).\n", n_selectors);
         return CLI_EXIT_BAD_ARGS;
      }
   }

   rc = cli_minimum_init(&opts);
   if (rc != CLI_EXIT_OK) return rc;
   /* compose_runtime_init's palette load is needed here too -- the raw
    * exporter calls into the palette path for color decoding. */
   compose_runtime_init();

   /* Output dir resolution:
    *   1. --out= wins
    *   2. preset's type drives raw_<type>_output fallback
    *   3. --type= drives raw_<type>_output fallback
    * If both come back empty, fail. */
   if ((out_str == NULL || out_str[0] == 0) && preset_str != NULL)
   {
      const EXPORT_PRESET_S *p = find_export_preset(preset_str);
      if (p != NULL)
         out_str = raw_default_out_for_type(p->type);
   }
   if ((out_str == NULL || out_str[0] == 0) && type_str != NULL)
      out_str = raw_default_out_for_type(type_str);
   if (out_str == NULL || out_str[0] == 0)
   {
      fprintf(stderr,
         "ds1edit export-raw: no output dir.\n"
         "Pass --out=<dir> or set [export_defaults] raw_<type>_output in "
         "Ds1edit.ini.\n");
      return CLI_EXIT_BAD_ARGS;
   }

   /* When upscaling, native PNGs go into a temp staging dir first;
    * the remote service then reads them, upscales each, and writes
    * the upscaled tree to out_str. Without upscale, native export
    * writes directly to out_str. */
   if (upscale_factor != 1)
   {
      if (!upscale_create_temp_dir(staging_dir, sizeof(staging_dir)))
      {
         fprintf(stderr,
            "ds1edit export-raw: failed to create staging dir for upscale.\n");
         return CLI_EXIT_BAD_ARGS;
      }
      export_dst = staging_dir;
   }
   else
   {
      export_dst = out_str;
   }

   /* Fast path: --target= without any glob metacharacters is an exact
    * file path. */
   if (target_str != NULL && target_str[0] != 0
       && strpbrk(target_str, "*?") == NULL)
   {
      int single_ok;
      compose_iter_ensure_dir(export_dst);
      if (opts.verbose >= 1)
         printf("export (single): %s -> %s%s\n", target_str, export_dst,
                upscale_factor != 1 ? " (then upscale)" : "");
      single_ok = (asset_export_png(target_str, export_dst) > 0);
      if (single_ok && upscale_factor != 1)
         single_ok = cli_run_remote_upscale(staging_dir, out_str, upscale_factor);
      if (staging_dir[0] != 0) upscale_remove_tree(staging_dir);
      if (single_ok)
      {
         printf("\nexported=1 failed=0 candidates=1  out=%s\n", out_str);
         return CLI_EXIT_OK;
      }
      printf("\nexported=0 failed=1 candidates=1  out=%s\n", out_str);
      return CLI_EXIT_PARTIAL;
   }

   /* Bulk paths need the MPQ filename_table populated before they can
    * enumerate. The in-MPQ (listfile) decompression has a known bug
    * for D2 MPQs, so we accept an external listfile via --listfile=
    * or the [export_defaults] listfile= config key. Single-file
    * fast-path above doesn't need this. */
   {
      const char *lf = listfile_str;
      if (lf == NULL || lf[0] == 0)
         lf = glb_config.export_default_listfile;
      if (lf != NULL && lf[0] != 0)
      {
         int seeded = asset_export_seed_listfile_from_file(lf);
         if (opts.verbose >= 1)
            printf("listfile: %d path(s) seeded from %s\n", seeded, lf);
      }
   }

   asset_export_plan_init(&plan);

   /* Plan construction. Order of preference matches the mutually-
    * exclusive validation above. */
   if (preset_str != NULL && preset_str[0] != 0)
   {
      const EXPORT_PRESET_S *p = find_export_preset(preset_str);
      if (p == NULL)
      {
         fprintf(stderr,
            "ds1edit export-raw: --preset=%s not found in [export_presets]\n",
            preset_str);
         return CLI_EXIT_BAD_ARGS;
      }
      planned_ok = asset_export_plan_for_pattern(p->pattern, &plan);
   }
   else if (target_str != NULL && target_str[0] != 0)
   {
      planned_ok = asset_export_plan_for_pattern(target_str, &plan);
   }
   else if (pattern_str != NULL && pattern_str[0] != 0)
   {
      planned_ok = asset_export_plan_for_pattern(pattern_str, &plan);
   }
   else
   {
      const char *scope = (scope_str != NULL && scope_str[0] != 0)
                          ? scope_str : "all";
      const char *type  = (type_str != NULL && type_str[0] != 0)
                          ? type_str : "all";

      if (strcasecmp(scope, "all") == 0)
      {
         /* "all" maps to prefix "Data" -- matches asset_export_all_png,
          * the existing GUI's "everything" path. An empty prefix
          * fails asset_path_matches_prefix (which rejects empty by
          * design) and matches nothing. */
         planned_ok = asset_export_plan_for_prefix("Data", type, &plan);
      }
      else if (strcasecmp(scope, "folder") == 0)
      {
         if (folder_str == NULL || folder_str[0] == 0)
         {
            fprintf(stderr,
               "ds1edit export-raw: --scope=folder requires --folder=<prefix>\n");
            asset_export_plan_free(&plan);
            return CLI_EXIT_BAD_ARGS;
         }
         planned_ok = asset_export_plan_for_prefix(folder_str, type, &plan);
      }
      else if (strcasecmp(scope, "area") == 0)
      {
         /* Area-scope export needs a loaded DS1 + the area browser's
          * group resolution. That machinery isn't initialised in CLI
          * mode, and threading it through is out of scope for this
          * phase. Document and reject for now. */
         fprintf(stderr,
            "ds1edit export-raw: --scope=area is GUI-only for now. "
            "Use --target=<glob> or --scope=folder to drive the same "
            "subset from the cmdline.\n");
         asset_export_plan_free(&plan);
         return CLI_EXIT_BAD_ARGS;
      }
      else
      {
         fprintf(stderr,
            "ds1edit export-raw: unknown --scope=%s "
            "(expected all / folder / area)\n", scope);
         asset_export_plan_free(&plan);
         return CLI_EXIT_BAD_ARGS;
      }
   }

   if (!planned_ok)
   {
      fprintf(stderr, "ds1edit export-raw: plan construction failed\n");
      asset_export_plan_free(&plan);
      return CLI_EXIT_BAD_ARGS;
   }

   if (opts.verbose >= 1)
      printf("plan: %d candidate(s) (after filter: %d to export)\n",
             plan.total_candidates, plan.count);

   if (plan.count <= 0)
   {
      printf("\nexported=0 failed=0 candidates=%d  out=%s\n",
             plan.total_candidates, out_str);
      asset_export_plan_free(&plan);
      return CLI_EXIT_NOTHING;
   }

   /* mkdir the destination once before writing (staging or final). */
   compose_iter_ensure_dir(export_dst);

   exported = asset_export_run_plan(&plan, export_dst);

   /* Upscale stage: pack the staging tree, POST to the remote
    * Real-ESRGAN service, unpack into out_str. Skip cleanly if the
    * native pass produced nothing. */
   if (upscale_factor != 1 && exported > 0)
   {
      compose_iter_ensure_dir(out_str);
      if (!cli_run_remote_upscale(staging_dir, out_str, upscale_factor))
      {
         /* Failure already reported on stderr. Treat the run as
          * partial: the native PNGs are in the staging dir, just
          * not upscaled. We don't auto-rescue them; the user gets
          * a clean failure mode. */
         upscale_remove_tree(staging_dir);
         asset_export_plan_free(&plan);
         printf("\nexported=0 failed=%d candidates=%d  out=%s\n",
                plan.count, plan.total_candidates, out_str);
         return CLI_EXIT_PARTIAL;
      }
   }
   if (staging_dir[0] != 0) upscale_remove_tree(staging_dir);

   {
      int failed = plan.count - exported;
      if (failed < 0) failed = 0;
      printf("\nexported=%d failed=%d candidates=%d  out=%s\n",
             exported, failed, plan.total_candidates, out_str);

      asset_export_plan_free(&plan);

      if (exported > 0 && failed == 0) return CLI_EXIT_OK;
      if (exported > 0)                return CLI_EXIT_PARTIAL;
      return CLI_EXIT_NOTHING;
   }
}

/* ---- dump-txt-row ---------------------------------------------------- */

/* Walk a tab-separated TXT buffer to the line whose first field
 * (case-insensitive) matches `id`. Header line is line 0. Returns the
 * matched line into out_line (capped) and the header line into
 * out_header (capped). Returns 1 on match. */
static int find_txt_row_by_id(const char *txt, long len, const char *id,
                              char *out_header, int hdr_cap,
                              char *out_line, int line_cap)
{
   long p = 0;
   int line_no = 0;
   int matched = 0;
   if (out_header != NULL && hdr_cap > 0) out_header[0] = 0;
   if (out_line != NULL && line_cap > 0) out_line[0] = 0;

   while (p < len)
   {
      long start = p;
      char field0[64];
      int f0n = 0;
      while (p < len && txt[p] != '\n' && txt[p] != '\r') p++;
      {
         long k = start;
         while (k < p && txt[k] != '\t' && f0n < (int) sizeof(field0) - 1)
            field0[f0n++] = txt[k++];
         field0[f0n] = 0;
      }
      if (line_no == 0 && out_header != NULL)
      {
         long n = (p - start) < (long) hdr_cap - 1 ? (p - start) : (long) hdr_cap - 1;
         memcpy(out_header, txt + start, (size_t) n);
         out_header[n] = 0;
      }
      if (line_no > 0 && strcasecmp(field0, id) == 0)
      {
         long n = (p - start) < (long) line_cap - 1 ? (p - start) : (long) line_cap - 1;
         memcpy(out_line, txt + start, (size_t) n);
         out_line[n] = 0;
         matched = 1;
         break;
      }
      while (p < len && (txt[p] == '\n' || txt[p] == '\r')) p++;
      line_no++;
   }
   return matched;
}

static int verb_dump_txt_row(int argc, char **argv)
{
   CLI_COMMON_OPTS_S opts;
   const char *positional[3];
   int n_pos;
   const char *txt_path;
   const char *row_id;
   char *buf = NULL;
   long buf_len = 0;
   char header[8192];
   char line  [8192];
   int rc;
   int field_idx = 0;
   long h = 0;
   long l = 0;

   if (!parse_common_opts(argc, argv, &opts))
      return CLI_EXIT_BAD_ARGS;

   n_pos = collect_positional(argc, argv, positional, 3);
   if (n_pos < 2)
   {
      fprintf(stderr,
         "ds1edit dump-txt-row: too few arguments\n"
         "Usage: ds1edit dump-txt-row <virtual-txt-path> <row-id>\n"
         "Example: ds1edit dump-txt-row data\\global\\excel\\MonStats2.txt skeleton1\n");
      return CLI_EXIT_BAD_ARGS;
   }
   txt_path = positional[0];
   row_id   = positional[1];

   rc = cli_minimum_init(&opts);
   if (rc != CLI_EXIT_OK) return rc;

   if (misc_load_mpq_file((char *) txt_path, &buf, &buf_len, 0) == -1
       || buf == NULL)
   {
      fprintf(stderr, "ds1edit dump-txt-row: %s not in chain\n", txt_path);
      if (buf != NULL) free(buf);
      return CLI_EXIT_NOTHING;
   }

   if (!find_txt_row_by_id(buf, buf_len, row_id,
                           header, (int) sizeof(header),
                           line,   (int) sizeof(line)))
   {
      /* Show the header anyway so the caller can re-issue with a
       * known-good first-column id. The "Expansion" sentinel is a
       * common waypoint -- pass it as <row-id> to find a row that
       * exists. */
      fprintf(stderr,
         "ds1edit dump-txt-row: no row in %s with first-column id <%s>\n",
         txt_path, row_id);
      if (header[0] != 0)
      {
         fprintf(stderr, "Header: %s\n", header);
      }
      free(buf);
      return CLI_EXIT_NOTHING;
   }

   /* Walk header + line in lockstep, printing "column = value" for
    * each field. */
   while (header[h] != 0 || line[l] != 0)
   {
      char hbuf[128], lbuf[128];
      int hi = 0, li = 0;
      while (header[h] != 0 && header[h] != '\t' && hi < (int) sizeof(hbuf) - 1)
         hbuf[hi++] = header[h++];
      hbuf[hi] = 0;
      if (header[h] == '\t') h++;

      while (line[l] != 0 && line[l] != '\t' && li < (int) sizeof(lbuf) - 1)
         lbuf[li++] = line[l++];
      lbuf[li] = 0;
      if (line[l] == '\t') l++;

      if (hbuf[0] != 0 || lbuf[0] != 0)
         printf("  [%3d] %-24s = %s\n", field_idx, hbuf, lbuf);
      field_idx++;
      if (hbuf[0] == 0 && lbuf[0] == 0) break;
   }

   free(buf);
   return CLI_EXIT_OK;
}

/* ---- dump-listfile --------------------------------------------------- */

static int verb_dump_listfile(int argc, char **argv)
{
   CLI_COMMON_OPTS_S opts;
   int rc, slot;
   GLB_MPQ_S *saved_mpq;
   extern GLB_MPQ_S *glb_mpq;
   extern int mpq_batch_load_in_mem(char *filename, void **buffer,
                                    long *buf_len, int output);

   if (!parse_common_opts(argc, argv, &opts))
      return CLI_EXIT_BAD_ARGS;

   rc = cli_minimum_init(&opts);
   if (rc != CLI_EXIT_OK) return rc;

   saved_mpq = glb_mpq;
   for (slot = 0; slot < MAX_MPQ_FILE; slot++)
   {
      void *buf = NULL;
      long buf_len = 0;
      int n;

      if (!glb_mpq_struct[slot].is_open) continue;
      glb_mpq = &glb_mpq_struct[slot];
      n = mpq_batch_load_in_mem("(listfile)", &buf, &buf_len, 0);
      if (n == -1 || buf == NULL || buf_len <= 0)
      {
         fprintf(stderr, "slot %d (%s): no listfile\n",
                 slot, slot_label(slot));
         if (buf != NULL) free(buf);
         continue;
      }
      printf(";;; ===== slot %d (%s) -- %ld bytes =====\n",
             slot, slot_label(slot), buf_len);
      fwrite(buf, 1, (size_t) buf_len, stdout);
      printf("\n");
      free(buf);
   }
   glb_mpq = saved_mpq;
   return CLI_EXIT_OK;
}

/* ---- area browser verbs ---------------------------------------------- */

/* Bare presence of "--name" (no "=value"), which find_flag_value cannot see. */
static int cli_has_flag(int argc, char **argv, const char *name)
{
   int i;
   for (i = 2; i < argc; i++)
   {
      const char *a = argv[i];
      if (a == NULL) continue;
      while (*a == '-') a++;
      if (strcasecmp(a, name) == 0) return 1;
   }
   return 0;
}

/* First argument after the verb that is not a flag. NULL if there is none. */
static const char *cli_first_positional(int argc, char **argv)
{
   int i;
   for (i = 2; i < argc; i++)
   {
      if (argv[i] != NULL && argv[i][0] != '-')
         return argv[i];
   }
   return NULL;
}

/* Shared prologue: config + MPQ chain, then the LvlTypes/LvlPrest/Levels
 * join the browser is built from. */
static int cli_area_browser_ready(int argc, char **argv)
{
   CLI_COMMON_OPTS_S opts;
   int rc;

   if (!parse_common_opts(argc, argv, &opts))
      return CLI_EXIT_BAD_ARGS;

   rc = cli_minimum_init(&opts);
   if (rc != CLI_EXIT_OK) return rc;

   /* cli_minimum_init deliberately skips the DS1/DT1 arenas, but the browser
    * reads glb_ds1[] while resolving each area's palette -- without these it
    * dereferences NULL partway through the LvlTypes join. */
   cli_alloc_global_buffers();

   /* txt_load() indexes glb_txt_req_ptr[] to find each table's required
    * columns; ds1edit_init() normally wires it and the CLI skips that. */
   ds1edit_init_txt_requirements();

   if (area_browser_init() != 0)
   {
      fprintf(stderr,
              "ds1edit: failed to load the Excel tables the area browser "
              "needs (Levels.txt / LvlPrest.txt / LvlTypes.txt).\n");
      return CLI_EXIT_NOTHING;
   }
   return CLI_EXIT_OK;
}

static int verb_list_areas(int argc, char **argv)
{
   int rc = cli_area_browser_ready(argc, argv);
   if (rc != CLI_EXIT_OK) return rc;

   /* Either "list-areas --ext" or the legacy "--list-areas-ext" spelling,
    * which arrives as argv[1] rather than as a flag. */
   if (cli_has_flag(argc, argv, "ext")
       || (argc > 1 && argv[1] != NULL && strstr(argv[1], "list-areas-ext") != NULL))
      area_browser_list_ext();
   else
      area_browser_list();
   return CLI_EXIT_OK;
}

static int verb_list_files(int argc, char **argv)
{
   const char *filter;
   int rc = cli_area_browser_ready(argc, argv);
   if (rc != CLI_EXIT_OK) return rc;

   /* Positional substring filter, matching the old --list-files <filter>. */
   filter = cli_first_positional(argc, argv);
   area_browser_list_files(filter);
   return CLI_EXIT_OK;
}

static int verb_audit_lvltypes(int argc, char **argv)
{
   int n;
   int rc = cli_area_browser_ready(argc, argv);
   if (rc != CLI_EXIT_OK) return rc;

   n = area_browser_audit_lvltypes(stdout);
   printf("\n%d LvlTypes row(s) disagree with their Act column.\n", n);
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
      "export / export-compose flags:\n"
      "  --target=TOKEN/MODE[/WEAPON]    single-tuple shortcut\n"
      "  --category=<chars|monsters|npcs|objects>\n"
      "  --token=<code>                  per-token filter for bulk runs\n"
      "  --mode=<list|all>               comma-separated codes or 'all'\n"
      "  --weapon=<list|all>             chars only; ignored elsewhere\n"
      "  --direction=<n|n,m|n-m|all>     direction selector\n"
      "  --upscale=<1|2|4>               nearest-neighbour APNG scale\n"
      "  --out=<dir>                     falls back to [export_defaults]\n"
      "                                  compose_output\n"
      "\n"
      "export-raw flags:\n"
      "  --type=<dcc|dc6|dt1|cof|all>    content-type filter\n"
      "  --target=<path or glob>         single file or pattern\n"
      "  --scope=<all|folder>            bulk scope (area = GUI-only)\n"
      "  --folder=<prefix>               required if --scope=folder\n"
      "  --preset=<name>                 named [export_presets] preset\n"
      "  --pattern=<glob>                ad-hoc glob\n"
      "  --listfile=<path>               external listfile (only needed if\n"
      "                                  the in-MPQ one fails to decompress)\n"
      "  --out=<dir>                     falls back to raw_<type>_output\n"
      "\n"
      "Examples:\n"
      "  ds1edit list-mpqs\n"
      "  ds1edit probe \"data\\Global\\AnimData.d2\"\n"
      "  ds1edit probe-cof chars NE WL HTH\n"
      "  ds1edit export --target=NE/WL/HTH --direction=all --upscale=2\n"
      "  ds1edit export --category=monsters --mode=NU --direction=0\n"
      "  ds1edit export-raw --type=dt1 --scope=all --out=C:\\tiles\n"
      "\n"
      "Exit codes:\n"
      "  0 = clean run; 1 = some failures; 2 = nothing produced;\n"
      "  3 = bad arguments.\n"
      "\n"
      "DS1Edit continues win_ds1edit by Paul Siramy (2002-2011); see NOTICE.\n");
   return CLI_EXIT_OK;
}

/* ---- Top-level dispatcher ------------------------------------------- */

int cli_run(int argc, char **argv)
{
   const CLI_VERB_S *v;

   if (argc < 2 || argv[1] == NULL)
      return verb_help(argc, argv);

   v = find_verb(argv[1]);

   /* Legacy spellings. cli_is_verb() routes every '-'-prefixed argv[1] here,
    * so these three README-documented flags used to be answered with
    * "unknown verb" instead of reaching main.c's handler. Strip the dashes
    * and look the verb up again rather than keeping a second CLI style. */
   if (v == NULL && argv[1][0] == '-')
   {
      const char *bare = argv[1];
      while (*bare == '-') bare++;
      v = find_verb(bare);
      /* --list-areas-ext is list-areas with the extended formatter. */
      if (v == NULL && strcasecmp(bare, "list-areas-ext") == 0)
         v = find_verb("list-areas");
   }
   if (v == NULL)
   {
      fprintf(stderr, "ds1edit: unknown verb '%s'\n", argv[1]);
      verb_help(argc, argv);
      return CLI_EXIT_BAD_ARGS;
   }
   return v->fn(argc, argv);
}
