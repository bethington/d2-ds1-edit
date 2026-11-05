#include <string.h>
#include "ui/props_panel_rules.h"

const AREA_DS1_ENTRY_S * props_panel_open_row(const AREA_BROWSER_S * ab)
{
   int gi = ab->loaded_group;
   int ei = ab->loaded_entry;

   if (gi < 0 || gi >= ab->group_count)
      return NULL;
   if (ei < 0 || ei >= ab->groups[gi].entry_count)
      return NULL;
   return &ab->groups[gi].entries[ei];
}

int props_panel_header_field_read_only(int header_field)
{
   switch (header_field)
   {
      case 1:  /* version */
      case 4:  /* act */
      case 6:  /* tag_type */
         return 0;
      default:
         return 1;
   }
}

void props_panel_follow_key_edit(AREA_BROWSER_S * ab, PP_FIELD_SOURCE_E source,
                                 const char * col_name, int old_value, int new_value)
{
   int is_def = source == PFS_LVLPREST && strcmp(col_name, "Def") == 0;
   int is_type = source == PFS_LVLTYPES && strcmp(col_name, "Id") == 0;
   int g, e;

   if (!is_def && !is_type)
      return;

   for (g = 0; g < ab->group_count; g++)
   {
      AREA_GROUP_S * group = &ab->groups[g];

      if (is_type && group->lvltype_id == old_value)
         group->lvltype_id = new_value;
      for (e = 0; e < group->entry_count; e++)
      {
         AREA_DS1_ENTRY_S * row = &group->entries[e];

         if (is_def && row->lvlprest_def == old_value)
            row->lvlprest_def = new_value;
         if (is_type && row->lvltype_id == old_value)
            row->lvltype_id = new_value;
      }
   }
}

int pp_scroll_clamp(int offset, int total, int visible)
{
   int limit = total - visible;

   if (offset > limit) offset = limit;
   return offset < 0 ? 0 : offset;
}
