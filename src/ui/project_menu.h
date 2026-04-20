#ifndef _PROJECT_MENU_H_
#define _PROJECT_MENU_H_

// Poll Ctrl+Shift+{N,O,W} and dispatch to the appropriate project action.
// Safe to call every frame. Blocks the main loop while a native file dialog
// is up -- that's the cost of using the OS dialogs, mirrors the behaviour of
// the rest of the editor's modal UI.
//
// The "Project: <name>" indicator itself lives in preview.c's bottom status
// row next to [Objects] / Mode, so the whole HUD shares one row system and
// nothing overlaps the top header bar.
void project_menu_handle_shortcuts(void);

#endif
