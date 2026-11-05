/* Derived from win_ds1edit by Paul Siramy (originally interfac.c).
 * See NOTICE at the repository root for attribution and license status. */

#include "structs.h"
#include "render/preview.h"
#include "editor/objects.h"
#include "misc.h"
#include "core/ds1.h"
#include "ui/dialogs.h"
#include "editor/tiles.h"
#include "editor/undo.h"
#include "core/animdata.h"
#include "core/cof.h"
#include "core/txtread.h"
#include "ui/bits_window.h"
#include "ui/edit_window.h"
#include "ui/dialogs.h"
#include "ui/interface.h"
#include "core/area_browser.h"
#include "core/ds1_manager.h"
#include "ui/props_panel.h"
#include "ui/ui_layout.h"
#include "ui/tile_picker.h"
#include "ui/project_menu.h"
#include "ui/preset_picker.h"
#include "ui/commands.h"
#include "ui/menu_bar.h"
#include "core/export_common.h"
#include "core/palette.h"
#include "ui/input.h"
#include "ui/selftest_verdict.h"
#include "core/ds1_save_io.h"
#include "ui/map_save_ui.h"
#include "ui/map_dialog.h"
#include "ui/fields_dialog.h"
#include "core/map_state.h"
#include "core/project.h"
#include "core/mpq_build.h"
#include "core/game_launch.h"
#include <allegro5/allegro_native_dialog.h>

/* Ask, naming the map and what will happen. */
static int interface_confirm_map_op(const char *title, const char *heading_fmt,
                                    const char *path, const char *text, int warn_unsaved)
{
   char heading[600], body[1024];
   int rc, k, dirty = 0;

   for (k = 0; k < DS1_MAX && warn_unsaved && !dirty; k++)
      dirty = glb_ds1[k].name[0] != 0 && map_is_dirty(k) &&
              map_state_path_matches(glb_ds1[k].name, path);

   snprintf(heading, sizeof(heading), heading_fmt, path);
   snprintf(body, sizeof(body), "%s%s", text,
            dirty ? "\n\nThis map has unsaved edits, which won't be saved." : "");
   rc = al_show_native_message_box(a5_display, title, heading, body, NULL,
                                   ALLEGRO_MESSAGEBOX_YES_NO | ALLEGRO_MESSAGEBOX_WARN);
   input_suppress_held();
   input_discard_pending();
   return rc == 1;
}

/* The box said unsaved edits to this map won't be saved: every slot that has it
 * open stops asking, so a later Save All can't write it back. */
static void interface_forget_edits(const char *path)
{
   int k;

   for (k = 0; k < DS1_MAX; k++)
      if (glb_ds1[k].name[0] != 0 && map_state_path_matches(glb_ds1[k].name, path))
         map_dirty_mark_saved(k);
}

/* Every slot showing this map now shows a file that's gone, so a real save is
 * refused until it's reopened: it would recreate a file nothing lists, and block
 * Restore from Backup. Loading or clearing a slot resets this. */
static void interface_set_detached(const char *path, int detached)
{
   int k;

   for (k = 0; k < DS1_MAX; k++)
      if (glb_ds1[k].name[0] != 0 && map_state_path_matches(glb_ds1[k].name, path))
         glb_ds1[k].detached = detached;
}

static void interface_map_op_failed(const char *title, const char *heading, const char *error)
{
   al_show_native_message_box(a5_display, title, heading, error, NULL, ALLEGRO_MESSAGEBOX_ERROR);
   input_suppress_held();
   input_discard_pending();
}

/* Delete a New map from the project. Undo can't bring it back: this is a file
 * operation, not a map edit, so it takes a confirmation, and Restore from Backup. */
static void interface_delete_map(int gi, int ei)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   char error[512] = "", path[256];

   snprintf(path, sizeof(path), "%s", ab->groups[gi].entries[ei].ds1_path);
   if (ds1_manager_delete_check(gi, ei, error, sizeof(error)) != 0)
   {
      interface_map_op_failed("Delete Map", "This map can't be deleted.", error);
      return;
   }
   if (!interface_confirm_map_op("Delete Map", "Delete %s?", path,
         "This moves the map's file to the project's backup folder and takes it out of "
         "its row in LvlPrest.txt. Map > Restore from Backup brings it back.", TRUE))
      return;

   /* ds1_manager_delete removes the row, then finds the open map's row again by
      path. If this was the open map, it has none now and stays in memory. */
   if (ds1_manager_delete(gi, ei, error, sizeof(error)) != 0)
   {
      interface_map_op_failed("Delete Map", "The map wasn't deleted.", error);
      return;
   }
   interface_forget_edits(path);
   interface_set_detached(path, TRUE);
}

static void interface_revert_map(int gi, int ei)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   char error[512] = "", path[256];
   int was_open = ab->loaded_group == gi && ab->loaded_entry == ei;
   int def = ab->groups[gi].entries[ei].lvlprest_def;

   snprintf(path, sizeof(path), "%s", ab->groups[gi].entries[ei].ds1_path);
   if (!interface_confirm_map_op("Revert to Game Version", "Revert %s to the game's version?", path,
         "This moves the project's copy to the project's backup folder. The game's "
         "version is used from now on. Map > Restore from Backup brings the copy back.", TRUE))
      return;

   if (ds1_manager_revert(gi, ei, error, sizeof(error)) != 0)
   {
      interface_map_op_failed("Revert to Game Version", "The map wasn't reverted.", error);
      return;
   }

   interface_forget_edits(path);
   interface_set_detached(path, TRUE);

   /* Show what was just reverted to, which reloads every slot. Other open maps with
      unsaved edits prompt here, and a cancel leaves this one showing until it's
      reopened. */
   if (was_open && area_browser_find_row(path, def, &gi, &ei))
      area_browser_switch_single(gi, ei);
}

static void interface_remove_map(int gi, int ei)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   char error[512] = "", path[256];
   int def = ab->groups[gi].entries[ei].lvlprest_def;

   snprintf(path, sizeof(path), "%s", ab->groups[gi].entries[ei].ds1_path);
   if (!map_save_ui_require_project())
      return;

   /* Opening a project rebuilds the rows, so find this one again, and only act
      on it if it's still a game map with no project change. */
   if (!area_browser_find_row(path, def, &gi, &ei) ||
       ab->groups[gi].entries[ei].state != MAP_STATE_GAME)
   {
      interface_map_op_failed("Remove from Project", "Nothing was removed.",
         "In this project the map isn't a plain game map any more. Check its row.");
      return;
   }
   if (ds1_manager_remove_check(gi, ei, error, sizeof(error)) != 0)
   {
      interface_map_op_failed("Remove from Project", "This map can't be removed.", error);
      return;
   }
   if (!interface_confirm_map_op("Remove from Project", "Remove %s from the project?", path,
         "This takes it out of the project's LvlPrest.txt, so the game won't use "
         "this map with your mod. The game's files don't change. Map > Restore Slot "
         "puts it back.", FALSE))
      return;

   if (ds1_manager_remove_slot(gi, ei, error, sizeof(error)) != 0)
      interface_map_op_failed("Remove from Project", "The map wasn't taken out of the row.", error);
}

static void interface_restore_slot(int gi, int ei)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   char error[512] = "", path[256];

   snprintf(path, sizeof(path), "%s", ab->groups[gi].entries[ei].ds1_path);
   if (!interface_confirm_map_op("Restore Slot", "Restore %s's slot?", path,
         "This puts the game's value back in the project's LvlPrest.txt.", FALSE))
      return;
   if (ds1_manager_restore_slot(gi, ei, error, sizeof(error)) != 0)
      interface_map_op_failed("Restore Slot", "The slot wasn't restored.", error);
}

static void interface_restore_backup(int gi, int ei)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   char error[512] = "", path[256];
   int i, j, found = 0, was_open, row_gi, row_ei;

   snprintf(path, sizeof(path), "%s", ab->groups[gi].entries[ei].ds1_path);
   if (!map_save_ui_require_project())
      return;

   /* Opening a project rebuilds the rows, so find this one again. */
   for (i = 0; i < ab->group_count && !found; i++)
      for (j = 0; ab->groups[i].is_backup && j < ab->groups[i].entry_count && !found; j++)
         if (strcmp(ab->groups[i].entries[j].ds1_path, path) == 0)
         {
            gi = i;
            ei = j;
            found = 1;
         }
   if (!found)
      return;

   was_open = ab->loaded_group == gi && ab->loaded_entry == ei;
   if (!interface_confirm_map_op("Restore from Backup", "Restore the backup %s?", path,
         "This moves it back to the project and registers it in LvlPrest.txt if needed.", TRUE))
      return;
   if (ds1_manager_restore(gi, ei, &row_gi, &row_ei, error, sizeof(error)) != 0)
   {
      interface_map_op_failed("Restore from Backup", "The backup wasn't restored.", error);
      return;
   }

   /* The backup's own file has gone, and the box said its edits wouldn't be saved.
      A slot still showing the deleted map can save again, now it has a file. */
   interface_forget_edits(path);
   interface_set_detached(path, TRUE);
   if (row_gi >= 0)
      interface_set_detached(ab->groups[row_gi].entries[row_ei].ds1_path, FALSE);

   /* Like Revert, show what's there now. A cancel at the unsaved prompt leaves the
      backup showing, refusing saves, until it's reopened. */
   if (was_open && row_gi >= 0)
      area_browser_switch_single(row_gi, row_ei);
}

/* Delete a backup for good. It may be the only copy of a deleted map. */
static void interface_delete_backup(int gi, int ei)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   char heading[600], path[256];
   int rc;

   snprintf(path, sizeof(path), "%s", ab->groups[gi].entries[ei].ds1_path);
   snprintf(heading, sizeof(heading), "Permanently delete the backup %s?", path);
   rc = al_show_native_message_box(a5_display, "Delete Backup", heading,
      "This can't be undone or restored.",
      NULL, ALLEGRO_MESSAGEBOX_YES_NO | ALLEGRO_MESSAGEBOX_WARN);
   input_suppress_held();
   input_discard_pending();
   if (rc != 1)
      return;

   if (ds1_manager_delete_permanent(gi, ei) == 0)
   {
      interface_set_detached(path, TRUE);
      printf("Backup deleted permanently\n");
   }
}

typedef struct
{
   double event_ms_total;
   double input_ms_total;
   double mouse_to_tile_ms_total;
   double editobj_ms_total;
   double anim_ms_total;
   double render_ms_total;
   double present_ms_total;
   double ui_ms_total;
   double frame_ms_total;

   double event_ms_max;
   double input_ms_max;
   double mouse_to_tile_ms_max;
   double editobj_ms_max;
   double anim_ms_max;
   double render_ms_max;
   double present_ms_max;
   double ui_ms_max;
   double frame_ms_max;

   int frames;
} PERF_STATS_S;

static PERF_STATS_S glb_perf_stats;

static double perf_now_ms(void)
{
   return al_get_time() * 1000.0;
}

static void perf_accumulate(double *total, double *max, double dt_ms)
{
   (*total) += dt_ms;
   if (dt_ms > *max)
      *max = dt_ms;
}

static void st_fold_sections(void);

static void perf_print_summary(void)
{
   double inv;

   if (glb_perf_stats.frames <= 0)
      return;

   inv = 1.0 / glb_perf_stats.frames;

   fprintf(stderr, "\n[perf] last %d frames\n", glb_perf_stats.frames);
   fprintf(stderr, "[perf] frame:       avg %7.2f ms  max %7.2f ms  (%.2f FPS)\n",
           glb_perf_stats.frame_ms_total * inv,
           glb_perf_stats.frame_ms_max,
           (glb_perf_stats.frame_ms_total > 0.0) ? (1000.0 * glb_perf_stats.frames / glb_perf_stats.frame_ms_total) : 0.0);
   fprintf(stderr, "[perf] events:      avg %7.2f ms  max %7.2f ms\n",
           glb_perf_stats.event_ms_total * inv, glb_perf_stats.event_ms_max);
   fprintf(stderr, "[perf] input:       avg %7.2f ms  max %7.2f ms\n",
           glb_perf_stats.input_ms_total * inv, glb_perf_stats.input_ms_max);
   fprintf(stderr, "[perf] mouse->tile: avg %7.2f ms  max %7.2f ms\n",
           glb_perf_stats.mouse_to_tile_ms_total * inv, glb_perf_stats.mouse_to_tile_ms_max);
   fprintf(stderr, "[perf] editobj:     avg %7.2f ms  max %7.2f ms\n",
           glb_perf_stats.editobj_ms_total * inv, glb_perf_stats.editobj_ms_max);
   fprintf(stderr, "[perf] anim:        avg %7.2f ms  max %7.2f ms\n",
           glb_perf_stats.anim_ms_total * inv, glb_perf_stats.anim_ms_max);
   fprintf(stderr, "[perf] render:      avg %7.2f ms  max %7.2f ms\n",
           glb_perf_stats.render_ms_total * inv, glb_perf_stats.render_ms_max);
   fprintf(stderr, "[perf] present:     avg %7.2f ms  max %7.2f ms\n",
           glb_perf_stats.present_ms_total * inv, glb_perf_stats.present_ms_max);
   fprintf(stderr, "[perf] ui/other:    avg %7.2f ms  max %7.2f ms\n",
           glb_perf_stats.ui_ms_total * inv, glb_perf_stats.ui_ms_max);
   fflush(stderr);

   st_fold_sections();
   memset(&glb_perf_stats, 0, sizeof(glb_perf_stats));
}

/* ---- --selftest frame-time summary ------------------------------------
   Per-frame render and present times are kept for the whole run so the report
   can give percentiles, not just a mean that one slow frame hides. Section
   totals are folded in from glb_perf_stats because perf_print_summary()
   zeroes it periodically. */
static double *st_render = NULL, *st_present = NULL;
static int st_n = 0, st_cap = 0;
static double st_t0 = 0.0;
static double st_cpu_ms_total = 0.0, st_cpu_t0 = 0.0;
static int st_win_w = 0, st_win_h = 0;
static double st_sec_total[9];
static double st_sec_max[9];
static const char *const st_sec_name[9] = {
   "events", "input", "mouse_to_tile", "editobj", "anim",
   "render", "present", "ui_other", "frame"};

