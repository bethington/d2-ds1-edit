#include <stdio.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_native_dialog.h>
#include "ui/message_box.h"

#include "structs.h"
#include "ui/compat.h"
#include "ui/dialogs.h"
#include "ui/input.h"
#include "ui/map_save_ui.h"
#include "ui/project_menu.h"
#include "core/area_browser.h"
#include "core/ds1_save_io.h"
#include "core/map_state.h"
#include "core/project.h"

int map_save_ui_require_project(void)
{
   if (glb_project.is_open)
      return 1;

   switch (msg_project_required_main())
   {
   case 0:  return project_menu_new_project();
   case 1:  return project_menu_open_project();
   default: return 0;
   }
}

/* Write one map, with no project question and no browser refresh. A real save
 * that would replace the project's own copy asks first. */
static int save_one(int ds1_idx, int is_tmp_file)
{
   char error[768];

   /* An autosave stays allowed: it only ever writes a .tmp. */
   if (!is_tmp_file && glb_ds1[ds1_idx].detached)
   {
      snprintf(error, sizeof(error), "%s was removed from the project, so it wasn't saved. "
               "Reopen it, or use Restore from Backup.", glb_ds1[ds1_idx].name);
      ds1_message_box(a5_display, "DS1Edit", "The map was not saved",
                                 error, NULL, ALLEGRO_MESSAGEBOX_ERROR);
      return -1;
   }

   if (!is_tmp_file && project_save_would_replace(glb_ds1[ds1_idx].name) &&
       msg_replace_project_copy_main() != 0)
      return -1;

   if (ds1_save_checked(ds1_idx, is_tmp_file, error, sizeof(error)) == 0)
      return 0;

   ds1_message_box(a5_display, "DS1Edit",
      is_tmp_file ? "The autosave (.tmp) was not written"
                  : "The map was not saved",
      error, NULL, ALLEGRO_MESSAGEBOX_ERROR);
   return -1;
}

int map_save_ui_save(int ds1_idx, int is_tmp_file)
{
   if (!is_tmp_file && glb_ds1[ds1_idx].name[0] != 0 && !glb_ds1[ds1_idx].detached &&
       !map_save_ui_require_project())
      return -1;

   if (save_one(ds1_idx, is_tmp_file) != 0)
      return -1;
   if (!is_tmp_file)
      area_browser_refresh_states();
   return 0;
}

int map_save_ui_save_dirty(void)
{
   int i, rc = 0, saved = 0;

   if (!map_any_dirty())
      return 0;
   if (!map_save_ui_require_project())
      return -1;

   for (i = 0; i < DS1_MAX; i++)
   {
      if (!map_is_dirty(i))
         continue;
      if (save_one(i, FALSE) == 0)
         saved = 1;
      else
         rc = -1;
   }
   if (saved)
      area_browser_refresh_states();
   return rc;
}

int map_save_ui_resolve_unsaved(const char *what)
{
   if (!map_any_dirty())
      return 1;

   switch (msg_unsaved_main(what))
   {
   case 0:  return map_save_ui_save_dirty() == 0;
   case 1:  return 1;
   default: return 0;
   }
}

int map_save_ui_unload_guard(void)
{
   return map_save_ui_resolve_unsaved(
      "\nOpening this map closes the open maps, and they have unsaved edits.");
}
