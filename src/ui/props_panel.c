#include "structs.h"
#include "misc.h"
#include "core/area_browser.h"
#include "core/ds1_manager.h"
#include "core/txtread.h"
#include "ui/compat.h"
#include "ui/props_panel.h"

/* Layout constants — match left sidebar style */
#define PP_FONT_H      8
#define PP_LINE_H      14
#define PP_MARGIN_X    8
#define PP_HEADER_H    22
#define PP_TAB_W       16
#define PP_LABEL_W     90   /* pixels reserved for field label column */

/* Forward declarations for C89 */
static int          pp_find_col(TXT_S * txt, RQ_ENUM rq, const char * col_name);
static int          pp_find_txt_row(TXT_S * txt, RQ_ENUM rq, const char * col_name, int key);
static long         pp_txt_get_num(TXT_S * txt, RQ_ENUM rq, int row, const char * col_name);
static const char * pp_txt_get_str(TXT_S * txt, RQ_ENUM rq, int row, const char * col_name);


void props_panel_init(void)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   int i;

   memset(pp, 0, sizeof(PROPS_PANEL_S));
   pp->scroll_offset = 0;
   pp->editing = FALSE;
   pp->pending_count = 0;
   pp->ds1_idx = 0;

   /* Core sections start expanded, detailed ones start collapsed */
   for (i = 0; i < PPS_MAX; i++)
      pp->section_expanded[i] = TRUE;
   pp->section_expanded[PPS_TXT_VISIBILITY]  = FALSE;
   pp->section_expanded[PPS_TXT_ENVIRONMENT] = FALSE;
   pp->section_expanded[PPS_TXT_MONTYPES]    = FALSE;
   pp->section_expanded[PPS_TXT_PROPERTIES]  = FALSE;
}


/* Recalculate how many Levels.txt rows share the same LvlType/LvlPrest Def. */
void props_panel_calc_shared_counts(void)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   TXT_S * lv = glb_ds1edit.levels_buff;
   int lvltype_id, lvlprest_def;
   int row, lt_col, lp_col;

   pp->shared_count_lvltypes = 0;
   pp->shared_count_lvlprest = 0;

   if (lv == NULL) return;

   {
      AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
      int gi = ab->loaded_group;
      int ei = ab->selected_entry;
      if (gi < 0 || gi >= ab->group_count) return;
      if (ei < 0 || ei >= ab->groups[gi].entry_count) ei = 0;
      if (ei >= ab->groups[gi].entry_count) return;
      lvltype_id = ab->groups[gi].entries[ei].lvltype_id;
      lvlprest_def = ab->groups[gi].entries[ei].lvlprest_def;
   }

   lt_col = pp_find_col(lv, RQ_LEVELS, "LevelType");
   /* No LevelPreset column in PD2, so count by LevelType only */
   lp_col = -1;

   for (row = 0; row < lv->line_num; row++)
   {
      if (lt_col >= 0)
      {
         long * lptr = (long *)(lv->data + (row * lv->line_size) + lv->col[lt_col].offset);
         if (*lptr == lvltype_id)
            pp->shared_count_lvltypes++;
      }
   }

   /* For LvlPrest, count how many File slots in the LvlPrest row are non-empty */
   {
      TXT_S * lp = glb_ds1edit.lvlprest_buff;
      int lp_row = pp_find_txt_row(lp, RQ_LVLPREST, "Def", lvlprest_def);
      if (lp_row >= 0)
      {
         int slot;
         for (slot = 1; slot <= 6; slot++)
         {
            char fcol[8];
            const char * fp;
            sprintf(fcol, "File%d", slot);
            fp = pp_txt_get_str(lp, RQ_LVLPREST, lp_row, fcol);
            if (fp[0] != '\0' && !(fp[0] == '0' && fp[1] == '\0'))
               pp->shared_count_lvlprest++;
         }
      }
   }
}


/* ---- Sync operations ---- */

/* Sync DS1 .act field from txt_act (LvlTypes.txt authoritative value). */
static void pp_sync_act_from_txt(int ds1_idx)
{
   if (ds1_idx < 0 || ds1_idx >= DS1_MAX) return;
   if (glb_ds1[ds1_idx].txt_act <= 0) return;

   printf("props_panel: sync act %ld -> %d (from txt_act)\n",
          glb_ds1[ds1_idx].act, glb_ds1[ds1_idx].txt_act);
   glb_ds1[ds1_idx].act = glb_ds1[ds1_idx].txt_act;
}

/* Sync DS1 embedded file list from LvlTypes.txt File 1-32 columns. */
static void pp_sync_files_from_lvltypes(int ds1_idx)
{
   TXT_S * lt = glb_ds1edit.lvltypes_buff;
   TXT_S * lp = glb_ds1edit.lvlprest_buff;
   int lvltype_id, lvlprest_def;
   int lt_row, lp_row;
   char new_buf[8192];
   int new_len, new_count, fi;
   long mask;

   if (ds1_idx < 0 || ds1_idx >= DS1_MAX) return;
   if (lt == NULL) return;

   {
      AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
      int gi = ab->loaded_group;
      int ei = ab->selected_entry;
      if (gi < 0 || gi >= ab->group_count) return;
      if (ei < 0) ei = 0;
      if (ei >= ab->groups[gi].entry_count) return;
      lvltype_id = ab->groups[gi].entries[ei].lvltype_id;
      lvlprest_def = ab->groups[gi].entries[ei].lvlprest_def;
   }

   lt_row = pp_find_txt_row(lt, RQ_LVLTYPE, "Id", lvltype_id);
   lp_row = pp_find_txt_row(lp, RQ_LVLPREST, "Def", lvlprest_def);
   if (lt_row < 0) return;

   /* Get Dt1Mask */
   mask = (lp_row >= 0) ? pp_txt_get_num(lp, RQ_LVLPREST, lp_row, "Dt1Mask") : 0xFFFFFFFF;

   /* Build new file buffer from LvlTypes File 1-32 */
   new_len = 0;
   new_count = 0;
   for (fi = 1; fi <= 32; fi++)
   {
      char col_name[16];
      const char * path;
      int plen;

      sprintf(col_name, "File %d", fi);
      path = pp_txt_get_str(lt, RQ_LVLTYPE, lt_row, col_name);
      if (path[0] == '\0' || (path[0] == '0' && path[1] == '\0'))
         continue;
      /* Check mask (slot 0/File1 always on, others check bit) */
      if (fi > 1 && !(mask & (1 << (fi - 1))))
         continue;

      plen = (int)strlen(path);
      if (new_len + plen + 1 < (int)sizeof(new_buf))
      {
         memcpy(new_buf + new_len, path, plen + 1); /* include null */
         new_len += plen + 1;
         new_count++;
      }
   }

   /* Replace file_buff */
   if (glb_ds1[ds1_idx].file_buff != NULL)
      free(glb_ds1[ds1_idx].file_buff);

   if (new_count > 0)
   {
      glb_ds1[ds1_idx].file_buff = (char *)malloc(new_len);
      if (glb_ds1[ds1_idx].file_buff != NULL)
         memcpy(glb_ds1[ds1_idx].file_buff, new_buf, new_len);
      glb_ds1[ds1_idx].file_len = new_len;
      glb_ds1[ds1_idx].file_num = new_count;
   }
   else
   {
      glb_ds1[ds1_idx].file_buff = NULL;
      glb_ds1[ds1_idx].file_len = 0;
      glb_ds1[ds1_idx].file_num = 0;
   }

   printf("props_panel: synced %d DT1 files from LvlTypes.txt\n", new_count);
   fflush(stdout);
}


/* ---- Drawing helpers ---- */

/* Draw a small clickable button. Returns 1 if visible. */
static int pp_draw_button(int x, int y, int w, int panel_bottom,
                           const char * text)
{
   if (y + PP_LINE_H >= panel_bottom)
      return 0;
   al_draw_filled_rectangle((float)x, (float)y,
                             (float)(x + w), (float)(y + PP_LINE_H - 2),
                             al_map_rgba(50, 70, 100, 200));
   al_draw_rectangle((float)x + 0.5f, (float)y + 0.5f,
                      (float)(x + w) - 0.5f, (float)(y + PP_LINE_H - 2) - 0.5f,
                      al_map_rgb(80, 110, 160), 1.0f);
   al_draw_textf(a5_font, al_map_rgb(200, 220, 255),
                  (float)(x + 4), (float)(y + 1), 0, "%s", text);
   return 1;
}


/* Global field index counter — tracks which field we're drawing/checking.
 * Reset to 0 at the start of each draw/click pass. */
static int pp_field_idx = 0;

/* Row counter for keyboard navigation (counts ALL rows: headers + fields + buttons). */
static int pp_row_idx = 0;

/* Mouse position for hover highlights (set at start of draw). */
static int pp_hover_mx = 0, pp_hover_my = 0;
static int pp_hover_panel_x = 0, pp_hover_panel_right = 0;

/* Track which rows are section headers (for PgUp/PgDn and Left/Right).
 * Set during draw, used by keyboard handler. */
#define PP_MAX_SECTIONS  20
static int pp_section_rows[PP_MAX_SECTIONS]; /* row indices of section headers */
static int pp_section_ids[PP_MAX_SECTIONS];  /* PP_SECTION_E for each */
static int pp_section_count = 0;


/* Draw a collapsible section header with optional scope badge.
 * scope: NULL for no badge, or text like "[this file only]" / "[shared: 5 maps]" */
