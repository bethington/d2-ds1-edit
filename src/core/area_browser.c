#include "structs.h"
#include "error.h"
#include "misc.h"
#include "core/ds1.h"
#include "core/txtread.h"
#include "core/animdata.h"
#include "core/cof.h"
#include "render/preview.h"
#include "core/area_browser.h"

#define AREA_INIT_GROUPS    64
#define AREA_INIT_ENTRIES   32


/* ---- helpers ---- */

/* Case-insensitive substring search. */
static int stristr_match(const char * haystack, const char * needle)
{
   int hlen, nlen, i;
   if (needle == NULL || needle[0] == '\0') return 1;
   hlen = (int)strlen(haystack);
   nlen = (int)strlen(needle);
   if (nlen > hlen) return 0;
   for (i = 0; i <= hlen - nlen; i++)
   {
      if (strnicmp(haystack + i, needle, nlen) == 0)
         return 1;
   }
   return 0;
}

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

/* Print all DS1 files from the area browser data. */
/* Print DS1 files, optionally filtered by substring (case-insensitive).
 * Pass NULL or "" for no filter. */
void area_browser_list_files(const char * filter)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int i, j, total = 0;
   char group_label[128];

   if (filter != NULL && filter[0] != '\0')
      printf("\nDS1 files matching \"%s\":\n\n", filter);
   else
      printf("\nAll DS1 files:\n\n");

   for (i = 0; i < ab->group_count; i++)
   {
      AREA_GROUP_S * g = &ab->groups[i];

      if (g->act > 0)
         sprintf(group_label, "Act %d - %s", g->act, g->name);
      else
         strncpy(group_label, g->name, sizeof(group_label) - 1);
      group_label[sizeof(group_label) - 1] = '\0';

      for (j = 0; j < g->entry_count; j++)
      {
         AREA_DS1_ENTRY_S * e = &g->entries[j];

         /* Apply filter: match against path or group label */
         if (filter != NULL && filter[0] != '\0')
         {
            if (!stristr_match(e->ds1_path, filter) &&
                !stristr_match(group_label, filter))
               continue;
         }

         printf("  %-55s LvlType=%-3d Def=%-5d (%s)\n",
                e->ds1_path, e->lvltype_id, e->lvlprest_def, group_label);
         total++;
      }
   }
   printf("\nTotal: %d DS1 files\n\n", total);
   fflush(stdout);
}

/* Find a DS1 file by path (case-insensitive, partial match on filename).
 * Opens it with its correct LvlType and DEF. Returns 0 on success. */
int area_browser_open_by_file(const char * ds1_path)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int i, j, ds1_idx = 0;
   const char * search_name;

   /* Extract just the filename for matching if a full path was given */
   search_name = strrchr(ds1_path, '/');
   if (search_name == NULL)
      search_name = strrchr(ds1_path, '\\');
   if (search_name != NULL)
      search_name++;
   else
      search_name = ds1_path;

   for (i = 0; i < ab->group_count; i++)
   {
      AREA_GROUP_S * g = &ab->groups[i];
      for (j = 0; j < g->entry_count; j++)
      {
         AREA_DS1_ENTRY_S * e = &g->entries[j];
         const char * entry_name;

         /* Try full path match first */
         if (stricmp(e->ds1_path, ds1_path) == 0)
            goto found;

         /* Try filename-only match */
         entry_name = strrchr(e->ds1_path, '/');
         if (entry_name == NULL)
            entry_name = strrchr(e->ds1_path, '\\');
         if (entry_name != NULL)
            entry_name++;
         else
            entry_name = e->ds1_path;

         if (stricmp(entry_name, search_name) == 0)
            goto found;

         continue;

      found:
         printf("Loading DS1: %s (LvlType=%d, Def=%d)\n",
                e->ds1_path, e->lvltype_id, e->lvlprest_def);
         fflush(stdout);

         /* Find first free ds1 slot */
         for (ds1_idx = 0; ds1_idx < DS1_MAX; ds1_idx++)
            if (glb_ds1[ds1_idx].name[0] == '\0')
               break;

         if (ds1_idx >= DS1_MAX)
         {
            printf("area_browser_open_by_file: DS1_MAX reached\n");
            return -1;
         }

         {
            char full_path[256];
            sprintf(full_path, "assets/tiles/%s", e->ds1_path);
            misc_open_1_ds1(ds1_idx, full_path, e->lvltype_id, e->lvlprest_def, 0, 0);
         }
         return 0;
      }
   }

   printf("area_browser_open_by_file: '%s' not found\n", ds1_path);
   return -1;
}

