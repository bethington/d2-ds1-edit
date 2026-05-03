#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <allegro5/allegro.h>
#include <allegro5/allegro_native_dialog.h>

#include "structs.h"
#include "ui/compat.h"
#include "ui/compose_category_picker.h"
#include "ui/compose_mode_modal.h"
#include "ui/compose_preset_picker.h"
#include "ui/export_type_picker.h"
#include "ui/project_menu.h"
#include "ui/scope_picker.h"
#include "ui/upscale_mode_picker.h"
#include "ui/win_folder_picker.h"

#include "core/asset_export.h"
#include "core/compose_apng.h"
#include "core/compose_cof_path.h"
#include "core/compose_index.h"
#include "core/compose_iter.h"
#include "core/compose_naming.h"
#include "core/compose_palette.h"
#include "core/compose_palette_index.h"
#include "core/compose_render.h"
#include "core/export_progress.h"
#include "core/monstats2.h"
#include "core/palette.h"
#include "core/project.h"
#include "core/preferences.h"
#include "core/upscale.h"

// Shortcut debounce helper: wait until all given keys are released.
static void wait_release(int k1, int k2, int k3)
{
   int pressed;
   do {
      al_rest(0.01);
      al_get_keyboard_state(&a5_kb_state);
      pressed = key_pressed(k1) || key_pressed(k2) || key_pressed(k3);
   } while (pressed);
}

// Basename extraction: returns pointer into `path` at the last path element.
static const char *basename_of(const char *path)
{
   const char *p, *last = path;
   for (p = path; *p != 0; p++)
   {
      if (*p == '\\' || *p == '/')
         last = p + 1;
   }
   return last;
}

// On successful open/create, record the project into prefs and persist.
static void record_and_persist(const char *project_path)
{
   prefs_record_recent_project(project_path);
   if (glb_project.d2_install[0] != 0)
   {
      strncpy(glb_prefs.last_d2_install, glb_project.d2_install,
              sizeof(glb_prefs.last_d2_install) - 1);
      glb_prefs.last_d2_install[sizeof(glb_prefs.last_d2_install) - 1] = 0;
   }
   prefs_save();
}

// Prompt for a folder and return its path in `out`. Returns 1 if the user
// selected a folder, 0 on cancel. `initial` may be NULL.
//
// On Windows we use IFileOpenDialog directly (modern Win7+ picker with a
// prominent "New folder" button in the toolbar), which is critical for the
// New Project flow where users need to create a fresh folder. Allegro's
// pick_folder path used SHBrowseForFolderW whose equivalent button is
// small and easy to miss.
static int pick_folder(const char *title, const char *initial,
                       char *out, int out_cap)
{
#ifdef WIN32
   return win_pick_folder(title, initial, out, out_cap);
#else
   ALLEGRO_FILECHOOSER *dlg;
   const char *picked;
   int rc;

   dlg = al_create_native_file_dialog(
            initial,
            title,
            NULL, // patterns (unused for folders)
            ALLEGRO_FILECHOOSER_FOLDER);
   if (dlg == NULL) return 0;

   rc = al_show_native_file_dialog(a5_display, dlg);
   if (rc && al_get_native_file_dialog_count(dlg) > 0)
   {
      picked = al_get_native_file_dialog_path(dlg, 0);
      if (picked != NULL)
      {
         strncpy(out, picked, out_cap - 1);
         out[out_cap - 1] = 0;
         al_destroy_native_file_dialog(dlg);
         return 1;
      }
   }

   al_destroy_native_file_dialog(dlg);
   return 0;
#endif
}


/* ---- actions ---- */

static void action_new_project(void)
{
   char path[PROJECT_PATH_MAX];
   char ini[PROJECT_PATH_MAX];
   const char *install;
   const char *name;
   FILE *fp;

   if (!pick_folder(
         "New Project — click \"New folder\" to create one, then Select",
         glb_prefs.last_d2_install[0] ? glb_prefs.last_d2_install : NULL,
         path, sizeof(path)))
      return;

   // Refuse to overwrite an existing project.
   if (project_ini_path(path, ini, sizeof(ini)))
   {
      fp = fopen(ini, "rb");
      if (fp != NULL)
      {
         fclose(fp);
         al_show_native_message_box(a5_display,
            "Project already exists",
            "project.ini was found in the selected folder.",
            "Use Open Project (Ctrl+Shift+O) to load it instead.",
            NULL,
            ALLEGRO_MESSAGEBOX_WARN);
         return;
      }
   }

   install = glb_prefs.last_d2_install[0] ? glb_prefs.last_d2_install
           : (glb_config.d2_install ? glb_config.d2_install : "");
   name = basename_of(path);

   if (!project_create(path, name, install))
   {
      al_show_native_message_box(a5_display,
         "Create Project failed",
         "Could not write project.ini",
         "Check folder permissions and try again.",
         NULL,
         ALLEGRO_MESSAGEBOX_ERROR);
      return;
   }

   if (!project_load(path))
   {
      al_show_native_message_box(a5_display,
         "Create Project failed",
         "Project was written but could not be re-loaded.",
         NULL, NULL, ALLEGRO_MESSAGEBOX_ERROR);
      return;
   }

   project_apply_to_config();
   record_and_persist(path);

   fprintf(stderr, "project_menu: created and opened <%s>\n", path);
}