static int pp_draw_section(int panel_x, int panel_right, int y,
                            int draw_row, int scroll_off, int panel_bottom,
                            PP_SECTION_E section, const char * title,
                            const char * scope)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   ALLEGRO_COLOR col_section = al_map_rgb(255, 200, 80);

   /* Record this section header for keyboard nav */
   if (pp_section_count < PP_MAX_SECTIONS)
   {
      pp_section_rows[pp_section_count] = pp_row_idx;
      pp_section_ids[pp_section_count] = (int)section;
      pp_section_count++;
   }

   if (draw_row >= scroll_off && y + PP_LINE_H < panel_bottom)
   {
      const char * arrow = pp->section_expanded[section] ? "-" : "+";

      /* Focus highlight for keyboard navigation */
      if (pp->has_focus && pp_row_idx == pp->focused_row)
      {
         al_draw_filled_rectangle((float)(panel_x + 2), (float)(y - 1),
                                   (float)(panel_right - 2), (float)(y + PP_LINE_H - 1),
                                   al_map_rgba(50, 60, 90, 150));
         al_draw_rectangle((float)(panel_x + 2) + 0.5f, (float)(y - 1) + 0.5f,
                            (float)(panel_right - 2) - 0.5f, (float)(y + PP_LINE_H - 1) - 0.5f,
                            al_map_rgb(80, 100, 160), 1.0f);
      }

      al_draw_textf(a5_font, col_section,
                     (float)(panel_x + PP_MARGIN_X), (float)y, 0,
                     "%s %s", arrow, title);
      if (scope != NULL)
      {
         ALLEGRO_COLOR col_scope = al_map_rgb(200, 160, 60);
         al_draw_textf(a5_font, col_scope,
                        (float)(panel_right - 80), (float)y, 0,
                        "%s", scope);
      }
   }
   pp_row_idx++;
   return y + PP_LINE_H;
}

/* Check if field_idx has a pending change. Returns the pending index or -1. */
static int pp_find_pending(int field_idx)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   int i;
   for (i = 0; i < pp->pending_count; i++)
   {
      if (pp->pending[i].field.row == field_idx) /* reuse row as field_idx */
         return i;
   }
   return -1;
}

/* Check if a field index corresponds to a read-only DS1 field. */
static int pp_is_read_only(int field_idx)
{
   /* DS1 header fields 0-11:
    * 0=file(RO), 1=version, 2=width(RO), 3=height(RO), 4=act, 5=txt_act(RO),
    * 6=tag_type, 7=walls(RO), 8=floors(RO), 9=shadows(RO), 10=tags(RO), 11=objects(RO) */
   if (field_idx == 0 || field_idx == 2 || field_idx == 3 ||
       field_idx == 5 || (field_idx >= 7 && field_idx <= 11))
      return 1;
   return 0;
}

/* Draw a label:value field row. Returns the new y position. */
static int pp_draw_field(int panel_x, int panel_right, int y,
                          int draw_row, int scroll_off, int panel_bottom,
                          const char * label, const char * value)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   int cur_field = pp_field_idx++;
   int pend_idx = pp_find_pending(cur_field);
   int is_ro = pp_is_read_only(cur_field);
   int is_hovered = 0;
   int val_x = panel_x + PP_MARGIN_X + PP_LABEL_W;
   ALLEGRO_COLOR col_label = al_map_rgb(140, 130, 120);
   /* Editable = bright white, read-only = dim gray */
   ALLEGRO_COLOR col_value = is_ro ? al_map_rgb(130, 130, 130)
                                    : al_map_rgb(220, 220, 220);

   if (draw_row >= scroll_off && y + PP_LINE_H < panel_bottom)
   {
      /* Check hover */
      is_hovered = (pp_hover_my >= y && pp_hover_my < y + PP_LINE_H &&
                    pp_hover_mx >= pp_hover_panel_x &&
                    pp_hover_mx < pp_hover_panel_right);

      /* Keyboard focus highlight */
      if (pp->has_focus && pp_row_idx == pp->focused_row && !is_hovered)
      {
         al_draw_filled_rectangle((float)(panel_x + PP_MARGIN_X + 6), (float)(y - 1),
                                   (float)(panel_right - 4), (float)(y + PP_LINE_H - 1),
                                   al_map_rgba(40, 50, 80, 100));
         if (!is_ro)
            al_draw_rectangle((float)(val_x - 2) + 0.5f, (float)(y - 1) + 0.5f,
                               (float)(panel_right - 4) - 0.5f,
                               (float)(y + PP_LINE_H - 1) - 0.5f,
                               al_map_rgba(60, 80, 120, 120), 1.0f);
      }

      /* Mouse hover: show edit box outline for editable fields */
      if (is_hovered && !is_ro)
      {
         al_draw_filled_rectangle((float)(val_x - 2), (float)(y - 1),
                                   (float)(panel_right - 4), (float)(y + PP_LINE_H - 1),
                                   al_map_rgba(35, 40, 55, 120));
         al_draw_rectangle((float)(val_x - 2) + 0.5f, (float)(y - 1) + 0.5f,
                            (float)(panel_right - 4) - 0.5f,
                            (float)(y + PP_LINE_H - 1) - 0.5f,
                            al_map_rgba(80, 100, 140, 150), 1.0f);
      }
      else if (is_hovered && is_ro)
      {
         /* Subtle hover for read-only */
         al_draw_filled_rectangle((float)(panel_x + PP_MARGIN_X + 6), (float)(y - 1),
                                   (float)(panel_right - 4), (float)(y + PP_LINE_H - 1),
                                   al_map_rgba(30, 30, 40, 60));
      }

      /* Pending change highlight */
      if (pend_idx >= 0)
      {
         al_draw_filled_rectangle((float)(val_x - 2), (float)(y - 1),
                                   (float)(panel_right - 4), (float)(y + PP_LINE_H - 1),
                                   al_map_rgba(60, 50, 20, 150));
         col_value = al_map_rgb(220, 200, 80);
         value = pp->pending[pend_idx].new_value;
      }

      /* Active edit box */
      if (pp->editing && pp->edit_field.row == cur_field)
      {
         int text_y = y + (PP_LINE_H - PP_FONT_H) / 2 - 1; /* vertically center text */
         int text_x = val_x + 4; /* left padding inside box */
         al_draw_filled_rectangle((float)(val_x - 2), (float)(y - 1),
                                   (float)(panel_right - 4), (float)(y + PP_LINE_H - 1),
                                   al_map_rgba(40, 40, 60, 240));
         al_draw_rectangle((float)(val_x - 2) + 0.5f, (float)(y - 1) + 0.5f,
                            (float)(panel_right - 4) - 0.5f,
                            (float)(y + PP_LINE_H - 1) - 0.5f,
                            al_map_rgb(100, 120, 180), 1.0f);
         al_draw_textf(a5_font, col_label,
                        (float)(panel_x + PP_MARGIN_X + 8), (float)text_y, 0,
                        "%s", label);
         al_draw_textf(a5_font, al_map_rgb(255, 255, 255),
                        (float)text_x, (float)text_y, 0,
                        "%s", pp->edit_buf);
         /* Blinking cursor */
         {
            double t = al_get_time();
            if (((int)(t * 2.0)) & 1)
            {
               int cx = text_x + pp->edit_cursor * 8;
               al_draw_line((float)cx, (float)text_y,
                            (float)cx, (float)(text_y + PP_FONT_H + 1),
                            al_map_rgb(255, 255, 255), 1.0f);
            }
         }
         pp_row_idx++;
         return y + PP_LINE_H;
      }

      /* Draw label */
      al_draw_textf(a5_font, col_label,
                     (float)(panel_x + PP_MARGIN_X + 8), (float)y, 0,
                     "%s", label);

      /* Draw value with truncation */
      {
         int max_chars = (panel_right - 6 - val_x) / 8; /* approx 8px per char */
         int vlen = (int)strlen(value);
         if (max_chars < 3) max_chars = 3;
         if (vlen > max_chars)
         {
            char trunc[PP_EDIT_BUF_MAX];
            strncpy(trunc, value, max_chars - 3);
            trunc[max_chars - 3] = '\0';
            strcat(trunc, "...");
            al_draw_textf(a5_font, col_value, (float)val_x, (float)y, 0,
                           "%s", trunc);
         }
         else
         {
            al_draw_textf(a5_font, col_value, (float)val_x, (float)y, 0,
                           "%s", value);
         }
      }
   }
   pp_row_idx++;
   return y + PP_LINE_H;
}

/* Draw a label:long field row. Returns the new y position. */
static int pp_draw_field_long(int panel_x, int panel_right, int y,
                               int draw_row, int scroll_off, int panel_bottom,
                               const char * label, long value)
{
   char buf[32];
   sprintf(buf, "%ld", value);
   return pp_draw_field(panel_x, panel_right, y, draw_row, scroll_off,
                         panel_bottom, label, buf);
}

/* Draw a label:int field row. Returns the new y position. */
static int pp_draw_field_int(int panel_x, int panel_right, int y,
                              int draw_row, int scroll_off, int panel_bottom,
                              const char * label, int value)
{
   char buf[32];
   sprintf(buf, "%d", value);
   return pp_draw_field(panel_x, panel_right, y, draw_row, scroll_off,
                         panel_bottom, label, buf);
}


/* ---- TXT data access helpers ---- */

/* Safe column lookup — returns col index or -1, does NOT call ds1edit_error. */
static int pp_find_col(TXT_S * txt, RQ_ENUM rq, const char * col_name)
{
   int i;
   char * desc;

   if (txt == NULL) return -1;
   for (i = 0; ; i++)
   {
      desc = glb_txt_req_ptr[rq][i];
      if (desc == NULL) return -1;  /* not found */
      if (stricmp(col_name, desc) == 0)
         return i;
   }
}

/* Find a row in a TXT_S by matching a numeric column value.
 * Returns row index, or -1 if not found. */