/* Switch to a new area: clear existing DS1s, load new ones, run post-load.
 * Returns the first ds1_idx of the loaded area, or -1 on error. */
int area_browser_switch_area(int group_idx)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int i, opened;

   if (group_idx < 0 || group_idx >= ab->group_count)
      return -1;
   if (ab->groups[group_idx].entry_count == 0)
      return -1;

   printf("Switching to area: ");
   fflush(stdout);

   /* Free all currently loaded DS1 files (and their DT1 references) */
   for (i = 0; i < DS1_MAX; i++)
   {
      if (glb_ds1[i].name[0] != '\0')
         ds1_free(i);
   }

   /* Clear all DS1 slots */
   memset(glb_ds1, 0, sizeof(DS1_S) * DS1_MAX);

   /* Destroy COF animation data but keep the obj_desc table —
    * it's loaded from obj.txt and doesn't change between areas. */
   {
      int od;
      for (od = 0; od < glb_ds1edit.obj_desc_num; od++)
      {
         if (glb_ds1edit.obj_desc[od].cof != NULL)
         {
            anim_destroy_cof(glb_ds1edit.obj_desc[od].cof);
            glb_ds1edit.obj_desc[od].cof = NULL;
         }
      }
   }

   /* Load the new area */
   opened = area_browser_open_group(group_idx);
   if (opened <= 0)
      return -1;

   /* Post-load: animdata, animations, colormaps */
   animdata_load();
   anim_update_gfx(FALSE);
   misc_make_cmaps();

   /* Promote bitmaps to VIDEO */
   al_set_new_bitmap_flags(ALLEGRO_VIDEO_BITMAP);
   ds1edit_recreate_render_targets();
   if (a5_current_palette != NULL)
      dt1_rebuild_bitmaps_from_cache(a5_current_palette);
   /* Promote animation bitmaps */
   {
      int oi, li, fi;
      for (oi = 0; oi < glb_ds1edit.obj_desc_num; oi++)
      {
         COF_S *cof = glb_ds1edit.obj_desc[oi].cof;
         if (cof == NULL) continue;
         for (li = 0; li < COMPOSIT_NB; li++)
         {
            LAY_INF_S *lay = &cof->lay_inf[li];
            if (lay->bmp == NULL) continue;
            for (fi = 0; fi < lay->bmp_num; fi++)
            {
               ALLEGRO_BITMAP *old_bmp = lay->bmp[fi];
               if (old_bmp == NULL) continue;
               if (al_get_bitmap_flags(old_bmp) & ALLEGRO_MEMORY_BITMAP)
               {
                  ALLEGRO_BITMAP *new_bmp = al_clone_bitmap(old_bmp);
                  if (new_bmp != NULL)
                  {
                     al_destroy_bitmap(old_bmp);
                     lay->bmp[fi] = new_bmp;
                  }
               }
            }
         }
      }
   }

   /* Reset viewport */
   glb_ds1edit.win_preview.x0 = glb_ds1[0].own_wpreview.x0;
   glb_ds1edit.win_preview.y0 = glb_ds1[0].own_wpreview.y0;
   glb_ds1edit.win_preview.w  = glb_config.screen.width;
   glb_ds1edit.win_preview.h  = glb_config.screen.height;

   wpreview_init_palette_state(0);
   glb_ds1edit.has_loaded_ds1 = TRUE;
   glb_ds1edit.ds1_group_idx = 0;

   /* Apply sticky zoom */
   if (glb_ds1edit.last_zoom > ZM_11 && glb_ds1edit.last_zoom < ZM_MAX)
      change_zoom(0, glb_ds1edit.last_zoom);

   if (opened > 1)
      glb_ds1edit.show_2nd_row = TRUE;

   printf("done (%d maps loaded)\n", opened);
   fflush(stdout);
   return 0;
}

