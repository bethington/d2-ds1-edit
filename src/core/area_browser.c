#include "structs.h"
#include "error.h"
#include "misc.h"
#include "core/txtread.h"
#include "core/area_browser.h"

#define AREA_INIT_GROUPS    64
#define AREA_INIT_ENTRIES   32


/* ---- helpers ---- */

/* Parse act number from a name like "Act 3 - Jungle". Returns 1-5, or 0 if
 * the name doesn't match the "Act X - ..." pattern. */
static int area_parse_act(const char * name)
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

/* Strip "Act X - " prefix to get display name. Returns pointer into the
 * original string (no allocation). Returns the full name if no prefix. */
static const char * area_strip_prefix(const char * name)
{
   if (area_parse_act(name) > 0)
      return name + 8;
   return name;
}

/* Add a DS1 entry to a group, growing the array if needed. */
static void area_group_add_entry(AREA_GROUP_S * grp, int lvltype_id,
                                  int def, const char * ds1_path)
{
   AREA_DS1_ENTRY_S * e;

   if (ds1_path == NULL || ds1_path[0] == '\0' || ds1_path[0] == '0')
      return;

   if (grp->entry_count >= grp->entry_max)
   {
      int new_max = grp->entry_max > 0 ? grp->entry_max * 2 : AREA_INIT_ENTRIES;
      AREA_DS1_ENTRY_S * tmp = (AREA_DS1_ENTRY_S *)
         realloc(grp->entries, sizeof(AREA_DS1_ENTRY_S) * new_max);
      if (tmp == NULL) return;
      grp->entries = tmp;
      grp->entry_max = new_max;
   }

   e = &grp->entries[grp->entry_count];
   e->lvltype_id = lvltype_id;
   e->lvlprest_def = def;
   strncpy(e->ds1_path, ds1_path, sizeof(e->ds1_path) - 1);
   e->ds1_path[sizeof(e->ds1_path) - 1] = '\0';
   grp->entry_count++;
}

/* Find or create a group for the given LvlType. */
static AREA_GROUP_S * area_find_or_create_group(AREA_BROWSER_S * ab,
                                                 int lvltype_id,
                                                 const char * name)
{
   int i;
   AREA_GROUP_S * g;

   /* search existing */
   for (i = 0; i < ab->group_count; i++)
   {
      if (ab->groups[i].lvltype_id == lvltype_id)
         return &ab->groups[i];
   }

   /* grow array if needed */
   if (ab->group_count >= ab->group_max)
   {
      int new_max = ab->group_max > 0 ? ab->group_max * 2 : AREA_INIT_GROUPS;
      AREA_GROUP_S * tmp = (AREA_GROUP_S *)
         realloc(ab->groups, sizeof(AREA_GROUP_S) * new_max);
      if (tmp == NULL) return NULL;
      ab->groups = tmp;
      ab->group_max = new_max;
   }

   /* create new */
   g = &ab->groups[ab->group_count];
   memset(g, 0, sizeof(AREA_GROUP_S));
   g->lvltype_id = lvltype_id;
   g->act = area_parse_act(name);
   strncpy(g->name, area_strip_prefix(name), sizeof(g->name) - 1);
   g->name[sizeof(g->name) - 1] = '\0';
   ab->group_count++;
   return g;
}

/* qsort comparator: sort groups by act (1-5 first, 0 last), then by name. */
static int area_group_cmp(const void * a, const void * b)
{
   const AREA_GROUP_S * ga = (const AREA_GROUP_S *) a;
   const AREA_GROUP_S * gb = (const AREA_GROUP_S *) b;
   int act_a, act_b;

   /* act 0 ("Other") sorts last */
   act_a = ga->act > 0 ? ga->act : 100;
   act_b = gb->act > 0 ? gb->act : 100;

   if (act_a != act_b)
      return act_a - act_b;
   return stricmp(ga->name, gb->name);
}


/* ---- public API ---- */

/* Load all Excel tables needed for the area browser. Returns 0 on success. */
int area_browser_init(void)
{
   int rc;

   /* Load LvlTypes.txt (reuses cache if already loaded) */
   if (glb_ds1edit.lvltypes_buff == NULL)
   {
      /* Call with dummy params — we just want it loaded and cached.
       * read_lvltypes_txt needs ds1_idx and type, but if the buffer
       * is NULL it loads the file first. We pass 0,0 which won't match
       * any row, but the file gets cached. */
      read_lvltypes_txt(0, 0);
   }

   /* Load LvlPrest.txt */
   if (glb_ds1edit.lvlprest_buff == NULL)
   {
      read_lvlprest_txt(0, 0);
   }

   /* Load Levels.txt */
   rc = read_levels_txt();
   if (rc != 0)
   {
      printf("area_browser_init: failed to load Levels.txt\n");
      return -1;
   }

   area_browser_build();
   return 0;
}