static int pp_find_txt_row(TXT_S * txt, RQ_ENUM rq, const char * col_name, int key)
{
   int col_idx, row;
   long * lptr;

   if (txt == NULL) return -1;
   col_idx = pp_find_col(txt, rq, col_name);
   if (col_idx < 0) return -1;

   for (row = 0; row < txt->line_num; row++)
   {
      lptr = (long *)(txt->data + (row * txt->line_size) + txt->col[col_idx].offset);
      if (*lptr == key)
         return row;
   }
   return -1;
}

/* Read a numeric cell value from a TXT row. Returns 0 if not found. */
static long pp_txt_get_num(TXT_S * txt, RQ_ENUM rq, int row, const char * col_name)
{
   int col_idx;
   long * lptr;

   if (txt == NULL || row < 0 || row >= txt->line_num) return 0;
   col_idx = pp_find_col(txt, rq, col_name);
   if (col_idx < 0) return 0;
   if (txt->col[col_idx].type != CT_NUM) return 0;

   lptr = (long *)(txt->data + (row * txt->line_size) + txt->col[col_idx].offset);
   return *lptr;
}

/* Read a string cell value from a TXT row. Returns "" if not found. */
static const char * pp_txt_get_str(TXT_S * txt, RQ_ENUM rq, int row, const char * col_name)
{
   int col_idx;
   char * sptr;

   if (txt == NULL || row < 0 || row >= txt->line_num) return "";
   col_idx = pp_find_col(txt, rq, col_name);
   if (col_idx < 0) return "";
   if (txt->col[col_idx].type != CT_STR) return "";

   sptr = txt->data + (row * txt->line_size) + txt->col[col_idx].offset;
   return sptr;
}

/* Get the lvltype_id and lvlprest_def for the currently displayed DS1. */
static void pp_get_entry_link(int * out_lvltype, int * out_def)
{
   AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
   int gi = ab->loaded_group;
   int ei = ab->selected_entry;

   *out_lvltype = 0;
   *out_def = 0;

   if (gi < 0 || gi >= ab->group_count) return;
   if (ei < 0 || ei >= ab->groups[gi].entry_count)
      ei = 0;  /* default to first entry */
   if (ei >= ab->groups[gi].entry_count) return;

   *out_lvltype = ab->groups[gi].entries[ei].lvltype_id;
   *out_def = ab->groups[gi].entries[ei].lvlprest_def;
}

/* Draw a field row with edit/pending support (TXT fields). Returns new y.
 * Delegates to pp_draw_field — tag parameter is now ignored (source shown in section header). */
static int pp_draw_field_tagged(int panel_x, int width, int y,
                                 int draw_row, int scroll_off, int panel_bottom,
                                 const char * label, const char * value,
                                 const char * tag)
{
   (void)tag;
   return pp_draw_field(panel_x, panel_x + width, y, draw_row, scroll_off,
                         panel_bottom, label, value);
}

/* Draw a numeric field. Returns new y. */
static int pp_draw_field_num_tagged(int panel_x, int width, int y,
                                     int draw_row, int scroll_off, int panel_bottom,
                                     const char * label, long value,
                                     const char * tag)
{
   char buf[32];
   (void)tag;
   sprintf(buf, "%ld", value);
   return pp_draw_field(panel_x, panel_x + width, y, draw_row, scroll_off,
                         panel_bottom, label, buf);
}


/* ---- Main draw function ---- */