/* Switch to a single DS1 file from an expanded group entry. */
int area_browser_switch_single(int group_idx, int entry_idx)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   AREA_GROUP_S * g;
   AREA_DS1_ENTRY_S * e;
   int i;
   char ds1_path[256];

   if (group_idx < 0 || group_idx >= ab->group_count)
      return -1;
   g = &ab->groups[group_idx];
   if (entry_idx < 0 || entry_idx >= g->entry_count)
      return -1;
   e = &g->entries[entry_idx];

   /* Free all currently loaded DS1 files */
   for (i = 0; i < DS1_MAX; i++)
   {
      if (glb_ds1[i].name[0] != '\0')
         ds1_free(i);
   }
   memset(glb_ds1, 0, sizeof(DS1_S) * DS1_MAX);

   /* Destroy COF animation data */
   {
      int od;
      for (od = 0; od < glb_ds1edit.obj_desc_num; od++)
      {
         if (glb_ds1edit.obj_desc[od].cof != NULL)
         {
            anim_destroy_cof(glb_ds1edit.obj_desc[od].cof);
            glb_ds1edit.obj_desc[od].cof = NULL;
         }
      }
   }

   /* Load just one DS1 */
   sprintf(ds1_path, "assets/tiles/%s", e->ds1_path);
   misc_open_1_ds1(0, ds1_path, e->lvltype_id, e->lvlprest_def, 0, 0);

   /* Post-load */
   animdata_load();
   anim_update_gfx(FALSE);
   misc_make_cmaps();

   al_set_new_bitmap_flags(ALLEGRO_VIDEO_BITMAP);
   ds1edit_recreate_render_targets();
   if (a5_current_palette != NULL)
      dt1_rebuild_bitmaps_from_cache(a5_current_palette);

   /* Promote animation bitmaps */
   {
      int oi, li, fi;
      for (oi = 0; oi < glb_ds1edit.obj_desc_num; oi++)
      {
         COF_S *cof = glb_ds1edit.obj_desc[oi].cof;
         if (cof == NULL) continue;
         for (li = 0; li < COMPOSIT_NB; li++)
         {
            LAY_INF_S *lay = &cof->lay_inf[li];
            if (lay->bmp == NULL) continue;
            for (fi = 0; fi < lay->bmp_num; fi++)
            {
               ALLEGRO_BITMAP *old_bmp = lay->bmp[fi];
               if (old_bmp == NULL) continue;
               if (al_get_bitmap_flags(old_bmp) & ALLEGRO_MEMORY_BITMAP)
               {
                  ALLEGRO_BITMAP *new_bmp = al_clone_bitmap(old_bmp);
                  if (new_bmp != NULL)
                  {
                     al_destroy_bitmap(old_bmp);
                     lay->bmp[fi] = new_bmp;
                  }
               }
            }
         }
      }
   }

   /* Reset viewport */
   glb_ds1edit.win_preview.x0 = glb_ds1[0].own_wpreview.x0;
   glb_ds1edit.win_preview.y0 = glb_ds1[0].own_wpreview.y0;
   glb_ds1edit.win_preview.w  = glb_config.screen.width;
   glb_ds1edit.win_preview.h  = glb_config.screen.height;

   wpreview_init_palette_state(0);
   glb_ds1edit.has_loaded_ds1 = TRUE;
   glb_ds1edit.ds1_group_idx = 0;
   glb_ds1edit.show_2nd_row = FALSE;

   /* Apply sticky zoom */
   if (glb_ds1edit.last_zoom > ZM_11 && glb_ds1edit.last_zoom < ZM_MAX)
      change_zoom(0, glb_ds1edit.last_zoom);

   return 0;
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
/* ---- Sidebar rendering ---- */

#define SB_FONT_H      8
#define SB_LINE_H      14
#define SB_MARGIN_X    8
#define SB_HEADER_H    22
#define SB_TAB_W       16