static void st_fold_sections(void)
{
   const PERF_STATS_S *p = &glb_perf_stats;
   const double tot[9] = {p->event_ms_total, p->input_ms_total,
      p->mouse_to_tile_ms_total, p->editobj_ms_total, p->anim_ms_total,
      p->render_ms_total, p->present_ms_total, p->ui_ms_total, p->frame_ms_total};
   const double mx[9] = {p->event_ms_max, p->input_ms_max,
      p->mouse_to_tile_ms_max, p->editobj_ms_max, p->anim_ms_max,
      p->render_ms_max, p->present_ms_max, p->ui_ms_max, p->frame_ms_max};
   int i;

   for (i = 0; i < 9; i++)
   {
      st_sec_total[i] += tot[i];
      if (mx[i] > st_sec_max[i])
         st_sec_max[i] = mx[i];
   }
}

static void st_record(double render_ms, double present_ms)
{
   if (st_n == st_cap)
   {
      int cap = st_cap ? st_cap * 2 : 1024;
      double *r = (double *)realloc(st_render, cap * sizeof(double));
      double *q = (double *)realloc(st_present, cap * sizeof(double));
      if (r != NULL)
         st_render = r;
      if (q != NULL)
         st_present = q;
      if (r == NULL || q == NULL)
         return;   /* a short sample beats a crash in a test harness */
      st_cap = cap;
   }
   st_render[st_n] = render_ms;
   st_present[st_n] = present_ms;
   st_n++;
}

static int st_cmp(const void *a, const void *b)
{
   double x = *(const double *)a, y = *(const double *)b;
   return (x > y) - (x < y);
}

typedef struct { double avg, p50, p95, p99, max; } ST_DIST_S;

static ST_DIST_S st_dist(const double *v, int n)
{
   ST_DIST_S d = {0, 0, 0, 0, 0};
   double *c, sum = 0.0;
   int i;

   if (n <= 0)
      return d;
   c = (double *)malloc(n * sizeof(double));
   if (c == NULL)
      return d;
   memcpy(c, v, n * sizeof(double));
   qsort(c, n, sizeof(double), st_cmp);
   for (i = 0; i < n; i++)
      sum += c[i];
   d.avg = sum / n;
   d.p50 = c[(int)(0.50 * (n - 1) + 0.5)];
   d.p95 = c[(int)(0.95 * (n - 1) + 0.5)];
   d.p99 = c[(int)(0.99 * (n - 1) + 0.5)];
   d.max = c[n - 1];
   free(c);
   return d;
}

static void st_json_string(FILE *f, const char *s)
{
   fputc('"', f);
   for (; s != NULL && *s; s++)
   {
      if (*s == '"' || *s == '\\')
      {
         fputc('\\', f);
         fputc(*s, f);
      }
      else if ((unsigned char)*s < 0x20)
         fputc(' ', f);
      else
         fputc(*s, f);
   }
   fputc('"', f);
}

/* Prints the summary and, with --selftest-report, writes it as JSON. */
static void st_report(int win_w, int win_h, double wall_s, int font_bad,
                      const char *failure)
{
   ST_DIST_S r = st_dist(st_render, st_n);
   ST_DIST_S pr = st_dist(st_present, st_n);
   int i, slow = -1;
   double slow_avg = 0.0;
   const char *path = glb_ds1edit.cmd_line.selftest_report;
   double fps = (wall_s > 0.0) ? st_n / wall_s : 0.0;

   st_fold_sections();
   for (i = 0; i < 8; i++)   /* "frame" is the sum, not a section */
   {
      double a = (st_n > 0) ? st_sec_total[i] / st_n : 0.0;
      if (a > slow_avg)
      {
         slow_avg = a;
         slow = i;
      }
   }

   fprintf(stdout, "selftest: render avg %.2f ms/frame\n", r.avg);
   fprintf(stdout,
           "selftest: frames=%d wall_fps=%.1f window=%dx%d driver=\"%s\"\n",
           st_n, fps, win_w, win_h, glb_display_driver);
   fprintf(stdout,
           "selftest: render ms avg %.2f p50 %.2f p95 %.2f p99 %.2f max %.2f\n",
           r.avg, r.p50, r.p95, r.p99, r.max);
   fprintf(stdout, "selftest: present ms avg %.2f max %.2f\n", pr.avg, pr.max);
   fprintf(stdout, "selftest: frame cpu ms avg %.2f (main thread)\n",
           st_n > 0 ? st_cpu_ms_total / st_n : 0.0);
   if (slow >= 0)
      fprintf(stdout, "selftest: slowest section %s avg %.2f ms\n",
              st_sec_name[slow], slow_avg);
   fprintf(stdout, "selftest: memory-bitmap draws=%u font_memory=%d\n",
           a5_gpu_guard_hits, font_bad);

   if (path != NULL)
   {
      FILE *f = fopen(path, "wt");
      if (f == NULL)
      {
         fprintf(stdout, "selftest: could not write report %s\n", path);
         return;
      }
      fprintf(f, "{\n  \"schema\": \"ds1edit.fps/1\",\n  \"build\": ");
      st_json_string(f, ds1edit_build_identity());
      fprintf(f, ",\n  \"driver\": ");
      st_json_string(f, glb_display_driver);
      fprintf(f, ",\n  \"window\": {\"width\": %d, \"height\": %d},\n",
              win_w, win_h);
      fprintf(f, "  \"frames\": %d,\n  \"wall_seconds\": %.4f,\n"
                 "  \"wall_fps\": %.2f,\n", st_n, wall_s, fps);
      fprintf(f, "  \"render_ms\": {\"avg\": %.4f, \"p50\": %.4f, "
                 "\"p95\": %.4f, \"p99\": %.4f, \"max\": %.4f},\n",
              r.avg, r.p50, r.p95, r.p99, r.max);
      fprintf(f, "  \"present_ms\": {\"avg\": %.4f, \"max\": %.4f},\n",
              pr.avg, pr.max);
      fprintf(f, "  \"frame_cpu_ms_avg\": %.4f,\n",
              st_n > 0 ? st_cpu_ms_total / st_n : 0.0);
      if (slow >= 0)
         fprintf(f, "  \"slowest_section\": {\"name\": \"%s\", "
                    "\"avg_ms\": %.4f},\n", st_sec_name[slow], slow_avg);
      else
         fprintf(f, "  \"slowest_section\": null,\n");
      fprintf(f, "  \"memory_bitmap_draws\": %u,\n"
                 "  \"font_memory_bitmap\": %s,\n",
              a5_gpu_guard_hits, font_bad ? "true" : "false");
      fprintf(f, "  \"ok\": %s,\n  \"failure\": ", failure == NULL ? "true" : "false");
      if (failure == NULL)
         fprintf(f, "null\n}\n");
      else
      {
         st_json_string(f, failure);
         fprintf(f, "\n}\n");
      }
      fclose(f);
   }
}

/* Decide whether the editor may close. Only unsaved maps need a question.
 * Returns TRUE to close. */
static int interface_confirm_quit(void)
{
   int i;

   if (!input_quit_needs_prompt(map_any_dirty(),
                                glb_ds1edit.props_panel.pending_count))
      return TRUE;

   /* The close request has been taken; the dialog must not see it as well. */
   a5_display_closed = 0;

   switch (msg_quit_main())
   {
   case 0:
      // stay open if any map could not be saved
      return map_save_ui_save_dirty() == 0;

   case 1:
      for (i = 0; i < DS1_MAX; i++)
         if (map_is_dirty(i))
            map_save_ui_save(i, TRUE); // save a .TMP map
      return TRUE;

   default:
      /* Cancel. That includes -1: another window close while the dialog was up.
         It stays pending, so the next frame asks again. */
      return FALSE;
   }
}

/* Start a copy or cut: the selection follows the mouse until a click pastes it. */
static void interface_begin_paste(int ds1_idx, int cx, int cy, PASTE_POS_S *paste, int is_cut)
{
   int i;

   if (paste->start != FALSE)
      return;
   for (i = 0; i < DS1_MAX; i++)
   {
      if (strlen(glb_ds1[i].name))
         edittile_paste_prepare(i);
   }
   paste->src_ds1_idx = ds1_idx;
   paste->old_ds1_idx = ds1_idx;
   paste->start = TRUE;
   paste->is_cut = is_cut;
   paste->cut_saved = FALSE;
   paste->old_x = cx;
   paste->old_y = cy;
   edittile_middle_select(ds1_idx, &paste->start_x, &paste->start_y);
   edittile_paste_preview(ds1_idx, cx - paste->start_x, cy - paste->start_y, paste);
}

/* Does the browser's loaded row name the map open in slot ds1_idx? A map opened
 * from the preset picker or switched to with a number key can leave the row
 * pointing at another map, and a backup row never names an open map. */
static int interface_loaded_row_is_open_map(int ds1_idx)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;

   if (!glb_ds1edit.has_loaded_ds1 || ds1_idx < 0 || ds1_idx >= DS1_MAX ||
       glb_ds1[ds1_idx].name[0] == '\0')
      return FALSE;
   if (ab->loaded_group < 0 || ab->loaded_entry < 0 ||
       ab->loaded_group >= ab->group_count ||
       ab->loaded_entry >= ab->groups[ab->loaded_group].entry_count)
      return FALSE;
   if (ab->groups[ab->loaded_group].is_backup)
      return FALSE;
   return map_state_path_matches(glb_ds1[ds1_idx].name,
             ab->groups[ab->loaded_group].entries[ab->loaded_entry].ds1_path);
}

/* Create a map, or clone the open one, through the dialog. "The first save or
 * new map asks the user" for a project. */
static void interface_new_map(int ds1_idx, int clone)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   int use_loaded = interface_loaded_row_is_open_map(ds1_idx);
   MAP_DIALOG_S d;
   char suggested[128], error[512] = "";
   int gi, ei, rc, i;

   if (clone && !use_loaded)
      return;
   if (!map_save_ui_require_project())
      return;

   /* A project just opened rebuilds the rows. */
   use_loaded = interface_loaded_row_is_open_map(ds1_idx);
   gi = use_loaded ? ab->loaded_group : ab->selected_group;
   ei = use_loaded ? ab->loaded_entry : ab->selected_entry;
   if (gi < 0 || gi >= ab->group_count || ab->groups[gi].is_backup ||
       ab->groups[gi].entry_count == 0)
   {
      gi = -1;
      for (i = 0; i < ab->group_count && gi < 0; i++)
         if (!ab->groups[i].is_backup && ab->groups[i].entry_count > 0)
            gi = i;
      ei = 0;
      if (gi < 0)
         return;
   }
   if (ei < 0 || ei >= ab->groups[gi].entry_count)
      ei = 0;

   memset(&d, 0, sizeof(d));
   d.is_clone = clone;
   d.group_idx = gi;
   d.def = ab->groups[gi].entries[ei].lvlprest_def;
   d.width = 20;
   d.height = 20;
   if (glb_ds1edit.has_loaded_ds1 && glb_ds1[ds1_idx].name[0] != '\0')
   {
      d.width = (int) glb_ds1[ds1_idx].width;
      d.height = (int) glb_ds1[ds1_idx].height;
   }
   ds1_manager_suggest_name(ab->groups[gi].entries[ei].ds1_path, &ab->groups[gi],
                            suggested, sizeof(suggested));
   snprintf(d.file_name, sizeof(d.file_name), "%s", suggested);

   if (!map_dialog_show(&d))
      return;

   if (clone)
      rc = ds1_manager_clone(ds1_idx, d.group_idx, d.def, d.file_name, error, sizeof(error));
   else
      rc = ds1_manager_create_empty(d.group_idx, d.def, d.file_name, d.width, d.height,
                                    error, sizeof(error));
   if (rc != 0)
   {
      al_show_native_message_box(a5_display, clone ? "Clone Map" : "New Map",
         "The map wasn't created. Nothing was changed.", error, NULL,
         ALLEGRO_MESSAGEBOX_WARN);
      input_suppress_held();
   }
}

/* A box that tells the user something, not a problem. */
static void interface_info_box(const char *title, const char *heading, const char *text)
{
   al_show_native_message_box(a5_display, title, heading, text, NULL, 0);
   input_suppress_held();
   input_discard_pending();
}

/* True if the open maps are saved or there was nothing to save. Asks first, since a
 * build or a test run reads the files on disk. A No, or a save that fails, is false. */
static int interface_save_before(const char *title, const char *heading)
{
   int rc;

   if (!map_any_dirty())
      return TRUE;
   rc = al_show_native_message_box(a5_display, title, heading,
      "Some maps have changes that aren't saved yet. They are used from the saved files.",
      NULL, ALLEGRO_MESSAGEBOX_YES_NO | ALLEGRO_MESSAGEBOX_QUESTION);
   input_suppress_held();
   input_discard_pending();
   if (rc != 1)
      return FALSE;
   return map_save_ui_save_dirty() == 0;
}

/* mpq_build_run asks with the path relative to Tiles, forward slashes, as the project
 * has it. misc_load_mpq_file turns the slashes into backslashes and the archives ignore
 * case, so the path goes through as it is. */
static int interface_map_in_game(const char *ds1_path, void *ctx)
{
   (void) ctx;
   return area_browser_game_has_map(ds1_path);
}

/* The folder the open archives came from, set once at startup. A project's d2_install
 * isn't used: with no archives open the table and in_game can't match the base. */
static const char *interface_game_folder(void)
{
   return project_open_game_folder();
}

/* Pack the project into <project>/build/patch_d2.mpq. 0 if it was written, else -1,
 * after telling the user why. quiet skips the report of what was written. */
