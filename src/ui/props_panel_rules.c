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