/* Draw the area browser sidebar on the backbuffer (target already set). */
void area_browser_draw_sidebar(int width, int height)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int y, i, last_act = -1;
   int draw_row = 0;
   int top_bar_h = glb_ds1edit.show_2nd_row ? 20 : 9;
   int bottom_bar_h = 10;
   int panel_top = top_bar_h;
   int panel_bottom = height - bottom_bar_h;
   ALLEGRO_COLOR col_title    = al_map_rgb(255, 200, 80);
   ALLEGRO_COLOR col_act      = al_map_rgb(180, 150, 60);
   ALLEGRO_COLOR col_item     = al_map_rgb(200, 200, 200);
   ALLEGRO_COLOR col_sel_bg   = al_map_rgba(50, 70, 130, 220);
   ALLEGRO_COLOR col_sel_text = al_map_rgb(255, 255, 255);
   ALLEGRO_COLOR col_count    = al_map_rgb(120, 120, 120);
   ALLEGRO_COLOR col_zero     = al_map_rgb(60, 60, 60);
   ALLEGRO_COLOR col_border   = al_map_rgb(60, 50, 40);
   ALLEGRO_COLOR col_close    = al_map_rgb(160, 160, 160);

   /* Semi-transparent background overlay — between top and bottom bars */
   al_draw_filled_rectangle(0, (float)panel_top, (float)width, (float)panel_bottom,
                             al_map_rgba(20, 16, 12, 200));

   /* Right border */
   al_draw_line((float)width - 0.5f, (float)panel_top,
                (float)width - 0.5f, (float)panel_bottom, col_border, 1.0f);

   /* Header */
   al_draw_filled_rectangle(0, (float)panel_top, (float)width,
                             (float)(panel_top + SB_HEADER_H), al_map_rgba(36, 30, 24, 220));
   al_draw_textf(a5_font, col_title, (float)SB_MARGIN_X, (float)(panel_top + 7), 0, "Areas");

   /* Close button "X" */
   al_draw_textf(a5_font, col_close, (float)(width - 16), (float)(panel_top + 7), 0, "X");

   /* Area list */
   y = panel_top + SB_HEADER_H + 4;
   for (i = 0; i < ab->group_count; i++)
   {
      AREA_GROUP_S * g = &ab->groups[i];
      int is_selected = (i == ab->selected_group);

      /* Act header */
      if (g->act != last_act)
      {
         if (draw_row >= ab->scroll_offset && y + SB_LINE_H < panel_bottom)
         {
            if (g->act > 0)
               al_draw_textf(a5_font, col_act, (float)SB_MARGIN_X, (float)y, 0,
                              "Act %d", g->act);
            else
               al_draw_textf(a5_font, col_act, (float)SB_MARGIN_X, (float)y, 0,
                              "Other");
            y += SB_LINE_H;
         }
         draw_row++;
         last_act = g->act;
      }

      /* Group row — with expand/collapse arrow */
      if (draw_row >= ab->scroll_offset && y + SB_LINE_H < panel_bottom)
      {
         ALLEGRO_COLOR text_col = g->entry_count > 0 ? col_item : col_zero;
         char count_str[16];
         const char * arrow = g->is_expanded ? "-" : "+";

         if (is_selected && g->entry_count > 0)
         {
            al_draw_filled_rectangle((float)(SB_MARGIN_X + 4), (float)(y - 1),
                                     (float)(width - 4), (float)(y + SB_LINE_H - 1),
                                     col_sel_bg);
            text_col = col_sel_text;
         }

         /* Arrow */
         if (g->entry_count > 0)
            al_draw_textf(a5_font, text_col, (float)(SB_MARGIN_X + 4), (float)y, 0,
                           "%s", arrow);

         /* Name */
         al_draw_textf(a5_font, text_col, (float)(SB_MARGIN_X + 16), (float)y, 0,
                        "%s", g->name);

         /* Count */
         sprintf(count_str, "%d", g->entry_count);
         al_draw_textf(a5_font, is_selected ? col_sel_text : col_count,
                        (float)(width - 32), (float)y, 0,
                        "%s", count_str);

         y += SB_LINE_H;
      }
      draw_row++;

      /* Expanded entries */
      if (g->is_expanded)
      {
         int j;
         ALLEGRO_COLOR col_file = al_map_rgb(160, 180, 200);
         ALLEGRO_COLOR col_file_sel = al_map_rgb(255, 255, 255);

         for (j = 0; j < g->entry_count; j++)
         {
            if (draw_row >= ab->scroll_offset && y + SB_LINE_H < panel_bottom)
            {
               const char * fname;
               ALLEGRO_COLOR fc = col_file;

               /* Extract just the filename */
               fname = strrchr(g->entries[j].ds1_path, '/');
               if (fname == NULL) fname = strrchr(g->entries[j].ds1_path, '\\');
               if (fname != NULL) fname++; else fname = g->entries[j].ds1_path;

               /* Highlight on hover — use selected_entry field */
               if (i == ab->selected_group && j == ab->selected_entry)
               {
                  al_draw_filled_rectangle((float)(SB_MARGIN_X + 18), (float)(y - 1),
                                           (float)(width - 4), (float)(y + SB_LINE_H - 1),
                                           al_map_rgba(60, 80, 100, 200));
                  fc = col_file_sel;
               }

               al_draw_textf(a5_font, fc, (float)(SB_MARGIN_X + 22), (float)y, 0,
                              "%s", fname);

               y += SB_LINE_H;
            }
            draw_row++;
         }
      }
   }
}

