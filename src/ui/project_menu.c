#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_native_dialog.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>

#include "structs.h"
#include "ui/compat.h"
#include "ui/project_menu.h"

#include "core/project.h"
#include "core/preferences.h"

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
static int pick_folder(const char *title, const char *initial,
                       char *out, int out_cap)
{
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
}


/* ---- actions ---- */

static void action_new_project(void)
{
   char path[PROJECT_PATH_MAX];
   char ini[PROJECT_PATH_MAX];
   const char *install;
   const char *name;
   FILE *fp;

   if (!pick_folder("New Project — pick or create a folder",
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
}

void project_menu_draw_indicator(ALLEGRO_BITMAP *target)
{
   char label[PROJECT_NAME_MAX + 32];
   int  text_w;
   int  pad = 4;

   if (!glb_project.is_open || target == NULL) return;

   snprintf(label, sizeof(label), "Project: %s",
            glb_project.name[0] ? glb_project.name : "<unnamed>");

   ALLEGRO_BITMAP *prev = al_get_target_bitmap();
   al_set_target_bitmap(target);

   text_w = al_get_text_width(a5_font, label);

   al_draw_filled_rectangle(
      0, 0,
      (float)(text_w + pad * 2), (float)(al_get_font_line_height(a5_font) + pad * 2),
      al_map_rgba(0, 0, 0, 180));
   al_draw_text(a5_font,
      al_map_rgb(180, 220, 255),
      (float)pad, (float)pad, 0, label);

   if (prev) al_set_target_bitmap(prev);
}
