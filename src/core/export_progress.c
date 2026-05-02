#include <stdio.h>
#include <string.h>

#include <allegro5/allegro.h>

#include "structs.h"
#include "core/export_progress.h"

static EXPORT_PROGRESS_S s_state;

// Tier-1 stage weights. Indexed by EXPORT_STAGE_E.
static const int s_stage_weights[] = {
   0,    /* NONE */
   5,    /* PREPARE */
   35,   /* NATIVE_EXPORT */
   35,   /* LOCAL_UPSCALE (replaces remote stages on local fallback) */
   10,   /* PACKAGE_UPLOAD */
   25,   /* REMOTE_PROCESSING */
   15,   /* DOWNLOAD */
   10    /* EXTRACT */
};

static int s_stage_count = (int) (sizeof(s_stage_weights) / sizeof(s_stage_weights[0]));

void export_progress_begin(const char *title)
{
   memset(&s_state, 0, sizeof(s_state));
   s_state.active = 1;
   s_state.stage = EXPORT_STAGE_NONE;
   s_state.result = EXPORT_RESULT_PENDING;
   if (title != NULL)
   {
      strncpy(s_state.title, title, sizeof(s_state.title) - 1);
      s_state.title[sizeof(s_state.title) - 1] = 0;
   }
}

void export_progress_end(void)
{
   memset(&s_state, 0, sizeof(s_state));
}

int export_task_is_active(void)
{
   return s_state.active;
}

const EXPORT_PROGRESS_S *export_progress_state(void)
{
   return &s_state;
}

void export_progress_set_stage(EXPORT_STAGE_E stage,
                               const char *stage_label,
                               int items_total)
{
   if (!s_state.active)
      return;
   s_state.stage = stage;
   s_state.items_done = 0;
   s_state.items_total = items_total > 0 ? items_total : 0;
   s_state.current_item[0] = 0;
   if (stage_label != NULL)
   {
      strncpy(s_state.stage_label, stage_label,
              sizeof(s_state.stage_label) - 1);
      s_state.stage_label[sizeof(s_state.stage_label) - 1] = 0;
   }
   else
   {
      s_state.stage_label[0] = 0;
   }
}

void export_progress_advance(int delta)
{
   if (!s_state.active || delta <= 0)
      return;
   s_state.items_done += delta;
   if (s_state.items_total > 0 && s_state.items_done > s_state.items_total)
      s_state.items_done = s_state.items_total;
}

void export_progress_set_current_item(const char *path)
{
   if (!s_state.active)
      return;
   if (path == NULL)
   {
      s_state.current_item[0] = 0;
      return;
   }
   strncpy(s_state.current_item, path, sizeof(s_state.current_item) - 1);
   s_state.current_item[sizeof(s_state.current_item) - 1] = 0;
}

void export_progress_request_cancel(void)
{
   if (!s_state.active)
      return;
   s_state.cancel_requested = 1;
}

int export_progress_cancel_requested(void)
{
   return s_state.cancel_requested;
}

static int stage_weight(EXPORT_STAGE_E stage)
{
   int idx = (int) stage;
   if (idx < 0 || idx >= s_stage_count)
      return 0;
   return s_stage_weights[idx];
}

int export_progress_percent(int include_remote_stages)
{
   /* Sum of stage weights for stages strictly before the current stage,
    * plus the within-stage fraction. Renormalize so the total equals 100. */
   int total_weight = stage_weight(EXPORT_STAGE_PREPARE);
   int passed = 0;
   int within = 0;
   EXPORT_STAGE_E s;

   total_weight += stage_weight(EXPORT_STAGE_NATIVE_EXPORT);

   if (include_remote_stages)
   {
      total_weight += stage_weight(EXPORT_STAGE_PACKAGE_UPLOAD);
      total_weight += stage_weight(EXPORT_STAGE_REMOTE_PROCESSING);
      total_weight += stage_weight(EXPORT_STAGE_DOWNLOAD);
      total_weight += stage_weight(EXPORT_STAGE_EXTRACT);
   }
   else
   {
      total_weight += stage_weight(EXPORT_STAGE_LOCAL_UPSCALE);
   }

   if (total_weight <= 0)
      return 0;

   /* Tally weights for stages that are fully done. */
   for (s = EXPORT_STAGE_PREPARE; s < s_state.stage; s++)
   {
      if (!include_remote_stages
          && (s == EXPORT_STAGE_PACKAGE_UPLOAD
              || s == EXPORT_STAGE_REMOTE_PROCESSING
              || s == EXPORT_STAGE_DOWNLOAD
              || s == EXPORT_STAGE_EXTRACT))
         continue;
      if (include_remote_stages && s == EXPORT_STAGE_LOCAL_UPSCALE)
         continue;
      passed += stage_weight(s);
   }

   /* Within-stage fraction. */
   {
      int sw = stage_weight(s_state.stage);
      if (s_state.items_total > 0)
         within = (sw * s_state.items_done) / s_state.items_total;
      else
         within = 0;
   }

   {
      int pct = ((passed + within) * 100) / total_weight;
      if (pct < 0)   pct = 0;
      if (pct > 100) pct = 100;
      return pct;
   }
}

int export_progress_pump(void)
{
   ALLEGRO_EVENT ev;

   if (!s_state.active)
      return 0;

   /* Drain pending events so the OS does not flag us as unresponsive.
    * Esc anywhere requests cancellation. */
   if (a5_event_queue != NULL)
   {
      while (al_get_next_event(a5_event_queue, &ev))
      {
         if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
         {
            s_state.cancel_requested = 1;
         }
         else if (ev.type == ALLEGRO_EVENT_KEY_CHAR)
         {
            if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
               s_state.cancel_requested = 1;
         }
      }
   }

   /* Dialog repaint will be wired in the next commit. */
   return s_state.cancel_requested;
}