/* Draw collapsed sidebar tab — small ">" button at left edge. */
void area_browser_draw_sidebar_tab(int height)
{
   int top_bar_h = glb_ds1edit.show_2nd_row ? 20 : 9;
   int bottom_bar_h = 10;
   int panel_top = top_bar_h;
   int panel_bottom = height - bottom_bar_h;
   ALLEGRO_COLOR col_bg   = al_map_rgba(36, 30, 24, 180);
   ALLEGRO_COLOR col_text = al_map_rgb(160, 160, 160);

   al_draw_filled_rectangle(0, (float)panel_top, (float)SB_TAB_W, (float)panel_bottom, col_bg);
   al_draw_line((float)SB_TAB_W - 0.5f, (float)panel_top,
                (float)SB_TAB_W - 0.5f, (float)panel_bottom,
                al_map_rgb(60, 50, 40), 1.0f);
   al_draw_textf(a5_font, col_text, 4, (float)((panel_top + panel_bottom) / 2 - 4), 0, ">");
}

/* Handle a mouse click in the sidebar.
 * Returns: -2=close, -1=no action, -3=entry clicked (loads single DS1),
 *          0+=group clicked (toggles expand/collapse). */
int area_browser_sidebar_click(int mx, int my, int sidebar_w, int disp_h)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int y, i, last_act = -1;
   int draw_row = 0;
   int top_bar_h = glb_ds1edit.show_2nd_row ? 20 : 9;
   int panel_top = top_bar_h;

   /* Close button */
   if (my >= panel_top && my < panel_top + SB_HEADER_H && mx > sidebar_w - 20)
      return -2;

   /* Find which row was clicked */
   y = panel_top + SB_HEADER_H + 4;
   for (i = 0; i < ab->group_count; i++)
   {
      AREA_GROUP_S * g = &ab->groups[i];

      /* Act header row */
      if (g->act != last_act)
      {
         if (draw_row >= ab->scroll_offset)
            y += SB_LINE_H;
         draw_row++;
         last_act = g->act;
      }

      /* Group row */
      if (draw_row >= ab->scroll_offset)
      {
         if (my >= y && my < y + SB_LINE_H)
         {
            if (g->entry_count > 0)
            {
               /* Toggle expand/collapse */
               g->is_expanded = !g->is_expanded;
               ab->selected_group = i;
               ab->selected_entry = -1;
               return i;
            }
            return -1;
         }
         y += SB_LINE_H;
      }
      draw_row++;

      /* Expanded entry rows */
      if (g->is_expanded)
      {
         int j;
         for (j = 0; j < g->entry_count; j++)
         {
            if (draw_row >= ab->scroll_offset)
            {
               if (my >= y && my < y + SB_LINE_H)
               {
                  /* Entry clicked — load single DS1 */
                  ab->selected_group = i;
                  ab->selected_entry = j;
                  return -3;
               }
               y += SB_LINE_H;
            }
            draw_row++;
         }
      }
   }
   return -1;
}

/* Handle mouse wheel scroll in sidebar. Returns 1 if handled. */
int area_browser_sidebar_scroll(int dz)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   ab->scroll_offset -= dz * 3;
   if (ab->scroll_offset < 0)
      ab->scroll_offset = 0;
   return 1;
}