void props_panel_draw(int width, int height)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   int disp_w = al_get_display_width(a5_display);
   int top_bar_h = glb_ds1edit.show_2nd_row ? 20 : 9;
   int bottom_bar_h = 10;
   int panel_top = top_bar_h;
   int panel_bottom = height - bottom_bar_h;
   int panel_x = disp_w - width;
   int panel_right = disp_w;
   int y, draw_row, idx;
   int focused = pp->has_focus;
   ALLEGRO_COLOR col_title  = al_map_rgb(255, 200, 80);
   ALLEGRO_COLOR col_border = focused ? al_map_rgb(80, 110, 180) : al_map_rgb(60, 50, 40);
   ALLEGRO_COLOR col_close  = al_map_rgb(160, 160, 160);
   ALLEGRO_COLOR col_none   = al_map_rgb(80, 80, 80);

   /* Semi-transparent background */
   al_draw_filled_rectangle((float)panel_x, (float)panel_top,
                             (float)panel_right, (float)panel_bottom,
                             al_map_rgba(20, 16, 12, 200));

   /* Left border — brighter when focused */
   al_draw_line((float)panel_x + 0.5f, (float)panel_top,
                (float)panel_x + 0.5f, (float)panel_bottom, col_border,
                focused ? 2.0f : 1.0f);

   /* Header bar — blue tint when focused */
   al_draw_filled_rectangle((float)panel_x, (float)panel_top,
                             (float)panel_right,
                             (float)(panel_top + PP_HEADER_H),
                             focused ? al_map_rgba(28, 32, 48, 230)
                                     : al_map_rgba(36, 30, 24, 220));
   al_draw_textf(a5_font, col_title,
                  (float)(panel_x + PP_MARGIN_X), (float)(panel_top + 7),
                  0, "Properties");
   al_draw_textf(a5_font, col_close,
                  (float)(panel_right - 16), (float)(panel_top + 7),
                  0, "X");

   /* Content area */
   y = panel_top + PP_HEADER_H + 4;
   draw_row = 0;
   pp_field_idx = 0;    /* Reset field counter for edit tracking */
   pp_row_idx = 0;      /* Reset row counter for keyboard navigation */
   pp_section_count = 0; /* Reset section header tracking */
   pp_hover_mx = a5_mouse_x;
   pp_hover_my = a5_mouse_y;
   pp_hover_panel_x = panel_x;
   pp_hover_panel_right = panel_right;

   if (!glb_ds1edit.has_loaded_ds1)
   {
      al_draw_textf(a5_font, col_none,
                     (float)(panel_x + PP_MARGIN_X), (float)y,
                     0, "No DS1 loaded");
      return;
   }

   idx = pp->ds1_idx;
   if (idx < 0 || idx >= DS1_MAX || glb_ds1[idx].name[0] == '\0')
   {
      al_draw_textf(a5_font, col_none,
                     (float)(panel_x + PP_MARGIN_X), (float)y,
                     0, "No DS1 selected");
      return;
   }

   /* ---- Section: DS1 Header ---- */
   y = pp_draw_section(panel_x, panel_right, y, draw_row, pp->scroll_offset,
                        panel_bottom, PPS_DS1_HEADER, "DS1 Header",
                        "[this file]");
   draw_row++;

   if (pp->section_expanded[PPS_DS1_HEADER])
   {
      /* Filename at top for context */
      y = pp_draw_field(panel_x, panel_right, y, draw_row++, pp->scroll_offset,
                         panel_bottom, "file", glb_ds1[idx].filename);
      y = pp_draw_field_long(panel_x, panel_right, y, draw_row++, pp->scroll_offset,
                              panel_bottom, "version", glb_ds1[idx].version);
      y = pp_draw_field_long(panel_x, panel_right, y, draw_row++, pp->scroll_offset,
                              panel_bottom, "width", glb_ds1[idx].width);
      y = pp_draw_field_long(panel_x, panel_right, y, draw_row++, pp->scroll_offset,
                              panel_bottom, "height", glb_ds1[idx].height);
      y = pp_draw_field_long(panel_x, panel_right, y, draw_row++, pp->scroll_offset,
                              panel_bottom, "act", glb_ds1[idx].act);
      y = pp_draw_field_int(panel_x, panel_right, y, draw_row++, pp->scroll_offset,
                             panel_bottom, "txt_act", glb_ds1[idx].txt_act);
      /* Sync act button (only show if act != txt_act) */
      if (glb_ds1[idx].txt_act > 0 && glb_ds1[idx].act != glb_ds1[idx].txt_act)
      {
         if (draw_row >= pp->scroll_offset)
            pp_draw_button(panel_x + PP_MARGIN_X + 8, y, 120, panel_bottom,
                           "Sync act<-txt");
         y += PP_LINE_H;
         draw_row++;
      }
      y = pp_draw_field_long(panel_x, panel_right, y, draw_row++, pp->scroll_offset,
                              panel_bottom, "tag_type", glb_ds1[idx].tag_type);
      y = pp_draw_field_int(panel_x, panel_right, y, draw_row++, pp->scroll_offset,
                             panel_bottom, "walls", glb_ds1[idx].wall_num);
      y = pp_draw_field_int(panel_x, panel_right, y, draw_row++, pp->scroll_offset,
                             panel_bottom, "floors", glb_ds1[idx].floor_num);
      y = pp_draw_field_int(panel_x, panel_right, y, draw_row++, pp->scroll_offset,
                             panel_bottom, "shadows", glb_ds1[idx].shadow_num);
      y = pp_draw_field_int(panel_x, panel_right, y, draw_row++, pp->scroll_offset,
                             panel_bottom, "tags", glb_ds1[idx].tag_num);
      y = pp_draw_field_long(panel_x, panel_right, y, draw_row++, pp->scroll_offset,
                              panel_bottom, "objects", glb_ds1[idx].obj_num);
   }

   /* ---- Section: Embedded DT1 Files ---- */
   {
      char sec_title[32];
      sprintf(sec_title, "DT1 Files (%ld)", glb_ds1[idx].file_num);
      y = pp_draw_section(panel_x, panel_right, y, draw_row, pp->scroll_offset,
                           panel_bottom, PPS_DS1_FILES, sec_title,
                           "[this file]");
      draw_row++;
   }

   if (pp->section_expanded[PPS_DS1_FILES])
   {
      /* Sync from LvlTypes button */
      if (draw_row >= pp->scroll_offset)
         pp_draw_button(panel_x + PP_MARGIN_X + 8, y, 160, panel_bottom,
                        "Sync from LvlTypes");
      y += PP_LINE_H;
      draw_row++;

      if (glb_ds1[idx].file_num > 0 && glb_ds1[idx].file_buff != NULL)
      {
         char * cptr = glb_ds1[idx].file_buff;
         int fi;
         for (fi = 0; fi < glb_ds1[idx].file_num; fi++)
         {
            const char * fname;
            int slen;

            /* Extract just the filename from the path */
            fname = strrchr(cptr, '\\');
            if (fname == NULL) fname = strrchr(cptr, '/');
            if (fname != NULL) fname++; else fname = cptr;

            if (draw_row >= pp->scroll_offset && y + PP_LINE_H < panel_bottom)
            {
               ALLEGRO_COLOR col_file = al_map_rgb(160, 180, 200);
               char idx_str[8];
               sprintf(idx_str, "[%d]", fi + 1);
               al_draw_textf(a5_font, al_map_rgb(100, 100, 100),
                              (float)(panel_x + PP_MARGIN_X + 8), (float)y, 0,
                              "%s", idx_str);
               al_draw_textf(a5_font, col_file,
                              (float)(panel_x + PP_MARGIN_X + 36), (float)y, 0,
                              "%s", fname);
            }
            y += PP_LINE_H;
            draw_row++;

            /* Advance to next null-terminated string */
            slen = (int)strlen(cptr);
            cptr += slen + 1;
         }
      }
      else
      {
         if (draw_row >= pp->scroll_offset && y + PP_LINE_H < panel_bottom)
         {
            al_draw_textf(a5_font, col_none,
                           (float)(panel_x + PP_MARGIN_X + 8), (float)y, 0,
                           "(none)");
         }
         y += PP_LINE_H;
         draw_row++;
      }
   }

   /* ---- Phase 3: TXT Map Configuration ---- */
   {
      int lvltype_id, lvlprest_def;
      int lp_row, lt_row, lv_row;
      TXT_S * lp = glb_ds1edit.lvlprest_buff;
      TXT_S * lt = glb_ds1edit.lvltypes_buff;
      TXT_S * lv = glb_ds1edit.levels_buff;

      pp_get_entry_link(&lvltype_id, &lvlprest_def);

      /* Find matching rows */
      lp_row = pp_find_txt_row(lp, RQ_LVLPREST, "Def", lvlprest_def);
      lt_row = pp_find_txt_row(lt, RQ_LVLTYPE, "Id", lvltype_id);
      lv_row = pp_find_txt_row(lv, RQ_LEVELS, "LevelType", lvltype_id);

      /* Build scope badge strings */
      {
         char scope_types[32], scope_prest[32];
         if (pp->shared_count_lvltypes > 1)
            sprintf(scope_types, "[%d maps]", pp->shared_count_lvltypes);
         else
            sprintf(scope_types, "[1 map]");
         if (pp->shared_count_lvlprest > 1)
            sprintf(scope_prest, "[%d files]", pp->shared_count_lvlprest);
         else
            sprintf(scope_prest, "[1 file]");

      /* ---- Section: Identity ---- */
      y = pp_draw_section(panel_x, panel_right, y, draw_row, pp->scroll_offset,
                           panel_bottom, PPS_TXT_IDENTITY, "Identity (Levels + Prest)",
                           scope_types);
      draw_row++;

      if (pp->section_expanded[PPS_TXT_IDENTITY])
      {
         if (lv_row >= 0)
         {
            y = pp_draw_field_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "level",
                  pp_txt_get_str(lv, RQ_LEVELS, lv_row, "Name"), "[Levels]");
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "level ID",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Id"), "[Levels]");
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "act (txt)",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Act"), "[Levels]");
         }
         if (lp_row >= 0)
         {
            y = pp_draw_field_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "preset",
                  pp_txt_get_str(lp, RQ_LVLPREST, lp_row, "Name"), "[Prest]");
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Def",
                  (long)lvlprest_def, "[Prest]");
         }
         if (lt_row >= 0)
         {
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "LvlType",
                  (long)lvltype_id, "[Types]");
         }
      }

      /* ---- Section: Tilesets ---- */
      y = pp_draw_section(panel_x, panel_right, y, draw_row, pp->scroll_offset,
                           panel_bottom, PPS_TXT_TILESETS, "Tilesets (LvlTypes)",
                           scope_types);
      draw_row++;

      if (pp->section_expanded[PPS_TXT_TILESETS])
      {
         if (lp_row >= 0)
         {
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Dt1Mask",
                  pp_txt_get_num(lp, RQ_LVLPREST, lp_row, "Dt1Mask"), "[Prest]");
         }

         /* Active DT1 files from LvlTypes.txt (where mask bit is set) */
         if (lt_row >= 0)
         {
            int fi;
            char col_name[16];
            for (fi = 1; fi <= 32; fi++)
            {
               const char * dt1_path;
               const char * dt1_fname;

               sprintf(col_name, "File %d", fi);
               dt1_path = pp_txt_get_str(lt, RQ_LVLTYPE, lt_row, col_name);

               /* Skip empty/zero entries */
               if (dt1_path[0] == '\0' || (dt1_path[0] == '0' && dt1_path[1] == '\0'))
                  continue;

               /* Check if mask bit is set (bit fi-1 of Dt1Mask, slot 0 always on) */
               if (fi > 1 && lp_row >= 0)
               {
                  long mask = pp_txt_get_num(lp, RQ_LVLPREST, lp_row, "Dt1Mask");
                  if (!(mask & (1 << (fi - 1))))
                     continue;  /* mask bit not set, skip */
               }

               /* Extract just filename — use pp_draw_field_tagged to keep
                * pp_field_idx in sync with the click handler */
               dt1_fname = strrchr(dt1_path, '\\');
               if (dt1_fname == NULL) dt1_fname = strrchr(dt1_path, '/');
               if (dt1_fname != NULL) dt1_fname++; else dt1_fname = dt1_path;

               {
                  char slot_str[8];
                  sprintf(slot_str, "[%d]", fi);
                  y = pp_draw_field_tagged(panel_x, width, y, draw_row++,
                        pp->scroll_offset, panel_bottom,
                        slot_str, dt1_fname, "[Types]");
               }
            }
         }
      }

      /* ---- Section: Layout ---- */
      y = pp_draw_section(panel_x, panel_right, y, draw_row, pp->scroll_offset,
                           panel_bottom, PPS_TXT_LAYOUT, "Layout (LvlPrest)",
                           scope_prest);
      draw_row++;

      if (pp->section_expanded[PPS_TXT_LAYOUT])
      {
         if (lp_row >= 0)
         {
            int slot;
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "LevelId",
                  pp_txt_get_num(lp, RQ_LVLPREST, lp_row, "LevelId"), "[Prest]");

            /* File1-6 slots */
            for (slot = 1; slot <= 6; slot++)
            {
               char fcol[8];
               const char * fpath;
               const char * ffname;
               sprintf(fcol, "File%d", slot);
               fpath = pp_txt_get_str(lp, RQ_LVLPREST, lp_row, fcol);
               if (fpath[0] == '\0' || (fpath[0] == '0' && fpath[1] == '\0'))
                  continue;
               ffname = strrchr(fpath, '/');
               if (ffname == NULL) ffname = strrchr(fpath, '\\');
               if (ffname != NULL) ffname++; else ffname = fpath;
               y = pp_draw_field_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                     panel_bottom, fcol, ffname, "[Prest]");
            }
         }
      }

      /* ---- Section: Room/Size ---- */
      y = pp_draw_section(panel_x, panel_right, y, draw_row, pp->scroll_offset,
                           panel_bottom, PPS_TXT_ROOMSIZE, "Room / Size (Levels)",
                           scope_types);
      draw_row++;

      if (pp->section_expanded[PPS_TXT_ROOMSIZE])
      {
         if (lv_row >= 0)
         {
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "SizeX",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "SizeX"), "[Levels]");
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "SizeY",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "SizeY"), "[Levels]");
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "OffsetX",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "OffsetX"), "[Levels]");
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "OffsetY",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "OffsetY"), "[Levels]");
         }
      }

      /* ---- Section: Monsters ---- */
      y = pp_draw_section(panel_x, panel_right, y, draw_row, pp->scroll_offset,
                           panel_bottom, PPS_TXT_MONSTERS, "Monsters (Levels)",
                           scope_types);
      draw_row++;

      if (pp->section_expanded[PPS_TXT_MONSTERS])
      {
         if (lv_row >= 0)
         {
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "MonLvl1",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "MonLvl1"), "[Levels]");
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "MonLvl2",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "MonLvl2"), "[Levels]");
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "MonLvl3",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "MonLvl3"), "[Levels]");
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "MonDen",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "MonDen"), "[Levels]");
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "MonDen(N)",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "MonDen(N)"), "[Levels]");
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "MonDen(H)",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "MonDen(H)"), "[Levels]");
         }
      }

      /* ---- Section: Quests / Waypoints ---- */
      y = pp_draw_section(panel_x, panel_right, y, draw_row, pp->scroll_offset,
                           panel_bottom, PPS_TXT_QUESTS, "Quests / Waypoints (Levels)",
                           scope_types);
      draw_row++;

      if (pp->section_expanded[PPS_TXT_QUESTS])
      {
         if (lv_row >= 0)
         {
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Waypoint",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Waypoint"), "[Levels]");
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Quest",
                  pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Quest"), "[Levels]");
         }
      }
      /* ---- Section: Visibility ---- */
      y = pp_draw_section(panel_x, panel_right, y, draw_row, pp->scroll_offset,
                           panel_bottom, PPS_TXT_VISIBILITY, "Visibility (Levels)",
                           scope_types);
      draw_row++;

      if (pp->section_expanded[PPS_TXT_VISIBILITY])
      {
         if (lv_row >= 0)
         {
            int vi;
            char vcol[8];
            for (vi = 0; vi < 8; vi++)
            {
               sprintf(vcol, "Vis%d", vi);
               y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                     panel_bottom, vcol, pp_txt_get_num(lv, RQ_LEVELS, lv_row, vcol), NULL);
            }
            for (vi = 0; vi < 8; vi++)
            {
               sprintf(vcol, "Warp%d", vi);
               y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                     panel_bottom, vcol, pp_txt_get_num(lv, RQ_LEVELS, lv_row, vcol), NULL);
            }
         }
      }

      /* ---- Section: Environment ---- */
      y = pp_draw_section(panel_x, panel_right, y, draw_row, pp->scroll_offset,
                           panel_bottom, PPS_TXT_ENVIRONMENT, "Environment (Levels)",
                           scope_types);
      draw_row++;

      if (pp->section_expanded[PPS_TXT_ENVIRONMENT])
      {
         if (lv_row >= 0)
         {
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Rain", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Rain"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Mud", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Mud"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "IsInside", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "IsInside"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Teleport", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Teleport"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Intensity", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Intensity"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Red", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Red"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Green", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Green"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Blue", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Blue"), NULL);
         }
      }

      /* ---- Section: Monster Types ---- */
      y = pp_draw_section(panel_x, panel_right, y, draw_row, pp->scroll_offset,
                           panel_bottom, PPS_TXT_MONTYPES, "Monster Types (Levels)",
                           scope_types);
      draw_row++;

      if (pp->section_expanded[PPS_TXT_MONTYPES])
      {
         if (lv_row >= 0)
         {
            int mi;
            char mcol[12];
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "NumMon", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "NumMon"), NULL);
            for (mi = 1; mi <= 10; mi++)
            {
               sprintf(mcol, "mon%d", mi);
               y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                     panel_bottom, mcol, pp_txt_get_num(lv, RQ_LEVELS, lv_row, mcol), NULL);
            }
            for (mi = 1; mi <= 10; mi++)
            {
               sprintf(mcol, "nmon%d", mi);
               y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                     panel_bottom, mcol, pp_txt_get_num(lv, RQ_LEVELS, lv_row, mcol), NULL);
            }
            for (mi = 1; mi <= 10; mi++)
            {
               sprintf(mcol, "umon%d", mi);
               y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                     panel_bottom, mcol, pp_txt_get_num(lv, RQ_LEVELS, lv_row, mcol), NULL);
            }
         }
      }

      /* ---- Section: Properties ---- */
      y = pp_draw_section(panel_x, panel_right, y, draw_row, pp->scroll_offset,
                           panel_bottom, PPS_TXT_PROPERTIES, "Properties (Levels)",
                           scope_types);
      draw_row++;

      if (pp->section_expanded[PPS_TXT_PROPERTIES])
      {
         if (lv_row >= 0)
         {
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "DrlgType", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "DrlgType"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "SubType", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "SubType"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "SubTheme", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "SubTheme"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Depend", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Depend"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Layer", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Layer"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Pal", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Pal"), NULL);
            y = pp_draw_field_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "LevelName", pp_txt_get_str(lv, RQ_LEVELS, lv_row, "LevelName"), NULL);
            y = pp_draw_field_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "LevelWarp", pp_txt_get_str(lv, RQ_LEVELS, lv_row, "LevelWarp"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "SoundEnv", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "SoundEnv"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Portal", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Portal"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "Position", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Position"), NULL);
            y = pp_draw_field_num_tagged(panel_x, width, y, draw_row++, pp->scroll_offset,
                  panel_bottom, "SaveMon", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "SaveMonsters"), NULL);
         }
      }

      } /* end scope badge string block */
   }

   /* Save total rows for keyboard navigation bounds */
   pp->total_rows = pp_row_idx;

   /* ---- Footer bar (always visible) ---- */
   {
      int footer_h = 22;
      int footer_y = panel_bottom - footer_h;
      ALLEGRO_COLOR fc_bg, fc_border, fc_text;

      /* Footer background */
      al_draw_filled_rectangle((float)panel_x, (float)footer_y,
                                (float)panel_right, (float)panel_bottom,
                                al_map_rgba(36, 30, 24, 230));
      al_draw_line((float)panel_x, (float)footer_y + 0.5f,
                   (float)panel_right, (float)footer_y + 0.5f,
                   al_map_rgb(60, 50, 40), 1.0f);

      if (pp->pending_count > 0)
      {
         char btn_text[48];
         int apply_x = panel_x + 6;
         int discard_x = panel_x + (panel_right - panel_x) / 2 + 4;
         int btn_w = (panel_right - panel_x) / 2 - 10;

         /* Apply button */
         sprintf(btn_text, "Apply (%d)", pp->pending_count);
         fc_bg = al_map_rgba(50, 90, 50, 220);
         fc_border = al_map_rgb(80, 140, 80);
         fc_text = al_map_rgb(200, 255, 200);
         al_draw_filled_rectangle((float)apply_x, (float)(footer_y + 3),
                                   (float)(apply_x + btn_w), (float)(footer_y + 18),
                                   fc_bg);
         al_draw_rectangle((float)apply_x + 0.5f, (float)(footer_y + 3) + 0.5f,
                            (float)(apply_x + btn_w) - 0.5f, (float)(footer_y + 18) - 0.5f,
                            fc_border, 1.0f);
         al_draw_textf(a5_font, fc_text,
                        (float)(apply_x + btn_w / 2), (float)(footer_y + 6),
                        ALLEGRO_ALIGN_CENTRE, "%s", btn_text);

         /* Discard button */
         fc_bg = al_map_rgba(90, 50, 50, 220);
         fc_border = al_map_rgb(140, 80, 80);
         fc_text = al_map_rgb(255, 200, 200);
         al_draw_filled_rectangle((float)discard_x, (float)(footer_y + 3),
                                   (float)(discard_x + btn_w), (float)(footer_y + 18),
                                   fc_bg);
         al_draw_rectangle((float)discard_x + 0.5f, (float)(footer_y + 3) + 0.5f,
                            (float)(discard_x + btn_w) - 0.5f, (float)(footer_y + 18) - 0.5f,
                            fc_border, 1.0f);
         al_draw_textf(a5_font, fc_text,
                        (float)(discard_x + btn_w / 2), (float)(footer_y + 6),
                        ALLEGRO_ALIGN_CENTRE, "Discard");
      }
      else
      {
         al_draw_textf(a5_font, al_map_rgb(80, 80, 80),
                        (float)((panel_x + panel_right) / 2), (float)(footer_y + 6),
                        ALLEGRO_ALIGN_CENTRE, "No changes  (Ctrl+Enter = Apply)");
      }
   }
}


