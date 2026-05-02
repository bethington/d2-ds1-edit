#include <stdio.h>
#include <string.h>

#include "core/compose_presets.h"
#include "ui/compose_preset_picker.h"
#include "ui/multi_select_picker.h"

#ifdef _WIN32
#define preset_picker_stricmp _stricmp
#else
#include <strings.h>
#define preset_picker_stricmp strcasecmp
#endif

static int already_in_selection(const COMPOSE_PRESET_SELECTION_S *sel,
                                const char *code)
{
   int i;
   if (code == NULL || code[0] == 0) return 1;
   for (i = 0; i < sel->code_count; i++)
   {
      if (preset_picker_stricmp(sel->codes[i], code) == 0)
         return 1;
   }
   return 0;
}

static void selection_add(COMPOSE_PRESET_SELECTION_S *sel, const char *code)
{
   if (sel == NULL || code == NULL) return;
   if (sel->code_count >= COMPOSE_SELECTED_CODES_MAX) return;
   if (already_in_selection(sel, code)) return;
   strncpy(sel->codes[sel->code_count], code, COMPOSE_PRESET_CODE_MAX - 1);
   sel->codes[sel->code_count][COMPOSE_PRESET_CODE_MAX - 1] = 0;
   sel->code_count++;
}

int compose_preset_picker_show(const char *title,
                               const char *all_label,
                               int preset_count,
                               const COMPOSE_PRESET_S *(*preset_at)(int),
                               COMPOSE_PRESET_SELECTION_S *out)
{
   MULTI_SELECT_ITEM_S items[MULTI_SELECT_MAX_ITEMS];
   int item_count;
   int total_presets;
   int i;

   if (out == NULL || preset_at == NULL)
      return 0;

   memset(out, 0, sizeof(*out));
   memset(items, 0, sizeof(items));

   /* item 0 = synthetic "All" toggle row */
   strncpy(items[0].label,
           all_label != NULL ? all_label : "All",
           MULTI_SELECT_LABEL_MAX - 1);
   items[0].is_all_toggle = 1;
   items[0].selected      = 1;  /* default to All-checked: graceful start */
   item_count = 1;

   /* Cap presets so item_count stays within MULTI_SELECT_MAX_ITEMS. */
   total_presets = preset_count;
   if (total_presets < 0) total_presets = 0;
   if (total_presets + 1 > MULTI_SELECT_MAX_ITEMS)
      total_presets = MULTI_SELECT_MAX_ITEMS - 1;

   for (i = 0; i < total_presets; i++)
   {
      const COMPOSE_PRESET_S *p = preset_at(i);
      if (p == NULL) continue;
      snprintf(items[item_count].label, MULTI_SELECT_LABEL_MAX,
               "%s", p->name);
      items[item_count].is_all_toggle = 0;
      items[item_count].selected      = 1;  /* default-checked for the All-on start */
      item_count++;
   }

   if (!multi_select_picker_show(title, items, item_count))
      return 0;

   /* Translate the selection into the COMPOSE_PRESET_SELECTION_S form. */
   if (items[0].selected)
   {
      out->use_all = 1;
      out->code_count = 0;
      return 1;
   }

   /* Specific presets selected: union their codes. */
   out->use_all = 0;
   for (i = 0; i < total_presets; i++)
   {
      int item_idx = i + 1;
      const COMPOSE_PRESET_S *p;
      int c;

      if (item_idx >= item_count) break;
      if (!items[item_idx].selected) continue;

      p = preset_at(i);
      if (p == NULL) continue;
      for (c = 0; c < p->code_count; c++)
         selection_add(out, p->codes[c]);
   }

   /* If nothing was selected after de-toggling All, treat it as
    * effectively use_all (graceful fallback rather than a confusing
    * empty export). */
   if (out->code_count == 0)
      out->use_all = 1;

   return 1;
}

int compose_mode_picker_show(COMPOSE_PRESET_SELECTION_S *out)
{
   return compose_preset_picker_show(
      "Compose - Choose Mode Presets",
      "All modes",
      compose_mode_presets_count(),
      compose_mode_presets_at,
      out);
}

int compose_weapon_picker_show(COMPOSE_PRESET_SELECTION_S *out)
{
   return compose_preset_picker_show(
      "Compose - Choose Weapon Presets",
      "All weapons",
      compose_weapon_presets_count(),
      compose_weapon_presets_at,
      out);
}