static int interface_build_mpq(int quiet)
{
   MPQ_BUILD_ARGS_S a;
   MPQ_BUILD_RESULT_S res;
   char base[PROJECT_PATH_MAX + 32], out[PROJECT_PATH_MAX + 32];
   char problems[4096], report[PROJECT_PATH_MAX + 1024];
   char *table;
   double t0, secs;
   int rc;

   if (!glb_project.is_open)
      return -1;
   if (interface_game_folder()[0] == '\0')
   {
      interface_map_op_failed("Build MPQ", "Nothing was built.",
         "The editor found no game folder when it started. Set d2_install in Ds1edit.ini and restart.");
      return -1;
   }
   if (!interface_save_before("Build MPQ", "Save all maps and build?"))
      return -1;

   table = area_browser_game_table();
   if (table == NULL)
   {
      interface_map_op_failed("Build MPQ", "Nothing was built.",
         "The game's LvlPrest.txt can't be read. Set d2_install in Ds1edit.ini and restart.");
      return -1;
   }

   mpq_build_base_path(interface_game_folder(), base, (int) sizeof(base));
   snprintf(out, sizeof(out), "%s/build/patch_d2.mpq", glb_project.path);
   memset(&a, 0, sizeof(a));
   a.project_dir = glb_project.path;
   a.base_mpq = base;
   a.test_game_dir = glb_project.test_game_dir;
   a.out_mpq = out;
   a.game_table = table;
   a.in_game = interface_map_in_game;
   memset(&res, 0, sizeof(res));
   problems[0] = '\0';

   t0 = al_get_time();
   rc = mpq_build_run(&a, &res, problems, (int) sizeof(problems));
   secs = al_get_time() - t0;
   free(table);

   if (rc != 0)
   {
      interface_map_op_failed("Build MPQ", "Nothing was built.", problems);
      return -1;
   }
   if (!quiet)
   {
      snprintf(report, sizeof(report),
         "%s\n\n%d files, %ld bytes, %.1f seconds.\n\n"
         "%s"
         "Install it by replacing patch_d2.mpq in a copy of the game. "
         "Check it in the game before you share it.",
         out, res.files, res.bytes, secs,
         res.pd2_ignores_tables ?
            "Project Diablo 2 won't use this MPQ's tables. Its pd2data.mpq outranks "
            "patch_d2.mpq and has its own LvlPrest, so a new or removed map won't reach "
            "the game. Changes to maps PD2's archives don't carry still work.\n\n" : "");
      interface_info_box("Build MPQ", "The MPQ was built.", report);
   }
   return 0;
}

typedef struct PROFILE_FORM
{
   char dir[PROJECT_PATH_MAX];
   char exe[128];
   char args[256];
   char wrapper[PROJECT_PATH_MAX];
} PROFILE_FORM;

#define PROFILE_INSTALLS_MAX (2 + MAX_MPQ_FILE)

/* What a test folder can't overlap: both d2_install values, and the folder of each
 * archive, since archives can be listed by hand from elsewhere. An archive in a root
 * folder adds nothing, or every folder on that disk would be refused. The count. */
static int interface_profile_installs(const char **installs)
{
   static char folders[MAX_MPQ_FILE][PROJECT_PATH_MAX];
   int i, n = 0;

   installs[n++] = glb_config.d2_install;
   installs[n++] = glb_project.d2_install;
   for (i = 0; i < MAX_MPQ_FILE; i++)
   {
      const char *file = glb_config.mpq_file[i];
      const char *end, *back;
      size_t len;

      if (file == NULL)
         continue;
      end = strrchr(file, '/');
      back = strrchr(file, '\\');
      if (back != NULL && (end == NULL || back > end))
         end = back;
      if (end == NULL)
         continue;
      len = (size_t) (end - file);
      if (len == 0 || (len == 2 && file[1] == ':') || len >= sizeof(folders[i]))
         continue;
      memcpy(folders[i], file, len);
      folders[i][len] = '\0';
      installs[n++] = folders[i];
   }
   return n;
}

static int interface_profile_check(char *status, int cap, void *ctx)
{
   const PROFILE_FORM *f = (const PROFILE_FORM *) ctx;
   GAME_PROFILE_S p;
   const char *installs[PROFILE_INSTALLS_MAX];
   int count = interface_profile_installs(installs);

   p.game_dir = f->dir;
   p.exe = f->exe;
   p.args = f->args;
   p.wrapper = f->wrapper;
   return game_profile_check(&p, glb_project.path, installs, count, status, cap);
}

/* Edit the test copy of the game and how it starts. 1 if the profile was stored,
 * 0 for cancel. */
static int interface_launch_profile(void)
{
   PROFILE_FORM f;
   FIELD_S fields[4];

   if (!glb_project.is_open)
      return 0;
   snprintf(f.dir, sizeof(f.dir), "%s", glb_project.test_game_dir);
   snprintf(f.exe, sizeof(f.exe), "%s", glb_project.test_exe);
   snprintf(f.args, sizeof(f.args), "%s", glb_project.test_args);
   snprintf(f.wrapper, sizeof(f.wrapper), "%s", glb_project.test_wrapper);
   fields[0].label = "Test game folder";
   fields[0].value = f.dir;
   fields[0].cap = (int) sizeof(f.dir);
   fields[1].label = "Executable";
   fields[1].value = f.exe;
   fields[1].cap = (int) sizeof(f.exe);
   fields[2].label = "Arguments";
   fields[2].value = f.args;
   fields[2].cap = (int) sizeof(f.args);
   fields[3].label = "Wrapper";
   fields[3].value = f.wrapper;
   fields[3].cap = (int) sizeof(f.wrapper);

   if (!fields_dialog_show("Launch Profile", fields, 4, interface_profile_check, &f))
      return 0;

   snprintf(glb_project.test_game_dir, sizeof(glb_project.test_game_dir), "%s", f.dir);
   snprintf(glb_project.test_exe, sizeof(glb_project.test_exe), "%s", f.exe);
   snprintf(glb_project.test_args, sizeof(glb_project.test_args), "%s", f.args);
   snprintf(glb_project.test_wrapper, sizeof(glb_project.test_wrapper), "%s", f.wrapper);
   if (!project_save())
   {
      interface_map_op_failed("Launch Profile", "The profile wasn't saved.",
         "project.ini can't be written. Check that the project folder is writable.");
      return 0;
   }
   return 1;
}

typedef struct SETTINGS_FORM
{
   char name[PROJECT_NAME_MAX];
   char game[PROJECT_PATH_MAX];
} SETTINGS_FORM;

static int interface_settings_check(char *status, int cap, void *ctx)
{
   const SETTINGS_FORM *f = (const SETTINGS_FORM *) ctx;

   if (f->name[0] == '\0')
   {
      snprintf(status, (size_t) cap, "Give the project a name.");
      return -1;
   }
   if (f->game[0] == '\0' || !al_filename_exists(f->game))
   {
      snprintf(status, (size_t) cap, "The game folder doesn't exist. Enter the full path to your Diablo II folder.");
      return -1;
   }
   return 0;
}

static void interface_project_settings(void)
{
   SETTINGS_FORM f;
   FIELD_S fields[2];
   char old_name[PROJECT_NAME_MAX], old_game[PROJECT_PATH_MAX];

   if (!glb_project.is_open)
      return;
   snprintf(f.name, sizeof(f.name), "%s", glb_project.name);
   snprintf(f.game, sizeof(f.game), "%s", glb_project.d2_install);
   fields[0].label = "Name";
   fields[0].value = f.name;
   fields[0].cap = (int) sizeof(f.name);
   fields[1].label = "Game folder";
   fields[1].value = f.game;
   fields[1].cap = (int) sizeof(f.game);

   if (!fields_dialog_show("Project Settings", fields, 2, interface_settings_check, &f))
      return;

   snprintf(old_name, sizeof(old_name), "%s", glb_project.name);
   snprintf(old_game, sizeof(old_game), "%s", glb_project.d2_install);
   snprintf(glb_project.name, sizeof(glb_project.name), "%s", f.name);
   snprintf(glb_project.d2_install, sizeof(glb_project.d2_install), "%s", f.game);
   if (!project_save())
   {
      snprintf(glb_project.name, sizeof(glb_project.name), "%s", old_name);
      snprintf(glb_project.d2_install, sizeof(glb_project.d2_install), "%s", old_game);
      interface_map_op_failed("Project Settings", "The settings weren't saved.",
         "project.ini can't be written. Check that the project folder is writable.");
      return;
   }
   if (strcmp(old_game, f.game) != 0)
      interface_info_box("Project Settings", "The game folder changed.",
         "The editor reads the game when it starts, so this takes effect after you reopen "
         "the project and restart the editor, unless Ds1edit.ini sets d2_install, which wins.");
}

/* Save, install the project's loose files in the test copy and start the game. The
 * game's window is the answer, so success says nothing. No MPQ: the game reads the
 * loose files, and a project's first test has no compiled tables to pack. */
static void interface_build_and_test(void)
{
   GAME_PROFILE_S p;
   const char *installs[PROFILE_INSTALLS_MAX];
   char error[2048];
   int count, files = 0;

   if (!glb_project.is_open)
      return;
   if (!interface_save_before("Build & Test", "Save all maps and test?"))
      return;

   p.game_dir = glb_project.test_game_dir;
   p.exe = glb_project.test_exe;
   p.args = glb_project.test_args;
   p.wrapper = glb_project.test_wrapper;
   count = interface_profile_installs(installs);
   error[0] = '\0';
   if (game_profile_check(&p, glb_project.path, installs, count, error, (int) sizeof(error)) != 0)
   {
      /* The dialog says what's wrong. A cancel there stops the test. */
      if (!interface_launch_profile())
         return;
   }

   error[0] = '\0';
   if (game_install(glb_project.path, glb_project.test_game_dir, &files, error,
                    (int) sizeof(error)) != 0)
   {
      interface_map_op_failed("Build & Test", "The game wasn't started.", error);
      return;
   }
   error[0] = '\0';
   if (game_start(&p, error, (int) sizeof(error)) != 0)
      interface_map_op_failed("Build & Test", "The game wasn't started.", error);
}

static CMD_CTX interface_command_context(int ds1_idx, const PASTE_POS_S *paste)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   CMD_CTX c;

   memset(&c, 0, sizeof(c));
   c.tile_mode = glb_ds1edit.mode == MOD_T;
   c.object_mode = glb_ds1edit.mode == MOD_O;
   c.has_map = glb_ds1edit.has_loaded_ds1 && glb_ds1[ds1_idx].name[0] != 0;
   c.has_open_entry = c.has_map && interface_loaded_row_is_open_map(ds1_idx);
   if (c.has_open_entry)
      c.open_state = ab->groups[ab->loaded_group].entries[ab->loaded_entry].state;
   /* interface_loaded_row_is_open_map rejects backup rows, so test them here. */
   c.open_is_backup = c.has_map && ab->loaded_group >= 0 &&
                      ab->loaded_group < ab->group_count &&
                      ab->groups[ab->loaded_group].is_backup &&
                      ab->loaded_entry >= 0 &&
                      ab->loaded_entry < ab->groups[ab->loaded_group].entry_count;
   c.any_dirty = map_any_dirty();
   c.has_project = glb_project.is_open;
   c.paste_active = paste->start != FALSE;
   c.can_undo_object = c.has_map && glb_ds1[ds1_idx].can_undo_obj != FALSE;
   return c;
}

/* Run a command chosen from a menu or a hotkey. Commands the polled code owns
 * mark their key as hit, so it runs them later in this frame. */
static void interface_run_command(CMD_ID cmd, int ds1_idx, int cx, int cy,
                                  PASTE_POS_S *paste, int *done)
{
   AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
   const CMD_DEF *def = commands_get(cmd);

   if (def != NULL && def->inject_key != 0)
   {
      a5_key_hit[def->inject_key] = 1;
      return;
   }

   switch (cmd)
   {
   case CMD_NEW_PROJECT:   project_menu_new_project();   break;
   case CMD_OPEN_PROJECT:  project_menu_open_project();  break;
   case CMD_CLOSE_PROJECT: project_menu_close_project(); break;
   case CMD_OPEN_PRESET:   preset_picker_open();         break;
   case CMD_EXPORT_ASSETS: project_menu_export();        break;

   case CMD_SAVE:
      /* The "saved" message only after a save that actually happened. A window
         close while it's up stays pending for the quit check. */
      if (map_save_ui_save(ds1_idx, FALSE) == 0)
         msg_save_main();
      break;

   case CMD_SAVE_ALL:
      if (map_save_ui_save_dirty() == 0)
         msg_save_main();
      break;

   case CMD_QUIT:
      if (interface_confirm_quit())
         *done = TRUE;
      break;

   case CMD_UNDO:
      if (glb_ds1edit.mode == MOD_T)
         undo_apply_tile_buffer(ds1_idx);
      else if (glb_ds1edit.mode == MOD_O)
      {
         editobj_undo(ds1_idx);
         anim_update_gfx(FALSE);
      }
      break;

   case CMD_CUT:  interface_begin_paste(ds1_idx, cx, cy, paste, TRUE);  break;
   case CMD_COPY: interface_begin_paste(ds1_idx, cx, cy, paste, FALSE); break;

   case CMD_MAP_NEW:   interface_new_map(ds1_idx, FALSE); break;
   case CMD_MAP_CLONE: interface_new_map(ds1_idx, TRUE);  break;

   case CMD_MAP_DELETE:
      interface_delete_map(ab->loaded_group, ab->loaded_entry);
      break;
   case CMD_MAP_REVERT:
      interface_revert_map(ab->loaded_group, ab->loaded_entry);
      break;
   case CMD_MAP_REMOVE:
      interface_remove_map(ab->loaded_group, ab->loaded_entry);
      break;
   case CMD_MAP_RESTORE_SLOT:
      interface_restore_slot(ab->loaded_group, ab->loaded_entry);
      break;
   case CMD_MAP_RESTORE_BACKUP:
      interface_restore_backup(ab->loaded_group, ab->loaded_entry);
      break;

   case CMD_PROJECT_SETTINGS: interface_project_settings();   break;
   case CMD_BUILD_MPQ:        interface_build_mpq(FALSE);     break;
   case CMD_BUILD_AND_TEST:   interface_build_and_test();     break;
   case CMD_LAUNCH_PROFILE:   interface_launch_profile();     break;

   default:
      break;
   }
}