/* Apply all pending changes. */
void props_panel_apply(void)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   int i, idx, txt_changed;

   txt_changed = 0;
   idx = pp->ds1_idx;
   if (idx < 0 || idx >= DS1_MAX)
   {
      pp->pending_count = 0;
      return;
   }

   for (i = 0; i < pp->pending_count; i++)
   {
      PP_PENDING_S * p = &pp->pending[i];

      if (p->field.source == PFS_DS1)
      {
         long val = atol(p->new_value);
         switch (p->field.col)
         {
            case 0:  /* filename — read-only, skip */
               break;
            case 1:  glb_ds1[idx].version  = val; break;
            case 2:  /* width — structural, warn */
               printf("props_panel: width change requires reload (not applied)\n");
               break;
            case 3:  /* height — structural, warn */
               printf("props_panel: height change requires reload (not applied)\n");
               break;
            case 4:  glb_ds1[idx].act      = val; break;
            case 5:  glb_ds1[idx].txt_act  = (int)val; break;
            case 6:  glb_ds1[idx].tag_type = val; break;
            case 7:  /* wall_num — structural */
               printf("props_panel: wall_num change requires reload (not applied)\n");
               break;
            case 8:  /* floor_num — structural */
               printf("props_panel: floor_num change requires reload (not applied)\n");
               break;
            case 9:  /* shadow_num — structural */
               break;
            case 10: /* tag_num — structural */
               break;
            case 11: /* obj_num — read-only */
               break;
         }
      }
      else if (p->field.source == PFS_LVLPREST ||
               p->field.source == PFS_LVLTYPES ||
               p->field.source == PFS_LEVELS)
      {
         /* TXT field write-back using generic cell editor.
          * We need the key column/value and target column name.
          * These are encoded: field.source tells us which TXT,
          * field.col was set to 0, field.sub_idx was 0.
          * We stored the field_idx in field.row, but we need
          * the actual TXT row and column name.
          * For now, we re-resolve from the area browser linkage. */
         int lvltype_id, lvlprest_def;
         RQ_ENUM rq = RQ_MAX;
         const char * key_col = NULL;
         int key_val = 0;

         pp_get_entry_link(&lvltype_id, &lvlprest_def);

         /* Determine which TXT file and key */
         switch (p->field.source)
         {
            case PFS_LVLPREST:
               rq = RQ_LVLPREST;
               key_col = "Def";
               key_val = lvlprest_def;
               break;
            case PFS_LVLTYPES:
               rq = RQ_LVLTYPE;
               key_col = "Id";
               key_val = lvltype_id;
               break;
            case PFS_LEVELS:
               rq = RQ_LEVELS;
               key_col = "Id";  /* Use unique level Id, not shared LevelType */
               key_val = p->key_val;  /* lv_id stored by click handler */
               break;
            default:
               break;
         }

         if (rq < RQ_MAX && key_col != NULL && p->col_name[0] != '\0')
         {
            if (ds1_manager_txt_set_cell(rq, key_col, p->key_val,
                                          p->col_name, p->new_value) == 0)
            {
               printf("props_panel: wrote %s.%s = '%s' (key %s=%d)\n",
                      key_col, p->col_name, p->new_value, key_col, p->key_val);
            }
            else
            {
               printf("props_panel: failed to write %s.%s\n",
                      key_col, p->col_name);
            }
            txt_changed = 1;
         }
      }
   }

   printf("props_panel: applied %d change(s)\n", pp->pending_count);
   fflush(stdout);

   /* Invalidate cached TXT buffers if any TXT files were modified */
   if (txt_changed)
   {
      if (glb_ds1edit.lvlprest_buff != NULL)
         glb_ds1edit.lvlprest_buff = txt_destroy(glb_ds1edit.lvlprest_buff);
      if (glb_ds1edit.lvltypes_buff != NULL)
         glb_ds1edit.lvltypes_buff = txt_destroy(glb_ds1edit.lvltypes_buff);
      if (glb_ds1edit.levels_buff != NULL)
         glb_ds1edit.levels_buff = txt_destroy(glb_ds1edit.levels_buff);
      printf("props_panel: invalidated TXT caches\n");
   }

   pp->pending_count = 0;
   pp->editing = FALSE;
   props_panel_calc_shared_counts();
}