static void action_open_project(void)
{
   char path[PROJECT_PATH_MAX];
   char ini[PROJECT_PATH_MAX];
   const char *initial;
   FILE *fp;

   // Prefer the most recent project dir as the starting location.
   initial = (glb_prefs.recent_count > 0)
           ? glb_prefs.recent_projects[0]
           : (glb_prefs.last_d2_install[0] ? glb_prefs.last_d2_install : NULL);

   if (!pick_folder("Open Project — pick the project folder",
                    initial, path, sizeof(path)))
      return;

   if (!project_ini_path(path, ini, sizeof(ini)) ||
       (fp = fopen(ini, "rb")) == NULL)
   {
      al_show_native_message_box(a5_display,
         "Not a Project",
         "project.ini was not found in the selected folder.",
         "Use Ctrl+Shift+N to create a new project here.",
         NULL,
         ALLEGRO_MESSAGEBOX_ERROR);
      return;
   }
   fclose(fp);

   if (!project_load(path))
   {
      al_show_native_message_box(a5_display,
         "Open Project failed",
         "project.ini could not be parsed.",
         NULL, NULL, ALLEGRO_MESSAGEBOX_ERROR);
      return;
   }

   project_apply_to_config();
   record_and_persist(path);

   fprintf(stderr, "project_menu: opened <%s>\n", path);
}

static void action_close_project(void)
{
   char msg[PROJECT_PATH_MAX + 64];

   if (!glb_project.is_open) return;

   snprintf(msg, sizeof(msg),
            "Close project \"%s\"?\n\nEdits already saved to disk are kept.\n"
            "Open DS1 files remain open.",
            glb_project.name[0] ? glb_project.name : glb_project.path);

   int rc = al_show_native_message_box(a5_display,
      "Close Project",
      msg,
      "Closing will clear the current mod overlay; new saves will go to the raw filesystem.",
      NULL,
      ALLEGRO_MESSAGEBOX_YES_NO | ALLEGRO_MESSAGEBOX_QUESTION);

   if (rc != 1) return; // Yes == 1

   fprintf(stderr, "project_menu: closing <%s>\n", glb_project.path);

   project_close();
   glb_config.mod_dir[0] = NULL; // clear overlay
}

static int make_virtual_prefix_from_mod_path(const char *picked_path,
                                             char *out_prefix,
                                             int out_cap)
{
   int mod_len;
   const char *relative;

   if (picked_path == NULL || out_prefix == NULL || out_cap <= 0 ||
       glb_config.mod_dir[0] == NULL)
      return 0;

   mod_len = (int) strlen(glb_config.mod_dir[0]);
   if (strnicmp(picked_path, glb_config.mod_dir[0], mod_len) != 0)
      return 0;

   relative = picked_path + mod_len;
   if (*relative == '\\' || *relative == '/')
      relative++;

   if (*relative == 0)
      snprintf(out_prefix, out_cap, "Data");
   else
      snprintf(out_prefix, out_cap, "Data\\%s", relative);
   return 1;
}

static const char *choose_export_type(const char *title)
{
   return export_type_picker_choose(title);
}

static int choose_upscale_mode(const char *title)
{
   if (!upscale_is_remote_configured())
      return UPSCALE_MODE_NONE;

   return upscale_mode_picker_choose(title, TRUE);
}

static int ensure_export_palette_ready(void)
{
   if (a5_current_palette == NULL)
      a5_current_palette = &glb_ds1edit.vga_pal[0];
   return 1;
}

static int directory_has_entries(const char *path)
{
#ifdef WIN32
   WIN32_FIND_DATAA fd;
   HANDLE hFind;
   char search[PROJECT_PATH_MAX + 8];

   if (path == NULL || path[0] == 0)
      return 0;

   snprintf(search, sizeof(search), "%s\\*", path);
   hFind = FindFirstFileA(search, &fd);
   if (hFind == INVALID_HANDLE_VALUE)
      return 0;

   do
   {
      if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
      {
         FindClose(hFind);
         return 1;
      }
   } while (FindNextFileA(hFind, &fd));

   FindClose(hFind);
#else
   (void) path;
#endif
   return 0;
}

static int confirm_overwrite_output(const char *title, const char *output_path)
{
   int rc;
   char detail[PROJECT_PATH_MAX + 96];

   if (!directory_has_entries(output_path))
      return 1;

   snprintf(detail, sizeof(detail),
            "The selected output folder already contains files.\n\n%s\n\nOverwrite existing export results?",
            output_path);
   rc = al_show_native_message_box(a5_display,
      title,
      "Output folder is not empty.",
      detail,
      NULL,
      ALLEGRO_MESSAGEBOX_YES_NO | ALLEGRO_MESSAGEBOX_WARN);
   return rc == 1;
}

static int run_upscale_pipeline(const char *title,
                                const char *staging_path,
                                const char *output_path,
                                int upscale_mode)
{
   int scale;
   int rc;
   char error[512];

   if (upscale_mode == UPSCALE_MODE_NONE)
      return 1;

   scale = (upscale_mode == UPSCALE_MODE_4X) ? 4 : 2;
   error[0] = 0;

   /* Remote path is one big synchronous blob (zip + upload + wait +
    * download + extract). Until the async-upscale plan lands, we can
    * only repaint the dialog ONCE before the call to indicate what is
    * happening, then the dialog freezes until the call returns. The
    * label tells the user; the bar advances when control returns. */
   if (export_task_is_active())
   {
      export_progress_set_show_remote_stages(1);
      export_progress_set_stage(EXPORT_STAGE_REMOTE_PROCESSING,
                                "Uploading and processing on remote service...",
                                0);
      export_progress_pump();
   }

   if (upscale_directory_remote(staging_path, output_path, scale,
                                "realesrgan", error, sizeof(error)))
      return 1;

   /* Remote pipeline is the only supported 2x/4x path. No silent
    * fallback to local -- the user explicitly asked for the docker
    * service's quality, so a failure should surface, not get
    * downgraded. The native error message box below carries the
    * detail; rc here just controls flow. */
   (void) rc;

   al_show_native_message_box(a5_display,
      title,
      "Upscale failed.",
      error[0] ? error
               : "The remote upscale service did not return a result. "
                 "Check that upscale_service_url is reachable and the "
                 "docker server is running.",
      NULL,
      ALLEGRO_MESSAGEBOX_ERROR);
   return 0;
}