/* Build the grouped area list from the loaded TXT data. */
void area_browser_build(void)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   TXT_S * lvltypes = glb_ds1edit.lvltypes_buff;
   TXT_S * lvlprest = glb_ds1edit.lvlprest_buff;
   int lt_row, lp_row;
   int lt_id_col, lt_name_col;
   int lp_name_col, lp_def_col;
   int lp_file_col[6];
   int f;
   char file_col_name[10];

   if (lvltypes == NULL || lvlprest == NULL)
      return;

   /* get column indices */
   lt_id_col   = misc_get_txt_column_num(RQ_LVLTYPE, "Id");
   lt_name_col = misc_get_txt_column_num(RQ_LVLTYPE, "Name");
   lp_name_col = misc_get_txt_column_num(RQ_LVLPREST, "Name");
   lp_def_col  = misc_get_txt_column_num(RQ_LVLPREST, "Def");
   for (f = 0; f < 6; f++)
   {
      sprintf(file_col_name, "File%d", f + 1);
      lp_file_col[f] = misc_get_txt_column_num(RQ_LVLPREST, file_col_name);
   }

   /* clear any previous data */
   area_browser_destroy();

   /* iterate all LvlType rows to create groups */
   for (lt_row = 0; lt_row < lvltypes->line_num; lt_row++)
   {
      char * lt_name;
      long * lt_id_ptr;
      long lt_id;
      int lt_name_len;
      AREA_GROUP_S * grp;

      lt_id_ptr = (long *)(lvltypes->data + (lt_row * lvltypes->line_size)
                   + lvltypes->col[lt_id_col].offset);
      lt_id = *lt_id_ptr;
      if (lt_id <= 0)
         continue;

      lt_name = lvltypes->data + (lt_row * lvltypes->line_size)
                + lvltypes->col[lt_name_col].offset;
      if (lt_name[0] == '\0')
         continue;

      grp = area_find_or_create_group(ab, (int)lt_id, lt_name);
      if (grp == NULL)
         continue;

      /* Find all LvlPrest rows that belong to this LvlType.
       *
       * Matching strategy: extract the act prefix ("Act X - ") and the first
       * keyword of the area name from the LvlType name. Then match LvlPrest
       * rows that share the same act prefix and start with that keyword.
       *
       * Examples:
       *   LvlType "Act 1 - Wilderness" → match "Act 1 - Wild"
       *   LvlType "Act 5 - Ice Caves"  → match "Act 5 - Ice"
       *   LvlType "Act 1 - Monestary"  → match "Act 1 - Mon"
       *   LvlType "Imperial Palace"    → match "Imperial Palace"
       */
      {
         const char * area_part;
         char match_prefix[80];
         int match_len;
         int act = area_parse_act(lt_name);

         if (act > 0)
         {
            /* Standard "Act X - AreaName" — use the act prefix plus the
             * first word of the area name (up to first space), minimum 3
             * chars. This handles abbreviated LvlPrest names like
             * "Act 1 - Wild Border" matching LvlType "Act 1 - Wilderness",
             * and "Act 1 - Mon Front" matching "Act 1 - Monestary". */
            int k;
            area_part = lt_name + 8; /* skip "Act X - " */
            /* Use first 4 characters of area name as match key.
             * 4 chars is enough to be unique within each act (e.g.,
             * "Cath" vs "Cata", "Barr" for Barracks/Barricade in
             * different acts) while still matching abbreviated LvlPrest
             * names (e.g., "Wild" matches "Wild Border"). */
            k = (int)strlen(area_part);
            if (k > 4) k = 4;
            sprintf(match_prefix, "Act %d - ", act);
            strncat(match_prefix, area_part, k);
         }
         else
         {
            /* Non-standard name — use full name as prefix */
            strncpy(match_prefix, lt_name, sizeof(match_prefix) - 1);
            match_prefix[sizeof(match_prefix) - 1] = '\0';
         }
         match_len = strlen(match_prefix);

         /* Also try a shorter 3-char prefix if the 4-char prefix might
          * miss abbreviated LvlPrest names (e.g., "Mone" misses "Mon Front").
          * We do a first pass with the 4-char prefix, and if we get 0 matches,
          * retry with 3 chars. For non-act names (like "Kurast"), also try
          * matching LvlPrest rows that contain the area name without act prefix. */
         for (lp_row = 0; lp_row < lvlprest->line_num; lp_row++)
         {
            char * lp_name;
            long * lp_def_ptr;
            long lp_def;

            lp_name = lvlprest->data + (lp_row * lvlprest->line_size)
                      + lvlprest->col[lp_name_col].offset;

            /* prefix match using the computed match key */
            if (strnicmp(lp_name, match_prefix, match_len) != 0)
               continue;

            lp_def_ptr = (long *)(lvlprest->data + (lp_row * lvlprest->line_size)
                         + lvlprest->col[lp_def_col].offset);
            lp_def = *lp_def_ptr;
            if (lp_def <= 0)
               continue;

            /* add each non-empty File1-6 as an entry */
            for (f = 0; f < 6; f++)
            {
               char * ds1_path = lvlprest->data + (lp_row * lvlprest->line_size)
                                 + lvlprest->col[lp_file_col[f]].offset;
               area_group_add_entry(grp, (int)lt_id, (int)lp_def, ds1_path);
            }
         }
      } /* end match_prefix block */
   }

   /* Retry pass: for groups with 0 entries, try shorter 3-char prefix.
    * This handles cases like "Monestary" where LvlPrest uses "Mon Front". */
   {
      int gi;
      for (gi = 0; gi < ab->group_count; gi++)
      {
         AREA_GROUP_S * grp = &ab->groups[gi];
         if (grp->entry_count > 0 || grp->act == 0)
            continue;

         /* Try 3-char prefix */
         {
            char short_prefix[80];
            const char * ap = grp->name; /* already stripped of "Act X - " */
            int slen = (int)strlen(ap);
            int sk = slen > 3 ? 3 : slen;
            int sm;

            sprintf(short_prefix, "Act %d - ", grp->act);
            strncat(short_prefix, ap, sk);
            sm = strlen(short_prefix);

            for (lp_row = 0; lp_row < lvlprest->line_num; lp_row++)
            {
               char * lp_name;
               long * lp_def_ptr;
               long lp_def;

               lp_name = lvlprest->data + (lp_row * lvlprest->line_size)
                         + lvlprest->col[lp_name_col].offset;
               if (strnicmp(lp_name, short_prefix, sm) != 0)
                  continue;

               lp_def_ptr = (long *)(lvlprest->data + (lp_row * lvlprest->line_size)
                            + lvlprest->col[lp_def_col].offset);
               lp_def = *lp_def_ptr;
               if (lp_def <= 0)
                  continue;

               for (f = 0; f < 6; f++)
               {
                  char * ds1_path = lvlprest->data + (lp_row * lvlprest->line_size)
                                    + lvlprest->col[lp_file_col[f]].offset;
                  area_group_add_entry(grp, grp->lvltype_id, (int)lp_def, ds1_path);
               }
            }
         }

         /* If still 0, try matching LvlPrest rows by area name without act prefix.
          * Handles "Kurast" in LvlPrest matching LvlType "Act 3 - Kurast". */
         if (grp->entry_count == 0)
         {
            const char * ap = grp->name; /* already stripped */
            int alen = (int)strlen(ap);


            if (alen >= 3)
            {
               for (lp_row = 0; lp_row < lvlprest->line_num; lp_row++)
               {
                  char * lp_name;
                  long * lp_def_ptr;
                  long lp_def;

                  lp_name = lvlprest->data + (lp_row * lvlprest->line_size)
                            + lvlprest->col[lp_name_col].offset;
                  if (strnicmp(lp_name, ap, alen) != 0)
                     continue;

                  lp_def_ptr = (long *)(lvlprest->data + (lp_row * lvlprest->line_size)
                               + lvlprest->col[lp_def_col].offset);
                  lp_def = *lp_def_ptr;
                  if (lp_def <= 0)
                     continue;

                  for (f = 0; f < 6; f++)
                  {
                     char * ds1_path = lvlprest->data + (lp_row * lvlprest->line_size)
                                       + lvlprest->col[lp_file_col[f]].offset;
                     area_group_add_entry(grp, grp->lvltype_id, (int)lp_def, ds1_path);
                  }
               }
            }
         }
      }
   }

   /* sort groups: by act then alphabetically */
   if (ab->group_count > 1)
      qsort(ab->groups, ab->group_count, sizeof(AREA_GROUP_S), area_group_cmp);

   /* debug output */
   {
      int i;
      printf("\n[area browser] %d groups built:\n", ab->group_count);
      for (i = 0; i < ab->group_count; i++)
      {
         AREA_GROUP_S * g = &ab->groups[i];
         if (g->act > 0)
            printf("  Act %d - %-25s (%d maps, LvlType %d)\n",
                   g->act, g->name, g->entry_count, g->lvltype_id);
         else
            printf("  Other - %-25s (%d maps, LvlType %d)\n",
                   g->name, g->entry_count, g->lvltype_id);
      }
      printf("\n");
      fflush(stdout);
   }
}