/* ---- GUI area browser (full-screen, for --no-arg startup fallback) ---- */

#define AB_FONT_H      8
#define AB_LINE_H      14
#define AB_MARGIN_X    20
#define AB_HEADER_H    30
#define AB_FOOTER_H    24

static void area_browser_draw(void)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int w = al_get_display_width(a5_display);
   int h = al_get_display_height(a5_display);
   int visible_lines = (h - AB_HEADER_H - AB_FOOTER_H) / AB_LINE_H;
   int y, i, last_act = -1;
   int draw_row = 0; /* counts rows including act headers */
   ALLEGRO_COLOR col_bg       = al_map_rgb(20, 16, 12);
   ALLEGRO_COLOR col_title    = al_map_rgb(255, 200, 80);
   ALLEGRO_COLOR col_act      = al_map_rgb(200, 170, 80);
   ALLEGRO_COLOR col_item     = al_map_rgb(220, 220, 220);
   ALLEGRO_COLOR col_sel_bg   = al_map_rgb(40, 60, 120);
   ALLEGRO_COLOR col_sel_text = al_map_rgb(255, 255, 255);
   ALLEGRO_COLOR col_count    = al_map_rgb(140, 140, 140);
   ALLEGRO_COLOR col_footer   = al_map_rgb(120, 120, 120);
   ALLEGRO_COLOR col_zero     = al_map_rgb(80, 80, 80);

   al_set_target_backbuffer(a5_display);
   al_clear_to_color(col_bg);

   /* Title bar */
   al_draw_filled_rectangle(0, 0, (float)w, (float)AB_HEADER_H, al_map_rgb(40, 32, 24));
   al_draw_textf(a5_font, col_title, (float)AB_MARGIN_X, 10, 0,
                 "DS1 Editor - Area Browser");
   al_draw_textf(a5_font, col_footer, (float)(w - 100), 10, 0, "Esc = Exit");

   /* Area list */
   y = AB_HEADER_H + 4;
   for (i = 0; i < ab->group_count; i++)
   {
      AREA_GROUP_S * g = &ab->groups[i];
      int is_selected = (i == ab->selected_group);

      /* Act header */
      if (g->act != last_act)
      {
         if (draw_row >= ab->scroll_offset && y + AB_LINE_H < h - AB_FOOTER_H)
         {
            if (g->act > 0)
               al_draw_textf(a5_font, col_act, (float)AB_MARGIN_X, (float)y, 0,
                              "Act %d", g->act);
            else
               al_draw_textf(a5_font, col_act, (float)AB_MARGIN_X, (float)y, 0,
                              "Other");
            y += AB_LINE_H;
         }
         draw_row++;
         last_act = g->act;
      }

      /* Group row */
      if (draw_row >= ab->scroll_offset && y + AB_LINE_H < h - AB_FOOTER_H)
      {
         ALLEGRO_COLOR text_col = g->entry_count > 0 ? col_item : col_zero;

         if (is_selected)
         {
            al_draw_filled_rectangle((float)(AB_MARGIN_X + 10), (float)(y - 1),
                                     (float)(w - AB_MARGIN_X), (float)(y + AB_LINE_H - 1),
                                     col_sel_bg);
            text_col = col_sel_text;
         }

         al_draw_textf(a5_font, text_col, (float)(AB_MARGIN_X + 16), (float)y, 0,
                        "%s", g->name);

         al_draw_textf(a5_font, is_selected ? col_sel_text : col_count,
                        (float)(w - 160), (float)y, 0,
                        "%d maps", g->entry_count);

         y += AB_LINE_H;
      }
      draw_row++;
   }

   /* Footer */
   al_draw_filled_rectangle(0, (float)(h - AB_FOOTER_H), (float)w, (float)h,
                             al_map_rgb(40, 32, 24));
   al_draw_textf(a5_font, col_footer, (float)AB_MARGIN_X, (float)(h - AB_FOOTER_H + 8), 0,
                  "Up/Down = Navigate    Enter = Load Area    Wheel = Scroll    Esc = Exit");

   al_flip_display();
}