static void show_export_result(const char *title,
                               const char *summary,
                               const char *detail,
                               int exported_count,
                               const char *output_path)
{
   char message[512];

   if (exported_count <= 0)
   {
      al_show_native_message_box(a5_display,
         title,
         "No PNGs were exported.",
         detail,
         NULL,
         ALLEGRO_MESSAGEBOX_ERROR);
      return;
   }

   snprintf(message, sizeof(message), summary, exported_count);
   al_show_native_message_box(a5_display,
      title,
      message,
      output_path,
      NULL,
      0);
}

/* ---- Compose-mode discovery + iteration ---- */

/* The set of leaf categories that get iterated. COMPOSE_CATEGORY_NONE
 * ("All composed") expands to the full set; the four narrow choices
 * just produce a single-element list. */
typedef struct COMPOSE_CAT_LIST_S
{
   COMPOSE_CATEGORY_E cats[4];
   int count;
} COMPOSE_CAT_LIST_S;

static void compose_expand_categories(COMPOSE_CATEGORY_E picked,
                                      COMPOSE_CAT_LIST_S *out)
{
   out->count = 0;
   if (picked == COMPOSE_CATEGORY_NONE)
   {
      out->cats[out->count++] = COMPOSE_CATEGORY_PLAYER_CHAR;
      out->cats[out->count++] = COMPOSE_CATEGORY_MONSTER;
      out->cats[out->count++] = COMPOSE_CATEGORY_NPC;
      out->cats[out->count++] = COMPOSE_CATEGORY_OBJECT;
   }
   else
   {
      out->cats[out->count++] = picked;
   }
}

/* Token enumeration: returns the count, and the i-th token. The
 * supplied buffers receive code + full name (full name may be empty
 * for player classes -- the naming helper handles that). For player
 * chars we use the hardcoded compose_iter list and look up the
 * descriptive name via compose_naming_class_name. For monsters / NPCs
 * / objects we use the compose_index built earlier in this function. */
static int compose_token_count(COMPOSE_CATEGORY_E category)
{
   switch (category)
   {
      case COMPOSE_CATEGORY_PLAYER_CHAR: return compose_iter_player_class_count();
      case COMPOSE_CATEGORY_MONSTER:     return compose_index_monster_count();
      case COMPOSE_CATEGORY_NPC:         return compose_index_npc_count();
      case COMPOSE_CATEGORY_OBJECT:      return compose_index_object_count();
      default: return 0;
   }
}

static int compose_token_at(COMPOSE_CATEGORY_E category, int idx,
                            char *code_out, int code_cap,
                            char *name_out, int name_cap)
{
   if (code_out != NULL && code_cap > 0) code_out[0] = 0;
   if (name_out != NULL && name_cap > 0) name_out[0] = 0;

   if (category == COMPOSE_CATEGORY_PLAYER_CHAR)
   {
      const char *code = compose_iter_player_class_at(idx);
      const char *name;
      if (code == NULL) return 0;
      strncpy(code_out, code, (size_t) code_cap - 1);
      code_out[code_cap - 1] = 0;
      name = compose_naming_class_name(code);
      if (name != NULL)
      {
         strncpy(name_out, name, (size_t) name_cap - 1);
         name_out[name_cap - 1] = 0;
      }
      return 1;
   }
   else
   {
      const COMPOSE_TOKEN_S *t = NULL;
      switch (category)
      {
         case COMPOSE_CATEGORY_MONSTER: t = compose_index_monster_at(idx); break;
         case COMPOSE_CATEGORY_NPC:     t = compose_index_npc_at(idx);     break;
         case COMPOSE_CATEGORY_OBJECT:  t = compose_index_object_at(idx);  break;
         default: break;
      }
      if (t == NULL) return 0;
      strncpy(code_out, t->code, (size_t) code_cap - 1);
      code_out[code_cap - 1] = 0;
      strncpy(name_out, t->name, (size_t) name_cap - 1);
      name_out[name_cap - 1] = 0;
      return 1;
   }
}

/* Mode/weapon iteration helpers: when the picker selection's
 * use_all flag is set, walk the hardcoded compose_iter default
 * list; otherwise walk the codes array stored in the selection. */
static int compose_sel_count(const COMPOSE_PRESET_SELECTION_S *sel,
                             int default_count)
{
   if (sel == NULL) return default_count;
   if (sel->use_all) return default_count;
   return sel->code_count;
}

static const char * compose_sel_at(const COMPOSE_PRESET_SELECTION_S *sel,
                                   int idx,
                                   const char *(*default_at)(int))
{
   if (sel == NULL || sel->use_all)
      return default_at(idx);
   if (idx < 0 || idx >= sel->code_count) return NULL;
   return sel->codes[idx];
}

/* Run state for the iteration loop.
 *
 * Failures are streamed to a log file rather than collected in memory:
 * tens of thousands of (token, mode, wclass, dir) tuples are walked
 * per run, and an in-memory list isn't load-bearing for the user
 * (they need either "yes everything worked" or a scrollable list to
 * triage). The log file path is shown in the final summary modal so
 * the user can open it in their editor of choice for triage. */
typedef struct COMPOSE_RUN_STATE_S
{
   int   success_count;
   int   failure_count;
   int   skipped_tuples;   /* (token, mode, wclass) tuples with no COF */
   int   scale;            /* 1 / 2 / 4 -- nearest-neighbour APNG scale */
   FILE *failure_log;      /* opened lazily on first failure */
   char  failure_log_path[PROJECT_PATH_MAX];
   char  first_failure[256];
   /* Sample of first few skipped paths, to surface in the "Nothing
    * Exported" dialog when ALL tuples skip. Without this the user can
    * only see a count and has no way to diagnose whether the MPQ
    * chain is missing, the path format is wrong, or the token list
    * is bogus. We capture path strings only — no filehandle, no
    * timestamps; this is a one-shot diagnostic, not a full audit. */
   #define COMPOSE_SKIP_SAMPLE_MAX 5
   int   skip_sample_count;
   char  skip_sample[COMPOSE_SKIP_SAMPLE_MAX][PROJECT_PATH_MAX];
} COMPOSE_RUN_STATE_S;