/* Draw collapsed panel tab — thin "<" button on right edge. */
void props_panel_draw_tab(int height)
{
   int disp_w = al_get_display_width(a5_display);
   int top_bar_h = glb_ds1edit.show_2nd_row ? 20 : 9;
   int bottom_bar_h = 10;
   int panel_top = top_bar_h;
   int panel_bottom = height - bottom_bar_h;
   ALLEGRO_COLOR col_bg   = al_map_rgba(36, 30, 24, 180);
   ALLEGRO_COLOR col_text = al_map_rgb(160, 160, 160);

   al_draw_filled_rectangle((float)(disp_w - PP_TAB_W), (float)panel_top,
                             (float)disp_w, (float)panel_bottom, col_bg);
   al_draw_line((float)(disp_w - PP_TAB_W) + 0.5f, (float)panel_top,
                (float)(disp_w - PP_TAB_W) + 0.5f, (float)panel_bottom,
                al_map_rgb(60, 50, 40), 1.0f);
   al_draw_textf(a5_font, col_text,
                  (float)(disp_w - 12),
                  (float)((panel_top + panel_bottom) / 2 - 4),
                  0, "<");
}


/* Handle mouse click in properties panel.
 * Returns: -2=close, -1=no action. */
int props_panel_click(int mx, int my, int panel_w, int disp_h)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   int disp_w = al_get_display_width(a5_display);
   int panel_x = disp_w - panel_w;
   int top_bar_h = glb_ds1edit.show_2nd_row ? 20 : 9;
   int panel_top = top_bar_h;
   int bottom_bar_h = 10;
   int panel_bottom = disp_h - bottom_bar_h;
   int y, draw_row;
   int idx;

   /* Close button */
   if (my >= panel_top && my < panel_top + PP_HEADER_H && mx > disp_w - 20)
      return -2;

   if (!glb_ds1edit.has_loaded_ds1)
      return -1;

   idx = pp->ds1_idx;
   if (idx < 0 || idx >= DS1_MAX || glb_ds1[idx].name[0] == '\0')
      return -1;

   /* Walk the same layout as draw to find clicked row */
   y = panel_top + PP_HEADER_H + 4;
   draw_row = 0;

   /* DS1 Header section header */
   if (draw_row >= pp->scroll_offset && my >= y && my < y + PP_LINE_H)
   {
      pp->section_expanded[PPS_DS1_HEADER] = !pp->section_expanded[PPS_DS1_HEADER];
      return 0;
   }
   y += PP_LINE_H;
   draw_row++;

   /* DS1 Header fields — check for field clicks if expanded */
   if (pp->section_expanded[PPS_DS1_HEADER])
   {
      /* DS1 header field names for labeling */
      static const char * ds1_field_labels[] = {
         "file", "version", "width", "height", "act", "txt_act",
         "tag_type", "walls", "floors", "shadows", "tags", "objects"
      };
      int fi;
      int field_count = 12;
      for (fi = 0; fi < field_count; fi++)
      {
         if (draw_row >= pp->scroll_offset && my >= y && my < y + PP_LINE_H)
         {
            /* Clicked a DS1 header field — start editing */
            char cur_val[PP_EDIT_BUF_MAX];
            cur_val[0] = '\0';
            switch (fi)
            {
               case 0: strncpy(cur_val, glb_ds1[idx].filename, PP_EDIT_BUF_MAX-1); break;
               case 1: sprintf(cur_val, "%ld", glb_ds1[idx].version); break;
               case 2: sprintf(cur_val, "%ld", glb_ds1[idx].width); break;
               case 3: sprintf(cur_val, "%ld", glb_ds1[idx].height); break;
               case 4: sprintf(cur_val, "%ld", glb_ds1[idx].act); break;
               case 5: sprintf(cur_val, "%d", glb_ds1[idx].txt_act); break;
               case 6: sprintf(cur_val, "%ld", glb_ds1[idx].tag_type); break;
               case 7: sprintf(cur_val, "%d", glb_ds1[idx].wall_num); break;
               case 8: sprintf(cur_val, "%d", glb_ds1[idx].floor_num); break;
               case 9: sprintf(cur_val, "%d", glb_ds1[idx].shadow_num); break;
               case 10: sprintf(cur_val, "%d", glb_ds1[idx].tag_num); break;
               case 11: sprintf(cur_val, "%ld", glb_ds1[idx].obj_num); break;
            }

            pp->editing = TRUE;
            pp->edit_field.source = PFS_DS1;
            pp->edit_field.row = fi;  /* field_idx = fi for DS1 header */
            pp->edit_field.col = fi;
            pp->edit_field.sub_idx = 0;
            strncpy(pp->edit_buf, cur_val, PP_EDIT_BUF_MAX - 1);
            pp->edit_buf[PP_EDIT_BUF_MAX - 1] = '\0';
            pp->edit_cursor = (int)strlen(pp->edit_buf);

            /* Save old value for pending */
            if (pp->pending_count < PP_MAX_PENDING)
            {
               strncpy(pp->pending[pp->pending_count].old_value, cur_val,
                       PP_EDIT_BUF_MAX - 1);
            }
            return 1;
         }
         y += PP_LINE_H;
         draw_row++;

         /* Sync act button (only present if act != txt_act, after field index 5 = txt_act) */
         if (fi == 5 && glb_ds1[idx].txt_act > 0 &&
             glb_ds1[idx].act != glb_ds1[idx].txt_act)
         {
            if (draw_row >= pp->scroll_offset && my >= y && my < y + PP_LINE_H)
            {
               pp_sync_act_from_txt(idx);
               return 1;
            }
            y += PP_LINE_H;
            draw_row++;
         }
      }
   }

   /* DT1 Files section header */
   if (draw_row >= pp->scroll_offset && my >= y && my < y + PP_LINE_H)
   {
      pp->section_expanded[PPS_DS1_FILES] = !pp->section_expanded[PPS_DS1_FILES];
      return 0;
   }
   y += PP_LINE_H;
   draw_row++;

   /* DT1 file entries if expanded */
   if (pp->section_expanded[PPS_DS1_FILES])
   {
      int file_rows;

      /* Sync from LvlTypes button */
      if (draw_row >= pp->scroll_offset && my >= y && my < y + PP_LINE_H)
      {
         pp_sync_files_from_lvltypes(idx);
         return 1;
      }
      y += PP_LINE_H;
      draw_row++;

      file_rows = glb_ds1[idx].file_num > 0 ? (int)glb_ds1[idx].file_num : 1;
      y += PP_LINE_H * file_rows;
      draw_row += file_rows;
   }

   /* TXT sections — use pp_field_idx to track fields for click-to-edit.
    * We mirror the exact draw logic to get accurate field positions.
    * pp_field_idx must be set to 12 here (after DS1 header's 12 fields). */
   pp_field_idx = 12; /* DS1 header has 12 fields (indices 0-11) */
   {
      int lvltype_id, lvlprest_def;
      int lp_row, lt_row, lv_row;
      TXT_S * lp = glb_ds1edit.lvlprest_buff;
      TXT_S * lt = glb_ds1edit.lvltypes_buff;
      TXT_S * lv = glb_ds1edit.levels_buff;

      pp_get_entry_link(&lvltype_id, &lvlprest_def);
      lp_row = pp_find_txt_row(lp, RQ_LVLPREST, "Def", lvlprest_def);
      lt_row = pp_find_txt_row(lt, RQ_LVLTYPE, "Id", lvltype_id);
      lv_row = pp_find_txt_row(lv, RQ_LEVELS, "LevelType", lvltype_id);

      /* Helper macro: check section header click, then walk fields */
      #define PP_CLICK_SECTION(SEC) \
         if (draw_row >= pp->scroll_offset && my >= y && my < y + PP_LINE_H) { \
            pp->section_expanded[SEC] = !pp->section_expanded[SEC]; \
            return 0; } \
         y += PP_LINE_H; draw_row++;

      /* Helper: check a single field row click and start editing.
       * KEY_VAL is the row key for TXT write-back (Def, Id, etc.) */
      #define PP_CLICK_FIELD(SRC, KEY, COL_NAME, CUR_VAL) \
         { \
            int _fi = pp_field_idx++; \
            if (pp->section_expanded[cur_sec] && \
                draw_row >= pp->scroll_offset && my >= y && my < y + PP_LINE_H) { \
               pp->editing = TRUE; \
               pp->edit_field.source = SRC; \
               pp->edit_field.row = _fi; \
               pp->edit_field.col = 0; \
               pp->edit_field.sub_idx = 0; \
               strncpy(pp->edit_buf, CUR_VAL, PP_EDIT_BUF_MAX - 1); \
               pp->edit_buf[PP_EDIT_BUF_MAX - 1] = '\0'; \
               pp->edit_cursor = (int)strlen(pp->edit_buf); \
               if (pp->pending_count < PP_MAX_PENDING) { \
                  strncpy(pp->pending[pp->pending_count].old_value, CUR_VAL, PP_EDIT_BUF_MAX - 1); \
                  strncpy(pp->pending[pp->pending_count].col_name, COL_NAME, 31); \
                  pp->pending[pp->pending_count].col_name[31] = '\0'; \
                  pp->pending[pp->pending_count].key_val = KEY; \
               } \
               return 1; \
            } \
            if (pp->section_expanded[cur_sec]) { y += PP_LINE_H; draw_row++; } \
         }

      {
         PP_SECTION_E cur_sec;
         char vbuf[64];
         /* Use Levels.txt unique Id as key (not LevelType which can be shared) */
         int lv_id = (lv_row >= 0) ? (int)pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Id") : 0;

         /* Identity */
         cur_sec = PPS_TXT_IDENTITY;
         PP_CLICK_SECTION(PPS_TXT_IDENTITY)
         if (lv_row >= 0)
         {
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "Name", pp_txt_get_str(lv, RQ_LEVELS, lv_row, "Name"))
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Id"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "Id", vbuf)
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Act"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "Act", vbuf)
         }
         if (lp_row >= 0)
         {
            PP_CLICK_FIELD(PFS_LVLPREST, lvlprest_def, "Name", pp_txt_get_str(lp, RQ_LVLPREST, lp_row, "Name"))
            sprintf(vbuf, "%d", lvlprest_def);
            PP_CLICK_FIELD(PFS_LVLPREST, lvlprest_def, "Def", vbuf)
         }
         if (lt_row >= 0)
         {
            sprintf(vbuf, "%d", lvltype_id);
            PP_CLICK_FIELD(PFS_LVLTYPES, lvltype_id, "Id", vbuf)
         }

         /* Tilesets — section header + Dt1Mask + DT1 file rows */
         cur_sec = PPS_TXT_TILESETS;
         PP_CLICK_SECTION(PPS_TXT_TILESETS)
         if (pp->section_expanded[PPS_TXT_TILESETS])
         {
            if (lp_row >= 0)
            {
               sprintf(vbuf, "%ld", pp_txt_get_num(lp, RQ_LVLPREST, lp_row, "Dt1Mask"));
               PP_CLICK_FIELD(PFS_LVLPREST, lvlprest_def, "Dt1Mask", vbuf)
            }
            /* DT1 file rows — skip for now (complex, handle later) */
            if (lt_row >= 0)
            {
               int fi;
               char col_name[16];
               for (fi = 1; fi <= 32; fi++)
               {
                  const char * dt1_path;
                  sprintf(col_name, "File %d", fi);
                  dt1_path = pp_txt_get_str(lt, RQ_LVLTYPE, lt_row, col_name);
                  if (dt1_path[0] == '\0' || (dt1_path[0] == '0' && dt1_path[1] == '\0'))
                     continue;
                  if (fi > 1 && lp_row >= 0)
                  {
                     long mask = pp_txt_get_num(lp, RQ_LVLPREST, lp_row, "Dt1Mask");
                     if (!(mask & (1 << (fi - 1))))
                        continue;
                  }
                  PP_CLICK_FIELD(PFS_LVLTYPES, lvltype_id, col_name, dt1_path)
               }
            }
         }

         /* Layout */
         cur_sec = PPS_TXT_LAYOUT;
         PP_CLICK_SECTION(PPS_TXT_LAYOUT)
         if (lp_row >= 0 && pp->section_expanded[PPS_TXT_LAYOUT])
         {
            int slot;
            sprintf(vbuf, "%ld", pp_txt_get_num(lp, RQ_LVLPREST, lp_row, "LevelId"));
            PP_CLICK_FIELD(PFS_LVLPREST, lvlprest_def, "LevelId", vbuf)
            for (slot = 1; slot <= 6; slot++)
            {
               char fcol[8];
               const char * fp;
               sprintf(fcol, "File%d", slot);
               fp = pp_txt_get_str(lp, RQ_LVLPREST, lp_row, fcol);
               if (fp[0] == '\0' || (fp[0] == '0' && fp[1] == '\0'))
                  continue;
               PP_CLICK_FIELD(PFS_LVLPREST, lvlprest_def, fcol, fp)
            }
         }

         /* Room/Size */
         cur_sec = PPS_TXT_ROOMSIZE;
         PP_CLICK_SECTION(PPS_TXT_ROOMSIZE)
         if (lv_row >= 0 && pp->section_expanded[PPS_TXT_ROOMSIZE])
         {
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "SizeX"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "SizeX", vbuf)
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "SizeY"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "SizeY", vbuf)
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "OffsetX"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "OffsetX", vbuf)
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "OffsetY"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "OffsetY", vbuf)
         }

         /* Monsters */
         cur_sec = PPS_TXT_MONSTERS;
         PP_CLICK_SECTION(PPS_TXT_MONSTERS)
         if (lv_row >= 0 && pp->section_expanded[PPS_TXT_MONSTERS])
         {
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "MonLvl1"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "MonLvl1", vbuf)
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "MonLvl2"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "MonLvl2", vbuf)
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "MonLvl3"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "MonLvl3", vbuf)
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "MonDen"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "MonDen", vbuf)
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "MonDen(N)"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "MonDen(N)", vbuf)
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "MonDen(H)"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "MonDen(H)", vbuf)
         }

         /* Quests/Waypoints */
         cur_sec = PPS_TXT_QUESTS;
         PP_CLICK_SECTION(PPS_TXT_QUESTS)
         if (lv_row >= 0 && pp->section_expanded[PPS_TXT_QUESTS])
         {
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Waypoint"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "Waypoint", vbuf)
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "Quest"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "Quest", vbuf)
         }

         /* Visibility */
         cur_sec = PPS_TXT_VISIBILITY;
         PP_CLICK_SECTION(PPS_TXT_VISIBILITY)
         if (lv_row >= 0 && pp->section_expanded[PPS_TXT_VISIBILITY])
         {
            int vi;
            char vcol[8];
            for (vi = 0; vi < 8; vi++)
            {
               sprintf(vcol, "Vis%d", vi);
               sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, vcol));
               PP_CLICK_FIELD(PFS_LEVELS, lv_id, vcol, vbuf)
            }
            for (vi = 0; vi < 8; vi++)
            {
               sprintf(vcol, "Warp%d", vi);
               sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, vcol));
               PP_CLICK_FIELD(PFS_LEVELS, lv_id, vcol, vbuf)
            }
         }

         /* Environment */
         cur_sec = PPS_TXT_ENVIRONMENT;
         PP_CLICK_SECTION(PPS_TXT_ENVIRONMENT)
         if (lv_row >= 0 && pp->section_expanded[PPS_TXT_ENVIRONMENT])
         {
            static const char * env_cols[] = {
               "Rain", "Mud", "IsInside", "Teleport",
               "Intensity", "Red", "Green", "Blue", NULL
            };
            int ei;
            for (ei = 0; env_cols[ei] != NULL; ei++)
            {
               sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, env_cols[ei]));
               PP_CLICK_FIELD(PFS_LEVELS, lv_id, env_cols[ei], vbuf)
            }
         }

         /* Monster Types */
         cur_sec = PPS_TXT_MONTYPES;
         PP_CLICK_SECTION(PPS_TXT_MONTYPES)
         if (lv_row >= 0 && pp->section_expanded[PPS_TXT_MONTYPES])
         {
            int mi;
            char mcol[12];
            sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, "NumMon"));
            PP_CLICK_FIELD(PFS_LEVELS, lv_id, "NumMon", vbuf)
            for (mi = 1; mi <= 10; mi++)
            {
               sprintf(mcol, "mon%d", mi);
               sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, mcol));
               PP_CLICK_FIELD(PFS_LEVELS, lv_id, mcol, vbuf)
            }
            for (mi = 1; mi <= 10; mi++)
            {
               sprintf(mcol, "nmon%d", mi);
               sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, mcol));
               PP_CLICK_FIELD(PFS_LEVELS, lv_id, mcol, vbuf)
            }
            for (mi = 1; mi <= 10; mi++)
            {
               sprintf(mcol, "umon%d", mi);
               sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, mcol));
               PP_CLICK_FIELD(PFS_LEVELS, lv_id, mcol, vbuf)
            }
         }

         /* Properties */
         cur_sec = PPS_TXT_PROPERTIES;
         PP_CLICK_SECTION(PPS_TXT_PROPERTIES)
         if (lv_row >= 0 && pp->section_expanded[PPS_TXT_PROPERTIES])
         {
            static const char * prop_num_cols[] = {
               "DrlgType", "SubType", "SubTheme", "Depend",
               "Layer", "Pal", NULL
            };
            static const char * prop_str_cols[] = {
               "LevelName", "LevelWarp", NULL
            };
            static const char * prop_num2_cols[] = {
               "SoundEnv", "Portal", "Position", "SaveMonsters", NULL
            };
            int pi;
            for (pi = 0; prop_num_cols[pi] != NULL; pi++)
            {
               sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, prop_num_cols[pi]));
               PP_CLICK_FIELD(PFS_LEVELS, lv_id, prop_num_cols[pi], vbuf)
            }
            for (pi = 0; prop_str_cols[pi] != NULL; pi++)
            {
               PP_CLICK_FIELD(PFS_LEVELS, lv_id, prop_str_cols[pi],
                  pp_txt_get_str(lv, RQ_LEVELS, lv_row, prop_str_cols[pi]))
            }
            for (pi = 0; prop_num2_cols[pi] != NULL; pi++)
            {
               sprintf(vbuf, "%ld", pp_txt_get_num(lv, RQ_LEVELS, lv_row, prop_num2_cols[pi]));
               PP_CLICK_FIELD(PFS_LEVELS, lv_id, prop_num2_cols[pi], vbuf)
            }
         }
      }

      #undef PP_CLICK_SECTION
      #undef PP_CLICK_FIELD
   }

   /* Footer bar click — Apply and Discard buttons */
   {
      int footer_h = 22;
      int footer_y = panel_bottom - footer_h;
      if (my >= footer_y && my < panel_bottom && pp->pending_count > 0)
      {
         int mid_x = panel_x + (disp_w - panel_x) / 2;
         if (mx < mid_x)
         {
            /* Apply button (left half) */
            props_panel_apply();
         }
         else
         {
            /* Discard button (right half) */
            pp->pending_count = 0;
            pp->editing = FALSE;
            printf("props_panel: discarded all pending changes\n");
            fflush(stdout);
         }
         return 1;
      }
   }

   /* Click outside any field — cancel editing if active */
   if (pp->editing)
      pp->editing = FALSE;

   (void)panel_x;

   return -1;
}