/* GUI area browser main loop. Returns selected group index, or -1 to exit. */
int area_browser_run(void)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   ALLEGRO_EVENT_QUEUE * eq;
   ALLEGRO_EVENT ev;
   int done = 0;
   int result = -1;
   int redraw = 1;

   /* Init GUI state */
   ab->selected_group = 0;
   ab->scroll_offset = 0;

   /* Skip to first group with maps */
   {
      int i;
      for (i = 0; i < ab->group_count; i++)
      {
         if (ab->groups[i].entry_count > 0)
         {
            ab->selected_group = i;
            break;
         }
      }
   }

   /* Create event queue for the browser */
   eq = al_create_event_queue();
   al_register_event_source(eq, al_get_keyboard_event_source());
   al_register_event_source(eq, al_get_mouse_event_source());
   al_register_event_source(eq, al_get_display_event_source(a5_display));

   while (!done)
   {
      if (redraw)
      {
         area_browser_draw();
         redraw = 0;
      }

      al_wait_for_event(eq, &ev);

      if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
      {
         done = 1;
         result = -1;
      }
      else if (ev.type == ALLEGRO_EVENT_KEY_DOWN)
      {
         switch (ev.keyboard.keycode)
         {
         case ALLEGRO_KEY_ESCAPE:
            done = 1;
            result = -1;
            break;

         case ALLEGRO_KEY_ENTER:
         case ALLEGRO_KEY_PAD_ENTER:
            if (ab->selected_group >= 0 &&
                ab->selected_group < ab->group_count &&
                ab->groups[ab->selected_group].entry_count > 0)
            {
               done = 1;
               result = ab->selected_group;
            }
            break;

         case ALLEGRO_KEY_UP:
            if (ab->selected_group > 0)
            {
               ab->selected_group--;
               redraw = 1;
            }
            break;

         case ALLEGRO_KEY_DOWN:
            if (ab->selected_group < ab->group_count - 1)
            {
               ab->selected_group++;
               redraw = 1;
            }
            break;

         case ALLEGRO_KEY_PGUP:
            ab->selected_group -= 10;
            if (ab->selected_group < 0) ab->selected_group = 0;
            redraw = 1;
            break;

         case ALLEGRO_KEY_PGDN:
            ab->selected_group += 10;
            if (ab->selected_group >= ab->group_count)
               ab->selected_group = ab->group_count - 1;
            redraw = 1;
            break;

         case ALLEGRO_KEY_HOME:
            ab->selected_group = 0;
            ab->scroll_offset = 0;
            redraw = 1;
            break;

         case ALLEGRO_KEY_END:
            ab->selected_group = ab->group_count - 1;
            redraw = 1;
            break;
         }
      }
      else if (ev.type == ALLEGRO_EVENT_MOUSE_AXES)
      {
         if (ev.mouse.dz != 0)
         {
            ab->scroll_offset -= ev.mouse.dz * 3;
            if (ab->scroll_offset < 0)
               ab->scroll_offset = 0;
            redraw = 1;
         }
         else
         {
            /* Hover: find which group the mouse is over */
            int my = ev.mouse.y;
            int row_y = AB_HEADER_H + 4;
            int draw_idx = 0;
            int last_a = -1;
            int gi;

            for (gi = 0; gi < ab->group_count; gi++)
            {
               /* Account for act header rows */
               if (ab->groups[gi].act != last_a)
               {
                  if (draw_idx >= ab->scroll_offset)
                     row_y += AB_LINE_H;
                  draw_idx++;
                  last_a = ab->groups[gi].act;
               }

               if (draw_idx >= ab->scroll_offset)
               {
                  if (my >= row_y && my < row_y + AB_LINE_H)
                  {
                     if (ab->selected_group != gi)
                     {
                        ab->selected_group = gi;
                        redraw = 1;
                     }
                     break;
                  }
                  row_y += AB_LINE_H;
               }
               draw_idx++;
            }
         }
      }
      else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN)
      {
         /* Click on selected group = load it */
         if (ab->selected_group >= 0 &&
             ab->selected_group < ab->group_count &&
             ab->groups[ab->selected_group].entry_count > 0)
         {
            done = 1;
            result = ab->selected_group;
         }
      }
   }

   al_destroy_event_queue(eq);
   return result;
}