/* The command a right click on this row offers, by its state. */
static const char *interface_row_action(int state, int *cmd)
{
   switch (state)
   {
   case MAP_STATE_NEW:      *cmd = CMD_MAP_DELETE;       return "Delete map...";
   case MAP_STATE_MODIFIED: *cmd = CMD_MAP_REVERT;       return "Revert to game version...";
   case MAP_STATE_REMOVED:  *cmd = CMD_MAP_RESTORE_SLOT; return "Restore slot...";
   default:                 *cmd = CMD_MAP_REMOVE;       return "Remove from project...";
   }
}

// ==========================================================================
// MAIN loop
void interfac_user_handler(int start_ds1_idx)
{
   int ds1_idx, done, cx = 0, cy = 0, n, i, dx, dy, old_ds1_idx = 0;
   int old_mouse_x = a5_mouse_x, old_mouse_y = a5_mouse_y, old_mouse_b = 0;
   int cur_mouse_z = 0, old_mouse_z = 0;
   int old_cell_x = -1, old_cell_y = -1;
   int old_identical_x = -1, old_identical_y = -1;
   int ticks_elapsed;
   int can_swich_mode, key_func_code[7] = {KEY_F1, KEY_F2,
                                           KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F11};
   TMP_SEL_S tmp_sel;
   PASTE_POS_S paste_pos;
   char tmp[150];
   MODE_E old_mode = 0;
   IT_ENUM itype = IT_NULL;
   int group_changed, old_group, found;
   ALLEGRO_BITMAP *old_screen_buff = NULL;
   double frame_start_ms, section_start_ms;
   int selftest_rendered = 0;
   int switchbench_done = 0;
   int switchbench_exit_in = 0;

   // init
   tmp_sel.x1 = tmp_sel.x2 = tmp_sel.y1 = tmp_sel.y2 = 0;
   tmp_sel.old_x2 = tmp_sel.old_y2 = 0;
   tmp_sel.type = TMP_NULL;
   tmp_sel.start = FALSE;
   paste_pos.old_ds1_idx = 0;
   paste_pos.old_x = 0;
   paste_pos.old_y = 0;
   paste_pos.src_ds1_idx = 0;
   paste_pos.start_x = 0;
   paste_pos.start_y = 0;
   paste_pos.start = FALSE;
   paste_pos.is_cut = FALSE;
   paste_pos.cut_saved = FALSE;
   ds1_idx = start_ds1_idx;
   done = FALSE;

   // main loop
   while (!done)
   {
      frame_start_ms = perf_now_ms();

      // drain Allegro 5 event queue
      section_start_ms = perf_now_ms();
      {
         ALLEGRO_EVENT event;
         input_begin_frame();
         while (al_get_next_event(a5_event_queue, &event))
         {
            /* Key/button edges, wheel, resize, focus loss and close all go
               through the input layer; only the editor-specific events are
               worth handling by hand below. */
            input_note_event(&event);

            if (event.type == ALLEGRO_EVENT_TIMER)
            {
               if (event.timer.source == a5_tick_timer)
                  glb_ds1edit.ticks_elapsed++;
               else if (event.timer.source == a5_fps_timer)
               {
                  glb_ds1edit.old_fps = glb_ds1edit.fps;
                  glb_ds1edit.fps = 0;
               }
            }
            else if (event.type == ALLEGRO_EVENT_KEY_CHAR)
            {
               /* Ctrl+Enter applies pending properties; Ctrl+Z discards an
                * active property edit and otherwise undoes a map edit. */
               if (glb_ds1edit.props_panel_visible &&
                   (event.keyboard.modifiers & DS1_KEYMOD_PRIMARY))
               {
                  if (event.keyboard.keycode == ALLEGRO_KEY_ENTER ||
                      event.keyboard.keycode == ALLEGRO_KEY_PAD_ENTER)
                  {
                     if (glb_ds1edit.props_panel.pending_count > 0)
                        props_panel_apply();
                  }
                  else if (event.keyboard.keycode == ALLEGRO_KEY_Z &&
                           input_properties_panel_owns_undo(
                               glb_ds1edit.props_panel_visible,
                               glb_ds1edit.props_panel.editing,
                               glb_ds1edit.props_panel.pending_count))
                  {
                     glb_ds1edit.props_panel.pending_count = 0;
                     glb_ds1edit.props_panel.editing = FALSE;
                     /* The panel consumed this press; do not also undo the
                      * map when its pending state is cleared below. */
                     a5_key_hit[ALLEGRO_KEY_Z] = 0;
                  }
               }
               else if (glb_ds1edit.props_panel.editing ||
                        glb_ds1edit.props_panel.has_focus)
               {
                  props_panel_handle_keychar(
                      event.keyboard.unichar,
                      event.keyboard.keycode);
                  /* The panel consumed this press; it may have just given up
                   * focus, and the quit check below must not see it as well. */
                  if (event.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
                     a5_key_hit[ALLEGRO_KEY_ESCAPE] = 0;
               }
            }
         }
      }
      perf_accumulate(
          &glb_perf_stats.event_ms_total,
          &glb_perf_stats.event_ms_max,
          perf_now_ms() - section_start_ms);

      // poll input state
      section_start_ms = perf_now_ms();
      input_end_frame();

      /* Close is a request, not an event to be swallowed: honour it here so
         it behaves the same whether or not a panel has focus. */
      if (a5_display_closed)
      {
         /* It clears the request itself once it asks, so a close that arrives
            during the question stays pending. */
         done = interface_confirm_quit();
         if (done)
            a5_display_closed = 0;
      }

      perf_accumulate(
          &glb_perf_stats.input_ms_total,
          &glb_perf_stats.input_ms_max,
          perf_now_ms() - section_start_ms);

      cur_mouse_z = a5_mouse_z;

      /* Menus and table-driven hotkeys. This runs before the polled keys below,
         so a command that marks a key as hit reaches them this frame. */
      {
         CMD_CTX cmd_ctx = interface_command_context(ds1_idx, &paste_pos);
         int z_before = a5_mouse_z;
         CMD_ID cmd = menu_bar_poll(&cmd_ctx, !glb_ds1edit.props_panel.editing);

         if (cmd != CMD_NONE)
            interface_run_command(cmd, ds1_idx, cx, cy, &paste_pos, &done);

         /* A dialog ran its own loop too: take in what happened meanwhile, and
            keep keys still held from the dialog from acting on the map. */
         switch (cmd)
         {
         case CMD_NEW_PROJECT: case CMD_OPEN_PROJECT: case CMD_CLOSE_PROJECT:
         case CMD_OPEN_PRESET: case CMD_EXPORT_ASSETS: case CMD_SAVE:
         case CMD_SAVE_ALL: case CMD_QUIT:
         case CMD_MAP_NEW: case CMD_MAP_CLONE:
         case CMD_PROJECT_SETTINGS: case CMD_BUILD_MPQ:
         case CMD_BUILD_AND_TEST: case CMD_LAUNCH_PROFILE:
            input_pump();
            input_suppress_held();
            break;
         default:
            break;
         }

         /* A menu that ran its own loop pumped input. Wheel turns it took
            belong to the menu, so they must not scroll or zoom on return. */
         old_mouse_z += a5_mouse_z - z_before;
         cur_mouse_z += a5_mouse_z - z_before;
      }
      if (!glb_ds1edit.has_loaded_ds1)
         ds1_idx = 0;  /* closing a project can discard the open maps */

      /* Sidebar toggle: [ (left bracket) key */
      if (key_hit(KEY_LBRACKET))
      {
         glb_ds1edit.sidebar_visible = !glb_ds1edit.sidebar_visible;
      }

      /* Properties panel: ] (right bracket) opens+focuses.
       * If closed: open and focus. If open but unfocused: focus. If focused: close.
       * Ctrl+] cycles Properties/Tiles tabs instead. */
      if (key_hit(KEY_RBRACKET) && !glb_ds1edit.props_panel.editing)
      {
         int ctrl_held = key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL);
         if (ctrl_held)
         {
            /* Cycle tabs — require panel visible so user sees the change */
            if (!glb_ds1edit.props_panel_visible)
            {
               glb_ds1edit.props_panel_visible = TRUE;
               glb_ds1edit.props_panel.has_focus = TRUE;
            }
            glb_ds1edit.props_panel.active_tab =
                (glb_ds1edit.props_panel.active_tab + 1) % PP_TAB_MAX;
         }
         else if (!glb_ds1edit.props_panel_visible)
         {
            /* Closed -> open + focused */
            glb_ds1edit.props_panel_visible = TRUE;
            glb_ds1edit.props_panel.has_focus = TRUE;
         }
         else if (!glb_ds1edit.props_panel.has_focus)
         {
            /* Open but unfocused -> focused */
            glb_ds1edit.props_panel.has_focus = TRUE;
         }
         else
         {
            /* Focused -> close */
            glb_ds1edit.props_panel_visible = FALSE;
            glb_ds1edit.props_panel.has_focus = FALSE;
         }
      }

      /* Sidebar: handle click on collapsed tab to expand */
      if (!glb_ds1edit.sidebar_visible && a5_mouse_b &&
          a5_mouse_x < 16) /* collapsed tab width */
      {
         glb_ds1edit.sidebar_visible = TRUE;
      }

      /* Properties panel: handle click on collapsed tab to expand */
      {
         int pp_disp_w = al_get_display_width(a5_display);
         if (!glb_ds1edit.props_panel_visible && a5_mouse_b &&
             a5_mouse_x > pp_disp_w - 16)
         {
            glb_ds1edit.props_panel_visible = TRUE;
         }
      }

      /* Sidebar: handle mouse interactions */
      if (glb_ds1edit.sidebar_visible &&
          a5_mouse_x < glb_ds1edit.sidebar_width)
      {
         /* Mouse wheel scrolls the sidebar (consume delta so zoom doesn't fire) */
         if (cur_mouse_z != old_mouse_z)
         {
            area_browser_sidebar_scroll(cur_mouse_z - old_mouse_z);
            old_mouse_z = cur_mouse_z;
         }

         /* Mouse click in sidebar */
         if (mouse_hit(1))
         {
            int click_result = area_browser_sidebar_click(
                a5_mouse_x, a5_mouse_y,
                glb_ds1edit.sidebar_width,
                al_get_display_height(a5_display));

            if (click_result == -2)
            {
               /* Close button */
               glb_ds1edit.sidebar_visible = FALSE;
            }
            else if (click_result == -3)
            {
               /* Individual DS1 entry clicked — load just that DS1 */
               {
                  int new_idx = area_browser_switch_single(
                      glb_ds1edit.area_browser.selected_group,
                      glb_ds1edit.area_browser.selected_entry);
                  if (new_idx >= 0)
                  {
                     ds1_idx = new_idx;
                     tile_picker_on_ds1_change(new_idx);
                  }
               }
            }
            else if (click_result >= 0)
            {
               /* Group clicked: expand or collapse, and nothing else.
                *
                * This used to load entry 0 as well, so asking to *see* a
                * group's list paid for a cold tileset load -- measured at
                * 59-92% of the cost of opening maps, several hundred ms to a
                * couple of seconds -- for a map the user had not chosen. The
                * load now happens on the click that actually wants one. */
            }

         }

         /* Right-click in sidebar — context menu for map and backup entries */
         if (mouse_hit(2))
         {
            AREA_BROWSER_S *rc_ab = &glb_ds1edit.area_browser;
            int rc_gi = rc_ab->selected_group;
            int rc_ei = rc_ab->selected_entry;

            /* Show context menu if right-clicked on a map or backup entry */
            if (rc_gi >= 0 && rc_gi < rc_ab->group_count &&
                rc_ei >= 0 &&
                rc_ei < rc_ab->groups[rc_gi].entry_count)
            {
               /* Simple popup menu: 2 items for a backup, 1 for a map */
               int rc_is_backup = rc_ab->groups[rc_gi].is_backup;
               int menu_x = a5_mouse_x;
               int menu_y = a5_mouse_y;
               int row_cmd;
               const char *row_label = interface_row_action(
                  rc_ab->groups[rc_gi].entries[rc_ei].state, &row_cmd);
               int menu_w = 212;
               int menu_h = rc_is_backup ? 28 : 14;
               int menu_sel = -1;
               int menu_done = 0;
               int disp_w = al_get_display_width(a5_display);
               int disp_h = al_get_display_height(a5_display);
               ALLEGRO_COLOR mc_bg = al_map_rgba(40, 36, 30, 240);
               ALLEGRO_COLOR mc_hover = al_map_rgba(60, 80, 120, 220);
               ALLEGRO_COLOR mc_text = al_map_rgb(220, 220, 220);
               ALLEGRO_COLOR mc_border = al_map_rgb(80, 70, 50);

               /* Clamp to display bounds */
               if (menu_x + menu_w > disp_w)
                  menu_x = disp_w - menu_w;
               if (menu_y + menu_h > disp_h)
                  menu_y = disp_h - menu_h;

               /* The click that opened this menu is still held; without
                  this it selects whatever sits under the cursor at once. */
               input_suppress_held();

               while (!menu_done)
               {
                  int mx, my;

                  /* This popup runs its own loop, so it has to refresh input
                     itself -- polling alone would leave key_hit/mouse_hit
                     holding whatever the main loop last saw, and the menu
                     would act on a click that happened before it opened. */
                  input_pump();
                  if (a5_display_closed)
                     menu_done = 1;
                  mx = al_get_mouse_state_axis(&a5_ms_state, 0);
                  my = al_get_mouse_state_axis(&a5_ms_state, 1);

                  /* Determine which menu item is hovered */
                  menu_sel = -1;
                  if (mx >= menu_x && mx < menu_x + menu_w)
                  {
                     if (my >= menu_y && my < menu_y + 14)
                        menu_sel = 0; /* Restore, or the row's action */
                     else if (rc_is_backup && my >= menu_y + 14 && my < menu_y + 28)
                        menu_sel = 1; /* Delete permanently */
                  }

                  /* Draw the popup over current frame */
                  al_set_target_backbuffer(a5_display);
                  al_draw_filled_rectangle((float)menu_x, (float)menu_y,
                                           (float)(menu_x + menu_w), (float)(menu_y + menu_h), mc_bg);
                  al_draw_rectangle((float)menu_x + 0.5f, (float)menu_y + 0.5f,
                                    (float)(menu_x + menu_w) - 0.5f, (float)(menu_y + menu_h) - 0.5f,
                                    mc_border, 1.0f);

                  /* Item 0: Restore, or the row's action */
                  if (menu_sel == 0)
                     al_draw_filled_rectangle((float)menu_x, (float)menu_y,
                                              (float)(menu_x + menu_w), (float)(menu_y + 14), mc_hover);
                  al_draw_textf(a5_font, mc_text, (float)(menu_x + 6), (float)(menu_y + 3), 0,
                                "%s", rc_is_backup ? "Restore" : row_label);

                  /* Item 1: Delete permanently */
                  if (rc_is_backup)
                  {
                     if (menu_sel == 1)
                        al_draw_filled_rectangle((float)menu_x, (float)(menu_y + 14),
                                                 (float)(menu_x + menu_w), (float)(menu_y + 28), mc_hover);
                     al_draw_textf(a5_font, mc_text, (float)(menu_x + 6), (float)(menu_y + 17), 0,
                                   "Delete permanently");
                  }

                  al_flip_display();
                  al_rest(0.02);

                  /* Click selects item */
                  if (mouse_hit(1))
                  {
                     if (menu_sel >= 0)
                     {
                        if (!rc_is_backup)
                        {
                           switch (row_cmd)
                           {
                           case CMD_MAP_DELETE:       interface_delete_map(rc_gi, rc_ei);   break;
                           case CMD_MAP_REVERT:       interface_revert_map(rc_gi, rc_ei);   break;
                           case CMD_MAP_RESTORE_SLOT: interface_restore_slot(rc_gi, rc_ei); break;
                           default:                   interface_remove_map(rc_gi, rc_ei);   break;
                           }
                        }
                        else if (menu_sel == 0)
                           interface_restore_backup(rc_gi, rc_ei);
                        else
                        {
                           interface_delete_backup(rc_gi, rc_ei);
                        }
                        fflush(stdout);
                     }
                     menu_done = 1;
                  }
                  /* Right-click or Escape cancels */
                  if (mouse_hit(2) ||
                      al_key_down(&a5_kb_state, ALLEGRO_KEY_ESCAPE))
                  {
                     menu_done = 1;
                  }
               }
            }
         }

         /* Update hover selection */
         {
            AREA_BROWSER_S *ab = &glb_ds1edit.area_browser;
            int sy, si, s_last_act = -1, s_draw_row = 0;
            int s_top = ui_top_bar_height();
            int found = 0;

            ab->selected_entry = -1;
            sy = s_top + 22 + 4; /* panel_top + SB_HEADER_H + 4 */
            for (si = 0; si < ab->group_count && !found; si++)
            {
               if (ab->groups[si].act != s_last_act)
               {
                  if (s_draw_row >= ab->scroll_offset)
                     sy += 14;
                  s_draw_row++;
                  s_last_act = ab->groups[si].act;
               }
               /* Group row */
               if (s_draw_row >= ab->scroll_offset)
               {
                  if (a5_mouse_y >= sy && a5_mouse_y < sy + 14)
                  {
                     ab->selected_group = si;
                     found = 1;
                  }
                  sy += 14;
               }
               s_draw_row++;

               /* Expanded entry rows */
               if (ab->groups[si].is_expanded && !found)
               {
                  int sj;
                  for (sj = 0; sj < ab->groups[si].entry_count; sj++)
                  {
                     if (s_draw_row >= ab->scroll_offset)
                     {
                        if (a5_mouse_y >= sy && a5_mouse_y < sy + 14)
                        {
                           ab->selected_group = si;
                           ab->selected_entry = sj;
                           found = 1;
                           break;
                        }
                        sy += 14;
                     }
                     s_draw_row++;
                  }
               }
            }
         }
      }

      /* Properties panel: handle mouse interactions (right side) */
      {
         int pp_disp_w = al_get_display_width(a5_display);
         if (glb_ds1edit.props_panel_visible &&
             a5_mouse_x > pp_disp_w - glb_ds1edit.props_panel_width)
         {
            /* Mouse wheel scrolls the panel (or zooms tile picker with Ctrl) */
            if (cur_mouse_z != old_mouse_z)
            {
               int dz = cur_mouse_z - old_mouse_z;
               int ctrl = key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL);
               if (glb_ds1edit.props_panel.active_tab == PP_TAB_TILES)
                  tile_picker_scroll(dz, ctrl);
               else
                  props_panel_scroll(dz);
               old_mouse_z = cur_mouse_z;
            }

            /* Mouse click in panel */
            if (mouse_hit(1))
            {
               int pp_result = props_panel_click(
                   a5_mouse_x, a5_mouse_y,
                   glb_ds1edit.props_panel_width,
                   al_get_display_height(a5_display));

               if (pp_result == -2)
                  glb_ds1edit.props_panel_visible = FALSE;
               else
                  glb_ds1edit.props_panel.has_focus = TRUE;

            }

            /* Consume right-click in panel */
            if (mouse_hit(2))
            {
            }
         }
      }

      /* Click outside properties panel removes its focus */
      if (glb_ds1edit.props_panel.has_focus && (a5_mouse_b & 1))
      {
         int pp_dw = al_get_display_width(a5_display);
         if (!glb_ds1edit.props_panel_visible ||
             a5_mouse_x <= pp_dw - glb_ds1edit.props_panel_width)
         {
            glb_ds1edit.props_panel.has_focus = FALSE;
         }
      }

      can_swich_mode = TRUE;
      if (glb_ds1edit.mode == MOD_P)
      {
         if (glb_ds1[ds1_idx].path_edit_win.mode != PEM_NONE)
            can_swich_mode = FALSE;
         else if (glb_ds1[ds1_idx].path_edit_win.obj_dirty == TRUE)
            can_swich_mode = FALSE;
      }

      old_mouse_x = a5_mouse_x;
      old_mouse_y = a5_mouse_y;
      old_mouse_b = a5_mouse_b;

      cur_mouse_z = a5_mouse_z;

      // keep the current mouse coordinates for the entire loop process

      // which tile (or sub-tile) is RIGHT NOW under the mouse ?
      section_start_ms = perf_now_ms();
      if (glb_ds1edit.has_loaded_ds1 && glb_ds1[ds1_idx].tile_w > 0)
         mouse_to_tile(ds1_idx, &cx, &cy);
      if (glb_ds1edit.mode == MOD_T)
      {
         if (cx < 0)
            cx = 0;
         else if (cx >= glb_ds1[ds1_idx].width)
            cx = glb_ds1[ds1_idx].width - 1;
         if (cy < 0)
            cy = 0;
         else if (cy >= glb_ds1[ds1_idx].height)
            cy = glb_ds1[ds1_idx].height - 1;
      }
      else
      {
         cx -= 2;
         cy += 2;
         if (cx < 0)
            cx = 0;
         else if (cx >= glb_ds1[ds1_idx].width * 5 - 1)
            cx = glb_ds1[ds1_idx].width * 5 - 1;
         if (cy < 0)
            cy = 0;
         else if (cy >= glb_ds1[ds1_idx].height * 5 - 1)
            cy = glb_ds1[ds1_idx].height * 5 - 1;
      }
      perf_accumulate(
          &glb_perf_stats.mouse_to_tile_ms_total,
          &glb_perf_stats.mouse_to_tile_ms_max,
          perf_now_ms() - section_start_ms);

      if (glb_ds1edit.mode == MOD_O)
      {
         section_start_ms = perf_now_ms();
         editobj_handler(
             ds1_idx, cx,
             cy,
             old_mouse_x,
             old_mouse_y,
             old_mouse_b);
         perf_accumulate(
             &glb_perf_stats.editobj_ms_total,
             &glb_perf_stats.editobj_ms_max,
             perf_now_ms() - section_start_ms);
      }

      if ((cx != old_cell_x) || (cy != old_cell_y))
      {
         old_cell_x = cx;
         old_cell_y = cy;
         old_identical_x = -1;
         old_identical_y = -1;
      }

      // check if need to redraw the screen because of floor animation
      section_start_ms = perf_now_ms();
      ticks_elapsed = glb_ds1edit.ticks_elapsed;
      if (ticks_elapsed && (glb_ds1[ds1_idx].animations_layer_mask == 1))
      {
         // animated floor rate = 10 fps
         // therefore it's at 2/5 of 25 fps
         // but internal unit is in 5th
         glb_ds1[ds1_idx].cur_anim_floor_frame += ticks_elapsed * 2;
      }
      else
         glb_ds1edit.ticks_elapsed = 0;
      perf_accumulate(
          &glb_perf_stats.anim_ms_total,
          &glb_perf_stats.anim_ms_max,
          perf_now_ms() - section_start_ms);

      // redraw the whole screen
      //
      // wpreview_draw_tiles() presents the frame itself, so charge
      // misc_draw_screen's time to "present" and leave "render" holding only
      // the compositing. Lumped together they cannot tell a slow tile blit
      // apart from a stalled al_flip_display, which are different bugs.
      section_start_ms = perf_now_ms();
      st_cpu_t0 = selftest_thread_cpu_ms();
      glb_present_ms = 0.0;
      if (glb_ds1edit.has_loaded_ds1)
         wpreview_draw_tiles(ds1_idx);
      else
      {
         /* No map: wpreview_draw_tiles() is not clearing the frame buffer, so
          * the last map, dialog and status text would stay on screen. Clear it
          * the same way wpreview_draw_tiles() does, before the sidebar goes on. */
         a5_clear(glb_ds1edit.screen_buff);
         misc_draw_screen(a5_mouse_x, a5_mouse_y);
      }
      glb_ds1edit.fps++;
      if (glb_ds1edit.cmd_line.selftest_frames > 0)
      {
         if (st_n == 0)
         {
            st_t0 = al_get_time();
            st_win_w = al_get_display_width(a5_display);
            st_win_h = al_get_display_height(a5_display);
         }
         st_cpu_ms_total += selftest_thread_cpu_ms() - st_cpu_t0;
         st_record(perf_now_ms() - section_start_ms - glb_present_ms,
                   glb_present_ms);
      }
      perf_accumulate(
          &glb_perf_stats.render_ms_total,
          &glb_perf_stats.render_ms_max,
          perf_now_ms() - section_start_ms - glb_present_ms);
      perf_accumulate(
          &glb_perf_stats.present_ms_total,
          &glb_perf_stats.present_ms_max,
          glb_present_ms);

      // scroll UP / DOWN / LEFT / RIGHT
      section_start_ms = perf_now_ms();

      // if the Object Editing Window is display
      if (glb_ds1[ds1_idx].draw_edit_obj == TRUE)
      {
         if (glb_config.winobj_scroll_keyb == TRUE)
         {
            // can scroll by keyboard (WASD)
            if (key_pressed(KEY_W))
            {
               glb_ds1edit.win_preview.y0 -= glb_ds1[ds1_idx].cur_scroll.keyb.y;
            }
            if (key_pressed(KEY_S))
            {
               glb_ds1edit.win_preview.y0 += glb_ds1[ds1_idx].cur_scroll.keyb.y;
            }
            if (key_pressed(KEY_A))
            {
               glb_ds1edit.win_preview.x0 -= glb_ds1[ds1_idx].cur_scroll.keyb.x;
            }
            if (key_pressed(KEY_D))
            {
               glb_ds1edit.win_preview.x0 += glb_ds1[ds1_idx].cur_scroll.keyb.x;
            }
         }

         if (glb_config.winobj_scroll_mouse == TRUE)
         {
            // can scroll by mouse
            if (old_mouse_y == 0)
            {
               glb_ds1edit.win_preview.y0 -= glb_ds1[ds1_idx].cur_scroll.mouse.y;
            }
            if (old_mouse_y == glb_config.screen.height - 1)
            {
               glb_ds1edit.win_preview.y0 += glb_ds1[ds1_idx].cur_scroll.mouse.y;
            }
            if (old_mouse_x == 0)
            {
               glb_ds1edit.win_preview.x0 -= glb_ds1[ds1_idx].cur_scroll.mouse.x;
            }
            if (old_mouse_x == glb_config.screen.width - 1)
            {
               glb_ds1edit.win_preview.x0 += glb_ds1[ds1_idx].cur_scroll.mouse.x;
            }
         }
      }
      else
      {
         /* WASD and the arrow keys scroll. Ctrl+arrows belong to map
          * browsing below. Touching the window border scrolls only when
          * mouse_edge_scroll is enabled: an idle pointer resting on an edge
          * (common in tests and after alt-tab) otherwise drifts the view off
          * the map. */
         int ctrl = key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL);
         int edge = glb_config.mouse_edge_scroll && a5_mouse_available();

         if (key_pressed(KEY_W) || (!ctrl && key_pressed(KEY_UP)))
         {
            glb_ds1edit.win_preview.y0 -= glb_ds1[ds1_idx].cur_scroll.keyb.y;
         }
         else if (edge && old_mouse_y == 0)
         {
            glb_ds1edit.win_preview.y0 -= glb_ds1[ds1_idx].cur_scroll.mouse.y;
         }

         if (key_pressed(KEY_S) || (!ctrl && key_pressed(KEY_DOWN)))
         {
            glb_ds1edit.win_preview.y0 += glb_ds1[ds1_idx].cur_scroll.keyb.y;
         }
         else if (edge && old_mouse_y == glb_config.screen.height - 1)
         {
            glb_ds1edit.win_preview.y0 += glb_ds1[ds1_idx].cur_scroll.mouse.y;
         }

         if (key_pressed(KEY_A) || (!ctrl && key_pressed(KEY_LEFT)))
         {
            glb_ds1edit.win_preview.x0 -= glb_ds1[ds1_idx].cur_scroll.keyb.x;
         }
         else if (edge && old_mouse_x == 0)
         {
            glb_ds1edit.win_preview.x0 -= glb_ds1[ds1_idx].cur_scroll.mouse.x;
         }

         if (key_pressed(KEY_D) || (!ctrl && key_pressed(KEY_RIGHT)))
         {
            glb_ds1edit.win_preview.x0 += glb_ds1[ds1_idx].cur_scroll.keyb.x;
         }
         else if (edge && old_mouse_x == glb_config.screen.width - 1)
         {
            glb_ds1edit.win_preview.x0 += glb_ds1[ds1_idx].cur_scroll.mouse.x;
         }
      }

      // zoom — keyboard (+/- keys): zoom around screen center
      if ((key_hit(KEY_MINUS_PAD) || key_hit(KEY_MINUS)) &&
          glb_ds1[ds1_idx].cur_zoom < ZM_116)
      {
         glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0;
         glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0;
         glb_ds1[ds1_idx].own_wpreview.w = glb_ds1edit.win_preview.w;
         glb_ds1[ds1_idx].own_wpreview.h = glb_ds1edit.win_preview.h;
         change_zoom(ds1_idx, glb_ds1[ds1_idx].cur_zoom + 1);
         glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
         glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
      }
      if ((key_hit(KEY_PLUS_PAD) || key_hit(KEY_EQUALS)) &&
          glb_ds1[ds1_idx].cur_zoom > ZM_21)
      {
         glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0;
         glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0;
         glb_ds1[ds1_idx].own_wpreview.w = glb_ds1edit.win_preview.w;
         glb_ds1[ds1_idx].own_wpreview.h = glb_ds1edit.win_preview.h;
         change_zoom(ds1_idx, glb_ds1[ds1_idx].cur_zoom - 1);
         glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
         glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
      }

      // zoom — mouse wheel: zoom toward/away from cursor (Google Maps style)
      if (cur_mouse_z != old_mouse_z && glb_ds1edit.has_loaded_ds1 &&
          !(a5_mouse_b & 4)) /* don't zoom if middle button held (center-to-cursor) */
      {
         int zoom_dir = (cur_mouse_z < old_mouse_z) ? 1 : -1; /* 1=out, -1=in */
         int new_zoom = glb_ds1[ds1_idx].cur_zoom + zoom_dir;

         if (new_zoom >= ZM_21 && new_zoom <= ZM_116)
         {
            /* World position under the cursor before zoom */
            int world_x = glb_ds1edit.win_preview.x0 + a5_mouse_x;
            int world_y = glb_ds1edit.win_preview.y0 + a5_mouse_y;

            /* Ratio of cursor position relative to viewport */
            float rx = (float)a5_mouse_x / (float)glb_ds1edit.win_preview.w;
            float ry = (float)a5_mouse_y / (float)glb_ds1edit.win_preview.h;

            /* Save and zoom */
            glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0;
            glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0;
            glb_ds1[ds1_idx].own_wpreview.w = glb_ds1edit.win_preview.w;
            glb_ds1[ds1_idx].own_wpreview.h = glb_ds1edit.win_preview.h;
            change_zoom(ds1_idx, new_zoom);

            /* After zoom, the world coordinates are rescaled.
             * Adjust viewport so the cursor world position stays fixed.
             * new_world = new_viewport_origin + cursor_screen_pos
             * We want: new_viewport_x0 + mouse_x = world_x * new_scale / old_scale */
            {
               int old_mul = (zoom_dir == 1) ? (new_zoom == ZM_11 ? 1 : (new_zoom == ZM_21 ? 2 : 1)) : (glb_ds1[ds1_idx].cur_zoom == ZM_21 ? 2 : 1);

               /* Simpler: just set viewport so cursor world position is preserved */
               float scale_ratio;
               int old_tw = (zoom_dir == 1) ? glb_ds1[ds1_idx].tile_w * 2 : /* was bigger before zoom out */
                                glb_ds1[ds1_idx].tile_w / 2;                /* was smaller before zoom in */

               if (old_tw == 0)
                  old_tw = 1;
               scale_ratio = (float)glb_ds1[ds1_idx].tile_w / (float)old_tw;

               glb_ds1edit.win_preview.x0 = (int)(world_x * scale_ratio) - a5_mouse_x;
               glb_ds1edit.win_preview.y0 = (int)(world_y * scale_ratio) - a5_mouse_y;
            }

            glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0;
            glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0;
         }
      }

      if (old_mouse_z != cur_mouse_z)
      {
         old_mouse_z = cur_mouse_z;
         if (a5_mouse_b & 4)
         {
            if (glb_ds1edit.mode == MOD_T)
            {
               // Center to mouse in TILE mode
               cx++;
               dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
               dy = (cy * glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);
               cx--;
               glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0 =
                   dx - glb_ds1edit.win_preview.w / 2;
               glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0 =
                   dy - glb_ds1edit.win_preview.h / 2;
               glb_ds1[ds1_idx].own_wpreview.w = glb_ds1edit.win_preview.w;
               glb_ds1[ds1_idx].own_wpreview.h = glb_ds1edit.win_preview.h;
               glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
               glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
               a5_set_mouse_xy(a5_display, glb_ds1edit.win_preview.w / 2, glb_ds1edit.win_preview.h / 2);
            }
            else
            {
               // Center to mouse in OBJECT / PATH mode
               cx /= 5;
               cy /= 5;
               cx++;
               dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
               dy = (cy * glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);
               cx--;
               cx *= 5;
               cy *= 5;
               glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0 =
                   dx - glb_ds1edit.win_preview.w / 2;
               glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0 =
                   dy - glb_ds1edit.win_preview.h / 2;
               glb_ds1[ds1_idx].own_wpreview.w = glb_ds1edit.win_preview.w;
               glb_ds1[ds1_idx].own_wpreview.h = glb_ds1edit.win_preview.h;
               glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
               glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
               a5_set_mouse_xy(a5_display, glb_ds1edit.win_preview.w / 2, glb_ds1edit.win_preview.h / 2);
            }
         }
      }

      // DS1 file navigation (Ctrl+arrows, PgUp/PgDn, Home/End). Plain
      // arrows scroll the map above.
      {
      int nav_ctrl = key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL);
      if (nav_ctrl && key_hit(KEY_UP))
      {
         int nav_idx = area_browser_nav_up();
         if (nav_idx >= 0 && glb_ds1edit.has_loaded_ds1)
            ds1_idx = nav_idx;
      }
      if (nav_ctrl && key_hit(KEY_DOWN))
      {
         int nav_idx = area_browser_nav_down();
         if (nav_idx >= 0 && glb_ds1edit.has_loaded_ds1)
            ds1_idx = nav_idx;
      }
      if (nav_ctrl && key_hit(KEY_LEFT))
      {
         area_browser_nav_left();
      }
      if (nav_ctrl && key_hit(KEY_RIGHT))
      {
         int nav_idx = area_browser_nav_right();
         if (nav_idx >= 0 && glb_ds1edit.has_loaded_ds1)
            ds1_idx = nav_idx;
      }
      }
      if (key_hit(KEY_PGUP) && !(key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL)))
      {
         area_browser_nav_pgup();
      }
      if (key_hit(KEY_PGDN) && !(key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL)))
      {
         area_browser_nav_pgdn();
      }
      if (key_hit(KEY_HOME) && !(key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT)))
      {
         int nav_idx = area_browser_nav_home();
         if (nav_idx >= 0 && glb_ds1edit.has_loaded_ds1)
            ds1_idx = nav_idx;
      }
      if (key_hit(KEY_END))
      {
         int nav_idx = area_browser_nav_end();
         if (nav_idx >= 0 && glb_ds1edit.has_loaded_ds1)
            ds1_idx = nav_idx;
      }

      // layers toggle
      if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
      {
         // if shift pressed, just 1 layer will be active
         for (n = 0; n < 7; n++)
         {
            if (key_pressed(key_func_code[n]))
            {
               for (i = 0; i < FLOOR_MAX_LAYER; i++)
                  glb_ds1[ds1_idx].floor_layer_mask[i] = 0;

               if (key_pressed(KEY_F11))
               {
                  for (i = 0; i < SHADOW_MAX_LAYER; i++)
                     glb_ds1[ds1_idx].shadow_layer_mask[i] = 3;
               }
               else
               {
                  for (i = 0; i < SHADOW_MAX_LAYER; i++)
                     glb_ds1[ds1_idx].shadow_layer_mask[i] = 0;
               }

               for (i = 0; i < WALL_MAX_LAYER; i++)
                  glb_ds1[ds1_idx].wall_layer_mask[i] = 0;

               break;
            }
         }
      }
      if (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
      {
         // if control pressed, just 1 layer will be inactive
         for (n = 0; n < 7; n++)
         {
            if (key_pressed(key_func_code[n]))
            {
               for (i = 0; i < FLOOR_MAX_LAYER; i++)
                  glb_ds1[ds1_idx].floor_layer_mask[i] = 1;

               for (i = 0; i < SHADOW_MAX_LAYER; i++)
                  glb_ds1[ds1_idx].shadow_layer_mask[i] = 3;

               for (i = 0; i < WALL_MAX_LAYER; i++)
                  glb_ds1[ds1_idx].wall_layer_mask[i] = 1;

               break;
            }
         }
      }
      if (key_hit(KEY_F1))
      {
         glb_ds1[ds1_idx].floor_layer_mask[0] = 1 - glb_ds1[ds1_idx].floor_layer_mask[0];
      }
      if (key_hit(KEY_F2))
      {
         glb_ds1[ds1_idx].floor_layer_mask[1] = 1 - glb_ds1[ds1_idx].floor_layer_mask[1];
      }
      if (key_hit(KEY_F5))
      {
         glb_ds1[ds1_idx].wall_layer_mask[0] = 1 - glb_ds1[ds1_idx].wall_layer_mask[0];
      }
      if (key_hit(KEY_F6))
      {
         glb_ds1[ds1_idx].wall_layer_mask[1] = 1 - glb_ds1[ds1_idx].wall_layer_mask[1];
      }
      if (key_hit(KEY_F7))
      {
         glb_ds1[ds1_idx].wall_layer_mask[2] = 1 - glb_ds1[ds1_idx].wall_layer_mask[2];
      }
      if (key_hit(KEY_F8))
      {
         glb_ds1[ds1_idx].wall_layer_mask[3] = 1 - glb_ds1[ds1_idx].wall_layer_mask[3];
      }

      // special tiles layer
      if (key_hit(KEY_F9))
      {
         glb_ds1[ds1_idx].special_layer_mask = 1 - glb_ds1[ds1_idx].special_layer_mask;
      }

      // animation layer
      if (key_hit(KEY_F3))
      {
         glb_ds1[ds1_idx].animations_layer_mask++;
         if (glb_ds1[ds1_idx].animations_layer_mask == 3)
            glb_ds1[ds1_idx].animations_layer_mask = 0;
      }

      // objects layer
      if (key_hit(KEY_F4) && (glb_ds1edit.mode != MOD_O))
      {
         glb_ds1[ds1_idx].objects_layer_mask++;
         if (glb_ds1[ds1_idx].objects_layer_mask >= OL_MAX)
            glb_ds1[ds1_idx].objects_layer_mask = OL_NONE;
      }

      // paths layer
      if (key_hit(KEY_F10) && (glb_ds1edit.mode != MOD_P))
      {
         glb_ds1[ds1_idx].paths_layer_mask = 1 - glb_ds1[ds1_idx].paths_layer_mask;
      }

      // shadow mode
      if (key_hit(KEY_F11))
      {
         if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
         {
            glb_ds1[ds1_idx].shadow_layer_mask[0]--;
            if (glb_ds1[ds1_idx].shadow_layer_mask[0] < 0)
               glb_ds1[ds1_idx].shadow_layer_mask[0] = 3;
         }
         else
         {
            glb_ds1[ds1_idx].shadow_layer_mask[0]++;
            if (glb_ds1[ds1_idx].shadow_layer_mask[0] >= 4)
               glb_ds1[ds1_idx].shadow_layer_mask[0] = 0;
         }
      }

      // walkable infos
      if (key_hit(KEY_SPACE))
      {
         glb_ds1[ds1_idx].walkable_layer_mask++;
         if (glb_ds1[ds1_idx].walkable_layer_mask >= 3)
            glb_ds1[ds1_idx].walkable_layer_mask = 0;
      }
      if (key_hit(KEY_T))
      {
         glb_ds1[ds1_idx].subtile_help_display =
             1 - glb_ds1[ds1_idx].subtile_help_display;
      }

      // gamma correction
      if (key_pressed(KEY_F12))
      {
         if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
         {
            if (glb_ds1edit.cur_gamma > GC_060)
            {
               rest(80);
               glb_ds1edit.cur_gamma--;
               misc_update_pal_with_gamma();
               a5_current_palette = &glb_ds1edit.vga_pal[palette_resolve_index(glb_ds1[ds1_idx].txt_act,
                                                                               glb_ds1[ds1_idx].act)];
            }
         }
         else
         {
            if (glb_ds1edit.cur_gamma < GC_300)
            {
               rest(80);
               glb_ds1edit.cur_gamma++;
               misc_update_pal_with_gamma();
               a5_current_palette = &glb_ds1edit.vga_pal[palette_resolve_index(glb_ds1[ds1_idx].txt_act,
                                                                               glb_ds1[ds1_idx].act)];
            }
         }
      }

      // Home (center the map)
      if (key_hit(KEY_HOME) && (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT)))
      {
         // Shift+Home: center map view
         cx = glb_ds1[ds1_idx].width / 2 + 1;
         cy = glb_ds1[ds1_idx].height / 2;
         dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
         dy = (cy * glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);
         glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0 = dx - glb_ds1edit.win_preview.w / 2;
         glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0 = dy - glb_ds1edit.win_preview.h / 2;
      }

      // Backspace (show all layers)
      if (key_hit(KEY_BACKSPACE))
      {
         for (i = 0; i < FLOOR_MAX_LAYER; i++)
            glb_ds1[ds1_idx].floor_layer_mask[i] = 1;
         for (i = 0; i < WALL_MAX_LAYER; i++)
            glb_ds1[ds1_idx].wall_layer_mask[i] = 1;
         for (i = 0; i < SHADOW_MAX_LAYER; i++)
            glb_ds1[ds1_idx].shadow_layer_mask[i] = 3;
      }

      // P ('P'rintscreen = screenshot)
      if (key_hit(KEY_P))
      {
         if ((key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT)))
         {
            // BIG screenshot (complete map)
            old_screen_buff = glb_ds1edit.screen_buff;
            if (wpreview_draw_tiles_big_screenshot(ds1_idx) == 0)
            {
               // big screenshot is ready
               export_make_screenshot_name(tmp, sizeof(tmp), glb_ds1edit.screenshot_num);
               while (a5_file_exists(tmp))
               {
                  glb_ds1edit.screenshot_num++;
                  export_make_screenshot_name(tmp, sizeof(tmp), glb_ds1edit.screenshot_num);
               }

               // handle palette
               if (glb_ds1edit.cmd_line.force_pal_num == -1)
               {
                  // use .ds1 act value for palette
                  al_save_bitmap(tmp, glb_ds1edit.screen_buff);
               }
               else
               {
                  // use force_pal value for palette
                  al_save_bitmap(tmp, glb_ds1edit.screen_buff);
               }

               // free temp bitmap
               al_destroy_bitmap(glb_ds1edit.screen_buff);
            }
            glb_ds1edit.screen_buff = old_screen_buff;
         }
         else
         {
            // normal screenshot (visible screen only)
            export_make_screenshot_name(tmp, sizeof(tmp), glb_ds1edit.screenshot_num);
            while (a5_file_exists(tmp))
            {
               glb_ds1edit.screenshot_num++;
               export_make_screenshot_name(tmp, sizeof(tmp), glb_ds1edit.screenshot_num);
            }

            // draw the mouse cursor onto the buffer
            a5_draw_sprite(
                glb_ds1edit.screen_buff,
                glb_ds1edit.mouse_cursor[glb_ds1edit.mode],
                old_mouse_x - 1,
                old_mouse_y - 1);

            // handle palette
            if (glb_ds1edit.cmd_line.force_pal_num == -1)
            {
               // use .ds1 act value for palette
               al_save_bitmap(tmp, glb_ds1edit.screen_buff);
            }
            else
            {
               // use force_pal value for palette
               al_save_bitmap(tmp, glb_ds1edit.screen_buff);
            }
         }

         // the buffer was saved
         glb_ds1edit.screenshot_num++;
      }

      // S
      if (key_hit(KEY_S) && (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT)) &&
          !key_pressed(KEY_LCONTROL) && !key_pressed(KEY_RCONTROL) && glb_ds1edit.mode == MOD_T)
      {
         // Shift+S : show all previously hidden tiles
         edittile_unhide_all(ds1_idx);
      }

      // 'C' : either Copy or Center
      if (key_pressed(KEY_C))
      {
         if (glb_ds1edit.mode == MOD_T)
         {
            // TILE mode
            if (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
            {
               // CTRL + C : copy selected layers (copy / paste)
               interface_begin_paste(ds1_idx, cx, cy, &paste_pos, FALSE);
            }
            else
            {
               // Center to mouse in TILE mode
               cx++;
               dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
               dy = (cy * glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);
               cx--;
               glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0 =
                   dx - glb_ds1edit.win_preview.w / 2;
               glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0 =
                   dy - glb_ds1edit.win_preview.h / 2;
               glb_ds1[ds1_idx].own_wpreview.w = glb_ds1edit.win_preview.w;
               glb_ds1[ds1_idx].own_wpreview.h = glb_ds1edit.win_preview.h;
               if (glb_config.center_zoom != -1)
                  change_zoom(ds1_idx, glb_config.center_zoom);
               glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
               glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
               a5_set_mouse_xy(a5_display, glb_ds1edit.win_preview.w / 2, glb_ds1edit.win_preview.h / 2);
            }
         }
         else
         {
            /* Ctrl is *held* while C is tapped, so this has to ask whether
               it is down, not whether it just went down -- the two are never
               the same frame. */
            if (!key_pressed(KEY_LCONTROL) && !key_pressed(KEY_RCONTROL))
            {
               // Center to mouse in OBJECT / PATH mode
               cx /= 5;
               cy /= 5;
               cx++;
               dx = (cy * -glb_ds1[ds1_idx].tile_w / 2) + (cx * glb_ds1[ds1_idx].tile_w / 2);
               dy = (cy * glb_ds1[ds1_idx].tile_h / 2) + (cx * glb_ds1[ds1_idx].tile_h / 2);
               cx--;
               cx *= 5;
               cy *= 5;
               glb_ds1[ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0 =
                   dx - glb_ds1edit.win_preview.w / 2;
               glb_ds1[ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0 =
                   dy - glb_ds1edit.win_preview.h / 2;
               glb_ds1[ds1_idx].own_wpreview.w = glb_ds1edit.win_preview.w;
               glb_ds1[ds1_idx].own_wpreview.h = glb_ds1edit.win_preview.h;
               if (glb_config.center_zoom != -1)
                  change_zoom(ds1_idx, glb_config.center_zoom);
               glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
               glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
               a5_set_mouse_xy(a5_display, glb_ds1edit.win_preview.w / 2, glb_ds1edit.win_preview.h / 2);
            }
         }
      }

      // CTRL + X : Copy selected tiles, WITH CUT (crop / paste)
      if (key_pressed(KEY_X) && (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL)))
      {
         if (glb_ds1edit.mode == MOD_T)
         {
            // TILE mode
            interface_begin_paste(ds1_idx, cx, cy, &paste_pos, TRUE);
         }
      }

      // DEL key (regular or keypad) : delete all selected layers of all tiles
      if (input_delete_hits_tiles(glb_ds1edit.mode == MOD_T, paste_pos.start != FALSE))
      {
         edittile_delete_selected_tiles(ds1_idx);
      }

      // Ctrl+Z / Ctrl+U: undo tiles modification
      if (input_undo_shortcut_hit(input_properties_panel_owns_undo(
              glb_ds1edit.props_panel_visible,
              glb_ds1edit.props_panel.editing,
              glb_ds1edit.props_panel.pending_count)))
      {
         if (glb_ds1edit.mode == MOD_T)
         {
            if (paste_pos.start == FALSE)
            {
               undo_apply_tile_buffer(ds1_idx);
            }
         }
      }

      // G : toggle tile grid
      if (key_hit(KEY_G))
      {
         if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
            glb_ds1edit.display_tile_grid--;
         else
            glb_ds1edit.display_tile_grid++;
         if (glb_ds1edit.display_tile_grid < TG_OFF)
            glb_ds1edit.display_tile_grid = TG_MAX - 1;
         if (glb_ds1edit.display_tile_grid >= TG_MAX)
            glb_ds1edit.display_tile_grid = TG_OFF;
      }

      // changing current ds1
      group_changed = FALSE;
      old_group = glb_ds1edit.ds1_group_idx;
      if (can_swich_mode)
      {
         if (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
         {
            if (key_hit(KEY_1))
            {
               glb_ds1edit.ds1_group_idx = 0;
               group_changed = TRUE;
            }
            else if (key_hit(KEY_2))
            {
               glb_ds1edit.ds1_group_idx = 1;
               group_changed = TRUE;
            }
            else if (key_hit(KEY_3))
            {
               glb_ds1edit.ds1_group_idx = 2;
               group_changed = TRUE;
            }
            else if (key_hit(KEY_4))
            {
               glb_ds1edit.ds1_group_idx = 3;
               group_changed = TRUE;
            }
            else if (key_hit(KEY_5))
            {
               glb_ds1edit.ds1_group_idx = 4;
               group_changed = TRUE;
            }
            else if (key_hit(KEY_6))
            {
               glb_ds1edit.ds1_group_idx = 5;
               group_changed = TRUE;
            }
            else if (key_hit(KEY_7))
            {
               glb_ds1edit.ds1_group_idx = 6;
               group_changed = TRUE;
            }
            else if (key_hit(KEY_8))
            {
               glb_ds1edit.ds1_group_idx = 7;
               group_changed = TRUE;
            }
            else if (key_hit(KEY_9))
            {
               glb_ds1edit.ds1_group_idx = 8;
               group_changed = TRUE;
            }
            else if (key_hit(KEY_0))
            {
               glb_ds1edit.ds1_group_idx = 9;
               group_changed = TRUE;
            }
         }

         if (group_changed == TRUE)
         {
            // try to swap to 1st ds1 of this group set
            found = FALSE;
            for (i = 0; i < 10; i++)
            {
               if (strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + i].name))
               {
                  // there's a ds1 open here
                  old_ds1_idx = ds1_idx;
                  ds1_idx = glb_ds1edit.ds1_group_idx * 10 + i;
                  found = TRUE;
                  break;
               }
            }
            if (found == FALSE)
            {
               // don't change to this group
               glb_ds1edit.ds1_group_idx = old_group;
               group_changed = FALSE;
            }
         }

         // swap to a different ds1 ?
         if (key_hit(KEY_1) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10;
         }
         if (key_hit(KEY_2) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 1].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 1;
         }
         if (key_hit(KEY_3) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 2].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 2;
         }
         if (key_hit(KEY_4) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 3].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 3;
         }
         if (key_hit(KEY_5) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 4].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 4;
         }
         if (key_hit(KEY_6) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 5].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 5;
         }
         if (key_hit(KEY_7) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 6].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 6;
         }
         if (key_hit(KEY_8) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 7].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 7;
         }
         if (key_hit(KEY_9) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 8].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 8;
         }
         if (key_hit(KEY_0) && strlen(glb_ds1[glb_ds1edit.ds1_group_idx * 10 + 9].name))
         {
            old_ds1_idx = ds1_idx;
            ds1_idx = glb_ds1edit.ds1_group_idx * 10 + 9;
         }
      }

      if (old_ds1_idx != ds1_idx)
      {
         // save current win preview state for the old ds1
         glb_ds1[old_ds1_idx].own_wpreview.x0 = glb_ds1edit.win_preview.x0;
         glb_ds1[old_ds1_idx].own_wpreview.y0 = glb_ds1edit.win_preview.y0;
         glb_ds1[old_ds1_idx].own_wpreview.w = glb_ds1edit.win_preview.w;
         glb_ds1[old_ds1_idx].own_wpreview.h = glb_ds1edit.win_preview.h;

         // put back old win preview state for the new ds1
         glb_ds1edit.win_preview.x0 = glb_ds1[ds1_idx].own_wpreview.x0;
         glb_ds1edit.win_preview.y0 = glb_ds1[ds1_idx].own_wpreview.y0;
         glb_ds1edit.win_preview.w = glb_ds1[ds1_idx].own_wpreview.w;
         glb_ds1edit.win_preview.h = glb_ds1[ds1_idx].own_wpreview.h;

         // ending swap
         old_ds1_idx = ds1_idx;
      }

      // toggle 2nd row
      if (key_hit(KEY_TILDE))
      {
         if (glb_ds1edit.show_2nd_row == FALSE)
            glb_ds1edit.show_2nd_row = TRUE;
         else
            glb_ds1edit.show_2nd_row = FALSE;
      }

      // TAB : change edit mode
      if (key_hit(KEY_TAB))
      {
         if (glb_ds1edit.mode == MOD_L)
            glb_ds1edit.mode = old_mode;
         else
         {
            if (can_swich_mode)
            {
               if ((key_pressed(KEY_LSHIFT)) || (key_pressed(KEY_RSHIFT)))
                  glb_ds1edit.mode--;
               else
                  glb_ds1edit.mode++;
            }
         }
         if (glb_ds1edit.mode < MOD_T)
            glb_ds1edit.mode = MOD_P;
         if ((glb_ds1edit.mode >= MOD_MAX) || (glb_ds1edit.mode == MOD_L))
            glb_ds1edit.mode = MOD_T;
         /* Auto-switch props panel tab on mode change */
         if (glb_ds1edit.mode == MOD_T && glb_ds1edit.props_panel_visible)
            glb_ds1edit.props_panel.active_tab = PP_TAB_TILES;
         // show_mouse(NULL);
         //         misc_set_mouse_cursor(glb_ds1edit.mouse_cursor[glb_ds1edit.mode]);
         // show_mouse(screen);
         old_cell_x = -1;
         old_cell_y = -1;
      }

      // N : Toggle Night mode
      if (key_hit(KEY_N))
      {
         if (glb_ds1edit.mode == MOD_L)
         {
            glb_ds1edit.night_mode++;
            if (glb_ds1edit.night_mode >= 2)
            {
               glb_ds1edit.night_mode = 0;
               glb_ds1edit.mode = old_mode;
            }
         }
         else
         {
            old_mode = glb_ds1edit.mode;
            glb_ds1edit.mode = MOD_L;
         }

         old_cell_x = -1;
         old_cell_y = -1;
      }

      // R : Refresh obj.txt
      if (key_hit(KEY_R))
      {

         // refresh animdata.d2
         animdata_load();

         // destroy all animations
         anim_exit();

         // destroy memory obj.txt and objects.txt
         glb_ds1edit.obj_buff = txt_destroy(glb_ds1edit.obj_buff);
         glb_ds1edit.objects_buff = txt_destroy(glb_ds1edit.obj_buff);

         // read the current obj.txt and objects.txt
         read_objects_txt(); // objects.txt first !
         read_obj_txt();

         // load new animations
         anim_update_gfx(FALSE);

         // reset the ticks counter
         glb_ds1edit.ticks_elapsed = 0;
      }

      /* Tile picker brush: paint active brush on left-click in MOD_T.
       * Paints on click edge and on drag into new tiles. */
      {
         static int tp_last_paint_x = -1, tp_last_paint_y = -1;
         if ((a5_mouse_b & 1) && glb_ds1edit.mode == MOD_T &&
             glb_ds1edit.props_panel.tiles.brush.valid &&
             glb_ds1edit.has_loaded_ds1)
         {
            int pp_disp_w = al_get_display_width(a5_display);
            int panel_left = glb_ds1edit.props_panel_visible
                                 ? pp_disp_w - glb_ds1edit.props_panel_width
                                 : pp_disp_w;
            if (a5_mouse_x < panel_left)
            {
               if (cx != tp_last_paint_x || cy != tp_last_paint_y)
               {
                  tile_picker_place_brush(ds1_idx, cx, cy);
                  tp_last_paint_x = cx;
                  tp_last_paint_y = cy;
               }
            }
         }
         else if (!(a5_mouse_b & 1))
         {
            tp_last_paint_x = -1;
            tp_last_paint_y = -1;
         }
      }

      // left mouse button
      if (old_mouse_b & 1 &&
          !(glb_ds1edit.mode == MOD_T &&
            glb_ds1edit.props_panel.tiles.brush.valid))
      {
         // mouse button 1 is pressed
         if (glb_ds1edit.mode == MOD_T)
         {
            if (paste_pos.start == TRUE)
            {
               // end a paste
               edittile_paste_final(ds1_idx);
               paste_pos.start = FALSE;
               while (a5_mouse_b & 1) // NOT old_mouse_b else infinite loop
               {
                  al_rest(0.01);
                  a5_get_mouse_state(&a5_ms_state);
               }
            }
            else if (tmp_sel.start == FALSE)
            {
               if (key_pressed(KEY_I) &&
                   (cx != old_identical_x || cy != old_identical_y))
               {
                  // for all the tiles Identical to the visible ones

                  if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
                  {
                     // add to selection
                     itype = IT_ADD;
                  }
                  else if (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
                  {
                     // delete from previous selection
                     itype = IT_DEL;
                  }
                  else
                  {
                     // new selection (delete previous one)
                     itype = IT_NEW;
                  }
                  edittile_identical(ds1_idx, itype, cx, cy,
                      key_pressed(KEY_ALT) || key_pressed(KEY_ALTGR));
                  old_identical_x = cx;
                  old_identical_y = cy;
               }
               else if (!key_pressed(KEY_I))
               {
                  // starting a temp selection
                  old_identical_x = -1;
                  old_identical_y = -1;

                  tmp_sel.start = TRUE;
                  tmp_sel.x1 = tmp_sel.x2 = tmp_sel.old_x2 = cx;
                  tmp_sel.y1 = tmp_sel.y2 = tmp_sel.old_y2 = cy;
                  edittile_delete_all_tmpsel(ds1_idx);
                  edittile_set_tmpsel(ds1_idx, &tmp_sel);
               }
            }
            else
            {
               if ((tmp_sel.old_x2 != cx) || (tmp_sel.old_y2 != cy))
               {
                  // update the temp selection
                  tmp_sel.x2 = tmp_sel.old_x2 = cx;
                  tmp_sel.y2 = tmp_sel.old_y2 = cy;
                  edittile_delete_all_tmpsel(ds1_idx);
                  edittile_set_tmpsel(ds1_idx, &tmp_sel);
               }
            }
         }
      }
      else
      {
         // mouse button 1 is not pressed
         if (glb_ds1edit.mode == MOD_T)
         {
            if (tmp_sel.start == TRUE)
            {
               // end of tmp sel, process it
               if (key_pressed(KEY_H))
                  tmp_sel.type = TMP_HIDE;
               else if (key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT))
                  tmp_sel.type = TMP_ADD;
               else if (key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL))
                  tmp_sel.type = TMP_DEL;
               else
                  tmp_sel.type = TMP_NEW;
               switch (tmp_sel.type)
               {
               case TMP_NEW:
                  edittile_delete_all_tmpsel(ds1_idx);
                  edittile_change_to_new_permanent_sel(ds1_idx, &tmp_sel);
                  tmp_sel.start = FALSE;
                  tmp_sel.type = TMP_NULL;
                  tmp_sel.x1 = tmp_sel.x2 = tmp_sel.y1 = tmp_sel.y2 = 0;
                  tmp_sel.old_x2 = tmp_sel.old_y2 = 0;
                  break;

               case TMP_ADD:
                  edittile_delete_all_tmpsel(ds1_idx);
                  edittile_change_to_add_permanent_sel(ds1_idx, &tmp_sel);
                  tmp_sel.start = FALSE;
                  tmp_sel.type = TMP_NULL;
                  tmp_sel.x1 = tmp_sel.x2 = tmp_sel.y1 = tmp_sel.y2 = 0;
                  tmp_sel.old_x2 = tmp_sel.old_y2 = 0;
                  break;

               case TMP_HIDE:
                  edittile_delete_all_tmpsel(ds1_idx);
                  edittile_change_to_hide_sel(ds1_idx, &tmp_sel);
                  tmp_sel.start = FALSE;
                  tmp_sel.type = TMP_NULL;
                  tmp_sel.x1 = tmp_sel.x2 = tmp_sel.y1 = tmp_sel.y2 = 0;
                  tmp_sel.old_x2 = tmp_sel.old_y2 = 0;
                  break;

               case TMP_DEL:
                  edittile_delete_all_tmpsel(ds1_idx);
                  edittile_change_to_del_sel(ds1_idx, &tmp_sel);
                  tmp_sel.start = FALSE;
                  tmp_sel.type = TMP_NULL;
                  tmp_sel.x1 = tmp_sel.x2 = tmp_sel.y1 = tmp_sel.y2 = 0;
                  tmp_sel.old_x2 = tmp_sel.old_y2 = 0;
                  break;
               }
            }
            else if (paste_pos.start == TRUE)
            {
               if ((paste_pos.old_x != cx) ||
                   (paste_pos.old_y != cy) ||
                   (paste_pos.old_ds1_idx != ds1_idx))
               {
                  edittile_paste_undo(paste_pos.old_ds1_idx);
                  edittile_paste_preview(ds1_idx,
                                         cx - paste_pos.start_x,
                                         cy - paste_pos.start_y,
                                         &paste_pos);
                  paste_pos.old_x = cx;
                  paste_pos.old_y = cy;
                  paste_pos.old_ds1_idx = ds1_idx;
               }
            }
         }
      }

      // right mouse button
      if (old_mouse_b & 2)
      {
         if (glb_ds1edit.mode == MOD_T)
         {
            int rc_ctrl = key_pressed(KEY_LCONTROL) || key_pressed(KEY_RCONTROL);
            int rc_shift = key_pressed(KEY_LSHIFT) || key_pressed(KEY_RSHIFT);

            if (rc_ctrl && rc_shift)
            {
               /* Ctrl+Shift+right-click: advanced bits window (legacy) */
               wbits_main(ds1_idx, cx, cy);
               a5_set_mouse_xy(a5_display, old_mouse_x, old_mouse_y);
            }
            else if (rc_shift)
            {
               /* Shift+right-click: full wedit_test (legacy) */
               wedit_test(ds1_idx, cx, cy);
               a5_set_mouse_xy(a5_display, old_mouse_x, old_mouse_y);
            }
            else
            {
               /* Plain right-click press-and-hold: radial popup.
                * popup_run runs its own modal loop while button held. */
               tile_picker_popup_run(ds1_idx, cx, cy,
                                     a5_mouse_x, a5_mouse_y);
               a5_set_mouse_xy(a5_display, old_mouse_x, old_mouse_y);
            }
         }
      }

      // quit
      if (key_hit(KEY_ESC) && (glb_ds1[ds1_idx].draw_edit_obj == FALSE) &&
          input_escape_may_quit(glb_ds1edit.props_panel.editing,
                                glb_ds1edit.props_panel.has_focus))
      {
         done = interface_confirm_quit();
      }

      perf_accumulate(
          &glb_perf_stats.ui_ms_total,
          &glb_perf_stats.ui_ms_max,
          perf_now_ms() - section_start_ms);
      perf_accumulate(
          &glb_perf_stats.frame_ms_total,
          &glb_perf_stats.frame_ms_max,
          perf_now_ms() - frame_start_ms);
      glb_perf_stats.frames++;
      if (glb_perf_stats.frames >= 30)
         perf_print_summary();

      /* --switchbench: cycle maps within one group and report each switch.
         Runs after a frame has been presented so the display and assets are
         real, which is what makes the numbers comparable to a live click. */
      if (!switchbench_done && glb_ds1edit.cmd_line.switchbench_group >= 0)
      {
         AREA_BROWSER_S * ab = &glb_ds1edit.area_browser;
         int gi = glb_ds1edit.cmd_line.switchbench_group;

         switchbench_done = 1;

         if (gi >= ab->group_count)
            printf("switchbench: no group %d (have %d)\n", gi, ab->group_count);
         else
         {
            int n = glb_ds1edit.cmd_line.switchbench_count;
            int e;

            if (n > ab->groups[gi].entry_count)
               n = ab->groups[gi].entry_count;

            printf("switchbench: group %d \"%s\", %d of %d map(s)\n",
                   gi, ab->groups[gi].name, n, ab->groups[gi].entry_count);
            fflush(stdout);

            for (e = 0; e < n; e++)
            {
               int new_idx = area_browser_switch_single(gi, e);

               /* Adopt the loaded map the same way a sidebar click does --
                  without this the benchmark measured the load but left the
                  view pointing at whatever the .ini opened. */
               if (new_idx >= 0)
               {
                  ds1_idx = new_idx;
                  tile_picker_on_ds1_change(new_idx);
               }
            }
         }

         /* Draw the map we just loaded before quitting, so what is on screen
            matches what was measured. Exiting straight from here left the
            window showing Rogue Encampment for every group benchmarked. */
         switchbench_exit_in = 3;
      }

      if ((switchbench_exit_in > 0) && (--switchbench_exit_in == 0))
         done = TRUE;

      /* --selftest: the frame counter is the whole point -- if we get here
         the display exists, the assets loaded and the loop is running. */
      if (glb_ds1edit.cmd_line.selftest_frames > 0)
      {
         selftest_rendered++;

         /* Drive one resize through the real event path. Nothing else in the
            project exercises DISPLAY_RESIZE, and a handler that rebuilds the
            render targets is exactly the kind of thing that works until the
            day it segfaults. Half way through, so there are frames drawn at
            both sizes. */
         if (selftest_rendered == glb_ds1edit.cmd_line.selftest_frames / 2)
         {
            ALLEGRO_EVENT rev;
            int rw = al_get_display_width(a5_display) - 80;
            int rh = al_get_display_height(a5_display) - 60;
            fprintf(stdout, "selftest: resizing display to %dx%d\n", rw, rh);
            fflush(stdout);
            al_resize_display(a5_display, rw, rh);
            /* al_resize_display does not post DISPLAY_RESIZE -- that event is
               for user-driven resizes -- so hand the handler the event the
               window manager would have sent. This is the path that runs when
               someone drags the window edge. */
            memset(&rev, 0, sizeof(rev));
            rev.display.type = ALLEGRO_EVENT_DISPLAY_RESIZE;
            rev.display.source = a5_display;
            rev.display.width = rw;
            rev.display.height = rh;
            input_note_event(&rev);
         }

         if (selftest_rendered >= glb_ds1edit.cmd_line.selftest_frames)
         {
            fprintf(stdout, "selftest: rendered %d frame(s), exiting\n",
                    selftest_rendered);

            /* --selftest-shot: dump the finished frame. This is the only way
               to see what the GUI actually drew -- the unit tests are headless
               and --headless never creates a display -- and it is how the
               missing torches were finally pinned down. */
            if (glb_ds1edit.cmd_line.selftest_shot != NULL)
            {
               if (al_save_bitmap(glb_ds1edit.cmd_line.selftest_shot,
                                  glb_ds1edit.screen_buff))
                  fprintf(stdout, "selftest: wrote %s\n",
                          glb_ds1edit.cmd_line.selftest_shot);
               else
                  fprintf(stdout, "selftest: could not write %s\n",
                          glb_ds1edit.cmd_line.selftest_shot);
            }

            /* Rendering proves nothing about input: a main loop that never
               pumps draws a perfect picture and ignores the keyboard for
               ever. That shipped once. The tick timer fires at 25 Hz, so
               after this many frames both counters must have moved. */
            fprintf(stdout,
                    "selftest: input frames=%lu events=%lu timer=%lu\n",
                    a5_input_frames, a5_input_events,
                    a5_input_timer_events);
            if (a5_input_frames == 0 || a5_input_timer_events == 0)
            {
               fprintf(stdout,
                       "selftest: FAIL -- the main loop is not wired to the "
                       "input layer, so key_hit()/mouse_hit() can never fire\n");
               fflush(stdout);
               exit(1);
            }

            /* A sprite or glyph left in a MEMORY bitmap still draws a correct
               picture -- just with the backbuffer locked once per draw. That
               has shipped twice (sprites in 2.1.1, the HUD font under
               Direct3D after it), both times at under 1 FPS. A healthy frame
               composes in about a millisecond; 250 ms is far above any real
               GPU or software renderer and far below the collapse. */
            {
               int font_bad = a5_gpu_guard_font(a5_font, "a5_font");
               const char *failure = NULL;
               double wall_s = al_get_time() - st_t0;

               /* Verdict first, so the JSON report carries it. The time
                  rule is on the main thread's CPU time, not wall time: on a
                  loaded host (parallel test lanes) frames wait seconds for a
                  core, and a wall-time mean over 250 ms failed about one
                  healthy run in 150 with exit 1. See selftest_verdict.h. */
               failure = selftest_render_verdict(st_n, st_cpu_ms_total,
                                                 a5_gpu_guard_hits);

               st_report(st_win_w, st_win_h, wall_s, font_bad, failure);
               if (failure != NULL)
               {
                  fprintf(stdout, "selftest: FAIL -- %s\n", failure);
                  fflush(stdout);
                  exit(1);
               }
            }
            fflush(stdout);
            done = TRUE;
         }
      }
   }

   perf_print_summary();
}