/* Lazily open the failures log inside the chosen output root. The log
 * is plain text, one line per failure, with timestamp + tuple +
 * output path. Called from the inner loop at the moment of the first
 * failure so success-only runs don't leave an empty log file. */
static void compose_open_failure_log(COMPOSE_RUN_STATE_S *st,
                                     const char *root)
{
   if (st->failure_log != NULL) return;
   if (root == NULL || root[0] == 0) return;

   /* Path-mirrored layout: <root>/compose_failures.log. We don't
    * stamp the filename with a timestamp; if the user re-runs we
    * overwrite the previous log (which matches the "freshest run
    * wins" mental model of the export-to-folder flow). */
   snprintf(st->failure_log_path, sizeof(st->failure_log_path),
            "%s\\compose_failures.log", root);

   /* The output root may not exist yet (the ensure_dir calls happen
    * per-token inside compose_run_token). Create it lazily. */
   compose_iter_ensure_dir(root);

   st->failure_log = fopen(st->failure_log_path, "w");
   if (st->failure_log == NULL)
   {
      /* If we can't open the log, swallow the failure -- the modal
       * will still show counts and the first_failure path. */
      st->failure_log_path[0] = 0;
      return;
   }

   fprintf(st->failure_log,
      "Compose-mode export failures\n"
      "============================\n"
      "Each line: <token> <mode> <wclass> dir<N> -> <output path>\n"
      "\n");
}

static void compose_record_failure(COMPOSE_RUN_STATE_S *st,
                                   const char *root,
                                   const char *token,
                                   const char *mode,
                                   const char *wclass,
                                   int direction,
                                   const char *output_path)
{
   st->failure_count++;
   if (st->first_failure[0] == 0 && output_path != NULL)
   {
      strncpy(st->first_failure, output_path,
              sizeof(st->first_failure) - 1);
      st->first_failure[sizeof(st->first_failure) - 1] = 0;
   }

   compose_open_failure_log(st, root);
   if (st->failure_log != NULL)
   {
      fprintf(st->failure_log, "%s %s %s dir%d -> %s\n",
              token != NULL ? token : "?",
              mode  != NULL ? mode  : "?",
              (wclass != NULL && wclass[0] != 0) ? wclass : "-",
              direction,
              output_path != NULL ? output_path : "?");
      fflush(st->failure_log);
   }
}

static void compose_close_failure_log(COMPOSE_RUN_STATE_S *st)
{
   if (st->failure_log != NULL)
   {
      fclose(st->failure_log);
      st->failure_log = NULL;
   }
}

/* Iterate the (mode, wclass, direction) sub-cube for one token.
 * Returns 1 if the user requested cancel, 0 to continue. */