/* Handle mouse wheel scroll in properties panel. Returns 1 if handled. */
int props_panel_scroll(int dz)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   pp->scroll_offset -= dz * 3;
   if (pp->scroll_offset < 0)
      pp->scroll_offset = 0;
   return 1;
}


/* Commit the current edit buffer as a pending change. */
static void pp_commit_edit(void)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   int pend_idx;

   if (!pp->editing) return;

   /* Check if already have a pending entry for this field */
   pend_idx = pp_find_pending(pp->edit_field.row);
   if (pend_idx >= 0)
   {
      /* Update existing pending entry */
      strncpy(pp->pending[pend_idx].new_value, pp->edit_buf, PP_EDIT_BUF_MAX - 1);
      pp->pending[pend_idx].new_value[PP_EDIT_BUF_MAX - 1] = '\0';
   }
   else
   {
      /* Add new pending entry (old_value, col_name, key_val were pre-populated
       * by the click handler into pp->pending[pp->pending_count]) */
      if (pp->pending_count < PP_MAX_PENDING)
      {
         PP_PENDING_S * p = &pp->pending[pp->pending_count];
         /* Check if value actually changed from old_value */
         if (strcmp(pp->edit_buf, p->old_value) != 0)
         {
            p->field = pp->edit_field;
            strncpy(p->new_value, pp->edit_buf, PP_EDIT_BUF_MAX - 1);
            p->new_value[PP_EDIT_BUF_MAX - 1] = '\0';
            pp->pending_count++;
         }
      }
   }

   pp->editing = FALSE;
}