/* Print all available areas to stdout. */
void area_browser_list(void)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int i, last_act = -1;

   printf("\nAvailable areas (%d groups):\n\n", ab->group_count);
   for (i = 0; i < ab->group_count; i++)
   {
      AREA_GROUP_S * g = &ab->groups[i];
      if (g->act != last_act)
      {
         if (g->act > 0)
            printf("  Act %d:\n", g->act);
         else
            printf("  Other:\n");
         last_act = g->act;
      }
      if (g->act > 0)
         printf("    %-30s %3d maps  (--area \"Act %d - %s\")\n",
                g->name, g->entry_count, g->act, g->name);
      else
         printf("    %-30s %3d maps  (--area \"%s\")\n",
                g->name, g->entry_count, g->name);
   }
   printf("\n");
   fflush(stdout);
}

/* Print extended list: individual levels from Levels.txt grouped by Act,
 * showing which LvlType (tileset) each level uses. */
void area_browser_list_ext(void)
{
   TXT_S * levels = glb_ds1edit.levels_buff;
   int lv_name_col, lv_id_col, lv_act_col, lv_type_col;
   int row, last_act = -1;

   if (levels == NULL)
   {
      printf("Levels.txt not loaded\n");
      return;
   }

   lv_name_col = misc_get_txt_column_num(RQ_LEVELS, "Name");
   lv_id_col   = misc_get_txt_column_num(RQ_LEVELS, "Id");
   lv_act_col  = misc_get_txt_column_num(RQ_LEVELS, "Act");
   lv_type_col = misc_get_txt_column_num(RQ_LEVELS, "LevelType");

   printf("\nAll levels (%d entries):\n\n", levels->line_num);

   for (row = 0; row < levels->line_num; row++)
   {
      char * name;
      long * id_ptr, * act_ptr, * type_ptr;
      long id, act, lvltype;
      int gi, map_count = 0;

      name = levels->data + (row * levels->line_size) + levels->col[lv_name_col].offset;
      id_ptr = (long *)(levels->data + (row * levels->line_size) + levels->col[lv_id_col].offset);
      act_ptr = (long *)(levels->data + (row * levels->line_size) + levels->col[lv_act_col].offset);
      type_ptr = (long *)(levels->data + (row * levels->line_size) + levels->col[lv_type_col].offset);

      id = *id_ptr;
      act = *act_ptr;
      lvltype = *type_ptr;

      if (id <= 0 || name[0] == '\0')
         continue;

      /* Find the group for this LvlType to show map count */
      for (gi = 0; gi < glb_ds1edit.area_browser.group_count; gi++)
      {
         if (glb_ds1edit.area_browser.groups[gi].lvltype_id == (int)lvltype)
         {
            map_count = glb_ds1edit.area_browser.groups[gi].entry_count;
            break;
         }
      }

      /* Act header */
      if (act != last_act)
      {
         if (act >= 0 && act <= 4)
            printf("  Act %ld:\n", act + 1);
         else
            printf("  Other:\n");
         last_act = (int)act;
      }

      printf("    %-40s Id=%-3ld LvlType=%-3ld %3d maps  (--area \"%s\")\n",
             name, id, lvltype, map_count, name);
   }
   printf("\n");
   fflush(stdout);
}

