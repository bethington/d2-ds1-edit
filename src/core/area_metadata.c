#include <stddef.h>
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


int area_preset_resolve_act(int level_id, int levels_act, const char * preset_name)
{
   /* Levels.Act is 0-based; LevelId 0 is the Null placeholder. */
   if (level_id > 0 && levels_act >= 0 && levels_act <= 4)
      return levels_act + 1;

   return area_name_parse_act(preset_name);
}


/* Palette act for a DS1: LvlTypes first, then the LvlPrest preset, then the
 * browser group's folder-derived act as a last resort. 0 = nothing valid. */
int area_resolve_txt_act(int lvltypes_act, int preset_act, int folder_act)
{
   if (lvltypes_act >= 1 && lvltypes_act <= 5)
      return lvltypes_act;
   if (preset_act >= 1 && preset_act <= 5)
      return preset_act;
   if (folder_act >= 1 && folder_act <= 5)
      return folder_act;

   return 0;
}


const char * area_name_strip_prefix(const char * name)
{
   if (area_name_parse_act(name) > 0)
      return name + 8;

   return name;
}


/* LvlPrest rows whose name does not start with their area's LvlTypes name, for
 * rows with LevelId 0, which is nearly all of them. Each tileset was checked
 * against a row of the same family that has a real level, or against the DT1
 * files that LvlType loads. */
static const struct { const char * prefix; const char * area; } AREA_PRESET_RULES[] =
{
   { "Act 1 - Andariel",  "Act 1 - Catacombs"  },
   { "Act 1 - Bivouac",   "Act 1 - Wilderness" },
   { "Act 1 - Bridge",    "Act 1 - Wilderness" },
   { "Act 1 - Cairn",     "Act 1 - Wilderness" },
   { "Act 1 - Camp",      "Act 1 - Wilderness" },
   { "Act 1 - Corral",    "Act 1 - Wilderness" },
   { "Act 1 - Cottages",  "Act 1 - Wilderness" },
   { "Act 1 - DOE",       "Act 1 - Wilderness" },
   { "Act 1 - Fallen",    "Act 1 - Wilderness" },
   { "Act 1 - Fence",     "Act 1 - Wilderness" },
   { "Act 1 - Graveyard", "Act 1 - Wilderness" },
   { "Act 1 - Inifus",    "Act 1 - Wilderness" },
   { "Act 1 - Pond",      "Act 1 - Wilderness" },
   { "Act 1 - River",     "Act 1 - Wilderness" },
   { "Act 1 - Ruin",      "Act 1 - Wilderness" },
   { "Act 1 - Stone",     "Act 1 - Wilderness" },
   { "Act 1 - Swamp",     "Act 1 - Wilderness" },
   { "Act 1 - Tower",     "Act 1 - Wilderness" },
   { "Act 1 - Tree",      "Act 1 - Wilderness" },
   { "Act 2 - Corrupt",   "Act 2 - Harem"      },
   { "Act 3 - Bridge",    "Act 3 - Kurast"     },
   { "Act 3 - Burbs",     "Act 3 - Kurast"     },
   { "Act 3 - Mephisto",  "Act 3 - Kurast"     },
   { "Act 3 - Metro",     "Act 3 - Kurast"     },
   { "Act 3 - Slums",     "Act 3 - Kurast"     },
   { "Act 3 - Travincal", "Act 3 - Kurast"     },
   { "Act 3 - Clearing",  "Act 3 - Jungle"     },
   { "Act 4 - Bridge",    "Act 4 - Lava"       },
   { "Act 4 - Diablo",    "Act 4 - Lava"       },
   { "Act 4 - Fortress",  "Act 4 - Town"       },
   { "Act 4 - Pits",      "Act 4 - Mesa"       }
};


const char * area_preset_area_name(const char * preset_name)
{
   size_t i;

   if (preset_name == NULL)
      return NULL;

   for (i = 0; i < sizeof(AREA_PRESET_RULES) / sizeof(AREA_PRESET_RULES[0]); i++)
   {
      const char * prefix = AREA_PRESET_RULES[i].prefix;
      if (strncmp(preset_name, prefix, strlen(prefix)) == 0)
         return AREA_PRESET_RULES[i].area;
   }
   return NULL;
}


/* Does a tiles-relative path start with this lowercase backslashed folder? */
static int area_path_in_folder(const char * path, const char * folder)
{
   for (; *folder != '\0'; path++, folder++)
   {
      char c = *path;
      if (c == '/') c = '\\';
      if (c >= 'A' && c <= 'Z') c = (char) (c - 'A' + 'a');
      if (c != *folder)
         return 0;
   }
   return 1;
}


/* Group for a Project Diablo 2 map that no LvlPrest row names. */
const char * area_pd2_folder_group(const char * rel_path)
{
   if (rel_path == NULL)
      return "Other";
   if (area_path_in_folder(rel_path, "expansion\\map\\"))
      return "Unnamed Maps";
   if (area_path_in_folder(rel_path, "act2\\arcane\\"))
      return "Arcane Variants";
   if (area_path_in_folder(rel_path, "guild\\"))
      return "Guilds";
   return "Other";
}


int area_map_destination(int is_pd2_only, int lvltype)
{
   if (is_pd2_only)
      return AREA_DEST_PD2;
   return lvltype > 0 ? AREA_DEST_AREA : AREA_DEST_UNLISTED;
}


/* Panel order: Act 1-5, then Project Diablo 2, then Other. */
int area_section_key(int is_pd2, int act)
{
   if (is_pd2)
      return 6;
   return (act >= 1 && act <= 5) ? act : 7;
}
