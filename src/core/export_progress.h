#ifndef _EXPORT_PROGRESS_H_
#define _EXPORT_PROGRESS_H_

// In-process export task progress state. A singleton; only one export
// task may be active at a time (the unified Ctrl+Shift+A flow enforces
// this with export_task_is_active).
//
// Stage weights (per the locked tier-1 model in
// EXPORT_PROGRESS_AND_ASYNC_UPSCALE_PLAN.md, decision 14):
//   PREPARE              5%
//   NATIVE_EXPORT       35%
//   PACKAGE_UPLOAD      10%
//   REMOTE_PROCESSING   25%
//   DOWNLOAD            15%
//   EXTRACT             10%
// Native-only exports normalize to 100% over PREPARE + NATIVE_EXPORT.

typedef enum EXPORT_STAGE_E
{
   EXPORT_STAGE_NONE              = 0,
   EXPORT_STAGE_PREPARE           = 1,
   EXPORT_STAGE_NATIVE_EXPORT     = 2,
   EXPORT_STAGE_LOCAL_UPSCALE     = 3,
   EXPORT_STAGE_PACKAGE_UPLOAD    = 4,
   EXPORT_STAGE_REMOTE_PROCESSING = 5,
   EXPORT_STAGE_DOWNLOAD          = 6,
   EXPORT_STAGE_EXTRACT           = 7
} EXPORT_STAGE_E;

typedef enum EXPORT_RESULT_E
{
   EXPORT_RESULT_PENDING  = 0,
   EXPORT_RESULT_SUCCESS  = 1,
   EXPORT_RESULT_CANCELED = 2,
   EXPORT_RESULT_FAILED   = 3
} EXPORT_RESULT_E;

#define EXPORT_PROGRESS_TITLE_MAX        96
#define EXPORT_PROGRESS_STAGE_LABEL_MAX  64
#define EXPORT_PROGRESS_ITEM_MAX        256

typedef struct EXPORT_PROGRESS_S
{
   int             active;
   EXPORT_STAGE_E  stage;
   int             items_done;
   int             items_total;
   int             cancel_requested;
   EXPORT_RESULT_E result;
   char            title[EXPORT_PROGRESS_TITLE_MAX];
   char            stage_label[EXPORT_PROGRESS_STAGE_LABEL_MAX];
   char            current_item[EXPORT_PROGRESS_ITEM_MAX];
} EXPORT_PROGRESS_S;

// Lifecycle.
void export_progress_begin(const char *title);
void export_progress_end(void);
int  export_task_is_active(void);
const EXPORT_PROGRESS_S * export_progress_state(void);

// Stage transitions and item-grain updates.
void export_progress_set_stage(EXPORT_STAGE_E stage,
                               const char *stage_label,
                               int items_total);
void export_progress_advance(int delta);
void export_progress_set_current_item(const char *path);

// Cancellation.
void export_progress_request_cancel(void);
int  export_progress_cancel_requested(void);

// Compute the overall percent (0..100) from current stage + within-stage
// progress, using the tier-1 weights documented above. When all the
// remote stages are skipped (native-only export), weights renormalize.
int  export_progress_percent(int include_remote_stages);

// Tells the dialog whether to use the remote weight set (PACKAGE_UPLOAD
// + REMOTE_PROCESSING + DOWNLOAD + EXTRACT) or the local one
// (LOCAL_UPSCALE in their place). Caller picks once before starting the
// task; default is local-only.
void export_progress_set_show_remote_stages(int yes);

// Cooperative pump: call from inner loops between items. Drains Allegro
// events (so the OS doesn't think we froze), repaints the dialog if it
// is showing, and checks for cancellation. Returns 1 if the user has
// requested cancel; caller should bail out at the next safe point.
int  export_progress_pump(void);

#endif