/* Free all area browser dynamic memory. */
void area_browser_destroy(void)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int i;

   if (ab->groups != NULL)
   {
      for (i = 0; i < ab->group_count; i++)
      {
         if (ab->groups[i].entries != NULL)
            free(ab->groups[i].entries);
      }
      free(ab->groups);
   }
   ab->groups = NULL;
   ab->group_count = 0;
   ab->group_max = 0;
   ab->selected_group = -1;
   ab->scroll_offset = 0;
   ab->is_active = FALSE;
}

/* Find a group by name (case-insensitive) and open it.
 * Returns 0 on success, -1 if not found. */
int area_browser_open_by_name(const char * area_name)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int i;

   for (i = 0; i < ab->group_count; i++)
   {
      char full_name[128];
      AREA_GROUP_S * g = &ab->groups[i];

      /* build full name for comparison: "Act X - Name" or just "Name" */
      if (g->act > 0)
         sprintf(full_name, "Act %d - %s", g->act, g->name);
      else
         strncpy(full_name, g->name, sizeof(full_name) - 1);
      full_name[sizeof(full_name) - 1] = '\0';

      if (stricmp(full_name, area_name) == 0)
         return area_browser_open_group(i);

      /* also try matching just the short name */
      if (stricmp(g->name, area_name) == 0)
         return area_browser_open_group(i);
   }

   /* No group matched — try individual level name from Levels.txt.
    * Look up the level's LevelType, then open that group. */
   {
      TXT_S * levels = glb_ds1edit.levels_buff;
      if (levels != NULL)
      {
         int lv_name_col = misc_get_txt_column_num(RQ_LEVELS, "Name");
         int lv_type_col = misc_get_txt_column_num(RQ_LEVELS, "LevelType");
         int row;

         for (row = 0; row < levels->line_num; row++)
         {
            char * lv_name = levels->data + (row * levels->line_size)
                             + levels->col[lv_name_col].offset;
            if (stricmp(lv_name, area_name) == 0)
            {
               long * type_ptr = (long *)(levels->data + (row * levels->line_size)
                                 + levels->col[lv_type_col].offset);
               long lvltype = *type_ptr;

               /* Find the group for this LevelType */
               for (i = 0; i < ab->group_count; i++)
               {
                  if (ab->groups[i].lvltype_id == (int)lvltype)
                  {
                     printf("Level '%s' uses LvlType %ld (group: %s)\n",
                            area_name, lvltype,
                            ab->groups[i].act > 0 ? ab->groups[i].name : ab->groups[i].name);
                     return area_browser_open_group(i);
                  }
               }
               printf("area_browser_open_by_name: level '%s' has LvlType %ld but no group found\n",
                      area_name, lvltype);
               return -1;
            }
         }
      }
   }

   printf("area_browser_open_by_name: area '%s' not found\n", area_name);
   return -1;
}

