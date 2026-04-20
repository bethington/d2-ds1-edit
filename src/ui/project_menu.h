#ifndef _PROJECT_MENU_H_
#define _PROJECT_MENU_H_

#include <allegro5/allegro.h>

// Poll Ctrl+Shift+{N,O,W} and dispatch to the appropriate project action.
// Safe to call every frame. Blocks the main loop while a native file dialog
// is up -- that's the cost of using the OS dialogs, mirrors the behaviour of
// the rest of the editor's modal UI.
void project_menu_handle_shortcuts(void);

// Render a small "Project: <name>" label in the top-left corner of the given
// target bitmap, or nothing if no project is open.
void project_menu_draw_indicator(ALLEGRO_BITMAP *target);

#endif