static int compose_run_token(const char *root,
                             COMPOSE_CATEGORY_E category,
                             const char *token,
                             const char *token_name,
                             const COMPOSE_PRESET_SELECTION_S *mode_sel,
                             const COMPOSE_PRESET_SELECTION_S *weapon_sel,
                             COMPOSE_RUN_STATE_S *st)
{
   int n_modes = compose_sel_count(mode_sel,
                                   compose_iter_default_mode_count());
   int n_weapons = (weapon_sel != NULL && weapon_sel->code_count > 0
                    && !weapon_sel->use_all
                    && weapon_sel->codes[0][0] == 0)
                   ? 1
                   : compose_sel_count(weapon_sel,
                                       compose_iter_default_weapon_count());
   const char *skin = compose_iter_category_skin(category);
   const char *base = compose_iter_category_base(category);
   int m, w, d;
   COMPOSE_RENDER_PARAMS_S params;
   char path_buf[PROJECT_PATH_MAX];
   char dir_buf[PROJECT_PATH_MAX];
   /* Per-token palette switch (palette v2). Save + restore so palette
    * state never leaks to the next token. */
   RGBA_PALETTE *saved_palette = a5_current_palette;
   {
      int act = compose_palette_resolve_act(category, token);
      if (act < 1 || act > ACT_MAX) act = 1;
      a5_current_palette = &glb_ds1edit.vga_pal[act - 1];
   }

   if (base == NULL) { a5_current_palette = saved_palette; return 0; }

   for (m = 0; m < n_modes; m++)
   {
      const char *mode = compose_sel_at(mode_sel, m,
                                        compose_iter_default_mode_at);
      if (mode == NULL || mode[0] == 0) continue;

      for (w = 0; w < n_weapons; w++)
      {
         const char *wclass;
         int dir_count;
         char resolved_wclass_buf[16] = {0};

         if (weapon_sel != NULL && weapon_sel->code_count > 0
             && !weapon_sel->use_all && weapon_sel->codes[0][0] == 0)
            wclass = "";
         else
            wclass = compose_sel_at(weapon_sel, w,
                                    compose_iter_default_weapon_at);
         if (wclass == NULL) wclass = "";

         /* Probe the COF to learn the direction count. The resolve
          * variant tries the supplied wclass first, then MonStats2
          * BaseW for monsters/NPCs, then "HTH" as a last fallback;
          * on success it writes the wclass that worked into
          * resolved_wclass_buf, which we feed to compose_apng_export
          * below. Most (token, mode, wclass) combinations are invalid
          * in D2 -- a Necromancer doesn't have a Whirlwind animation,
          * etc. -- so dir_count==0 means "skip this tuple cleanly." */
         dir_count = compose_iter_probe_direction_count_resolve(
            category, token, mode, wclass,
            resolved_wclass_buf, (int) sizeof(resolved_wclass_buf));
         if (dir_count > 0 && resolved_wclass_buf[0] != 0)
            wclass = resolved_wclass_buf;
         if (dir_count <= 0)
         {
            st->skipped_tuples++;
            /* Capture the first few skipped paths so the "Nothing
             * Exported" dialog can show them. Reconstructs the same
             * path that compose_iter_probe_direction_count tried,
             * which is what was looked up against the MPQ chain. */
            if (st->skip_sample_count < COMPOSE_SKIP_SAMPLE_MAX)
            {
               char skip_path[PROJECT_PATH_MAX];
               if (compose_cof_path_build(
                       skip_path, (int) sizeof(skip_path),
                       base, token, mode, wclass != NULL ? wclass : ""))
               {
                  strncpy(
                     st->skip_sample[st->skip_sample_count],
                     skip_path,
                     sizeof(st->skip_sample[0]) - 1);
                  st->skip_sample[st->skip_sample_count]
                     [sizeof(st->skip_sample[0]) - 1] = 0;
                  st->skip_sample_count++;
               }
            }
            if (export_progress_pump())
               return 1;
            continue;
         }

         /* Make sure the per-token output dir exists once per tuple. */
         if (compose_iter_build_output_dir(dir_buf, (int) sizeof(dir_buf),
                                           root, category, token,
                                           token_name))
            compose_iter_ensure_dir(dir_buf);

         memset(&params, 0, sizeof(params));
         params.base   = base;
         params.token  = token;
         params.mode   = mode;
         params.wclass = wclass;
         params.skin   = skin;

         /* For monsters / NPCs, MonStats2 has per-layer skin variants.
          * Look them up via compose_index's stored MonStatsEx. */
         if (category == COMPOSE_CATEGORY_MONSTER
             || category == COMPOSE_CATEGORY_NPC)
         {
            const COMPOSE_TOKEN_S *(*at)(int) =
               (category == COMPOSE_CATEGORY_MONSTER)
                  ? compose_index_monster_at
                  : compose_index_npc_at;
            int n_tok = (category == COMPOSE_CATEGORY_MONSTER)
                           ? compose_index_monster_count()
                           : compose_index_npc_count();
            int ti;
            for (ti = 0; ti < n_tok; ti++)
            {
               const COMPOSE_TOKEN_S *t = at(ti);
               const MONSTATS2_ENTRY_S *e;
               int li;
               if (t == NULL) continue;
               if (stricmp(t->code, token) != 0) continue;
               if (t->mon_stats_ex[0] == 0) break;
               e = monstats2_find(t->mon_stats_ex);
               if (e == NULL) break;
               for (li = 0; li < COMPOSE_RENDER_LAYER_COUNT
                        && li < MONSTATS2_LAYER_COUNT; li++)
               {
                  if (e->layers[li].used && e->layers[li].skin[0] != 0)
                     strncpy(params.skin_per_layer[li],
                             e->layers[li].skin,
                             COMPOSE_RENDER_SKIN_MAX - 1);
               }
               break;
            }
         }

         for (d = 0; d < dir_count; d++)
         {
            char status[256];
            int ok;

            params.direction = d;

            if (!compose_iter_build_output_path(path_buf, (int) sizeof(path_buf),
                                                root, category, token,
                                                token_name, mode,
                                                wclass, d))
            {
               compose_record_failure(st, root, token, mode, wclass, d,
                                      "<path-build-failure>");
               continue;
            }

            snprintf(status, sizeof(status),
                     "%s %s%s dir %d",
                     token, mode,
                     (wclass[0] != 0) ? wclass : "",
                     d);
            export_progress_set_current_item(status);

            ok = compose_apng_export_scaled(&params, path_buf,
                                            st->scale > 0 ? st->scale : 1);
            if (ok)
               st->success_count++;
            else
               compose_record_failure(st, root, token, mode, wclass, d,
                                      path_buf);

            export_progress_advance(1);
            if (export_progress_pump())
            {
               a5_current_palette = saved_palette;
               return 1;
            }
         }
      }
   }

   a5_current_palette = saved_palette;
   return 0;
}

