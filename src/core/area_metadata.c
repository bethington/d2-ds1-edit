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


/* Palette act for a map with no LvlTypes row. A file that names a later act
 * means it: expansion\\lavaX.ds1 holds Act 4 tiles in a folder filed under Act 5.
 * Act 1 is what files say when nothing set it, so the folder outranks that.
 * 0 = nothing valid. */
int area_tileless_palette_act(int ds1_act, int folder_act)
{
   if (ds1_act >= 2 && ds1_act <= 5)
      return ds1_act;
   if (folder_act >= 1 && folder_act <= 5)
      return folder_act;

   return ds1_act == 1 ? 1 : 0;
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
   { "Act 1 - Cave Entrance", "Act 1 - Wilderness" },
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
   { "Act 4 - Fortress Transition", "Act 4 - Mesa" },
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


/* Folders that no table's tileset owns. Guild maps are Act 1 outdoors whatever
 * their own Act field says; the arena is built from the Act 2 tomb. */
static const struct { const char * folder; const char * area; } AREA_FOLDER_RULES[] =
{
   { "guild\\cottages\\", "Act 1 - Wilderness" },
   { "guild\\house1\\",   "Act 1 - Wilderness" },
   { "guild\\house2\\",   "Act 1 - Wilderness" },
   { "arena\\tombs\\",    "Act 2 - Tomb"       }
};


const char * area_folder_area_name(const char * rel_path)
{
   size_t i;

   if (rel_path == NULL)
      return NULL;

   for (i = 0; i < sizeof(AREA_FOLDER_RULES) / sizeof(AREA_FOLDER_RULES[0]); i++)
   {
      if (area_path_in_folder(rel_path, AREA_FOLDER_RULES[i].folder))
         return AREA_FOLDER_RULES[i].area;
   }
   return NULL;
}


/* May a LvlPrest row whose name starts like this area's be filed under it? Its
 * level's tileset decides first, then an explicit rule; a name is only a name. */
int area_row_accepts_name_match(int level_type, const char * rule_area,
                                int group_lvltype, const char * group_area)
{
   if (level_type > 0)
      return level_type == group_lvltype;
   if (rule_area != NULL)
      return group_area != NULL && strcmp(rule_area, group_area) == 0;
   return 1;
}


static unsigned long area_le32(const unsigned char * p)
{
   return (unsigned long) p[0] | ((unsigned long) p[1] << 8)
        | ((unsigned long) p[2] << 16) | ((unsigned long) p[3] << 24);
}


/* The DT1 name table of a DS1, version 3 onward. *names points into buf at
 * count NUL-terminated strings. Returns 0 when the file has no complete table. */
int area_ds1_embedded_names(const unsigned char * buf, size_t len, int * act,
                            const char ** names, int * count)
{
   unsigned long version, stored;
   size_t pos = 12, p;

   if (buf == NULL || len < 12)
      return 0;

   version = area_le32(buf);
   if (version < 3 || version > 0x7FFFFFFFUL)
      return 0;

   *act = 1;
   if (version >= 8)
   {
      if (len < pos + 4)
         return 0;
      stored = area_le32(buf + pos);
      *act = stored >= 4 ? 5 : (int) stored + 1;
      pos += 4;
   }
   if (version >= 10)
      pos += 4;
   if (len < pos + 4)
      return 0;

   stored = area_le32(buf + pos);
   pos += 4;
   if (stored > 0x7FFFFFFFUL)
      return 0;

   p = pos;
   {
      unsigned long i;
      for (i = 0; i < stored; i++)
      {
         const unsigned char * nul = p < len
            ? (const unsigned char *) memchr(buf + p, 0, len - p) : NULL;
         if (nul == NULL)
            return 0;
         p = (size_t) (nul - buf) + 1;
      }
   }

   *names = (const char *) buf + pos;
   *count = (int) stored;
   return 1;
}


static char area_path_char(char c)
{
   if (c == '/') c = '\\';
   if (c >= 'A' && c <= 'Z') c = (char) (c - 'A' + 'a');
   return c;
}


/* Start of "folder\name" in a path, or NULL when it has no folder. *end_out is
 * where the name stops, before any extension: maps saved with .tg1 names load
 * the matching .dt1. */
static const char * area_dt1_tail(const char * path, const char ** end_out)
{
   const char * last = NULL, * prev = NULL, * p;

   for (p = path; *p != '\0'; p++)
   {
      if (*p == '/' || *p == '\\') { prev = last; last = p; }
   }
   if (last == NULL)
      return NULL;

   *end_out = p;
   for (p = last; *p != '\0'; p++)
   {
      if (*p == '.') *end_out = p;
   }
   return prev != NULL ? prev + 1 : path;
}


int area_dt1_names_match(const char * embedded, const char * lvltype_file)
{
   const char * a, * b, * a_end = NULL, * b_end = NULL;

   if (embedded == NULL || lvltype_file == NULL)
      return 0;

   a = area_dt1_tail(embedded, &a_end);
   b = area_dt1_tail(lvltype_file, &b_end);
   if (a == NULL || b == NULL || (a_end - a) != (b_end - b))
      return 0;

   for (; a < a_end; a++, b++)
   {
      if (area_path_char(*a) != area_path_char(*b))
         return 0;
   }
   return 1;
}


/* Percent of a map's DT1 names that this tileset's files hold, or -1 when the
 * map names nothing to judge by. The warp tiles are in nearly every map and
 * nearly every tileset, so they say nothing. */
int area_dt1_names_score(const char * names, int count,
                         const char * const * files, int file_count)
{
   int i, f, judged = 0, hits = 0;

   if (names == NULL)
      return -1;

   for (i = 0; i < count; i++, names += strlen(names) + 1)
   {
      if (area_dt1_names_match(names, "barracks\\warp"))
         continue;
      judged++;
      for (f = 0; f < file_count; f++)
      {
         if (area_dt1_names_match(names, files[f]))
         {
            hits++;
            break;
         }
      }
   }
   return judged > 0 ? hits * 100 / judged : -1;
}


/* The score that lets a map's names choose its tileset. A Project Diablo 2 map
 * mostly names the mod's own DT1s and stays in its own section whatever it is
 * given, so the nearest tileset beats opening it with none. */
int area_dt1_names_min_score(int is_pd2_only)
{
   return is_pd2_only ? 20 : AREA_DT1_NAMES_MIN_SCORE;
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
