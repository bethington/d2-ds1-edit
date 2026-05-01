#include <string.h>

#include "core/area_metadata.h"


int area_name_parse_act(const char * name)
{
   int act;

   if (name == NULL || name[0] == '\0')
      return 0;
   if (strncmp(name, "Act ", 4) != 0)
      return 0;

   act = name[4] - '0';
   if (act < 1 || act > 5)
      return 0;
   if (name[5] != ' ' || name[6] != '-' || name[7] != ' ')
      return 0;

   return act;
}


int area_name_has_act_mismatch(int txt_act, const char * name)
{
   int name_act;

   name_act = area_name_parse_act(name);
   if (name_act <= 0)
      return 0;
   if (txt_act < 1 || txt_act > 5)
      return 0;

   return name_act != txt_act;
}


int area_group_resolve_act(int txt_act, const char * name)
{
   if (txt_act >= 1 && txt_act <= 5)
      return txt_act;

   return area_name_parse_act(name);
}


const char * area_name_strip_prefix(const char * name)
{
   if (area_name_parse_act(name) > 0)
      return name + 8;

   return name;
}