// Compose-mode export flow. Invoked from action_export_unified when
// the user picks DCC or All from the type picker AND confirms compose
// mode in the follow-up modal. Walks through the compose-specific
// picker sequence:
//
//   1. Compose category picker (player chars / monsters / NPCs / objects / all)
//   2. Mode preset picker (multi-select)
//   3. Weapon preset picker (multi-select; only when chars in scope)
//   4. Output folder picker
//   5. Discovery + per-tuple iteration loop
//
// The iteration walks (category x token x mode x wclass x direction)
// tuples; each leaf produces one APNG. COFs that don't exist for a
// given (token, mode, wclass) combo are silently skipped (the vast
// majority of combinations are invalid in D2).
static void action_export_compose(void)
{
   COMPOSE_CATEGORY_E category = COMPOSE_CATEGORY_NONE;
   COMPOSE_PRESET_SELECTION_S mode_sel;
   COMPOSE_PRESET_SELECTION_S weapon_sel;
   COMPOSE_CAT_LIST_S cat_list;
   COMPOSE_RUN_STATE_S run;
   char output_path[PROJECT_PATH_MAX];
   int any_chars_in_scope;
   int c;
   int cancelled = 0;
   char message[1024];

   memset(&mode_sel,   0, sizeof(mode_sel));
   memset(&weapon_sel, 0, sizeof(weapon_sel));
   memset(&run,        0, sizeof(run));

   if (!compose_category_picker_show(&category))
      return;

   if (!compose_mode_picker_show(&mode_sel))
      return;

   any_chars_in_scope = (category == COMPOSE_CATEGORY_PLAYER_CHAR
                         || category == COMPOSE_CATEGORY_NONE);

   if (any_chars_in_scope)
   {
      if (!compose_weapon_picker_show(&weapon_sel))
         return;
   }
   else
   {
      /* Monsters / NPCs / objects don't have weapon-class variants;
       * fill in a single empty entry so the iteration loop has a
       * uniform shape. */
      weapon_sel.use_all = 0;
      weapon_sel.code_count = 1;
      weapon_sel.codes[0][0] = 0;
   }

   /* Initial folder for the picker: prefer [export_defaults]
    * compose_output if set, then the open project, then nothing.
    * The CLI uses the same default as a fallback when --out= is
    * omitted, so the two surfaces stay consistent. */
   {
      const char *initial = NULL;
      if (glb_config.export_default_compose_output != NULL
          && glb_config.export_default_compose_output[0] != 0)
         initial = glb_config.export_default_compose_output;
      else if (glb_project.is_open)
         initial = glb_project.path;

      if (!pick_folder("Compose - choose an output folder",
                       initial, output_path, sizeof(output_path)))
         return;
   }

   /* Upscale picker. Always offered (1x / 2x / 4x) for compose mode --
    * unlike raw export which gates 2x/4x behind upscale_is_remote_
    * configured, compose-mode's scaler is local nearest-neighbour
    * applied to the per-frame RGBA buffers before APNG write. It's
    * pixel-perfect for D2 sprite art and has no external dependency. */
   {
      int picked = upscale_mode_picker_choose(
         "Compose Export - choose upscale", FALSE);
      if (picked < 0) return;  /* cancel */
      if (picked == UPSCALE_MODE_4X)      run.scale = 4;
      else if (picked == UPSCALE_MODE_2X) run.scale = 2;
      else                                run.scale = 1;
   }

   /* Build the monster / NPC / object index from MonStats.txt and
    * Objects.txt; build the MonStats2 sprite-info index too so
    * monsters' COF wclass + per-layer skins resolve correctly.
    * Both are idempotent and skipped for player-chars-only runs. */
   compose_expand_categories(category, &cat_list);
   if (category != COMPOSE_CATEGORY_PLAYER_CHAR)
   {
      (void) compose_index_build();
      (void) monstats2_build();
      /* Per-monster Act resolution from Levels.txt (palette v2). */
      (void) compose_palette_index_build();
   }

   /* Drive the run via the export_progress dialog. We don't know the
    * exact items_total ahead of time (each tuple's direction count is
    * resolved by COF probe at iteration time), so we feed the dialog
    * a coarse upper bound and let it advance one APNG per success.
    * For the active-stage label we use NATIVE_EXPORT, the same stage
    * the raw-export path uses for its main loop. */
   export_progress_begin("Compose Export");
   export_progress_set_show_remote_stages(0);
   export_progress_set_stage(EXPORT_STAGE_PREPARE, "Indexing assets", 0);
   export_progress_force_repaint();
   export_progress_pump();

   {
      /* Coarse upper bound: each token contributes (n_modes * n_weapons
       * * 16). The dialog clamps the bar at 100% so an over-estimate
       * is harmless; we just won't see the bar fill all the way. The
       * skip-on-COF-miss path is the dominant trim, and we don't pay
       * for it here. */
      int n_modes = compose_sel_count(&mode_sel,
                                      compose_iter_default_mode_count());
      int n_weapons = compose_sel_count(&weapon_sel,
                                        compose_iter_default_weapon_count());
      int total = 0;
      int i;
      for (i = 0; i < cat_list.count; i++)
         total += compose_token_count(cat_list.cats[i]);
      total *= n_modes * n_weapons * 16; /* 16 dirs upper bound per tuple */
      if (total < 1) total = 1;

      export_progress_set_stage(EXPORT_STAGE_NATIVE_EXPORT,
                                "Composing animations", total);
      export_progress_force_repaint();
   }

   /* Pre-build a "no weapon class" selection for monster / NPC / object
    * categories. Player chars use the picker's weapon_sel verbatim. */
   {
      COMPOSE_PRESET_SELECTION_S empty_wclass;
      memset(&empty_wclass, 0, sizeof(empty_wclass));
      empty_wclass.code_count = 1;
      empty_wclass.codes[0][0] = 0;

      for (c = 0; !cancelled && c < cat_list.count; c++)
      {
         COMPOSE_CATEGORY_E cat = cat_list.cats[c];
         int n_tokens = compose_token_count(cat);
         const COMPOSE_PRESET_SELECTION_S *cat_weapons =
            (cat == COMPOSE_CATEGORY_PLAYER_CHAR) ? &weapon_sel
                                                  : &empty_wclass;
         int t;

         for (t = 0; !cancelled && t < n_tokens; t++)
         {
            char tok_code[COMPOSE_TOKEN_CODE_MAX];
            char tok_name[COMPOSE_TOKEN_NAME_MAX];

            if (!compose_token_at(cat, t,
                                  tok_code, sizeof(tok_code),
                                  tok_name, sizeof(tok_name)))
               continue;

            cancelled = compose_run_token(output_path, cat, tok_code, tok_name,
                                          &mode_sel, cat_weapons, &run);
         }
      }
   }

   export_progress_end();
   compose_close_failure_log(&run);

   /* Final summary. Failures (if any) were streamed to a log file in
    * the output root; we point the user at it for triage rather than
    * inflating the modal with a per-failure list. */
   {
      char failure_tail[PROJECT_PATH_MAX + 64];
      failure_tail[0] = 0;
      if (run.failure_count > 0 && run.failure_log_path[0] != 0)
         snprintf(failure_tail, sizeof(failure_tail),
                  "\n\nFailure log: %s", run.failure_log_path);
      else if (run.first_failure[0] != 0)
         snprintf(failure_tail, sizeof(failure_tail),
                  "\n\nFirst failure: %s", run.first_failure);

      if (cancelled)
      {
         snprintf(message, sizeof(message),
            "Compose export was cancelled.\n\n"
            "  Animations exported:    %d\n"
            "  Failures:               %d\n"
            "  Skipped (no COF):       %d%s",
            run.success_count, run.failure_count, run.skipped_tuples,
            failure_tail);
         al_show_native_message_box(a5_display,
            "Compose Export - Cancelled",
            "The export was interrupted.",
            message, NULL, ALLEGRO_MESSAGEBOX_WARN);
      }
      else if (run.success_count == 0 && run.failure_count == 0)
      {
         /* Append a sample of the first few paths the probe tried so
          * the user can see whether the path format / casing / MPQ
          * residency is what they expect. Without this they only
          * have a count and no actionable diagnostic. */
         char sample_tail[PROJECT_PATH_MAX * COMPOSE_SKIP_SAMPLE_MAX + 256];
         sample_tail[0] = 0;
         if (run.skip_sample_count > 0)
         {
            int sn;
            int off = snprintf(sample_tail, sizeof(sample_tail),
                               "\n\nFirst %d path(s) probed:",
                               run.skip_sample_count);
            for (sn = 0; sn < run.skip_sample_count
                 && off < (int) sizeof(sample_tail); sn++)
            {
               int n = snprintf(sample_tail + off,
                                sizeof(sample_tail) - (size_t) off,
                                "\n  %s", run.skip_sample[sn]);
               if (n < 0) break;
               off += n;
            }
         }
         snprintf(message, sizeof(message),
            "No COFs matched the selected (category, mode, weapon)\n"
            "tuples. This usually means the chosen combinations don't\n"
            "exist in the loaded MPQ chain.\n\n"
            "  Skipped (no COF):  %d%s",
            run.skipped_tuples, sample_tail);
         al_show_native_message_box(a5_display,
            "Compose Export - Nothing Exported",
            "No animations were produced.",
            message, NULL, ALLEGRO_MESSAGEBOX_WARN);
      }
      else
      {
         snprintf(message, sizeof(message),
            "  Animations exported:    %d\n"
            "  Failures:               %d\n"
            "  Skipped (no COF):       %d\n"
            "\n"
            "Output: %s%s",
            run.success_count, run.failure_count, run.skipped_tuples,
            output_path, failure_tail);
         al_show_native_message_box(a5_display,
            "Compose Export - Done",
            "Compose export finished.",
            message, NULL,
            (run.failure_count > 0) ? ALLEGRO_MESSAGEBOX_WARN : 0);
      }
   }
}

