#ifndef DS1EDIT_MENU_BAR_H
#define DS1EDIT_MENU_BAR_H

#include "ui/commands.h"

// Draw the bar and any open menu onto the current target. Does nothing when the
// bar has no height, as in headless renders.
void menu_bar_draw(void);

// Call once a frame. Opens a menu on Alt+F, E, M, V or B or a press on the bar,
// and runs it modally: an open menu owns all input until it closes. Otherwise
// looks up this frame's hotkey, unless keys_enabled is 0 because a text field
// has the keyboard. A dispatched key is used up, so polled code can't also act
// on it. Returns a command to run, or CMD_NONE. A window close while a menu is
// open closes the menu and stays pending for the caller.
CMD_ID menu_bar_poll(const CMD_CTX *ctx, int keys_enabled);

#endif