/* Open all DS1 files in the given group. Returns number of DS1s opened. */
int area_browser_open_group(int group_idx)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   int i, ds1_idx = 0, opened = 0;

   if (group_idx < 0 || group_idx >= ab->group_count)
      return -1;

   g = &ab->groups[group_idx];
   if (g->act > 0)
      printf("Opening area: Act %d - %s (%d maps)\n", g->act, g->name, g->entry_count);
   else
      printf("Opening area: %s (%d maps)\n", g->name, g->entry_count);

   /* find the first free ds1 slot */
   for (ds1_idx = 0; ds1_idx < DS1_MAX; ds1_idx++)
   {
      if (glb_ds1[ds1_idx].name[0] == '\0')
         break;
   }

   for (i = 0; i < g->entry_count; i++)
   {
      AREA_DS1_ENTRY_S * e = &g->entries[i];
      char ds1_path[256];

      if (ds1_idx >= DS1_MAX)
         break;

      /* Build the full path — LvlPrest paths look like "Act1/Town/TownN1.ds1",
       * the editor needs "assets/tiles/..." or the MPQ internal path */
      sprintf(ds1_path, "assets/tiles/%s", e->ds1_path);

      printf("  [%d] lvltype=%d def=%d %s\n", i, e->lvltype_id, e->lvlprest_def, ds1_path);
      fflush(stdout);

      misc_open_1_ds1(ds1_idx, ds1_path, e->lvltype_id, e->lvlprest_def, 0, 0);
      ds1_idx++;
      opened++;
   }

   if (opened > 1)
      glb_ds1edit.show_2nd_row = TRUE;

   printf("Opened %d maps\n", opened);
   fflush(stdout);
   return opened;
}

/* GUI area browser — placeholder for Phase 3 */
int area_browser_run(void)
{
   /* TODO: Phase 3 — GUI area selector */
   printf("area_browser_run: GUI not implemented yet\n");
   return -1;
}