// Unified export action: type picker -> scope picker -> output folder ->
// upscale mode -> run. Replaces the four legacy export actions.
static void action_export_unified(void)
{
   const char *type_filter;
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   int area_available;
   SCOPE_RESULT_S scope;
   ASSET_EXPORT_PLAN_S plan;
   char output_path[PROJECT_PATH_MAX];
   char staging_path[PROJECT_PATH_MAX];
   const char *export_path;
   char folder_path[PROJECT_PATH_MAX];
   char asset_prefix[PROJECT_PATH_MAX];
   int exported_count;
   int upscale_mode;

   /* Defensive guard against re-entry. Should never fire under the
    * current flow (export runs synchronously on the main thread, so a
    * second Ctrl+Shift+A can't arrive mid-export), but keeps us safe
    * if a future scripted invocation or worker-thread driver ever
    * lands. */
   if (export_task_is_active())
      return;

   if (glb_config.mod_dir[0] == NULL || glb_config.mod_dir[0][0] == 0)
   {
      al_show_native_message_box(a5_display,
         "Export Assets",
         "No mod overlay is configured.",
         "Set a mod directory in Ds1edit.ini before exporting.",
         NULL,
         ALLEGRO_MESSAGEBOX_WARN);
      return;
   }

   type_filter = choose_export_type("Export - choose asset type");
   if (type_filter == NULL)
      return;

   /* Q5b: when type is DCC or All, ask whether the user wants compose
    * mode (fully-blended character/monster animations as APNG) or the
    * existing raw-frame export. The follow-up modal returns 1 = yes,
    * 2 = no, 0 = cancel. */
   if (stricmp(type_filter, "dcc") == 0 || stricmp(type_filter, "all") == 0)
   {
      int compose_choice = compose_mode_modal_show();
      if (compose_choice == 0)
         return;  /* user cancelled the entire flow */
      if (compose_choice == 1)
      {
         /* Compose mode: branch into the compose-specific picker
          * sequence. Falls through to the placeholder summary at the
          * end of action_export_compose. */
         action_export_compose();
         return;
      }
      /* compose_choice == 2 -> raw export; fall through to existing
       * scope picker flow. */
   }

   area_available =
      (ab->selected_group >= 0 && ab->selected_group < ab->group_count);

   if (!scope_picker_choose("Export - choose scope",
                            type_filter, area_available, &scope))
      return;

   asset_export_plan_init(&plan);

   switch (scope.kind)
   {
   case SCOPE_KIND_ALL:
      asset_export_plan_for_prefix("Data", type_filter, &plan);
      break;

   case SCOPE_KIND_AREA:
      if (area_available)
         asset_export_plan_for_area_group(&ab->groups[ab->selected_group],
                                          &plan);
      break;

   case SCOPE_KIND_FOLDER:
      if (!pick_folder("Export - choose a source folder under the mod overlay",
                       glb_config.mod_dir[0],
                       folder_path, sizeof(folder_path)))
         return;
      if (!make_virtual_prefix_from_mod_path(folder_path,
                                             asset_prefix, sizeof(asset_prefix)))
      {
         al_show_native_message_box(a5_display,
            "Export Assets",
            "The selected folder is outside the current mod overlay.",
            glb_config.mod_dir[0],
            NULL,
            ALLEGRO_MESSAGEBOX_ERROR);
         return;
      }
      asset_export_plan_for_prefix(asset_prefix, type_filter, &plan);
      break;

   case SCOPE_KIND_PATTERN:
      asset_export_plan_for_pattern(scope.pattern, &plan);
      break;

   default:
      return;
   }

   if (plan.count <= 0)
   {
      int candidates = plan.total_candidates;
      asset_export_plan_free(&plan);

      if (candidates == 0)
      {
         al_show_native_message_box(a5_display,
            "Export Assets",
            "Scope matched no files.",
            "Check the pattern, the selected folder, or your mod_dir setting.",
            NULL,
            ALLEGRO_MESSAGEBOX_WARN);
      }
      else
      {
         char detail[256];
         snprintf(detail, sizeof(detail),
            "Found %d candidate(s) but skipped all of them. The only "
            "content-level filter active is single-frame DC6 "
            "(export_dc6_single_frame_only=YES). Set it to NO in "
            "Ds1edit.ini to include multi-frame files.",
            candidates);
         al_show_native_message_box(a5_display,
            "Export Assets",
            "All matching files were filtered out.",
            detail,
            NULL,
            ALLEGRO_MESSAGEBOX_WARN);
      }
      return;
   }

   if (!pick_folder("Export - choose an output folder",
                    glb_project.is_open ? glb_project.path : NULL,
                    output_path, sizeof(output_path)))
   {
      asset_export_plan_free(&plan);
      return;
   }
   if (!confirm_overwrite_output("Export Assets", output_path))
   {
      asset_export_plan_free(&plan);
      return;
   }

   upscale_mode = choose_upscale_mode("Export - choose upscale mode");
   if (upscale_mode < 0)
   {
      asset_export_plan_free(&plan);
      return;
   }

   export_path = output_path;
   staging_path[0] = 0;
   if (upscale_mode != UPSCALE_MODE_NONE)
   {
      if (!upscale_create_temp_dir(staging_path, sizeof(staging_path)))
      {
         asset_export_plan_free(&plan);
         al_show_native_message_box(a5_display,
            "Export Assets",
            "Failed to prepare temporary export staging.",
            NULL,
            NULL,
            ALLEGRO_MESSAGEBOX_ERROR);
         return;
      }
      export_path = staging_path;
   }

   if (scope.kind == SCOPE_KIND_AREA && area_available)
   {
      int pal_idx = palette_resolve_index(ab->groups[ab->selected_group].act, 0);
      a5_current_palette = &glb_ds1edit.vga_pal[pal_idx];
   }
   else
   {
      ensure_export_palette_ready();
   }

   /* From here on the export is running. The progress dialog renders
    * from the cooperative pump that asset_export_run_plan and
    * upscale_directory_local_recursive call between items. */
   export_progress_begin("Export Assets");
   export_progress_set_stage(EXPORT_STAGE_NATIVE_EXPORT,
                             "Exporting native PNGs...", plan.count);

   exported_count = asset_export_run_plan(&plan, export_path);
   asset_export_plan_free(&plan);

   if (export_progress_cancel_requested())
   {
      char detail[256];
      snprintf(detail, sizeof(detail),
         "Exported %d native PNG(s) to %s before cancel; partial output "
         "was kept.", exported_count,
         staging_path[0] != 0 ? staging_path : output_path);
      export_progress_end();
      al_show_native_message_box(a5_display,
         "Export Assets",
         "Canceled.",
         detail,
         NULL,
         ALLEGRO_MESSAGEBOX_WARN);
      return;
   }

   if (exported_count <= 0)
   {
      if (staging_path[0] != 0)
         upscale_remove_tree(staging_path);
      export_progress_end();
      al_show_native_message_box(a5_display,
         "Export Assets",
         "No PNGs were written.",
         "The matched assets did not produce any PNGs (decoder errors or empty input).",
         NULL,
         ALLEGRO_MESSAGEBOX_ERROR);
      return;
   }

   if (upscale_mode != UPSCALE_MODE_NONE)
   {
      const char *upscale_label = (upscale_mode == UPSCALE_MODE_4X)
         ? "Upscaling 4x..." : "Upscaling 2x...";
      export_progress_set_stage(EXPORT_STAGE_LOCAL_UPSCALE,
                                upscale_label, 0);

      if (!run_upscale_pipeline("Export Assets", staging_path,
                                output_path, upscale_mode))
      {
         /* On cancel during upscale, keep both staging and partial
          * output_path per the locked Q3 multi-stage cancel rule. */
         if (export_progress_cancel_requested())
         {
            char detail[512];
            snprintf(detail, sizeof(detail),
               "Canceled during upscale. Native PNGs are at %s; partially "
               "upscaled files are at %s.", staging_path, output_path);
            export_progress_end();
            al_show_native_message_box(a5_display,
               "Export Assets",
               "Canceled.",
               detail,
               NULL,
               ALLEGRO_MESSAGEBOX_WARN);
            return;
         }
         upscale_remove_tree(staging_path);
         export_progress_end();
         return;
      }
      upscale_remove_tree(staging_path);
   }

   export_progress_end();

   show_export_result(
      "Export Assets",
      "Exported %d PNG(s).",
      "No PNGs were exported.",
      exported_count,
      output_path);
}


/* ---- public entry points ---- */

void project_menu_handle_shortcuts(void)
{
   int ctrl, shift;

   ctrl  = key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL);
   shift = key_pressed(KEY_LSHIFT)   || key_pressed(KEY_RSHIFT);

   if (!ctrl || !shift) return;

   if (key_pressed(KEY_N))
   {
      wait_release(KEY_N, KEY_LCONTROL, KEY_LSHIFT);
      action_new_project();
   }
   else if (key_pressed(KEY_O))
   {
      wait_release(KEY_O, KEY_LCONTROL, KEY_LSHIFT);
      action_open_project();
   }
   else if (key_pressed(KEY_W))
   {
      wait_release(KEY_W, KEY_LCONTROL, KEY_LSHIFT);
      action_close_project();
   }
   else if (key_pressed(ALLEGRO_KEY_A))
   {
      wait_release(ALLEGRO_KEY_A, KEY_LCONTROL, KEY_LSHIFT);
      action_export_unified();
   }
}

