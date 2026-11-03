/* Derived from win_ds1edit by Paul Siramy (originally wMsg.h, msg_quit.h and msg_save.h).
 * See NOTICE at the repository root for attribution and license status. */

#ifndef _DIALOGS_H_

#define _DIALOGS_H_

int wmsg_main     (WMSG_S * wmsg);
int msg_quit_main (void);
int msg_save_main (void);
int msg_unsaved_main          (const char *text);
int msg_project_required_main (void);

// 0 : replace the project's copy, 1 or -1 : cancel
int msg_replace_project_copy_main (void);

#endif