/* Handle key character input — navigation when not editing, text input when editing. */
void props_panel_handle_keychar(int unichar, int keycode)
{
   PROPS_PANEL_S * pp = &glb_ds1edit.props_panel;
   int len;

   /* --- Navigation mode (not editing) --- */
   if (!pp->editing)
   {
      switch (keycode)
      {
         case ALLEGRO_KEY_UP:
            if (pp->focused_row > 0)
               pp->focused_row--;
            pp->has_focus = TRUE;
            break;

         case ALLEGRO_KEY_DOWN:
            if (pp->focused_row < pp->total_rows - 1)
               pp->focused_row++;
            pp->has_focus = TRUE;
            break;

         case ALLEGRO_KEY_LEFT:
            /* Collapse the current or nearest section */
            {
               int si;
               for (si = pp_section_count - 1; si >= 0; si--)
               {
                  if (pp_section_rows[si] <= pp->focused_row)
                  {
                     pp->section_expanded[pp_section_ids[si]] = FALSE;
                     pp->focused_row = pp_section_rows[si];
                     break;
                  }
               }
            }
            break;

         case ALLEGRO_KEY_RIGHT:
            /* Expand the section at focused row (if it's a header) */
            {
               int si;
               for (si = 0; si < pp_section_count; si++)
               {
                  if (pp_section_rows[si] == pp->focused_row)
                  {
                     pp->section_expanded[pp_section_ids[si]] = TRUE;
                     break;
                  }
               }
            }
            break;

         case ALLEGRO_KEY_PGUP:
            /* Jump to previous section header */
            {
               int si;
               for (si = pp_section_count - 1; si >= 0; si--)
               {
                  if (pp_section_rows[si] < pp->focused_row)
                  {
                     pp->focused_row = pp_section_rows[si];
                     break;
                  }
               }
            }
            pp->has_focus = TRUE;
            break;

         case ALLEGRO_KEY_PGDN:
            /* Jump to next section header */
            {
               int si;
               for (si = 0; si < pp_section_count; si++)
               {
                  if (pp_section_rows[si] > pp->focused_row)
                  {
                     pp->focused_row = pp_section_rows[si];
                     break;
                  }
               }
            }
            pp->has_focus = TRUE;
            break;

         case ALLEGRO_KEY_HOME:
            pp->focused_row = 0;
            pp->has_focus = TRUE;
            break;

         case ALLEGRO_KEY_END:
            pp->focused_row = pp->total_rows - 1;
            pp->has_focus = TRUE;
            break;

         case ALLEGRO_KEY_TAB:
            /* Tab to next editable field, Shift+Tab to previous */
            {
               int dir = 1; /* forward by default */
               int r;
               ALLEGRO_KEYBOARD_STATE ks;
               al_get_keyboard_state(&ks);
               if (al_key_down(&ks, ALLEGRO_KEY_LSHIFT) ||
                   al_key_down(&ks, ALLEGRO_KEY_RSHIFT))
                  dir = -1;

               r = pp->focused_row + dir;
               while (r >= 0 && r < pp->total_rows)
               {
                  /* Check if this row is an editable field (not read-only, not section header) */
                  int is_section = 0;
                  int si;
                  for (si = 0; si < pp_section_count; si++)
                  {
                     if (pp_section_rows[si] == r)
                     { is_section = 1; break; }
                  }
                  if (!is_section)
                  {
                     pp->focused_row = r;
                     break;
                  }
                  r += dir;
               }
            }
            pp->has_focus = TRUE;
            break;

         case ALLEGRO_KEY_ENTER:
         case ALLEGRO_KEY_PAD_ENTER:
            /* If focused row is a section header, toggle expand/collapse.
             * Otherwise, simulate a click to start editing. */
            {
               int si, is_section = 0;
               for (si = 0; si < pp_section_count; si++)
               {
                  if (pp_section_rows[si] == pp->focused_row)
                  {
                     pp->section_expanded[pp_section_ids[si]] =
                        !pp->section_expanded[pp_section_ids[si]];
                     is_section = 1;
                     break;
                  }
               }
               /* If not a section header and not read-only, start editing
                * by calling the click handler with a synthetic position.
                * We use focused_row to derive what field it is — the click
                * handler will walk the same layout and find the match.
                * For simplicity, just call props_panel_click with the panel
                * center and a y that corresponds to the focused row. */
               if (!is_section)
               {
                  int disp_w = al_get_display_width(a5_display);
                  int disp_h = al_get_display_height(a5_display);
                  int top_h = glb_ds1edit.show_2nd_row ? 20 : 9;
                  int click_x = disp_w - glb_ds1edit.props_panel_width / 2;
                  int click_y = top_h + PP_HEADER_H + 4
                                + pp->focused_row * PP_LINE_H
                                - pp->scroll_offset * PP_LINE_H;
                  props_panel_click(click_x, click_y,
                                    glb_ds1edit.props_panel_width, disp_h);
               }
            }
            pp->has_focus = TRUE;
            break;

         case ALLEGRO_KEY_ESCAPE:
            pp->has_focus = FALSE;
            break;
      }
      return;
   }

   /* --- Editing mode --- */
   len = (int)strlen(pp->edit_buf);

   switch (keycode)
   {
      case ALLEGRO_KEY_ENTER:
      case ALLEGRO_KEY_PAD_ENTER:
         pp_commit_edit();
         break;

      case ALLEGRO_KEY_ESCAPE:
         pp->editing = FALSE;
         break;

      case ALLEGRO_KEY_BACKSPACE:
         if (pp->edit_cursor > 0)
         {
            memmove(pp->edit_buf + pp->edit_cursor - 1,
                    pp->edit_buf + pp->edit_cursor,
                    len - pp->edit_cursor + 1);
            pp->edit_cursor--;
         }
         break;

      case ALLEGRO_KEY_DELETE:
         if (pp->edit_cursor < len)
         {
            memmove(pp->edit_buf + pp->edit_cursor,
                    pp->edit_buf + pp->edit_cursor + 1,
                    len - pp->edit_cursor);
         }
         break;

      case ALLEGRO_KEY_LEFT:
         if (pp->edit_cursor > 0)
            pp->edit_cursor--;
         break;

      case ALLEGRO_KEY_RIGHT:
         if (pp->edit_cursor < len)
            pp->edit_cursor++;
         break;

      case ALLEGRO_KEY_HOME:
         pp->edit_cursor = 0;
         break;

      case ALLEGRO_KEY_END:
         pp->edit_cursor = len;
         break;

      default:
         /* Insert printable character */
         if (unichar >= 32 && unichar < 127 && len < PP_EDIT_BUF_MAX - 2)
         {
            memmove(pp->edit_buf + pp->edit_cursor + 1,
                    pp->edit_buf + pp->edit_cursor,
                    len - pp->edit_cursor + 1);
            pp->edit_buf[pp->edit_cursor] = (char)unichar;
            pp->edit_cursor++;
         }
         break;
   }
}
