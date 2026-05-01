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
#include "ui/export_type_picker.h"
#include "ui/project_menu.h"
#include "ui/upscale_mode_picker.h"
#include "ui/win_folder_picker.h"

#include "core/asset_export.h"
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

static void action_export_area_assets(void)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S *group;
   char path[PROJECT_PATH_MAX];
   char staging_path[PROJECT_PATH_MAX];
   const char *export_path;
   int pal_idx;
   int exported_count;
   char message[512];
   int upscale_mode;

   if (ab->selected_group < 0 || ab->selected_group >= ab->group_count)
   {
      al_show_native_message_box(a5_display,
         "Export Area Assets",
         "No area is selected.",
         "Select an area in the sidebar first.",
         NULL,
         ALLEGRO_MESSAGEBOX_WARN);
      return;
   }

   group = &ab->groups[ab->selected_group];
   if (!pick_folder("Export Area Assets - choose an output folder",
                    glb_project.is_open ? glb_project.path : NULL,
                    path, sizeof(path)))
      return;
   if (!confirm_overwrite_output("Export Area Assets", path))
      return;

   upscale_mode = choose_upscale_mode("Export Area Assets - choose upscale mode");
   if (upscale_mode < 0)
      return;

   export_path = path;
   staging_path[0] = 0;
   if (upscale_mode != UPSCALE_MODE_NONE)
   {
      if (!upscale_create_temp_dir(staging_path, sizeof(staging_path)))
      {
         al_show_native_message_box(a5_display,
            "Export Area Assets",
            "Failed to prepare temporary export staging.",
            NULL,
            NULL,
            ALLEGRO_MESSAGEBOX_ERROR);
         return;
      }
      export_path = staging_path;
   }

   pal_idx = palette_resolve_index(group->act, 0);
   a5_current_palette = &glb_ds1edit.vga_pal[pal_idx];

   exported_count = asset_export_area_group_png(group, export_path);
   if (exported_count <= 0)
   {
      if (staging_path[0] != 0)
         upscale_remove_tree(staging_path);
      al_show_native_message_box(a5_display,
         "Export Area Assets",
         "No PNGs were exported.",
         "The selected area does not currently resolve to exportable supported assets.",
         NULL,
         ALLEGRO_MESSAGEBOX_ERROR);
      return;
   }

    if (upscale_mode != UPSCALE_MODE_NONE)
    {
       if (!run_upscale_pipeline("Export Area Assets", staging_path, path, upscale_mode))
       {
          upscale_remove_tree(staging_path);
          return;
       }
       upscale_remove_tree(staging_path);
    }

   snprintf(message, sizeof(message),
      "Exported %d PNG(s) for area \"%s\".",
      exported_count,
      group->name);
   al_show_native_message_box(a5_display,
      "Export Area Assets",
      message,
      path,
      NULL,
      0);
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

   if (upscale_directory_remote(staging_path, output_path, scale,
                                "realesrgan", error, sizeof(error)))
      return 1;

   rc = al_show_native_message_box(a5_display,
      title,
      "Remote upscale failed.",
      "Would you like to fall back to the built-in local 2x/4x upscaler?",
      error[0] ? error : NULL,
      ALLEGRO_MESSAGEBOX_YES_NO | ALLEGRO_MESSAGEBOX_WARN);
   if (rc != 1)
      return 0;

   error[0] = 0;
   if (upscale_directory_local(staging_path, output_path, scale,
                               error, sizeof(error)))
      return 1;

   al_show_native_message_box(a5_display,
      title,
      "Upscale failed.",
      error[0] ? error : "The local fallback upscaler was unable to generate output.",
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

static void action_export_folder_assets(void)
{
   char source_path[PROJECT_PATH_MAX];
   char output_path[PROJECT_PATH_MAX];
   char staging_path[PROJECT_PATH_MAX];
   char asset_prefix[PROJECT_PATH_MAX];
   const char *export_path;
   int exported_count;
   int upscale_mode;

   if (glb_config.mod_dir[0] == NULL || glb_config.mod_dir[0][0] == 0)
   {
      al_show_native_message_box(a5_display,
         "Export Folder Assets",
         "No mod overlay is configured.",
         "Set a mod directory first so a folder can be mapped to a virtual asset prefix.",
         NULL,
         ALLEGRO_MESSAGEBOX_WARN);
      return;
   }

   if (!pick_folder("Export Folder Assets - choose a source folder under the mod overlay",
                    glb_config.mod_dir[0],
                    source_path, sizeof(source_path)))
      return;

   if (!make_virtual_prefix_from_mod_path(source_path, asset_prefix, sizeof(asset_prefix)))
   {
      al_show_native_message_box(a5_display,
         "Export Folder Assets",
         "The selected folder is outside the current mod overlay.",
         glb_config.mod_dir[0],
         NULL,
         ALLEGRO_MESSAGEBOX_ERROR);
      return;
   }

   if (!pick_folder("Export Folder Assets - choose an output folder",
                    glb_project.is_open ? glb_project.path : NULL,
                    output_path, sizeof(output_path)))
      return;
   if (!confirm_overwrite_output("Export Folder Assets", output_path))
      return;

   upscale_mode = choose_upscale_mode("Export Folder Assets - choose upscale mode");
   if (upscale_mode < 0)
      return;

   export_path = output_path;
   staging_path[0] = 0;
   if (upscale_mode != UPSCALE_MODE_NONE)
   {
      if (!upscale_create_temp_dir(staging_path, sizeof(staging_path)))
      {
         al_show_native_message_box(a5_display,
            "Export Folder Assets",
            "Failed to prepare temporary export staging.",
            NULL,
            NULL,
            ALLEGRO_MESSAGEBOX_ERROR);
         return;
      }
      export_path = staging_path;
   }

   ensure_export_palette_ready();

   exported_count = asset_export_prefix_png(asset_prefix, "all", export_path);
   if (exported_count > 0 && upscale_mode != UPSCALE_MODE_NONE)
   {
      if (!run_upscale_pipeline("Export Folder Assets", staging_path, output_path, upscale_mode))
      {
         upscale_remove_tree(staging_path);
         return;
      }
      upscale_remove_tree(staging_path);
   }
   else if (staging_path[0] != 0)
   {
      upscale_remove_tree(staging_path);
   }
   show_export_result(
      "Export Folder Assets",
      "Exported %d PNG(s) from the selected folder.",
      "This folder currently has no discoverable DT1, DC6, or DCC assets under the configured overlay/index.",
      exported_count,
      output_path);
}

static void action_export_folder_assets_of_type(void)
{
   char source_path[PROJECT_PATH_MAX];
   char output_path[PROJECT_PATH_MAX];
   char staging_path[PROJECT_PATH_MAX];
   char asset_prefix[PROJECT_PATH_MAX];
   const char *type_filter;
   const char *export_path;
   int exported_count;
   int upscale_mode;

   if (glb_config.mod_dir[0] == NULL || glb_config.mod_dir[0][0] == 0)
   {
      al_show_native_message_box(a5_display,
         "Export Folder Assets By Type",
         "No mod overlay is configured.",
         "Set a mod directory first so a folder can be mapped to a virtual asset prefix.",
         NULL,
         ALLEGRO_MESSAGEBOX_WARN);
      return;
   }

   type_filter = choose_export_type("Choose Asset Type");
   if (type_filter == NULL)
      return;

   if (!pick_folder("Export Folder Assets By Type - choose a source folder under the mod overlay",
                    glb_config.mod_dir[0],
                    source_path, sizeof(source_path)))
      return;

   if (!make_virtual_prefix_from_mod_path(source_path, asset_prefix, sizeof(asset_prefix)))
   {
      al_show_native_message_box(a5_display,
         "Export Folder Assets By Type",
         "The selected folder is outside the current mod overlay.",
         glb_config.mod_dir[0],
         NULL,
         ALLEGRO_MESSAGEBOX_ERROR);
      return;
   }

   if (!pick_folder("Export Folder Assets By Type - choose an output folder",
                    glb_project.is_open ? glb_project.path : NULL,
                    output_path, sizeof(output_path)))
      return;
   if (!confirm_overwrite_output("Export Folder Assets By Type", output_path))
      return;

   upscale_mode = choose_upscale_mode("Export Folder Assets By Type - choose upscale mode");
   if (upscale_mode < 0)
      return;

   export_path = output_path;
   staging_path[0] = 0;
   if (upscale_mode != UPSCALE_MODE_NONE)
   {
      if (!upscale_create_temp_dir(staging_path, sizeof(staging_path)))
      {
         al_show_native_message_box(a5_display,
            "Export Folder Assets By Type",
            "Failed to prepare temporary export staging.",
            NULL,
            NULL,
            ALLEGRO_MESSAGEBOX_ERROR);
         return;
      }
      export_path = staging_path;
   }

   ensure_export_palette_ready();
   exported_count = asset_export_prefix_png(asset_prefix, type_filter, export_path);
   if (exported_count > 0 && upscale_mode != UPSCALE_MODE_NONE)
   {
      if (!run_upscale_pipeline("Export Folder Assets By Type", staging_path, output_path, upscale_mode))
      {
         upscale_remove_tree(staging_path);
         return;
      }
      upscale_remove_tree(staging_path);
   }
   else if (staging_path[0] != 0)
   {
      upscale_remove_tree(staging_path);
   }
   show_export_result(
      "Export Folder Assets By Type",
      "Exported %d PNG(s) from the selected folder and asset type.",
      "This folder currently has no discoverable assets of the selected type under the configured overlay/index.",
      exported_count,
      output_path);
}

static void action_export_all_assets(void)
{
   char output_path[PROJECT_PATH_MAX];
   char staging_path[PROJECT_PATH_MAX];
   const char *type_filter;
   const char *export_path;
   int exported_count;
   int upscale_mode;

   type_filter = choose_export_type("Export All Assets");
   if (type_filter == NULL)
      return;

   if (!pick_folder("Export All Assets - choose an output folder",
                    glb_project.is_open ? glb_project.path : NULL,
                    output_path, sizeof(output_path)))
      return;
   if (!confirm_overwrite_output("Export All Assets", output_path))
      return;

   upscale_mode = choose_upscale_mode("Export All Assets - choose upscale mode");
   if (upscale_mode < 0)
      return;

   export_path = output_path;
   staging_path[0] = 0;
   if (upscale_mode != UPSCALE_MODE_NONE)
   {
      if (!upscale_create_temp_dir(staging_path, sizeof(staging_path)))
      {
         al_show_native_message_box(a5_display,
            "Export All Assets",
            "Failed to prepare temporary export staging.",
            NULL,
            NULL,
            ALLEGRO_MESSAGEBOX_ERROR);
         return;
      }
      export_path = staging_path;
   }

   ensure_export_palette_ready();
   exported_count = asset_export_all_png(type_filter, export_path);
   if (exported_count > 0 && upscale_mode != UPSCALE_MODE_NONE)
   {
      if (!run_upscale_pipeline("Export All Assets", staging_path, output_path, upscale_mode))
      {
         upscale_remove_tree(staging_path);
         return;
      }
      upscale_remove_tree(staging_path);
   }
   else if (staging_path[0] != 0)
   {
      upscale_remove_tree(staging_path);
   }
   show_export_result(
      "Export All Assets",
      "Exported %d PNG(s) from all indexed assets.",
      "No indexed DT1, DC6, or DCC assets matched the selected type.",
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
   else if (key_pressed(ALLEGRO_KEY_E))
   {
      wait_release(ALLEGRO_KEY_E, KEY_LCONTROL, KEY_LSHIFT);
      action_export_area_assets();
   }
   else if (key_pressed(ALLEGRO_KEY_R))
   {
      wait_release(ALLEGRO_KEY_R, KEY_LCONTROL, KEY_LSHIFT);
      action_export_folder_assets();
   }
   else if (key_pressed(ALLEGRO_KEY_T))
   {
      wait_release(ALLEGRO_KEY_T, KEY_LCONTROL, KEY_LSHIFT);
      action_export_folder_assets_of_type();
   }
   else if (key_pressed(ALLEGRO_KEY_A))
   {
      wait_release(ALLEGRO_KEY_A, KEY_LCONTROL, KEY_LSHIFT);
      action_export_all_assets();
   }
}

